// Helper for extract_kid_gore.py: reads the display tree of a library symbol of a SWF and
// renders single frames of its clips to PNG through JPEXS FFDec's library (ffdec_lib.jar), with
// the clip's registration point on a known pixel. Run as a single-file program (Java 11+):
//   java -cp "<ffdec>/lib/ffdec_lib.jar;<ffdec>/lib/*" FlashPartRender.java <swf> <commands.txt>
// Commands, one per line:
//   tree <symbol class name | @root>
//       prints "ROOT <id>", then for every clip reachable from it "S <id> <frameCount>" and one
//       "P <clipId> <frame> <depth> <characterId> <isClip 0|1> <instanceName|-> <tx> <ty> <a> <b> <c> <d>"
//       per display-list entry (frames 1-based, translation in twips; a b c d = the placement
//       matrix: scaleX, rotateSkew0, rotateSkew1, scaleY). "@root" walks the SWF's main timeline,
//       reported as clip id 0 (the player characters' SWFs keep their parts there). Each placed
//       character's bounds are printed once as "B <characterId> <xmin> <ymin> <xmax> <ymax>" (twips).
//   render <out.png> <clipId> <frame> <zoom> <ignoredDepths|->
//       renders the frame (zoom = output pixels per Flash pixel) without the listed depths and
//       prints "R <out.png> <originX> <originY>", the pixel of the clip's (0,0). The canvas covers
//       the clip's bounds over all frames, so every frame of a clip shares one coordinate system.
import com.jpexs.decompiler.flash.SWF;
import com.jpexs.decompiler.flash.exporters.commonshape.ExportRectangle;
import com.jpexs.decompiler.flash.exporters.commonshape.Matrix;
import com.jpexs.decompiler.flash.tags.DefineSpriteTag;
import com.jpexs.decompiler.flash.tags.base.BoundedTag;
import com.jpexs.decompiler.flash.tags.base.CharacterTag;
import com.jpexs.decompiler.flash.tags.base.RenderContext;
import com.jpexs.decompiler.flash.timeline.DepthState;
import com.jpexs.decompiler.flash.timeline.Frame;
import com.jpexs.decompiler.flash.timeline.Timeline;
import com.jpexs.decompiler.flash.types.RECT;
import com.jpexs.helpers.SerializableImage;

import java.awt.image.BufferedImage;
import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileReader;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import javax.imageio.ImageIO;

public class FlashPartRender {
    public static void main(String[] args) throws Exception {
        SWF swf;
        try (FileInputStream in = new FileInputStream(args[0])) {
            swf = new SWF(in, false);
        }
        try (BufferedReader r = new BufferedReader(new FileReader(args[1]))) {
            String line;
            while ((line = r.readLine()) != null) {
                line = line.trim();
                if (line.isEmpty() || line.startsWith("#")) continue;
                String[] p = line.split("\\s+");
                if (p[0].equals("tree")) {
                    tree(swf, p[1]);
                } else if (p[0].equals("render")) {
                    render(swf, p[1], Integer.parseInt(p[2]), Integer.parseInt(p[3]),
                           Double.parseDouble(p[4]), p.length > 5 ? p[5] : "-");
                } else {
                    throw new IllegalArgumentException("unknown command: " + line);
                }
            }
        }
        System.out.flush();
        System.exit(0);
    }

    static void tree(SWF swf, String className) {
        Set<Integer> seen = new HashSet<>();
        Set<Integer> bounded = new HashSet<>();
        ArrayDeque<DefineSpriteTag> queue = new ArrayDeque<>();
        boolean mainTimeline = className.equals("@root");
        if (mainTimeline) {
            System.out.println("ROOT 0");
        } else {
            CharacterTag root = swf.getCharacterByClass(className);
            if (!(root instanceof DefineSpriteTag)) {
                System.out.println("ERROR no clip with class " + className);
                return;
            }
            System.out.println("ROOT " + root.getCharacterId());
            queue.add((DefineSpriteTag) root);
            seen.add(root.getCharacterId());
        }
        while (mainTimeline || !queue.isEmpty()) {
            Timeline tl;
            int id;
            if (mainTimeline) {
                tl = swf.getTimeline();
                id = 0;
                mainTimeline = false;
            } else {
                DefineSpriteTag sprite = queue.poll();
                tl = sprite.getTimeline();
                id = sprite.getCharacterId();
            }
            System.out.println("S " + id + " " + tl.getFrameCount());
            for (int f = 0; f < tl.getFrameCount(); f++) {
                Frame frame = tl.getFrame(f);
                for (Map.Entry<Integer, DepthState> e : frame.layers.entrySet()) {
                    DepthState ds = e.getValue();
                    if (ds == null || ds.characterId < 0) continue;
                    CharacterTag ch = swf.getCharacter(ds.characterId);
                    boolean isClip = ch instanceof DefineSpriteTag;
                    String name = ds.instanceName == null || ds.instanceName.isEmpty() ? "-" : ds.instanceName;
                    int tx = ds.matrix == null ? 0 : ds.matrix.translateX;
                    int ty = ds.matrix == null ? 0 : ds.matrix.translateY;
                    Matrix mm = ds.matrix == null ? new Matrix() : new Matrix(ds.matrix);
                    System.out.println("P " + id + " " + (f + 1) + " " + e.getKey() + " " + ds.characterId + " "
                                       + (isClip ? 1 : 0) + " " + name + " " + tx + " " + ty + " "
                                       + mm.scaleX + " " + mm.rotateSkew0 + " " + mm.rotateSkew1 + " " + mm.scaleY);
                    if (ch instanceof BoundedTag && bounded.add(ds.characterId)) {
                        RECT r = ((BoundedTag) ch).getRect();
                        System.out.println("B " + ds.characterId + " " + r.Xmin + " " + r.Ymin + " " + r.Xmax + " " + r.Ymax);
                    }
                    if (isClip && seen.add(ds.characterId)) {
                        queue.add((DefineSpriteTag) ch);
                    }
                }
            }
        }
    }

