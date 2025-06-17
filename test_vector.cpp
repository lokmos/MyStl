#include "gtest/gtest.h"
#include "mystl_vector.h"
#include <vector>
#include <string>
#include <memory>
#include <iterator>
#include <list>

using mystl::vector;

// Helper to compare mystl::vector<T> to std::vector<T>
template <typename T>
void expect_equal(const vector<T>& mv, const std::vector<T>& sv) {
    ASSERT_EQ(mv.size(), sv.size());
    for (size_t i = 0; i < mv.size(); ++i) {
        EXPECT_EQ(mv[i], sv[i]);
    }
}

// ------------------ Value-initialize constructor ------------------
TEST(VectorOptTest, ValueInitNonZero) {
    vector<int> v(5);
    EXPECT_EQ(v.size(), 5u);
    for (size_t i = 0; i < v.size(); ++i) {
        EXPECT_EQ(v[i], 0);
    }
}

TEST(VectorOptTest, ValueInitZero) {
    vector<int> v(0);
    EXPECT_TRUE(v.empty());
}

// ------------------ Fill constructor ------------------
TEST(VectorOptTest, FillNonZero) {
    vector<int> v(4, 7);
    EXPECT_EQ(v.size(), 4u);
    for (size_t i = 0; i < v.size(); ++i) {
        EXPECT_EQ(v[i], 7);
    }
}

TEST(VectorOptTest, FillZero) {
    vector<int> v(0, 42);
    EXPECT_TRUE(v.empty());
}

// ------------------ Contiguous range constructor ------------------
TEST(VectorOptTest, ContiguousRangeNonZero) {
    int arr[] = {1,2,3,4,5};
    vector<int> v(arr, arr + 5);
    expect_equal(v, std::vector<int>{1,2,3,4,5});
}

TEST(VectorOptTest, ContiguousRangeZero) {
    int arr[] = {};
    vector<int> v(arr, arr);
    EXPECT_TRUE(v.empty());
}

// ------------------ Generic range constructor (non-trivial type) ------------------
TEST(VectorOptTest, GenericRangeNonZero) {
    std::vector<std::string> src = {"a", "bb", "ccc"};
    vector<std::string> v(src.begin(), src.end());
    expect_equal(v, src);
}

TEST(VectorOptTest, GenericRangeZero) {
    std::vector<std::string> src;
    vector<std::string> v(src.begin(), src.end());
    EXPECT_TRUE(v.empty());
}

// Test growth path: insert > initial capacity
TEST(VectorOptTest, GenericRangeGrowth) {
    std::vector<int> src;
    for (int i = 0; i < 10; ++i) src.push_back(i);
    vector<int> v(src.begin(), src.end());
    expect_equal(v, src);
}

// ------------------ Copy constructor ------------------
TEST(VectorOptTest, CopyNonEmpty) {
    vector<int> a = {5,6,7};
    vector<int> b(a);
    expect_equal(b, std::vector<int>{5,6,7});
}

TEST(VectorOptTest, CopyEmpty) {
    vector<int> a;
    vector<int> b(a);
    EXPECT_TRUE(b.empty());
}

// ------------------ Move constructor ------------------
TEST(VectorOptTest, MoveNonEmpty) {
    vector<int> a = {8,9};
    vector<int> b(std::move(a));
    expect_equal(b, std::vector<int>{8,9});
    EXPECT_TRUE(a.empty());
}

TEST(VectorOptTest, MoveEmpty) {
    vector<int> a;
    vector<int> b(std::move(a));
    EXPECT_TRUE(b.empty());
}

// ------------------ Move-only type via generic range ------------------
TEST(VectorOptTest, MoveOnlyGenericRange) {
    std::vector<std::unique_ptr<int>> src;
    src.push_back(std::make_unique<int>(1));
    src.push_back(std::make_unique<int>(2));
    auto mbegin = std::make_move_iterator(src.begin());
    auto mend = std::make_move_iterator(src.end());
    vector<std::unique_ptr<int>> v(mbegin, mend);
    ASSERT_EQ(v.size(), 2u);
    EXPECT_EQ(*v[0], 1);
    EXPECT_EQ(*v[1], 2);
    // original src elements should be nullptr
    for (auto& up : src) EXPECT_EQ(up, nullptr);
}

