// OpenWheels Android activity.
//
// The original game's org.cocos2dx.cpp.AppActivity extended sdkbox's SDKBoxActivity and, in
// onCreate, hid the system bars (setEnableVirtualButton(false)), set
// layoutInDisplayCutoutMode = SHORT_EDGES (API 28+), then started its ad SDK (AdHelper.init) and
// Google Play Billing (IAPHelper). OpenWheels keeps only the screen part: there is no ad, billing,
// analytics, consent or connectivity code anywhere on the Java side.
//
// Additions for modern devices (platform layer only, the game code is untouched):
//   * immersive sticky fullscreen through WindowInsetsController on API 30+ (cocos2d-x 3.17's
//     reflection-based setSystemUiVisibility path is kept for older releases);
//   * the game view is kept inside the display cutout's safe insets (black margin over the notch
//     or punch hole), so no game UI ends up under it - and out from under system bars when they
//     are really shown (split screen / desktop windows);
//   * screens narrower than 3:2 (4:3 tablets, foldables' inner displays) are letterboxed, since
//     the game's menus are laid out for 16:9 at a fixed height and would run off the edges;
//   * size changes without an activity restart (rotation, fold/unfold, split screen, freeform
//     windows - see configChanges in AndroidManifest.xml) resize the cocos2d-x frame
//     (nativeSurfaceResized, src/platform/android/main.cpp) instead of stretching the old one;
//   * the screen stays on while the game is in front.
package org.openwheels.game;

import android.content.Context;
import android.graphics.Color;
import android.os.Build;
import android.os.Bundle;
import android.view.DisplayCutout;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;
import android.widget.FrameLayout;

import org.cocos2dx.lib.Cocos2dxActivity;
import org.cocos2dx.lib.Cocos2dxGLSurfaceView;

public class AppActivity extends Cocos2dxActivity {

    // Narrowest aspect ratio (width / height) the game is shown at; narrower screens are
    // letterboxed. 3:2 still fits every menu of the original 16:9 layout.
    static final float MIN_ASPECT = 1.5f;

    // src/platform/android/main.cpp; runs on the GL thread.
    private static native void nativeSurfaceResized(int width, int height);

    private GameFrame mGameFrame;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        // As the original: no navigation/status bars.
        setEnableVirtualButton(false);

        final Window window = getWindow();
        if (Build.VERSION.SDK_INT >= 28) {
            WindowManager.LayoutParams lp = window.getAttributes();
            lp.layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
            window.setAttributes(lp);
        }
        if (Build.VERSION.SDK_INT >= 30) {
            window.setDecorFitsSystemWindows(false);
        }

        super.onCreate(savedInstanceState);
        if (mFrameLayout == null || getGLSurfaceView() == null) {
            return;  // Cocos2dxActivity finished a second instance (not the task root)
        }

        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

