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

### 成员函数

`void emplace_back(Args&&... args)`

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

### 通用函数

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

#### `explicit vector(const allocator_type& alloc)`

显式的 allocator 构造，基类 `vector_base` 接受一个 allocator 作为构造参数，因此直接调用对应基类的构造函数即可

```c++
explicit vector(const allocator_type& alloc) : base(alloc) {}
```

#### `vector() noexcept(noexcept(Allocator()))`

`vector() noexcept(noexcept(Allocator())) : vector(Allocator()) {}`

是 C++11 以来标准库推荐的实现方式，目的是：

- ✅ 利用 统一委托构造（delegating constructor）简化逻辑
- ✅ 保证 noexcept 语义对齐：如果构造 Allocator() 是 noexcept，这个默认构造函数也是 noexcept
  - 其意思是，如果默认构造 allocator 不会抛异常，那么整个 vector 构造也不会抛异常

借用上一个显式的 allocator 构造函数

```c++
vector() noexcept(noexcept(allocator_type())) : vector(allocator_type()) {}
```

#### `explicit vector(size_type n, const allocator_type& alloc = allocator_type())`

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

#### `constexpr vector(size_type count, const T& value, const allocator_type& alloc = allocator_type())` —— 用一个值构造 n 个元素

它需要完成的事情：

- 分配 n 个元素空间
- 对每个元素构造 T(val)
- 设置 `_start`, `_finish`, `_end_of_storage`

该函数调用 `_fill_initialize(n, val)` 来完成

#### `constexpr vector(InputIt first, InputIt last, const allocator& alloc = allocator_type())`

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

#### 拷贝构造

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

#### 移动构造

##### `constexpr vector(vector&& other) noexcept`

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

##### `constexpr vector(vector&& other, const allocator_type& alloc)`

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

#### 初始化列表构造函数

```c++
constexpr vector(std::initializer_list<T> init, const allocator_type& alloc = allocator_type())
    : base(alloc)
{
    _range_initialize(init.begin(), init.end(), std::random_access_iterator_tag{});
}
```

### 析构函数

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

### 拷贝赋值

#### `vector& operator=(const vector& other)`

这个操作的本质是 深拷贝赋值，核心目标是：将 `other` 中的元素拷贝到当前对象中，并保持强异常安全。

```c++
constexpr vector& operator=(const vector& other)
{
    if (this == other) {
        return *this;
    }

    const size_type otherSize = other.size();

    if (otherSize > this->capacity()) {
        // 容量不足，采用强异常安全策略：先构造一个副本，再 swap
        vector tmp(other);  // 拷贝构造
        this->swap(tmp);    // 交换资源，旧资源由 tmp 析构时自动释放
    } else {
        // 容量足够，重用现有内存
        size_type i = 0;
        // 赋值已有元素
        for (; i < this->size() && i < otherSize; ++i) {
            this->_start[i] = other._start[i];
        }
        // 构造新元素（如果 other 比当前长）
        for (; i < otherSize; ++i) {
            this->_allocator.construct(this->_start + i, other._start[i]);
            ++this->_finish;
        }
        // 销毁多余元素（如果当前比 other 长）
        for (; i < this->size(); ++i) {
            this->_allocator.destroy(this->_start + i);
        }
        this->_finish = this->_start + otherSize;
    }
}
```

首先判断是否是自赋值

- 如果是，则直接返回，不需要任何操作
- 否则进入拷贝赋值阶段

在拷贝赋值阶段，先判断当前 vector 容量是否足够

- 如果容量不足，采用强异常安全策略：先构造一个副本，再 swap
- 否则重用现有内存
  - 在赋值已有元素的时候，不需要考虑两个 vector 的 allocator 不同的情况
    - 此处我们已经处于拷贝赋值运算符内部，所以 other._allocator 是 const Allocator&，我们无法修改它，也 不允许 拷贝 other._allocator 替换当前对象的 allocator。
    - 标准库的实现约定：
      - 即使 other._allocator 与 this->_allocator 不同，拷贝赋值也不会替换 allocator。
      - allocator 的一致性只在 拷贝构造 和 **operator=(vector&&)（根据 propagate_on_container_move_assignment）中处理。
      - 如果你想允许在拷贝赋值时替换 allocator，你得实现 operator=(const vector& other, const allocator_type&)，这在标准中并不存在。
  - 另外，也不需要考虑类型不一致的问题
    - operator= 是成员函数，这两个 vector 必须是完全相同模板类型的实例，即 `vector<T, Alloc>`
    - 若你需要支持类型转换（如 `vector<int>` 赋值给 `vector<float>`），那属于 容器间的转换赋值，标准库不支持，也不应由这个函数承担。

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
