# ToyHive

一个用于学习容器实现的 C++17 `hive<T>` 原型。项目尝试以链式分块存储（每块 64 个槽位）和 skipfield 标记空洞，在元素不连续时仍支持高效地遍历已构造的元素。

> 当前项目处于早期实验阶段，不应在生产环境中使用。

## 当前特性

- 模板容器 `hive<T>`，实现位于单个头文件 [`hive.hpp`](hive.hpp)。
- 每个块固定容纳 64 个对象槽位，并通过双向链表连接多个块。
- 使用 placement new 就地构造对象：`emplace(args...)`。
- 支持复制构造、复制赋值、移动构造和移动赋值；移动后源容器为空且可继续使用。
- 提供 `insert(const T&)`、`insert(T&&)`，分别支持复制插入和移动插入。
- 支持 `erase(iterator)`：销毁指定元素、合并相邻空洞，并返回下一个存活元素的迭代器或 `end()`。
- 提供 `size()`、`empty()`、`begin()`、`end()`，以及可前后遍历空洞和空块的双向 `iterator`。
- 容器和块销毁时会析构所有仍存活的元素并释放块内存；块分配使用 `alignof(T)`，支持过对齐元素类型。
- 提供 CTest 边界、生命周期、擦除/skipfield、复制/移动和异常安全测试及 AddressSanitizer、UndefinedBehaviorSanitizer 覆盖脚本。
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

测试覆盖块边界插入、非平凡对象生命周期、空洞合并、跨空洞及空块的正反向遍历、`erase` 的返回迭代器行为，以及复制/移动后的值、空洞、资源所有权和生命周期。

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

- 复制操作要求 `T` 可复制构造；移动操作按块转移存储，不移动单个元素。
- 已提供 `iterator`、`const_iterator` 以及 `begin()`、`end()`、`cbegin()` 和 `cend()`；const 容器和 const 迭代器只能读取元素，不能通过迭代器修改元素。
- 尚未提供分配器支持或完整的标准容器接口；当前提供的 `clear()` 用于清空全部元素和块。
- 迭代器失效规则、`erase` 的使用前提和异常安全保证见下文；对 `end()`、空迭代器或不属于该容器/不指向存活元素的迭代器调用 `erase` 均不受支持。

这些限制是该项目后续完善的重点。

## 异常安全保证

- `emplace` 和 `insert` 在元素构造或新块分配失败时，不会增加容器大小；已有元素、空洞状态和链表结构保持不变。
- 当插入需要创建新 block 时，元素会先在未挂入容器的新 block 中构造；如果构造失败，该 block 会自动释放，不会修改原容器。
- 复制构造在复制过程中失败时，会清理已经创建的临时副本，不会泄漏资源。
- 拷贝赋值采用 copy-and-swap：如果副本构造失败，目标容器保持赋值前的状态。
- 这些保证只覆盖容器自身维护的资源和状态；`T` 的构造、复制构造以及内存分配抛出的异常会继续向调用方传播。
- 当前未承诺完整的标准容器异常规范；特别是，用户类型 `T` 的析构函数不应抛出异常。

## 迭代器失效规则

当前实现使用分块存储，插入和删除不会移动其他仍存活的元素。规则如下：

- 向已有 block 的空洞插入元素，不会使指向已有元素的 `iterator`、`const_iterator`、指针或引用失效。
- 当插入创建新 block 时，指向已有元素的 `iterator`、`const_iterator`、指针和引用仍然有效；但是旧的 `end()` 迭代器会失效，因为尾 block 已经改变，应重新获取 `end()`。
- `erase(it)` 只使被删除元素对应的 `iterator`、`const_iterator`、指针和引用失效。其他仍存活元素的迭代器、指针和引用保持有效。
- `erase(it)` 返回下一个存活元素的迭代器；如果被删除元素之后没有存活元素，则返回容器当前的 `end()`。
- `clear()` 使该容器的所有 `iterator`、`const_iterator`、指针和引用失效。
- 拷贝赋值使目标容器原有的所有 `iterator`、`const_iterator`、指针和引用失效；源容器的迭代器不受影响。
- 移动构造和移动赋值完成后，源容器以及目标容器原有的迭代器、指针和引用均视为失效；需要重新获取目标容器的迭代器。
- `swap` 完成后，两边容器原有的 `iterator`、`const_iterator`、指针和引用均视为失效。
- 容器析构后，与该容器元素或 block 相关的所有迭代器、指针和引用均失效。

失效的迭代器不能再被解引用、递增、递减或传给 `erase`。`erase` 的参数必须是当前容器中指向存活元素的有效 `iterator`；`end()`、默认构造的空迭代器以及其他容器的迭代器都不能传给 `erase`。

推荐使用重新获取 `end()` 的循环形式处理删除：

```cpp
for (auto it = values.begin(); it != values.end();) {
    if (should_remove(*it)) {
        it = values.erase(it);
    } else {
        ++it;
    }
}
```

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