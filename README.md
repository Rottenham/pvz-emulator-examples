# PvZ Emulator 使用例

本仓库包含了一些适用于 [PvZ Emulator](https://github.com/Rottenham/PvZ-Emulator) 的测试用例.

## 如何编译

假设你已将所有与 PvZ Emulator 有关的文件放在了 `/lib` 目录下...

如果你使用 VSCode, 直接 `Ctrl+Shift+B` 即可编译并运行任何 .cpp 源码. 输出文件在 `/dest` 目录下 (文件名后缀可以在 `.vscode/settings.json` 中按需修改).

你也可以使用源码中提供的编译指令.

## PvZ Emulator 的亿些特性

⚠️ 以下内容仅供参考, 不确保完全准确.

- 发炮函数 (`subsystem::cob_cannon::launch`) 不是 `public`, 但可以把这段代码复制出来自己用.
    - `x` 与 `y` 为准星值, 与常用坐标系相同 (已验证). 上界之风需要自行提供转换后的 `y`. 
- 没有序列号
    - 蹦极索取目标只看编号 (已验证)
- 本帧铲除 (`plant_factory.destroy`) 的植物, 编号在下一帧才回收
- `row`, `col` 为 0 下标起始（与内存数据一致）
    - `scene.get_max_row()` 是 1 下标起始
- 跳跳和玉米炮的互动有 bug? 似乎没有特殊处理 (待验证).