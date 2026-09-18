#pragma once
#include "portal.hpp"

namespace por2 {
struct MovementInput { bool left = false, right = false, jump = false; };
struct PortalMotion {
    // Portal on the side containing the body's center. No frame cooldown.
    int activePortal = -1;
    int crossings = 0;
};
class CollisionWorld {
public:
    CollisionWorld(const TileMap& map, const std::array<Portal, 2>& portals) : map_(map), portals_(portals) {}
    bool clear(Rect rectangle) const;
    bool canOccupy(const Body& body, int aperture = -1) const;
    bool recover(Player& player, PortalMotion& motion) const;
    bool move(Player& player, PortalMotion& motion, const MovementInput& input) const;
private:
    bool trial(const Body& body, int active, Vec2 delta, Body& result, int& next, bool& crossed) const;
    bool paired() const { return portals_[0].active() && portals_[1].active(); }
    const TileMap& map_;
    std::array<Portal, 2> portals_;
};
} // namespace por2
