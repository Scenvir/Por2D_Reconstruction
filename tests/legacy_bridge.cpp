#include "legacy_bridge.hpp"
#include "legacy_stubs/graphics.h"
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreturn-type"
#pragma clang diagnostic ignored "-Wparentheses"
#pragma clang diagnostic ignored "-Wbitwise-op-parentheses"
#pragma clang diagnostic ignored "-Wlogical-op-parentheses"
#pragma clang diagnostic ignored "-Winvalid-utf8"
#pragma clang diagnostic ignored "-Wmissing-braces"
#pragma clang diagnostic ignored "-Wempty-body"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
#pragma GCC diagnostic ignored "-Wparentheses"
#endif
#define main original_game_main
#include "../../NewlyUpdateMap.cpp"
#undef main
#undef blsize
#undef hitsize
#undef reveal_portal_direction
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

// Only maps and the valid portal transforms remain legacy invariants.
// The unsafe integer collision/placement behavior is deliberately not an oracle.
namespace legacy {
por2::Level level(int id) {
    level_reset();
    cx = cy = endx = endy = endd = 0;
    const std::array<void(*)(), 16> maps{{map0, map1, map2, map3, map4, map5, map6, map7,
                                        map8, map9, map10, map11, map12, map13, map14, map15}};
    maps.at(id)();
    por2::Level result;
    result.id = id;
    result.spawn = {{cx, cy}, por2::Direction::Up};
    if (id != 1 && id != 2) result.exit = por2::Body{{endx, endy}, por2::Direction::Up};
    for (int x = 0; x < 50; ++x) for (int y = 0; y < 30; ++y)
        result.map.set(x, y, static_cast<por2::Tile>(block[x][y]));
    return result;
}
por2::Vec2 transform(por2::Vec2 value, const std::array<por2::Portal, 2>& portals, bool velocity) {
    for (int i = 0; i < 2; ++i) portal[i] = {portals[i].tile.x, portals[i].tile.y, portals[i].code};
    cx = 900; cy = 500; chd = 0; tele = 0;
    reload_position();
    const int component = velocity ? 2 : 0;
    return {trans(static_cast<int>(value.x), static_cast<int>(value.y), 0, component),
            trans(static_cast<int>(value.x), static_cast<int>(value.y), 0, component + 1)};
}
CrossingLock crossingLock() {
    level_reset();
    map0();
    portal[0] = {36, 15, 1};
    portal[1] = {12, 15, 5};
    cx = 713; cy = 301; chd = 0;
    cvx = 5; cvy = 0; tele = 0; d_flag = 0; is_on_ground = 0;
    reload_position();
    return {{{cx, cy}, static_cast<por2::Direction>(chd)}, is_in_portal};
}
}
