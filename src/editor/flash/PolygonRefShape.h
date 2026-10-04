#pragma once
// EDITOR (browser features, PC addition): browser polygon and art shapes (Flash editor
// PolygonShape <sh t="3"> and ArtShape <sh t="4">, with a <v f id n v0="x_y" ...> vertex list).
//   * polygon: a physics shape, convex, clockwise in Flash coordinates, at most 8 vertices (the
//     game's Box2D limit; the Flash editor allowed 10);
//   * art: decoration without physics (no collision, not counted as a shape), up to 100 vertices;
//     loaded bezier handles are kept (drawn flattened, written back unchanged).
// Vertices are kept in Flash pixels relative to the shape's position (y down). Width / height
// scale them (Flash EdgeShape.shapeWidth over the default extent).

#include "RefShape.h"

class PolygonRefShape : public RefShape
{
public:
    static const int kPolygonLevelItemID = 6003;
    static const int kArtLevelItemID = 6004;
    static const int kMaxPolygonVerts = 8;
    static const int kMaxArtVerts = 100;

    static PolygonRefShape* create(bool art);
    bool initWithArt(bool art);

    bool isArt() const { return _art; }
    const std::vector<cocos2d::Vec2>& vertsPx() const { return _verts; }
    // Replaces the outline (Flash px, relative to the position, y down). Art shapes may carry
    // Flash bezier handles per vertex (in, out; relative to the vertex), kept as loaded.
    void setVertsPx(const std::vector<cocos2d::Vec2>& verts);
    void setHandlesPx(const std::vector<cocos2d::Vec2>& handlesIn, const std::vector<cocos2d::Vec2>& handlesOut);
    const std::vector<cocos2d::Vec2>& handlesIn() const { return _handlesIn; }
    const std::vector<cocos2d::Vec2>& handlesOut() const { return _handlesOut; }
    bool hasHandles() const;
    bool closed() const { return _closed; }
    void setClosed(bool closed) { _closed = closed; }
    // Flash's default extent of the outline (with the origin), px.
    float extentPx(bool alongX) const;

    // Checks a candidate outline for the polygon tool: convex and clockwise (y down).
    static bool validPolygon(const std::vector<cocos2d::Vec2>& verts);

    // RefShape / Special
    float width() override;
    float height() override;
    void setWidth(float width) override;
    void setHeight(float height) override;
    void updateRefRect() override;
    void updateDrawingWithNode(cocos2d::DrawNode* node) override;
    std::vector<std::string> propertyKeysForUI() override;
    cocos2d::ValueMap properties() override;
    void setProperties(const cocos2d::ValueMap& properties) override;
    void setInteractive(const cocos2d::Value& interactive) override;

protected:
    PolygonRefShape() = default;
    bool _art = false;
    bool _closed = true;
    std::vector<cocos2d::Vec2> _verts;
    std::vector<cocos2d::Vec2> _handlesIn;
    std::vector<cocos2d::Vec2> _handlesOut;
    std::vector<cocos2d::Vec2> outlinePx() const;   // the drawn outline (beziers flattened)
};
