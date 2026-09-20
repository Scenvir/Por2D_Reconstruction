#include "por2/render.hpp"
#include <algorithm>
#include <cstdlib>
#include <fstream>

namespace por2 {
namespace {
constexpr std::uint32_t PlayerColor = 0xE6E67D;
constexpr std::array<std::uint32_t, 2> PortalColors{{0x3296FF, 0xFF9632}};
constexpr std::array<std::array<std::uint32_t, 3>, 2> PortalRamps{{
    {{0x17477B, 0x3296FF, 0xBFEAFF}}, {{0x88401C, 0xFF9632, 0xFFE7AF}}
}};
std::uint32_t gradient(int id, double progress) {
    progress = std::clamp(progress, 0.0, 1.0) * 2.0;
    const int segment = progress < 1 ? 0 : 1;
    const double weight = progress - segment;
    const auto a = PortalRamps[id][segment], b = PortalRamps[id][segment + 1];
    std::uint32_t color = 0;
    for (int shift : {0, 8, 16}) {
        const double value = ((a >> shift) & 255) * (1 - weight) + ((b >> shift) & 255) * weight;
        color |= static_cast<std::uint32_t>(std::lround(value)) << shift;
    }
    return color;
}
}
Renderer::Renderer() : pixels_(WindowWidth * WindowHeight) {}

void Renderer::rectangle(int x1, int y1, int x2, int y2, std::uint32_t color) {
    x1 = std::clamp(x1, 0, WindowWidth);
    x2 = std::clamp(x2 + 1, 0, WindowWidth);
    y1 = std::clamp(y1, 0, WindowHeight);
    y2 = std::clamp(y2 + 1, 0, WindowHeight);
    if (x1 >= x2 || y1 >= y2) return;
    for (int y = y1; y < y2; ++y)
        std::fill(pixels_.begin() + y * WindowWidth + x1,
                  pixels_.begin() + y * WindowWidth + x2, color);
}

void Renderer::line(Vec2 a, Vec2 b, std::uint32_t color, int thickness) {
    int x = static_cast<int>(std::lround(a.x)), y = static_cast<int>(std::lround(a.y));
    const int endX = static_cast<int>(std::lround(b.x)), endY = static_cast<int>(std::lround(b.y));
    const int dx = std::abs(endX - x);
    const int dy = -std::abs(endY - y);
    const int sx = x < endX ? 1 : -1;
    const int sy = y < endY ? 1 : -1;
    int error = dx + dy;
    for (;;) {
        rectangle(x, y, x + thickness - 1, y + thickness - 1, color);
        if (x == endX && y == endY) break;
        const int doubled = error * 2;
        if (doubled >= dy) { error += dy; x += sx; }
        if (doubled <= dx) { error += dx; y += sy; }
    }
}

void Renderer::body(const Body& value, std::uint32_t color, const Portal* clip) {
    const auto r = clip ? clipToFront(value.bounds(), *clip) : value.bounds();
    if (r.empty()) return;
    rectangle(static_cast<int>(std::floor(r.left)), static_cast<int>(std::floor(r.top)),
              static_cast<int>(std::ceil(r.right)) - 1, static_cast<int>(std::ceil(r.bottom)) - 1, color);
}

void Renderer::draw(const Game& game, bool debug, bool grid, std::optional<Shot> preview) {
    std::fill(pixels_.begin(), pixels_.end(), 0);
    if (grid) {
        for (int x = 0; x < WindowWidth; x += TileSize) line({x, 0}, {x, WindowHeight - 1}, 0x505050);
        for (int y = 0; y < WindowHeight; y += TileSize) line({0, y}, {WindowWidth - 1, y}, 0x505050);
    }
    if (game.level().exit) {
        const auto& exit=*game.level().exit;
        body(exit, 0xC8C8C8);
        const auto head=exit.head();
        const int x=static_cast<int>(std::lround(head.x)),y=static_cast<int>(std::lround(head.y));
        rectangle(x-1,y-1,x+1,y+1,0x969696);
    }
    const int entry = game.traversal().portalIndex;
    if (game.traversal().projection && entry >= 0)
        body(*game.traversal().projection, PlayerColor, &game.portals()[1 - entry]);
    body(game.player().body, PlayerColor, entry >= 0 ? &game.portals()[entry] : nullptr);
    // Walls occlude both the real and projected bodies, as in the original.
    for (int x = 0; x < MapWidth; ++x)
        for (int y = 0; y < MapHeight; ++y) {
            const auto tile = game.level().map.at(x, y);
            if (tile == Tile::Empty) continue;
            rectangle(x * TileSize, y * TileSize, (x + 1) * TileSize, (y + 1) * TileSize, 0);
            rectangle(x * TileSize + 1, y * TileSize + 1, x * TileSize + 19, y * TileSize + 19,
                      tile == Tile::PortalSurface ? 0xFFFFFF : 0x323232);
        }
    for (int id = 0; id < 2; ++id) {
        const auto& portal = game.portals()[id];
        if (!portal.active()) continue;
        const auto frame = decode(portal);
        for (int i = 0; i < 3; ++i) {
            const Cell tile{portal.tile.x + (portal.horizontal() ? i : 0), portal.tile.y + (portal.horizontal() ? 0 : i)};
            for (int along = 1; along < TileSize; ++along) {
                const Vec2 position = por2::pixels(tile) + (portal.horizontal() ? Vec2{along, 10} : Vec2{10, along});
                const double progress = dot(position - frame.anchor, frame.tangent) / 60.0;
                const auto color = gradient(id, progress);
                if (portal.horizontal()) rectangle(tile.x * TileSize + along, tile.y * TileSize + 1,
                                                  tile.x * TileSize + along, tile.y * TileSize + 19, color);
                else rectangle(tile.x * TileSize + 1, tile.y * TileSize + along,
                               tile.x * TileSize + 19, tile.y * TileSize + along, color);
            }
            // The exposed edge makes the facing direction visible as well.
            const Vec2 start = por2::pixels(tile);
            if (frame.normal.x < 0) line(start + Vec2{0, 1}, start + Vec2{0, 19}, PortalColors[id], 2);
            if (frame.normal.x > 0) line(start + Vec2{19, 1}, start + Vec2{19, 19}, PortalColors[id], 2);
            if (frame.normal.y < 0) line(start + Vec2{1, 0}, start + Vec2{19, 0}, PortalColors[id], 2);
            if (frame.normal.y > 0) line(start + Vec2{1, 19}, start + Vec2{19, 19}, PortalColors[id], 2);
        }
    }
    for (const auto& trace : game.traces()) line(trace.origin, trace.end, PortalColors[trace.portal], 3);
    if (preview && !game.finished()) {
        // Use the real firing rules on a copy: preview never changes live portals.
        std::array<bool, 2> available{};
        std::array<Portal, 2> candidates{};
        for (int id = 0; id < 2; ++id) {
            auto portals = game.portals();
            std::vector<ShotTrace> traces;
            available[id] = firePortal(game.level().map, game.player(), game.motion(),
                                       portals, Shot{id, preview->target}, traces);
            candidates[id] = portals[id];
        }
        const bool valid = available[0] || available[1];
        const Vec2 origin = game.traversal().aimOrigin;
        const auto hit = castShot(game.level().map, origin, preview->target);
        const Vec2 end = hit ? hit->point : preview->target;
        const auto color = valid ? 0x50E080u : 0xEF7070u;
        const Vec2 ray = end - origin;
        const double length = std::sqrt(dot(ray, ray));
        for (double d = 0; d < length; d += 12)
            line(origin + ray * (d / length), origin + ray * (std::min(d + 4, length) / length), color);
        if (valid) {
            for (int id = 0; id < 2; ++id) {
            if (!available[id]) continue;
            const auto& portal = candidates[id];
            // Only outline the three tiles, preserving visibility of the wall.
            for (int i = 0; i < 3; ++i) {
                const Vec2 tile = por2::pixels(portal.tile) + (portal.horizontal() ? Vec2{i*20,0} : Vec2{0,i*20});
                for (int n = 1; n < 20; ++n) {
                    const auto shade = color;
                    if (portal.horizontal()) {
                        line(tile+Vec2{n,1},tile+Vec2{n,2},shade);
                        line(tile+Vec2{n,18},tile+Vec2{n,19},shade);
                    } else {
                        line(tile+Vec2{1,n},tile+Vec2{2,n},shade);
                        line(tile+Vec2{18,n},tile+Vec2{19,n},shade);
                    }
                }
            }
            const Vec2 middle = por2::pixels(portal.tile) + (portal.horizontal() ? Vec2{30,10} : Vec2{10,30});
            // Headward direction matches the tutorial diagram (light to dark).
            const Vec2 forward = decode(portal).tangent * -1;
            const Vec2 side{-forward.y,forward.x};
            const Vec2 separation=std::abs(side.x)>0.5?Vec2{1,0}:Vec2{0,1};
            const Vec2 center = middle + separation * (available[0] && available[1] ? (id == 0 ? -5.0 : 5.0) : 0.0);
            const Vec2 tip = center + forward * 8;
            // Separate the arrow from an existing same-color portal only where they overlap.
            const auto& existing = game.portals()[id];
            bool overlapsSameColor = false;
            for (int i = 0; i < 3 && existing.active(); ++i) {
                const int x = portal.tile.x + (portal.horizontal() ? i : 0);
                const int y = portal.tile.y + (portal.horizontal() ? 0 : i);
                overlapsSameColor = overlapsSameColor || existing.occupies(x, y);
            }
            if (overlapsSameColor) {
                const Vec2 inset{-1,-1};
                line(center-forward*8+inset,tip+inset,0x101822,4);
                line(tip+inset,tip-forward*5+side*3+inset,0x101822,4);
                line(tip+inset,tip-forward*5-side*3+inset,0x101822,4);
            }
            line(center-forward*8,tip,PortalColors[id],2);
            line(tip,tip-forward*5+side*3,PortalColors[id],2);
            line(tip,tip-forward*5-side*3,PortalColors[id],2);
            }
        } else {
            line(end-Vec2{4,4},end+Vec2{4,4},color,2);
            line(end+Vec2{-4,4},end+Vec2{4,-4},color,2);
        }
    }
    // Draw last so aiming previews cannot hide the locked portal's thin outline.
    const int locked = game.traversal().lockedPortal;
    if (locked >= 0 && game.portals()[locked].active()) {
        const auto& portal = game.portals()[locked];
        const Vec2 topLeft = por2::pixels(portal.tile);
        const Vec2 bottomRight = topLeft + (portal.horizontal() ? Vec2{60, 20} : Vec2{20, 60});
        constexpr std::uint32_t LockColor = 0xFFD700;
        line(topLeft, {bottomRight.x, topLeft.y}, LockColor);
        line({bottomRight.x, topLeft.y}, bottomRight, LockColor);
        line(bottomRight, {topLeft.x, bottomRight.y}, LockColor);
        line({topLeft.x, bottomRight.y}, topLeft, LockColor);
    }
    if (debug) {
        const auto head = game.traversal().aimOrigin;
        const int x = static_cast<int>(std::lround(head.x)), y = static_cast<int>(std::lround(head.y));
        rectangle(x - 1, y - 1, x + 1, y + 1, 0xFF0000);
    }
}

void Renderer::saveBitmap(const std::filesystem::path& path) const {
    std::ofstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("cannot create screenshot");
    auto word = [&](std::uint32_t value, int count) {
        for (int i = 0; i < count; ++i) file.put(static_cast<char>((value >> (8 * i)) & 255));
    };
    file.put('B'); file.put('M');
    word(54 + WindowWidth * WindowHeight * 4, 4);
    word(0, 4); word(54, 4); word(40, 4);
    word(WindowWidth, 4); word(WindowHeight, 4);
    word(1, 2); word(32, 2); word(0, 4);
    word(WindowWidth * WindowHeight * 4, 4);
    word(0, 4); word(0, 4); word(0, 4); word(0, 4);
    for (int y = WindowHeight - 1; y >= 0; --y)
        for (int x = 0; x < WindowWidth; ++x) word(pixels_[y * WindowWidth + x], 4);
    if (!file) throw std::runtime_error("cannot write screenshot");
}
} // namespace por2
