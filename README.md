# PvZ Emulator 使用例

本仓库包含了一些适用于 [PvZ Emulator](https://github.com/Rottenham/PvZ-Emulator) 的测试用例.

## 依赖

Windows x86-64, VSCode, Cmake, Ninja, g++.

## 如何编译

在 VSCode 中 `Ctrl+Shift+B` 以编译并运行任何 .cpp 源码. 输出文件在 `/dest` 目录下 (文件名后缀可以在 `.vscode/settings.json` 中按需修改).

## PvZ Emulator 的亿些特性

⚠️ 以下内容仅供参考, 不确保完全准确.

- 发炮函数 (`subsystem::cob_cannon::launch`) 不是 `public`, 但可以把这段代码复制出来自己用.
    - `x` 与 `y` 为准星值, 与常用坐标系相同 (已验证). 上界之风需要自行提供转换后的 `y`. 
- 没有序列号
    - 蹦极索取目标只看编号 (已验证)
- 本帧铲除 (`plant_factory.destroy`) 的植物, 编号在下一帧才回收
- `row`, `col` 为 0 下标起始（与内存数据一致）
    - `scene.get_max_row()` 是 1 下标起始
- <del>跳跳和玉米炮的互动有 bug, 将玉米炮视作普通植物.</del> (已修复)
- 搭梯判断有 bug, 梯子僵尸会给所有植物搭梯. (本 repo 已修复, 原 repo 尚未修复)
- rect::is_overlap_with_circle 有 bug (本 repo 已修复, 原 repo 尚未修复)