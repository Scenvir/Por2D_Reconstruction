#pragma once
#include "level.hpp"
#include "physics.hpp"
#include <vector>

namespace por2 {
struct Shot { int portal = 0; Vec2 target; };
struct ShotTrace { int portal = 0; Vec2 origin; Vec2 end; };
struct InputFrame {
    MovementInput movement;
    bool restart = false;
    bool useExit = false;
    bool skip = false;
    std::vector<Shot> shots;
};

struct Traversal {
    int portalIndex = -1;
    int lockedPortal = -1;
    std::array<double, 2> bodyAreas{};
    std::optional<Body> projection;
    Vec2 aimOrigin;
    Direction aimDirection = Direction::Up;
    bool headThrough = false;
};
Traversal describeTraversal(const Player& player, const PortalMotion& motion, const std::array<Portal, 2>& portals);
bool replacePortal(const TileMap& map, const Player& player, const PortalMotion& motion,
                   std::array<Portal, 2>& portals, int id, const Portal& replacement);
bool firePortal(const TileMap& map, const Player& player, const PortalMotion& motion,
                std::array<Portal, 2>& portals, const Shot& shot, std::vector<ShotTrace>& traces);

class Game {
public:
    explicit Game(int initialLevel = 0);
    void tick(const InputFrame& input);
    void restart();
    void startCampaign();
    const Level& level() const { return level_; }
    const Player& player() const { return player_; }
    const std::array<Portal, 2>& portals() const { return portals_; }
    const Traversal& traversal() const { return traversal_; }
    const PortalMotion& motion() const { return motion_; }
    const std::vector<ShotTrace>& traces() const { return traces_; }
    bool finished() const { return finished_; }
private:
    void load(int id);
    void advance();
    bool atExit() const;
    Level level_;
    Player player_;
    std::array<Portal, 2> portals_{};
    Traversal traversal_;
    PortalMotion motion_;
    std::vector<ShotTrace> traces_;
    int campaignIndex_ = 0;
    bool finished_ = false;
};
} // namespace por2
