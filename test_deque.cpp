// test_deque.cpp
#include "mystl_deque.h"
#include <gtest/gtest.h>
#include <list>
#include <type_traits>
#include <vector>

using mystl::deque;

// 一个简单的 stateful allocator，用来测试带 alloc 的构造分支
template <typename T> struct CountingAllocator {
  using value_type = T;
  int id;
  CountingAllocator(int id = 0) : id(id) {}
  template <typename U>
  CountingAllocator(const CountingAllocator<U> &o) noexcept : id(o.id) {}
  T *allocate(std::size_t n) {
    return static_cast<T *>(::operator new(n * sizeof(T)));
  }
  void deallocate(T *p, std::size_t) noexcept { ::operator delete(p); }
  template <typename U, typename... A> void construct(U *p, A &&...a) {
    ::new ((void *)p) U(std::forward<A>(a)...);
  }
  template <typename U> void destroy(U *p) noexcept { p->~U(); }
  bool operator==(CountingAllocator const &o) const noexcept {
    return id == o.id;
  }
  bool operator!=(CountingAllocator const &o) const noexcept {
    return id != o.id;
  }
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
  for (auto &v : d)
    EXPECT_EQ(v, 123);
}

TEST(DequeTest, CountFillCtorZero) {
  deque<int> d(0, 999);
  EXPECT_EQ(d.size(), 0u);
  EXPECT_TRUE(d.begin() == d.end());
}

TEST(DequeTest, RangeCtorFromVector) {
  std::vector<int> src = {1, 2, 3, 4};
  deque<int> d(src.begin(), src.end());
  EXPECT_EQ(d.size(), src.size());
  auto it = d.begin();
  for (auto v : src) {
    EXPECT_EQ(*it++, v);
  }
}

TEST(DequeTest, RangeCtorFromArray) {
  int arr[] = {10, 20, 30};
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
  for (auto &v : d)
    EXPECT_EQ(v, 4);
}

TEST(DequeTest, CopyCtor) {
  deque<int> a = {5, 6, 7, 8};
  deque<int> b(a);
  EXPECT_EQ(b.size(), a.size());
  for (size_t i = 0; i < a.size(); ++i) {
    EXPECT_EQ(b[i], a[i]);
  }
}

TEST(DequeTest, MoveCtorStealsWhenAllocEqual) {
  deque<int> a = {1, 2, 3};
  // 记录 move 前第一个元素所在内存地址
  int *raw_ptr = &*a.begin();
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
  deque<int, CountingAllocator<int>> a_src({9, 8, 7}, alloc_old);
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
  deque<int, CountingAllocator<int>> a2_src({3, 1, 4, 1}, alloc_old);
  // 记录源底层第一个元素地址
  int *raw_ptr2 = &*a2_src.begin();
  // 用不同分配器 move 构造 → 深拷贝
  deque<int, CountingAllocator<int>> b(std::move(a2_src), alloc_new);
  EXPECT_EQ(b.size(), 4u);
  EXPECT_NE(&*b.begin(), raw_ptr2);
  // moved-from 对象应清空
  EXPECT_EQ(a2_src.size(), 0u);
  EXPECT_TRUE(a2_src.begin() == a2_src.end());
}

TEST(DequeTest, InitListCtor) {
  deque<int> d = {11, 22, 33, 44};
  EXPECT_EQ(d.size(), 4u);
  EXPECT_EQ(d[0], 11);
  EXPECT_EQ(d[3], 44);
}

TEST(DequeTest, InitListCtorEmpty) {
  deque<int> d({});
  EXPECT_EQ(d.size(), 0u);
}

TEST(DequeTest, AtThrows) {
  deque<int> d = {1, 2, 3};
  EXPECT_NO_THROW(d.at(2));
  EXPECT_THROW(d.at(3), std::out_of_range);
}

// 测试 assign 函数
TEST(DequeTest, AssignCountValue) {
  deque<int> d = {1, 2, 3};
  d.assign(5, 42);
  EXPECT_EQ(d.size(), 5u);
  for (auto &v : d)
    EXPECT_EQ(v, 42);
}

TEST(DequeTest, AssignCountValueEmpty) {
  deque<int> d;
  d.assign(3, 99);
  EXPECT_EQ(d.size(), 3u);
  for (auto &v : d)
    EXPECT_EQ(v, 99);
}

TEST(DequeTest, AssignRange) {
  std::vector<int> src = {10, 20, 30, 40};
  deque<int> d = {1, 2, 3};
  d.assign(src.begin(), src.end());
  EXPECT_EQ(d.size(), src.size());
  auto it = d.begin();
  for (auto v : src) {
    EXPECT_EQ(*it++, v);
  }
}

