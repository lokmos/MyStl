// vector_benchmark.cpp
#include <benchmark/benchmark.h>
#include <vector>
#include "mystl_vector.h"

// Aliases for convenience
template <typename T>
using std_vec = std::vector<T>;

template <typename T>
using mystl_vec = mystl::vector<T>;

// ----------------------------------------------------------------------------
// Push-back benchmark (with and without reserve)
// ----------------------------------------------------------------------------

static void BM_PushBack_Std_NoReserve(benchmark::State& state) {
    const size_t N = state.range(0);
    for (auto _ : state) {
        std_vec<int> v;
        for (size_t i = 0; i < N; ++i) {
            v.push_back(int(i));
        }
        benchmark::DoNotOptimize(v);
    }
}
BENCHMARK(BM_PushBack_Std_NoReserve)
    ->Arg(1000)->Arg(10000)->Arg(100000)->Arg(1000000);

static void BM_PushBack_Mystl_NoReserve(benchmark::State& state) {
    const size_t N = state.range(0);
    for (auto _ : state) {
        mystl_vec<int> v;
        for (size_t i = 0; i < N; ++i) {
            v.push_back(int(i));
        }
        benchmark::DoNotOptimize(v);
    }
}
BENCHMARK(BM_PushBack_Mystl_NoReserve)
    ->Arg(1000)->Arg(10000)->Arg(100000)->Arg(1000000);

static void BM_PushBack_Std_WithReserve(benchmark::State& state) {
    const size_t N = state.range(0);
    for (auto _ : state) {
        std_vec<int> v;
        v.reserve(N);
        for (size_t i = 0; i < N; ++i) {
            v.push_back(int(i));
        }
        benchmark::DoNotOptimize(v);
    }
}
BENCHMARK(BM_PushBack_Std_WithReserve)
    ->Arg(1000)->Arg(10000)->Arg(100000)->Arg(1000000);

static void BM_PushBack_Mystl_WithReserve(benchmark::State& state) {
    const size_t N = state.range(0);
    for (auto _ : state) {
        mystl_vec<int> v;
        v.reserve(N);
        for (size_t i = 0; i < N; ++i) {
            v.push_back(int(i));
        }
        benchmark::DoNotOptimize(v);
    }
}
BENCHMARK(BM_PushBack_Mystl_WithReserve)
    ->Arg(1000)->Arg(10000)->Arg(100000)->Arg(1000000);

// ----------------------------------------------------------------------------
// Random access benchmark
// ----------------------------------------------------------------------------

static void BM_RandomAccess_Std(benchmark::State& state) {
    const size_t N = state.range(0);
    std_vec<int> v(N);
    for (size_t i = 0; i < N; ++i) v[i] = int(i);
    for (auto _ : state) {
        int sum = 0;
        for (size_t i = 0; i < N; ++i) {
            sum += v[i];
        }
        benchmark::DoNotOptimize(sum);
    }
}
BENCHMARK(BM_RandomAccess_Std)
    ->Arg(1000)->Arg(10000)->Arg(100000)->Arg(1000000);

static void BM_RandomAccess_Mystl(benchmark::State& state) {
    const size_t N = state.range(0);
    mystl_vec<int> v(N);
    for (size_t i = 0; i < N; ++i) v[i] = int(i);
    for (auto _ : state) {
        int sum = 0;
        for (size_t i = 0; i < N; ++i) {
            sum += v[i];
        }
        benchmark::DoNotOptimize(sum);
    }
}
BENCHMARK(BM_RandomAccess_Mystl)
    ->Arg(1000)->Arg(10000)->Arg(100000)->Arg(1000000);

// ----------------------------------------------------------------------------
// Iteration benchmark
// ----------------------------------------------------------------------------

static void BM_Iterate_Std(benchmark::State& state) {
    const size_t N = state.range(0);
    std_vec<int> v(N, 1);
    for (auto _ : state) {
        int sum = 0;
        for (auto& x : v) sum += x;
        benchmark::DoNotOptimize(sum);
    }
}
BENCHMARK(BM_Iterate_Std)
    ->Arg(1000)->Arg(10000)->Arg(100000)->Arg(1000000);

