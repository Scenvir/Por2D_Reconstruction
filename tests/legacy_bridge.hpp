#pragma once
#include "por2/level.hpp"
#include "por2/portal.hpp"
namespace legacy {
por2::Level level(int id);
por2::Vec2 transform(por2::Vec2 value, const std::array<por2::Portal, 2>& portals, bool velocity);
struct CrossingLock { por2::Body body; int flag; };
CrossingLock crossingLock();
}
