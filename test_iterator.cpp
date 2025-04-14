#include <gtest/gtest.h>
#include <vector>
#include <list>
#include <algorithm>
#include <numeric>
#include <chrono>

#include "iterator.h"

namespace mystl {

constexpr int test_data[] = {1, 2, 3, 4, 5};
constexpr size_t test_size = sizeof(test_data) / sizeof(int);

TEST(InputIteratorTest, TraverseAndCompare) {
    input_iterator<int, false> it(const_cast<int*>(test_data));
    input_iterator<int, false> end(const_cast<int*>(test_data + test_size));

    int expected = 1;
    while (it != end) {
        EXPECT_EQ(*it, expected);
        ++it;
        ++expected;
    }
}

TEST(ForwardIteratorTest, CopyAndReTraverse) {
    forward_iterator<int, false> it1(const_cast<int*>(test_data));
    forward_iterator<int, false> it2 = it1;

    EXPECT_EQ(*it1, *it2);
    ++it1;
    ++it2;
    EXPECT_EQ(*it1, *it2);
}

TEST(BidirectionalIteratorTest, BackwardTraversal) {
    bidirectional_iterator<int, false> it(const_cast<int*>(test_data + test_size));
    --it;

    for (int i = test_size; i-- > 0; ) {
        EXPECT_EQ(*it, test_data[i]);
        if (i > 0) --it;
    }
}

TEST(RandomAccessIteratorTest, IndexAccessAndDistance) {
    random_access_iterator<int, false> it(const_cast<int*>(test_data));

    for (int i = 0; i < test_size; ++i) {
        EXPECT_EQ(it[i], test_data[i]);
    }

    auto it2 = it + 3;
    EXPECT_EQ(*it2, 4);
    EXPECT_EQ(it2 - it, 3);
}

TEST(ContiguousIteratorTest, PointerArithmetic) {
    contiguous_iterator<int, false> it(const_cast<int*>(test_data));
    contiguous_iterator<int, false> it2 = it + 2;

    EXPECT_EQ(*it2, 3);
    EXPECT_EQ(it2 - it, 2);
    EXPECT_EQ(&*it2, test_data + 2);
}

TEST(ConstIteratorConversionTest, InputToConstInput) {
    input_iterator<int, false> it(const_cast<int*>(test_data));
    input_iterator<int, true> cit = it;
    EXPECT_EQ(*cit, 1);
}

TEST(ConstIteratorConversionTest, RandomAccessToConstRandom) {
    random_access_iterator<int, false> it(const_cast<int*>(test_data));
    random_access_iterator<int, true> cit = it;
    EXPECT_EQ(cit[2], 3);
}

TEST(ReverseIteratorTest, BasicReverseTraversal) {
    using rev_it = reverse_iterator<random_access_iterator<int, false>>;
    rev_it rit(random_access_iterator<int, false>(const_cast<int*>(test_data + test_size)));
    rev_it rend(random_access_iterator<int, false>(const_cast<int*>(test_data)));

    int expected = 5;
    while (rit != rend) {
        EXPECT_EQ(*rit, expected);
        ++rit;
        --expected;
    }
}

TEST(ReverseIteratorPerformanceTest, CompareWithStdReverse) {
    constexpr size_t N = 100000;
    std::vector<int> data(N);
    std::iota(data.begin(), data.end(), 0);

    // test standard library reverse traversal
    auto std_start = std::chrono::high_resolution_clock::now();
    int sum_std = 0;
    for (auto it = data.rbegin(); it != data.rend(); ++it) {
        sum_std += *it;
    }
    auto std_end = std::chrono::high_resolution_clock::now();
    auto std_time = std::chrono::duration_cast<std::chrono::microseconds>(std_end - std_start).count();

    // test mystl reverse iterator traversal
    using my_iter = reverse_iterator<contiguous_iterator<int, false>>;
    contiguous_iterator<int, false> begin(data.data());
    contiguous_iterator<int, false> end(data.data() + data.size());
    my_iter my_begin(end);
    my_iter my_end(begin);

    auto my_start = std::chrono::high_resolution_clock::now();
    int sum_my = 0;
    for (auto it = my_begin; it != my_end; ++it) {
        sum_my += *it;
    }
    auto my_end_time = std::chrono::high_resolution_clock::now();
    auto my_time = std::chrono::duration_cast<std::chrono::microseconds>(my_end_time - my_start).count();

    EXPECT_EQ(sum_std, sum_my);
    std::cout << "std::reverse_iterator time:  " << std_time << "us\n";
    std::cout << "mystl::reverse_iterator time: " << my_time << "us\n";
}

} // namespace mystl

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
