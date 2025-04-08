#ifndef MYSTL_ALLOCATOR_H
#define MYSTL_ALLOCATOR_H

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
    };
}

#endif