# PvZ-Emulator

更多说明请看[原 repo](https://github.com/dnartz/PvZ-Emulator).

## 快速开始

使用 [PvZ Emulator 的最小可复现用例](https://github.com/Rottenham/min-pvz-emulator).

该仓库里包含了已经编译好的 `libpvzemu.a`, 因此开箱即用, 局限在于无法修改源码.

## 从头编译

默认你已经:
- 安装 Cmake 和 Ninja
- 新建了一个目录并 cd 进去

目前经测试可用的编译方式：

编译为 c++静态库：
```console
cmake -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
ninja
```

使用 pip install 作为 python 模块安装：
```console
python -m pip install git+https://github.com/Rottenham/PvZ-Emulator
```

可行但是比较麻烦的编译方式：

编译 debug server:
```console
cmake -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DPVZEMU_BUILD_DEBUGGER=True ..
ninja
```

麻烦的原因在于要将各种 `main.pak` 里的资源文件拖出来，并且还要用 **旧版本** 的 Node.js. 得到的结果是一个能看到僵尸攻击域/防御域的可视化网页. 

然而, 这个只能用来调试, 跑模拟的话速度并不快, 所以个人认为没有太大用处(且编译还很麻烦).

不可用的编译方式：

编译 python 静态库：
```console
cmake -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DPVZEMU_BUILD_PYBIND=True -DPYBIND11_PYTHON_VERSION=3.8 ..
ninja
```

尝试各种方法都并没有成功(

最后一步爆各种 linker error (当然, 要走到这一步也需要改3~4个地方), 整不明白.

## 说明

<del>不知为何, python 模块里并没有 `pybind` 任何输出函数. 也就是虽然 python 模块能用, 但是读取不出任何数据(wtf). 解决方法未知.</del> 已由好心同学增加了 binding.

C++ 版使用正常. 源代码有无 bug 未知, 待进一步验证.

据 testla 本人说, 源码都是反汇编扒下来的. 但肯定有简化过的地方, 比如出怪选行明显就简化过, 不如出怪选行论里介绍的那样复杂.
