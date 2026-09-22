#include "por2/tutorial.hpp"

namespace por2 {
Tutorial::Tutorial(){reset(0);}
void Tutorial::resetScene(Direction direction){
    Level level;level.id=-1;level.name="教学演示";
    level.map.fill(Tile::Solid,12,7,12,20);
    level.map.fill(Tile::PortalSurface,36,7,36,20);
    level.map.fill(Tile::Solid,12,21,36,21);
    if(stage==3){
        level.map.fill(Tile::PortalSurface,12,7,36,7);
        level.map.fill(Tile::PortalSurface,12,21,36,21);
    }
    if(stage==1||stage==2)level.map.fill(Tile::PortalSurface,12,15,12,17);
    else level.map.fill(Tile::PortalSurface,12,7,12,20);
    level.spawn={{stage==5?300:430,(static_cast<int>(direction)%2)?402:362},direction};
    scene=Game(level);movement={};aim.reset();attempted=accepted=false;initialLock=-1;heldFrames=0;
    if(stage>=4){
        // The third mapping example places only the blue portal with a reversed head.
        if(stage==4&&phase==2)scene.player_.body.direction=Direction::Down;
        scene.shoot({0,{260,389}});
        scene.player_.body.direction=direction;
        scene.shoot({1,{720,389}});
    }
}
void Tutorial::reset(int step){stage=std::clamp(step,0,5);frame=phase=0;resetScene();}
void Tutorial::update(){
    const int cycle=stage==0?240:stage==1?240:stage==2?360:stage==4?3*MappingPhaseFrames:480;
    if(frame>=cycle){frame=0;resetScene();}
    phase=stage==0?frame/60:stage==1||stage==2||stage==3?frame/120:0;
    if(stage>0&&stage<4&&frame%120==0)resetScene(stage==3?static_cast<Direction>(phase):Direction::Up);
    movement={};
    if(stage==0){
        movement={phase==2||phase==3,phase==0||phase==1,phase==1||phase==3};
        InputFrame input;input.movement=movement;scene.tick(input);
    }else if(stage<4){
        const Vec2 target=stage==1?Vec2{260,phase==0?330:250}:stage==2?Vec2{260,310+phase*20}:Vec2{260,330};
        aim=Shot{0,target};
        scene.tick({});
        if(frame%120==65){attempted=true;accepted=scene.shoot(*aim);}
    }else if(stage==4){
        phase=frame/MappingPhaseFrames;
        const int localFrame=frame%MappingPhaseFrames;
        if(localFrame==0)resetScene(phase==1?Direction::Down:Direction::Up);
        // Walk continuously through both mouths, then rest clear of the exit.
        // Keep advancing physics even while resting; never freeze a partial traversal.
        movement.left=localFrame>=60&&localFrame<115;
        InputFrame input;input.movement=movement;scene.tick(input);
    }else {
        if(initialLock<0){
            movement.left=true;InputFrame input;input.movement=movement;scene.tick(input);
            initialLock=scene.traversal().lockedPortal;
        }else {
            ++heldFrames;
            phase=heldFrames<100?0:heldFrames<200?1:2;
            aim=Shot{phase==0?initialLock:1-initialLock,{phase==0?260:720,270}};
            if(heldFrames==40||heldFrames==140){attempted=true;accepted=scene.shoot(*aim);}
            if(heldFrames==100){attempted=accepted=false;}
            if(heldFrames>=200&&heldFrames%12==0){movement.left=true;InputFrame input;input.movement=movement;scene.tick(input);}
        }
    }
    ++frame;
}
}
