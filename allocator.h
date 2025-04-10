#ifndef MYSTL_ALLOCATOR_H
#define MYSTL_ALLOCATOR_H

#include<new>

namespace mystl {

template <typename T>
class allocator {
public:
    using value_type = T;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using propagate_on_container_move_assignment = std::true_type;
    using is_always_equal = std::true_type;
    
    template <typename U>
    using rebind_alloc = allocator<U>;

public:
    // 对于无状态的allocator来说，构造函数和拷贝构造函数用默认的即可
    constexpr allocator() noexcept = default;
    constexpr allocator(const allocator&) noexcept = default;
    
    // 类型转换构造函数，因为allocator无状态，也不依赖U的内部信息
    // 所以这么做完全不会有问题，只是为了满足标准容器的 rebind / allocator_traits 要求
    template <typename U>
    constexpr allocator(const allocator<U>&) noexcept {}

    constexpr ~allocator() noexcept = default;

    constexpr pointer allocate(size_type n); 
    void deallocate(pointer p, size_type n);

    template <typename U, typename... Args>
    U* construct(U *p, Args&&... args);

    template <typename U>
    void destroy(U *p);

    size_type max_szie() const noexcept;
    pointer address(reference x) const noexcept;
    const_pointer address(const_reference x) const noexcept;
};

template <typename T>
constexpr typename allocator<T>::pointer
allocator<T>::allocate(typename allocator<T>::size_type n)
{
    if (__builtin_mul_overflow(n, sizeof(T), &n))
        throw std::bad_array_new_length();

    return static_cast<T*>(::operator new(n));
}

template <typename T>
void allocator<T>::deallocate(typename allocator<T>::pointer p, typename allocator<T>::size_type n)
{
    if (p == nullptr) return;
    ::operator delete(p);
}

// 注意这里要用模板嵌套
// allocator 是模板类，所以外层要 template<typename T>
// construct 是 allocator 的成员函数，所以内层也要 template<typename U, typename... Args>
template <typename T>
template <typename U, typename... Args>
U* allocator<T>::construct(U *p, Args&&... args)
{
    return ::new(static_cast<void*>(p)) U(std::forward<Args>(args)...);
}

template <typename T>
template <typename U>
void allocator<T>::destroy(U *p)
{
    if (p == nullptr) return;
    p->~U();
}

template <typename T>
typename allocator<T>::size_type allocator<T>::max_szie() const noexcept
{
    return std::numeric_limits<size_type>::max() / sizeof(T);
}

template <typename T>
typename allocator<T>::pointer allocator<T>::address(typename allocator<T>::reference x) const noexcept
{
    return std::addressof(x);
}

template <typename T>
typename allocator<T>::const_pointer allocator<T>::address(typename allocator<T>::const_reference x) const noexcept
{
    return std::addressof(x);

}

// 对于无状态allocator来说，标准写法是， “所有 allocator<T> 和 allocator<U> 实例，哪怕 T ≠ U，也认为是可互换的”
template <typename T, typename U>
constexpr bool operator==(const allocator<T>&, const allocator<U>&) noexcept
{
    return true;

}

template <typename T, typename U>
constexpr bool operator!=(const allocator<T>&, const allocator<U>&) noexcept
{
    return false;
}

}
#endif