// Copy‐assignment

TEST(VectorAssignTest, CopyAssign_LargerToSmaller) {
    mystl::vector<int> src{1, 2, 3, 4, 5};
    mystl::vector<int> dst{10, 20};
    dst = src;
    EXPECT_EQ(dst.size(), src.size());
    expect_equal(dst, std::vector<int>({1,2,3,4,5}));
}

TEST(VectorAssignTest, CopyAssign_SmallerToLarger) {
    mystl::vector<int> src{1, 2};
    mystl::vector<int> dst{10, 20, 30, 40};
    dst = src;
    EXPECT_EQ(dst.size(), src.size());
    expect_equal(dst, std::vector<int>({1,2}));
}

TEST(VectorAssignTest, CopyAssign_SelfAssign) {
    mystl::vector<int> v{5, 6, 7};
    v = v;  // 自身赋值
    EXPECT_EQ(v.size(), 3);
    expect_equal(v, std::vector<int>({5,6,7}));
}


// Move-assignment

TEST(VectorAssignTest, MoveAssign_Basic) {
    mystl::vector<int> src{1,2,3};
    mystl::vector<int> dst{9,9};
    dst = std::move(src);
    // dst 应该拿到原 src 的数据
    EXPECT_EQ(dst.size(), 3);
    expect_equal(dst, std::vector<int>({1,2,3}));
    // src 变成有效但未指定状态，size()==0
    EXPECT_TRUE(src.empty());
}

TEST(VectorAssignTest, MoveAssign_SelfMove) {
    mystl::vector<int> v{1,2,3};
    v = v;  // 自身赋值
    // 行为等同于 self-assignment：不出错，内容不变
    EXPECT_EQ(v.size(), 3);
    expect_equal(v, std::vector<int>({1,2,3}));
}


// Initializer-list assignment

TEST(VectorAssignTest, ListAssign_Shrink) {
    mystl::vector<int> v{1,2,3,4};
    v = {10,20};
    EXPECT_EQ(v.size(), 2);
    expect_equal(v, std::vector<int>({10,20}));
}

TEST(VectorAssignTest, ListAssign_Grow) {
    mystl::vector<int> v{1};
    v = {5,6,7,8};
    EXPECT_EQ(v.size(), 4);
    expect_equal(v, std::vector<int>({5,6,7,8}));
}

TEST(VectorAssignTest, ListAssign_Empty) {
    mystl::vector<int> v{1,2,3};
    v = {};
    EXPECT_TRUE(v.empty());
    expect_equal(v, std::vector<int>({}));
}

// assign(count, value)
TEST(VectorAssignTest, AssignCount_Expand) {
    mystl::vector<int> v;          // 初始 capacity = 0
    v.assign(5, 42);
    EXPECT_EQ(v.size(), 5);
    expect_equal(v, std::vector<int>({42,42,42,42,42}));
}

TEST(VectorAssignTest, AssignCount_Shrink) {
    mystl::vector<int> v = {1,2,3,4};
    v.assign(2, 7);
    EXPECT_EQ(v.size(), 2);
    expect_equal(v, std::vector<int>({7,7}));
}

TEST(VectorAssignTest, AssignCount_SameSize) {
    mystl::vector<int> v = {5,6,7};
    v.assign(3, 9);
    EXPECT_EQ(v.size(), 3);
    expect_equal(v, std::vector<int>({9,9,9}));
}

TEST(VectorAssignTest, AssignCount_Zero) {
    mystl::vector<int> v = {1,2,3};
    v.assign(0, 99);
    EXPECT_TRUE(v.empty());
    expect_equal(v, std::vector<int>({}));
}

// assign(first, last)
TEST(VectorAssignTest, AssignRange_Expand) {
    std::vector<int> src = {10,20,30,40,50};
    mystl::vector<int> v = {1,2};
    v.assign(src.begin(), src.end());
    EXPECT_EQ(v.size(), src.size());
    expect_equal(v, src);
}

