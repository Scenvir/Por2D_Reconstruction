# 当前战役通关回放

本目录的 `level00.txt` 至 `level14.txt` 按**当前菜单编号**排列。点击菜单“回放”或按 F6 导入即可；脚本里的 `level` 数字是源码 ID，无需手动修改。需要慢放时，在设置里调整运行速度。

| 菜单关卡 | 名称 | 源码 ID | 回放 |
| --- | --- | --- | --- |
| 0 | 概念 | 0 | [level00.txt](level00.txt) |
| 1 | 高山 | 5 | [level01.txt](level01.txt) |
| 2 | 旋转 | 6 | [level02.txt](level02.txt) |
| 3 | 远跳 | 7 | [level03.txt](level03.txt) |
| 4 | 大脑 | 8 | [level04.txt](level04.txt) |
| 5 | 穿梭 | 9 | [level05.txt](level05.txt) |
| 6 | 支点 | 10 | [level06.txt](level06.txt) |
| 7 | 裂缝 | 3 | [level07.txt](level07.txt) |
| 8 | 铁砧 | 14 | [level08.txt](level08.txt) |
| 9 | 倒立 | 11 | [level09.txt](level09.txt) |
| 10 | 远见 | 12 | [level10.txt](level10.txt) |
| 11 | 愚者 | 15 | [level11.txt](level11.txt) |
| 12 | 阀门 | 13 | [level12.txt](level12.txt) |
| 13 | 深渊 | 4 | [level13.txt](level13.txt) |
| 14 | 天梯 | 16 | [level14.txt](level14.txt) |

“天梯”使用当前布局的 [route1](../level12_new_layout/route1.txt)，另三条路线仍在原目录。这些是代表性通关路线，不保证最短。

[verification.json](verification.json) 保存当前逐关运行结果及校验值。成功结果应包含 `replay=cleared`；最后一关仍显示 `level=16`，其他关通常显示下一关的源码 ID。

在 `por2_reconstructed` 目录执行 `python tools/verify_difficulty_replays.py` 可重新验证全部 15 份脚本并输出新的 JSON 报告；任意关卡未正常通关时返回失败。

整理前的脚本及原验证记录完整保存在 [archive/2026-09-20](archive/2026-09-20/README.md)。历史难度统计引用该快照，避免旧编号与当前文件混淆。
