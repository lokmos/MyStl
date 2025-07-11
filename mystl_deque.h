#ifndef MYSTL_DEQUE_H
#define MYSTL_DEQUE_H

#include <cstddef>
#include <iterator>
#include <algorithm>
#include <memory>
#include "mystl_allocator.h"
#include "mystl_iterator.h"

namespace mystl
{
template <typename T, typename Alloc = allocator<T>>
class deque
{
public:
    using value_type = T;
    using allocator_type = Alloc;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;

    // 为了分配 _map（pointer*）我们需要 rebind allocator 到 pointer
    /*
        之所以要 rebind，正是因为你这里要分配的不是 T，而是一段 pointer（也就是 T*）数组
            你的 allocator_type（比如 mystl::allocator<T>）天然是用来给 T 分配内存的：它的 allocate(n) 会分配 n * sizeof(T) 大小。
            但是 _map 本质上是一个 pointer* 数组，你想分配 map_size 个 pointer 对象，也就是总共 map_size * sizeof(pointer) 的空间。
            如果直接用 allocator_type 去调用 allocate(map_size)，那分配器会认为你是在分配 map_size 个 T，而不是 pointer，两者的 sizeof 很可能不同。
        C++11 起标准库就引入了 rebind（或 allocator_traits::rebind_alloc<U>）这样一套机制，让你从一个 allocator<T> “转换”得到一个 allocator<U>，从而可以用同一份分配器策略（stateful or stateless）去给不同类型 U 分配内存。
    */
    using map_allocator_type = typename std::allocator_traits<allocator_type>::template rebind_alloc<pointer>;
    using map_traits_type = std::allocator_traits<map_allocator_type>;


    // 为 deque 的迭代器服务
    // 与 vector 不同，deque 不能使用裸指针
    // 因为 deque 不同块的地址不连续，而裸指针遇到块的边界后，不知道如何处理
    // iterator 是元素级的，不是块级的
    class iterator;
    class const_iterator;
    class reverse_iterator;
    class const_reverse_iterator;

public:
// construct
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
            // —— 直接"窃取" other 的底层存储 —— //
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

            _finish = dst;

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

    // operator=
    deque& operator=(const deque& other)
    {
        if (this == &other) {
            return *this;
        }

        allocator_type old_alloc = _alloc;
        map_allocator_type old_map_alloc = _map_alloc;

        bool alloc_changed = false;
        if constexpr (std::allocator_traits<allocator_type>::propagate_on_container_copy_assignment::value) {
            if (_alloc != other._alloc) {
                alloc_changed = true;
            }
            _alloc = other._alloc;
            _map_alloc = other._map_alloc;
        }

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
        }
        else {
            // 5. 如果分配器没变，可以简单地释放旧资源并复制
            if constexpr (!std::is_trivially_destructible<T>::value) {
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
        }
        return *this;
    }

    // move assignment
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
            _map_alloc = std::move(other._map_alloc);
        }

        // 2. 如果分配器允许，直接"窃取"other的资源
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

