#pragma once
#include "types.hpp"
#include <optional>
#include <string>

namespace por2 {
struct Level {
    int id = 0;
    std::string name;
    std::string commentary;
    TileMap map;
    Body spawn;
    std::optional<Body> exit;
};
inline constexpr std::array<int, 15> Campaign{{0, 5, 6, 7, 8, 9, 10, 3, 14, 11, 12, 15, 13, 4, 16}};
Level makeLevel(int id);
} // namespace por2
