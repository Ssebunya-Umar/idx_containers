// example: HybridArray (basic:: and threadSafe::)
// Compile: g++ -std=c++17 -DidxDEBUG -I../containers -pthread ex_hybridArray.cpp -o ex_hybridArray

#include <iostream>
#include <thread>
#include <vector>
#include "hybridArray.hpp"

using namespace idx;

void basic_example() {
    std::cout << "=== basic::HybridArray<int, 4, uint32> ===\n";
    std::cout << "    (first 4 elements inline, rest on heap)\n";

    basic::HybridArray<int,4,uint32> arr;
    std::cout << "empty: " << arr.empty() << "\n";

    // First 4 go inline (zero heap allocation)
    for (int i = 1; i <= 4; ++i) arr.pushBack(i);
    std::cout << "4 inline elements: size=" << arr.size() << "\n";

    // 5th+ spill to heap transparently
    arr.pushBack(5); arr.pushBack(6);
    std::cout << "6 total (2 on heap): size=" << arr.size() << "\n";

    // Unified access — no difference at the call site
    std::cout << "arr[0]=" << arr[0] << " (inline)  arr[5]=" << arr[5] << " (heap)\n";
    std::cout << "front=" << arr.front() << "  back=" << arr.back() << "\n";

    // Erase from inline tier — heap front element promotes to fill the gap
    arr.erase(0);   // removes slot 0 (value 1); slot 0 is filled by front of heap tier
    std::cout << "after erase(0): size=" << arr.size() << "  arr[0]=" << arr[0] << "\n";

    // Erase from heap tier — swap-with-last within the heap tier
    arr.erase(4);
    std::cout << "after erase(4): size=" << arr.size() << "\n";

    // find searches both tiers
    int* p = arr.find(3);
    std::cout << "find(3): " << (p ? *p : -1) << "\n";

    // toDynamicArray — returns a heap-only copy
    auto d = arr.toDynamicArray();
    std::cout << "toDynamicArray size=" << d.size() << "\n";

    // Range-for
    int sum = 0;
    for (int v : arr) sum += v;
    std::cout << "range-for sum: " << sum << "\n";

    // shallowClear and deepClear
    arr.shallowClear(true);
    std::cout << "shallowClear: empty=" << arr.empty() << "\n";

    for (int i = 0; i < 8; ++i) arr.pushBack(i);
    arr.deepClear();
    std::cout << "deepClear: empty=" << arr.empty() << "\n";

    // Cross-type construction from StaticArray
    basic::StaticArray<int,4,uint32> sa;
    sa.pushBack(10); sa.pushBack(20);
    // Cross-type construction not supported across different stackCapacity values
    // basic::HybridArray<int,4,uint32> fromStatic(sa);  // compile error: different stackCapacity
    // Use toDynamicArray() + range push instead:
    basic::HybridArray<int,4,uint32> fromDynamic;
    auto saArr = sa;
    for (uint32 i = 0; i < saArr.size(); ++i) fromDynamic.pushBack(saArr[i]);
    
}

void threadsafe_example() {
    std::cout << "\n=== threadSafe::HybridArray<int, 4, uint32> ===\n";

    threadSafe::HybridArray<int,4,uint32> arr;

    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t)
        threads.emplace_back([&]{ for (int i = 0; i < 25; ++i) arr.pushBack(i); });
    for (auto& th : threads) th.join();

    std::cout << "size after 4×25 concurrent pushBack: " << arr.size() << "\n";  // 100

    int sum = 0;
    for (int v : arr) sum += v;
    std::cout << "sum via range-for: " << sum << "\n";
}

int main() {
    basic_example();
    threadsafe_example();
}
