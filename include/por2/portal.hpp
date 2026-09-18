#pragma once
#include "types.hpp"
#include <optional>

namespace por2 {
struct Portal {
    Cell tile;
    int code = 0;
    bool active() const { return code >= 1 && code <= 8; }
    bool horizontal() const { return active() && ((code - 1) & 2) != 0; }
    bool occupies(int x, int y) const;
};
struct PortalFrame { Vec2 anchor, normal, tangent; };
PortalFrame decode(const Portal& portal);
Vec2 transformPoint(Vec2 point, const Portal& from, const Portal& to);
Vec2 transformVector(Vec2 vector, const Portal& from, const Portal& to);
Body transformBody(const Body& body, const Portal& from, const Portal& to);
double normalRadius(const Body& body, const Portal& portal);
bool fitsPortal(const Body& body, const Portal& portal);
bool intersectsPortal(const Body& body, const Portal& portal);
Rect clipToFront(Rect rectangle, const Portal& portal);
bool validPlacement(const TileMap& map, const Portal& portal, const Portal& other);
struct RayHit {
    Vec2 point;
    Cell tile;
    Vec2 normal;
    Vec2 ray;
};
std::optional<RayHit> castShot(const TileMap& map, Vec2 origin, Vec2 target);
std::optional<Portal> choosePortal(const TileMap& map, const Portal& other,
                                 Vec2 origin, Direction direction, const RayHit& hit);
} // namespace por2