TEST(DequeTest, AssignInitializerList) {
  deque<int> d = {1, 2, 3};
  d.assign({7, 8, 9, 10});
  EXPECT_EQ(d.size(), 4u);
  EXPECT_EQ(d[0], 7);
  EXPECT_EQ(d[3], 10);
}

// 测试元素访问
TEST(DequeTest, OperatorBracket) {
  deque<int> d = {1, 2, 3, 4, 5};
  EXPECT_EQ(d[0], 1);
  EXPECT_EQ(d[2], 3);
  EXPECT_EQ(d[4], 5);
  d[2] = 33;
  EXPECT_EQ(d[2], 33);
}

TEST(DequeTest, At) {
  deque<int> d = {1, 2, 3};
  EXPECT_EQ(d.at(0), 1);
  EXPECT_EQ(d.at(2), 3);
  d.at(1) = 22;
  EXPECT_EQ(d.at(1), 22);
}

TEST(DequeTest, FrontBack) {
  deque<int> d = {1, 2, 3, 4};
  EXPECT_EQ(d.front(), 1);
  EXPECT_EQ(d.back(), 4);
  d.front() = 11;
  d.back() = 44;
  EXPECT_EQ(d.front(), 11);
  EXPECT_EQ(d.back(), 44);
}

// 测试迭代器
TEST(DequeTest, IteratorArithmetic) {
  deque<int> d = {1, 2, 3, 4, 5};
  auto it = d.begin();
  EXPECT_EQ(*(it + 2), 3);
  EXPECT_EQ(*(it + 4), 5);
  it += 3;
  EXPECT_EQ(*it, 4);
  it -= 2;
  EXPECT_EQ(*it, 2);
}

TEST(DequeTest, IteratorComparison) {
  deque<int> d = {1, 2, 3, 4};
  auto it1 = d.begin();
  auto it2 = d.begin() + 2;
  EXPECT_TRUE(it1 < it2);
  EXPECT_TRUE(it2 > it1);
  EXPECT_TRUE(it1 <= it2);
  EXPECT_TRUE(it2 >= it1);
  EXPECT_FALSE(it1 == it2);
  EXPECT_TRUE(it1 != it2);
}

TEST(DequeTest, IteratorDistance) {
  deque<int> d = {1, 2, 3, 4, 5};
  auto it1 = d.begin();
  auto it2 = d.begin() + 3;
  EXPECT_EQ(it2 - it1, 3);
  EXPECT_EQ(it1 - it2, -3);
}

// 测试 clear
TEST(DequeTest, Clear) {
  deque<int> d = {1, 2, 3, 4, 5};
  d.clear();
  EXPECT_EQ(d.size(), 0u);
  EXPECT_TRUE(d.begin() == d.end());
}

// 测试 emplace_back
TEST(DequeTest, EmplaceBack) {
  deque<int> d;
  d.emplace_back(1);
  EXPECT_EQ(d.size(), 1u);
  EXPECT_EQ(d.back(), 1);

  d.emplace_back(2);
  EXPECT_EQ(d.size(), 2u);
  EXPECT_EQ(d.back(), 2);
}

// 测试分配器传播
TEST(DequeTest, AllocatorPropagation) {
  CountingAllocator<int> alloc1(1), alloc2(2);
  deque<int, CountingAllocator<int>> d1({1, 2, 3}, alloc1);
  deque<int, CountingAllocator<int>> d2({4, 5, 6}, alloc2);

  // 测试拷贝赋值
  d1 = d2;
  EXPECT_EQ(d1.size(), d2.size());
  for (size_t i = 0; i < d1.size(); ++i) {
    EXPECT_EQ(d1[i], d2[i]);
  }

  // 测试移动赋值
  deque<int, CountingAllocator<int>> d3({7, 8, 9}, alloc1);
  d1 = std::move(d3);
  EXPECT_EQ(d1.size(), 3u);
  EXPECT_EQ(d1[0], 7);
  EXPECT_EQ(d1[2], 9);
}

// 测试边界条件
TEST(DequeTest, EdgeCases) {
  // 空容器操作
  deque<int> d;
  EXPECT_EQ(d.size(), 0u);
  EXPECT_TRUE(d.begin() == d.end());

  // 单元素容器
  d.emplace_back(1);
  EXPECT_EQ(d.size(), 1u);
  EXPECT_EQ(d.front(), d.back());

  // 大容量测试
  deque<int> large(1000);
  EXPECT_EQ(large.size(), 1000u);
  for (auto &v : large) {
    EXPECT_EQ(v, 0);
  }
}

// 测试反向迭代器
TEST(DequeTest, ReverseIterator) {
  deque<int> d = {1, 2, 3, 4, 5};
  auto rit = d.rbegin();
  EXPECT_EQ(*rit, 5);
  ++rit;
  EXPECT_EQ(*rit, 4);
  rit += 2;
  EXPECT_EQ(*rit, 2);
  --rit;
  EXPECT_EQ(*rit, 3);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