TEST(VectorAssignTest, AssignRange_Shrink) {
    std::vector<int> src = {9,8};
    mystl::vector<int> v = {5,6,7,8};
    v.assign(src.begin(), src.end());
    EXPECT_EQ(v.size(), src.size());
    expect_equal(v, src);
}

TEST(VectorAssignTest, AssignRange_SameSize) {
    std::vector<int> src = {3,4,5};
    mystl::vector<int> v = {7,8,9};
    v.assign(src.begin(), src.end());
    EXPECT_EQ(v.size(), src.size());
    expect_equal(v, src);
}

TEST(VectorAssignTest, AssignRange_Empty) {
    std::vector<int> src;
    mystl::vector<int> v = {1,2,3};
    v.assign(src.begin(), src.end());
    EXPECT_TRUE(v.empty());
    expect_equal(v, std::vector<int>({}));
}

// assign(initializer_list)
TEST(VectorAssignTest, AssignInitList_Expand) {
    mystl::vector<int> v = {1};
    v.assign({4,5,6,7});
    EXPECT_EQ(v.size(), 4);
    expect_equal(v, std::vector<int>({4,5,6,7}));
}

TEST(VectorAssignTest, AssignInitList_Shrink) {
    mystl::vector<int> v = {1,2,3,4};
    v.assign({8,9});
    EXPECT_EQ(v.size(), 2);
    expect_equal(v, std::vector<int>({8,9}));
}

TEST(VectorAssignTest, AssignInitList_SameSize) {
    mystl::vector<int> v = {5,6,7};
    v.assign({0,0,0});
    EXPECT_EQ(v.size(), 3);
    expect_equal(v, std::vector<int>({0,0,0}));
}

TEST(VectorAssignTest, AssignInitList_Empty) {
    mystl::vector<int> v = {1,2};
    v.assign({});
    EXPECT_TRUE(v.empty());
    expect_equal(v, std::vector<int>({}));
}

// Element access: operator[], at(), front(), back(), data()
//===----------------------------------------------------------------------===//

TEST(VectorAccessTest, OperatorIndexNonConst) {
    mystl::vector<int> v{10,20,30};
    EXPECT_EQ(v[0], 10);
    EXPECT_EQ(v[1], 20);
    EXPECT_EQ(v[2], 30);
    // 可写
    v[1] = 25;
    EXPECT_EQ(v[1], 25);
}

TEST(VectorAccessTest, OperatorIndexConst) {
    const mystl::vector<int> v{1,2,3};
    EXPECT_EQ(v[0], 1);
    EXPECT_EQ(v[2], 3);
}

TEST(VectorAccessTest, AtValid) {
    mystl::vector<int> v{5,6,7};
    EXPECT_EQ(v.at(0), 5);
    EXPECT_EQ(v.at(2), 7);
}

TEST(VectorAccessTest, AtThrowsOutOfRange) {
    mystl::vector<int> v{5,6,7};
    EXPECT_THROW(v.at(3), std::out_of_range);
    EXPECT_THROW(v.at(100), std::out_of_range);
}

TEST(VectorAccessTest, FrontBackData) {
    mystl::vector<int> v{42,84,168};
    // front/back
    EXPECT_EQ(v.front(), 42);
    EXPECT_EQ(v.back(), 168);
    // data pointer
    int* p = v.data();
    EXPECT_EQ(p[0], 42);
    EXPECT_EQ(*(p+2), 168);
    // data() 可写入
    p[1] = 99;
    EXPECT_EQ(v[1], 99);
}

//===----------------------------------------------------------------------===//
// Iterators: begin(), end(), cbegin(), cend(), rbegin(), rend()
//===----------------------------------------------------------------------===//

TEST(VectorIteratorTest, BeginEndForward) {
    mystl::vector<int> v{1,2,3};
    auto it = v.begin();
    EXPECT_EQ(*it, 1);
    ++it;
    EXPECT_EQ(*it, 2);
    it = v.end();
    --it;
    EXPECT_EQ(*it, 3);
}

