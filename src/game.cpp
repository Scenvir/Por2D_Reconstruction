#include "por2/game.hpp"

namespace por2 {
Traversal describeTraversal(const Player& player, const PortalMotion& motion, const std::array<Portal, 2>& portals) {
    Traversal view;
    view.aimOrigin = player.body.head();
    view.aimDirection = player.body.direction;
    if (!portals[0].active() || !portals[1].active()) return view;
    int active = motion.activePortal;
    if (active < 0) {
        for (int i = 0; i < 2; ++i) {
            const auto frame = decode(portals[i]);
            if (intersectsPortal(player.body, portals[i]) && dot(player.body.center() - frame.anchor, frame.normal) >= -Epsilon)
                active = i;
        }
    }
    if (active < 0 || !intersectsPortal(player.body, portals[active])) return view;
    view.portalIndex = active;
    const auto& from = portals[active];
    const auto& to = portals[1 - active];
    const auto frame = decode(from);
    view.projection = transformBody(player.body, from, to);
    const auto area = [](Rect r) { return r.empty() ? 0.0 : (r.right - r.left) * (r.bottom - r.top); };
    view.bodyAreas[active] = area(clipToFront(player.body.bounds(), from));
    view.bodyAreas[1 - active] = area(clipToFront(view.projection->bounds(), to));
    // Uniform thickness: 2D area has exactly the same ordering as body volume.
    if (view.bodyAreas[active] > Epsilon && view.bodyAreas[1 - active] > Epsilon) {
        const double tolerance = player.body.width() * player.body.height() * Epsilon;
        view.lockedPortal = view.bodyAreas[active] + tolerance >= view.bodyAreas[1 - active] ? active : 1 - active;
    }
    if (dot(view.aimOrigin - frame.anchor, frame.normal) < -Epsilon) {
        view.headThrough = true;
        view.aimOrigin = transformPoint(view.aimOrigin, from, to);
        view.aimDirection = view.projection->direction;
    }
    return view;
}

bool replacePortal(const TileMap& map, const Player& player, const PortalMotion& motion,
                   std::array<Portal, 2>& portals, int id, const Portal& replacement) {
    if (id < 0 || id > 1) return false;
    const auto view = describeTraversal(player, motion, portals);
    if (id == view.lockedPortal || !validPlacement(map, replacement, portals[1 - id])) return false;
    auto proposed = portals;
    proposed[id] = replacement;
    // Moving the minority side is allowed. Validate its new projected fragment
    // transactionally: an obstructed target must not displace the majority body.
    const int aperture = view.lockedPortal >= 0 ? view.portalIndex : -1;
    if (!CollisionWorld(map, proposed).canOccupy(player.body, aperture)) return false;
    portals = proposed;
    return true;
}

bool firePortal(const TileMap& map, const Player& player, const PortalMotion& motion,
                std::array<Portal, 2>& portals, const Shot& shot, std::vector<ShotTrace>& traces) {
    if (shot.portal < 0 || shot.portal > 1) return false;
    // Re-evaluate for every event: moving the other portal can also move the head.
    const auto view = describeTraversal(player, motion, portals);
    if (shot.portal == view.lockedPortal) return false;
    const auto hit = castShot(map, view.aimOrigin, shot.target);
    if (!hit) return false;
    traces.push_back({shot.portal, view.aimOrigin, hit->point});
    const auto replacement = choosePortal(map, portals[1 - shot.portal], view.aimOrigin, view.aimDirection, *hit);
    return replacement && replacePortal(map, player, motion, portals, shot.portal, *replacement);
}

Game::Game(int initialLevel) {
    const auto it = std::find(Campaign.begin(), Campaign.end(), initialLevel);
    campaignIndex_ = it == Campaign.end() ? -1 : static_cast<int>(it - Campaign.begin());
    load(initialLevel);
}
Game::Game(const Level& customLevel):customLevel_(customLevel),campaignIndex_(-1){load(customLevel.id);}
void Game::load(int id) {
    level_ = customLevel_?*customLevel_:makeLevel(id);
    player_ = {};
    player_.body = level_.spawn;
    portals_ = {};
    motion_ = {};
    traversal_ = describeTraversal(player_, motion_, portals_);
    traces_.clear();
    finished_ = false;
}
void Game::restart() { load(level_.id); }
void Game::startCampaign() { if(customLevel_){restart();return;}campaignIndex_ = 0; load(Campaign[0]); }
void Game::advance() {
    if (campaignIndex_ < 0 || campaignIndex_ + 1 >= static_cast<int>(Campaign.size())) {
        finished_ = true;
        return;
    }
    load(Campaign[++campaignIndex_]);
}
bool Game::atExit() const {
    if (!level_.exit) return false;
    const auto delta = player_.body.position - level_.exit->position;
    return std::abs(delta.x) < 10 && std::abs(delta.y) < 10 && player_.body.direction == level_.exit->direction;
}
void Game::tick(const InputFrame& input) {
    if (finished_) {
        if (input.restart) startCampaign();
        return;
    }
    if ((input.skip && portals_[1].active()) || (input.useExit && atExit())) { advance(); return; }
    if (input.restart) { restart(); return; }
    traces_.clear();
    const CollisionWorld world(level_.map, portals_);
    if (!world.move(player_, motion_, input.movement)) { restart(); return; }
    // One authoritative post-physics pose feeds head marker, aiming and rendering.
    traversal_ = describeTraversal(player_, motion_, portals_);
    for (const auto& shot : input.shots) {
        shoot(shot);
    }
    traversal_ = describeTraversal(player_, motion_, portals_);
}
bool Game::shoot(const Shot& shot) {
    const bool accepted=firePortal(level_.map,player_,motion_,portals_,shot,traces_);
    traversal_=describeTraversal(player_,motion_,portals_);
    return accepted;
}
} // namespace por2
