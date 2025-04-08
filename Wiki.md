# 配置器

## allocator

**标准的 std::allocator\<T\> 实现了以下四项职责：**

| 类别 | 方法名 | 功能说明 |
|------|--------|----------|
| 分配 | `T* allocate(size_t n)` | 分配能存储 `n` 个 `T` 对象的原始内存（未构造） |
| 释放 | `void deallocate(T* p, size_t)` | 释放之前通过 `allocate` 分配的内存 |
| 构造 | `void construct(T* p, Args&&...)` | 在已分配的地址上用 placement new 构造对象（C++17 前直接提供，之后通过 traits） |
| 析构 | `void destroy(T* p)` | 调用析构函数销毁对象（C++17 前直接提供） |

### 分配

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