TEST(VectorIteratorTest, CBeginCEnd) {
    const mystl::vector<int> v{7,8,9};
    auto it = v.cbegin();
    EXPECT_EQ(*it, 7);
    auto cit_end = v.cend();
    --cit_end;
    EXPECT_EQ(*cit_end, 9);
}

TEST(VectorIteratorTest, ReverseIterator) {
    mystl::vector<int> v{3,6,9};
    auto rit = v.rbegin();
    EXPECT_EQ(*rit, 9);
    ++rit;
    EXPECT_EQ(*rit, 6);
    auto rend = v.rend();
    --rend;
    EXPECT_EQ(*rend, 3);
}

//===----------------------------------------------------------------------===//
// Capacity: empty(), size(), capacity()
//===----------------------------------------------------------------------===//

TEST(VectorCapacityTest, EmptySizeCapacity) {
    mystl::vector<int> v;
    EXPECT_TRUE(v.empty());
    EXPECT_EQ(v.size(), 0u);
    EXPECT_EQ(v.capacity(), 0u);
}

TEST(VectorCapacityTest, SizeAfterPushBack) {
    mystl::vector<int> v;
    for (int i = 0; i < 5; ++i) {
        v.push_back(i*2);
        EXPECT_EQ(v.size(), static_cast<size_t>(i+1));
        EXPECT_FALSE(v.empty());
    }
    EXPECT_GE(v.capacity(), v.size());
}

TEST(VectorCapacityTest, CapacityAfterReserve) {
    mystl::vector<int> v{1,2,3};
    v.reserve(10);
    EXPECT_GE(v.capacity(), 10u);
    // 调用 reserve 并不改变已有元素
    expect_equal(v, std::vector<int>({1,2,3}));
}

//===----------------------------------------------------------------------===//
// Tests for reserve() and shrink_to_fit()
//===----------------------------------------------------------------------===//

TEST(VectorCapacityTest, ReserveIncreasesCapacity) {
    mystl::vector<int> v = {1, 2, 3, 4, 5};
    auto old_cap = v.capacity();
    // reserve more than current capacity
    v.reserve(old_cap + 10);
    EXPECT_GE(v.capacity(), old_cap + 10);
    // contents must be preserved
    expect_equal(v, std::vector<int>({1,2,3,4,5}));
}

TEST(VectorCapacityTest, ReserveNoOpWhenSmallerOrEqual) {
    mystl::vector<int> v = {1,2,3};
    auto old_cap = v.capacity();
    // reserve less than size → no-op
    v.reserve(1);
    EXPECT_EQ(v.capacity(), old_cap);
    expect_equal(v, std::vector<int>({1,2,3}));

    // reserve exactly current capacity → no-op
    v.reserve(old_cap);
    EXPECT_EQ(v.capacity(), old_cap);
    expect_equal(v, std::vector<int>({1,2,3}));
}

TEST(VectorCapacityTest, ReserveThrowsLengthError) {
    mystl::vector<int> v;
    // use max_size() + 1 to trigger length_error
    EXPECT_THROW(v.reserve(v.max_size() + 1), std::length_error);
}

TEST(VectorCapacityTest, ShrinkToFitReducesCapacity) {
    mystl::vector<int> v = {10,20,30};
    v.reserve(100);
    auto expanded_cap = v.capacity();
    ASSERT_GE(expanded_cap, 100u);
    v.shrink_to_fit();
    // now capacity should equal size()
    EXPECT_EQ(v.capacity(), v.size());
    expect_equal(v, std::vector<int>({10,20,30}));
}

TEST(VectorCapacityTest, ShrinkToFitNoOpWhenAtSize) {
    mystl::vector<int> v = {7,8,9,10};
    // capacity() >= size(), but exactly size() so shrink_to_fit is no-op
    v.reserve(v.size());
    auto cap = v.capacity();
    v.shrink_to_fit();
    EXPECT_EQ(v.capacity(), cap);
    expect_equal(v, std::vector<int>({7,8,9,10}));
}

TEST(VectorCapacityTest, ShrinkToFitEmptyVector) {
    mystl::vector<int> v;
    v.reserve(5);
    EXPECT_GE(v.capacity(), 5u);
    v.shrink_to_fit();
    // empty vector should have capacity 0 after shrink_to_fit
    EXPECT_EQ(v.capacity(), 0u);
    EXPECT_TRUE(v.empty());
}