        // Wrap cocos2d-x's content view (set by Cocos2dxActivity.init) in the GameFrame. The window
        // is not attached yet, so the GL surface has not been created at this point.
        mGameFrame = new GameFrame(this);
        setContentView(mGameFrame);
        mGameFrame.addView(mFrameLayout, new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));

        installCutoutInsets();
        installResizeHandling();
    }

    // Keeps the GL view out of the display cutout: the game frame is padded by the cutout's safe
    // insets (0 on devices without one). System bars are normally hidden and need no room.
    private void installCutoutInsets() {
        mGameFrame.setOnApplyWindowInsetsListener(new View.OnApplyWindowInsetsListener() {
            @Override
            public WindowInsets onApplyWindowInsets(View view, WindowInsets insets) {
                int left = 0, top = 0, right = 0, bottom = 0;
                if (Build.VERSION.SDK_INT >= 30) {
                    // The cutout; in split screen, freeform and desktop windows also the system
                    // bars, which stay visible there. Fullscreen, the bars are hidden (immersive)
                    // and are ignored: they are briefly reported as shown at start-up, and taking
                    // them would size the GL surface wrongly for a frame.
                    int types = WindowInsets.Type.displayCutout();
                    if (isInMultiWindowMode()) {
                        types |= WindowInsets.Type.systemBars();
                    }
                    android.graphics.Insets safe = insets.getInsets(types);
                    left = safe.left;
                    top = safe.top;
                    right = safe.right;
                    bottom = safe.bottom;
                } else if (Build.VERSION.SDK_INT >= 28) {
                    DisplayCutout cutout = insets.getDisplayCutout();
                    if (cutout != null) {
                        left = cutout.getSafeInsetLeft();
                        top = cutout.getSafeInsetTop();
                        right = cutout.getSafeInsetRight();
                        bottom = cutout.getSafeInsetBottom();
                    }
                }
                if (view.getPaddingLeft() != left || view.getPaddingTop() != top
                        || view.getPaddingRight() != right || view.getPaddingBottom() != bottom) {
                    view.setPadding(left, top, right, bottom);
                }
                return insets;
            }
        });
        mGameFrame.requestApplyInsets();
    }

    // Root view: holds cocos2d-x's layout (GL view + edit box) inside the safe area and, on screens
    // narrower than MIN_ASPECT (4:3 tablets, foldables' inner displays, split screen), letterboxes
    // it top and bottom. The game lays its menus out for 16:9 and wider at a fixed height (design
    // 3600x2000, FIXED_HEIGHT), so at 4:3 the main menu's button row runs off the left edge.
    // Measuring here (not padding after the fact) gives the GL surface its final size before
    // cocos2d-x reads it in nativeInit.
    static final class GameFrame extends FrameLayout {
        GameFrame(Context context) {
            super(context);
            setBackgroundColor(Color.BLACK);
        }

        @Override
        protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
            final int width = MeasureSpec.getSize(widthMeasureSpec);
            final int height = MeasureSpec.getSize(heightMeasureSpec);
            setMeasuredDimension(width, height);
            final int[] box = contentBox(width, height);
            for (int i = 0; i < getChildCount(); ++i) {
                getChildAt(i).measure(MeasureSpec.makeMeasureSpec(box[2], MeasureSpec.EXACTLY),
                                      MeasureSpec.makeMeasureSpec(box[3], MeasureSpec.EXACTLY));
            }
        }

        @Override
        protected void onLayout(boolean changed, int l, int t, int r, int b) {
            final int[] box = contentBox(r - l, b - t);
            for (int i = 0; i < getChildCount(); ++i) {
                getChildAt(i).layout(box[0], box[1], box[0] + box[2], box[1] + box[3]);
            }
        }

        // {x, y, width, height} of the game inside a frame of the given size.
        private int[] contentBox(int width, int height) {
            int x = getPaddingLeft();
            int y = getPaddingTop();
            int w = Math.max(1, width - getPaddingLeft() - getPaddingRight());
            int h = Math.max(1, height - getPaddingTop() - getPaddingBottom());
            if ((float) w / h < MIN_ASPECT) {
                final int letterboxed = Math.max(1, Math.round(w / MIN_ASPECT));
                y += (h - letterboxed) / 2;
                h = letterboxed;
            }
            return new int[] {x, y, w, h};
        }
    }

    // cocos2d-x 3.17 sizes its frame once (Cocos2dxRenderer.nativeInit) and ignores later surface
    // changes; forward them so the view keeps its resolution policy at the new size.
    private void installResizeHandling() {
        final Cocos2dxGLSurfaceView glView = getGLSurfaceView();
        glView.addOnLayoutChangeListener(new View.OnLayoutChangeListener() {
            @Override
            public void onLayoutChange(View v, int l, int t, int r, int b,
                                       int oldL, int oldT, int oldR, int oldB) {
                final int width = r - l;
                final int height = b - t;
                if (width <= 0 || height <= 0) return;
                if (width == oldR - oldL && height == oldB - oldT) return;
                glView.queueEvent(new Runnable() {
                    @Override
                    public void run() {
                        nativeSurfaceResized(width, height);
                    }
                });
            }
        });
    }

    @Override
    protected void hideVirtualButton() {
        if (Build.VERSION.SDK_INT >= 30) {
            try {
                WindowInsetsController controller = getWindow().getInsetsController();
                if (controller != null) {
                    controller.hide(WindowInsets.Type.systemBars());
                    controller.setSystemBarsBehavior(
                            WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
                    return;
                }
            } catch (RuntimeException e) {
                // fall back to the legacy flags below
            }
        }
        super.hideVirtualButton();
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            hideVirtualButton();  // re-enter immersive mode after dialogs / the keyboard / swipes
        }
    }
}
