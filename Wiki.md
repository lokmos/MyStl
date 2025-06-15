# 注意到的一些事情

## C++20中，将大量的原来为 throw 的函数改成了 noexcept

### 区别

| 声明方式      | C++03 含义                        | C++11+ 含义              |
|---------------|-----------------------------------|--------------------------|
| `throw()`     | 表示“**不会抛出任何异常**”        | ✅ 在 C++11 中等价于 `noexcept(true)` |
| `noexcept`    | C++11 引入的明确语法              | 更强：允许编译器优化、硬约束 |
| 没有声明异常  | 可能抛出异常                      | 默认“不知道”              |

### 更改的原因

#### 提升性能（允许更激进优化）

- 编译器对 noexcept 函数可以更大胆地优化调用、内联和栈展开
- 函数标记为 noexcept 后，调用者就可以省略额外的异常处理指令
- throw() 虽然也表示不抛异常，但对优化支持不如 noexcept 明确

#### 统一语法与现代风格

- throw() 是 C++03 异常规范老写法，已经过时（实际上 C++17 就 deprecated）
- noexcept 是现代 C++（11 起）推荐的新写法，语义更强更明确

#### 提升类型系统和泛型编程的表现

- 在模板中，你可以检测是否 noexcept，并做出更好的决策：

```c++
if constexpr (std::is_nothrow_constructible_v<T, Args...>) {
    // 安全构造
}
```

- 原来 throw() 没法直接做这种类型推导，且在模板系统中作用有限。

#### 为 constexpr 和 compile-time 编程做铺垫

- 很多标准库函数被 constexpr 化之后，如果仍然可能 throw，就无法在 constexpr 环境中用。

- 而 noexcept 是 constexpr 函数的友军：能 noexcept 的函数才更容易成为 constexpr。

### 即使声明为 noexcept，也依然可能抛出异常

```c++
T* allocate(std::size_t n) noexcept {
    if (__builtin_mul_overflow(n, sizeof(T), &n))
        throw std::bad_array_new_length(); // ❗ 实际仍可能抛异常
}
```

- 即使标记了 noexcept，如果真的抛异常，程序会 terminate（调用 std::terminate）。所以编译器 + 你要协同保证“理论上不会抛”。

## constexpr

constexpr 的本质是：“这个东西能在编译期求值”.它的目标是：

- 让你能写出在编译期间就可以计算好的表达式、函数、变量，提升程序运行时效率。

constexpr 表示“常量表达式上下文可用的内容”

- 加上 constexpr，就是告诉编译器：「我希望这个函数/变量在编译期就能被评估成一个常量，如果做不到，编译器报错。」

| 用法             | 示例                                 | 含义                 |
|------------------|--------------------------------------|----------------------|
| `constexpr` 变量  | `constexpr int x = 10 + 20;`         | 编译时常量           |
| `constexpr` 函数  | `constexpr int square(int x) { return x * x; }` | 能在编译期被调用求值 |
| `constexpr` 构造函数 | 用于类的 `constexpr` 实例化         | 编译期可创建对象     |

| 特性             | `const`                       | `constexpr`                            |
|------------------|-------------------------------|----------------------------------------|
| 修饰变量         | ✅ 表示值不可修改             | ✅ 表示值是 **编译期常量**              |
| 修饰函数         | ❌ 无意义                     | ✅ 表示函数可用于编译期求值            |
| 编译期求值能力   | ❌ 不一定能                   | ✅ **必须能**（否则编译失败）          |
| 表达式示例       | `const int x = f();`          | `constexpr int x = f();`               |

### 对比 const

```c++
const int x = 10 + 20;        // x 是运行期常量，可能编译期也能优化
constexpr int y = 10 + 20;    // y 是编译期常量，必须立即计算出 30

constexpr int add(int a, int b) {
    return a + b;
}

int arr[add(2, 3)]; // ✅ 编译期确定大小，add 必须是 constexpr
```

### 对比 consteval

- constexpr 如果能在编译期就计算，否则运行期
- consteval 只能在编译器使用，否则就报错。一般用于
  - 元编程工具函数（如类型判断、静态断言）
  - 编译期计算表、状态、逻辑
  - 强制模板只能在编译期工作（防止误用）

### 用于 new 和 delete

- C++20之前， new 和 delete 不能用于 constexpr 函数
  - 因为内存分配是不确定的行为，违反常量表达式要求
- C++20 开始支持
  - 但编译期调用时，new 分配的内存在表达式结束前必须 delete 掉。否则编译错误！

```c++
constexpr int* alloc_array(int n) {
    int* arr = new int[n];
    for (int i = 0; i < n; ++i)
        arr[i] = i;
    delete[] arr;
    return nullptr;
}
```

编译器将整个表达式看作“一个封闭生命周期”

```c++
constexpr int* leak() {
    return new int[10];      // ❌ 错误！没 delete
}
```

### 在 allocator 中的使用

支持 constexpr 容器

- C++20 开始，标准库中的许多容器（如 std::vector, std::array, std::string）都开始支持 constexpr 构造和使用。
- allocator 要能支持 constexpr，容器才能在编译期构造、操作。

现代 allocator 必须支持 constexpr 分配函数

```c++
constexpr T* allocate(std::size_t n) {
    if (std::__is_constant_evaluated())
        return new T[n];  // 编译期版本
    else
        return static_cast<T*>(::operator new(n * sizeof(T)));
}
```

这个逻辑可以：

- 在 constexpr 上下文中使用 allocator::allocate
- 在运行时走普通 operator new 分配

统一类型系统（支持 allocator_traits 推导）

```c++
static_assert(std::is_nothrow_constructible_v<T, Args...>);
```

- 这些类型判断依赖 constexpr 能力，让 allocator_traits 判断构造、析构是否安全。

## trivial vs non-trivial

### trivial

一个类型是 trivial 的，意味着：

- 它的默认构造函数、拷贝构造函数、拷贝赋值运算符、移动构造函数、移动赋值运算符和析构函数都是编译器自动生成的，并且
- 这些函数只做简单的内存复制或不做任何操作，没有用户自定义逻辑。
- 也就是，它们就像 memcpy 那样直接操作内存，没有副作用、没有额外动作。

简单说：trivial 类型可以通过二进制复制来安全地初始化、赋值和销毁。

#### 标准定义（C++标准 § [basic.types.general]）

- 它有一个 trivial 的默认构造函数
- 如果它有拷贝/移动构造函数或拷贝/移动赋值运算符，它们也必须是 trivial 的
- 它的析构函数必须是 trivial 的
- 它没有虚函数，也没有虚基类

#### trivial 的构造和析构

对于平凡类型（trivially‐default‐constructible、trivially‐destructible）的“构造”与“析构”实际上什么都不做——编译器根本不会生成任何初始化或清理的代码

- 平凡的默认构造
  - 例如内置类型 int、POD struct，或者带默认成员初始值的简单 aggregate，它们的默认构造就是一个“空操作”。
  - 在 C++ 术语里，`std::is_trivially_default_constructible_v<T>` 为 true 时，T{} 或 new (p) T 都不会在运行时生成任何指令。
- 平凡的析构
  - 如果 `std::is_trivially_destructible_v<T>` 为 true，那么对它们调用析构也是空操作，编译器不会做任何事情。

因此，当我们在一大块原始内存上“批量构造”或“批量析构”平凡类型时，实际上无需逐个调用构造/析构：

- 构造时，你只要把指针往前推进就相当于调用了一系列的空操作。
- 析构时，同理——什么都不用做，直接跳过。

### non-trivial

如果一个类型自定义了上述某些函数（比如构造函数、析构函数、拷贝/移动函数），或者需要做额外的工作，比如：

- 需要初始化资源（new、malloc、打开文件、加锁等）
- 有虚函数表指针（存在继承和多态）
- 需要正确地析构子对象
- 需要执行复杂逻辑，比如日志输出、状态设置
- 有虚基类

那么这个类型就是 non-trivial 的

### 区分的原因

性能考虑：trivial 类型可以用 memcpy、malloc/free，更快，优化器也能更好地优化。

标准库要求：比如 `std::is_trivial<T>`、`std::is_trivially_copyable<T>` 这些 traits，可以根据类型是不是 trivial 来决定是否使用更高效的实现。

ABI（应用二进制接口）一致性：trivial 类型在不同编译器、不同平台之间，内存布局和操作规则是一样的；non-trivial 类型可能不同。

# 配置器

## allocator

### 成员变量

| 类型名                                   | 定义                                                               | 说明                                                          | 弃用/移除状态          |
| ---------------------------------------- | ------------------------------------------------------------------ | ------------------------------------------------------------- | ---------------------- |
| `value_type`                             | `T`                                                                | 分配器管理的元素类型，所有 allocator 必须提供                 | 无                     |
| `pointer`                                | `T*`                                                               | 指向 `value_type` 的指针类型                                  | C++17 弃用，C++20 移除 |
| `const_pointer`                          | `const T*`                                                         | 指向 `const value_type` 的指针类型                            | C++17 弃用，C++20 移除 |
| `reference`                              | `T&`                                                               | `value_type` 的引用类型                                       | C++17 弃用，C++20 移除 |
| `const_reference`                        | `const T&`                                                         | `const value_type` 的引用类型                                 | C++17 弃用，C++20 移除 |
| `size_type`                              | `std::size_t`                                                      | 用于表示分配元素数量的无符号整数类型                          | 无                     |
| `difference_type`                        | `std::ptrdiff_t`                                                   | 指针差值类型，用于表示两个指针之间的距离                      | 无                     |
| `propagate_on_container_move_assignment` | `std::true_type`                                                   | 表示是否在容器 move 赋值时传播 allocator（C++11 引入）        | 无                     |
| `rebind`                                 | `template<class U> struct rebind { typedef allocator<U> other; };` | 将当前 allocator 重新绑定到另一个类型 `U`（旧式写法）         | C++17 弃用，C++20 移除 |
| `is_always_equal`                        | `std::true_type`                                                   | 表示两个 allocator 实例是否总是相等（用于简化比较和拷贝行为） | C++23 弃用，C++26 移除 |

**标准的 std::allocator\<T\> 实现了以下四项职责：**

| 类别 | 方法名                            | 功能说明                                                                       |
| ---- | --------------------------------- | ------------------------------------------------------------------------------ |
| 分配 | `T* allocate(size_t n)`           | 分配能存储 `n` 个 `T` 对象的原始内存（未构造）                                 |
| 释放 | `void deallocate(T* p, size_t)`   | 释放之前通过 `allocate` 分配的内存                                             |
| 构造 | `void construct(T* p, Args&&...)` | 在已分配的地址上用 placement new 构造对象（C++17 前直接提供，之后通过 traits） |
| 析构 | `void destroy(T* p)`              | 调用析构函数销毁对象（C++17 前直接提供）                                       |

### 分配

| 函数签名                                              | 可用版本范围               | 说明                         |
| ----------------------------------------------------- | -------------------------- | ---------------------------- |
| `pointer allocate(size_type n, const void* hint = 0)` | 直到 C++17（已弃用）       | 最早期版本，带 hint 参数     |
| `T* allocate(std::size_t n, const void* hint)`        | C++17 起已弃用，C++20 移除 | 明确指针类型版本，带 hint    |
| `T* allocate(std::size_t n)`                          | 自 C++17 起                | 去除 hint 参数，更现代的形式 |
| `constexpr T* allocate(std::size_t n)`                | 自 C++20 起                | 支持在常量上下文中使用       |

- 使用 hint 参数的 allocator::allocate 函数之所以在 C++20 中被移除，是因为它在实际中几乎没有用处，标准库的容器也从未利用它的功能，反而带来了额外的接口复杂性和实现负担。
  - 这个 hint 是一个“指针提示”。意图是：告诉分配器“最好把新内存分配在 hint 附近”。这样在某些平台或 allocator 中可以尝试提升局部性（cache locality）。
  - 几乎没有 allocator 实现真正利用 hint
    - 包括 std::allocator 在内，绝大多数分配器实现都会忽略 hint 参数，直接使用 ::operator new 或 malloc。
  - 标准容器根本没用 hint
- 操作系统层面不支持“基于 hint 的分配”
  - malloc() 完全忽略 hint
  - operator new() 也是不带 hint 的
  - 即使是 mmap()，你也只能“请求”一个地址（通常用于手写内存池），成功率不高且平台不统一
- 没有上下文信息：hint 的“意图”不明确
  - 你要多靠近？
  - 你想按什么规律靠近？
  - hint 是要前后排列还是对齐分布？

`operator new` 是 C++ 中用于分配原始内存（未构造对象）的函数，和 `new` 关键字不同，它只负责分配，不负责构造。
**标准的 `operator new` 函数签名**：

```c++
void* operator new(std::size_t size);                        // 常用，有异常
void* operator new(std::size_t size, const std::nothrow_t&); // 不抛异常

void* operator new[](std::size_t size);                      // 分配数组用
void* operator new[](std::size_t size, const std::nothrow_t&);
```

标准库实现大致如下（GNU libstdc++ / libc++ 中）：

```c++
void* operator new(std::size_t size) {
    if (void* p = std::malloc(size))
        return p;
    throw std::bad_alloc();  // 内存分配失败时抛出异常
}
```

可以自定义 `operator new`：

```c++
struct MyClass {
    static void* operator new(std::size_t size) {
        std::cout << "Custom operator new: size = " << size << '\n';
        return std::malloc(size);
    }

    static void operator delete(void* ptr) {
        std::cout << "Custom operator delete\n";
        std::free(ptr);
    }
};

MyClass* obj = new MyClass();  // 自动使用你重载的 operator new
delete obj;                    // 使用你定义的 delete
```

#### `allocator.h`中的实现

```c++
template <typename T>
constexpr typename allocator<T>::pointer
allocator<T>::allocate(typename allocator<T>::size_type n)
{
    if (__builtin_mul_overflow(n, sizeof(T), &n))
        throw std::bad_array_new_length();

    return static_cast<T*>(::operator new(n));
}

```

- 在C++20中，只需要实现这一个版本即可，编译器会根据上下文判断是否在常量表达式中使用
- 要注意，在 C++ 中，两个函数如果除了 constexpr 之外其签名完全相同，是不允许重定义的。因此，不能把上述表格中的三四两种函数都实现。

**和标准版的对比**

```c++
constexpr _Tp*
allocate(size_t __n)
{
    if (std::__is_constant_evaluated())
    {
        if (__builtin_mul_overflow(__n, sizeof(_Tp), &__n))
            std::__throw_bad_array_new_length();
        return static_cast<_Tp*>(::operator new(__n));
    }
    return __allocator_base<_Tp>::allocate(__n, 0);
}

```

- 标准版本增加了对常量求值环境的检查
  - 常量求值环境（constant evaluation context），就是指 编译器在编译阶段执行表达式求值，而不是运行时执行
  - C++20 中提供了这个函数：`constexpr bool std::is_constant_evaluated();` 来在代码中判断当前是否在 constant evaluation
  - 在 编译期常量上下文中，你不能使用常规的 运行时分配机制（比如 malloc / operator new）