//===----------------------------------------------------------------------===//
// erase: single element
//===----------------------------------------------------------------------===//

TEST(VectorEraseTest, EraseSingleBegin) {
    mystl::vector<int> v{10, 20, 30};
    auto it = v.erase(v.begin());
    // v should now be {20,30}
    EXPECT_EQ(v.size(), 2u);
    expect_equal(v, std::vector<int>({20,30}));
    // iterator should point to the new first element
    EXPECT_EQ(*it, 20);
}

TEST(VectorEraseTest, EraseSingleMiddle) {
    mystl::vector<int> v{1, 2, 3, 4};
    auto it = v.erase(v.begin() + 1);
    // v should now be {1,3,4}
    EXPECT_EQ(v.size(), 3u);
    expect_equal(v, std::vector<int>({1,3,4}));
    // iterator should point to the element that followed the erased one
    EXPECT_EQ(*it, 3);
}

TEST(VectorEraseTest, EraseSingleEnd) {
    mystl::vector<int> v{5,6,7};
    auto it = v.erase(v.begin() + v.size() - 1);
    // v should now be {5,6}
    EXPECT_EQ(v.size(), 2u);
    expect_equal(v, std::vector<int>({5,6}));
    // iterator should == v.end()
    EXPECT_EQ(it, v.end());
}

TEST(VectorEraseTest, EraseEmptyRangeDoesNothing) {
    mystl::vector<int> v{1,2,3};
    auto old_begin = v.begin();
    auto it = v.erase(v.begin(), v.begin());
    // no change
    EXPECT_EQ(v.size(), 3u);
    expect_equal(v, std::vector<int>({1,2,3}));
    // iterator should equal first argument
    EXPECT_EQ(it, old_begin);
}


//===----------------------------------------------------------------------===//
// erase: range of elements
//===----------------------------------------------------------------------===//

TEST(VectorEraseTest, EraseRangeMiddle) {
    mystl::vector<int> v{1,2,3,4,5};
    // remove elements 2,3,4
    auto it = v.erase(v.begin()+1, v.begin()+4);
    EXPECT_EQ(v.size(), 2u);
    expect_equal(v, std::vector<int>({1,5}));
    // iterator should point to 5
    EXPECT_EQ(*it, 5);
}

TEST(VectorEraseTest, EraseRangeAll) {
    mystl::vector<int> v{9,8,7};
    auto it = v.erase(v.begin(), v.end());
    EXPECT_TRUE(v.empty());
    // iterator should == v.begin() == v.end()
    EXPECT_EQ(it, v.begin());
    EXPECT_EQ(it, v.end());
}


//===----------------------------------------------------------------------===//
// erase: non‐trivial type destruction
//===----------------------------------------------------------------------===//

struct DT {
    static int destroyed;
    int x;
    DT(int v): x(v) {}
    ~DT() { ++destroyed; }
};
int DT::destroyed = 0;

TEST(VectorEraseTest, EraseDestroysNonTrivial) {
    DT::destroyed = 0;
    mystl::vector<DT> v;
    for (int i = 0; i < 5; ++i)
        v.emplace_back(i);
    // 先把扩容时的析构次数清零
    DT::destroyed = 0;
    v.erase(v.begin() + 1, v.begin() + 3);
    EXPECT_EQ(DT::destroyed, 2);
}

//===----------------------------------------------------------------------===//
// emplace at arbitrary positions
//===----------------------------------------------------------------------===//

TEST(VectorInsertOptimizedTest, EmplaceAtBegin) {
    mystl::vector<int> v{2,3,4};
    auto it = v.emplace(v.begin(), 1);
    EXPECT_EQ(v.size(), 4u);
    expect_equal(v, std::vector<int>({1,2,3,4}));
    EXPECT_EQ(*it, 1);
}

TEST(VectorInsertOptimizedTest, EmplaceAtMiddle) {
    mystl::vector<int> v{1,3,4};
    auto it = v.emplace(v.begin()+1, 2);
    EXPECT_EQ(v.size(), 4u);
    expect_equal(v, std::vector<int>({1,2,3,4}));
    EXPECT_EQ(*it, 2);
}

