// Map coordinates imported verbatim from NewlyUpdateMap.cpp.
// Regenerate with: python tools/import_levels.py
#include "por2/level.hpp"

namespace por2 {
namespace {
Level map0() {
    Level level;
    level.id = 0;
    level.name = u8"概念";
    level.commentary = u8"一种内在的结构，一个巧妙的概念。";
    Body exit;
    level.spawn.position.x = 320;
    level.spawn.position.y = 360;
    level.map.fill(static_cast<Tile>(2), 8, 6, 40, 24);
    level.map.fill(static_cast<Tile>(0), 13, 10, 35, 20);
    level.map.fill(static_cast<Tile>(1), 13, 9, 35, 9);
    level.map.fill(static_cast<Tile>(1), 13, 21, 35, 21);
    level.map.fill(static_cast<Tile>(1), 12, 10, 12, 20);
    level.map.fill(static_cast<Tile>(1), 36, 10, 36, 20);
    exit.position.x = 580;
    exit.position.y = 360;
    exit.direction = Direction::Up;
    level.exit = exit;
    return level;
}

Level map1() {
    Level level;
    level.id = 1;
    level.name = u8"实验地图一";
    level.commentary = u8"";
    // Original test map omitted its spawn; use a safe explicit default.
    level.spawn.position = {200, 200};
    for (int i = 0; i < 50; i++)
        level.map.set(i, 4, static_cast<Tile>(1));
    level.map.set(3, 4, static_cast<Tile>(0));
    level.map.set(5, 4, static_cast<Tile>(0));
    level.map.set(6, 4, static_cast<Tile>(0));
    level.map.set(7, 3, static_cast<Tile>(1));
    level.map.set(20, 3, static_cast<Tile>(1));
    level.map.set(10, 4, static_cast<Tile>(0));
    level.map.set(11, 4, static_cast<Tile>(0));
    level.map.set(12, 4, static_cast<Tile>(0));
    level.map.set(13, 4, static_cast<Tile>(0));
    level.map.set(12, 6, static_cast<Tile>(1));
    level.map.set(11, 7, static_cast<Tile>(1));
    level.map.set(13, 6, static_cast<Tile>(1));
    level.map.set(14, 6, static_cast<Tile>(1));
    level.map.set(15, 7, static_cast<Tile>(1));
    level.map.set(15, 6, static_cast<Tile>(1));
    for (int i = 0; i < 30; i++)
        level.map.set(i, 8, static_cast<Tile>(1));
    for (int i = 0; i < 50; i++)
        level.map.set(i, 13, static_cast<Tile>(1));
    level.map.set(20, 11, static_cast<Tile>(1));
    level.map.set(20, 10, static_cast<Tile>(1));
    level.map.set(20, 12, static_cast<Tile>(1));
    return level;
}

Level map2() {
    Level level;
    level.id = 2;
    level.name = u8"实验地图二";
    level.commentary = u8"";
    level.spawn.position.x = 200;
    level.spawn.position.y = 200;
    for (int i = 4; i < 26; i++)
    {
        level.map.set(4, i, static_cast<Tile>(1));
        level.map.set(30, i, static_cast<Tile>(1));
    }
    for (int i = 4; i < 31; i++)
    {
        level.map.set(i, 4, static_cast<Tile>(1));
        level.map.set(i, 26, static_cast<Tile>(1));
    }
    for (int i = 6; i < 24; i++)
    {
        level.map.set(i, 24, static_cast<Tile>(1));
    }
    level.map.fill(static_cast<Tile>(1), 15, 20, 20, 20);
    level.map.fill(static_cast<Tile>(1), 2, 2, 3, 28);
    level.map.fill(static_cast<Tile>(1), 2, 2, 32, 4);
    level.map.fill(static_cast<Tile>(1), 31, 4, 32, 28);
    level.map.fill(static_cast<Tile>(1), 2, 26, 32, 28);
    return level;
}

Level map3() {
    Level level;
    level.id = 3;
    level.name = u8"裂缝";
    level.commentary = u8"从裂缝中，窥见深渊的一角。";
    Body exit;
    level.spawn.position.x = 200;
    level.spawn.position.y = 320;
    level.spawn.direction = Direction::Up;
    level.map.fill(static_cast<Tile>(2), 5, 4, 40, 25);
    level.map.fill(static_cast<Tile>(1), 8, 16, 8, 18);
    level.map.fill(static_cast<Tile>(0), 9, 15, 15, 20);
    level.map.fill(static_cast<Tile>(1), 12, 21, 14, 21);
    level.map.fill(static_cast<Tile>(1), 18, 22, 18, 24);
    level.map.fill(static_cast<Tile>(1), 12, 14, 14, 14);
    level.map.fill(static_cast<Tile>(0), 16, 15, 19, 17);
    level.map.fill(static_cast<Tile>(0), 19, 15, 23, 25);
    level.map.fill(static_cast<Tile>(2), 21, 15, 21, 22);
    level.map.fill(static_cast<Tile>(1), 24, 22, 24, 24);
    level.map.fill(static_cast<Tile>(0), 22, 7, 23, 14);
    level.map.fill(static_cast<Tile>(1), 24, 13, 24, 15);
    level.map.fill(static_cast<Tile>(0), 16, 7, 31, 11);
    level.map.fill(static_cast<Tile>(2), 25, 7, 25, 10);
    level.map.fill(static_cast<Tile>(1), 17, 6, 19, 6);
    level.map.fill(static_cast<Tile>(1), 17, 12, 19, 12);
    level.map.fill(static_cast<Tile>(1), 32, 8, 32, 10);
    level.map.fill(static_cast<Tile>(2), 9, 20, 10, 20);
    level.map.fill(static_cast<Tile>(2), 9, 19, 9, 19);
    exit.position.x = 580;
    exit.position.y = 180;
    exit.direction = Direction::Up;
    level.exit = exit;
    return level;
}

Level map4() {
    Level level;
    level.id = 4;
    level.name = u8"深渊";
    level.commentary = u8"当你凝视深渊的时候，深渊也在凝视你。";
    Body exit;
    level.spawn.position.x = 240;
    level.spawn.position.y = 320;
    level.spawn.direction = Direction::Up;
    level.map.fill(static_cast<Tile>(2), 5, 4, 40, 25);
    level.map.fill(static_cast<Tile>(1), 7, 16, 7, 18);
    level.map.fill(static_cast<Tile>(0), 8, 15, 15, 20);
    level.map.fill(static_cast<Tile>(1), 9, 21, 11, 21);
    level.map.fill(static_cast<Tile>(1), 18, 22, 18, 24);
    level.map.fill(static_cast<Tile>(1), 13, 14, 15, 14);
    level.map.fill(static_cast<Tile>(0), 16, 15, 19, 17);
    level.map.fill(static_cast<Tile>(0), 19, 15, 24, 25);
    level.map.fill(static_cast<Tile>(2), 20, 15, 20, 21);
    level.map.fill(static_cast<Tile>(2), 20, 23, 20, 25);
    level.map.fill(static_cast<Tile>(0), 23, 7, 24, 14);
    level.map.fill(static_cast<Tile>(1), 25, 13, 25, 15);
    level.map.fill(static_cast<Tile>(0), 16, 7, 31, 11);
    level.map.fill(static_cast<Tile>(2), 25, 7, 25, 10);
    level.map.fill(static_cast<Tile>(1), 20, 6, 22, 6);
    level.map.fill(static_cast<Tile>(1), 15, 8, 15, 10);
    level.map.fill(static_cast<Tile>(1), 32, 8, 32, 10);
    level.map.fill(static_cast<Tile>(2), 23, 19, 23, 23);
    level.map.fill(static_cast<Tile>(2), 23, 13, 23, 14);
    level.map.fill(static_cast<Tile>(2), 8, 20, 8, 20);
    exit.position.x = 580;
    exit.position.y = 180;
    exit.direction = Direction::Up;
    level.exit = exit;
    return level;
}

Level map5() {
    Level level;
    level.id = 5;
    level.name = u8"高山";
    level.commentary = u8"攀登极限，因为山就在那里。";
    Body exit;
    level.spawn.position.x = 300;
    level.spawn.position.y = 340;
    level.map.fill(static_cast<Tile>(2), 8, 6, 40, 24);
    level.map.fill(static_cast<Tile>(0), 13, 10, 35, 20);
    level.map.fill(static_cast<Tile>(2), 19, 18, 19, 20);
    level.map.fill(static_cast<Tile>(2), 20, 18, 20, 20);
    level.map.fill(static_cast<Tile>(2), 21, 19, 21, 20);
    level.map.fill(static_cast<Tile>(2), 22, 20, 22, 20);
    level.map.fill(static_cast<Tile>(2), 24, 10, 26, 14);
    level.map.fill(static_cast<Tile>(1), 24, 11, 24, 13);
    level.map.fill(static_cast<Tile>(1), 12, 18, 12, 20);
    level.map.fill(static_cast<Tile>(2), 31, 18, 31, 20);
    level.map.fill(static_cast<Tile>(1), 36, 18, 36, 20);
    exit.position.x = 660;
    exit.position.y = 360;
    exit.direction = Direction::Up;
    level.exit = exit;
    return level;
}

Level map6() {
    Level level;
    level.id = 6;
    level.name = u8"旋转";
    level.commentary = u8"换个角度看世界。";
    Body exit;
    level.spawn.position.x = 300;
    level.spawn.position.y = 340;
    level.map.fill(static_cast<Tile>(2), 8, 6, 40, 24);
    level.map.fill(static_cast<Tile>(0), 13, 10, 35, 20);
    level.map.fill(static_cast<Tile>(2), 19, 15, 23, 20);
    level.map.fill(static_cast<Tile>(2), 14, 15, 31, 15);
    level.map.fill(static_cast<Tile>(2), 29, 10, 29, 13);
    level.map.fill(static_cast<Tile>(2), 28, 10, 28, 12);
    level.map.fill(static_cast<Tile>(2), 27, 10, 27, 11);
    level.map.fill(static_cast<Tile>(2), 26, 10, 26, 10);
    level.map.fill(static_cast<Tile>(1), 12, 11, 12, 13);
    level.map.fill(static_cast<Tile>(1), 12, 16, 12, 20);
    level.map.fill(static_cast<Tile>(1), 20, 15, 22, 15);
    level.map.fill(static_cast<Tile>(1), 23, 17, 23, 20);
    level.map.fill(static_cast<Tile>(1), 24, 21, 33, 21);
    exit.position.x = 560;
    exit.position.y = 360;
    exit.direction = Direction::Up;
    level.exit = exit;
    return level;
}

Level map7() {
    Level level;
    level.id = 7;
    level.name = u8"远跳";
    level.commentary = u8"这是角色的一小步，却是整个世界的一大步。";
    Body exit;
    level.spawn.position.x = 300;
    level.spawn.position.y = 340;
    level.map.fill(static_cast<Tile>(2), 8, 6, 40, 24);
    level.map.fill(static_cast<Tile>(0), 13, 10, 35, 20);
    level.map.fill(static_cast<Tile>(2), 20, 14, 22, 20);
    level.map.fill(static_cast<Tile>(2), 23, 14, 31, 16);
    level.map.fill(static_cast<Tile>(1), 15, 9, 17, 9);
    level.map.fill(static_cast<Tile>(1), 15, 21, 17, 21);
    level.map.fill(static_cast<Tile>(1), 12, 10, 12, 12);
    level.map.fill(static_cast<Tile>(1), 32, 21, 34, 21);
    level.map.fill(static_cast<Tile>(1), 22, 18, 22, 20);
    exit.position.x = 580;
    exit.position.y = 360;
    exit.direction = Direction::Up;
    level.exit = exit;
    return level;
}

Level map8() {
    Level level;
    level.id = 8;
    level.name = u8"大脑";
    level.commentary = u8"只送大脑。";
    Body exit;
    level.spawn.position.x = 340;
    level.spawn.position.y = 340;
    level.map.fill(static_cast<Tile>(2), 8, 6, 40, 24);
    level.map.fill(static_cast<Tile>(0), 13, 10, 35, 20);
    level.map.fill(static_cast<Tile>(2), 15, 14, 31, 16);
    level.map.fill(static_cast<Tile>(2), 22, 11, 24, 20);
    level.map.fill(static_cast<Tile>(1), 18, 21, 20, 21);
    level.map.fill(static_cast<Tile>(1), 26, 21, 28, 21);
    level.map.fill(static_cast<Tile>(1), 12, 13, 12, 15);
    level.map.fill(static_cast<Tile>(1), 36, 13, 36, 15);
    level.map.fill(static_cast<Tile>(1), 22, 9, 24, 9);
    exit.position.x = 580;
    exit.position.y = 360;
    exit.direction = Direction::Up;
    level.exit = exit;
    return level;
}

Level map9() {
    Level level;
    level.id = 9;
    level.name = u8"穿梭";
    level.commentary = u8"复行数十步，豁然开朗。";
    Body exit;
    level.spawn.position.x = 320;
    level.spawn.position.y = 360;
    level.map.fill(static_cast<Tile>(2), 8, 6, 40, 24);
    level.map.fill(static_cast<Tile>(0), 13, 10, 35, 20);
    level.map.fill(static_cast<Tile>(2), 14, 15, 27, 17);
    level.map.fill(static_cast<Tile>(2), 19, 20, 19, 20);
    level.map.fill(static_cast<Tile>(2), 20, 19, 20, 20);
    level.map.fill(static_cast<Tile>(2), 21, 18, 27, 20);
    level.map.fill(static_cast<Tile>(2), 28, 15, 31, 16);
    level.map.fill(static_cast<Tile>(2), 23, 10, 25, 13);
    level.map.fill(static_cast<Tile>(1), 15, 17, 19, 17);
    level.map.fill(static_cast<Tile>(1), 12, 15, 12, 17);
    level.map.fill(static_cast<Tile>(1), 16, 9, 18, 9);
    level.map.fill(static_cast<Tile>(1), 36, 13, 36, 15);
    level.map.fill(static_cast<Tile>(1), 31, 21, 33, 21);
    exit.position.x = 580;
    exit.position.y = 360;
    exit.direction = Direction::Up;
    level.exit = exit;
    return level;
}

Level map10() {
    Level level;
    level.id = 10;
    level.name = u8"支点";
    level.commentary = u8"给我一个支点，我就能撑起自己。";
    Body exit;
    level.spawn.position.x = 360;
    level.spawn.position.y = 380;
    level.map.fill(static_cast<Tile>(2), 8, 6, 40, 24);
    level.map.fill(static_cast<Tile>(0), 12, 9, 36, 21);
    level.map.fill(static_cast<Tile>(2), 15, 14, 15, 20);
    level.map.fill(static_cast<Tile>(2), 13, 21, 13, 21);
    level.map.fill(static_cast<Tile>(2), 16, 14, 32, 16);
    level.map.fill(static_cast<Tile>(2), 23, 17, 25, 21);
    level.map.fill(static_cast<Tile>(2), 32, 12, 32, 13);
    level.map.fill(static_cast<Tile>(2), 29, 13, 29, 13);
    level.map.fill(static_cast<Tile>(1), 11, 14, 11, 16);
    level.map.fill(static_cast<Tile>(1), 17, 8, 19, 8);
    level.map.fill(static_cast<Tile>(1), 17, 16, 19, 16);
    level.map.fill(static_cast<Tile>(1), 23, 19, 23, 21);
    level.map.fill(static_cast<Tile>(1), 31, 22, 33, 22);
    level.map.fill(static_cast<Tile>(1), 37, 14, 37, 16);
    exit.position.x = 560;
    exit.position.y = 380;
    exit.direction = Direction::Up;
    level.exit = exit;
    return level;
}

Level map11() {
    Level level;
    level.id = 11;
    level.name = u8"倒立";
    level.commentary = u8"上下颠倒。";
    Body exit;
    level.spawn.position.x = 320;
    level.spawn.position.y = 360;
    level.map.fill(static_cast<Tile>(2), 8, 6, 40, 24);
    level.map.fill(static_cast<Tile>(0), 13, 10, 35, 20);
    level.map.fill(static_cast<Tile>(2), 13, 10, 20, 15);
    level.map.fill(static_cast<Tile>(2), 23, 18, 23, 20);
    level.map.fill(static_cast<Tile>(2), 26, 10, 28, 19);
    level.map.fill(static_cast<Tile>(2), 24, 20, 24, 20);
    level.map.fill(static_cast<Tile>(1), 14, 15, 19, 15);
    level.map.fill(static_cast<Tile>(1), 12, 18, 12, 20);
    level.map.fill(static_cast<Tile>(1), 36, 18, 36, 20);
    level.map.fill(static_cast<Tile>(1), 26, 11, 26, 18);
    exit.position.x = 620;
    exit.position.y = 360;
    exit.direction = Direction::Up;
    level.exit = exit;
    return level;
}

Level map12() {
    Level level;
    level.id = 12;
    level.name = u8"远见";
    level.commentary = u8"带给下一刻的自己。";
    Body exit;
    level.spawn.position.x = 320;
    level.spawn.position.y = 360;
    level.map.fill(static_cast<Tile>(2), 8, 6, 40, 24);
    level.map.fill(static_cast<Tile>(0), 13, 10, 35, 20);
    level.map.fill(static_cast<Tile>(2), 14, 14, 25, 17);
    level.map.fill(static_cast<Tile>(2), 18, 20, 20, 20);
    level.map.fill(static_cast<Tile>(2), 26, 10, 28, 17);
    level.map.fill(static_cast<Tile>(2), 29, 10, 35, 15);
    level.map.fill(static_cast<Tile>(2), 31, 19, 31, 20);
    level.map.fill(static_cast<Tile>(2), 32, 18, 32, 20);
    level.map.fill(static_cast<Tile>(2), 30, 20, 30, 20);
    level.map.fill(static_cast<Tile>(1), 12, 14, 12, 17);
    level.map.fill(static_cast<Tile>(1), 17, 9, 19, 9);
    level.map.fill(static_cast<Tile>(1), 26, 11, 26, 13);
    level.map.fill(static_cast<Tile>(1), 17, 17, 21, 17);
    level.map.fill(static_cast<Tile>(1), 36, 18, 36, 20);
    exit.position.x = 680;
    exit.position.y = 360;
    exit.direction = Direction::Up;
    level.exit = exit;
    return level;
}

Level map13() {
    Level level;
    level.id = 13;
    level.name = u8"阀门";
    level.commentary = u8"关好它。";
    Body exit;
    level.spawn.position.x = 260;
    level.spawn.position.y = 380;
    level.map.fill(static_cast<Tile>(2), 8, 6, 40, 24);
    level.map.fill(static_cast<Tile>(0), 12, 9, 36, 21);
    level.map.fill(static_cast<Tile>(2), 22, 13, 36, 15);
    level.map.fill(static_cast<Tile>(2), 18, 9, 20, 17);
    level.map.fill(static_cast<Tile>(2), 15, 18, 15, 21);
    level.map.fill(static_cast<Tile>(2), 22, 16, 28, 18);
    level.map.fill(static_cast<Tile>(2), 31, 20, 31, 21);
    level.map.fill(static_cast<Tile>(2), 32, 19, 32, 21);
    level.map.fill(static_cast<Tile>(2), 34, 16, 34, 18);
    level.map.fill(static_cast<Tile>(2), 17, 25, 26, 28);
    level.map.fill(static_cast<Tile>(0), 23, 13, 27, 17);
    level.map.fill(static_cast<Tile>(0), 18, 22, 20, 27);
    level.map.fill(static_cast<Tile>(0), 21, 25, 23, 27);
    level.map.fill(static_cast<Tile>(2), 22, 27, 22, 27);
    level.map.fill(static_cast<Tile>(2), 30, 21, 30, 21);
    level.map.fill(static_cast<Tile>(1), 11, 19, 11, 21);
    level.map.fill(static_cast<Tile>(1), 18, 11, 18, 13);
    level.map.fill(static_cast<Tile>(1), 20, 13, 20, 15);
    level.map.fill(static_cast<Tile>(1), 24, 22, 26, 22);
    level.map.fill(static_cast<Tile>(1), 37, 19, 37, 21);
    level.map.fill(static_cast<Tile>(1), 24, 8, 26, 8);
    level.map.fill(static_cast<Tile>(1), 30, 13, 32, 13);
    level.map.fill(static_cast<Tile>(1), 24, 25, 24, 27);
    exit.position.x = 680;
    exit.position.y = 200;
    exit.direction = Direction::Up;
    level.exit = exit;
    return level;
}

Level map14() {
    Level level;
    level.id = 14;
    level.name = u8"铁砧";
    level.commentary = u8"锻打成型的勇气。";
    Body exit;
    level.spawn.position.x = 300;
    level.spawn.position.y = 340;
    level.map.fill(static_cast<Tile>(2), 8, 6, 40, 24);
    level.map.fill(static_cast<Tile>(0), 13, 10, 35, 20);
    level.map.fill(static_cast<Tile>(2), 17, 13, 33, 16);
    level.map.fill(static_cast<Tile>(2), 33, 12, 33, 12);
    level.map.fill(static_cast<Tile>(2), 20, 17, 26, 20);
    level.map.fill(static_cast<Tile>(1), 16, 9, 18, 9);
    level.map.fill(static_cast<Tile>(1), 12, 17, 12, 19);
    level.map.fill(static_cast<Tile>(1), 36, 18, 36, 20);
    exit.position.x = 580;
    exit.position.y = 360;
    exit.direction = Direction::Up;
    level.exit = exit;
    return level;
}

Level map15() {
    Level level;
    level.id = 15;
    level.name = u8"愚者";
    level.commentary = u8"微笑面对命运。";
    Body exit;
    level.spawn.position.x = 660;
    level.spawn.position.y = 320;
    level.map.fill(static_cast<Tile>(2), 8, 6, 40, 24);
    level.map.fill(static_cast<Tile>(0), 13, 10, 35, 20);
    level.map.fill(static_cast<Tile>(2), 14, 19, 34, 19);
    level.map.fill(static_cast<Tile>(2), 20, 12, 20, 12);
    level.map.fill(static_cast<Tile>(2), 21, 11, 21, 15);
    level.map.fill(static_cast<Tile>(2), 24, 10, 24, 18);
    level.map.fill(static_cast<Tile>(2), 26, 12, 26, 12);
    level.map.fill(static_cast<Tile>(1), 20, 9, 22, 9);
    level.map.fill(static_cast<Tile>(1), 12, 17, 12, 20);
    level.map.fill(static_cast<Tile>(1), 36, 17, 36, 20);
    level.map.fill(static_cast<Tile>(1), 26, 9, 28, 9);
    level.map.fill(static_cast<Tile>(2), 23, 20, 25, 20);
    level.map.fill(static_cast<Tile>(0), 20, 21, 28, 21);
    level.map.fill(static_cast<Tile>(2), 27, 11, 27, 15);
    level.map.fill(static_cast<Tile>(2), 14, 18, 14, 18);
    level.map.fill(static_cast<Tile>(2), 34, 18, 34, 18);
    exit.position.x = 300;
    exit.position.y = 320;
    exit.direction = Direction::Up;
    level.exit = exit;
    return level;
}

Level map16() {
    Level level;
    level.id = 16;
    level.name = u8"天梯";
    level.commentary = u8"愿你得偿所愿。";
level.spawn = {{261, 201}, Direction::Up};
level.exit = Body{{701, 101}, Direction::Up};
level.map.fill(Tile::Solid, 10, 5, 29, 5);
level.map.fill(Tile::Solid, 10, 6, 14, 6);
level.map.fill(Tile::PortalSurface, 15, 6, 17, 6);
level.map.fill(Tile::Solid, 18, 6, 28, 6);
level.map.fill(Tile::PortalSurface, 29, 6, 29, 6);
level.map.fill(Tile::Solid, 10, 7, 11, 7);
level.map.fill(Tile::Solid, 18, 7, 18, 7);
level.map.fill(Tile::Solid, 26, 7, 28, 7);
level.map.fill(Tile::PortalSurface, 29, 7, 29, 7);
level.map.fill(Tile::Solid, 10, 8, 11, 8);
level.map.fill(Tile::Solid, 16, 8, 16, 8);
level.map.fill(Tile::Solid, 18, 8, 18, 8);
level.map.fill(Tile::Solid, 26, 8, 28, 8);
level.map.fill(Tile::PortalSurface, 29, 8, 29, 8);
level.map.fill(Tile::Solid, 35, 8, 38, 8);
level.map.fill(Tile::Solid, 10, 9, 11, 9);
level.map.fill(Tile::Solid, 15, 9, 16, 9);
level.map.fill(Tile::Solid, 18, 9, 18, 9);
level.map.fill(Tile::Solid, 26, 9, 31, 9);
level.map.fill(Tile::PortalSurface, 36, 9, 36, 9);
level.map.fill(Tile::Solid, 37, 9, 38, 9);
level.map.fill(Tile::Solid, 10, 10, 11, 10);
level.map.fill(Tile::Solid, 26, 10, 28, 10);
level.map.fill(Tile::PortalSurface, 36, 10, 36, 10);
level.map.fill(Tile::Solid, 37, 10, 38, 10);
level.map.fill(Tile::Solid, 10, 11, 11, 11);
level.map.fill(Tile::Solid, 22, 11, 22, 11);
level.map.fill(Tile::Solid, 26, 11, 28, 11);
level.map.fill(Tile::PortalSurface, 36, 11, 36, 11);
level.map.fill(Tile::Solid, 37, 11, 38, 11);
level.map.fill(Tile::Solid, 10, 12, 10, 12);
level.map.fill(Tile::PortalSurface, 11, 12, 11, 12);
level.map.fill(Tile::Solid, 22, 12, 22, 12);
level.map.fill(Tile::PortalSurface, 26, 12, 26, 12);
level.map.fill(Tile::Solid, 27, 12, 27, 12);
level.map.fill(Tile::PortalSurface, 28, 12, 28, 12);
level.map.fill(Tile::Solid, 34, 12, 38, 12);
level.map.fill(Tile::Solid, 10, 13, 10, 13);
level.map.fill(Tile::PortalSurface, 11, 13, 11, 13);
level.map.fill(Tile::Solid, 13, 13, 24, 13);
level.map.fill(Tile::PortalSurface, 26, 13, 26, 13);
level.map.fill(Tile::Solid, 27, 13, 27, 13);
level.map.fill(Tile::PortalSurface, 28, 13, 28, 13);
level.map.fill(Tile::Solid, 38, 13, 38, 13);
level.map.fill(Tile::Solid, 10, 14, 10, 14);
level.map.fill(Tile::PortalSurface, 11, 14, 11, 14);
level.map.fill(Tile::Solid, 14, 14, 17, 14);
level.map.fill(Tile::PortalSurface, 18, 14, 20, 14);
level.map.fill(Tile::Solid, 21, 14, 22, 14);
level.map.fill(Tile::PortalSurface, 26, 14, 26, 14);
level.map.fill(Tile::Solid, 27, 14, 27, 14);
level.map.fill(Tile::PortalSurface, 28, 14, 28, 14);
level.map.fill(Tile::Solid, 38, 14, 38, 14);
level.map.fill(Tile::Solid, 10, 15, 12, 15);
level.map.fill(Tile::Solid, 14, 15, 16, 15);
level.map.fill(Tile::Solid, 26, 15, 29, 15);
level.map.fill(Tile::Solid, 38, 15, 38, 15);
level.map.fill(Tile::Solid, 10, 16, 12, 16);
level.map.fill(Tile::Solid, 20, 16, 27, 16);
level.map.fill(Tile::Solid, 38, 16, 38, 16);
level.map.fill(Tile::Solid, 10, 17, 12, 17);
level.map.fill(Tile::Solid, 19, 17, 20, 17);
level.map.fill(Tile::Solid, 26, 17, 27, 17);
level.map.fill(Tile::Solid, 37, 17, 38, 17);
level.map.fill(Tile::Solid, 10, 18, 12, 18);
level.map.fill(Tile::Solid, 18, 18, 19, 18);
level.map.fill(Tile::Solid, 26, 18, 27, 18);
level.map.fill(Tile::PortalSurface, 37, 18, 37, 18);
level.map.fill(Tile::Solid, 38, 18, 38, 18);
level.map.fill(Tile::Solid, 10, 19, 12, 19);
level.map.fill(Tile::Solid, 14, 19, 18, 19);
level.map.fill(Tile::Solid, 26, 19, 27, 19);
level.map.fill(Tile::PortalSurface, 37, 19, 37, 19);
level.map.fill(Tile::Solid, 38, 19, 38, 19);
level.map.fill(Tile::Solid, 10, 20, 12, 20);
level.map.fill(Tile::Solid, 15, 20, 15, 20);
level.map.fill(Tile::Solid, 22, 20, 22, 20);
level.map.fill(Tile::PortalSurface, 37, 20, 37, 20);
level.map.fill(Tile::Solid, 38, 20, 38, 20);
level.map.fill(Tile::Solid, 10, 21, 12, 21);
level.map.fill(Tile::Solid, 15, 21, 15, 21);
level.map.fill(Tile::Solid, 21, 21, 22, 21);
level.map.fill(Tile::Solid, 37, 21, 38, 21);
level.map.fill(Tile::Solid, 10, 22, 12, 22);
level.map.fill(Tile::Solid, 20, 22, 22, 22);
level.map.fill(Tile::Solid, 36, 22, 38, 22);
level.map.fill(Tile::Solid, 10, 23, 13, 23);
level.map.fill(Tile::PortalSurface, 14, 23, 16, 23);
level.map.fill(Tile::Solid, 17, 23, 38, 23);
level.map.fill(Tile::Solid, 10, 24, 38, 24);
    return level;
}

} // namespace

Level makeLevel(int id) {
    switch (id) {
    case 0: return map0();
    case 1: return map1();
    case 2: return map2();
    case 3: return map3();
    case 4: return map4();
    case 5: return map5();
    case 6: return map6();
    case 7: return map7();
    case 8: return map8();
    case 9: return map9();
    case 10: return map10();
    case 11: return map11();
    case 12: return map12();
    case 13: return map13();
    case 14: return map14();
    case 15: return map15();
    case 16: return map16();
    default: throw std::invalid_argument("level id must be valid");
    }
}
} // namespace por2