- 标准版本使用了两层实现，在基类的基础上派生了 allocator，主要是为了可扩展性以及加入多个宏或版本控制

### 释放

`void deallocate( T* p, std::size_t n );`

**n 表示你之前通过 allocate(n) 分配的元素数量，**不是字节数，而是 元素个数。

虽然在大多数实现中，这个参数在 deallocate 里不会被使用，但是它仍然是标准接口要求保留的。

标准库为了兼容更复杂的 allocator 实现，有状态 allocator / memory pool：

- 一些高级 allocator 会使用 n 来判断：
  - 要不要从不同的池中回收内存
  - 记录块大小用于调试或统计
  - 做内存页归还（类似 jemalloc/tcmalloc）

#### '`alocator.h`中的实现

```c++
template <typename T>
void allocator<T>::deallocate(typename allocator<T>::pointer p, typename allocator<T>::size_type n)
{
    if (p == nullptr) return;
    ::operator delete(p);
}
```

**和标准版的对比**

```c++
constexpr void
deallocate(_Tp* __p, size_t __n)
{
  if (std::__is_constant_evaluated())
  {
    ::operator delete(__p);
    return;
  }
  __allocator_base<_Tp>::deallocate(__p, __n);
}
```

- 同样的，标准版本多了一个二级设计

### 构造

allocator 中 allocate 和 construct 是分开的，先分配内存，然后在分配的内存上构造新的对象。这么做的原因是

- 手动控制对象生命周期
  - 可以只分配内存、延迟构造对象，也可以显式析构对象但保留内存（如内存池）。
- 解耦构造与分配
  - 适用于需要“原地构造”（如 emplace_back）或自定义构造逻辑（如对象池、对象重用等）。

```c++
template< class U, class... Args >
void construct( U* p, Args&&... args );
```

**为什么 construct 中的类型是 U 而不是 T？**

- 为了支持构造“非 T 类型”的对象
- 虽然你定义的是 `allocator<T>`，但容器内部可能会需要你分配/构造别的类型的对象，例如：
  - `std::map<K, V>` 可能用 `allocator<std::pair<const K, V>>` 分配节点，但构造时需要 TreeNode、PairProxy 等内部结构
  - `vector<T>` 在某些实现中可能构造 T* 前要构造临时对象
- 容器用 `allocator_traits<A>::rebind_alloc<U>` 得到新的 allocator 类型，再尝试构造 U 类型对象
- 所以标准库要求 construct() 必须是泛型的，支持构造任意类型 U，不是只限于 T。

**为什么还要用可变参数 `Args&&... args`**

- 为了支持构造任意参数组合的对象，统一支持所有构造函数
  - 可变参数模板（Variadic Templates）
    - 接收任意数量、任意类型的参数
  - 完美转发（Perfect Forwarding）
    - 原样传递这些参数到构造函数

#### `allocator.h`中的实现

```c++
// 注意这里要用模板嵌套
// allocator 是模板类，所以外层要 template<typename T>
// construct 是 allocator 的成员函数，所以内层也要 template<typename U, typename... Args>
template <typename T>
template <typename U, typename... Args>
U* allocator<T>::construct(U *p, Args&&... args)
{
    return ::new(static_cast<void*>(p)) U(std::forward<Args>(args)...);
}
```

**placement new**

```c++
new (memory_address) Type(constructor_args...)
```

- 意思是：在指定的内存地址 `memory_address` 上，以 `Type(constructor_args...)`的方式构造对象
- 和普通的 `new`（比如 `new T(...)`）不同，placement new 不会分配内存，它只是在给定内存上“就地构造”对象。

**指针p如何指示内存信息**

- 指针 p 并不“知道”大小
  - C++ 中的原始指针（比如 T* p）只是一个 内存地址，它本身 不包含任何“大小”信息
- ::operator new(size_t bytes) 被调用
  - 这是标准的 C++ 分配函数，它向操作系统请求 bytes 字节的原始内存
- 内存管理器（比如 malloc、jemalloc、glibc malloc 等）会记录分配的大小
  - 些信息并不在 p 本身，而是在p 前面的几个字节里 —— 这是 malloc 实现的“内部结构”。

**回到代码**

| 部分                                | 含义 |
|-------------------------------------|------|
| `::new`                              | 调用的是全局作用域的 `operator new`，这里是 **placement new** |
| `(static_cast<void*>(p))`            | 把指针 `p` 转成 `void*`，告诉编译器“我要在这个内存地址上构造对象” |
| `U(...)`                             | 构造类型为 `U` 的对象 |
| `std::forward<Args>(args)...`        | 完美转发参数，保证右值保持右值、左值保持左值 |

### 析构

和构造类似，但析构只需要考虑支持构造“非 T 类型”的对象

#### `allocator.h`中的实现

```c++
template <typename T>
template <typename U>
void allocator<T>::destroy(U *p)
{
    if (p == nullptr) return;
    p->~U();
}
```

### 补充：其他函数

#### max_size

返回理论上能够 `allocate(n, 0)` 的最大值 n

```c++
template <typename T>
typename allocator<T>::size_type allocator<T>::max_szie() const noexcept
{
    return std::numeric_limits<size_type>::max() / sizeof(T);
}
```

#### address

确保在没有重载 `operator&` 的情况下，也可以获得 x 的地址

```c++
template <typename T>
typename allocator<T>::const_pointer allocator<T>::address(typename allocator<T>::const_reference x) const noexcept
{
    return std::addressof(x);

}
```

#### 待做

C++23的特性和函数，例如 `allocate_at_least`

# 迭代器

整体的思路是：

- 首先设计一个基类
- 实现迭代器的类型萃取
- 实现六类迭代器
- 实现通用的 reverse_iterator

## 基类 `iterator_base`

这个类什么都不做，只提供成员变量

## 萃取 `iterator_traits`

实现了通用版本和针对原始指针的特化版本

### 特化版本中使用 random_access_iterator_tag

因为：C++ 中的原始指针（如 int*）天然就支持所有随机访问操作，它的语义和能力完全满足 random_access_iterator 的要求。

不使用 contiguous_iterator_tag

- 这个是 C++20 引入的专门标记“保证内存物理连续”的 iterator 类型，像 vector::iterator 就是一个 contiguous_iterator_tag。
- 虽然 `T*` 当然是连续的，但标准库中没有对 T* 提供 contiguous_iterator_tag 的萃取支持，主要是：
  - 保证 旧代码行为不变
  - 保留对 contiguous_iterator_tag 的使用控制在库作者手中
  - 防止泛型算法意外匹配（比如 ranges 中的 tag dispatch）

在自己的STL中可以使用，我为了对应标准库的做法，暂时先不写

## 6类迭代器

| 类别                      | 继承于               | 功能范围         | 必须操作符                                              | 可选操作符                  | 示例容器              |
|---------------------------|----------------------|------------------|---------------------------------------------------------|-----------------------------|-----------------------|
| `output_iterator`         | -                    | 单向写入         | `*`, `=`, `++`, `++(int)`                               | 无                          | `ostream_iterator`    |
| `input_iterator`          | -                    | 单向读取         | `*`, `->`, `++`, `++(int)`, `==`, `!=`                  | 无                          | `istream_iterator`    |
| `forward_iterator`        | `input_iterator`     | 多次遍历         | 继承 `input_iterator` 所有，支持拷贝构造与赋值         | 稳定 deref 语义             | `forward_list`        |
| `bidirectional_iterator`  | `forward_iterator`   | 双向遍历         | 上述所有 + `--`, `--(int)`                             | 无                          | `list`, `map`         |
| `random_access_iterator`  | `bidirectional_iterator` | 随机访问   | 上述所有 + `+`, `-`, `+=`, `-=`, `[]`, 比较运算符      | `it2 - it1` 差值计算         | `vector`, `deque`     |
| `contiguous_iterator`     | `random_access_iterator` | 物理连续内存 | 上述所有，语义要求 `&*(it + n) == base + n`            | `base()`（可选）            | `vector`, `array`     |

- 所有 `<T, IsConst>` 类型迭代器都建议添加：
  
```c++  
template <bool B, std::enable_if_t<IsConst && !B, int> = 0>
  my_iterator(const my_iterator<T, B>& other)
```

- 禁止 const → non-const 转换
- 不提供 operator= 跨类型赋值（与标准库一致）

## reverse_iterator

`reverse_iterator` 是一个“反向的视图”，它的 operator* 总是返回它“前一个”元素，即 --current_。

reverse_iterator 是一个适配器（Adapter），其本质设计是，对于任意普通迭代器 it，reverse_iterator(it) 表示的其实是：
“访问 it - 1 所指向的元素”

需要注意的是，虽然 reverse_iterator 是泛型模板，可以适配除 output_iterator 之外的其他五类迭代器，但只有当底层迭代器本身支持某个操作（如 []、+、-）时，reverse_iterator 才能提供这些操作。

- 如果使用了某个底层迭代器不支持的函数，编译会报错，这也是标准库预期的功能

## 补充

### 通用判断

```c++
mystl::is_iterator_of_tag<Iter, mystl::input_iterator_tag> → true/false
```

用于判断某个迭代器 `Iter` 是否属于（或派生于）指定的 iterator tag

```c++
template <typename Iter, typename Tag>
concept is_iterator_of_tag = requires {
    typename iterator_traits<Iter>::iterator_category;
    requires std::derived_from<typename iterator_traits<Iter>::iterator_category, Tag>;
};
```

- `typename iterator_traits<Iter>::iterator_category`：先提取该迭代器的 category。
- `requires std::derived_from<...>`：判断这个 category 是否是我们指定的 Tag 的派生类。
  - 使用派生，我们可以确保，如果某个低级的 iterator 通过判断，那么比它高级的 iterator 一定也能通过判断
  - 这体现了STL 算法和容器设计时的原则：函数接受的迭代器类型，应为它正确工作的最“低级”迭代器种类

## 和标准库的差距

### 当前实现

| 模块                        | 状态      | 说明                                         |
|-----------------------------|-----------|----------------------------------------------|
| 六大迭代器分类（input~contiguous） | ✅ 已全实现 | 按照 C++ iterator category 正确封装         |
| `const` 转换（非 const → const） | ✅ 安全实现 | 使用 SFINAE 限制构造                        |
| `reverse_iterator` 泛型封装     | ✅ 完整     | 与标准库基本对齐                            |
| `iterator_traits` + 指针特化   | ✅ 萃取完备 | 可用于泛型算法                              |
| `iterator_concept` 支持       | ✅ C++20 结构 | 提供 `iterator_concept` 与 `iterator_category` |

### 关键差距

| 点位                           | 标准库实现方式                      | 你当前的实现                      | 差距或建议                           |
|--------------------------------|-------------------------------------|-----------------------------------|--------------------------------------|
| 🧊 裸指针封装（ABI 安全）        | 用 `__normal_iterator` 封装 T*      | 直接使用 `T*`                     | ⚠️ 缺少封装器结构，无法封装容器信息  |
| 🔒 安全性检测（越界、合法访问） | Debug 模式提供 `_Safe_iterator`    | ❌ 无                              | ⚠️ 可加 Debug 宏进行断言检查         |
| 🔄 迭代器变种（reverse, insert） | 有 `reverse_iterator`, `insert_iterator` | 仅实现 `reverse_iterator`       | ✅ reverse 已完成，插入类待扩展      |
| 🧠 多容器类型支持               | iterator 封装支持不同容器切换        | 仅以裸指针为核心                   | ⚠️ 需考虑 future for `map`, `list`   |
| 🧰 `std::iterator` 接口         | 已在 C++17 弃用                    | ✅ 合理未实现                      | ✅ 与现代标准一致                     |
| 📜 range/ranges 适配（C++20）   | 提供 `iterator_concept` 等接口     | ✅ 已定义 concept 类型别名         | ✅ 可进一步对接 std::ranges 使用     |
| 📦 拷贝/移动/比较语义完整性     | 全部支持                          | ✅ 正常实现                        | ⚠️ 可加 static_assert 检测支持性     |

### 建议扩展模块

| 模块                     | 优先级 | 建议做法                             |
|--------------------------|--------|--------------------------------------|
| `insert_iterator` 等变种 | ⭐⭐⭐     | 扩展 `back_insert`, `front_insert`   |
| `normal_iterator<T*>`    | ⭐⭐     | 封装裸指针为 ABI 安全可控迭代器       |
| `debug_iterator`         | ⭐⭐     | 宏开关控制断言，如 `MYSTL_DEBUG`     |
| range/ranges 适配         | ⭐⭐     | 提供 `sentinel`, `common_iterator`  |
| allocator-aware iterator | ⭐      | 提供能提取容器内 allocator 的 iterator |

# memory

这个头文件中存放和内存相关的一些操作

## 批量填充和析构

### 批量填充 n 个元素

```c++
// uninitialized_value_construct_n
// Constructs n elements of type T in uninitialized memory [first, first+n)
// If T is trivially default-constructible, skips per-element calls.
template <typename Alloc, typename ForwardIt, typename Size>
ForwardIt uninitialized_value_construct_n(Alloc& alloc, ForwardIt first, Size n)
{
    using T = typename std::iterator_traits<ForwardIt>::value_type;
    ForwardIt cur = first;
    Size constructed = 0;
    try {
        // 对于“平凡可默认构造”（POD 或内置类型），不需要逐元素调用 ctor
        if constexpr(std::is_trivially_default_constructible_v<T>) {
            // For trivially default-constructible types, zero-initialize if pointer
            // 如果迭代器真的是原始指针，还可以一次性 memset 将内存清零（value-init 效果）；
            if constexpr(std::is_pointer_v<ForwardIt>) {
                std::memset(first, 0, n * sizeof(T));
            }
            
            //直接把已“构造”计数设成 n，并把游标 cur 前移 n。
            constructed = n;
            std::advance(cur, n);
        }
        else {
            for(; constructed < n; ++constructed, ++cur) {
                std::allocator_traits<Alloc>::construct(alloc, std::addressof(*cur));
            };
        }
        return cur;
    } catch (...) {
        destroy_n(alloc, first, constructed);
        throw;
    }
}

// uninitialized_fill_n
// Constructs n copies of value in uninitialized memory [first, first+n)
// Returns iterator past the last constructed element.
template <typename Alloc, typename ForwardIt, typename Size, typename U>
ForwardIt uninitialized_fill_n(Alloc& alloc, ForwardIt first, Size n, const U& value)
{
    using T = typename std::iterator_traits<ForwardIt>::value_type;
    ForwardIt cur = first;

    if constexpr (std::is_trivially_copyable_v<T> && std::is_copy_constructible_v<T>) {
        // Trivial T: direct placement-new in a simple loop, no exception rollback needed.
        for (Size i = 0; i < n; ++i, ++cur) {
            std::allocator_traits<Alloc>::construct(alloc, std::addressof(*cur), value);
        }
    }
    else {
        // Non-trivial T: need strong exception safety.
        Size constructed = 0;
        try {
            for (; constructed < n; ++constructed, ++cur) {
                std::allocator_traits<Alloc>::construct(alloc, std::addressof(*cur), value);
            }
        } catch (...) {
            destroy_n(alloc, first, constructed);
            throw;
        }
    }

    return cur;
}

// destroy_n
// Destroys n elements of type T in memory [first, first+n)
// If T is trivially destructible, does nothing and advances iterator.
template <typename Alloc, typename ForwardIt, typename Size>
ForwardIt destroy_n(Alloc& alloc, ForwardIt first, Size n) {
    using T = typename std::iterator_traits<ForwardIt>::value_type;
    ForwardIt cur = first;
    if constexpr (std::is_trivially_destructible_v<T>) {
        std::advance(cur, n);
    }
    else {
        for (Size i = 0; i < n; ++i, ++cur) {
            std::allocator_traits<Alloc>::destroy(alloc, std::addressof(*cur));
        }
    }
    return cur;
}
```

