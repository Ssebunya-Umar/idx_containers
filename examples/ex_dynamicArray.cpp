// example: DynamicArray (basic:: and threadSafe::)
// Compile: g++ -std=c++17 -DidxDEBUG -I../containers -pthread ex_dynamicArray.cpp -o ex_dynamicArray

#include <iostream>
#include <thread>
#include <vector>
#include "dynamicArray.hpp"

using namespace idx;

// ── helpers ───────────────────────────────────────────────────────────────────
static void print(const char* label, const basic::DynamicArray<int,uint32>& arr) {
    std::cout << label << " [";
    for (uint32 i = 0; i < arr.size(); ++i) std::cout << (i?",":"") << arr[i];
    std::cout << "] size=" << arr.size() << " cap=" << arr.capacity() << "\n";
}

// ── basic:: ───────────────────────────────────────────────────────────────────
void basic_example() {
    std::cout << "=== basic::DynamicArray ===\n";

    basic::DynamicArray<int,uint32> arr;
    std::cout << "empty: " << arr.empty() << "\n";  // 1

    // Push
    arr.pushBack(10);
    arr.pushBack(20);
    arr.pushBack(30);
    print("after pushBack 10,20,30:", arr);

    // Raw-array push
    int vals[] = {40, 50};
    arr.pushBack(vals, 2);
    print("after pushBack array [40,50]:", arr);

    // Bulk push from another array
    basic::DynamicArray<int,uint32> extra;
    extra.pushBack(60); extra.pushBack(70);
    arr.pushBack(extra);
    print("after pushBack extra [60,70]:", arr);

    // Index and front/back
    std::cout << "arr[0]=" << arr[0] << "  front=" << arr.front() << "  back=" << arr.back() << "\n";

    // Erase — swap-with-last, O(1), unordered
    arr.erase(0);    // removes 10; last element (70) fills slot 0
    print("after erase(0):", arr);

    // Pop
    arr.popBack();
    print("after popBack:", arr);

    // Find
    int* p = arr.find(30);
    std::cout << "find(30): " << (p ? *p : -1) << "\n";
    std::cout << "find(99): " << (arr.find(99) ? "found" : "nullptr") << "\n";

    // Resize — grows by default-constructing, shrinks by destroying
    arr.resize(3);
    print("after resize(3):", arr);

    // Reserve — ensures capacity without changing size
    arr.reserve(20);
    std::cout << "after reserve(20): size=" << arr.size() << " cap=" << arr.capacity() << "\n";

    // Wrap — trim capacity to size
    arr.wrap();
    std::cout << "after wrap: size=" << arr.size() << " cap=" << arr.capacity() << "\n";

    // shallowClear — resets size but keeps memory
    uint32 cap = arr.capacity();
    arr.shallowClear(true);
    std::cout << "shallowClear: size=" << arr.size() << " cap=" << arr.capacity()
              << " (was " << cap << ")\n";

    // deepClear — destroys everything and frees memory
    arr.pushBack(1); arr.pushBack(2);
    arr.deepClear();
    std::cout << "deepClear: size=" << arr.size() << " cap=" << arr.capacity() << "\n";

    // Range-for iteration
    arr.pushBack(1); arr.pushBack(2); arr.pushBack(3);
    int sum = 0;
    for (int v : arr) sum += v;
    std::cout << "range-for sum: " << sum << "\n";

    // Copy and move
    basic::DynamicArray<int,uint32> copy(arr);
    basic::DynamicArray<int,uint32> moved(std::move(copy));
    std::cout << "moved size=" << moved.size() << " copy size=" << copy.size() << "\n";
}

// ── threadSafe:: ──────────────────────────────────────────────────────────────
void threadsafe_example() {
    std::cout << "\n=== threadSafe::DynamicArray ===\n";

    threadSafe::DynamicArray<int,uint32> arr;

    // Four threads each push 250 elements concurrently
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t)
        threads.emplace_back([&]{ for (int i = 0; i < 250; ++i) arr.pushBack(i); });
    for (auto& th : threads) th.join();

    std::cout << "size after 4×250 concurrent pushBack: " << arr.size() << "\n";  // 1000

    // Iteration holds the shared_lock for the entire traversal —
    // no writer can modify arr while this loop runs.
    int count = 0;
    for (int v : arr) { (void)v; ++count; }
    std::cout << "iterated " << count << " elements\n";

    // Concurrent erase + pushBack
    threadSafe::DynamicArray<int,uint32> arr2;
    for (int i = 0; i < 100; ++i) arr2.pushBack(i);

    std::vector<std::thread> mix;
    mix.emplace_back([&]{ for (int i=0;i<50;i++) arr2.pushBack(i); });
    mix.emplace_back([&]{ for (int i=0;i<50;i++) { if (!arr2.empty()) arr2.erase(0); } });
    for (auto& th : mix) th.join();
    std::cout << "after concurrent push+erase: size=" << arr2.size() << "\n";
}

int main() {
    basic_example();
    threadsafe_example();
}
