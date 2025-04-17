#ifndef MYSTL_VECTOR_H
#define MYSTL_VECTOR_H

#include "allocator.h"
#include "iterator.h"
#include <memory>
#include <concepts>
#include <iterator> // 使用标准 iterator_traits

namespace mystl
{

template <typename T, typename Alloc>
class vector_base
{
public:
    using value_type = T;
    using allocator_type = Alloc;
    using pointer = T*;
    using size_type = std::size_t;
protected:
    pointer _start; // 指向第一个元素
    pointer _finish; // 指向最后一个元素的下一个位置
    pointer _end_of_storage; // 指向分配的内存的末尾
    allocator_type _allocator;
public:
    vector_base() noexcept
        : _start(nullptr), _finish(nullptr), _end_of_storage(nullptr) {}
    explicit vector_base(const allocator_type& alloc) noexcept
        : _start(nullptr), _finish(nullptr), _end_of_storage(nullptr), _allocator(alloc) {}

    ~vector_base() = default; // 真正资源释放由 vector 控制

    template <typename U, typename A>
    friend class vector;
};

template <typename T, typename Alloc = mystl::allocator<T>>
class vector : protected vector_base<T, Alloc>
{
public:
    using base = vector_base<T, Alloc>;
    using value_type      = typename base::value_type;
    using allocator_type  = typename base::allocator_type;
    using size_type       = typename base::size_type;
    using difference_type = std::ptrdiff_t;
    using pointer         = typename base::pointer;
    using const_pointer   = const pointer;
    using reference       = value_type&;
    using const_reference = const value_type&;
    using iterator        = pointer;
    using const_iterator  = const_pointer;
    // 为了 rbegin() 和 rend() 提供反向迭代器
    using reverse_iterator = mystl::reverse_iterator<iterator>;
    using const_reverse_iterator = mystl::reverse_iterator<const_iterator>;

public:
    vector() noexcept(noexcept(allocator_type())) : vector(allocator_type()) {}

    explicit vector(const allocator_type& alloc) : base(alloc) {}

    template <std::default_initializable U = T>
    explicit vector(size_type n, const allocator_type& alloc = allocator_type())
        : base(alloc)
    {
        _fill_initialize(n, T{});
    }

    template <std::copy_constructible U = T>
    constexpr vector(size_type count, const T& value, const allocator_type& alloc = allocator_type())
        : base(alloc)
    {
        _fill_initialize(count, value);
    }

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

    // 拷贝构造
    constexpr vector(const vector& other)
        : base(other._allocator)
    {
        _range_initialize(other._start, other._finish, std::random_access_iterator_tag{});
    }

    constexpr vector(const vector& other, const allocator_type& alloc)
        : base(alloc)
    {
        _range_initialize(other._start, other._finish, std::random_access_iterator_tag{});
    }


    // 移动构造
    constexpr vector(vector&& other) noexcept
        : base(std::move(other._allocator))
    {
        this->_start = other._start;
        this->_finish = other._finish;
        this->_end_of_storage = other._end_of_storage;

        other._start = nullptr;
        other._finish = nullptr;
        other._end_of_storage = nullptr;
    }

    constexpr vector(vector&& other, const allocator_type& alloc)
        : base(alloc)
    {
        // 如果分配器相同，直接移动资源
        if (alloc == other._allocator) {
            this->_start = other._start;
            this->_finish = other._finish;
            this->_end_of_storage = other._end_of_storage;
            other._start = nullptr;
            other._finish = nullptr;
            other._end_of_storage = nullptr;
        }
        // 否则，使用 range_initialize
        // 重新分配内存并移动元素
        else {
            _range_initialize(
                // 用 std::make_move_iterator 返回一个移动迭代器
                // 这个迭代器会在迭代时移动元素
                std::make_move_iterator(other._start),
                std::make_move_iterator(other._finish),
                std::random_access_iterator_tag{}
            );
        }
    }

    // 初始化列表
    constexpr vector(std::initializer_list<T> init, const allocator_type& alloc = allocator_type())
        : base(alloc)
    {
        _range_initialize(init.begin(), init.end(), std::random_access_iterator_tag{});
    }

    // 析构函数
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

    // 拷贝赋值
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

public:
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

/// 迭代器
public:
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

// 通用函数
public:
    constexpr size_type size() const noexcept
    {
        return mystl::distance(this->_start, this->_finish);
    }

    constexpr size_type capacity() const noexcept
    {
        return mystl::distance(this->_start, this->_end_of_storage);
    }

    void swap(vector& other) noexcept
    {
        if (this->_allocator == other._allocator) {
            std::swap(this->_start, other._start);
            std::swap(this->_finish, other._finish);
            std::swap(this->_end_of_storage, other._end_of_storage);
        }
        else {
            // 如果分配器不同，使用 std::allocator_traits 进行交换
            allocator_traits<allocator_type>::swap(this->_allocator, other._allocator);
        }
    }

private:
    template <typename... Args>
    void _realloc_insert(Args&&... args)
    {
        const size_type old_size = this->_finish - this->_start;
        const size_type old_capacity = this->_end_of_storage - this->_start;
        const size_type new_capacity = old_capacity == 0 ? 1 : old_capacity * 2;

        pointer new_start = this->_allocator.allocate(new_capacity);
        pointer new_finish = new_start;

        try {
            for (pointer p = this->_start; p != this->_finish; ++p, ++new_finish) {
                this->_allocator.construct(new_finish, std::move(*p));
            }
            this->_allocator.construct(new_finish, std::forward<Args>(args)...);
            ++new_finish;
        }
        catch(...) {
            for (pointer p = new_start; p != new_finish; ++p) {
                this->_allocator.destroy(p);
            }
            this->_allocator.deallocate(new_start, new_capacity);
            throw;
        }

        for (pointer p = this->_start; p != this->_finish; ++p) {
            this->_allocator.destroy(p);
        }
        this->_allocator.deallocate(this->_start, old_capacity);

        this->_start = new_start;
        this->_finish = new_finish;
        this->_end_of_storage = new_start + new_capacity;
    }

    void _fill_initialize(size_type n, const T& value)
    {
        this->_start = this->_allocator.allocate(n);
        this->_finish = this->_start;
        this->_end_of_storage = this->_start + n;

        try {
            for (; this->_finish != this->_end_of_storage; ++this->_finish)
                this->_allocator.construct(this->_finish, value);
        } catch (...) {
            for (pointer p = this->_start; p != this->_finish; ++p)
                this->_allocator.destroy(p);
            this->_allocator.deallocate(this->_start, n);
            this->_start = this->_finish = this->_end_of_storage = nullptr;
            throw;
        }
    }

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
        size_type n = mystl::distance(first, last);
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
};

}

#endif
