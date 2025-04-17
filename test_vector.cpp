#include <gtest/gtest.h>
#include "vector.h"
#include <list>

using mystl::vector;

struct MoveOnly {
    int value;
    MoveOnly(int v) : value(v) {}
    MoveOnly(MoveOnly&& other) noexcept : value(other.value) { other.value = -1; }
    MoveOnly& operator=(MoveOnly&& other) noexcept {
        if (this != &other) {
            value = other.value;
            other.value = -1;
        }
        return *this;
    }
    MoveOnly(const MoveOnly&) = delete;
    MoveOnly& operator=(const MoveOnly&) = delete;
};

// ---------- Default Constructor ----------
TEST(VectorTest, DefaultConstructor) {
    vector<int> vec;
    EXPECT_EQ(vec.size(), 0);
}

// ---------- Fill Constructor ----------
TEST(VectorTest, FillConstructor) {
    vector<int> vec(5, 42);
    EXPECT_EQ(vec.size(), 5);
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(vec.begin()[i], 42);
    }
}

// ---------- Range Constructor ----------
TEST(VectorTest, RangeConstructor) {
    std::list<int> lst = {1, 2, 3, 4};
    vector<int> vec(lst.begin(), lst.end());
    EXPECT_EQ(vec.size(), 4);
    for (int i = 0; i < 4; ++i) {
        EXPECT_EQ(vec.begin()[i], i + 1);
    }
}

// ---------- Copy Constructor ----------
TEST(VectorTest, CopyConstructor) {
    vector<int> src(3, 99);
    vector<int> dst(src);
    EXPECT_EQ(dst.size(), 3);
    for (int i = 0; i < 3; ++i) {
        EXPECT_EQ(dst.begin()[i], 99);
    }
}

// ---------- Move Constructor ----------
TEST(VectorTest, MoveConstructor) {
    vector<MoveOnly> src;
    src.emplace_back(100);
    src.emplace_back(200);

    vector<MoveOnly> dst(std::move(src));
    EXPECT_EQ(dst.size(), 2);
    EXPECT_EQ(dst.begin()[0].value, 100);
    EXPECT_EQ(dst.begin()[1].value, 200);
}

// ---------- Initializer List Constructor ----------
TEST(VectorTest, InitializerListConstructor) {
    vector<int> vec = {7, 8, 9};
    EXPECT_EQ(vec.size(), 3);
    EXPECT_EQ(vec.begin()[0], 7);
    EXPECT_EQ(vec.begin()[1], 8);
    EXPECT_EQ(vec.begin()[2], 9);
}

// ---------- Iterator Begin / End ----------
TEST(VectorTest, IteratorBeginEnd) {
    vector<int> vec = {10, 20, 30};

    auto it = vec.begin();
    EXPECT_EQ(*it, 10);
    ++it;
    EXPECT_EQ(*it, 20);
    ++it;
    EXPECT_EQ(*it, 30);
    ++it;
    EXPECT_EQ(it, vec.end()); // 最后一位之后
}

// ---------- Const Iterator Begin / End ----------
TEST(VectorTest, ConstIteratorBeginEnd) {
    const vector<int> vec = {100, 200, 300};

    auto it = vec.begin();  // const_iterator
    EXPECT_EQ(*it, 100);
    ++it;
    EXPECT_EQ(*it, 200);
    ++it;
    EXPECT_EQ(*it, 300);
    ++it;
    EXPECT_EQ(it, vec.end());
}

// ---------- Reverse Iterator Begin / End ----------
TEST(VectorTest, ReverseIterator) {
    vector<int> vec = {1, 2, 3};

    auto rit = vec.rbegin();
    EXPECT_EQ(*rit, 3);
    ++rit;
    EXPECT_EQ(*rit, 2);
    ++rit;
    EXPECT_EQ(*rit, 1);
    ++rit;
    EXPECT_EQ(rit, vec.rend()); // 越过最前端
}

// ---------- Const Reverse Iterator Begin / End ----------
TEST(VectorTest, ConstReverseIterator) {
    const vector<int> vec = {5, 6, 7};

    auto rit = vec.rbegin();
    EXPECT_EQ(*rit, 7);
    ++rit;
    EXPECT_EQ(*rit, 6);
    ++rit;
    EXPECT_EQ(*rit, 5);
    ++rit;
    EXPECT_EQ(rit, vec.rend());
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
