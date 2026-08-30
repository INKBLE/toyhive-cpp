# ToyHive

一个用于学习容器实现的 C++17 `hive<T>` 原型。项目尝试以链式分块存储（每块 64 个槽位）和 skipfield 标记空洞，在元素不连续时仍支持高效地遍历已构造的元素。

> 当前项目处于早期实验阶段，不应在生产环境中使用。

## 当前特性

- 模板容器 `hive<T>`，实现位于单个头文件 [`hive.hpp`](hive.hpp)。
- 每个块固定容纳 64 个对象槽位，并通过双向链表连接多个块。
- 使用 placement new 就地构造对象：`emplace(args...)`。
- 提供 `size()`、`empty()`、`begin()`、`end()`，以及符合双向迭代器形式的 `iterator`。
- [`main.cpp`](main.cpp) 是最小可运行示例。

## 构建与运行

需要 CMake 3.16+ 和支持 C++17 的编译器。

```bash
cmake -S . -B build
cmake --build build
./build/toyhive_example
```

程序会输出：

```text
Success!
```

## 基本用法

```cpp
#include "hive.hpp"

hive<int> values;
values.emplace(42);
values.emplace(7);

for (int value : values) {
    // 使用 value
}
```

## 当前限制

- 尚未实现 `erase`、`clear`、析构、复制和移动语义；容器销毁时已构造元素及分配的块尚不会被清理。
- 尚未提供 `const_iterator`、异常安全保证、分配器支持或完整的标准容器接口。
- 迭代器失效规则尚未定义，也未建立自动化测试套件。

这些限制是该项目后续完善的重点。

## 目录结构

```text
.
├── CMakeLists.txt  # CMake 构建配置
├── hive.hpp        # hive<T> 容器实现
├── main.cpp        # 示例程序
└── README.md       # 项目说明
```

## 许可证

当前仓库尚未声明许可证。在添加许可证文件前，请勿假定可将其用于特定用途。