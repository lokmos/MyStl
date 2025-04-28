#ifndef MYSTL_MEMORY_H
#define MYSTL_MEMORY_H

#include <iterator>
#include <type_traits>
#include <memory>
#include <cstring>
#include <utility>

namespace mystl
{
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

    // Uninitialized copy of n elements from [first, first+n) into uninitialized memory starting at result.
    // Returns iterator past the last constructed element.
    //
    // Special‐case for contiguous pointers + trivially copyable T: bulk memcpy.
    // Otherwise, uses allocator_traits::construct + strong exception safety.
    template <typename Alloc, typename InputIt, typename Size, typename ForwardIt>
    ForwardIt uninitialized_copy_n(Alloc& alloc, InputIt first, Size n, ForwardIt result)
    {
        using T = typename std::iterator_traits<ForwardIt>::value_type;
        ForwardIt cur = result;

        if constexpr (std::is_pointer_v<InputIt> && 
                      std::is_pointer_v<ForwardIt> &&
                      std::is_trivially_copyable_v<T>) 
        {
            std::memcpy(result, first, n * sizeof(T));
            return result + n;
        }
        else {
            Size constructed = 0;
            try {
                for (; constructed < n; ++constructed, ++first, ++cur) {
                    std::allocator_traits<Alloc>::construct(alloc, std::addressof(*cur), *first);
                }
                return cur;
            } catch (...) {
                destroy_n(alloc, result, constructed);
                throw;
            }
        }
    }

    // Uninitialized move of n elements from [first, first+n) into uninitialized memory at result.
    // Returns iterator past the last constructed element.
    //
    // Optimizes for contiguous pointers and trivially copyable T via a single memmove.
    // Otherwise, does per-element placement-new with move, and on exception rolls back.
    template <typename Alloc, typename InputIt, typename Size, typename ForwardIt>
    ForwardIt uninitialized_move_n(Alloc& alloc, InputIt first, Size n, ForwardIt result)
    {
        using T = typename std::iterator_traits<ForwardIt>::value_type;
        ForwardIt cur = result;

        if constexpr (std::is_pointer_v<InputIt> &&
                      std::is_pointer_v<ForwardIt> &&
                      std::is_trivially_copyable_v<T> )
        {   
            // contiguous & trivial: bulk move via memmove
            std::memmove(result, first, n * sizeof(T));
            return result + n;
        }
        else {
            Size constructed = 0;
            try {
                for (; constructed < n; ++constructed, ++first, ++cur) {
                    std::allocator_traits<Alloc>::construct(alloc, std::addressof(*cur), std::move(*first));
                };
                return cur;
            } catch (...) {
                // Rollback on exception: destroy all constructed elements
                destroy_n(alloc, result, constructed);
                throw;
            }
        }
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
}

#endif // MYSTL_MEMORY_H