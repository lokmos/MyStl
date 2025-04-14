#ifndef MYSTL_ITERATOR_H
#define MYSTL_ITERATOR_H

#include <cstddef>

namespace mystl
{

// =========================================
// 1. Iterator Category Tags
// =========================================

struct input_iterator_tag {};
struct output_iterator_tag {};
struct forward_iterator_tag : public input_iterator_tag {};
struct bidirectional_iterator_tag : public forward_iterator_tag {};
struct random_access_iterator_tag : public bidirectional_iterator_tag {};
struct contiguous_iterator_tag : public random_access_iterator_tag {};

// =========================================
// 2. iterator_base 模板
// =========================================

template <typename Category, typename T, typename Distance = std::ptrdiff_t,
          typename Pointer = T*, typename Reference = T&>
struct iterator_base
{
    using iterator_category = Category;
    using value_type = T;
    using difference_type = Distance;
    using pointer = Pointer;
    using reference = Reference;
};

// =========================================
// 3. iterator_traits 萃取类
// =========================================

template <typename Iter>
struct iterator_traits
{
    using iterator_category = typename Iter::iterator_category;
    using value_type = typename Iter::value_type;
    using difference_type = typename Iter::difference_type;
    using pointer = typename Iter::pointer;
    using reference = typename Iter::reference;
};

// 针对原始指针的特化
template <typename T>
struct iterator_traits<T*>
{
    using iterator_category = random_access_iterator_tag;
    using value_type = T;
    using difference_type = std::ptrdiff_t;
    using pointer = T*;
    using reference = T&;
};

template <typename T>
struct iterator_traits<const T*>
{
    using iterator_category = random_access_iterator_tag;
    using value_type = T;
    using difference_type = std::ptrdiff_t;
    using pointer = const T*;
    using reference = const T&;
};

// output_iterator_tag
// 只能写入，不能读取，不能比较，不能多次遍历
template <typename T>
class output_iterator
{
public:
    using iterator_category = output_iterator_tag;
    using iterator_concept = output_iterator_tag;
    using value_type = void;
    using difference_type = void;
    using pointer = void;
    using reference = void;
    using self = output_iterator;

    explicit output_iterator(T *ptr = nullptr) : ptr_(ptr) {}

    // 解引用返回自身，用于写操作
    self &operator*() { return *this; };

    // 写入操作：*it = value
    self &operator=(const T &value)
    {
        *ptr_ = value;
        return *this;
    } 

    // 前置递增
    self& operator++() {
        ++ptr_;
        return *this;
    }

    // 后置递增
    self operator++(int) {
        self tmp = *this;
        ++ptr_;
        return tmp;
    }

private:
    T *ptr_; // 指向要写入的值的指针
};

// input_iterator_tag
// 支持只读访问、一次遍历、== 比较
template <typename T, bool IsConst>
class input_iterator {
public:
    using iterator_category = input_iterator_tag;
    using iterator_concept  = input_iterator_tag;
    using value_type        = T;
    using difference_type   = std::ptrdiff_t;
    using pointer           = std::conditional_t<IsConst, const T*, T*>;
    using reference         = std::conditional_t<IsConst, const T&, T&>;
    
    using self = input_iterator<T, IsConst>;

    explicit input_iterator(pointer ptr = nullptr) : ptr_(ptr) {}
    // 应当支持从 非const 构造出 const
    template <bool B,
            std::enable_if_t<IsConst && !B, int> = 0>
    input_iterator(const input_iterator<T, B>& other)
        : ptr_(other.ptr_) {}


    reference operator*() const { return *ptr_; }
    pointer operator->() const { return ptr_; }

    self& operator++() {
        ++ptr_;
        return *this;
    }

    self operator++(int) {
        self tmp = *this;
        ++ptr_;
        return tmp;
    }

    bool operator==(const self& other) const { return ptr_ == other.ptr_; }
    bool operator!=(const self& other) const { return ptr_ != other.ptr_; }

private:
    pointer ptr_;
    template <typename, bool>
    friend class input_iterator;
};


template <typename T, bool IsConst>
class forward_iterator
{
public:
    using iterator_category = forward_iterator_tag;
    using iterator_concept  = forward_iterator_tag;
    using value_type = T;
    using difference_type = std::ptrdiff_t;
    using pointer = std::conditional_t<IsConst, const T*, T*>;
    using reference = std::conditional_t<IsConst, const T&, T&>;

    using self = forward_iterator<T, IsConst>;

    explicit forward_iterator(pointer ptr = nullptr) : ptr_(ptr) {}
    // 应当支持从 非const 构造出 const
    template <bool B,
        std::enable_if_t<IsConst && !B, int> = 0>
    forward_iterator(const forward_iterator<T, B>& other) : ptr_(other.ptr_) {}

