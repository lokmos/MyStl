// test_deque.cpp
#include <gtest/gtest.h>
#include "mystl_deque.h"
#include <vector>
#include <list>
#include <type_traits>

using mystl::deque;

// 一个简单的 stateful allocator，用来测试带 alloc 的构造分支
template<typename T>
struct CountingAllocator {
    using value_type = T;
    int id;
    CountingAllocator(int id = 0) : id(id) {}
    template<typename U> CountingAllocator(const CountingAllocator<U>& o) noexcept : id(o.id) {}
    T* allocate(std::size_t n) { return static_cast<T*>(::operator new(n * sizeof(T))); }
    void deallocate(T* p, std::size_t) noexcept { ::operator delete(p); }
    template<typename U, typename... A>
    void construct(U* p, A&&... a) { ::new((void*)p) U(std::forward<A>(a)...); }
    template<typename U> void destroy(U* p) noexcept { p->~U(); }
    bool operator==(CountingAllocator const& o) const noexcept { return id == o.id; }
    bool operator!=(CountingAllocator const& o) const noexcept { return id != o.id; }
};

TEST(DequeTest, DefaultCtorProducesEmpty) {
    deque<int> d;
    EXPECT_EQ(d.size(), 0u);
    EXPECT_TRUE(d.begin() == d.end());
}

TEST(DequeTest, AllocOnlyCtorProducesEmpty) {
    CountingAllocator<int> alloc(42);
    deque<int, CountingAllocator<int>> d(alloc);
    EXPECT_EQ(d.size(), 0u);
    EXPECT_TRUE(d.begin() == d.end());
}

TEST(DequeTest, CountDefaultCtor) {
    deque<int> d(5);
    EXPECT_EQ(d.size(), 5u);
    for (auto it = d.begin(); it != d.end(); ++it) {
        // int value-initialized → 0
        EXPECT_EQ(*it, 0);
    }
}

TEST(DequeTest, CountDefaultCtorZero) {
    deque<int> d(0);
    EXPECT_EQ(d.size(), 0u);
    EXPECT_TRUE(d.begin() == d.end());
}

TEST(DequeTest, CountFillCtor) {
    deque<int> d(7, 123);
    EXPECT_EQ(d.size(), 7u);
    for (auto& v : d) EXPECT_EQ(v, 123);
}

TEST(DequeTest, CountFillCtorZero) {
    deque<int> d(0, 999);
    EXPECT_EQ(d.size(), 0u);
    EXPECT_TRUE(d.begin() == d.end());
}

TEST(DequeTest, RangeCtorFromVector) {
    std::vector<int> src = {1,2,3,4};
    deque<int> d(src.begin(), src.end());
    EXPECT_EQ(d.size(), src.size());
    auto it = d.begin();
    for (auto v : src) {
        EXPECT_EQ(*it++, v);
    }
}

TEST(DequeTest, RangeCtorFromArray) {
    int arr[] = {10,20,30};
    deque<int> d(arr, arr + 3);
    EXPECT_EQ(d.size(), 3u);
    EXPECT_EQ(d[0], 10);
    EXPECT_EQ(d[1], 20);
    EXPECT_EQ(d[2], 30);
}

TEST(DequeTest, RangeCtorEmpty) {
    std::list<int> empty;
    deque<int> d(empty.begin(), empty.end());
    EXPECT_EQ(d.size(), 0u);
    EXPECT_TRUE(d.begin() == d.end());
}

TEST(DequeTest, RangeCtorIntegralDisambiguation) {
    // 传入两个整型，应当调用 count-fill ctor，而非 range
    deque<int> d(3, 4);
    EXPECT_EQ(d.size(), 3u);
    for (auto& v : d) EXPECT_EQ(v, 4);
}

TEST(DequeTest, CopyCtor) {
    deque<int> a = {5,6,7,8};
    deque<int> b(a);
    EXPECT_EQ(b.size(), a.size());
    for (size_t i = 0; i < a.size(); ++i) {
        EXPECT_EQ(b[i], a[i]);
    }
}

TEST(DequeTest, MoveCtorStealsWhenAllocEqual) {
    deque<int> a = {1,2,3};
    // 记录 move 前第一个元素所在内存地址
    int* raw_ptr = &*a.begin();
    deque<int> b(std::move(a));
    EXPECT_EQ(b.size(), 3u);
    // 如果偷走了底层存储，则地址应相同
    EXPECT_EQ(&*b.begin(), raw_ptr);
    EXPECT_EQ(a.size(), 0u);
    EXPECT_TRUE(a.begin() == a.end());
}

TEST(DequeTest, CopyCtorWithAllocator) {
    CountingAllocator<int> alloc_old(2), alloc_new(99);
    // 先构造带有老分配器的源对象
    deque<int, CountingAllocator<int>> a_src({9,8,7}, alloc_old);
    // 用带分配器的拷贝构造：源和目标分配器类型相同，但值不同
    deque<int, CountingAllocator<int>> b(a_src, alloc_new);
    EXPECT_EQ(b.size(), a_src.size());
    EXPECT_EQ(b[0], 9);
    EXPECT_EQ(b[1], 8);
    EXPECT_EQ(b[2], 7);
}

TEST(DequeTest, MoveCtorWithAllocatorDeepCopy) {
    CountingAllocator<int> alloc_old(2), alloc_new(1);
    // 构造带有老分配器的源对象
    deque<int, CountingAllocator<int>> a2_src({3,1,4,1}, alloc_old);
    // 记录源底层第一个元素地址
    int* raw_ptr2 = &*a2_src.begin();
    // 用不同分配器 move 构造 → 深拷贝
    deque<int, CountingAllocator<int>> b(std::move(a2_src), alloc_new);
    EXPECT_EQ(b.size(), 4u);
    EXPECT_NE(&*b.begin(), raw_ptr2);
    // moved-from 对象应清空
    EXPECT_EQ(a2_src.size(), 0u);
    EXPECT_TRUE(a2_src.begin() == a2_src.end());
}

TEST(DequeTest, InitListCtor) {
    deque<int> d = {11,22,33,44};
    EXPECT_EQ(d.size(), 4u);
    EXPECT_EQ(d[0], 11);
    EXPECT_EQ(d[3], 44);
}

TEST(DequeTest, InitListCtorEmpty) {
    deque<int> d({});
    EXPECT_EQ(d.size(), 0u);
}

TEST(DequeTest, AtThrows) {
    deque<int> d = {1,2,3};
    EXPECT_NO_THROW(d.at(2));
    EXPECT_THROW(d.at(3), std::out_of_range);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