#### `uninitialized_value_construct_n`

这个函数采用默认构造

- 通过 `iterator_traits` 拿到元素类型 `T`。
- 对于平凡类型，因为它们的构造函数什么都不做，所以使用 `memset` + `advance` 直接移动指针即可
- 对于非平凡类型，对每一个位置，用 `allocator_traits::construct(alloc, ptr)` 触发 `new (ptr) T()`。

所谓的在那块内存上以 `placement new` 的方式调用 `T()` —— 也就是默认构造:

```c++
std::allocator_traits<Alloc>::construct(
    alloc,
    std::addressof(*cur)    // 取出 cur（T*）所指内存的地址
);
```

等价于：

```c++
::new (static_cast<void*>(std::addressof(*cur))) T();
```

#### `uninitialized_fill_n`

这个函数与上一个最大的不同在于不再使用默认构造了，因此，即便是 `trivially copyable`、`trivially default‐constructible` 的类型，标准库的 `std::uninitialized_fill_n` 通常也还是在每个位置做一次 **placement‐new** （或等价的构造/赋值）——因为这才能保证对所有类型都满足正确的构造语义。而它对所谓“平凡类型”的优化，往往只发生在 默认构造（`uninitialized_default_construct_n`／`uninitialized_value_construct_n`）那一支路上

- 因为只有这样才能保证“在未初始化存储上调用 T(x)”这一语义，对所有类型都安全
- 而对 **trivial** 类型来说，这个 `::new (p) T(x)` 或者 `allocator_traits::construct(alloc,p,x)` 在编译之后，往往就被优化成一条简单的内存拷贝或寄存器赋值指令，根本没有函数调用开销——所以标准库也没必要再额外做 `memcpy` 的 hack。
- 如果你对非零的填充值做更激进的批量拷贝优化，就需要自己判定迭代器是原始指针且 `T` 是 **trivially_copyable**，然后先在第一个元素上 `construct(…,value)`，再用 `memcpy` 或指数倍增(copy‐doubling)把它复制到后续内存。但这超出了标准规定的通用实现——标准库只在“value‐init”那种“全部置零”场景下才做批量优化。

# vector

## vector_base

### 设计的目的

| 目的                        | 解释                                                                 |
|-----------------------------|----------------------------------------------------------------------|
| **内存管理职责分离**        | 把“指针 + allocator”统一封装，便于构造、析构集中管理               |
| **代码复用与模块解耦**      | 有些构造函数只处理内存而不涉及逻辑（如 `allocate_space(n)`)         |
| **EBO 优化**                | 如果 `allocator` 是空类，继承它可以节省空间（Empty Base Optimization） |
| **减少模板展开冗余**        | 某些编译器对大模板类处理开销大，拆出 `base` 有助于减少依赖链         |
| **对标标准库结构**          | `libstdc++`, `libc++`, MS STL 都是这么设计的                         |

### 成员拆解

```c++
vector_base<T, Alloc>       // 包含裸指针 + allocator
   └── _start
   └── _finish
   └── _end_of_storage
   └── _allocator

vector<T, Alloc> : protected vector_base<T, Alloc>
   └── 实现逻辑（构造、元素操作、接口等）
```

这种分层的好处在于：

- 构造/析构可以只操作 vector_base
- `vector<T>` 专注于逻辑（如 push_back, insert 等）
- 更方便将来实现异常安全构造、拷贝、移动（统一处理底层指针）

### Empty Base Optimization（EBO）

当 allocator 是空类（如 `std::allocator<T>`），你继承它而不是包含它，可以节省空间！

```c++
// EBO-friendly
class vector_base : private Alloc {
    T* _start;
    T* _finish;
    T* _end;
};

// 非 EBO，会额外占 1 byte：
class vector {
    Alloc alloc_;    // 即使是空类也要占 1 字节
    T* _start;
    ...
};
```

## vector

### 辅助函数

按照标准库做法，这些函数会用在 vector 接口的不同实现中，所以预先实现这些功能

#### `_fill_initialize(n, val)`

该函数构造正好 n 个元素，不预留额外空间

- 构造函数的语义是“准确地初始化 N 个元素”
  - 它不是 reserve 语义
  - 用户没请求额外空间，库就不该超配（否则浪费）
- 构造时多余的内存是浪费

#### `void _realloc_insert(Args&&... args)`

扩容函数，完成以下几步：

1. 计算新空间大小
2. 分配新空间大小的内存
3. 移动旧元素
4. 插入新元素
5. 销毁旧元素并释放空间

在3，4步中，如果构造失败，则要销毁已构造并释放新内存

```c++
template <typename... Args>
void _realloc_insert(Args&&... args)
{
  const size_type old_size = this->_finish - this->_start;
  const size_type old_capacity = this->_end_of_storage - this->_start;
  const size_type new_capacity = old_capacity == 0 ? 1 : old_capacity * 2;

  pointer new_start = this->_allocator.allocate(new_capacity);
  pointer new_finish = new_start;

  try {
      // 移动旧元素
      for (pointer p = this->_start; p != this->_finish; ++p, ++new_finish) {
          this->_allocator.construct(new_finish, std::move(*p));
      }

      // 插入新元素
      this->_allocator.construct(new_finish, std::forward<Args>(args)...);
      ++new_finish;
  }
  catch(...) {
      // 构造失败则销毁已构造并释放新内存
      for (pointer p = new_start; p != new_finish; ++p) {
          this->_allocator.destroy(p);
      }
      this->_allocator.deallocate(new_start, new_capacity);
      throw;
  }

  // 销毁旧元素并释放旧空间
  for (pointer p = this->_start; p != this->_finish; ++p) {
      this->_allocator.destroy(p);
  }
  this->_allocator.deallocate(this->_start, old_capacity);

  // 更新指针
  this->_start = new_start;
  this->_finish = new_finish;
  this->_end_of_storage = new_start + new_capacity;
}
```

在移动旧元素时，使用 `std::move`　是为了支持那些 只能移动，不能拷贝 的类型：

- `std::unique_ptr<T>`
- 带 `delete copy constructor` 的类（常用于资源管理）
- 不支持 copy，但支持 move 的自定义对象

使用 `move` 后，新旧地址的资源不会冲突，以 `unique_ptr` 为例

```c++
unique_ptr(unique_ptr&& other) noexcept
    : ptr_(other.ptr_) {
    other.ptr_ = nullptr; // <== 核心逻辑在这
}
```

- 不会有两个拥有相同资源的地址，不会冲突，unique_ptr 被移动后旧地址会失效
- 新的 unique_ptr 拿走了原来的资源（原来的地址）
- 原来的 unique_ptr 变成空指针，不再拥有任何资源
  - 当 old 析构时，delete nullptr 是合法的、什么都不做

这里不能使用 `std::move_if_noexcept`

- 它的行为是：
  - 如果 T 的 move 构造函数是 noexcept，就用 std::move(x)；
  - 否则用 x（调用拷贝构造）。
- 它的设计意图是：
  - 在 可能会抛异常 的情况下，避免 move 破坏了源对象，转而使用 safer 的 copy。
- 原因一：在重新构造 old elements 时，不怕异常
  - 此处是新空间中的构造操作，即便抛异常，我们会：
    - 及时 destroy() 已构造元素
    - deallocate() 新空间
    - 保持原 vector 的数据不动（因为原数据还在 old buffer）
- 原因二：如果用了 move_if_noexcept，反而不如 move
  - 考虑一个只有 move 构造、但 `noexcept(false)`
  - 使用 std::move_if_noexcept(x) 会 fallback 到 copy —— 而这个类没有 copy，直接报错！

#### `_range_initialize`

这个函数的目标，是构造两个迭代器所指示的区间内的值，因为只有 random access 及以上的迭代器才可以获得区间长度，所以有两种针对不同迭代器的实现

```c++
    // input/forward
    template <typename InputIt>
    void _range_initialize(InputIt first, InputIt last, std::input_iterator_tag)
    {
        while (first != last) {
            emplace_back(*first);
            ++first;
        }
    }

    // random access/contiguous
    template <typename RandomIt>
    void _range_initialize(RandomIt first, RandomIt last, std::random_access_iterator_tag)
    {
        size_type n = static_cast<size_type>(last - first);
        this->_start = this->_allocator.allocate(n);
        this->_finish = this->_start;
        this->_end_of_storage = this->_start + n;

        try {
            for (; first != last; ++first, ++this->_finish) {
                this->_allocator.construct(this->_finish, *first);
            }
        } catch (...) {
            for (pointer p = this->_start; p != this->_finish; ++p)
                this->_allocator.destroy(p);
            this->_allocator.deallocate(this->_start, n);
            this->_start = this->_finish = this->_end_of_storage = nullptr;
            throw;
        }
    }
```

对于使用 emplace_back 的版本来说，会引入扩容的操作，但因为事先不知道区间大小，所以无法预分配精准的空间，因此这样的做法是合理的。

- 会带来空间上的浪费

### 构造相关函数

#### `vector()`

##### `explicit vector(const allocator_type& alloc)`

显式的 allocator 构造，基类 `vector_base` 接受一个 allocator 作为构造参数，因此直接调用对应基类的构造函数即可

```c++
explicit vector(const allocator_type& alloc) : base(alloc) {}
```

##### `vector() noexcept(noexcept(Allocator()))`

`vector() noexcept(noexcept(Allocator())) : vector(Allocator()) {}`

是 C++11 以来标准库推荐的实现方式，目的是：

- ✅ 利用 统一委托构造（delegating constructor）简化逻辑
- ✅ 保证 noexcept 语义对齐：如果构造 Allocator() 是 noexcept，这个默认构造函数也是 noexcept
  - 其意思是，如果默认构造 allocator 不会抛异常，那么整个 vector 构造也不会抛异常

借用上一个显式的 allocator 构造函数

```c++
vector() noexcept(noexcept(allocator_type())) : vector(allocator_type()) {}
```

##### `explicit vector(size_type n, const allocator_type& alloc = allocator_type())`

目标：构造一个包含 `count` 个元素的 `vector`，其中每个元素都是通过默认构造 `T{}` 得到的

类型要求

- `std::default_initializable<T>`: 确保 `T{}` 是合法表达式

```c++
template <std::default_initializable U = T>
explicit vector(size_type n, const allocator_type& alloc = allocator_type())
  : base(alloc)
{
  _fill_initialize(n, T{});
}
```

cppreference 要求

- Constructs a vector with `count` default-inserted objects of T. No copies are made.
- If T is not DefaultInsertable into `std::vector<T>`, the behavior is undefined.
- `template <std::default_initializable U = T>`
  - `U` 是一个模板参数，默认是 `T`。
  - `std::default_initializable<U>` 是一个 **Concept**，用于检查：
    - 是否可以写 `U u{}` 进行默认构造
  - 如果 `T` 没有默认构造函数（如 `struct S { S(int); };）`，这段代码将 拒绝实例化

##### `constexpr vector(size_type count, const T& value, const allocator_type& alloc = allocator_type())` —— 用一个值构造 n 个元素

它需要完成的事情：

- 分配 n 个元素空间
- 对每个元素构造 T(val)
- 设置 `_start`, `_finish`, `_end_of_storage`

该函数调用 `_fill_initialize(n, val)` 来完成

##### `constexpr vector(InputIt first, InputIt last, const allocator& alloc = allocator_type())`

区间构造函数，因为 InputIterator 和 RandomAccessIterator 有区别，后者可以提前获取区间大小，而前者只能逐一遍历，所以在实现策略上也不同，要实现函数分发。这一点借助于 `_range_initialize` 的两个不同版本实现。

```c++
template <typename Integer>
    requires std::integral<Integer>
    vector(Integer n, const T& value, const allocator_type& alloc = allocator_type())
        : base(alloc)
    {
        _fill_initialize(static_cast<size_type>(n), value);
    }

    // 区间构造，使用标准 iterator_traits 和 iterator_category
    template<typename InputIt>
    requires std::input_iterator<InputIt> &&
             std::constructible_from<T, typename std::iterator_traits<InputIt>::reference> &&
             (!std::integral<InputIt>)
    vector(InputIt first, InputIt last, const allocator_type& alloc = allocator_type())
        : base(alloc)
    {
        _range_initialize(first, last, typename std::iterator_traits<InputIt>::iterator_category{});
    }
```

区间构造函数很复杂，因为如果不做限定，一个构造函数例如 `vector(5,10)` 就会落入到这个构造函数中，但 `int` 显然不具有迭代器的任何性质，所以会报错

在 vector(4, 10) 中，参数类型是两个 int，于是 C++ 会尝试匹配：

- vector(int, int) ← 这是可以视为 InputIt first = int, InputIt last = int 的一个模板实例
- vector(size_type, const T&) ← 也能匹配

所以 两者都可以匹配成功，但：

- 模板函数的推导优先级会偏向“更泛化”的模板（即 InputIt 版本），如果它能匹配，那它就会被选择！

因为：

- int 可以是 InputIt（虽然是个不合法的 iterator，但语义上类型满足）

###### 不可拷贝的区间

区间构造实现了两个函数，如果是 不平凡，或者不可拷贝的类型，会走通用的分支，即逐个构造；但是通用版本也会尝试从 `*first` 构造，因此要求类型有一个 拷贝构造函数
对于 `unique_ptr` 这样只能 move 的类型，即使通用版本也无法直接处理，按照标准库的做法，针对这种类型，不会做特殊化，而是需要用户传入 **移动迭代器**

```c++
#include <iterator>  // std::make_move_iterator

std::vector<std::unique_ptr<Foo>> src = /*…*/;
std::vector<std::unique_ptr<Foo>> dst(
    std::make_move_iterator(src.begin()),
    std::make_move_iterator(src.end())
);
```

- `std::make_move_iterator` 会让 `*it` 产生一个 `T&&`（右值引用）
- 于是通用构造就变成 `allocator_traits::construct(alloc, address, std::move(original));`
  - 这正好能调动 unique_ptr 的 移动构造

##### 拷贝构造