    reference operator*() const { return *ptr_; }
    pointer operator->() const { return ptr_; }

    self & operator++() {
        ++ptr_;
        return *this;
    }

    self operator++(int) {
        self tmp = *this;
        ++ptr_;
        return tmp;
    }

    bool operator==(const self& other) const { return ptr_ == other.ptr_; }
    bool operator!=(const self& other) const { return ptr_ != other.ptr_; }

private:
    pointer ptr_;
    template <typename, bool>
    friend class forward_iterator;
};

template <typename T, bool IsConst>
class bidirectional_iterator
{
public:
    using iterator_tag = bidirectional_iterator_tag;
    using iterator_concept = bidirectional_iterator_tag;
    using value_type = T;
    using difference_type = std::ptrdiff_t;
    using pointer = std::conditional_t<IsConst, const T*, T*>;
    using reference = std::conditional_t<IsConst, const T&, T&>;

    using self = bidirectional_iterator<T, IsConst>;

    explicit bidirectional_iterator(pointer ptr = nullptr) : ptr_(ptr) {}
    // 应当支持从 非const 构造出 const
    template <bool B,
            std::enable_if_t<IsConst && !B, int> = 0>
    bidirectional_iterator(const bidirectional_iterator<T, B>& other) : ptr_(other.ptr_) {} 

    reference operator*() const { return *ptr_; }
    pointer operator->() const { return ptr_; }

    self& operator++() {
        ++ptr_;
        return *this;
    }

    self operator++(int) {
        self tmp = *this;
        ++ptr_;
        return tmp;
    }

    self& operator--() {
        --ptr_;
        return *this;
    }

    self operator--(int) {
        self tmp = *this;
        --ptr_;
        return *this;
    }

    bool operator==(const self& other) const { return ptr_ == other.ptr_; }
    bool operator!=(const self& other) const { return ptr_ != other.ptr_; }

private:
    pointer ptr_;
    template <typename, bool>
    friend class bidirectional_iterator;
};

template <typename T, bool IsConst>
class random_access_iterator
{
public:
    using iterator_category = random_access_iterator_tag;
    using iterator_concept  = random_access_iterator_tag;
    using value_type = T;
    using difference_type = std::ptrdiff_t;
    using pointer = std::conditional_t<IsConst, const T*, T*>;
    using reference = std::conditional_t<IsConst, const T&, T&>;

    using self = random_access_iterator<T, IsConst>;

    explicit random_access_iterator(pointer ptr = nullptr) : ptr_(ptr) {}
    // 应当支持从 非const 构造出 const
    template <bool B,
            std::enable_if_t<IsConst && !B, int> = 0>
    random_access_iterator(const random_access_iterator<T, B>& other) : ptr_(other.ptr_) {}

    reference operator*() const { return *ptr_; }
    pointer operator->() const { return ptr_; }

    self& operator++() {
        ++ptr_;
        return *this;
    }

    self operator++(int) {
        self tmp = *this;
        ++ptr_;
        return tmp;
    }

    self& operator--() {
        --ptr_;
        return *this;
    }

    self operator--(int) {
        self tmp = *this;
        --ptr_;
        return tmp;
    }

    self& operator+=(difference_type n) {
        ptr_ += n;
        return *this;
    }

    self& operator-=(difference_type n) {
        ptr_ -= n;
        return *this;
    }

    self operator+(difference_type n) const {
        return self(ptr_ + n);
    }

    self operator-(difference_type n) const {
        return self(ptr_ - n);
    }

    difference_type operator-(const self& other) const {
        return ptr_ - other.ptr_;
    }

    reference operator[](difference_type n) const {
        return ptr_[n];
    }

    bool operator==(const self& other) const { return ptr_ == other.ptr_; }
    bool operator!=(const self& other) const { return ptr_ != other.ptr_; }
    bool operator<(const self& other) const { return ptr_ < other.ptr_; }
    bool operator>(const self& other) const { return ptr_ > other.ptr_; }
    bool operator<=(const self& other) const { return ptr_ <= other.ptr_; }
    bool operator>=(const self& other) const { return ptr_ >= other.ptr_; }

private:
    pointer ptr_;
    template <typename, bool>
    friend class random_access_iterator;
};

template <typename T, bool IsConst>
class contiguous_iterator {
public:
    using value_type        = T;
    using difference_type   = std::ptrdiff_t;
    using pointer           = std::conditional_t<IsConst, const T*, T*>;
    using reference         = std::conditional_t<IsConst, const T&, T&>;

    // C++20 分类支持（用于 ranges）
    using iterator_category = mystl::random_access_iterator_tag;
    using iterator_concept  = mystl::contiguous_iterator_tag;

    using self = contiguous_iterator<T, IsConst>;

