#include "por2/portal.hpp"
#include <limits>

namespace por2 {
bool Portal::occupies(int x, int y) const {
    if (!active()) return false;
    return horizontal() ? y == tile.y && x >= tile.x && x < tile.x + 3
                        : x == tile.x && y >= tile.y && y < tile.y + 3;
}
PortalFrame decode(const Portal& portal) {
    if (!portal.active()) throw std::invalid_argument("inactive portal");
    static constexpr std::array<PortalFrame, 8> frames{{
        {{0, 0}, {-1, 0}, {0, 1}}, {{0, 3}, {-1, 0}, {0, -1}},
        {{3, 0}, {0, -1}, {-1, 0}}, {{0, 0}, {0, -1}, {1, 0}},
        {{1, 0}, {1, 0}, {0, 1}}, {{1, 3}, {1, 0}, {0, -1}},
        {{3, 1}, {0, 1}, {-1, 0}}, {{0, 1}, {0, 1}, {1, 0}}
    }};
    auto result = frames.at(static_cast<std::size_t>(portal.code - 1));
    result.anchor = pixels(portal.tile) + result.anchor * TileSize;
    return result;
}
Vec2 transformVector(Vec2 vector, const Portal& from, const Portal& to) {
    const auto a = decode(from), b = decode(to);
    return b.normal * -dot(vector, a.normal) + b.tangent * dot(vector, a.tangent);
}
Vec2 transformPoint(Vec2 point, const Portal& from, const Portal& to) {
    return decode(to).anchor + transformVector(point - decode(from).anchor, from, to);
}
Body transformBody(const Body& body, const Portal& from, const Portal& to) {
    Body result;
    result.direction = vectorDirection(transformVector(directionVector(body.direction), from, to));
    result.position = transformPoint(body.center(), from, to) - Vec2{result.width() / 2, result.height() / 2};
    return result;
}
double normalRadius(const Body& body, const Portal& portal) {
    return (portal.horizontal() ? body.height() : body.width()) * 0.5;
}
bool fitsPortal(const Body& body, const Portal& portal) {
    if (!portal.active()) return false;
    const auto frame = decode(portal);
    const double middle = dot(body.center() - frame.anchor, frame.tangent);
    const double radius = (portal.horizontal() ? body.width() : body.height()) * 0.5;
    return middle - radius >= -Epsilon && middle + radius <= 60.0 + Epsilon;
}
bool intersectsPortal(const Body& body, const Portal& portal) {
    if (!fitsPortal(body, portal)) return false;
    const auto frame = decode(portal);
    return std::abs(dot(body.center() - frame.anchor, frame.normal)) <= normalRadius(body, portal) + Epsilon;
}
Rect clipToFront(Rect rectangle, const Portal& portal) {
    const auto frame = decode(portal);
    if (frame.normal.x > 0) rectangle.left = std::max(rectangle.left, frame.anchor.x);
    if (frame.normal.x < 0) rectangle.right = std::min(rectangle.right, frame.anchor.x);
    if (frame.normal.y > 0) rectangle.top = std::max(rectangle.top, frame.anchor.y);
    if (frame.normal.y < 0) rectangle.bottom = std::min(rectangle.bottom, frame.anchor.y);
    return rectangle;
}
std::optional<RayHit> castShot(const TileMap& map, Vec2 origin, Vec2 target) {
    if (!finite(origin) || !finite(target)) return std::nullopt;
    const auto delta = target - origin;
    const double length = std::hypot(delta.x, delta.y);
    if (length < Epsilon || !std::isfinite(length) || origin.x < 0 || origin.y < 0 ||
        origin.x >= WindowWidth || origin.y >= WindowHeight) return std::nullopt;
    const auto ray = delta / length;
    // A head exactly on an open portal plane may aim back into air. Sample on
    // the ray's outgoing side of that boundary instead of treating it as buried.
    const auto sample = origin + ray * (4 * Epsilon);
    Cell cell{static_cast<int>(std::floor(sample.x / TileSize)), static_cast<int>(std::floor(sample.y / TileSize))};
    if (map.at(cell.x, cell.y) != Tile::Empty) return std::nullopt;
    const int sx = ray.x > 0 ? 1 : -1, sy = ray.y > 0 ? 1 : -1;
    const double infinity = std::numeric_limits<double>::infinity();
    const double stepX = std::abs(ray.x) > Epsilon ? TileSize / std::abs(ray.x) : infinity;
    const double stepY = std::abs(ray.y) > Epsilon ? TileSize / std::abs(ray.y) : infinity;
    double nextX = std::isfinite(stepX) ? ((cell.x + (sx > 0)) * TileSize - origin.x) / ray.x : infinity;
    double nextY = std::isfinite(stepY) ? ((cell.y + (sy > 0)) * TileSize - origin.y) / ray.y : infinity;
    auto hit = [&](Cell candidate, Vec2 normal, double distance) -> std::optional<RayHit> {
        if (TileMap::contains(candidate.x, candidate.y) && map.at(candidate.x, candidate.y) != Tile::Empty)
            return RayHit{origin + ray * distance, candidate, normal, ray};
        return std::nullopt;
    };
    for (int count = 0; count < MapWidth + MapHeight + 4; ++count) {
        if (std::abs(nextX - nextY) < Epsilon) {
            // Supercover corner: both adjacent cells precede the diagonal.
            if (auto result = hit({cell.x + sx, cell.y}, {-sx, 0}, nextX)) return result;
            if (auto result = hit({cell.x, cell.y + sy}, {0, -sy}, nextY)) return result;
            cell.x += sx; cell.y += sy;
            const Vec2 normal = std::abs(ray.x) >= std::abs(ray.y) ? Vec2{-sx, 0} : Vec2{0, -sy};
            if (auto result = hit(cell, normal, nextX)) return result;
            nextX += stepX; nextY += stepY;
        } else if (nextX < nextY) {
            cell.x += sx;
            if (auto result = hit(cell, {-sx, 0}, nextX)) return result;
            nextX += stepX;
        } else {
            cell.y += sy;
            if (auto result = hit(cell, {0, -sy}, nextY)) return result;
            nextY += stepY;
        }
        if (!TileMap::contains(cell.x, cell.y)) break;
    }
    return std::nullopt;
}
bool validPlacement(const TileMap& map, const Portal& portal, const Portal& other) {
    if (!portal.active()) return false;
    const auto normal = decode(portal).normal;
    for (int i = 0; i < 3; ++i) {
        const int x = portal.tile.x + (portal.horizontal() ? i : 0), y = portal.tile.y + (portal.horizontal() ? 0 : i);
        const int frontX = x + static_cast<int>(normal.x), frontY = y + static_cast<int>(normal.y);
        if (map.at(x, y) != Tile::PortalSurface || other.occupies(x, y) || map.at(frontX, frontY) != Tile::Empty)
            return false;
    }
    return true;
}
std::optional<Portal> choosePortal(const TileMap& map, const Portal& other,
                                 Vec2 origin, Direction direction, const RayHit& hit) {
    (void)origin;
    const bool horizontal = hit.normal.y != 0;
    const Cell start{hit.tile.x - (horizontal ? 1 : 0), hit.tile.y - (horizontal ? 0 : 1)};
    const double incidence = dot(hit.ray, hit.normal);
    if (incidence >= -Epsilon) return std::nullopt;
    // Project head-to-feet onto the actual hit face along the shooting ray.
    const Vec2 feet = directionVector(direction) * -1;
    const Vec2 projected = feet - hit.ray * (dot(feet, hit.normal) / incidence);
    const Vec2 positiveTangent = horizontal ? Vec2{1, 0} : Vec2{0, 1};
    double sign = dot(projected, positiveTangent);
    if (std::abs(sign) < 1e-6) {
        const auto head = directionVector(direction);
        sign = dot(Vec2{-head.y, head.x}, positiveTangent); // Perpendicular-shot tie break.
    }
    const bool positive = sign >= 0;
    const int code = horizontal ? (hit.normal.y < 0 ? 3 : 7) + (positive ? 1 : 0)
                                : (hit.normal.x < 0 ? 1 : 5) + (positive ? 0 : 1);
    const Portal portal{start, code};
    return validPlacement(map, portal, other) ? std::optional<Portal>{portal} : std::nullopt;
}
} // namespace por2