```c++
// 拷贝构造
constexpr vector(const vector& other)
    : base(other._allocator)
{
    _range_initialize(other._start, other._finish, std::random_access_iterator_tag{});
}

constexpr vector(const vector& other, const Allocator& alloc)
    : base(alloc)
{
    _range_initialize(other._start, other._finish, std::random_access_iterator_tag{});
}
```

##### 移动构造

###### `constexpr vector(vector&& other) noexcept`

```c++
constexpr vector(vector&& other) noexcept
    : base(std::move(other.allocator))
{
    this->_start = other._start;
    this->_finish = other._finish;
    this->_end_of_storage = other._end_of_storage;

    other._start = nullptr;
    other._finish = nullptr;
    other._end_of_storage = nullptr;
}
```

前提需要保证 allocator 是可以移动的，当然，因为我实现的allocator是无状态的，所以使用默认的移动构造和移动赋值函数即可

注意一定要把原来的 vector 的指针都置空，这样才符合移动的语义

###### `constexpr vector(vector&& other, const allocator_type& alloc)`

```c++
constexpr vector(vector&& other, const allocator_type& alloc)
    : base(alloc)
{
    if (alloc == other._allocator) {
        this->_start = other._start;
        this->_finish = other._finish;
        this->_end_of_storage = other._end_of_storage;
        other._start = nullptr;
        other._finish = nullptr;
        other._end_of_storage = nullptr;
    }
    else {
        _range_initialize(
            std::make_move_iterator(other._start),
            std::make_move_iterator(other._finish),
            std::random_access_iterator_tag{}
        );
    }
}
```

如果 alloc 和目标 vector 的 allocator 一致，说明可以直接偷取原vector的资源，因此和第一个移动构造函数没有区别。否则，因为两者的 allocator 不同，只能采取一个一个 move 元素的方法，这一点可以交给 `_range_initialize`　来完成，但因为是 move，所以需要用 `std::make_move_iterator`

- 它是 C++11 引入的一个工具函数，用于将普通迭代器包装成一个“移动迭代器”
- 它让迭代器在解引用`（*it）`时返回 `std::move(*it)`，而不是 `*it` 本身
- 用一个专门为“移动”而设计的迭代器来初始化当前容器，避免拷贝，提升性能

##### 初始化列表构造函数

```c++
constexpr vector(std::initializer_list<T> init, const allocator_type& alloc = allocator_type())
    : base(alloc)
{
    _range_initialize(init.begin(), init.end(), std::random_access_iterator_tag{});
}
```

#### 析构函数

```c++
~vector()
{
    // 首先销毁所有元素
    for (pointer p = this->_start; p != this->_finish; ++p) {
        this->_allocator.destroy(p);
    }

    // 然后释放内存
    this->_allocator.deallocate(this->_start, mystl::distance(this->_start, this->_end_of_storage));
    
    // 最后将指针置空
    this->_start = nullptr;
    this->_finish = nullptr;
    this->_end_of_storage = nullptr;
}
```

#### 赋值函数

##### 拷贝赋值

###### `vector& operator=(const vector& other)`

这个操作的本质是 深拷贝赋值，核心目标是：将 `other` 中的元素拷贝到当前对象中，并保持强异常安全。

```c++
constexpr vector& operator=(const vector& other)
{
    if (this == &other) {
        return *this;
    }

    // 如果需要传播 allocator，且 allocator 不等，先释放原有资源
    if constexpr (std::allocator_traits<allocator_type>::propagate_on_container_copy_assignment::value) {
        if (this->_allocator != other._allocator) {
            clear();
            this->_allocator = other._allocator;
            this->_start = this->_finish = this->_end_of_storage = nullptr;
        }
    }

    const size_type otherSize = other.size();

    if (otherSize > this->.capacity()) {
        // 强异常安全策略：拷贝构造副本，再交换
        vector tmp(other);
        swap(tmp); // tmp 会析构，释放资源
    } else {
        // 容量足够，重用内存
        size_type i = 0;
        for (; i < this->size() && i < otherSize; ++i) {
            // 原始指针天然支持 copy-assign
            this->_start[i] = other._start[i];
        }
        for (; i < otherSize; ++i) {
            this->_allocator.construct(this->_finish, other._start[i]);
            ++this->_finish;
        }
        for (; i < this->size(); ++i) {
            this->_allocator.destroy(this->_start + i);
        }
        this->_finish = this->_start + otherSize;
    }
    
    return *this;
}
```

首先判断是否是自赋值

- 如果是，则直接返回，不需要任何操作
- 否则进入拷贝赋值阶段

按照 cppreference 的要求，需要判断 POCCA 条件

- POCCA 表示 在容器进行拷贝赋值操作时，是否应将 allocator 一起拷贝。
- 在 C++ 标准库中，分配器（Allocator）控制了容器的内存分配行为，而 allocator 本身可能有状态（比如自定义分配器中保存了一个内存池的指针
- 这个决策由 allocator_traits 的成员：`std::allocator_traits<Allocator>::propagate_on_container_copy_assignment` 来决定，其类型是 `std::true_type` 或 `std::false_type`。

在拷贝赋值阶段，先判断当前 vector 容量是否足够

- 如果容量不足，采用强异常安全策略：先构造一个副本，再 swap
- 否则重用现有内存
  - 另外，也不需要考虑类型不一致的问题
    - operator= 是成员函数，这两个 vector 必须是完全相同模板类型的实例，即 `vector<T, Alloc>`
    - 若你需要支持类型转换（如 `vector<int>` 赋值给 `vector<float>`），那属于 容器间的转换赋值，标准库不支持，也不应由这个函数承担。

###### 标准库实现了类似的逻辑

```c++
template<typename _Tp, typename _Alloc>
    _GLIBCXX20_CONSTEXPR
    vector<_Tp, _Alloc>&
    vector<_Tp, _Alloc>::
    operator=(const vector<_Tp, _Alloc>& __x)
    {
      if (std::__addressof(__x) != this)
        {
        _GLIBCXX_ASAN_ANNOTATE_REINIT;
    #if __cplusplus >= 201103L
        if (_Alloc_traits::_S_propagate_on_copy_assign())
            {
            if (!_Alloc_traits::_S_always_equal()
                && _M_get_Tp_allocator() != __x._M_get_Tp_allocator())
                {
            // replacement allocator cannot free existing storage
            this->clear();
            _M_deallocate(this->_M_impl._M_start,
                    this->_M_impl._M_end_of_storage
                    - this->_M_impl._M_start);
            this->_M_impl._M_start = nullptr;
            this->_M_impl._M_finish = nullptr;
            this->_M_impl._M_end_of_storage = nullptr;
            }
            std::__alloc_on_copy(_M_get_Tp_allocator(),
                    __x._M_get_Tp_allocator());
            }
    #endif
        }
    }
```

- `_Alloc_traits::_S_propagate_on_copy_assign()` 实际就是 `allocator_traits<Alloc>::propagate_on_container_copy_assignment::value`

##### 移动赋值

```c++
// 移动赋值
constexpr vector& operator=(vector&& other) noexcept (
    std::allocator_traits<Alloc>::propagate_on_container_move_assignment::value ||
    std::allocator_traits<Alloc>::is_always_equal::value
)
{
    if (this == &other) {
        return *this;
    }

    constexpr bool pocma = std::allocator_traits<allocator_type>::propagate_on_container_move_assignment::value;

    // 如果 PO_MCA 为 true，且 allocator 允许传播，直接 move allocator
    if constexpr (pocma) {
        // allocator 允许 move 赋值，直接偷资源
        // 清理旧资源
        this->clear();
        this->_allocator.deallocate(this->_start, this->capacity());

        this->_allocator = std::move(other._allocator);
        this->_start = other._start;
        this->_finish = other._finish;
        this->_end_of_storage = other._end_of_storage;

        // cppreference: After the move, other is in a valid but unspecified state.
        // 这里我们将 other 的指针置空，防止析构时 double free
        // 仍然可以访问 other，但它的状态是 "有效但未指定"（valid but unspecified） —— 你不能假设其中有任何元素或其 size() 是多少，只能安全执行析构、赋值、clear()、empty() 等操作。
        other._start = other._finish = other._end_of_storage = nullptr;
    } else {
        // allocator 不传播，必须检查是否相等
        if (this->_allocator == other.allocator) {
            // allocator 相等，资源可以转移
            // 清理旧资源
            this->clear();
            this->_allocator.deallocate(this->_start, this->capacity());

            this->_start = other._start;
            this->_finish = other._finish;
            this->_end_of_storage = other._end_of_storage;
            other._start = other._finish = other._end_of_storage = nullptr;
        } else {
            // allocator 不相等，只能拷贝赋值
            this->clear();
            this->_allocator.deallocate(this->_start, this->capacity());

            _range_initialize(
                std::make_move_iterator(other._start),
                std::make_move_iterator(other._finish),
                std::random_access_iterator_tag{}
            );
        }
    }
}
```

##### 初始化列表赋值

因为标准库的要求是尽可能复用现有内存，所以需要根据当前 `vector` 的 `capacity`，选择不同的策略

```c++
constexpr vector& operator=(std::initializer_list<value_type> ilist)
{
    const size_type n = ilist.size();

    if (n > this->capacity()) {
        // 容量不足，重新分配内存
        vector tmp(ilist);
        this->swap(tmp);
    } else {
        auto it = ilist.begin();
        size_type i = 0;

        for (; i < this->size() && it != ilist.end(); ++i, ++it) {
            this->_start[i] = *it;
        }

        // 构造新元素（如果 ilist 比当前长）
        for (; it != ilist.end(); ++i; ++it) {
            this->_allocator.construct(this->_finish, *it);
        }

        // 销毁多余元素（如果当前比 ilist 长）
        for (; i  < this->size(); ++i) {
            this->_allocator.destroy(this->_start + i);
        }

        this->_finish = this->_start + n;
    }

    return *this;
}
```

#### assign

总体的逻辑是：

1. 如果 `count <= size()`：

   - 覆盖前 `count` 个元素。
   - 销毁多余的（`size() - count`）元素。
   - 不重新分配内存。

2. 如果 `count <= capacity()`：

   - 覆盖已有元素。
   - 构造新元素（从 `size()` 到 `count`）。
   - 不重新分配内存。

3. 如果 `count > capacity()`：

   - 析构所有元素。
   - 重新分配内存并填充构造。

##### `void assign( size_type count, const T& value )`

```c++
constexpr void assign (size_type count, const T& value)
{
    if (count > this->capacity()) {
        vector tmp(count, value, this->_allocator);
        this->swap(tmp);
    } else {
        size_type i = 0;

        for (; i < this->size() && i < count; ++i) {
            this->_start[i] = value;
        }

        for (; i < count; ++i) {
            this->_allocator.construct(this->_start + i, value);
            // 建议 finish 在这里更新，避免中间异常导致 finish 未定义
            ++this->_finish;
        }

        for (; i < this->size(); ++i) {
            this->_allocator.destroy(this->_start + i);
        }
    }
}
```

##### `constexpr void assign(InputIt first, InputIt last)`

```c++
template <typename InputIt>
requires std::input_iterator<InputIt> &&
            std::constructible_from<T, typename std::iterator_traits<InputIt>::reference> &&
            (!std::integral<InputIt>)
constexpr void assign(InputIt first, InputIt last)
{
    const size_type n = mystl::distance(first, last);

    if (n > this->capacity()) {
        vector tmp(first, last);
        this->swap(tmp);
    } else {
        size_type i = 0;

        for (; i < this->size() && first != last; ++i, ++first) {
            this->_start[i] = *first;
        }

        for (; first != last; ++first, ++i) {
            this->_allocator.construct(this->_start + i, *first);
            ++this->_finish;
        }

        for (; i < this->size(); ++i) {
            this->_allocator.destroy(this->_start + i);
        }
        this->_finish = this->_start + n;
    }
}
```

##### `constexpr void assign (std::initializer_list<value_type> ilist)`

```c++
constexpr void assign (std::initializer_list<value_type> ilist)
{
    const size_type n = ilist.size();

    if (n > this->capacity()) {
        vector tmp(ilist);
        this->swap(tmp);
    } else {
        auto it = ilist.begin();
        size_type i = 0;

        for (; i < this->size() && it != ilist.end(); ++i, ++it) {
            this->_start[i] = *it;
        }

        for (; it != ilist.end(); ++i, ++it) {
            this->_allocator.construct(this->_start + i, *it);
            ++this->_finish;
        }
        
        for (; i < this->size(); ++i) {
            this->_allocator.destroy(this->_start + i);
        }
        this->_finish = this->_start + n;
    }
}
```

#### 获取分配器

```c++
constexpr allocator_type get_allocator() const noexcept
{
    return this->_allocator;
}
```

### Element Access

#### `operator[]`

这个函数，标准库要求不检查是否越界，如果越界，则行为未定义

```c++
constexpr reference operator[](size_type pos)
{   
    // 标准库要求不检查范围
    return *(this->_start + pos);
}

constexpr const_reference operator[](size_type pos) const
{
    // 标准库要求不检查范围
    return *(this->_start + pos);
}
```

#### `at()`

`at()` 是 `operator[]` 的更安全版本，如果超出范围，会抛出错误

```c++
constexpr reference at(size_type pos) 
{
    if (pos >= this->size()) {
        throw std::out_of_range("mystl::vector::at: pos out of range");
    }
    return *(this->_start + pos);
}

constexpr const_reference at(size_type pos) const
{
    if (pos >= this->size()) {
        throw std::out_of_range("mystl::vector::at: pos out of range");
    }
    return *(this->_start + pos);
}
```

#### `data()`

```c++
constexpr pointer data() noexcept
{
    return this->_start;
}

constexpr const_pointer data() const noexcept
{
    return this->_start;
}
```

#### `front()`

```c++
constexpr reference front()
{
    return *this->_start;
}

constexpr const_reference front() const
{
    return *this->_start;
}
```

#### `back()`

```c++
constexpr reference back()
{
    return *(this->_finish - 1);
}

constexpr const_reference back() const
{
    return *(this->_finish - 1);
}
```

### 迭代器相关函数

| 用法        | 对象类型           | 返回类型         | 是否可修改元素 |
|-------------|--------------------|------------------|----------------|
| `begin()`   | 非 `const` 容器    | `iterator`       | ✅ 是           |
| `begin()`   | `const` 容器       | `const_iterator` | ❌ 否           |
| `cbegin()`  | 任意（包括非 `const`）| `const_iterator` | ❌ 否           |

