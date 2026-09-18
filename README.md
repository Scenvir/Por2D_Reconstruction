# Por2D · 基础重构版

本仓库 `Por2D_Reconstruction` 对应工作区目录 `por2_reconstructed/`，用于保留和整理固定向下重力的传送门玩法。[Por2D 四向重力版](https://github.com/Scenvir/Por2D) 则在此基础上加入 G 键切换重力；本版没有这一操作。两版都生成 `Por2D.exe`，可分别构建、分别游玩。

Por2D 是一款二维传送门解谜游戏。你控制一个淡黄色矩形角色，在白墙上放置相互连接的蓝门和橙门，利用传送改变位置、速度方向与身体朝向，最终以正确姿态抵达灰色出口，按 E 过关。

本版本的重力始终朝屏幕下方。解谜的关键不仅是到达出口，还包括利用传送门调整身体朝向、转换下落速度，以及在穿门过程中重新布置另一端。

第一次游玩请阅读 [新手教程](新手教程.md)。

## 游戏特色

- **位置、姿态与惯性解谜**：两扇门双向连通，身体和速度随门的方向变换；相同深浅的渐变位置彼此对应。
- **身体跨门操作**：角色可以同时露在两端。身体占比较大的一端会锁定并显示细金色边框，另一端在空间允许时仍可重新放置。
- **实时射门预览**：默认开启，按 Q 切换。蓝、橙箭头分别表示可放置的门，两种都能放时并列显示；绿色虚线和轮廓提示位置，两种都不能放时显示红色叉号。
- **中文关卡呈现**：15 个正式关卡配有中文名称和评语，入场时播放约 3 秒的短动画，可按任意键或点击鼠标跳过。
- **便捷重试与选关**：R 重开本关，Esc 暂停并进入选关菜单。触碰地图任意边界会重开本关；死亡和 R 重试不重复播放入场动画。
- **原生 Windows 窗口**：支持窗口和等比例全屏显示，无需 EasyX。

## 开始游玩

已有构建产物时，双击本目录的 `build/Por2D.exe`。启动后先看到自动演示画面，按任意键进入选关菜单，从 **Level 0「概念」** 开始即可。

| 操作 | 功能 |
| --- | --- |
| A / D、W | 左右移动、跳跃 |
| 鼠标左键 / 右键 | 按下并松开，放置蓝门 / 橙门 |
| Q | 开关射门预览 |
| E | 与出口的位置、朝向匹配时过关 |
| R | 重开本关；全部通关后重新开始 |
| Esc | 选关菜单 / 继续游戏 |
| F11 / Alt+Enter | 全屏 / 窗口 |
| F1 / F2 | 头部红点 / 辅助网格 |

选关菜单可用鼠标点击，或用方向键选择、Enter 开始，PageUp / PageDown 翻页。选择关卡会重开该关，“继续游戏”保留暂停状态。

## 关卡

正式关卡按显示编号排列：

| Level | 名称 | Level | 名称 |
| --- | --- | --- | --- |
| 0 | 概念 | 8 | 铁砧 |
| 1 | 高山 | 9 | 倒立 |
| 2 | 旋转 | 10 | 远见 |
| 3 | 远跳 | 11 | 生死逆转 |
| 4 | 大脑 | 12 | 生死逆转 |
| 5 | 穿梭 | 13 | 阀门 |
| 6 | 支点 | 14 | 深渊 |
| 7 | 裂缝 | — | — |

菜单末尾另有两张无出口的实验地图，适合自由练习。显示编号与源地图 ID 不同：正式战役对应的地图 ID 依次为 `0, 5, 6, 7, 8, 9, 10, 3, 14, 11, 12, 15, 16, 13, 4`。

## 从源码构建

项目使用 C++17，Windows 构建需要带 Windows 工具链的 `g++`（例如 LLVM-MinGW）和 PowerShell。以下命令均在本 README 所在目录运行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1
.\build\Por2D.exe
```

脚本会生成 `build/Por2D.exe`。可通过 `-Compiler` 指定编译器，或通过 `-DebugBuild` 启用调试构建。

也可使用 CMake：

```powershell
cmake -S . -B build-cmake -DBUILD_TESTING=OFF
cmake --build build-cmake --config Release
```

窗口程序面向 Windows。需要运行回归测试时，从 [原始源码仓库](https://github.com/Scenvir/Por2D-origin) 取得 `NewlyUpdateMap.cpp`，放到本项目目录的上一级，再执行 `powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Test`。测试需要该文件，单独构建游戏不需要。

常用启动参数：

```powershell
.\build\Por2D.exe --fullscreen
# --level 使用源地图 ID：5 对应菜单里的 Level 1「高山」
.\build\Por2D.exe --level 5
.\build\Por2D.exe --headless --level 5 --frames 60 --screenshot .\build\level1.bmp
.\build\Por2D.exe --window-smoke-test
```

无窗口模式用于模拟和地图截图，不展示窗口绘制的入场文字动画。

## 项目结构与背景

项目在原始地图与传送变换基础上重构，使用浮点碰撞、分步运动，以及同一帧内的角色、投影和瞄准更新。工作区的 `por2_reconstructed/` 是基础版，`Por2D/` 是四向重力版；共同功能在两版中保持同步。

- `src/levels.cpp`：地图、中文名称与评语。
- `src/portal.cpp`、`src/physics.cpp`：射线、传送变换与碰撞运动。
- `src/game.cpp`：关卡流程和游戏状态。
- `src/render.cpp`、`src/main.cpp`：画面、射门预览、窗口交互和入场动画。
- `tests/`：核心逻辑回归测试。

更多资料：[新手教程](新手教程.md)、[验证记录](VERIFICATION.md)、[设计笔记](DESIGN_NOTES.md)。后两份文档保留开发阶段的技术记录，具体行为以当前代码和新手教程为准。
