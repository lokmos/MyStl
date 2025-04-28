#ifndef MYSTL_VECTOR_H
#define MYSTL_VECTOR_H

#include "mystl_allocator.h"
#include "mystl_iterator.h"
#include "mystl_memory.h"
#include <memory>
#include <concepts>
#include <iterator> // 使用标准 iterator_traits
#include <stdexcept> // std::out_of_range
#include <initializer_list>
#include <algorithm>

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

    // 为了rbegin(),rend() 等反向迭代器函数的实现
    using reverse_iterator = mystl::reverse_iterator<iterator>;
    using const_reverse_iterator = mystl::reverse_iterator<const_iterator>;

// Construct
public:
    vector() noexcept(noexcept(allocator_type())) : vector(allocator_type()) {}

    explicit vector(const allocator_type& alloc) : base(alloc) {}

    template <std::default_initializable U = T>
    explicit vector(size_type n, const allocator_type& alloc = allocator_type())
        : base(alloc)
    {
        // _fill_initialize(n, T{});

        // 优化后
        if (n == 0) {
            this->_start = this->_finish = this->_end_of_storage = nullptr;
            return;
        }
        pointer p = this->_allocator.allocate(n);
        this->_start = p;
        this->_end_of_storage = p + n;

        try {
            auto cur = mystl::uninitialized_value_construct_n(this->_allocator, p, n);
            this->_finish = cur;
        } catch (...) {
            // uninitialized_value_construct_n 内部已经 destroy 了
            this->_allocator.deallocate(p, n);
            throw;
        }
        
    }

    template <std::copy_constructible U = T>
    constexpr vector(size_type n, const T& value, const allocator_type& alloc = allocator_type())
        : base(alloc)
    {
        // _fill_initialize(count, value);

        // 优化后
        if (n == 0) {
            this->_start = this->_finish = this->_end_of_storage = nullptr;
            return;
        }
        pointer p = this->_allocator.allocate(n);
        this->_start = p;
        this->_end_of_storage = p + n;

        try {
            // uninitialized_fill_n uses allocator_traits internally
            auto cur = mystl::uninitialized_fill_n(this->_allocator, p, n, value);
            this->_finish = cur;
        } catch(...) {
            this->_allocator.deallocate(p, n);
            throw;
        }
    }

    // template <typename Integer>
    // requires std::integral<Integer>
    // vector(Integer n, const T& value, const allocator_type& alloc = allocator_type())
    //     : base(alloc)
    // {
    //     _fill_initialize(static_cast<size_type>(n), value);
    // }

    // range construct: contiguous + trivially copyable
    template <typename InputIt>
    requires std::contiguous_iterator<InputIt> && std::is_trivially_copyable_v<T>
    vector(InputIt first, InputIt last, const allocator_type& alloc = allocator_type())
        : base(alloc)
    {
        size_type n = static_cast<size_type>(last - first);
        if (n == 0) {
            this->_start = this->_finish = this->_end_of_storage = nullptr;
            return;
        }
        pointer p = this->_allocator.allocate(n);
        this->_start = p;
        this->_finish = p + n;
        this->_end_of_storage = this->_finish;
        // std::to_address(it) 会对原生指针、以及实现了 pointer_traits 的 fancy pointer 都返回底层的原始地址。
        std::memcpy(this->_start, std::to_address(first), n * sizeof(T));
    }

    // generic range construct
    template<typename InputIt>
    requires 
            std::input_iterator<InputIt> &&
            std::constructible_from<T, typename std::iterator_traits<InputIt>::reference> &&
            (!std::contiguous_iterator<InputIt> || !std::is_trivially_copyable_v<T>)
    vector(InputIt first, InputIt last, const allocator_type& alloc = allocator_type())
        : base(alloc)
    {
        // _range_initialize(first, last, typename std::iterator_traits<InputIt>::iterator_category{});

        // 优化后
        this->_start = this->_finish = this->_end_of_storage = nullptr;
        for (; first != last; ++first) {
            if (!this->_start) {
                // initial allocation for first element
                pointer p = this->_allocator.allocate(1);
                this->_start = p;
                this->_end_of_storage = p + 1;
                std::allocator_traits<allocator_type>::construct(
                    this->_allocator,
                    std::addressof(*this->_start),
                    *first
                );
                this->_finish = this->_start + 1;
            }
            else if (this->_finish == this->_end_of_storage) {
                // grow and append current value
                _realloc_insert(*first);
            } else {
                std::allocator_traits<allocator_type>::construct(
                    this->_allocator,
                    std::addressof(*this->_finish),
                    *first
                );
                ++this->_finish;
            }
        }
    }

    // 拷贝构造
    vector(const vector& other)
      : base(std::allocator_traits<allocator_type>::select_on_container_copy_construction(
            other._allocator))
    {
        size_type n = other.size();
        if (n == 0) {
            this->_start = this->_finish = this->_end_of_storage = nullptr;
            return;
        }
        pointer p = this->_allocator.allocate(n);
        this->_start = p;
        this->_end_of_storage = p + n;
        std::uninitialized_copy_n(other._start, n, p);
        this->_finish = p + n;
    }

    constexpr vector(const vector& other, const allocator_type& alloc)
        : base(alloc)
    {
        // _range_initialize(other._start, other._finish, std::random_access_iterator_tag{});

        // 优化后
        size_type n = other.size();
        pointer p = this->_allocator.allocate(n);
        this->_start = p;
        this->_end_of_storage = p + n;
        std::uninitialized_copy_n(other._start, n, p);
        this->_finish = p + n;
    }


    // 移动构造
    constexpr vector(vector&& other) noexcept(
        std::allocator_traits<allocator_type>::is_always_equal::value ||
        std::allocator_traits<allocator_type>::propagate_on_container_move_assignment::value)
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

        if (otherSize > this->capacity()) {
            // 强异常安全策略：拷贝构造副本，再交换
            vector tmp(other);
            this->swap(tmp); // tmp 会析构，释放资源
        } else {
            // 容量足够，重用内存
            size_type i = 0;
            for (; i < this->size() && i < otherSize; ++i) {
                // 原始指针天然支持 copy-assign
                this->_start[i] = other._start[i];
            }
            // for (; i < otherSize; ++i) {
            //     this->_allocator.construct(this->_finish, other._start[i]);
            //     ++this->_finish;
            // }
            // for (; i < this->size(); ++i) {
            //     this->_allocator.destroy(this->_start + i);
            // }

            // construct new elements
            if (i < otherSize) {
                pointer cur = this->_start + i;
                std::uninitialized_copy_n(other._start + i, otherSize - i, cur);
            }
            // destroy excess elements
            else if (i < this->size()) {
                destroy_n(this->_allocator, this->_start + i, this->size() - i);
            }
            this->_finish = this->_start + otherSize;
        }
        
        return *this;
    }

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
        return *this;
    }

    // 初始化列表赋值
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
            for (; it != ilist.end(); ++i, ++it) {
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

    // assign
    // 1) assign(count, value)
    constexpr void assign(size_type count, const T& value) {
        if (count > capacity()) {
            // 强异常安全：用临时 + swap
            vector tmp(count, value, get_allocator());
            swap(tmp);
            return;
        }
        size_type old_size = size();
        size_type min_cnt  = old_size < count ? old_size : count;

        // （1）覆盖前 min_cnt 个元素
        std::fill_n(this->_start, min_cnt, value);

        if (count > old_size) {
            // （2）构造尾部 [old_size, count)
            auto new_end = mystl::uninitialized_fill_n(
                this->_allocator,
                this->_finish,
                count - old_size,
                value
            );
            this->_finish = new_end;
        }
        else if (count < old_size) {
            // （3）销毁多余 [count, old_size)
            mystl::destroy_n(
                this->_allocator,
                this->_start + count,
                old_size - count
            );
            this->_finish = this->_start + count;
        }
    }

    // 2) assign(first, last)
    template <typename InputIt>
    requires std::input_iterator<InputIt> &&
             std::constructible_from<T,
                  typename std::iterator_traits<InputIt>::reference>
    constexpr void assign(InputIt first, InputIt last) {
        size_type n = static_cast<size_type>(std::distance(first, last));
        if (n > capacity()) {
            // 强异常安全
            vector tmp(first, last, get_allocator());
            swap(tmp);
            return;
        }
        size_type old_size = size();
        size_type min_cnt  = old_size < n ? old_size : n;

        // （1）覆盖前 min_cnt 个
        InputIt it = first;
        for (size_type i = 0; i < min_cnt; ++i, ++it) {
            this->_start[i] = *it;
        }

        if (n > old_size) {
            // （2）构造尾部 [old_size, n)
            auto new_end = mystl::uninitialized_copy_n(
                this->_allocator,
                it,
                n - old_size,
                this->_start + old_size
            );
            this->_finish = new_end;
        }
        else if (n < old_size) {
            // （3）析构多余
            mystl::destroy_n(
                this->_allocator,
                this->_start + n,
                old_size - n
            );
            this->_finish = this->_start + n;
        }
    }

    // 3) assign(initializer_list)
    constexpr void assign(std::initializer_list<value_type> ilist) {
        assign(ilist.begin(), ilist.end());
    }

    constexpr allocator_type get_allocator() const noexcept
    {
        return this->_allocator;
    }

//Element Access
public:
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

/// Iterators
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

// Capacity
public:
    constexpr bool empty() const noexcept
    {
        return this->_start == this->_finish;
    }

    constexpr size_type size() const noexcept
    {
        return mystl::distance(this->_start, this->_finish);
    }

    constexpr size_type max_size() const noexcept
    {
        return std::numeric_limits<size_type>::max() / sizeof(T);
    }

    constexpr size_type capacity() const noexcept
    {
        return mystl::distance(this->_start, this->_end_of_storage);
    }

    // strong exception guarantee
    constexpr void reserve(size_type new_cap) 
    {
        if (new_cap <= capacity()) return;
    
        if (new_cap > max_size()) {
            throw std::length_error("mystl::vector::reserve: new_cap > max_size");
        }

        pointer old_start = this->_start;
        pointer old_finish = this->_finish;
        size_type old_size = old_finish - old_start;
        size_type old_cap  = this->_end_of_storage - old_start;
    
        pointer new_start = this->_allocator.allocate(new_cap);
        pointer new_finish = new_start;
    
        if constexpr ( std::is_trivially_move_constructible_v<T> ) {
            // Trivial 类型：一次 memmove
            std::memmove(new_start, old_start, old_size * sizeof(T));
            new_finish = new_start + old_size;
        } else {
            // 非平凡类型：逐个移动构造，异常回滚
            try {
                new_finish = mystl::uninitialized_move_n(
                    this->_allocator,
                    old_start,
                    old_size,
                    new_start
                );
            } catch (...) {
                this->_allocator.deallocate(new_start, new_cap);
                throw;
            }
        }
    
        // 把旧元素析构并释放内存
        mystl::destroy_n(this->_allocator, old_start, old_size);
        this->_allocator.deallocate(old_start, old_cap);

        // 更新指针
        this->_start = new_start;
        this->_finish = new_finish;
        this->_end_of_storage = new_start + new_cap;
    }
    
    void shrink_to_fit() noexcept(
        noexcept(vector(std::declval<pointer>(), std::declval<pointer>(), std::declval<allocator_type>()))
        && noexcept(swap(std::declval<vector&>()))
    ) {
        if (capacity() > size()) {
            vector tmp(this->_start, this->_finish, this->_allocator);
            swap(tmp);
        }
    }

// Modifiers
public:
    constexpr void clear() noexcept
    {
        for (pointer p = this->_start; p != this->_finish; ++p) {
            this->_allocator.destroy(p);
        }
        this->_finish = this->_start;
    }

    constexpr iterator erase(const_iterator pos)
    {
        return erase(pos, pos + 1);
    }

    constexpr iterator erase(const_iterator first, const_iterator last)
    {
        // pointer non_const_first = const_cast<pointer>(first);
        // pointer non_const_last = const_cast<pointer>(last);

        // if (first == last) {
        //     return non_const_first;
        // }

        // pointer new_finish = this->_finish - (non_const_last - non_const_first);

        // // move 后面元素到前面
        // for (pointer p = non_const_last, d= non_const_first; p != this->_finish; ++p, ++d) {
        //     *d = std::move(*p);
        // }

        // // destroy 多出来的尾部元素
        // for (pointer p = new_finish; p != this->_finish; ++p) {
        //     this->_allocator.destroy(p);
        // }

        // this->_finish = new_finish;
        // return non_const_first;

        pointer p_first = const_cast<pointer>(first);
        pointer p_last  = const_cast<pointer>(last);

        if (p_first == p_last) {
            return p_first;
        }

        const size_type to_remove = p_last - p_first;
        pointer new_finish = this->_finish - to_remove;
        const size_type tail_count = this->_finish - p_last;

        // 1) 对于 Trivial 可搬运/可析构的类型，走一次 memmove
        if constexpr (
            std::is_trivially_move_assignable_v<T> &&
            std::is_trivially_destructible_v<T>
        ) {
            if (tail_count > 0) {
                std::memmove(
                    p_first,
                    p_last,
                    tail_count * sizeof(T)
                );
            }
            // 不需要显式析构
        }
        else {
            // 2) 非平凡类型：逐元素 move，然后按需析构尾部
            for (pointer read = p_last, write = p_first;
                 read != this->_finish;
                 ++read, ++write)
            {
                *write = std::move(*read);
            }

            if constexpr (!std::is_trivially_destructible_v<T>) {
                // 清理 [new_finish, old_finish)
                for (pointer p = new_finish; p != this->_finish; ++p) {
                    std::allocator_traits<allocator_type>::destroy(
                        this->_allocator, p
                    );
                }
            }
        }

        this->_finish = new_finish;
        return p_first;
    }

    // emplace
    // 在 pos 处插入元素，返回新元素的迭代器
    // optimized emplace at pos
    // emplace at pos
    template <typename... Args>
    iterator emplace(const_iterator cpos, Args&&... args) {
        size_type idx = cpos - this->_start;
        pointer pos = this->_start + idx;

        // fast path: there is room
        if (this->_finish != this->_end_of_storage) {
            // append?
            if (pos == this->_finish) {
                std::allocator_traits<allocator_type>::construct(
                    this->_allocator, this->_finish,
                    std::forward<Args>(args)...
                );
                ++this->_finish;
                return pos;
            }
            // middle insert: make room by shifting tail one slot right
            pointer last = this->_finish - 1;
            std::allocator_traits<allocator_type>::construct(
                this->_allocator, this->_finish,
                std::move_if_noexcept(*last)
            );
            size_type n = last - pos;
            if constexpr (std::is_trivially_copyable_v<T>) {
                std::memmove(pos + 1, pos, n * sizeof(T));
            } else {
                _move_range(this->_allocator, pos, last, pos + 1);
            }
            std::allocator_traits<allocator_type>::destroy(
                this->_allocator, pos
            );
            std::allocator_traits<allocator_type>::construct(
                this->_allocator, pos,
                std::forward<Args>(args)...
            );
            ++this->_finish;
            return pos;
        }

        // slow path: no room → build tmp + swap
        vector tmp;
        tmp.reserve(this->size() ? this->size() * 2 : 1);
        // prefix [begin, pos)
        for (pointer it = this->_start; it != pos; ++it)
            tmp.emplace_back(std::move(*it));
        // new element
        tmp.emplace_back(std::forward<Args>(args)...);
        // suffix [pos, end)
        for (pointer it = pos; it != this->_finish; ++it)
            tmp.emplace_back(std::move(*it));
        tmp.swap(*this);
        return this->_start + idx;
    }

    // insert single copy/move
    iterator insert(const_iterator pos, const T& value) {
        return emplace(pos, value);
    }
    iterator insert(const_iterator pos, T&& value) {
        return emplace(pos, std::move(value));
    }

    // insert count copies
    iterator insert(const_iterator cpos, size_type count, const T& value) {
        if (count == 0) return const_cast<pointer>(cpos);
        size_type idx = cpos - this->_start;
        pointer pos = this->_start + idx;
        size_type old_sz = this->size();

        // fast path
        if (this->capacity() >= old_sz + count) {
            pointer old_end = this->_finish;
            size_type tail = old_end - pos;
            pointer new_end = old_end + count;

            if constexpr (std::is_trivially_copyable_v<T>) {
                std::memmove(pos + count, pos, tail * sizeof(T));
                for (size_type i = 0; i < count; ++i) {
                    std::allocator_traits<allocator_type>::construct(
                        this->_allocator, pos + i, value
                    );
                }
            } else {
                _move_range(this->_allocator, pos, old_end, pos + count);
                for (size_type i = 0; i < count; ++i) {
                    std::allocator_traits<allocator_type>::construct(
                        this->_allocator, pos + i, value
                    );
                }
            }

            this->_finish = new_end;
            return pos;
        }

        // slow path
        vector tmp;
        tmp.reserve(old_sz + count);
        // prefix
        for (pointer it = this->_start; it != pos; ++it)
            tmp.emplace_back(std::move(*it));
        // inserts
        for (size_type i = 0; i < count; ++i)
            tmp.emplace_back(value);
        // suffix
        for (pointer it = pos; it != this->_finish; ++it)
            tmp.emplace_back(std::move(*it));
        tmp.swap(*this);
        return this->_start + idx;
    }

    // insert range [first,last)
    template <typename InputIt>
    requires std::input_iterator<InputIt>
         && std::constructible_from<
               T, typename std::iterator_traits<InputIt>::reference>
    iterator insert(const_iterator cpos, InputIt first, InputIt last) {
        size_type count = static_cast<size_type>(std::distance(first, last));
        if (count == 0) return const_cast<pointer>(cpos);
        size_type idx = cpos - this->_start;
        pointer pos = this->_start + idx;
        size_type old_sz = this->size();

        // fast path
        if (this->capacity() >= old_sz + count) {
            pointer old_end = this->_finish;
            size_type tail = old_end - pos;
            pointer new_end = old_end + count;

            if constexpr (std::is_trivially_copyable_v<T>
                       && std::contiguous_iterator<InputIt>) {
                std::memmove(pos + count, pos, tail * sizeof(T));
                std::memcpy(pos,
                            std::to_address(first),
                            count * sizeof(T));
            } else {
                _move_range(this->_allocator, pos, old_end, pos + count);
                pointer cur = pos;
                for (; first != last; ++first, ++cur) {
                    std::allocator_traits<allocator_type>::construct(
                        this->_allocator,
                        std::addressof(*cur),
                        *first
                    );
                }
            }

            this->_finish = new_end;
            return pos;
        }

        // slow path
        vector tmp;
        tmp.reserve(old_sz + count);
        for (pointer it = this->_start; it != pos; ++it)
            tmp.emplace_back(std::move(*it));
        for (auto it = first; it != last; ++it)
            tmp.emplace_back(std::move(*it));
        for (pointer it = pos; it != this->_finish; ++it)
            tmp.emplace_back(std::move(*it));
        tmp.swap(*this);
        return this->_start + idx;
    }

    constexpr iterator insert(const_iterator pos, std::initializer_list<T> il) 
    {
        // delegate to the [first,last) overload
        return insert(pos, il.begin(), il.end());
    }

    // emplace_back / push_back / pop_back
    template <typename... Args>
    void emplace_back(Args&&... args) {
        if (this->_finish != this->_end_of_storage) {
            std::allocator_traits<allocator_type>::construct(
                this->_allocator, this->_finish,
                std::forward<Args>(args)...
            );
            ++this->_finish;
        } else {
            emplace(end(), std::forward<Args>(args)...);
        }
    }

    void push_back(const T& value) {
        emplace_back(value);
    }
    void push_back(T&& value) {
        emplace_back(std::move(value));
    }

    void pop_back() {
        --this->_finish;
        std::allocator_traits<allocator_type>::destroy(
            this->_allocator, this->_finish
        );
    }

    // resize
    /*
    Resizes the container to contain count elements:
        If count is equal to the current size, does nothing.
        If the current size is greater than count, the container is reduced to its first count elements.
        If the current size is less than count, then:
            1) Additional default-inserted elements(since C++11) are appended.
            2) Additional copies of value are appended.
    */
    constexpr void resize(size_type count)
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
            for (pointer p = new_finish; p != this->_finish; ++p) {
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


    void swap(vector& other) noexcept
    {
        using std::swap;
        swap(this->_start, other._start);
        swap(this->_finish, other._finish);
        swap(this->_end_of_storage, other._end_of_storage);
        /*
        If std::allocator_traits<allocator_type>::propagate_on_container_swap::value is true, 
        then the allocators are exchanged using an unqualified call to non-member swap. 
        Otherwise, they are not swapped (
        and if get_allocator() != other.get_allocator(), the behavior is undefined
        ).
        */
        swap(this->_allocator, other._allocator);
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
        size_type n = std::distance(first, last);
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

    // 左闭右开区间
    void _move_range(Alloc& alloc, T* from, T* end, T* to)
    {
        const size_type n = static_cast<size_type>(end - from);

        if (n == 0 || from == to) return;

        // 前向构造
        if (to < from) {
            for (size_type i = 0; i < n; ++i) {
                if constexpr (std::is_trivially_move_assignable_v<T>) {
                    to[i] = std::move(from[i]);
                } else {
                    std::allocator_traits<Alloc>::construct(alloc, to + i, std::move_if_noexcept(from[i]));
                    std::allocator_traits<Alloc>::destroy(alloc, from + i);
                }
            }
        } else {
            // 后向构造
            for (size_type i = n; i > 0; --i) {
                if constexpr (std::is_trivially_move_assignable_v<T>) {
                    to[i - 1] = std::move(from[i - 1]);
                } else {
                    std::allocator_traits<Alloc>::construct(alloc, to + i - 1, std::move_if_noexcept(from[i - 1]));
                    std::allocator_traits<Alloc>::destroy(alloc, from + i - 1);
                }
            }
        }
    }

    void _uninitialized_fill_n(allocator_type& alloc, pointer dest, size_type n, const T& value)
    {
        pointer cur = dest;
        try {
            for (size_type i = 0; i < n; ++i, ++cur) {
                std::allocator_traits<Alloc>::construct(alloc, cur, value);
            }
        } catch(...) {
            for (pointer p = dest; p != cur; ++p) {
                std::allocator_traits<Alloc>::destroy(alloc, p);
            }
            throw;
        }
    }
};

// non-member functions
template <typename T, typename Alloc>
bool operator==(const vector<T, Alloc>& lhs, const vector<T, Alloc>& rhs)
{
    return std::equal(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());

}

template <typename T, typename Alloc>
bool operator!=(const vector<T, Alloc>& lhs, const vector<T, Alloc>& rhs)
{
    return !(lhs == rhs);
}

template <typename T, typename Alloc>
bool operator<(const vector<T, Alloc>& lhs, const vector<T, Alloc>& rhs)
{
    return std::lexicographical_compare(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
}

template <typename T, typename Alloc>
bool operator<=(const vector<T, Alloc>& lhs, const vector<T, Alloc>& rhs)
{
    return !(rhs < lhs);
}

template <typename T, typename Alloc>
bool operator>(const vector<T, Alloc>& lhs, const vector<T, Alloc>& rhs)
{
    return rhs < lhs;
}

template <typename T, typename Alloc>
bool operator>=(const vector<T, Alloc>& lhs, const vector<T, Alloc>& rhs)
{
    return !(lhs < rhs);
}

template <typename T, typename Alloc>
/*
    如果 lhs.swap(rhs) 是 noexcept 的，那么这里也是 noexcept。
    这个写法符合 标准库 noexcept 传递原则。
*/
void swap(vector<T, Alloc>& lhs, vector<T, Alloc>& rhs) noexcept(noexcept(lhs.swap(rhs)))
{
    lhs.swap(rhs);
}

template <typename T, typename Alloc, typename U>
constexpr typename vector<T, Alloc>::size_type
erase(vector<T, Alloc>& c, const U& value)
{
    auto oldSize = c.size();
    auto new_end = std::remove(c.begin(), c.end(), value);
    auto newSize = static_cast<typename mystl::vector<T, Alloc>::size_type>(new_end - c.begin());
    c.erase(new_end, c.end());
    return oldSize - newSize;
}

template <class T, class Alloc, class Pred>
constexpr typename mystl::vector<T, Alloc>::size_type
erase_if(mystl::vector<T, Alloc>& c, Pred pred)
{
    auto oldSize = c.size();
    auto new_end = std::remove_if(c.begin(), c.end(), pred);
    auto newSize = static_cast<typename mystl::vector<T, Alloc>::size_type>(new_end - c.begin());
    c.erase(new_end, c.end());
    return oldSize - newSize;
}


} // namespace mystl
#endif // MYSTL_VECTOR_H