```c++
constexpr iterator begin() noexcept
{
    return this->_start;
}

constexpr const_iterator begin() const noexcept
{
    return this->_start;
}

constexpr const_iterator cbegin() const noexcept
{
    return this->_start;
}

constexpr iterator end() noexcept
{
    return this->_finish;
}

constexpr const_iterator end() const noexcept
{
    return this->_finish;
}

constexpr const_iterator cend() const noexcept
{
    return this->_finish;
}

// 按照cppreference，reverse_iterator 实际上 持有的是指向其要访问的元素“下一位”的迭代器”
// rbegin() 返回最后一个元素，因此其应当指向 finish，这样解引用的时候，就可以顺利访问到最后一个元素
constexpr reverse_iterator rbegin() noexcept
{
    return reverse_iterator(this->_finish);
}

constexpr const_reverse_iterator rbegin() const noexcept
{
    return const_reverse_iterator(this->_finish);
} 

constexpr const_reverse_iterator crbegin() const noexcept
{
    return const_reverse_iterator(this->_finish);
}

// rend() 返回第一个元素的前一位，因此其应当指向 start，这样解引用的时候，就可以指向第一个元素的前一位
constexpr reverse_iterator rend() noexcept
{
    return reverse_iterator(this->_start);
}

constexpr const_reverse_iterator rend() const noexcept
{
    return const_reverse_iterator(this->_start);
}

constexpr const_reverse_iterator crend() const noexcept
{
    return const_reverse_iterator(this->_start);
}
```

C++ 编译器会根据对象的const性来决定调用哪个版本的 begin

- 当容器对象是 const 类型时，只能调用带有 const 限定符的成员函数；
- 而你的 begin() 函数如果没有 const 限定符，就不允许被 const 对象调用。
- cbegin() 通常是为了明确表示“我就是要一个 const_iterator，不管容器是不是 const”，它避免了隐式类型混淆，也增加了可读性。

```c++
vector<int> v;
auto it = v.begin();  // 调用的是 iterator begin()
const vector<int> cv;
auto it = cv.begin();  // 调用的是 const_iterator begin() const
```

#### 遇到的核心问题

```c++
const vector<int> vec = {5, 6, 7};

auto rit = vec.rbegin();
EXPECT_EQ(*rit, 7);
++rit;
EXPECT_EQ(*rit, 6);
++rit;
EXPECT_EQ(*rit, 5);
++rit;
EXPECT_EQ(rit, vec.rend());
```

对于一个 const vector 来说，使用 const_iterator 时，reverse_iterator 被实例化为： `reverse_iterator<int* const>`，导致以下编译错误：

```bash
error: decrement of read-only variable ‘tmp’
error: decrement of read-only member ‘current_’
```

本质原因是：

- int* const 是 不可修改的指针，即 const 修饰了指针本身，而不是指向的值；
- 这使得你无法执行 --current_、--tmp 等操作，因为这些操作会修改指针本身；
- 但 reverse_iterator 的核心功能就是维护一个可变的底层迭代器 current_。

解决办法是在 reverse_iterator 中移除对 const 的修饰

- 类型萃取的处理
  - `using iterator_type = std::remove_cv_t<std::remove_reference_t<Iter>>;`
  - 这一步确保了 `reverse_iterator<int* const>` 会变成 `reverse_iterator<int*>`，从而支持指针自增减。
- 修正成员类型 current_

```c++
using nonconst_iterator = std::remove_const_t<Iter>;
nonconst_iterator current_;
```

- 避免将 current_ 声明为 int* const；
- 这样在 operator++/-- 等操作中就不会报错。

### Element Access

```c++
// operator[]
constexpr reference operator[](size_type pos)
{   
    // 标准库要求不检查范围
    return *(this->_start + pos);
}

constexpr const_reference operator[](size_type pos) const
{
    // 标准库要求不检查范围
    return *(this->_start + pos);
}

// at()
constexpr reference at(size_type pos) 
{
    if (pos >= this->size()) {
        throw std::out_of_range("mystl::vector::at: pos out of range");
    }
    return *(this->_start + pos);
}

constexpr const_reference at(size_type pos) const
{
    if (pos >= this->size()) {
        throw std::out_of_range("mystl::vector::at: pos out of range");
    }
    return *(this->_start + pos);
}

// data()
constexpr pointer data() noexcept
{
    return this->_start;
}

constexpr const_pointer data() const noexcept
{
    return this->_start;
}

// front()
// For a container c, the expression c.front() is equivalent to *c.begin().
constexpr reference front()
{
    return *this->_start;
}

constexpr const_reference front() const
{
    return *this->_start;
}

// back()
// For a container c, the expression c.back() is equivalent to *(--c.end()).
constexpr reference back()
{
    return *(this->_finish - 1);
}

constexpr const_reference back() const
{
    return *(this->_finish - 1);
}
```

### Capacity

#### `reserve`

函数保证强异常安全，同时，按照 cppreference，在要求的空间大于理论可以分配的最大空间时，抛出 `std::length_error`

```c++
constexpr void reserve(size_type new_cap) 
{
    if (new_cap <= capacity()) return;

    if (new_cap > max_size()) {
        throw std::length_error("mystl::vector::reserve: new_cap > max_size");
    }

    pointer new_start = this->_allocator.allocate(new_cap);
    pointer new_finish = new_start;

    try {
        for (pointer p = this->_start; p != this->_finish; ++p, ++new_finish) {
            std::allocator_traits<allocator_type>::construct(
                this->_allocator, new_finish, std::move_if_noexcept(*p));
        }
    } catch (...) {
        for (pointer p = new_start; p != new_finish; ++p) {
            std::allocator_traits<allocator_type>::destroy(this->_allocator, p);
        }
        this->_allocator.deallocate(new_start, new_cap);
        throw;
    }

    // 销毁旧数据
    for (pointer p = this->_start; p != this->_finish; ++p) {
        std::allocator_traits<allocator_type>::destroy(this->_allocator, p);
    }

    this->_allocator.deallocate(this->_start, capacity());

    // 更新指针
    this->_start = new_start;
    this->_finish = new_finish;
    this->_end_of_storage = new_start + new_cap;
}
```

#### `shrink_to_fit`

将 vector 的容量（capacity()）缩减为当前实际元素个数（size()），以减少内存占用。

按照 cppreference：shrink_to_fit is a non-binding request to reduce capacity() to size().

- 标准库 不强制要求 shrink_to_fit() 必须释放内存，它只是一个“请求”，实现可以选择忽略这个请求
- 有些标准库不会实现

| 实现                | 实际是否收缩容量 | 备注                          |
|---------------------|------------------|-------------------------------|
| **libstdc++ (GCC)** | ✅ 收缩          | 调用 `swap` trick            |
| **libc++ (Clang)**  | ✅ 收缩          | 使用 `std::vector(tmp).swap(*this)` |
| **MSVC**            | ⚠️ 有时不收缩     | 基于 allocator 策略决定       |

另一方面，是否缩容还取决于底层 allocator 的实现

- 有些底层系统或自定义分配器可能不是“字节精确分配”，而是按页（如 4KB）来分配
  - 那么，如果一个 vector 当前容量是 4096，而实际只用了 4095 个元素，此时再去 shrink_to_fit() 是没有意义的。
  - 因为你哪怕少用了一个元素，新的容量也不能小于一个页面，还是得分配 同样的 4KB 内存，也就是说不会节省任何内存
- 另一种可能是，如果当前的 capacity 只比 size 大一点，那么这些内存的节省完全无法弥补性能和时间上的开销

```c++
constexpr void shrink_to_fit()
{
    if (capacity() > size()) {
        vector tmp(this->_start, this->_finish, this->_allocator);
        this->swap(tmp);
    }
}
```

### Modifiers

#### `clear`

清空 vector，size归零，但不会影响 capacity

```c++
constexpr void clear() noexcept
{
    for (pointer p = this->_start; p != this->_finish; ++p) {
        this->_allocator.destroy(p);
    }
    this->_finish = this->_start;
}
```

#### `erase`

The iterator `pos` must be valid and dereferenceable. Thus the `end()` iterator (which is valid, but is not dereferenceable) cannot be used as a value for `pos`.
The iterator `first` does not need to be dereferenceable if `first == last`: erasing an empty range is a no-op.
返回最后被移除的元素的后一个元素

- 如果 `pos` 是最后一个元素，返回 `end()`
- 如果 `last == end()`， 返回更新后的 `end()`
- 如果 `[first, last)` 为空，返回 `last`

满足异常安全：只要移动操作不抛异常，erase 就不会抛异常

```c++
constexpr iterator erase(const_iterstor pos)
{
    return erase(pos, pos + 1);
}

constexpr iterator erase(const_iterator first, const_iterator last)
{
    pointer non_const_first = const_cast<pointer>(first);
    pointer non_const_last = const_cast<pointer>(last);

    if (first == last) {
        return non_const_first;
    }

    pointer new_finish = this->_finish - (non_const_last - non_const_first);

    // move 后面元素到前面
    for (pointer p = non_const_last, d= non_const_first; p != this->_finish; ++p, ++d) {
        *d = std::move(*p);
    }

    // destroy 多出来的尾部元素
    for (pointer p = new_finish; p != this->_finish; ++p) {
        this->_allocator.destroy(p);
    }

    this->finish = new_finish;
    return non_const_first;
}
```

##### 接口参数的改变

早期 STL 中 erase 接受的是 iterator

- 原因很简单——这些接口在早期 STL 中的目标就是修改容器本身，既然容器会被修改，那么参数类型也该是非 const 的 iterator。

C++20 变成了 const_iterator

- C++20 为了统一容器接口行为，引入了 std::erase_if, std::ranges 等机制，并在多个标准容器中 让所有可“只读访问”的迭代器都支持用作参数传递。
- 这带来了几个好处：
  - 更宽松的参数类型支持：
    - `vec.erase(v.cbegin()); // OK, 不修改容器，旧接口不支持！`
  - 提高与 `std::ranges` 的一致性
    - 现代 `ranges`,`views` 的接口几乎全是基于 const_iterator 实现的，统一 const_iterator 是为了更好地支持

#### `emplace`

这个函数在 pos 前构造一个新的元素

```c++
template <typename... Args>
constexpr iterator emplace(const_iterator pos, Args... args)
{
    size_type idx = pos - this->_start;
    pointer insert_pos = this->_start + idx;

    // 空间足够
    if (this->_finish != this->_end_of_storage) {
        if (insert_pos == this->_finish) {
            this->emplace_back(std::forward<Args>(args)...);
        } 
        else {
            // 构造末尾元素备份，然后 move [insert_pos, finish) 到 [insert_pos + 1, finish + 1)
            this->_allocator.construct(this->_finish, std::move_if_noexcept(*(this->_finish - 1)));
            _move_range(this->_allocator, insert_pos, this->_finish - 1, insert_pos + 1);
            
            // 在 insert_pos 处构造新元素
            this->_allocator.destroy(insert_pos);
            this->_allocator.construct(insert_pos, std::forward<Args>(args)...);
        }

        ++this->_finish;
    }
    else {
        // 空间不足，重新分配
        const size_type oldSize = this->size();
        const newCapacity = oldSize == 0 ? 1 : oldSize * 2;

        pointer new_start = this->_allocator.allocate(newCapacity);
        pointer new_finish = new_start;

        try {
            _move_range(this->_allocator, this->_start, insert_pos, new_start);
            new_finish = new_start + idx;

            this->_allocator.construct(new_finish, std::forward<Args>(args)...);
            ++new_finish;

            _move_range(this->_allocator, insert_pos, this->_finish, new_finish);
            new_finish += this->_finish - insert_pos;
        } 
        catch (...) {
            for (pointer p = new_start; p != new_finish; ++p) {
                this->_allocator.destroy(p);
            }
            this->_allocator.deallocate(new_start, newCapacity);
            throw;
        }

        for (pointer p = this->_start; p != this->_finish; ++p) {
            this->_allocator.destroy(p);
        }
        this->_allocator.deallocate(this->_start, this->capacity());

        this->_start = new_start;
        this->_finish = new_finish;
        this->_end_of_storage = new_start + newCapacity;
    }

    return this->_start + idx;
}
```

#### `void emplace_back(Args&&... args)`

这个函数用于向 vector 中添加元素，并在容量不足时触发扩容

```c++
template <typename... Args>
void emplace_back(Args&&... args)
{
  if (this->_finish != this->_end_of_storage) {
      this->_allocator.construct(this->_finish, std::forward<Args>(args)...);
      ++this->_finish;
  }
  else {
      _realloc_insert(std::forward<Args>(args)...);
  }
}
```

#### insert

```c++
//  Inserts a copy of value before pos.
//  Returns an iterator pointing to the inserted value.
constexpr iterator insert(const_iterator pos, const T& value)
{
    return emplace(pos, value);
}

constexpr iterator insert(const_iterator pos, T&& value)
{
    return emplace(pos, std::move(value));
}

constexpr iterator insert(const_iterator pos, size_type count, const T& value)
{
    if (count == 0) {
        return const_cast<iterator>(pos);
    }

    size_type idx = pos - this->_start;
    pointer insert_pos = this->_start + idx;

    // 空间足够
    if (this->capacity() >= this->size() + count) {
        pointer old_finish = this->_finish;
        pointer move_src = insert_pos;
        pointer move_dst = insert_pos + count;

        // move [insert_pos, finish) 到 [insert_pos + count, finish + count)
        _move_range(this->_allocator, move_src, old_finish, move_dst);

        // 在 insert_pos 处构造新元素
        _uninitialized_fill_n(this->_allocator, insert_pos, count, value);

        this->_finish += count;
    }
    // 空间不足
    else {
        const size_type oldSize = this->size();
        const size_type newCapacity = oldSize + std::max(count, oldSize);
        pointer new_start = this->_allocator.allocate(newCapacity);
        pointer new_finish = new_start;

        try {
            // 构造前段 [0, idx)
            for (size_type i = 0; i < idx; ++i, ++new_finish) {
                this->_allocator.construct(new_finish, std::move_if_noexcept(this->_start[i]));
            }

            _uninitialized_fill_n(this->_allocator, new_finish, count, value);
            new_finish += count;

            // 构造后段 [idx, oldSize)
            for (size_type i = idx; i < oldSize; ++i, ++new_finish) {
                this->_allocator.construct(new_finish, std::move_if_noexcept(this->_start[i]));
            }
        }
        catch(...) {
            for (pointer p = new_start; p != new_finish; ++p) {
                this->_allocator.destroy(p);
            }
            this->_allocator.deallocate(new_start, newCapacity);
            throw;
        }

        for (pointer p = this->_start; p != this->_finish; ++p) {
            this->_allocator.destroy(p);
        }
        this->_allocator.deallocate(this->_start, this->capacity());

        this->_start = new_start;
        this->_finish = new_finish;
        this->_end_of_storage = new_start + newCapacity;
    }

    return this->_start + idx;
}

// 不保证异常安全
template<typename InputIt>
requires std::input_iterator<InputIt> &&
            std::constructible_from<T, typename std::iterator_traits<InputIt>::reference> &&
            (!std::integral<InputIt>)
constexpr iterator insert(const_iterator pos, InputIt first, InputIt last)
{
    size_type insertCount = static_cast<size_type>(std::distance(first, last));
    if (insertCount == 0) {
        return const_cast<iterator>(pos);
    }

    size_type insertOffset = static_cast<size_type>(pos - this->_start);
    pointer insertPos = this->_start + insertOffset;

    if (static_cast<size_type>(this->_end_of_storage - this->_finish) >= insertCount) {
        // 后段先移动到新位置（后向构造）
        const size_type tailCount = static_cast<size_type>(this->_finish - insertPos);
        if (tailCount > 0) {
            _move_range(this->_allocator, insertPos, this->_finish, insertPos + insertCount);
        }

        // 中间插入新元素
        pointer cur = insertPos;
        try {
            for (; first != last; ++first, ++cur) {
                std::allocator_traits<allocator_type>::construct(
                    this->_allocator, cur, std::move_if_noexcept(*first)
                );
            }
        } catch(...) {
            for (pointer p = insertPos; p != cur; ++p) {
                std::allocator_traits<allocator_type>::destroy(this->_allocator, p);
            }
            throw;
        }

        this->_finish += insertCount;
    }
    else {
        // 容量不足，分配新内存
        const size_type oldSize = this->size();
        const size_type newCapacity = std::max(oldSize + insertCount, oldSize * 2);

        pointer new_start = std::allocator_traits<allocator_type>::allocate(this->_allocator, newCapacity);
        pointer new_finish = new_start;

        try {
            // 1. 复制/move 插入点前段 [0, insertOffset)
            for (size_type i = 0; i < insertOffset; ++i, ++new_finish)
                std::allocator_traits<allocator_type>::construct(this->_allocator, new_finish, std::move_if_noexcept(this->_start[i]));

            // 2. 插入新元素
            for (; first != last; ++first, ++new_finish)
                std::allocator_traits<allocator_type>::construct(this->_allocator, new_finish, *first);

            // 3. 复制/move 尾段 [insertOffset, oldSize)
            for (size_type i = insertOffset; i < oldSize; ++i, ++new_finish)
                std::allocator_traits<allocator_type>::construct(this->_allocator, new_finish, std::move_if_noexcept(this->_start[i]));
        } catch (...) {
            for (pointer p = new_start; p != new_finish; ++p)
                std::allocator_traits<allocator_type>::destroy(this->_allocator, p);
            this->_allocator.deallocate(new_start, newCapacity);
            throw;
        }
        // 清理旧资源
        for (pointer p = this->_start; p != this->_finish; ++p)
            std::allocator_traits<allocator_type>::destroy(this->_allocator, p);
        this->_allocator.deallocate(this->_start, this->capacity());

        this->_start = new_start;
        this->_finish = new_finish;
        this->_end_of_storage = new_start + newCapacity;
        insertPos = this->_start + insertOffset;  // 更新 insertPos 指针
    }

    return insertPos;
}

// 直接转发即可
constexpr iterator insert(const_iterator pos, std::initializer_list<T> ilist)
{
    return insert(pos, ilist.begin(), ilist.end());
}
```