static void BM_Iterate_Mystl(benchmark::State& state) {
    const size_t N = state.range(0);
    mystl_vec<int> v(N, 1);
    for (auto _ : state) {
        int sum = 0;
        for (auto& x : v) sum += x;
        benchmark::DoNotOptimize(sum);
    }
}
BENCHMARK(BM_Iterate_Mystl)
    ->Arg(1000)->Arg(10000)->Arg(100000)->Arg(1000000);

// ----------------------------------------------------------------------------
// Insert at middle benchmark
// ----------------------------------------------------------------------------

static void BM_InsertMiddle_Std(benchmark::State& state) {
    const size_t N = state.range(0);
    for (auto _ : state) {
        std_vec<int> v;
        v.reserve(N);
        for (size_t i = 0; i < N; ++i) {
            v.insert(v.begin() + (v.size() / 2), int(i));
        }
        benchmark::DoNotOptimize(v);
    }
}
BENCHMARK(BM_InsertMiddle_Std)
    ->Arg(1000)->Arg(10000)->Arg(50000);

static void BM_InsertMiddle_Mystl(benchmark::State& state) {
    const size_t N = state.range(0);
    for (auto _ : state) {
        mystl_vec<int> v;
        v.reserve(N);
        for (size_t i = 0; i < N; ++i) {
            v.insert(v.begin() + (v.size() / 2), int(i));
        }
        benchmark::DoNotOptimize(v);
    }
}
BENCHMARK(BM_InsertMiddle_Mystl)
    ->Arg(1000)->Arg(10000)->Arg(50000);

// ----------------------------------------------------------------------------
// Erase at middle benchmark
// ----------------------------------------------------------------------------

static void BM_EraseMiddle_Std(benchmark::State& state) {
    const size_t N = state.range(0);
    for (auto _ : state) {
        std_vec<int> v(N);
        // fill with dummy
        for (size_t i = 0; i < N; ++i) v[i] = int(i);
        // erase one by one at middle
        while (!v.empty()) {
            v.erase(v.begin() + v.size()/2);
        }
        benchmark::DoNotOptimize(v);
    }
}
BENCHMARK(BM_EraseMiddle_Std)
    ->Arg(1000)->Arg(10000)->Arg(50000);

static void BM_EraseMiddle_Mystl(benchmark::State& state) {
    const size_t N = state.range(0);
    for (auto _ : state) {
        mystl_vec<int> v(N);
        for (size_t i = 0; i < N; ++i) v[i] = int(i);
        while (!v.empty()) {
            v.erase(v.begin() + v.size()/2);
        }
        benchmark::DoNotOptimize(v);
    }
}
BENCHMARK(BM_EraseMiddle_Mystl)
    ->Arg(1000)->Arg(10000)->Arg(50000);

// ----------------------------------------------------------------------------
// Reserve / Shrink benchmark
// ----------------------------------------------------------------------------

static void BM_Reserve_Std(benchmark::State& state) {
    const size_t N = state.range(0);
    for (auto _ : state) {
        std_vec<int> v;
        v.reserve(N);
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_Reserve_Std)
    ->Arg(1000)->Arg(10000)->Arg(100000);

static void BM_Reserve_Mystl(benchmark::State& state) {
    const size_t N = state.range(0);
    for (auto _ : state) {
        mystl_vec<int> v;
        v.reserve(N);
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_Reserve_Mystl)
    ->Arg(1000)->Arg(10000)->Arg(100000);

static void BM_ShrinkToFit_Std(benchmark::State& state) {
    const size_t N = state.range(0);
    for (auto _ : state) {
        std_vec<int> v(N);
        v.reserve(N * 2);
        v.shrink_to_fit();
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_ShrinkToFit_Std)
    ->Arg(1000)->Arg(10000)->Arg(100000);

static void BM_ShrinkToFit_Mystl(benchmark::State& state) {
    const size_t N = state.range(0);
    for (auto _ : state) {
        mystl_vec<int> v(N);
        v.reserve(N * 2);
        v.shrink_to_fit();
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_ShrinkToFit_Mystl)
    ->Arg(1000)->Arg(10000)->Arg(100000);

// ----------------------------------------------------------------------------
// Register main
// ----------------------------------------------------------------------------
BENCHMARK_MAIN();
