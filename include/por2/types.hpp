#pragma once

#include <array>
#include <cmath>
#include <algorithm>
#include <stdexcept>

namespace por2 {
inline constexpr int TileSize = 20;
inline constexpr int MapWidth = 50;
inline constexpr int MapHeight = 30;
inline constexpr int WindowWidth = MapWidth * TileSize;
inline constexpr int WindowHeight = MapHeight * TileSize;
inline constexpr int TickMilliseconds = 20;

struct Vec2 {
    double x = 0;
    double y = 0;
    constexpr Vec2() = default;
    template<class X, class Y>
    constexpr Vec2(X xValue, Y yValue) : x(static_cast<double>(xValue)), y(static_cast<double>(yValue)) {}
};
inline Vec2 operator+(Vec2 a, Vec2 b) { return {a.x + b.x, a.y + b.y}; }
inline Vec2 operator-(Vec2 a, Vec2 b) { return {a.x - b.x, a.y - b.y}; }
inline Vec2 operator*(Vec2 a, double scale) { return {a.x * scale, a.y * scale}; }
inline Vec2 operator/(Vec2 a, double scale) { return {a.x / scale, a.y / scale}; }
inline bool operator==(Vec2 a, Vec2 b) { return a.x == b.x && a.y == b.y; }
inline bool operator!=(Vec2 a, Vec2 b) { return !(a == b); }
inline double dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }
inline bool finite(Vec2 a) { return std::isfinite(a.x) && std::isfinite(a.y); }
inline constexpr double Epsilon = 1e-7;
struct Cell { int x = 0; int y = 0; };
inline Vec2 pixels(Cell cell) { return {cell.x * TileSize, cell.y * TileSize}; }

enum class Direction { Up, Right, Down, Left };
enum class Tile { Empty, PortalSurface, Solid };
inline Vec2 directionVector(Direction direction) {
    constexpr std::array<Vec2, 4> vectors{{{0, -1}, {1, 0}, {0, 1}, {-1, 0}}};
    return vectors.at(static_cast<std::size_t>(direction));
}
inline Direction vectorDirection(Vec2 vector) {
    if (std::abs(vector.x) > std::abs(vector.y)) return vector.x > 0 ? Direction::Right : Direction::Left;
    return vector.y > 0 ? Direction::Down : Direction::Up;
}
struct Rect {
    double left, top, right, bottom;
    bool empty() const { return right - left <= Epsilon || bottom - top <= Epsilon; }
};

struct Body {
    Vec2 position;
    Direction direction = Direction::Up;
    int width() const { return (static_cast<int>(direction) & 1) ? 58 : 18; }
    int height() const { return (static_cast<int>(direction) & 1) ? 18 : 58; }
    Vec2 farCorner() const { return position + Vec2{width(), height()}; }
    Vec2 center() const { return position + Vec2{width() / 2, height() / 2}; }
    Vec2 head() const {
        return center() + directionVector(direction) * 20.0;
    }
    Rect bounds() const { return {position.x, position.y, position.x + width(), position.y + height()}; }
};

struct Contacts {
    bool ground = false;
    bool ceiling = false;
    bool left = false;
    bool right = false;
};

struct Player {
    Body body;
    Vec2 velocity;
    Contacts contacts;
};

class TileMap {
public:
    static bool contains(int x, int y) {
        return x >= 0 && x < MapWidth && y >= 0 && y < MapHeight;
    }
    Tile at(int x, int y) const {
        return contains(x, y) ? cells_[x][y] : Tile::Solid;
    }
    void set(int x, int y, Tile tile) {
        if (!contains(x, y)) throw std::out_of_range("map coordinates");
        cells_[x][y] = tile;
    }
    void fill(Tile tile, int x1, int y1, int x2, int y2) {
        if (!contains(x1, y1) || !contains(x2, y2) || x1 > x2 || y1 > y2)
            throw std::out_of_range("map rectangle");
        for (int x = x1; x <= x2; ++x)
            for (int y = y1; y <= y2; ++y) set(x, y, tile);
    }
private:
    std::array<std::array<Tile, MapHeight>, MapWidth> cells_{};
};
} // namespace por2