需要注意一点是，按照标准库，`insert`　不保证异常安全，即在移动元素后，如果在新位置构造元素的过程抛出异常，函数结果是未定义的

#### push_back

相较于 emplace_back，其本质操作是复制（或移动）一个元素到容器尾部，所以总是需要一次拷贝或移动构造。

在实现上，直接转发到 emplace_back 即可

```c++
constexpr void push_back(const T& value)
{
    emplace_back(value);
}

constexpr void push_back(T&& value)
{
    emplace_back(std::move(value));
}
```

#### pop_back

从末尾弹出一个元素，如果弹出前 vector 为空，那么结果未定义

- 按照 cppreference 和 标准库的做法，不会对 vector 是否为空做检查
- 这么做是出于性能的考虑

```c++
constexpr void pop_back()
{
    --this->_finish;
    this->_allocator.destroy(this->_finish);
}
```

#### resize

```c++
// resize
/*
Resizes the container to contain count elements:
    If count is equal to the current size, does nothing.
    If the current size is greater than count, the container is reduced to its first count elements.
    If the current size is less than count, then:
        1) Additional default-inserted elements(since C++11) are appended.
        2) Additional copies of value are appended.
*/
constexpr void resize(size_type conut)
{
    if (count < this->size()) {
        pointer new_finish = this->_start + count;
        for (pointer p = new_finish; p != this->_finish; ++p) {
            this->_allocator.destroy(p);
        }
        this->_finish = new_finish;
    }
    else if (count > this->size()) {
        if (count > this->capacity()) {
            reserve(count);
        }
        for (pointer p = this->_finish; p != this->_start + count; ++p) {
            // if T is not DefaultInsertable or MoveInsertable into vector,the behavior is undefined
            this->_allocator.construct(p, T{});
        }
        this->_finish = this->_start + count;
    }
}

constexpr void resize(size_type count, const value_type& value)
{
    if (count < this->size()) {
        pointer new_finish = this->_start + count;
        for (pointer p = new_finish; p != this->_finishl ++p) {
            this->_allocator.destroy(p);
        }
        this->_finish = new_finish;
    }
    else if (count > this->size()) {
        if (count > this->capacity()) {
            reserve(count);
        }
        // if T is not CopyInsertable into vector,the behavior is undefined
        for (pointer p = this->_finish; p != this->_start + count; ++p) {
            this->_allocator.construct(p, value);
        }
        this->_finish = this->_start + count;
    }
}
```

# deque

## 底层设计

### 1. 分块存储

不像 `vector` 是一整块连续内存，`deque` 将元素分成若干块（block 或 buffer），每块大小相同。

块大小通常固定为一个“缓冲区大小” `buffer_size`（取决于 `sizeof(T)`，例如 `max(1, 512/sizeof(T))`），以保证每块占用大约 512 字节或至少能存 1 个对象。

### 2. 中心映射表

`deque` 维护一个指向“块指针”的数组，称为 **map**。

例如 map 是 `T** map_`，其中每个 `map_[i]` 指向一个已分配的块。

这样，所有元素并不连续，而是“块A、块B、块C...”按顺序拼起来。

### 3. 双端生长

`deque` 要支持 `push_front` 和 `push_back`：当头部或尾部块用尽，就在 `map_` 前端或末端再分配一个新块。

如果 `map_` 本身的两端指针用尽，还要重新分配更大的 `map_`（通常扩大一倍），并把旧的块指针拷贝到新的 `map_` 中部位置。

#### 4. 迭代器设计

迭代器通常存储：

- `pointer cur`：当前块内的元素位置
- `pointer first, last`：当前块的开始指针/结束指针的下一个位置
- `T** node`：指向 `map_` 中当前块指针的地址

这样依然可以 O(1) 随机访问（通过两级索引：计算目标块号和块内偏移）。

`deque` 的迭代器要重新设计，不能和 `vector` 一样使用原始指针，因为 `deque` 涉及到在不连续的块之间的跳跃

## iterator

```c++
class iterator {
public: 
    using iterator_category = std::random_access_iterator_tag;
    using value_type = T;
    using difference_type = std::ptrdiff_t;
    using pointer = T*;
    using reference = T&;

public:
    iterator() noexcept
        : _cur(nullptr), _first(nullptr), _last(nullptr), _node(nullptr) {}
    iterator(pointer cur, pointer first, pointer last, pointer* node) noexcept
        : _cur(cur), _first(first), _last(last), _node(node) {}
    
    reference operator*() const noexcept { return *_cur; }
    pointer operator->() const noexcept { return _cur; }

    // 前缀 ++
    iterator& operator++() noexcept {
        if (++_cur == _last) {
            ++_node;
            _first = *_node;
            _last = _first + static_cast<difference_type>(_buffer_size());
            _cur = _first;
        }
        return *this;
    }
    
    // 后缀 ++
    iterator& operator++(int) noexcept {
        iterator tmp = *this;
        ++(*this);
        return tmp;
    }

    // 前缀 --
    iterator& operator--() noexcept {
        if (_cur == _first) {
            --node;
            _first = *_node;
            _last = _first + static_cast<difference_type>(_buffer_size());
            _cur = _last;
        }
        --_cur;
        return *this;
    }

    // 后缀 --
    iterator& operator--(int) noexcept {
        iterator tmp = *this;
        --(*this);
        return tmp;
    }

    // += n
    iterator& operator+=(difference_type n) {
        const difference_type buf = static_cast<difference_type>(_buffer_size());
        // 1. 计算总偏移
        difference_type offset = (_cur - _first) + n;
        // 2. 块数／块内偏移，使用 C++ 除法可能向零舍入
        difference_type block = offset / buf;
        difference_type idx   = offset % buf;
        // 3. 如果 idx 负了，就借一块
        if (idx < 0) {
            idx   += buf;
            --block;
        }
        // 4. 更新 node、first、last、cur
        _node += block;
        _first = *_node;
        _last  = _first + buf;
        _cur   = _first + idx;
        return *this;
    }        

    // + n
    iterator& operator+(difference_type n) const {
        iterator tmp = *this;
        return tmp += n;
    }

    // -= n
    iterator& operator-=(difference_type n) {
        return *this += -n;
    }
    
    // - n
    iterator& operator-(difference_type n) const {
        iterator tmp = *this;
        return tmp -= n;
    }

    // - iterator
    difference_type operator-(const iterator& other) const noexcept {
        difference_type block_diff = _node - other._node;
        difference_type cur_diff = (_cur - _first) - (other._cur - other._first);
        return block_diff * static_cast<difference_type>(_buffer_size()) + cur_diff;
    }

    // [] 运算符
    reference operator[](difference_type n) const noexcept {
        return *(*this + n);
    }

    // 比较运算符
    bool operator==(const iterator& other) const noexcept {
        return _cur == other._cur;
    }
    bool operator!=(const iterator& other) const noexcept {
        return _cur != other._cur;
    }
    bool operator<(const iterator& other) const noexcept {
        return (_node == other._node)
                ? (_cur < other._cur)
                : (_node < other._node);
    }
    bool operator>(const iterator& other) const noexcept {
        return other < *this;
    }
    bool operator<=(const iterator& other) const noexcept {
        return !(*this > other);
    }
    bool operator>=(const iterator& other) const noexcept {
        return !(*this < other);
    }

private:
    pointer _cur; // 当前元素位置
    pointer _first; // 当前block的第一个元素
    pointer _last; // 当前block的最后一个元素的下一个位置
    pointer* _node; // 指向_map 上对应块指针的地址

    static size_type _buffer_size() {
        return deque::buffer_size();
    }
};

class const_iterator {
public:
    using iterator_category = std::random_access_iterator_tag;
    using value_type = T;
    using difference_type = std::ptrdiff_t;
    using pointer = const T*;
    using reference = const T&;

public:
    const_iterator() noexcept
        : _cur(nullptr), _first(nullptr), _last(nullptr), _node(nullptr) {}
    const_iterator(pointer cur, pointer first, pointer last, pointer* node) noexcept
        : _cur(cur), _first(first), _last(last), _node(node) {}
    // 支持从 iterator 隐式转换
    const_iterator(const iterator& it) noexcept 
        : _cur(it._cur), _first(it._first), _last(it._last), _node(it._node) {}

    reference operator*() const noexcept { return *_cur; }
    pointer operator->() const noexcept { return _cur; }

    // 前缀++
    const_iterator& operator++() noexcept {
        if (++_cur == _last) {
            ++_node;
            _first = *_node;
            _last = _first + _buffer_size();
            _cur = _first;
        }
        return *this;
    }

    // 后缀++
    const_iterator& operator++(int) noexcept {
        const_iterator tmp = *this;
        ++*this;
        return tmp;
    }

    // 前缀--
    const_iterator& operator--() noexcept {
        if (_cur == _first) {
            --_node;
            _first = *_node;
            _last = _first + _buffer_size();
            _cur = _last;
        }
        --_cur;
        return *this;
    }

    // 后缀--
    const_iterator operator--(int) noexcept {
        const_iterator tmp = *this;
        --*this;
        return tmp;
    }

    // += n
    const_iterator& operator+=(difference_type n) {
        const difference_type buf = static_cast<difference_type>(_buffer_size());
        difference_type offset = _cur - _first + n;
        difference_type block = offset / buf;
        difference_type idx = offset % buf;

        if (idx < 0) {
            idx += buf;
            --block;
        }

        _node += block;
        _first = *_node;
        _last = _first + buf;
        _cur = _first + idx;

        return *this;
    }

    // + n
    const_iterator operator+ (difference_type n) const {
        const_iterator tmp = *this;
        return tmp += n;
    }

    // -= n
    const_iterator& operator-=(difference_type n) {
        return *this += -n;
    }

    // - n
    const_iterator operator- (difference_type n) const {
        const_iterator tmp = *this;
        return tmp -= n;
    }

    // distance
    difference_type operator-(const const_iterator& o) const noexcept {
        difference_type buf = difference_type(buffer_size());
        difference_type block_diff = _node - o._node;
        difference_type cur_diff   = (_cur - _first) - (o._cur - o._first);
        return block_diff * buf + cur_diff;
    }

    // 下标
    reference operator[](difference_type n) const {
        return *(*this + n);
    }

    // 关系比较
    bool operator==(const const_iterator& o) const noexcept { return _cur == o._cur; }
    bool operator!=(const const_iterator& o) const noexcept { return _cur != o._cur; }
    bool operator< (const const_iterator& o) const noexcept {
        return (_node == o._node)
            ? (_cur < o._cur)
            : (_node < o._node);
    }
    bool operator> (const const_iterator& o) const noexcept { return o < *this; }
    bool operator<=(const const_iterator& o) const noexcept { return !(*this > o); }
    bool operator>=(const const_iterator& o) const noexcept { return !(*this < o); }

private:
    pointer _cur;
    pointer _first;
    pointer _last;
    pointer* _node;

    static size_type _buffer_size() {
        return deque::buffer_size();
    }
}
```

## Constructor

