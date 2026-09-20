# 回放编号说明

当前战役最后三关为 **Level 12「阀门」→ Level 13「深渊」→ Level 14「天梯」**，源码地图 ID 分别为 **13、4、16**。脚本中的 `level` 始终使用源码 ID，战役重新排序不改变它。

- `level12_new_layout/route1.txt` 至 `route4.txt`：当前最终关「天梯」的四条通关路线；目录中的 12 是调整前编号，按 F6 导入即可。
- `difficulty_review/level13.txt`：当前 Level 12「阀门」。
- `difficulty_review/level14.txt`：当前 Level 13「深渊」。
- 其余旧的 `level12_*` 和 `difficulty_review/level12.txt`：旧布局「天梯」的历史记录，不是当前地图的已验证解。

历史验证 JSON 保留生成时的关卡顺序与输出。当前最终关通关后返回 `replay=cleared level=16`，不会进入下一张地图；菜单仍可重新选关。最新难度与路线说明见 [关卡难度评估](../关卡难度评估.md)。