    static void render(SWF swf, String out, int id, int frame, double zoom, String ignore) throws Exception {
        DefineSpriteTag sprite = (DefineSpriteTag) swf.getCharacter(id);
        Timeline tl = sprite.getTimeline();
        RECT b = sprite.getRect();
        int pad = 4;
        // Registration point on a whole pixel so callers can line canvases up on it exactly.
        int ox = (int) Math.ceil(-b.Xmin * zoom / 20.0) + pad;
        int oy = (int) Math.ceil(-b.Ymin * zoom / 20.0) + pad;
        int w = ox + (int) Math.ceil(b.Xmax * zoom / 20.0) + pad;
        int h = oy + (int) Math.ceil(b.Ymax * zoom / 20.0) + pad;
        // View rectangle in twips covering the whole image.
        RECT view = new RECT((int) Math.floor(-ox * 20.0 / zoom), (int) Math.ceil((w - ox) * 20.0 / zoom),
                             (int) Math.floor(-oy * 20.0 / zoom), (int) Math.ceil((h - oy) * 20.0 / zoom));
        SerializableImage img = new SerializableImage(w, h, SerializableImage.TYPE_INT_ARGB_PRE);
        img.fillTransparent();
        // Same set-up as FFDec's SWF.frameToImageGet, with our own origin.
        Matrix m = new Matrix();
        m.translate(ox * 20.0, oy * 20.0);
        m.scale(zoom);
        List<Integer> ignoreDepths = new ArrayList<>();
        Set<Integer> hideChars = new HashSet<>();
        if (!ignore.equals("-")) {
            for (String s : ignore.split(",")) {
                if (s.isEmpty()) continue;
                // "c<id>": hide that character wherever it is placed inside the clip (nested too),
                // e.g. the Flash editor's collision-shape overlays (EDITOR, PC addition).
                if (s.charAt(0) == 'c') hideChars.add(Integer.parseInt(s.substring(1)));
                else ignoreDepths.add(Integer.parseInt(s));
            }
        }
        if (!hideChars.isEmpty()) hideCharacters(swf, sprite, hideChars);
        ExportRectangle vr = new ExportRectangle(view);
        tl.toImage(frame - 1, 0, new RenderContext(), img, img, false, m, new Matrix(), m, null, zoom, true,
                   vr, vr, m, true, Timeline.DRAW_MODE_ALL, 0, true, ignoreDepths, 1);
        BufferedImage src = img.getBufferedImage();
        BufferedImage dst = new BufferedImage(w, h, BufferedImage.TYPE_INT_ARGB);
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                dst.setRGB(x, y, src.getRGB(x, y));  // getRGB un-premultiplies
            }
        }
        File f = new File(out);
        if (f.getParentFile() != null) f.getParentFile().mkdirs();
        ImageIO.write(dst, "png", f);
        System.out.println("R " + out + " " + ox + " " + oy);
    }

    // Drops every placement of `ids` from the timelines reachable from `root` (in memory only).
    static void hideCharacters(SWF swf, DefineSpriteTag root, Set<Integer> ids) {
        Set<Integer> seen = new HashSet<>();
        ArrayDeque<DefineSpriteTag> queue = new ArrayDeque<>();
        queue.add(root);
        seen.add(root.getCharacterId());
        while (!queue.isEmpty()) {
            Timeline tl = queue.poll().getTimeline();
            for (int f = 0; f < tl.getFrameCount(); f++) {
                Frame frame = tl.getFrame(f);
                frame.layers.entrySet().removeIf(e -> e.getValue() != null && ids.contains(e.getValue().characterId));
                for (DepthState ds : frame.layers.values()) {
                    if (ds == null || ds.characterId < 0) continue;
                    CharacterTag ch = swf.getCharacter(ds.characterId);
                    if (ch instanceof DefineSpriteTag && seen.add(ds.characterId)) queue.add((DefineSpriteTag) ch);
                }
            }
        }
    }
}
