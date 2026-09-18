#include "por2/render.hpp"
#include "legacy_bridge.hpp"
#include <functional>
#include <iostream>
#include <random>
#include <sstream>

namespace {
using namespace por2;
int checks = 0;
void expect(bool condition, const std::string& message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}
bool near(double a, double b, double tolerance = 1e-5) { return std::abs(a - b) < tolerance; }
bool near(Vec2 a, Vec2 b, double tolerance = 1e-5) { return near(a.x, b.x, tolerance) && near(a.y, b.y, tolerance); }
std::string pose(const Player& p) {
    std::ostringstream s;
    s << p.body.position.x << ',' << p.body.position.y << " v=" << p.velocity.x << ',' << p.velocity.y;
    return s.str();
}

void mapsAndTransforms() {
    // Maps are user-editable; only portal transforms remain a legacy oracle.
    for (int id = 0; id <= 16; ++id) {
        const auto current = makeLevel(id);
        expect(current.id == id && !current.name.empty(), "registered level metadata");
        for (int x = 0; x < MapWidth; ++x) for (int y = 0; y < MapHeight; ++y)
            expect(current.map.at(x,y)==Tile::Empty || current.map.at(x,y)==Tile::Solid ||
                   current.map.at(x,y)==Tile::PortalSurface, "valid map tile");
        expect(std::isfinite(current.spawn.position.x) && std::isfinite(current.spawn.position.y), "finite spawn");
    }
    for (int a = 1; a <= 8; ++a) for (int b = 1; b <= 8; ++b) {
        const std::array<Portal, 2> pair{{{{10, 10}, a}, {{30, 18}, b}}};
        for (Vec2 sample : {Vec2{321, 147}, Vec2{-20, 17}, Vec2{0, 0}}) {
            expect(near(transformPoint(sample, pair[0], pair[1]), legacy::transform(sample, pair, false)), "legacy point mapping");
            expect(near(transformVector(sample, pair[0], pair[1]), legacy::transform(sample, pair, true)), "legacy velocity mapping");
        }
        for (Vec2 sample : {Vec2{321.375, 147.125}, Vec2{-20.0625, 17.8125}}) {
            const auto point = transformPoint(sample, pair[0], pair[1]);
            const auto vector = transformVector(sample, pair[0], pair[1]);
            expect(near(transformPoint(point, pair[1], pair[0]), sample), "fractional position roundtrip");
            expect(near(transformVector(vector, pair[1], pair[0]), sample), "fractional velocity roundtrip");
            expect(near(dot(vector, vector), dot(sample, sample)), "speed preserved");
        }
        for (int d = 0; d < 4; ++d) {
            const Body body{{330.25, 210.125}, static_cast<Direction>(d)};
            const auto projected = transformBody(body, pair[0], pair[1]);
            expect(near(projected.head(), transformPoint(body.head(), pair[0], pair[1])), "head follows exact body transform");
            expect(near(transformBody(projected, pair[1], pair[0]).position, body.position), "body roundtrip");
            const auto area = [](Rect r) { return r.empty() ? 0.0 : (r.right - r.left) * (r.bottom - r.top); };
            expect(near(area(clipToFront(body.bounds(), pair[0])) + area(clipToFront(projected.bounds(), pair[1])),
                        body.width() * body.height()), "portal clipping preserves complete body area");
        }
    }
}

void placementPreview() {
    Game game;
    Renderer baseline, preview;
    baseline.draw(game);
    const auto position=game.player().body.position;
    for(int id=0;id<2;++id) {
        const Shot shot{id,{260,389}};
        auto portals=game.portals();
        std::vector<ShotTrace> traces;
        expect(firePortal(game.level().map,game.player(),game.motion(),portals,shot,traces),"preview fixture places portal");
        preview.draw(game,true,false,shot);
        expect(!std::equal(baseline.pixels(),baseline.pixels()+WindowWidth*WindowHeight,preview.pixels()),"preview is visible");
        expect(!game.portals()[0].active() && !game.portals()[1].active() && near(position,game.player().body.position),
               "preview must not mutate game");
        const Vec2 center=pixels(portals[id].tile)+(portals[id].horizontal()?Vec2{30,10}:Vec2{10,30});
        expect(preview.pixels()[static_cast<int>(center.y)*WindowWidth+static_cast<int>(center.x)]==
               0x50E080u,"both portal previews use green at actual placement");
    }
    preview.draw(game,true,false,Shot{0,game.traversal().aimOrigin});
    expect(!game.portals()[0].active(),"invalid preview never places portal");
    preview.draw(game);
    expect(std::equal(baseline.pixels(),baseline.pixels()+WindowWidth*WindowHeight,preview.pixels()),"leaving aim clears preview");
}

void raysAndDirections() {
    const auto level = makeLevel(0);
    const Vec2 origin{450, 300};
    const std::array<Vec2, 4> targets{{{720, 300}, {450, 600}, {0, 300}, {450, 0}}};
    const std::array<Vec2, 4> normals{{{-1, 0}, {0, -1}, {1, 0}, {0, 1}}};
    const std::array<Vec2, 4> points{{{720, 300}, {450, 420}, {260, 300}, {450, 200}}};
    for (int wall = 0; wall < 4; ++wall) for (int direction = 0; direction < 4; ++direction) {
        const auto hit = castShot(level.map, origin, targets[wall]);
        expect(hit && near(hit->normal, normals[wall]) && near(hit->point, points[wall]), "cardinal ray actual face");
        const auto portal = choosePortal(level.map, {}, origin, static_cast<Direction>(direction), *hit);
        expect(portal.has_value(), "cardinal portal fits");
        expect(near(decode(*portal).normal, normals[wall]), "body orientation must never change wall normal");
    }
    // Oblique head-to-feet projection: floor right and floor left have opposite tangents.
    const auto right = castShot(level.map, origin, {550, 500});
    const auto left = castShot(level.map, origin, {350, 500});
    const auto pr = choosePortal(level.map, {}, origin, Direction::Up, *right);
    const auto pl = choosePortal(level.map, {}, origin, Direction::Up, *left);
    expect(pr && pl && decode(*pr).tangent.x < 0 && decode(*pl).tangent.x > 0, "oblique portal tangents");
    const auto steep = castShot(level.map, origin, {450.01, -1000});
    expect(steep && steep->point.x > 450 && steep->point.x < 450.01 && near(steep->normal, {0, 1}), "steep ray not slope-clamped");
    expect(!castShot(level.map, origin, origin), "zero ray");
    expect(!castShot(TileMap{}, origin, {10000, 9999}), "open ray terminates");
    expect(!castShot(level.map, {-1, 300}, origin), "out of bounds origin");
    const auto boundary = castShot(level.map, {350, 420}, {350, 100});
    expect(boundary && near(boundary->point, {350, 200}), "head exactly on floor portal plane can shoot into air");
    expect(!choosePortal(level.map, *pr, origin, Direction::Up, *right), "opposite portals cannot overlap");
    TileMap corner;
    corner.set(2, 1, Tile::PortalSurface);
    const auto cornerHit = castShot(corner, {30, 30}, {50, 50});
    expect(cornerHit && cornerHit->tile.x == 2 && cornerHit->tile.y == 1, "ray supercover cannot miss corner wall");
    TileMap blockedFace = level.map;
    blockedFace.set(21, 20, Tile::Solid);
    const auto floorHit = castShot(level.map, origin, {450, 600});
    expect(!choosePortal(blockedFace, {}, origin, Direction::Up, *floorHit), "all three wall faces must be exposed");
}

void terrain() {
    TileMap map;
    map.fill(Tile::Solid, 0, 20, 49, 20);
    map.fill(Tile::Solid, 20, 3, 20, 19);
    const CollisionWorld world(map, {});
    Player player{{{300.25, 300.5}, Direction::Up}, {}, {}};
    PortalMotion motion;
    for (int tick = 0; tick < 100; ++tick) expect(world.move(player, motion, {}), "fall state valid");
    expect(near(player.body.position.y, 342) && player.contacts.ground, "floor contact exact");
    for (int tick = 0; tick < 100; ++tick) {
        expect(world.move(player, motion, {false, true, false}), "wall movement valid");
        expect(world.clear(player.body.bounds()), "no wall penetration");
    }
    expect(near(player.body.position.x, 382), "full box stops against wall");
    const double beforeJump = player.body.position.y;
    expect(world.move(player, motion, {false, false, true}), "jump valid");
    expect(near(player.body.position.y, beforeJump - 6) && player.velocity.y < 0, "jump preserves effective legacy launch speed");
    player = {{{100, 280}, Direction::Right}, {800, 0}, {}};
    motion = {};
    expect(world.move(player, motion, {false, true, false}), "high-speed movement valid");
    expect(player.body.farCorner().x <= 400 + 1e-5 && world.clear(player.body.bounds()), "no high-speed tunneling");
    player = {{{395, 300}, Direction::Up}, {}, {}};
    expect(world.recover(player, motion) && world.clear(player.body.bounds()), "embedded body recovered to valid space");

    const auto valve = makeLevel(13);
    const CollisionWorld valveWorld(valve.map, {});
    player = {{{381, 430}, Direction::Up}, {}, {}};
    motion = {};
    for (int tick = 0; tick < 100; ++tick) {
        expect(valveWorld.move(player, motion, {}), "map13 shaft fall valid");
        expect(valveWorld.clear(player.body.bounds()), "map13 shaft cannot embed");
    }
    expect(near(player.body.position.y, 502), "map13 shaft lands at bottom, not inside upper/lower structure");
    for (int tick = 0; tick < 100; ++tick) expect(valveWorld.move(player, motion, {false, true, false}), "map13 lower corner stable");
    expect(near(player.body.position.x, 422), "upright body correctly stops at lower post");
    player = {{{380, 520}, Direction::Right}, {}, {}};
    motion = {};
    for (int tick = 0; tick < 100; ++tick) {
        expect(valveWorld.move(player, motion, {false, true, false}), "map13 horizontal body in lower passage");
        expect(valveWorld.clear(player.body.bounds()), "map13 horizontal body remains free");
    }
    expect(player.body.position.x > 400, "rotated body can use lower passage");
    for (int tick = 0; tick < 30; ++tick) expect(valveWorld.move(player, motion, {true, false, false}), "can reverse away from corner");
    expect(player.body.position.x < 400, "no persistent sticking at lower corner");
}

void journeys() {
    const auto level = makeLevel(0);
    const std::array<Cell, 4> first{{{36, 15}, {16, 21}, {12, 15}, {16, 9}}};
    const std::array<Cell, 4> second{{{36, 11}, {26, 21}, {12, 11}, {26, 9}}};
    int passed = 0;
    for (int a = 1; a <= 8; ++a) for (int b = 1; b <= 8; ++b) for (int d = 0; d < 4; ++d) {
        const std::array<Portal, 2> pair{{{first[(a - 1) / 2], a}, {second[(b - 1) / 2], b}}};
        const auto frame = decode(pair[0]);
        Player player;
        player.body.direction = static_cast<Direction>(d);
        player.body.position = frame.anchor + frame.tangent * 30 + frame.normal * (normalRadius(player.body, pair[0]) + 1)
            - Vec2{player.body.width() / 2, player.body.height() / 2};
        player.velocity = frame.normal * -80;
        PortalMotion motion;
        const CollisionWorld world(level.map, pair);
        expect(world.clear(player.body.bounds()), "journey starts fully in room");
        bool crossed = false;
        for (int tick = 0; tick < 8 && !crossed; ++tick) {
            expect(world.move(player, motion, {}), "journey valid " + std::to_string(a) + "/" + std::to_string(b) + " " + pose(player));
            expect(world.canOccupy(player.body, motion.activePortal), "both portal fragments free");
            crossed = motion.crossings > 0;
        }
        expect(crossed, "must teleport " + std::to_string(a) + "/" + std::to_string(b) + " d=" + std::to_string(d) + " " + pose(player));
        ++passed;
        const Direction expected = vectorDirection(transformVector(directionVector(static_cast<Direction>(d)), pair[0], pair[1]));
        expect(player.body.direction == expected, "teleport body orientation");
        const auto view = describeTraversal(player, motion, pair);
        const auto head = view.headThrough ? transformPoint(player.body.head(), pair[view.portalIndex], pair[1 - view.portalIndex]) : player.body.head();
        expect(near(view.aimOrigin, head), "same-frame portal head");
    }
    std::cout << "  " << passed << " full-body entrance/exit journeys passed\n";

    TileMap map;
    map.fill(Tile::PortalSurface, 10, 5, 10, 20);
    map.fill(Tile::PortalSurface, 30, 5, 30, 20);
    map.fill(Tile::Solid, 28, 5, 28, 20);
    const std::array<Portal, 2> pair{{{{10, 10}, 1}, {{30, 10}, 1}}};
    const CollisionWorld blockedExit(map, pair);
    Player player{{{140, 221}, Direction::Right}, {30, 0}, {}};
    PortalMotion motion;
    int crossed = 0;
    for (int i = 0; i < 10; ++i) {
        // Cancel gravity to isolate the normal clearance of the 20px exit gap.
        player.velocity = {12, -1};
        expect(blockedExit.move(player, motion, {false, true, false}), "blocked-exit movement valid");
        crossed += motion.crossings;
    }
    expect(crossed == 0 && player.body.center().x < 200, "exit obstruction stops body before center crosses");
    expect(blockedExit.canOccupy(player.body, motion.activePortal), "no clipping into exit obstruction");
    map.fill(Tile::Empty, 28, 5, 28, 20);
    for (int i = 0; i < 10 && !crossed; ++i) {
        player.velocity = {12, -1};
        expect(blockedExit.move(player, motion, {false, true, false}), "unblocked-exit movement valid");
        crossed += motion.crossings;
    }
    expect(crossed > 0, "clearing exit permits passage");

    // Outside the 60px aperture there is still a solid wall, never a carved hole.
    player = {{{170, 203}, Direction::Up}, {50, 0}, {}};
    motion = {};
    expect(blockedExit.move(player, motion, {}), "misaligned entrance valid");
    expect(motion.crossings == 0 && player.body.farCorner().x <= 200 + 1e-5, "misaligned body cannot pass without teleporting");
    player = {{{182, 201}, Direction::Up}, {}, {}};
    motion = {0, 0};
    expect(blockedExit.move(player, motion, {}), "body exactly flush with portal is valid");
    expect(motion.activePortal == -1, "fully withdrawn body does not remain locked in transit");

    // map13's lower floor-to-wall pair rotates the body into the passage above the post.
    const auto valve = makeLevel(13);
    const std::array<Portal, 2> lowerPair{{{{24, 22}, 4}, {{24, 25}, 1}}};
    const CollisionWorld valveWorld(valve.map, lowerPair);
    player = {{{501, 380}, Direction::Up}, {0, 20}, {}};
    motion = {};
    crossed = 0;
    for (int i = 0; i < 35; ++i) {
        expect(valveWorld.move(player, motion, {}), "map13 lower portal journey valid " + pose(player));
        crossed += motion.crossings;
        expect(valveWorld.canOccupy(player.body, motion.activePortal), "map13 projected body never enters post");
    }
    expect(crossed > 0 && player.body.position.x < 480, "map13 lower portal passage actually traversed");
}

void frameAndGradient() {
    Game game;
    InputFrame input;
    input.shots = {{0, {260, 389}}, {1, {730, 389}}};
    game.tick(input);
    expect(game.portals()[0].active() && game.portals()[1].active(), "both portals placed");
    Renderer renderer;
    renderer.draw(game);
    const auto& portal = game.portals()[0];
    const int x = portal.tile.x * TileSize + 10;
    const auto a = renderer.pixels()[(portal.tile.y * TileSize + 10) * WindowWidth + x];
    const auto b = renderer.pixels()[(portal.tile.y * TileSize + 30) * WindowWidth + x];
    const auto c = renderer.pixels()[(portal.tile.y * TileSize + 50) * WindowWidth + x];
    expect(a != b && b != c && a != c, "three distinct gradient regions");
    expect(a < b && b < c, "gradient follows downward portal tangent");
    Game floorGame;
    InputFrame floorShot;
    floorShot.shots = {{0, {500, 500}}};
    floorGame.tick(floorShot);
    expect(floorGame.portals()[0].active() && decode(floorGame.portals()[0]).tangent.x < 0, "reversed horizontal portal placed");
    Renderer floorRenderer;
    floorRenderer.draw(floorGame);
    const auto floorTile = floorGame.portals()[0].tile;
    const int row = (floorTile.y * TileSize + 10) * WindowWidth;
    expect(floorRenderer.pixels()[row + floorTile.x * TileSize + 10] >
           floorRenderer.pixels()[row + floorTile.x * TileSize + 50], "gradient reverses with portal tangent");
    for (int index = 0; index < WindowWidth * WindowHeight; ++index)
        expect(renderer.pixels()[index] != 0x00FF00, "green anchor removed");
    int crossings = 0;
    for (int frame = 0; frame < 100; ++frame) {
        InputFrame move;
        move.movement = {frame < 30, frame >= 30, frame % 29 == 0};
        game.tick(move);
        crossings += game.motion().crossings;
        const auto expected = describeTraversal(game.player(), game.motion(), game.portals());
        expect(near(game.traversal().aimOrigin, expected.aimOrigin), "view synchronized after physics");
        renderer.draw(game);
        const auto head = game.traversal().aimOrigin;
        const int hx = static_cast<int>(std::lround(head.x)), hy = static_cast<int>(std::lround(head.y));
        expect(renderer.pixels()[hy * WindowWidth + hx] == 0xFF0000, "red marker uses current head position");
    }
    expect(crossings > 0, "same-frame render test includes actual teleportation");
    game.restart();
    expect(near(game.traversal().aimOrigin, game.player().body.head()) &&
           game.traversal().aimDirection == game.player().body.direction, "restart has no stale aiming orientation");
    expect(!game.portals()[0].active() && game.motion().activePortal == -1, "restart clears transit");
}

void minorityPortalShooting() {
    const auto level = makeLevel(0);
    const std::array<Cell, 4> first{{{36, 15}, {16, 21}, {12, 15}, {16, 9}}};
    const std::array<Cell, 4> second{{{36, 11}, {26, 21}, {12, 11}, {26, 9}}};
    for (int a = 1; a <= 8; ++a) for (int b = 1; b <= 8; ++b) for (int d = 0; d < 4; ++d) {
        std::array<Portal, 2> pair{{{first[(a - 1) / 2], a}, {second[(b - 1) / 2], b}}};
        Player player;
        player.body.direction = static_cast<Direction>(d);
        const auto frame = decode(pair[0]);
        const double radius = normalRadius(player.body, pair[0]);
        player.body.position = frame.anchor + frame.tangent * 30 + frame.normal * (radius * 0.5)
            - Vec2{player.body.width() / 2, player.body.height() / 2};
        const auto originalPosition = player.body.position;
        const PortalMotion motion{0, 0};
        const auto view = describeTraversal(player, motion, pair);
        expect(view.lockedPortal == 0 && near(view.bodyAreas[0], 18 * 58 * 0.75), "75 percent blue side is locked");
        Portal moved = pair[1];
        moved.code = b % 2 ? b + 1 : b - 1;
        if (moved.horizontal()) ++moved.tile.x; else ++moved.tile.y;
        expect(replacePortal(level.map, player, motion, pair, 1, moved), "minority orange portal can move and reverse");
        expect(near(player.body.position, originalPosition) && pair[0].code == a, "majority body and its portal stay fixed");
        Portal locked = pair[0];
        locked.code = a % 2 ? a + 1 : a - 1;
        expect(!replacePortal(level.map, player, motion, pair, 0, locked), "majority blue portal cannot be replaced");
        expect(CollisionWorld(level.map, pair).canOccupy(player.body, 0), "relocated minority fragment is collision-free");

        // Exact half: retain the current side, rather than toggling from rounding.
        player.body.position = player.body.position - frame.normal * (radius * 0.5);
        expect(describeTraversal(player, motion, pair).lockedPortal == 0, "50/50 keeps blue ownership");
        const Player halfOther{transformBody(player.body, pair[0], pair[1]), {}, {}};
        expect(describeTraversal(halfOther, {1, 0}, pair).lockedPortal == 1, "50/50 keeps orange ownership");

        player.body.position = player.body.position - frame.normal * (radius * 0.5);
        player.body = transformBody(player.body, pair[0], pair[1]);
        const PortalMotion crossed{1, 1};
        const auto newView = describeTraversal(player, crossed, pair);
        expect(newView.lockedPortal == 1 && near(newView.bodyAreas[1], 18 * 58 * 0.75), "ownership swaps immediately on crossing");
        expect(replacePortal(level.map, player, crossed, pair, 0, locked), "former entrance unlocks immediately");
        expect(!replacePortal(level.map, player, crossed, pair, 1, moved), "majority orange portal stays locked");
    }

    // Run the original function to demonstrate its stale is_in_portal after teleport.
    const auto original = legacy::crossingLock();
    const std::array<Portal, 2> originalPair{{{{36, 15}, 1}, {{12, 15}, 5}}};
    expect(original.flag == 0, "original still locks blue on the crossing frame");
    expect(describeTraversal({original.body, {}, {}}, {1, 1}, originalPair).lockedPortal == 1,
           "actual majority after original teleport is orange");

    // Shoot from the head on the minority side, move it, then shoot again in the same frame.
    std::array<Portal, 2> pair{{{{16, 21}, 4}, {{36, 11}, 1}}};
    Player player{{{341, 376.5}, Direction::Down}, {}, {}};
    const PortalMotion motion{0, 0};
    const auto before = describeTraversal(player, motion, pair);
    expect(before.headThrough && before.lockedPortal == 0, "head can be on unlocked minority side");
    std::vector<ShotTrace> traces;
    expect(firePortal(level.map, player, motion, pair, {1, {250, 300}}, traces), "shoot from projected head to move minority portal");
    const auto after = describeTraversal(player, motion, pair);
    expect(!near(before.aimOrigin, after.aimOrigin) && near(traces[0].origin, before.aimOrigin), "head relocates immediately; trace records emission point");
    expect(firePortal(level.map, player, motion, pair, {1, {700, 195}}, traces), "second shot can move minority portal again");
    expect(near(traces[1].origin, after.aimOrigin), "second same-frame shot uses relocated head");
    expect(!firePortal(level.map, player, motion, pair, {0, {260, 300}}, traces), "cannot shoot locked portal even from other side");
    expect(near(player.body.position, {341, 376.5}), "shooting never moves majority body");
    pair = {{{{16, 21}, 4}, {{36, 11}, 1}}};
    player = {{{341, 371}, Direction::Down}, {}, {}}; // Head exactly at y=420.
    expect(firePortal(level.map, player, motion, pair, {1, {500, 180}}, traces), "minority shooting allowed at exact head/plane boundary");

    // A legal wall can still lack space for the fragment already protruding from its new mouth.
    TileMap tight;
    tight.fill(Tile::PortalSurface, 10, 5, 10, 20);
    tight.fill(Tile::PortalSurface, 30, 5, 30, 20);
    tight.fill(Tile::PortalSurface, 40, 5, 40, 20);
    tight.fill(Tile::Solid, 38, 5, 38, 20);
    pair = {{{{10, 10}, 1}, {{30, 10}, 1}}};
    player = {{{166, 221}, Direction::Right}, {3.5, -2.5}, {}};
    const Portal target{{40, 10}, 1};
    expect(validPlacement(tight, target, pair[0]), "target has three exposed wall faces");
    expect(!replacePortal(tight, player, motion, pair, 1, target), "reject only unsafe target, not minority shooting in general");
    expect(pair[1].tile.x == 30 && near(player.body.position, {166, 221}) && near(player.velocity, {3.5, -2.5}),
           "rejected placement leaves portals, body and momentum unchanged");
    tight.fill(Tile::Empty, 38, 5, 38, 20);
    expect(replacePortal(tight, player, motion, pair, 1, target), "same minority shot permitted once projected fragment fits");
}

void stress() {
    std::mt19937 random(20260917);
    for (int id = 0; id < 16; ++id) {
        Game game(id);
        for (int tick = 0; tick < 3000; ++tick) {
            InputFrame input;
            input.movement = {random() % 3 == 0, random() % 3 == 0, random() % 7 == 0};
            input.restart = tick % 401 == 0;
            if (tick % 7 == 0) input.shots.push_back({static_cast<int>(random() % 2),
                {static_cast<int>(random() % 1400) - 200, static_cast<int>(random() % 1000) - 200}});
            game.tick(input);
            const CollisionWorld world(game.level().map, game.portals());
            expect(world.canOccupy(game.player().body, game.motion().activePortal), "stress no embedded body map " + std::to_string(id));
            expect(finite(game.player().body.position) && finite(game.player().velocity), "stress finite physics");
            if (!game.traversal().headThrough) expect(near(game.traversal().aimOrigin, game.player().body.head()), "stress head in same frame");
        }
    }
    Game game;
    for (int i = 0; i < 180 && game.level().id == 0; ++i) {
        InputFrame input;
        input.movement.right = true;
        input.useExit = true;
        game.tick(input);
    }
    expect(game.level().id == 5, "first exit remains reachable");
    Game campaign;
    for (int id : Campaign) {
        expect(campaign.level().id == id, "campaign order unchanged");
        InputFrame skip;
        skip.skip = true;
        campaign.tick(skip);
        expect(campaign.level().id == id && !campaign.finished(), "skip still requires orange portal");
        bool placed = false;
        for (int x = 0; x < WindowWidth && !placed; x += 20)
            for (int y = 0; y < WindowHeight && !placed; y += 20) {
                campaign.restart();
                InputFrame shot;
                shot.shots.push_back({1, {x, y}});
                campaign.tick(shot);
                placed = campaign.portals()[1].active();
            }
        expect(placed, "an exposed portal surface is reachable from spawn in map " + std::to_string(id));
        campaign.tick(skip);
    }
    expect(campaign.finished(), "campaign completes");
    InputFrame replay;
    replay.restart = true;
    campaign.tick(replay);
    expect(!campaign.finished() && campaign.level().id == 0, "campaign replay");
}
}

int main() {
    const std::pair<const char*, std::function<void()>> suites[]{
        {"non-mutating placement preview and actual portal position", placementPreview},
        {"editable level data and 64 invertible portal transforms", mapsAndTransforms},
        {"actual ray faces, direction rules and corner/vertical rays", raysAndDirections},
        {"floating point collision, high speed, recovery and map13 geometry", terrain},
        {"256 portal journeys, blocked exits and misaligned apertures", journeys},
        {"same-frame body/head, portal gradient and restart", frameAndGradient},
        {"majority lock, minority reopening and same-frame projected-head shooting", minorityPortalShooting},
        {"48000 randomized ticks with nonpenetration assertions", stress}
    };
    for (const auto& suite : suites) {
        try { suite.second(); std::cout << "PASS " << suite.first << '\n'; }
        catch (const std::exception& error) { std::cerr << "FAIL " << suite.first << ": " << error.what() << '\n'; return 1; }
    }
    std::cout << "All " << checks << " checks passed.\n";
}
