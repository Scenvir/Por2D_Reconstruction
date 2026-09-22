#pragma once
#include "game.hpp"

namespace por2 {
// A separate live game: teaching never changes the player's level or recording.
class Tutorial {
public:
    static constexpr int MappingPhaseFrames=300;
    int stage=0, frame=0, phase=0, heldFrames=0;
    Game scene;
    MovementInput movement;
    std::optional<Shot> aim;
    bool attempted=false, accepted=false;
    int initialLock=-1;
    Tutorial();
    void reset(int step);
    void update();
private:
    void resetScene(Direction direction=Direction::Up);
};
}