    explicit contiguous_iterator(pointer ptr = nullptr) : ptr_(ptr) {}

    // 允许从 non-const 转换为 const
    template <bool B,
            std::enable_if_t<IsConst && !B, int> = 0>
    contiguous_iterator(const contiguous_iterator<T, B>& other)
        : ptr_(other.ptr_) {}

    // 解引用与访问
    reference operator*() const { return *ptr_; }
    pointer operator->() const { return ptr_; }

    // 递增递减
    self& operator++() { ++ptr_; return *this; }
    self operator++(int) { self tmp = *this; ++(*this); return tmp; }
    self& operator--() { --ptr_; return *this; }
    self operator--(int) { self tmp = *this; --(*this); return tmp; }

    // 随机访问
    self& operator+=(difference_type n) { ptr_ += n; return *this; }
    self& operator-=(difference_type n) { ptr_ -= n; return *this; }
    self operator+(difference_type n) const { return self(ptr_ + n); }
    self operator-(difference_type n) const { return self(ptr_ - n); }
    difference_type operator-(const self& other) const { return ptr_ - other.ptr_; }

    reference operator[](difference_type n) const { return ptr_[n]; }

    // 比较
    bool operator==(const self& other) const { return ptr_ == other.ptr_; }
    bool operator!=(const self& other) const { return ptr_ != other.ptr_; }
    bool operator<(const self& other) const { return ptr_ < other.ptr_; }
    bool operator>(const self& other) const { return ptr_ > other.ptr_; }
    bool operator<=(const self& other) const { return ptr_ <= other.ptr_; }
    bool operator>=(const self& other) const { return ptr_ >= other.ptr_; }

    // 取底层原始指针
    pointer base() const { return ptr_; }

private:
    pointer ptr_;
    template <typename, bool> 
    friend class contiguous_iterator;
};

template <typename Iter>
class reverse_iterator
{
public:
    using iterator_type = Iter;
    using iterator_category = typename iterator_traits<Iter>::iterator_category;
    using value_type = typename iterator_traits<Iter>::value_type;
    using difference_type = typename iterator_traits<Iter>::difference_type;
    using pointer = typename iterator_traits<Iter>::pointer;
    using reference = typename iterator_traits<Iter>::reference;

    using self = reverse_iterator<Iter>;

    reverse_iterator() : current_() {}
    explicit reverse_iterator(iterator_type it) : current_(it) {}

    // 拷贝构造支持 const → non-const
    template <typename U,
              std::enable_if_t<std::is_convertible_v<U, Iter>, int> = 0>
    reverse_iterator(const reverse_iterator<U>& other)
        : current_(other.base()) {}

    Iter base() const { return current_; }

    reference operator*() const {
        Iter tmp = current_;
        return *--tmp;
    }

    pointer operator->() const {
        return std::addressof(operator*());
    }

    self& operator++() {
        --current_;
        return *this;
    }

    self operator++(int) {
        self tmp = *this;
        --current_;
        return tmp;
    }

    self& operator--() {
        ++current_;
        return *this;
    }

    self operator--(int) {
        self tmp = *this;
        ++current_;
        return tmp;
    }

    self operator+(difference_type n) const {
        return self(current_ - n);
    }

    self& operator+=(difference_type n) {
        current_ -= n;
        return *this;
    }

    self operator-(difference_type n) const {
        return self(current_ + n);
    }

    self& operator-=(difference_type n) {
        current_ += n;
        return *this;
    }

    reference operator[](difference_type n) const {
        return *(*this + n);
    }

    friend bool operator==(const reverse_iterator& lhs, const reverse_iterator& rhs) {
        return lhs.current_ == rhs.current_;
    }

    friend bool operator!=(const reverse_iterator& lhs, const reverse_iterator& rhs) {
        return lhs.current_ != rhs.current_;
    }

    friend bool operator<(const reverse_iterator& lhs, const reverse_iterator& rhs) {
        return lhs.current_ > rhs.current_;
    }

    friend bool operator<=(const reverse_iterator& lhs, const reverse_iterator& rhs) {
        return lhs.current_ >= rhs.current_;
    }

    friend bool operator>(const reverse_iterator& lhs, const reverse_iterator& rhs) {
        return lhs.current_ < rhs.current_;
    }

    friend bool operator>=(const reverse_iterator& lhs, const reverse_iterator& rhs) {
        return lhs.current_ <= rhs.current_;
    }

    friend difference_type operator-(const reverse_iterator& lhs, const reverse_iterator& rhs) {
        return rhs.current_ - lhs.current_;
    }

    friend reverse_iterator operator+(difference_type n, const reverse_iterator& it) {
        return it + n;
    }

private:
    Iter current_;
};

} // namespace mystl


#endif // MYSTL_ITERATOR_H