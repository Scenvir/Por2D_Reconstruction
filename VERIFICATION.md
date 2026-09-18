# 当前浮点物理版本验证记录

日期：2026-09-17。工具链：Windows、LLVM-MinGW / Clang 21.1.0、C++17。

原始 `../NewlyUpdateMap.cpp` 未改动，SHA-256 为：

```text
799F808E16F33A72E3531F4A6AB06DCD174AB1D15CF516E62D310185C47396AB
```

## 回归测试

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\por2_reconstructed\build.ps1 -Test
```

构建成功，无编译警告。结果：

```text
PASS unchanged maps and 64 invertible portal transforms
PASS actual ray faces, direction rules and corner/vertical rays
PASS floating point collision, high speed, recovery and map13 geometry
  256 full-body entrance/exit journeys passed
PASS 256 portal journeys, blocked exits and misaligned apertures
PASS same-frame body/head, portal gradient and restart
PASS majority lock, minority reopening and same-frame projected-head shooting
PASS 48000 randomized ticks with nonpenetration assertions
All 774659 checks passed.
```

地图与门的有效坐标变换仍对照实际编译执行的原代码。旧整数碰撞与斜率放门规则已被明确替换，不再作为新物理的正确性标准。新的回归检查真实几何约束、传送事件、投影一致性和用户反馈的 map13 地形。

另有原版锁定时序的实际执行对照：蓝门 `(36,15), code=1`、橙门 `(12,15), code=5`，角色从 `(713,301)` 调用 `reload_position()` 后已到橙门侧，但 `is_in_portal` 仍为 0。新实现对此姿态计算的占比锁定值是 1，立即锁定真正的本体所在侧。

## UBSan

用以下参数编译并运行同一套测试，检查未定义行为：

```powershell
$sources = @(
    'por2_reconstructed/src/levels.cpp',
    'por2_reconstructed/src/portal.cpp',
    'por2_reconstructed/src/physics.cpp',
    'por2_reconstructed/src/game.cpp',
    'por2_reconstructed/src/render.cpp',
    'por2_reconstructed/tests/tests.cpp',
    'por2_reconstructed/tests/legacy_bridge.cpp'
)
g++ -std=c++17 -O1 -g -fsanitize=undefined -fno-sanitize-recover=all `
    -I por2_reconstructed/include -I por2_reconstructed/tests/legacy_stubs `
    @sources -o por2_reconstructed/build/por2_tests_ubsan.exe
.\por2_reconstructed\build\por2_tests_ubsan.exe
```

同一套 7 组测试、774,659 项断言在 UBSan 下全部通过，未报告未定义行为。UBSan 不是 AddressSanitizer，不代表所有内存错误都已被排除。

## 窗口与截图

原生隐藏窗口自动检查已通过：

```text
PASS native window: timer, keyboard, mouse, restart, focus and paint
```

检查命令为 README 中的 `--window-smoke-test`，包含真实窗口消息的鼠标点击、移动、重开、失焦和绘制回调。已查看 `build/window-smoke.bmp`，两扇门为三色渐变，没有绿点。

`map13` 无输入运行 60 帧：

```text
level=13 frames=60 position=260,382 velocity=0,0
```

已查看 `build/map13-new.bmp`，角色站立在实际地面上，红点与角色同帧。

## 范围限制

- 未在 EasyX 环境中运行原版窗口。
- 没有人工解完所有 14 个谜题；物理参数和严格间隙检查可能改变部分关卡难度。
- 未验证 CMake / MSVC 构建路径。
- 随机输入和回归用例覆盖已知故障类型，不是穷尽所有状态的数学证明。