    // initialize list
    deque& operator=(std::initializer_list<value_type> ilist) 
    {
        // 1. free current elements
        if constexpr (!std::is_trivially_destructible<T>::value) {
            for (auto it = _start; it != _finish; ++it) {
                std::allocator_traits<allocator_type>::destroy(_alloc, std::addressof(*it));
            }
        }
        _deallocate_all_blocks();

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
            for (auto cur_node = new_finish._node + 1; cur_node <= _finish._node; ++cur_node) {
                std::allocator_traits<allocator_type>::deallocate(_alloc, *cur_node, buffer_size());
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
    void assign(InputIt first, InputIt last,
                typename std::enable_if<!std::is_integral<InputIt>::value>::type* = nullptr)
    {
        if constexpr (std::is_same<typename std::iterator_traits<InputIt>::iterator_category, std::random_access_iterator_tag>::value) {
            size_type count = mystl::distance(first, last);
            size_type n = _finish - _start;

            if (n >= count) {
                auto it = _start;
                for (size_type i = 0; i < count; ++i, ++it, ++first) {
                    if constexpr (std::is_trivially_copy_assignable<T>::value) {
                        *it = *first;
                    } else {
                        std::allocator_traits<allocator_type>::construct(_alloc, std::addressof(*it), *first);
                    }
                }

                auto new_finish = _start + static_cast<difference_type>(count);
                if constexpr (!std::is_trivially_destructible<T>::value) {
                    for (auto cur = new_finish; cur != _finish; ++cur) {
                        std::allocator_traits<allocator_type>::destroy(_alloc, std::addressof(*cur));
                    }
                }

                // 释放多余的块
                for (auto cur_node = new_finish._node + 1; cur_node <= _finish._node; ++cur_node) {
                    std::allocator_traits<allocator_type>::deallocate(_alloc, *cur_node, buffer_size());
                }

                _finish = new_finish;
            } else {
                for (auto it = _start; it != _finish; ++it, ++first) {
                    if constexpr (std::is_trivially_copy_assignable<T>::value) {
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

    // get_allocator
    allocator_type get_allocator() const noexcept
    {
        return _alloc;
    }

public:
// Element Access
    // 随机访问，不做边界检查
    reference operator[](size_type pos) 
    {
        return *(_start + static_cast<difference_type>(pos));
    }

    const_reference operator[](size_type pos) const 
    {
        return *(_start + static_cast<difference_type>(pos));
    }

    // 带边界检查的访问：越界抛 std::out_of_range
    reference at(size_type pos) 
    {
        if (pos >= size()) {
            throw std::out_of_range("deque::at: index out of range");
        }
        return operator[](pos);
    }

    const_reference at(size_type pos) const 
    {
        if (pos >= size()) {
            throw std::out_of_range("deque::at: index out of range");
        }
        return operator[](pos);
    }

    // front
    // if dq is empty, the behavior is undefined
    reference front()
    {
        return *_start;
    }

    const_reference front() const
    {
        return *_start;
    }

    // back
    // if dq is empty, the behavior is undefined
    reference back()
    {
        return *(_finish - 1);
    }

    const_reference back() const
    {
        return *(_finish - 1);
    }

public:
// Iterators
    iterator begin() noexcept
    {
        return _start;
    }
    const_iterator begin() const noexcept
    {
        return _start;
    }
    const_iterator cbegin() const noexcept
    {
        return _start;
    }
    iterator end() noexcept
    {
        return _finish;
    }
    const_iterator end() const noexcept
    {
        return _finish;
    }
    const_iterator cend() const noexcept
    {
        return _finish;
    }

public:
// Capacity
    size_type size() noexcept 
    {
        return static_cast<size_type>(_finish - _start);
    }

public:
// Modifier
    // clear
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

            for (auto node = first_node + 1; node <= _finish._node; ++node) {
                std::allocator_traits<allocator_type>::deallocate(_alloc, *node, buffer_size());
            }
        }

        _finish = _start;
    }

    // insert
    iterator insert(const_iterator pos, const T& value)
    {
        // 1) 计算插入位置的偏移量
        difference_type index = pos - cbegin();
        difference_type num_elements = size();

        // 2) Empty container
        if (num_elements == 0) {
            empalce_back(value);
            return begin();
        }

        // 3) Special Case: insert at front or back
        if (index == 0) {
            emplace_front(value);
            return begin();
        } 
        if (index == num_elements) {
            emplace_back(value);
            return end() - 1;
        }

        // 4) Check if there is enough space
        bool front_has_space = _start._cur != _start._first;
        bool back_has_space = _finish._cur != _finish._last;

        // 5) Decide move strategy
        bool move_front;

        if (front_has_space && !back_has_space) {
            move_front = true;
        } else if (!front_has_space && back_has_space) {
            move_front = false;
        } else {
            move_front = (index * 2 < num_elements);
        }

        if (move_front) {
            return _insert_aux_front(index, value);
        }
        return _insert_aux_back(index, value);
    }

    iterator insert(const_iterator pos, T&& value)
    {
        return insert(pos, std::move(value));
    }

    iterator insert(const_iterator pos, size_type count, const T& value)
    {
        if (count == 0) {
            return begin() + (pos - cbegin());  // 使用索引而不是直接与迭代器相加
        }

        difference_type index = pos - cbegin();
        difference_type num_elements = size();

        if (num_elements == 0) {
            for (size_type i = 0; i < count; ++i) {
                emplace_back(value);
            }
            return begin();
        }

        if (index == 0) {
            for (size_type i = 0; i < count; ++i) {
                emplace_front(value);
            }
            return begin();
        }

        if (index == num_elements) {
            iterator result = end();  // 保存插入前的end()位置
            for (size_type i = 0; i < count; ++i) {
                emplace_back(value);
            }
            return result;  // 返回指向第一个插入元素的迭代器
        }

        bool front_has_space = _start._cur - _start._first >= count;
        bool back_has_space = _finish._last - _finish._cur >= count;

        bool move_front;

        if (front_has_space && !back_has_space) {
            move_front = true;
        } else if (!front_has_space && back_has_space) {
            move_front = false;
        } else {
            move_front = (index * 2 < num_elements);
        }

        if (move_front) {
            return _insert_aux_front(index, count, value);
        }
        return _insert_aux_back(index, count, value);
    }

    template <typename InputIt>
    iterator insert(const_iterator pos, InputIt first, InputIt last)
    {
        difference_type index = pos - cbegin();
        difference_type num_elements = size();

        auto len = std::distance(first, last);
        if (len == 0) {
            return begin() + index;
        }

        bool move_front;
        bool front_has_space = _start._cur - _start._first >= len;
        bool back_has_space = _finish._last - _finish._cur >= len;

        if (front_has_space && !back_has_space) {
            move_front = true;
        }
        else if (!front_has_space && back_has_space) {
            move_front = false;
        }
        else {
            move_front = (index * 2 < num_elements);
        }

        if (move_front) {   
            return _insert_aux_front(index, first, last);
        }
        return _insert_aux_back(index, first, last);
    }

    iterator insert(const_iterator pos, std::initializer_list<T> ilist)
    {
        return insert(pos, ilist.begin(), ilist.end());
    }

    // emplace
    template <typename... Args>
    iterator emplace(const_iterator pos, Args&&... args)
    {
        if (pos == cbegin()) {
            emplace_front(std::forward<Args>(args)...);
            return begin();
        }
        else if (pos == cend()) {
            emplace_back(std::forward<Args>(args)...);
            return end() - 1;
        }
        else {
            return _emplace_aux(pos, std::forward<Args>(args)...);
        }
    }

    template <typename... Args>
    reference emplace_front(Args&&... args)
    {
        // 快路径：当前块还有剩余空间
        if (_start._cur != _start._first) {
            T* p = _start._cur - 1;
            std::allocator_traits<allocator_type>::construct(_alloc, p, std::forward<Args>(args)...);
            --_start._cur;
            return *p;
        }

        // 慢路径：当前块已满，需要分配新块
        // 如果前面没有多余块，需要重新分配空间
        if (_start._node == _map) {
            _reallocate_map(1, 0);
        }

        pointer* new_node = _start._node - 1;
        *new_node = std::allocator_traits<allocator_type>::allocate(_alloc, buffer_size());
        _start._node = new_node;
        _start._first = *new_node;
        _start._last = _start._first + buffer_size();
        _start._cur = _start._last - 1;

        // 在新 node 上构造元素
        T* p = _start._cur;
        std::allocator_traits<allocator_type>::construct(_alloc, p, std::forward<Args>(args)...);
        --_start._cur;
        return p;
    }

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
        pointer* new_node = _finish._node + 1;

        // 如果 map_ 末端也已用尽，先扩容 map_
        if (new_node == _map + _map_size - 1) {
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
        ++_finish._cur;
        return *p;
    }



private:
    // _map 指向一块 pointer 数组，数组中每个元素指向一个 buffer block
    pointer* _map; // aka _MapPtr, 大小为 map_size_
    size_type _map_size; // _map 数组的长度（可存放多少 block 指针）

    // start_/finish_ 用来标记 deque 内有效元素的 begin/end，
    // 它们是封装了四个指针的迭代器：
    //   cur —— 当前块内的位置
    //   first/last —— 当前块的 begin/end
    //   node —— 指向 map_ 上对应块指针的地址
    iterator _start;
    iterator _finish;

    // allocator
    allocator_type _alloc;
    map_allocator_type _map_alloc; // 用于分配 _map 的 allocator

private:
    static inline size_type buffer_size() noexcept {
        constexpr size_type buf_bytes = 512;
        if constexpr (sizeof(T) < buf_bytes) {
            return buf_bytes / sizeof(T);
        }
        else {
            return 1;
        }
    }

    // _initialize_map
    /*
        1. 计算出 map 的初始大小（至少 nblocks + 2），
        2. 调用 _alloc 给 _map 分配这段指针数组，
        3. 在中间位置给每个 block 指针分配一个 buffer，
        4. 设置 _start/_finish 的内部指针指向第一个（或唯一的）block 的起始位置。
    */
    void _initialize_map(size_type nblocks) {
        // 至少一个 block，以免之后迭代器里出现 nullptr
        size_type num_blocks = nblocks ? nblocks : 1;

        // 给 map_ 分配 num_blocks + 2 大小，前后各留一个 slot 以便 push_front/push_back
        _map_size = num_blocks + 2;
        
        // 用 operator new 分配指针数组
        // 要用 rebind 后的 map_allocator_type 来分配 map_，因为它是一个指针数组
        _map = static_cast<pointer*>(map_traits_type::allocate(_map_alloc, _map_size));

        // 把实际的 block 指针数组放在 map_ 的中间
        pointer* block_ptr = _map + 1;
        for (size_type i = 0; i < num_blocks; ++i) {
            block_ptr[i] = std::allocator_traits<allocator_type>::allocate(_alloc, buffer_size());
        }

        // 设置 start
        _start = iterator(
        /*cur*/   block_ptr[0],
        /*first*/ block_ptr[0],
        /*last*/  block_ptr[0] + buffer_size(),
        /*node*/  block_ptr
        );
        _finish = _start;
        _finish += static_cast<difference_type>(nblocks);
    }
    
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
            (new_map_size - old_nodes) / 2：把这批空槽位平均分成前后一半，这样旧的 block 指针就能"居中"放在新数组中间
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

        // 6) 重设 start_/finish_ 的 node/first/last/cur
        difference_type start_offset  = _start._cur  - _start._first;
        difference_type finish_offset = _finish._cur - _finish._first;

        _start._node  = new_start;
        _start._first = *_start._node;
        _start._last  = _start._first + buffer_size();
        _start._cur   = _start._first + start_offset;

        _finish._node  = new_start + (old_nodes - 1);
        _finish._first = *_finish._node;
        _finish._last  = _finish._first + buffer_size();
        _finish._cur   = _finish._first + finish_offset;
    }

    //
    void _deallocate_all_blocks() noexcept 
    {
        if (!_map) {
            return;
        }

        // 1) 计算 block 数量
        size_type blocks = static_cast<size_type>(_finish._node - _start._node + 1);

        // 2) 逐块释放
        pointer* node = _start._node;
        for (size_type i = 0; i < blocks; ++i, ++node) {
            // 先释放 block 内存（无需 destroy，因为元素已在调用处销毁）
            std::allocator_traits<allocator_type>::deallocate(_alloc, *node, buffer_size());
        }

        // 3) 释放 _map 数组
        std::allocator_traits<map_allocator_type>::deallocate(_map_alloc, _map, _map_size);
    }

    // 向前移动元素并插入
    iterator _insert_aux_front(difference_type index, const T& value)
    {
        bool front_has_space = _start._cur != _start._first;

        if (!front_has_space) {
            if (_start._node == _map) {
                _reallocate_map(1, 0);
            }
            pointer* new_node = _start._node - 1;
            *new_node = std::allocator_traits<allocator_type>::allocate(_alloc, buffer_size());
            _start._node = new_node;
            _start._first = *new_node;
            _start._last = _start._first + buffer_size();
            _start._cur = _start._last - 1;
        }
        else {
            --_start._cur;
        }

        iterator front_pos = begin();
        iterator old_front = front_pos + 1;
        iterator target = front_pos + index + 1;

        std::allocator_traits<allocator_type>::construct(_alloc, std::addressof(*front_pos));

        for (auto it = old_front; it != target; ++it, ++front_pos) {
            *front_pos = std::move(*it);
        }

        // 在目标位置放入新值
        iterator result = front_pos;
        if constexpr (std::is_trivially_copy_assignable_v<T>) {
            *result = value;
        } else {
            std::allocator_traits<allocator_type>::destroy(_alloc, std::addressof(*result));
            std::allocator_traits<allocator_type>::construct(_alloc, std::addressof(*result), value);
        }

        return result;
    }

    iterator _insert_aux_front(difference_type index, size_type count, const T& value)
    {
        bool front_has_space = _start._cur - _start._first >= count;
        if (!front_has_space) {
            size_type buf_sz = buffer_size();
            size_type need_spaces = count - (_start._cur - _start._first);
            size_type need_blocks = (need_spaces + buf_sz - 1) / buf_sz;

            if (_start._node - _map < need_blocks) {
                _reallocate_map(need_blocks, 0);
            }

            for (size_type i = 0; i < need_blocks; ++i) {
                pointer* new_node = _start._node - i - 1;
                *new_node = std::allocator_traits<allocator_type>::allocate(_alloc, buf_sz);
            }

            _start._node -= need_blocks;
            _start._first = *_start._node;
            _start._last = _start._first + buf_sz;
            _start._cur = _start._last - need_spaces % buf_sz;
            if (_start._cur == _start._last) {
                _start._cur = _start._first;
            }
        }
        else {
            _start._cur -= count;
        }

        iterator front_pos = begin();
        iterator old_front = front_pos + count;
        iterator target = front_pos + index + count;

        for (iterator it = front_pos; it != old_front; ++it) {
            std::allocator_traits<allocator_type>::construct(_alloc, std::addressof(*it));
        }

        for (iterator it = old_front; it != target; ++it) {
            *(it - count) = std::move(*it);
        }

        iterator result = target - count;
        for (size_type i = 0; i < count; ++i) {
            if constexpr (std::is_trivially_copy_assignable_v<T>) {
                *(result + i) = value;
            }
            else {
                std::allocator_traits<allocator_type>::destroy(_alloc, std::addressof(*(result + i)));
                std::allocator_traits<allocator_type>::construct(_alloc, std::addressof(*(result + i)), value);
            }
        }
        return result;
    }

    template <typename InputIt>
    iterator _insert_aux_front(difference_type index, InputIt first, InputIt last)
    {
        auto len = std::distance(first, last);
        if (len == 0) {
            return begin() + index;
        }

        bool front_has_space = _start._cur - _start._first >= len;

        if (!front_has_space) {
            size_type buf_sz = buffer_size();
            size_type need_spaces = len - (_start._cur - _start._first);
            size_type need_blocks = (need_spaces + buf_sz - 1) / buf_sz;

            if (_start._node - _map < need_blocks) {
                _reallocate_map(need_blocks, 0);
            }
            
            for (size_type i = 0; i < need_blocks; ++i) {
                pointer* new_node = _start._node - i - 1;
                *new_node = std::allocator_traits<allocator_type>::allocate(_alloc, buf_sz);
            }

            _start._node -= need_blocks;
            _start._first = *_start._node;
            _start._last = _start._first + buf_sz;
            _start._cur = _start._last - need_spaces % buf_sz;
            if (_start._cur == _start._last) {
                _start._cur = _start._first;
            }
        }
        else {
            _start._cur -= len;
        }

        iterator front_pos = begin();
        iterator old_front = front_pos + len;
        iterator target = front_pos + index + len;

        for (iterator it = front_pos; it != old_front; ++it) {
            std::allocator_traits<allocator_type>::construct(_alloc, std::addressof(*it));
        }

        for (iterator it = old_front; it != target; ++it) {
            *(it - len) = std::move(*it);
        }

        iterator result = target - len;
        for (size_type i = 0; i < len; ++i) {
            if constexpr (std::is_trivially_copy_assignable_v<T>) {
                *(result + i) = *first;
            }
            else {
                std::allocator_traits<allocator_type>::destroy(_alloc, std::addressof(*(result + i)));
                std::allocator_traits<allocator_type>::construct(_alloc, std::addressof(*(result + i)), *first);
            }
            ++first;
        }
        return result;
    }

    // 向后移动元素并插入
    iterator _insert_aux_back(difference_type index, const T& value)
    {
        bool back_has_space = _finish._cur != _finish._last;

        if (!back_has_space) {
            if (_finish._node == _map + _map_size - 1) {
                _reallocate_map(0, 1);
            }
            pointer* new_node = _finish._node + 1;
            *new_node = std::allocator_traits<allocator_type>::allocate(_alloc, buffer_size());
            _finish._node = new_node;
            _finish._first = *new_node;
            _finish._last = _finish._first + buffer_size();
            _finish._cur = _finish._first;
        }

        iterator back_pos = end();
        iterator old_back = back_pos - 1;
        iterator target = begin() + index;

        std::allocator_traits<allocator_type>::construct(_alloc, std::addressof(*back_pos));
        ++_finish._cur;

        while (old_back != target) {
            *back_pos = std::move(*old_back);
            --back_pos;
            --old_back;
        }

        iterator result = target;
        if constexpr (std::is_trivially_copy_assignable_v<T>) {
            *result = value;
        } else {
            std::allocator_traits<allocator_type>::destroy(_alloc, std::addressof(*result));
            std::allocator_traits<allocator_type>::construct(_alloc, std::addressof(*result), value);
        }

        return result;
    }

    iterator _insert_aux_back(difference_type index, size_type count, const T& value)
    {
        bool back_has_space = _finish._last - _finish._cur >= count;
        if (!back_has_space) {
            size_type buf_sz = buffer_size();
            size_type need_spaces = count - (_finish._last - _finish._cur);
            size_type need_blocks = (need_spaces + buf_sz - 1) / buf_sz;

            if (_map + _map_size - 1 - _finish._node < need_blocks) {
                _reallocate_map(0, need_blocks);
            }

            for (size_type i = 0; i < need_blocks; ++i) {
                pointer* new_node = _finish._node + i + 1;
                *new_node = std::allocator_traits<allocator_type>::allocate(_alloc, buf_sz);
            }

            _finish._node += need_blocks;
            _finish._first = *_finish._node;
            _finish._last = _finish._first + buf_sz;
            _finish._cur = _finish._first + need_spaces % buf_sz;
            // 当need_spaces是buf_sz的整数倍时，_finish._cur会指向块的起始位置，这是正确的
        }
        else {
            _finish._cur += count;
        }

        iterator back_pos = end();
        iterator old_back = back_pos - count;
        iterator target = begin() + index;

        // 从前向后在新空间构造默认对象
        for (iterator it = old_back; it != back_pos; ++it) {
            std::allocator_traits<allocator_type>::construct(_alloc, std::addressof(*it));
        }

        // 从后向前移动元素，注意起始位置和终止条件
        for (iterator it = old_back - 1; it >= target; --it) {
            *(it + count) = std::move(*it);
        }

        iterator result = target;
        for (size_type i = 0; i < count; ++i) {
            if constexpr (std::is_trivially_copy_assignable_v<T>) {
                *(result + i) = value;
            }
            else {
                std::allocator_traits<allocator_type>::destroy(_alloc, std::addressof(*(result + i)));
                std::allocator_traits<allocator_type>::construct(_alloc, std::addressof(*(result + i)), value);
            }
        }
        return result;
    }

    template <typename InputIt>
    iterator _insert_aux_back(difference_type index, InputIt first, InputIt last)
    {
        auto len = std::distance(first, last);
        if (len == 0) {
            return begin() + index;
        }

        bool back_has_space = _finish._last - _finish._cur >= len;
        if (!back_has_space) {
            size_type buf_sz = buffer_size();
            size_type need_spaces = len - (_finish._last - _finish._cur);
            size_type need_blocks = (need_spaces + buf_sz - 1) / buf_sz;

            if (_map + _map_size - 1 - _finish._node < need_blocks) {
                _reallocate_map(0, need_blocks);
            }
            
            for (size_type i = 0; i < need_blocks; ++i) {
                pointer* new_node = _finish._node + i + 1;
                *new_node = std::allocator_traits<allocator_type>::allocate(_alloc, buf_sz);
            }

            _finish._node += need_blocks;
            _finish._first = *_finish._node;    
            _finish._last = _finish._first + buf_sz;
            _finish._cur = _finish._first + need_spaces % buf_sz;
        }
        else {
            _finish._cur += len;
        }

        iterator back_pos = end();
        iterator old_back = back_pos - len;
        iterator target = begin() + index;

        for (iterator it = old_back; it != back_pos; ++it) {
            std::allocator_traits<allocator_type>::construct(_alloc, std::addressof(*it));
        }

        for (iterator it = old_back - 1; it >= target; --it) {
            *(it + len) = std::move(*it);
        }

        iterator result = target;
        for (size_type i = 0; i < len; ++i) {
            if constexpr (std::is_trivially_copy_assignable_v<T>) {
                *(result + i) = *first;
            }
            else {
                std::allocator_traits<allocator_type>::destroy(_alloc, std::addressof(*(result + i)));
                std::allocator_traits<allocator_type>::construct(_alloc, std::addressof(*(result + i)), *first);
            }
            ++first;
        }
        return result;
    }

    template <typename... Args>
    iterator _emplace_aux(const_iterator pos, Args&&... args)
    {
        difference_type index = pos - begin();

        if (index < size() / 2) {
            emplace_front(std::forward<Args>(args)...);
            std::rotate(begin(), begin() + 1, begin() + index + 1);
        }
        else {
            emplace_back(std::forward<Args>(args)...);
            std::rotate(begin() + index, end() - 1, end());
        }

        return begin() + index;
    }

public:
    class iterator {
        // 让 const_iterator 可以访问 iterator 的私有成员
        friend class const_iterator;

        // 让 deque 可以访问 iterator 的私有成员
        friend class deque<T,Alloc>;
    public: 
        using iterator_category = std::random_access_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = T*;
        using reference = T&; 
        using map_pointer = pointer*; // 指向 map_ 上块指针数组的指针

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
        iterator operator++(int) noexcept {
            iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        // 前缀 --
        iterator& operator--() noexcept {
            if (_cur == _first) {
                --_node;
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
        map_pointer _node; // 指向_map 上对应块指针的地址

        static size_type _buffer_size() {
            return deque::buffer_size();
        }
    };

    class const_iterator {
        // 让 deque 可以访问 iterator 的私有成员
        friend class deque<T,Alloc>;
    public:
        using iterator_category = std::random_access_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = const T*;
        using reference = const T&;
        using map_pointer = typename deque::pointer*;

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
        const_iterator operator++(int) noexcept {
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
        map_pointer _node;

        static size_type _buffer_size() {
            return deque::buffer_size();
        }
    };

    class reverse_iterator
    {
        // 让 const_reverse_iterator 可以访问 reverse_iterator 的私有成员
        friend class const_reverse_iterator;

        // 让 deque 可以访问 reverse_iterator 的私有成员
        friend class deque<T, Alloc>;

    public:
        using iterator_category = std::random_access_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = T*;
        using reference = T&;
        using map_pointer = pointer*;

    public:
        reverse_iterator() noexcept
            : _cur(nullptr), _first(nullptr), _last(nullptr), _node(nullptr) {}
        reverse_iterator(pointer cur, pointer first, pointer last, map_pointer node) noexcept
            : _cur(cur), _first(first), _last(last), _node(node) {}
        

        reference operator*() const noexcept { return *_cur; }
        pointer operator->() const noexcept { return _cur; }

        // 前缀++
        reverse_iterator& operator++() noexcept
        {
            if (_cur == _first) {
                --_node;
                _first = *_node;
                _last = _first + static_cast<difference_type>(_buffer_size());
                _cur = _last - 1;
            }
            else {
                --_cur;
            }
            return *this;
        }

        // 后缀++
        reverse_iterator operator++(int) noexcept
        {
            reverse_iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        // 前缀--
        reverse_iterator& operator--() noexcept
        {
            if (_cur == _last - 1) {
                ++_node;
                _first = *_node;
                _last = _first + static_cast<difference_type>(_buffer_size());
                _cur = _first;
            }
            else {
                ++_cur;
            }
            return *this;
        }

        // 后缀--
        reverse_iterator operator--(int) noexcept
        {
            reverse_iterator tmp = *this;
            --(*this);
            return tmp;
        }

        // += n
        reverse_iterator& operator+=(difference_type n)
        {
            const difference_type buf = static_cast<difference_type>(_buffer_size());
            // 对于反向迭代器，+= n 等价于正向迭代器的 -= n
            difference_type offset = (_cur - _first) - n;
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
        reverse_iterator operator+(difference_type n) const
        {
            reverse_iterator tmp = *this;
            return tmp += n;
        }

        // -= n
        reverse_iterator& operator-=(difference_type n)
        {
            return *this += -n;
        }

        // - n
        reverse_iterator operator-(difference_type n) const
        {
            reverse_iterator tmp = *this;
            return tmp -= n;
        }

        // - reverse_iterator
        difference_type operator-(const reverse_iterator& other) const noexcept
        {
            difference_type block_diff = other._node - _node;
            difference_type cur_diff = (other._cur - other._first) - (_cur - _first);
            return block_diff * static_cast<difference_type>(_buffer_size()) + cur_diff;
        }

        // [] 运算符
        reference operator[](difference_type n) const noexcept
        {
            return *(*this + n);
        }

        // 关系比较
        bool operator==(const reverse_iterator& other) const noexcept
        {
            return _cur == other._cur;
        }
        bool operator!=(const reverse_iterator& other) const noexcept
        {
            return _cur != other._cur;
        }
        bool operator<(const reverse_iterator& other) const noexcept
        {
            return (_node == other._node)
                    ? (_cur > other._cur)
                    : (_node > other._node);
        }
        bool operator>(const reverse_iterator& other) const noexcept
        {
            return other < *this;
        }
        bool operator<=(const reverse_iterator& other) const noexcept
        {
            return !(*this > other);
        }
        bool operator>=(const reverse_iterator& other) const noexcept
        {
            return !(*this < other);
        }

    private:
        pointer _cur;
        pointer _first;
        pointer _last;
        map_pointer _node;

        static size_type _buffer_size() {
            return deque::buffer_size();
        }
    };

    class const_reverse_iterator
    {
        friend class reverse_iterator;
        friend class deque<T, Alloc>;

    public:
        using iterator_category = std::random_access_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = const T*;
        using reference = const T&;
        using map_pointer = typename deque::pointer*;

    public:
        const_reverse_iterator() noexcept
            : _cur(nullptr), _first(nullptr), _last(nullptr), _node(nullptr) {}
        const_reverse_iterator(pointer cur, pointer first, pointer last, map_pointer node) noexcept
            : _cur(cur), _first(first), _last(last), _node(node) {}
        const_reverse_iterator(const reverse_iterator& it) noexcept 
            : _cur(it._cur), _first(it._first), _last(it._last), _node(it._node) {}

        reference operator*() const noexcept { return *_cur; }
        pointer operator->() const noexcept { return _cur; }

        const_reverse_iterator& operator++() noexcept {
            if (_cur == _first) {
                --_node;
                _first = *_node;
                _last = _first + _buffer_size();
                _cur = _last - 1;
            }
            else {
                --_cur;
            }
            return *this;
        }

        const_reverse_iterator operator++(int) noexcept {
            const_reverse_iterator tmp = *this;
            ++*this;
            return tmp;
        }

        const_reverse_iterator& operator--() noexcept {
            if (_cur == _last - 1) {
                ++_node;
                _first = *_node;
                _last = _first + _buffer_size();
                _cur = _first;
            }
            else {
                ++_cur;
            }
            return *this;
        }

        const_reverse_iterator operator--(int) noexcept {
            const_reverse_iterator tmp = *this;
            --*this;
            return tmp;
        }

        const_reverse_iterator& operator+=(difference_type n) {
            const difference_type buf = static_cast<difference_type>(_buffer_size());
            difference_type offset = (_cur - _first) - n;
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

        const_reverse_iterator operator+(difference_type n) const {
            const_reverse_iterator tmp = *this;
            return tmp += n;
        }

        const_reverse_iterator& operator-=(difference_type n) {
            return *this += -n;
        }

        const_reverse_iterator operator-(difference_type n) const {
            const_reverse_iterator tmp = *this;
            return tmp -= n;
        }

        difference_type operator-(const const_reverse_iterator& other) const noexcept {
            difference_type block_diff = other._node - _node;
            difference_type cur_diff = (other._cur - other._first) - (_cur - _first);
            return block_diff * static_cast<difference_type>(_buffer_size()) + cur_diff;
        }

        reference operator[](difference_type n) const noexcept {
            return *(*this + n);
        }

        bool operator==(const const_reverse_iterator& other) const noexcept {
            return _cur == other._cur;
        }
        bool operator!=(const const_reverse_iterator& other) const noexcept {
            return _cur != other._cur;
        }
        bool operator<(const const_reverse_iterator& other) const noexcept {
            return (_node == other._node)
                    ? (_cur > other._cur)
                    : (_node > other._node);
        }
        bool operator>(const const_reverse_iterator& other) const noexcept {
            return other < *this;
        }
        bool operator<=(const const_reverse_iterator& other) const noexcept {
            return !(*this > other);
        }
        bool operator>=(const const_reverse_iterator& other) const noexcept {
            return !(*this < other);
        }

    private:
        pointer _cur;
        pointer _first;
        pointer _last;
        map_pointer _node;

        static size_type _buffer_size() {
            return deque::buffer_size();
        }
    };

    // 添加反向迭代器相关成员函数
    reverse_iterator rbegin() noexcept {
        if (empty()) {
            return reverse_iterator(nullptr, nullptr, nullptr, nullptr);
        }
        return reverse_iterator(_finish._cur - 1, _finish._first, _finish._last, _finish._node);
    }

    reverse_iterator rend() noexcept {
        if (empty()) {
            return reverse_iterator(nullptr, nullptr, nullptr, nullptr);
        }
        return reverse_iterator(_start._cur - 1, _start._first, _start._last, _start._node);
    }

    const_reverse_iterator rbegin() const noexcept {
        if (empty()) {
            return const_reverse_iterator(nullptr, nullptr, nullptr, nullptr);
        }
        return const_reverse_iterator(_finish._cur - 1, _finish._first, _finish._last, _finish._node);
    }

    const_reverse_iterator rend() const noexcept {
        if (empty()) {
            return const_reverse_iterator(nullptr, nullptr, nullptr, nullptr);
        }
        return const_reverse_iterator(_start._cur - 1, _start._first, _start._last, _start._node);
    }

    const_reverse_iterator crbegin() const noexcept {
        return rbegin();
    }

    const_reverse_iterator crend() const noexcept {
        return rend();
    }

    // 添加 empty() 函数
    bool empty() const noexcept {
        return _start == _finish;
    }
};
}

#endif // MYSTL_DEQUE_H