```c++
 // 默认构造，委托给带 allocator 的版本
    deque() noexcept(noexcept(allocator_type()))
        : deque(allocator_type()) {}
    
    explicit deque(const allocator_type& alloc)
        : _map(nullptr), _map_size(0), _start(), _finish(), _alloc(alloc), _map_alloc(_alloc)
    {   
        // 得到一个空容器，但内部已经有一个block
        _initialize_map(0);
    }

    /*
        Constructs a deque with count default-inserted objects of T. No copies are made.
        If T is not DefaultInsertable into std::deque<T>, the behavior is undefined.
    */
    explicit deque(size_type count, const allocator_type& alloc = allocator_type())
        : _map(nullptr), _map_size(0), _start(), _finish(), _alloc(alloc), _map_alloc(_alloc)
    {
        // 1) 计算需要多少个 block
        const size_type buf_sz = buffer_size();
        const size_type nblocks = count ? (count + buf_sz - 1) / buf_sz : 0;

        // 2) 直接为 nblocks 分配 map_ 和 buffers
        _initialize_map(nblocks);

        // 3) 默认构造 count 个元素
        iterator it = _start;
        for (size_type i = 0; i < count; ++i, ++it) {
            if constexpr (std::is_trivially_default_constructible<T>::value) {
                // trivial 类型：必须手动 value-initialize，否则留下随机值
                *it = T();  // 置 0
            } else {
                std::allocator_traits<allocator_type>::construct(_alloc, std::addressof(*it));
            }
        }

        // 4) 设置 finish_ 到正确位置
        _finish = it;
    }

    /*
        Constructs a deque with count copies of elements with value value.
        If T is not CopyInsertable into std::deque<T>, the behavior is undefined.
    */
    deque(size_type count, const T& value, const allocator_type& alloc = allocator_type())
        : _map(nullptr), _map_size(0), _start(), _finish(), _alloc(alloc), _map_alloc(_alloc)
    {
        const size_type buf_sz = buffer_size();
        const size_type nblocks = count ? (count + buf_sz - 1) / buf_sz : 0;

        _initialize_map(nblocks);

        iterator it = _start;
        for (size_type i = 0; i < count; ++i, ++it) {
            if constexpr (std::is_trivially_copy_constructible<T>::value) {
                // trivial 优化：直接赋值
                *it = value;
            } else {
                std::allocator_traits<allocator_type>::construct(_alloc, std::addressof(*it), value);
            }
        }

        _finish = it;
    }

    /*
        Constructs a deque with the contents of the range [first, last). Each iterator in [first, last) is dereferenced exactly once.
        If InputIt does not satisfy the requirements of LegacyInputIterator, overload (4) is called instead with arguments static_cast<size_type>(first), last and alloc.
        This overload participates in overload resolution only if InputIt satisfies the requirements of LegacyInputIterator.
        If T is not EmplaceConstructible into std::deque<T> from *first, the behavior is undefined.
    */
    template <typename InputIt>
    requires std::input_iterator<InputIt> && std::constructible_from<T, typename std::iterator_traits<InputIt>::reference> && (!std::integral<InputIt>)
    deque(InputIt first, InputIt last, const allocator_type& alloc = allocator_type())
        : _map(nullptr), _map_size(0), _start(), _finish(), _alloc(alloc)
    {
        // 先分配一个空 block，保证 emplace_back 可以工作
        _initialize_map(0);
        
        // 逐元素插入
        for(; first != last; ++first) {
            emplace_back(*first);
        }
    }
    
    // copy constructor
    deque(const deque& other)
        : _map(nullptr), _map_size(0), _start(), _finish(), _alloc(std::allocator_traits<allocator_type>::select_on_container_copy_construction(other._alloc)), _map_alloc(_alloc)
    {
        // 1) 计算要分配多少 block
        const size_type n = static_cast<size_type>(other._finish - other._start);
        const size_type buf_sz = buffer_size();
        const size_type nblocks = n ? (n + buf_sz - 1) / buf_sz : 0;

        // 2) 分配 _map 和 block buffers
        _initialize_map(nblocks);

        // 3) 按元素 copy‐construct 到新内存
        iterator it = _start;
        for (const_reference val : other) {
            if constexpr (std::is_trivially_copy_constructible<T>::value) {
                // trivial 优化：直接赋值
                *it = val;
            } else {
                std::allocator_traits<allocator_type>::construct(_alloc, std::addressof(*it), val);
            }
            ++it;
        }

        // 4) 将 finish_ 移到 start_ + n
        _finish = _start;
        _finish += static_cast<difference_type>(n);
    }

    // Move constructor
    deque(deque&& other)
        : _map(nullptr), _map_size(0), _start(), _finish(), _alloc(std::move(other._alloc)), _map_alloc(_alloc)
    {
        // 当 allocator 可传播或总是相等时，可以偷取 other 的内存
        constexpr bool propagate = std::allocator_traits<allocator_type>::propagate_on_container_move_assignment::value;
        constexpr bool always_eq = std::allocator_traits<allocator_type>::is_always_equal::value;
        if (propagate || always_eq || other._alloc == _alloc) {
            // —— 直接“窃取” other 的底层存储 —— //
            _map = other._map;
            _map_size = other._map_size;
            _start = other._start;
            _finish = other._finish;
            // 将 other 重置为空状态，确保析构后不会释放刚才窃取的内存
            other._map = nullptr;
            other._map_size = 0;
            other._start = iterator();
            other._finish = iterator();
        }
        else {
            // —— 分配新存储并逐元素移动 —— //
            // 1) 计算元素总数和需要的 block 数
            size_type n = static_cast<size_type>(other._finish - other._start);
            size_type buf_sz = buffer_size();
            size_type nblocks = n ? (n + buf_sz - 1) / buf_sz : 0;

            // 2) 分配 map_ 和各个 block buffers
            _initialize_map(nblocks);

            // 3) 在新容器中按顺序 move-construct 每个元素
            iterator dst = _start;
            for (iterator src = other._start; src != other._finish; ++src, ++dst) {
                if constexpr (std::is_trivially_move_constructible<T>::value) {
                    // trivial 优化：直接赋值
                    *dst = std::move(*src);
                } else {
                    std::allocator_traits<allocator_type>::construct(_alloc, std::addressof(*dst), std::move(*src));
                }
            }

            // 4) 设置 _finish 到正确位置
            _finish = _start;
            _finish += static_cast<difference_type>(n);

            // 5) 销毁 other 中的旧元素并释放它的所有存储
            // 非平凡时才需要逐元素析构
            if constexpr (!std::is_trivially_destructible<T>::value) {
                for (iterator it = other._start; it != other._finish; ++it) {
                    std::allocator_traits<allocator_type>::destroy(other._alloc, std::addressof(*it));
                }
            }
            other._deallocate_all_blocks();

            // 6) 将 other 重置为空状态
            other._map = nullptr;
            other._map_size = 0;
            other._start = iterator();
            other._finish = iterator();
        }
    }

    // 拷贝构造＋指定分配器
    deque(const deque& other, const allocator_type& alloc)
        : _map(nullptr), _map_size(0), _start(), _finish(), _alloc(alloc), _map_alloc(_alloc)
    {
        size_type n = static_cast<size_type>(other._finish - other._start);
        size_type buf_sz = buffer_size();
        size_type nblocks = n ? (n + buf_sz - 1) / buf_sz : 0;

        _initialize_map(nblocks);

        iterator dst = _start;
        for (const_reference v : other) {
            if constexpr (std::is_trivially_copy_constructible<T>::value) {
                // trivial 优化：直接赋值
                *dst = v;
            } else {
                std::allocator_traits<allocator_type>::construct(_alloc, std::addressof(*dst), v);
            }
            ++dst;
        }

        _finish = _start;
        _finish += static_cast<difference_type>(n);
    }

    // move + alloc
    deque(deque&& other, const allocator_type& alloc) 
        : _map(nullptr), _map_size(0), _start(), _finish(), _alloc(alloc), _map_alloc(_alloc)
    {
        if (_alloc == other._alloc) {
            _map = other._map;
            _map_size = other._map_size;
            _start = other._start;
            _finish = other._finish;
            other._map = nullptr;
            other._map_size = 0;
            other._start = iterator();
            other._finish = iterator();
        }
        else {
            size_type n = static_cast<size_type>(other._finish - other._start);
            size_type buf_sz = buffer_size();
            size_type nblocks = n ? (n + buf_sz - 1) / buf_sz : 0;

            _initialize_map(nblocks);

            iterator dst = _start;
            for (iterator src = other._start; src != other._finish; ++src, ++dst) {
                if constexpr (std::is_trivially_move_constructible<T>::value) {
                    // trivial 优化：直接赋值
                    *dst = std::move(*src);
                } else {
                    std::allocator_traits<allocator_type>::construct(_alloc, std::addressof(*dst), std::move(*src));
                }
            }

            _finish = _start;
            _finish += static_cast<difference_type>(n);

            for (iterator it = other._start; it != other._finish; ++it) {
                std::allocator_traits<allocator_type>::destroy(other._alloc, std::addressof(*it));
            }
            other._deallocate_all_blocks();

            other._map = nullptr;
            other._map_size = 0;
            other._start = iterator();
            other._finish = iterator();
        }
    }

    // initializer_list
    deque(std::initializer_list<T> init, const allocator_type& alloc = allocator_type())
        : _map(nullptr), _map_size(0), _start(), _finish(), _alloc(alloc), _map_alloc(_alloc)
    {
        size_type n = init.size();
        size_type buf_sz = buffer_size();
        size_type nblocks = n ? (n + buf_sz - 1) / buf_sz : 0;

        _initialize_map(nblocks);

        iterator it = _start;
        for (const_reference v : init) {
            if constexpr (std::is_trivially_copy_constructible<T>::value) {
                // trivial 优化：直接赋值
                *it = v;
            } else {
                std::allocator_traits<allocator_type>::construct(_alloc, std::addressof(*it), v);
            }
            ++it;
        }

        _finish = it;
    }


    // destructor
    ~deque() 
    {
        // 只在非平凡析构时才逐元素销毁
        if constexpr (!std::is_trivially_destructible<T>::value) {
            for (auto it = _start; it != _finish; ++it) {
                std::allocator_traits<allocator_type>::destroy(_alloc, std::addressof(*it));
            }
        }

        _deallocate_all_blocks();
    }
```

### 赋值函数 `operator=`

需要注意，在标准库的做法中，`operator=` 的赋值，不会尝试复用已有元素，而是会清空旧元素，然后重新构造

#### 拷贝赋值

如果分配器的 trait `propagate_on_container_copy_assignment` 为 `true`，那么当前对象的分配器（allocator）会被 `other` 的分配器拷贝替换。

如果分配器发生了变化（即赋值后分配器和原来不同），**必须用旧分配器释放原有内存，然后用新分配器分配新内存，再拷贝元素**。
- 这是因为分配器有可能是有状态的（比如自定义内存池），用错分配器释放会出错。
- 这也是为什么标准库实现通常会先保存旧分配器，释放旧内存，再切换分配器。

```c++
deque& operator=(const deque& other)
{
    if (this == &other) {
        return *this;
    }

    // 1. 保存旧分配器的引用，用于后续可能的内存释放
    allocator_type old_alloc = _alloc;
    map_allocator_type old_map_alloc = _map_alloc;

    // 2. 检查分配器传播
    bool alloc_changed = false;
    if constexpr (std::allocator_traits<allocator_type>::propagete_on_container_copy_assignment::value) {
        if (_alloc != other._alloc) {
            alloc_changed = true;
        }
        _alloc = other._alloc;
        _map_alloc = other._map_alloc;
    }

    // 3. 如果分配器发生变化，必须用旧分配器释放内存
    if (alloc_changed) {
        // 用旧分配器释放资源
        if constexpr (!std::is_trivially_destructible<T>::value) {
            for (auto it = _start; it != _finish; ++it) {
                std::allocator_traits<allocator_type>::destroy(old_alloc, std::addressof(*it));
            }
        }

        // 用旧分配器释放 blocks
        if (_map) {
            size_type blocks = mystl::distance(_start._node, _finish._node) + 1;
            pointer* node = _start._node;
            for (size_type i = 0; i < blocks; ++i, ++node) {
                std::allocator_traits<allocator_type>::deallocate(old_alloc, *node, buffer_size());
            }
            std::allocator_traits<map_allocator_type>::deallocate(old_map_alloc, _map, _map_size);
        }

        // 重置状态
        _map = nullptr;
        _map_size = 0;
        _start = iterator();
        _finish = iterator();

        // 4. 用新分配器分配资源并复制元素
        size_type n = mystl::distance(other._start, other._finish);
        size_type buf_sz = buffer_size();
        size_type nblocks = n ? (n + buf_sz - 1) / buf_sz : 0;
        _initialize_map(nblokcs);

        iterator it = _start;
        for (const_reference v : other) {
            if constexpr (std::is_trivially_copy_constructible<T>::value) {
                *it = v;
            }
            else {
                std::allocator_traits<allocator_type>::construct(_alloc, std::addressof(*it), v);
            }
            ++it;
        }

        _finish = it;
        _finish += static_cast<difference_type>(n);
    }
    else {
        // 5. 如果分配器没变，可以简单地释放旧资源并复制
        if constexpr (!std::is_trivially_copy_constructible<T>::value) {
            for (auto it = _start; it != _finish; ++it) {
                std::allocator_traits<allocator_type>::destroy(old_alloc, std::addressof(*it));
            }
        }

        _deallocate_all_blocks();

        size_type n = mystl::distance(other._start, other._finish);
        size_type buf_sz = buffer_size();
        size_type nblocks = n ? (n + buf_sz - 1) / buf_sz : 0;
        _initialize_map(nblocks);

        iterator it = _start;
        for (const_reference v : other) {
            if constexpr (std::is_trivially_copy_constructible<T>::value) {
                *it = v;
            }
            else {
                std::allocator_traits<allocator_type>::construct(_alloc, std::addressof(*it), v);
            }
            ++it;
        }

        _finish = it;
        _finish += static_cast<difference_type>(n);
    }
    return *this;
}
```
#### 移动赋值

移动赋值运算符使用移动语义将 `other` 的内容替换到当前对象中。`other` 在操作后处于**有效但未指定的状态**。   

如果分配器的 trait `propagate_on_container_move_assignment` 为 `true`，则当前对象的分配器会被 `other` 的分配器替换。

如果 `propagate_on_container_move_assignment` 为 `false`，且当前对象和 `other` 的分配器不相等，则不能直接“窃取” `other` 的内存。必须逐个元素进行移动赋值，并在需要时用自己的分配器分配额外内存。

```c++
deque& operator=(deque&& other) noexcept(
    std::allocator_traits<allocator_type>::is_always_equal::value
) 
{
    if (this == &other) {
        return *this;
    }

    // 1. 检查分配器传播
    if constexpr (std::allocator_traits<allocator_type>::propagate_on_container_move_assignment::value) {
        _alloc = std::move(other._alloc);
        _map_alloc = std::move(other._map_alloc)
    }

    // 2. 如果分配器允许，直接“窃取”other的资源
    constexpr bool propagate = std::allocator_traits<allocator_type>::propagate_on_container_move_assignment::value;
    constexpr bool always_equal = std::allocator_traits<allocator_type>::is_always_equal::value;
    if (propagate || always_equal || _alloc == other._alloc) {
        // 释放当前对象的旧资源
        if constexpr (!std::is_trivially_destructible<T>::value) {
            for (auto it = _start; it != _finish; ++it) {
                std::allocator_traits<allocator_type>::destroy(_alloc, std::addressof(*it));
            }
        }
        _deallocate_all_blocks();

        // 获取 other 的资源
        _map = other._map;
        _map_size = other._map_size;
        _start = other._start;
        _finish = other._finish;

        // 将 other 置空
        other._map = nullptr;
        other._map_size = 0;
        other._start = iterator();
        other._finish = iterator();
    } else {
        // 3. 否则，逐元素移动赋值
        if constexpr (!std::is_trivially_move_constructible<T>::value) {
            for (auto it = _start; it != _finish; ++it) {
                std::allocator_traits<allocator_type>::destroy(_alloc, std::addressof(*it));
            }
        }

        _deallocate_all_blocks();

        size_type n = mystl::distance(other._start, other._finish);
        size_type buf_sz = buffer_size();
        size_type nblocks = n ? (n + buf_sz - 1) / buf_sz : 0;
        _initialize_map(nblocks);

        iterator it = _start;
        for (iterator src = other._start; src != other._finish; ++src, ++it) {
            if constexpr (std::is_trivially_move_constructible<T>::value) {
                *it = std::move(*src);
            } else {
                std::allocator_traits<allocator_type>::construct(_alloc, std::addressof(*it), std::move(*src));
            }
        }

        _finish = it;

        // 销毁 other 的元素并释放资源
        for (iterator it = other._start; it != other._finish; ++it) {
            std::allocator_traits<allocator_type>::destroy(other._alloc, std::addressof(*it));
        }
        other._deallocate_all_blocks();

        other._map = nullptr;
        other._map_size = 0;
        other._start = iterator();
        other._finish = iterator();
    }
    return *this;
}   
```

