// example: IndexedStaticArray (basic:: and threadSafe::)
// Compile: g++ -std=c++17 -DidxDEBUG -I../containers -pthread ex_indexedStaticArray.cpp -o ex_indexedStaticArray

#include <iostream>
#include <thread>
#include <vector>
#include "indexedStaticArray.hpp"
#include "core/types.hpp"

using namespace idx;

void basic_example() {
    std::cout << "=== basic::IndexedStaticArray<int, 16, uint32> ===\n";
    std::cout << "    (caller chooses which slot each element occupies)\n";

    basic::IndexedStaticArray<int,16,uint32> arr;
    std::cout << "empty: " << arr.empty() << "\n";

    // Insert at explicit slots — useful for lookup tables and sparse grids
    arr.insert(100, 0u);
    arr.insert(200, 7u);
    arr.insert(300, 15u);
    std::cout << "size=" << arr.size() << "\n";

    // Access by raw index or typed Index
    std::cout << "arr[0]=" << arr[0u] << "  arr[7]=" << arr[7u] << "  arr[15]=" << arr[15u] << "\n";

    // Typed Index access
    auto idx = arr.find(200);
    std::cout << "find(200): valid=" << idx.valid() << "  arr[idx]=" << arr[idx] << "\n";

    // Occupancy check via find()
    std::cout << "slot 0 occupied: " << (arr.find(100).valid() ? "yes" : "no") << "\n";
    std::cout << "slot 5 occupied: " << (arr.find(200).valid() ? "yes" : "no") << "\n";

    // Erase
    arr.erase(7u);
    std::cout << "after erase(7): size=" << arr.size() << "\n";
    std::cout << "slot 7 occupied: " << (arr.find(200).valid() ? "yes" : "no") << "\n";

    // Iteration skips free slots
    int sum = 0;
    for (int v : arr) sum += v;
    std::cout << "sum of live elements: " << sum << "\n";  // 100+300=400

    // deepClear resets all slots and the bitmap
    arr.deepClear();
    std::cout << "deepClear: empty=" << arr.empty() << "\n";
}

void threadsafe_example() {
    std::cout << "\n=== threadSafe::IndexedStaticArray<int, 100, uint32> ===\n";

    threadSafe::IndexedStaticArray<int,100,uint32> arr;

    // Assign disjoint slots across threads — no slot is written by two threads
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t)
        threads.emplace_back([&,t]{
            for (int i = 0; i < 25; ++i)
                arr.insert(t * 25 + i, (uint32)(t * 25 + i));
        });
    for (auto& th : threads) th.join();

    std::cout << "size after concurrent inserts: " << arr.size() << "\n";  // 100

    // Verify all slots were written
    bool all_ok = true;
    for (uint32 i = 0; i < 100; ++i)
        if (arr[i] != (int)i) all_ok = false;
    std::cout << "all slots correct: " << (all_ok ? "yes" : "no") << "\n";
}

int main() {
    basic_example();
    threadsafe_example();
}
