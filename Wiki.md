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
