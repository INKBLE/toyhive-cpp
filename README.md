# ToyHive

一个用于学习容器实现的 C++17 `hive<T>` 原型。项目尝试以链式分块存储（每块 64 个槽位）和 skipfield 标记空洞，在元素不连续时仍支持高效地遍历已构造的元素。

> 当前项目处于早期实验阶段，不应在生产环境中使用。

## 当前特性

- 模板容器 `hive<T>`，实现位于单个头文件 [`hive.hpp`](hive.hpp)。
- 每个块固定容纳 64 个对象槽位，并通过双向链表连接多个块。
- 使用 placement new 就地构造对象：`emplace(args...)`。
- 支持 `erase(iterator)`：销毁指定元素、合并相邻空洞，并返回下一个存活元素的迭代器或 `end()`。
- 提供 `size()`、`empty()`、`begin()`、`end()`，以及可前后遍历空洞和空块的双向 `iterator`。
- 容器和块销毁时会析构所有仍存活的元素并释放块内存；块分配使用 `alignof(T)`，支持过对齐元素类型。
- 提供 CTest 边界/skipfield 测试及 AddressSanitizer、UndefinedBehaviorSanitizer 覆盖脚本。
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

## 测试

启用 CTest 测试目标并运行：

```bash
cmake -S . -B build-tests -DTOYHIVE_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-tests --parallel
ctest --test-dir build-tests --output-on-failure
```

测试覆盖块边界插入、非平凡对象生命周期、空洞合并、跨空洞及空块的正反向遍历，以及 `erase` 的返回迭代器行为。

在支持 GCC 或兼容 Sanitizer 选项的编译器环境中，可运行：

```bash
./scripts/run_sanitized_tests.sh
```

该脚本使用 AddressSanitizer 和 UndefinedBehaviorSanitizer 重新构建并执行上述覆盖用例，同时编译公开迭代器和 `erase` API 检查。

## 基本用法

```cpp
#include "hive.hpp"

hive<int> values;
values.emplace(42);
values.emplace(7);

auto first = values.begin();
values.erase(first); // 返回指向原第二个元素的迭代器

for (int value : values) {
    // 使用 value
}
```

## 当前限制

- `hive` 显式禁止复制构造和复制赋值，也没有移动语义。
- 尚未实现 `clear`、`const_iterator`、分配器支持或完整的标准容器接口。
- 未定义迭代器失效规则；对 `end()`、空迭代器或不属于该容器/不指向存活元素的迭代器调用 `erase` 均不受支持。
- 尚未提供异常安全保证；若元素构造或内存分配抛出异常，容器状态不应被假定为满足标准容器的异常保证。

这些限制是该项目后续完善的重点。

## 目录结构

```text
.
├── CMakeLists.txt  # CMake 构建配置
├── hive.hpp        # hive<T> 容器实现
├── main.cpp        # 示例程序
├── scripts/        # Sanitizer 测试脚本
├── tests/          # CTest 与 API 编译检查
└── README.md       # 项目说明
```

## 许可证

当前仓库尚未声明许可证。在添加许可证文件前，请勿假定可将其用于特定用途。