#### 初始化列表

```c++
deque& operator=(std::initializer_list<value_type> ilist) 
{
    // 1. free current elements
    if constexpr (!std::is_trivially_destructible<T>::value) {
        for (auto it = _start; it != _finish; ++it) {
            std::allocator_traits<allocator_type>::destroy(_alloc, std::addressof(*it));
        }
    }
    _dealloc_all_blocks();

    // 2. assign new resources
    size_type n = ilist.size();
    size_type buf_sz = buffer_size();
    size_type nblocks = n ? (n + buf_sz - 1) / buf_sz : 0;
    _initialize_map(nblocks);

    // 3. copy elements
    auto it = _start;
    for (const_reference v : ilist) {
        if constexpr (std::is_trivially_copy_assignable<T>::value) {
            *it = v;
        } else {
            std::allocator_traits<allocator_type>::construct(_alloc, std::addressof(*it), v);
        }
        ++it;
    }
    _finish = it;

    return *this;
}
```

### `assign`

在标准库做法中，assign 要尽可能复用原有的空间

```c++
// assign
void assign(size_type count, const T& value)
{
    size_type n = _finish - _start;
    // if c > count, there is enough elements
    if (n >= count) {
        // reuse the first count elements
        auto it = _start;
        for (size_type i = 0; i < count; ++i, ++it) {
            if constexpr (std::is_trivially_copy_assignable<T>::value) {
                *it = value;
            } else {
                std::allocator_traits<allocator_type>::construct(_alloc, std::addressof(*it), value);
            }
        }

        // destroy the rest elements
        auto new_finish = _start + static_cast<difference_type>(count);
        if constexpr (!std::is_trivially_destructible<T>::value) {
            for (auto cur = new_finish; cur != _finish; ++cur) {
                std::allocator_traits<allocator_type>::destroy(_alloc, std::addressof(*cur));
            }
        }

        // free the rest blocks
        for (auto cur_node = new_finish._node; cur_node != _finish._node; ++cur_node) {
            std::allocator_traits<map_allocator_type>::deallocate(_map_alloc, *cur_node, buffer_size());
        }

        _finish = new_finish;
    }
    else {
        // current elements are not enough
        // reuse the first n elements
        for (auto it = _start; it != _finish; ++it) {
            if constexpr (std::is_trivially_copy_assignable<T>::value) {
                *it = value;
            } else {
                std::allocator_traits<allocator_type>::construct(_alloc, std::addressof(*it), value);
            }
        }

        // emplace the rest elements
        for (size_type i = n; i < count; ++i) {
            emplace_back(value);
        }
    }
}

template <typename InputIt>
void assign (InputIt first, InputIt last,
            typename std::enable_if<!std::is_integral<InputIt>::value>::type* = nullptr)
{
    // for random access iterator, we can use distance to get the number of elements
    if constexpr (std::is_same<typename std::iterator_traits<InputIt>::iterator_category, std::random_access_iterator_tag>::value) {
        size_type count = mystl::distance(first, last);
        size_type n = _finish - _start;

        if (n >= count) {
            auto it = _start;
            for (size_type i = 0; i < count; ++i, ++it, ++first) {
                if constexpr (std::is_trivially_copy_assignable<T, typename std::iterator_traits<InputIt>::value_type>::value) {
                    *it = *first;
                } else {
                    std::allocator_traits<allocator_type>::construct(_alloc, std::addressof(*it), *first);
                }
            }

            auto new_finish = _start + static_cast<difference_type>(count);
            if constexpr (!std::is_trivially_destrutible<T>::value) {
                for (auto cur = new_finish; cur != _finish; ++cur) {
                    std::allocator_traits<allocator_type>::destroy(_alloc, std::addressof(*cur));
                }
            }

            for (auto cur_node = new_finish._node; cur_node != _finish._node; ++cur_node) {
                std::allocator_traits<map_allocator_type>::deallocate(_map_alloc, *cur_node, buffer_size());
            }

            _finish = new_finish;
        } else {
            // current elements are not enough
            for (auto it = _start; it != _finish; ++it, ++first) {
                if constexpr (std::is_trivially_copy_assignable<T, typename std::iterator_traits<InputIt>::value_type>::value) {
                    *it = *first;
                } else {
                    std::allocator_traits<allocator_type>::construct(_alloc, std::addressof(*it), *first);
                }
            }

            for (; first != last; ++first) {
                emplace_back(*first);
            }
        }
    }
    else {
        // for forward iterator, we can't use distance to get the number of elements
        clear();
        for (; first != last; ++first) {
            emplace_back(*first);
        }
    }
}

void assign(std::initializer_list<value_type> ilist)
{
    assign(ilist.begin(), ilist.end());
}
```

## Capacity
### `shrink_to_fit`
```c++
// shrink_to_fit
void shrink_to_fit() 
{
    // 如果为空或只有一个 block，无需收缩
    if (empty() || (_start._node == _finish._node)) {
        return;
    }

    try {
        // 1. 计算实际使用的元素数量和 blocks
        size_type element_count = size();
        size_type used_blocks = static_cast<size_type>(_finish._node - _start._node) + 1;
        
        // 2. 判断是否值得收缩
        // 条件：map 大小超过使用量的 2 倍，或者浪费的内存超过 4KB
        bool should_shrink = _map_size > used_blocks * 2 || 
                            (_map_size - used_blocks) * buffer_size() * sizeof(T) > 4096;
        
        if (!should_shrink) {
            return;
        }

        // 3. 计算新的 map 大小，保留一些增长空间
        size_type new_map_size = used_blocks + 2;  // 前后各预留一个位置

        // 4. 分配新的 map
        pointer* new_map = nullptr;
        try {
            new_map = map_traits_type::allocate(_map_alloc, new_map_size);
        }
        catch (...) {
            // 分配失败，保持原状
            return;
        }

        // 5. 设置新的起始位置，保持在中间
        pointer* new_start = new_map + 1;

        // 6. 如果元素数量很少且内存过于分散，尝试紧凑存储
        if (element_count < buffer_size() && used_blocks > 1) {
            // 分配单个连续的 block
            pointer new_block = nullptr;
            try {
                new_block = std::allocator_traits<allocator_type>::allocate(_alloc, buffer_size());
                
                // 移动所有元素到新 block
                size_type i = 0;
                for (auto it = _start; it != _finish; ++it, ++i) {
                    std::allocator_traits<allocator_type>::construct(
                        _alloc, 
                        new_block + i, 
                        std::move(*it)
                    );
                }

                // 释放原有元素
                if constexpr (!std::is_trivially_destructible<T>::value) {
                    for (auto it = _start; it != _finish; ++it) {
                        std::allocator_traits<allocator_type>::destroy(_alloc, std::addressof(*it));
                    }
                }

                // 释放原有 blocks
                for (pointer* node = _start._node; node <= _finish._node; ++node) {
                    std::allocator_traits<allocator_type>::deallocate(_alloc, *node, buffer_size());
                }

                // 设置新的 block
                new_start[0] = new_block;
                
                // 更新迭代器
                _map = new_map;
                _map_size = new_map_size;
                
                _start._node = new_start;
                _start._first = new_block;
                _start._last = new_block + buffer_size();
                _start._cur = new_block;
                
                _finish = _start;
                _finish._cur += element_count;
                
                // 释放旧的 map
                map_traits_type::deallocate(_map_alloc, _map, _map_size);
                
                return;
            }
            catch (...) {
                // 如果紧凑化失败，回退到普通的收缩
                if (new_block) {
                    std::allocator_traits<allocator_type>::deallocate(_alloc, new_block, buffer_size());
                }
            }
        }

        // 7. 常规收缩：复制有效的 block 指针
        for (size_type i = 0; i < used_blocks; ++i) {
            new_start[i] = _start._node[i];
        }

        // 8. 释放多余的 blocks
        for (pointer* node = _map; node < _start._node; ++node) {
            if (*node) {
                std::allocator_traits<allocator_type>::deallocate(_alloc, *node, buffer_size());
            }
        }
        for (pointer* node = _finish._node + 1; node < _map + _map_size; ++node) {
            if (*node) {
                std::allocator_traits<allocator_type>::deallocate(_alloc, *node, buffer_size());
            }
        }

        // 9. 释放旧的 map
        pointer* old_map = _map;
        size_type old_map_size = _map_size;

        // 10. 更新成员
        _map = new_map;
        _map_size = new_map_size;

        // 11. 更新迭代器
        difference_type start_offset = _start._cur - _start._first;
        difference_type finish_offset = _finish._cur - _finish._first;

        _start._node = new_start;
        _start._first = *new_start;
        _start._last = _start._first + buffer_size();
        _start._cur = _start._first + start_offset;

        _finish._node = new_start + (used_blocks - 1);
        _finish._first = *_finish._node;
        _finish._last = _finish._first + buffer_size();
        _finish._cur = _finish._first + finish_offset;

        // 12. 释放旧的 map
        map_traits_type::deallocate(_map_alloc, old_map, old_map_size);
    }
    catch (...) {
        // 如果发生任何异常，保持容器不变
    }
}
```


## Modifier

### `clear`
Erases all elements from the container. After this call, `size()` returns zero.

Invalidates any references, pointers, and iterators referring to contained elements. Any past-the-end iterators are also invalidated.

在这个操作之后，只会保留一个 block

```c++
void clear() noexcept
{
    if constexpr (!std::is_trivially_destructible<T>::value) {
        for (auto it = _start; it != _finish; ++it) {
            std::allocator_traits<allocator_type>::destroy(_alloc, std::addressof(*it));
        }
    }

    // free all blocks, maintian one block
    if (_start._node != _finish._node) {
        // maintain the first block
        pointer* first_node = _start._node;

        for (auto node = first_node + 1; node != _finish._node; ++node) {
            std::allocator_traits<map_allocator_type>::deallocate(_map_alloc, *node, buffer_size());
        }
    }

    _finish = _start;
}
```


### `emplace_back`

Appends a new element to the end of the container. The element is constructed through std::allocator_traits::construct, which typically uses placement new to construct the element in-place at the location provided by the container. The arguments `args...` are forwarded to the constructor as `std::forward<Args>(args)...`
All iterators (including the end() iterator) are invalidated. No references are invalidated.

当空间不足时会触发扩容

```c++
template <typename... Args>
reference emplace_back(Args&&... args)
{
    // 快路径：当前块还有剩余空间
    if (_finish._cur != _finish._last) {
        T* p = _finish._cur;
        std::allocator_traits<allocator_type>::construct(_alloc, p, std::forward<Args>(args)...);
        ++_finish._cur;
        return *p;
    }

    // 慢路径：当前块已满，需要分配新块
    pointer* new_node = _finish._node + 1

    // 如果 map_ 末端也已用尽，先扩容 map_
    if (new_node == _map + map_size - 1) {
        // 在后端增 1 个 slot
        _reallocate_map(0, 1);
        new_node = _finish._node + 1;
    }
    // 分配新 block
    *new_node = std::allocator_traits<allocator_type>::allocate(_alloc, buffer_size());
    // 更新 finish 的迭代器状态指向新 block
    _finish._node  = new_node;
    _finish._first = *new_node;
    _finish._last  = _finish._first + buffer_size();
    _finish._cur   = _finish._first;

    // 在新 block 上构造元素
    T* p = _finish._cur;
    std::allocator_traits<allocator_type>::construct(_alloc, p, std::forward<Args>(args)...);
    ++finish._cur;
    return *p;
}
```

#### _reallocate_map

这个函数是一个辅助函数，用于扩容，接受两个参数：

- add_front: 扩容后前面需要保留的空 block 数量
- add_back: 扩容后后面需要保留的空 block 数量

```c++
// _reallocate_map
/*
    在前端留 add_front，后端留 add_back，然后倍增 map_size
*/
void _reallocate_map(size_type add_front, size_type add_back)
{
    size_type old_map_size = _map_size;
    size_type old_nodes = static_cast<size_type>(_finish._node - _start._node + 1);
    size_type new_map_size = old_map_size + std::max(old_map_size, add_front + add_back);

    // 1) 分配新的 map_
    pointer* new_map = map_traits_type::allocate(_map_alloc, new_map_size);

    // 2) 计算新起始位置
    /*
        尽量将旧 block 中的内容放在居中位置，并确保前面有 add_front 个 block
        (new_map_size - old_nodes)：空槽位总数
        (new_map_size - old_nodes) / 2：把这批空槽位平均分成前后一半，这样旧的 block 指针就能“居中”放在新数组中间
        + add_front：标记你想在「这批居中空位之上」再多留 add_front 个槽位
    */
    pointer* new_start = new_map + (new_map_size - old_nodes) / 2 + add_front;

    // 3) 复制旧 blocks
    for (size_type i = 0; i < old_nodes; ++i) {
        new_start[i] = _start._node[i];
    }

    // 4) 释放旧 map_
    map_traits_type::deallocate(_map_alloc, _map, old_map_size);

    // 5) 更新成员
    _map = new_map;
    _map_size = new_map_size;

    // 6) 重设 start_/finish_ 的 node/first/last
    _start._node = new_start;
    _finish._node = new_start + old_nodes - 1;
    _start._first = *_start._node;
    _start._last = *_start._first + buffer_size();
    _finish._first = *_finish._node;
    _finish._last = *_finish._first + buffer_size();
}
```

需要注意的是，因为是对 `pointer*` 进行分配，而原来的 `std::allocator_traits` 是针对 `T` 进行分配的

- 当容器内部既要分配元素类型 `T`，又要分配其它类型（比如迭代器需要的指针 map、node 链表架构等）时，就必须用 `rebind`（或 `allocator_traits::rebind_alloc`）来得到正确的分配器类型。
- 保证类型安全——分配器知道自己在给哪种类型分配内存，`allocate(n)` 永远是分配 `n * sizeof(U)`。
- 保证策略统一——即便你的 `Alloc` 是有状态的（记录了某些池子或标识），`rebind_alloc<pointer>` 也会把那个状态正确地“映射”到新的 `allocator<pointer>` 上。
