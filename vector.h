#ifndef MYSTL_VECTOR_H
#define MYSTL_VECTOR_H

#include "allocator.h"
#include <memory>

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
    pointer _start;
    pointer _finish;
    pointer _end_of_storage;
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

template <typename T, typename Allocator = mystl::allocator<T>>
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
    // 注意这里，迭代器实际上是指向元素的指针
    using iterator        = pointer;
    using const_iterator  = const_pointer;

private:
    void _fill_initialize(size_type n, const T& value)
    {
        this->_start = this->_allocator.allocate(n);
        this->_finish = this->_start;
        this->_end_of_storage = this->_start + n; // 标准做法，不会预留空间

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

};
}

#endif