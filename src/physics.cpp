#include "por2/physics.hpp"
#include <limits>
#include <vector>

namespace por2 {
bool CollisionWorld::clear(Rect r) const {
    if (!std::isfinite(r.left) || !std::isfinite(r.top) || !std::isfinite(r.right) || !std::isfinite(r.bottom)) return false;
    if (r.empty()) return true;
    if (r.left < -Epsilon || r.top < -Epsilon || r.right > WindowWidth + Epsilon || r.bottom > WindowHeight + Epsilon)
        return false;
    const int x0 = std::max(0, static_cast<int>(std::floor((r.left + Epsilon) / TileSize)));
    const int x1 = std::min(MapWidth - 1, static_cast<int>(std::floor((r.right - Epsilon) / TileSize)));
    const int y0 = std::max(0, static_cast<int>(std::floor((r.top + Epsilon) / TileSize)));
    const int y1 = std::min(MapHeight - 1, static_cast<int>(std::floor((r.bottom - Epsilon) / TileSize)));
    for (int x = x0; x <= x1; ++x)
        for (int y = y0; y <= y1; ++y)
            if (map_.at(x, y) != Tile::Empty) return false;
    return true;
}
bool CollisionWorld::canOccupy(const Body& body, int aperture) const {
    if (!finite(body.position)) return false;
    if (!paired() || aperture < 0 || aperture > 1) return clear(body.bounds());
    const auto& from = portals_[aperture];
    const auto frame = decode(from);
    const double depth = dot(body.center() - frame.anchor, frame.normal);
    if (depth >= normalRadius(body, from) - Epsilon) return clear(body.bounds());
    if (!fitsPortal(body, from)) return false;
    const auto& to = portals_[1 - aperture];
    const auto projected = transformBody(body, from, to);
    return clear(clipToFront(body.bounds(), from)) && clear(clipToFront(projected.bounds(), to));
}
bool CollisionWorld::trial(const Body& body, int active, Vec2 delta, Body& result, int& next, bool& crossed) const {
    result = body;
    result.position = result.position + delta;
    crossed = false;
    next = paired() ? active : -1;
    if (next >= 0) {
        const auto frame = decode(portals_[next]);
        if (dot(body.center() - frame.anchor, frame.normal) >= normalRadius(body, portals_[next]) - Epsilon)
            next = -1;
    }
    if (paired() && next < 0) {
        for (int i = 0; i < 2; ++i) {
            const auto frame = decode(portals_[i]);
            const double before = dot(body.center() - frame.anchor, frame.normal);
            const double after = dot(result.center() - frame.anchor, frame.normal);
            if (before >= -Epsilon && after < normalRadius(result, portals_[i]) &&
                after >= -normalRadius(result, portals_[i]) && fitsPortal(result, portals_[i])) {
                next = i;
                break;
            }
        }
    }
    if (!canOccupy(result, next)) return false;
    if (next >= 0) {
        const auto frame = decode(portals_[next]);
        const double before = dot(body.center() - frame.anchor, frame.normal);
        const double after = dot(result.center() - frame.anchor, frame.normal);
        if (before >= -Epsilon && after < -Epsilon) {
            result = transformBody(result, portals_[next], portals_[1 - next]);
            next = 1 - next;
            crossed = true;
            if (!canOccupy(result, next)) return false;
        }
        const auto now = decode(portals_[next]);
        if (dot(result.center() - now.anchor, now.normal) >= normalRadius(result, portals_[next]) - Epsilon)
            next = -1;
    }
    return true;
}
bool CollisionWorld::recover(Player& player, PortalMotion& motion) const {
    if (canOccupy(player.body, motion.activePortal)) return true;
    if (!finite(player.body.position)) return false;
    // Choose the nearest valid tile-boundary correction, testing the whole box.
    std::vector<double> xs{0}, ys{0};
    const auto r = player.body.bounds();
    for (int x = 0; x <= MapWidth; ++x)
        for (double shift : {x * TileSize - r.left, x * TileSize - r.right})
            if (std::abs(shift) <= 60) xs.push_back(shift);
    for (int y = 0; y <= MapHeight; ++y)
        for (double shift : {y * TileSize - r.top, y * TileSize - r.bottom})
            if (std::abs(shift) <= 60) ys.push_back(shift);
    double best = std::numeric_limits<double>::infinity();
    Body chosen;
    for (double x : xs) for (double y : ys) {
        const double distance = x * x + y * y;
        if (distance >= best || distance > 3600) continue;
        Body candidate = player.body;
        candidate.position = candidate.position + Vec2{x, y};
        if (clear(candidate.bounds())) { best = distance; chosen = candidate; }
    }
    if (!std::isfinite(best)) return false;
    player.body = chosen;
    player.velocity = {};
    motion.activePortal = -1;
    return true;
}
bool CollisionWorld::move(Player& player, PortalMotion& motion, const MovementInput& input) const {
    // Check visible body pieces, including the part emerging from the other portal.
    auto touchesBoundary = [&]() {
        const auto touches = [](Rect r) {
            constexpr double tolerance = 1e-6;
            return !r.empty() && (r.left <= tolerance || r.top <= tolerance ||
                r.right >= WindowWidth - tolerance || r.bottom >= WindowHeight - tolerance);
        };
        const int active = motion.activePortal;
        if (paired() && active >= 0 && active < 2) {
            const auto projected = transformBody(player.body, portals_[active], portals_[1 - active]);
            return touches(clipToFront(player.body.bounds(), portals_[active])) ||
                   touches(clipToFront(projected.bounds(), portals_[1 - active]));
        }
        return touches(player.body.bounds());
    };
    if (!finite(player.velocity) || touchesBoundary() || !recover(player, motion) || touchesBoundary()) return false;
    motion.crossings = 0;
    auto blocked = [&](Vec2 delta) {
        Body result; int next; bool crossed;
        return !trial(player.body, motion.activePortal, delta, result, next, crossed);
    };
    const bool grounded = blocked({0, 0.02});
    const double acceleration = input.left ? -1.0 : (input.right ? 1.0 : 0.0);
    if (std::abs(player.velocity.x) <= 6.0)
        player.velocity.x = std::clamp(player.velocity.x + acceleration, -6.0, 6.0);
    else if (player.velocity.x * acceleration < 0) player.velocity.x += acceleration;
    else player.velocity.x *= 0.99;
    if (grounded && !input.left && !input.right) player.velocity.x *= 0.5;
    if (std::abs(player.velocity.x) < 0.01) player.velocity.x = 0;
    // Legacy -7 was immediately followed by +1 gravity before its first movement.
    if (input.jump && grounded) player.velocity.y = -6.0;
    else if (grounded && player.velocity.y >= 0) player.velocity.y = 0;
    else player.velocity.y = std::min(player.velocity.y + 1.0, 20.0);
    player.velocity.x = std::clamp(player.velocity.x, -1000.0, 1000.0);
    player.velocity.y = std::clamp(player.velocity.y, -1000.0, 1000.0);
    const int steps = std::max(1, static_cast<int>(std::ceil(std::max(std::abs(player.velocity.x), std::abs(player.velocity.y)) / 0.5)));
    const double dt = 1.0 / steps;
    auto commit = [&](const Body& body, int next, bool crossed) {
        if (crossed) {
            player.velocity = transformVector(player.velocity, portals_[1 - next], portals_[next]);
            ++motion.crossings;
        }
        player.body = body;
        motion.activePortal = next;
    };
    auto segment = [&](Vec2 delta) -> bool {
        Body result; int next; bool crossed;
        if (trial(player.body, motion.activePortal, delta, result, next, crossed)) {
            commit(result, next, crossed);
            return crossed;
        }
        double low = 0, high = 1;
        Body best = player.body;
        int bestNext = motion.activePortal;
        bool bestCrossed = false;
        for (int i = 0; i < 24; ++i) {
            const double middle = (low + high) * 0.5;
            if (trial(player.body, motion.activePortal, delta * middle, result, next, crossed)) {
                low = middle; best = result; bestNext = next; bestCrossed = crossed;
            } else high = middle;
        }
        Vec2 axis = std::abs(delta.x) > 0 ? Vec2{1, 0} : Vec2{0, 1};
        if (bestCrossed) axis = transformVector(axis, portals_[1 - bestNext], portals_[bestNext]);
        commit(best, bestNext, bestCrossed);
        player.velocity = player.velocity - axis * dot(player.velocity, axis);
        return bestCrossed;
    };
    for (int step = 0; step < steps; ++step) {
        const auto delta = player.velocity * dt;
        Body result; int next; bool crossed;
        if (trial(player.body, motion.activePortal, delta, result, next, crossed)) {
            commit(result, next, crossed);
        } else {
            Vec2 remaining{0, delta.y};
            if (std::abs(delta.x) > Epsilon && segment({delta.x, 0}))
                remaining = transformVector(remaining, portals_[1 - motion.activePortal], portals_[motion.activePortal]);
            if (std::abs(remaining.x) + std::abs(remaining.y) > Epsilon) segment(remaining);
        }
        if (touchesBoundary()) return false;
    }
    if (touchesBoundary()) return false;
    player.contacts = {blocked({0, 0.02}), blocked({0, -0.02}), blocked({-0.02, 0}), blocked({0.02, 0})};
    return canOccupy(player.body, motion.activePortal);
}
} // namespace por2
