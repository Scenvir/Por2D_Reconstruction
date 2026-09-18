#pragma once
#include "game.hpp"
#include <cstdint>
#include <filesystem>
#include <vector>

namespace por2 {
// CPU framebuffer is shared by the Win32 window and headless screenshots.
class Renderer {
public:
    Renderer();
    void draw(const Game& game, bool debug = true, bool grid = false);
    const std::uint32_t* pixels() const { return pixels_.data(); }
    void saveBitmap(const std::filesystem::path& path) const;
private:
    void rectangle(int x1, int y1, int x2, int y2, std::uint32_t color);
    void line(Vec2 a, Vec2 b, std::uint32_t color, int thickness = 1);
    void body(const Body& body, std::uint32_t color, const Portal* clip = nullptr);
    std::vector<std::uint32_t> pixels_;
};
} // namespace por2