TEST(VectorInsertOptimizedTest, EmplaceAtEnd) {
    mystl::vector<int> v{1,2,3};
    auto it = v.emplace(v.end(), 4);
    EXPECT_EQ(v.size(), 4u);
    expect_equal(v, std::vector<int>({1,2,3,4}));
    EXPECT_EQ(it, v.end()-1);
}

//===----------------------------------------------------------------------===//
// single-element insert (copy & move overloads)
//===----------------------------------------------------------------------===//

TEST(VectorInsertOptimizedTest, InsertCopy) {
    mystl::vector<int> v{1,2,4};
    auto it = v.insert(v.begin()+2, 3);
    EXPECT_EQ(v.size(), 4u);
    expect_equal(v, std::vector<int>({1,2,3,4}));
    EXPECT_EQ(*it, 3);
}

TEST(VectorInsertOptimizedTest, InsertMoveOnly) {
    mystl::vector<std::unique_ptr<int>> v;
    v.reserve(2);
    v.push_back(std::make_unique<int>(10));
    v.push_back(std::make_unique<int>(30));
    std::unique_ptr<int> x = std::make_unique<int>(20);
    auto it = v.insert(v.begin()+1, std::move(x));
    EXPECT_EQ(v.size(), 3u);
    EXPECT_EQ(*v[1], 20);
    EXPECT_EQ(it, v.begin()+1);
}

//===----------------------------------------------------------------------===//
// count-insert
//===----------------------------------------------------------------------===//

TEST(VectorInsertOptimizedTest, InsertCountTrivial) {
    mystl::vector<int> v{1,4};
    auto it = v.insert(v.begin()+1, 2, 2);
    EXPECT_EQ(v.size(), 4u);
    expect_equal(v, std::vector<int>({1,2,2,4}));
    EXPECT_EQ(*it, 2);
}

//===----------------------------------------------------------------------===//
// range-insert
//===----------------------------------------------------------------------===//

TEST(VectorInsertOptimizedTest, InsertRangeContiguous) {
    int arr[] = {5,6,7};
    mystl::vector<int> v{1,2,3,4};
    auto it = v.insert(v.begin()+2, std::begin(arr), std::end(arr));
    EXPECT_EQ(v.size(), 7u);
    expect_equal(v, std::vector<int>({1,2,5,6,7,3,4}));
    EXPECT_EQ(*it, 5);
}

TEST(VectorInsertOptimizedTest, InsertRangeNonContiguous) {
    std::list<int> src = {8,9,10};
    mystl::vector<int> v{1,2,3};
    auto it = v.insert(v.end(), src.begin(), src.end());
    EXPECT_EQ(v.size(), 6u);
    expect_equal(v, std::vector<int>({1,2,3,8,9,10}));
    EXPECT_EQ(it, v.begin()+3);
}

//===----------------------------------------------------------------------===//
// initializer_list insert
//===----------------------------------------------------------------------===//

TEST(VectorInsertOptimizedTest, InsertInitList) {
    mystl::vector<int> v{1,2,5};
    auto it = v.insert(v.begin()+2, {3,4});
    EXPECT_EQ(v.size(), 5u);
    expect_equal(v, std::vector<int>({1,2,3,4,5}));
    EXPECT_EQ(*it, 3);
}

//===----------------------------------------------------------------------===//
// emplace_back / push_back / pop_back
//===----------------------------------------------------------------------===//

TEST(VectorInsertOptimizedTest, EmplaceBackAndPushBack) {
    mystl::vector<int> v;
    v.emplace_back(1);
    v.emplace_back(2);
    v.push_back(3);
    EXPECT_EQ(v.size(), 3u);
    expect_equal(v, std::vector<int>({1,2,3}));
}

TEST(VectorInsertOptimizedTest, PopBackReducesSize) {
    mystl::vector<int> v{1,2,3,4};
    v.pop_back();
    EXPECT_EQ(v.size(), 3u);
    expect_equal(v, std::vector<int>({1,2,3}));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
