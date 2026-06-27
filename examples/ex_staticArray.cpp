// example: StaticArray (basic:: and threadSafe::)
// Compile: g++ -std=c++17 -DidxDEBUG -I../containers -pthread ex_staticArray.cpp -o ex_staticArray

#include <iostream>
#include <thread>
#include <vector>
#include "staticArray.hpp"

using namespace idx;

void basic_example() {
    std::cout << "=== basic::StaticArray<int, 8, uint32> ===\n";

    basic::StaticArray<int,8,uint32> arr;
    std::cout << "empty: " << arr.empty() << "  capacity: 8\n";

    arr.pushBack(10); arr.pushBack(20); arr.pushBack(30);
    std::cout << "size=" << arr.size() << "  [" << arr[0] << "," << arr[1] << "," << arr[2] << "]\n";
    std::cout << "front=" << arr.front() << "  back=" << arr.back() << "\n";

    // Bulk push from raw array
    int vals[] = {40, 50};
    arr.pushBack(vals, 2);
    std::cout << "after bulk push [40,50]: size=" << arr.size() << "\n";

    // Erase — swap-with-last (index 1 gets replaced by back=50)
    arr.erase(1);
    std::cout << "after erase(1): [";
    for (uint32 i=0;i<arr.size();i++) std::cout << (i?",":"") << arr[i];
    std::cout << "]\n";

    // popBack
    arr.popBack();
    std::cout << "after popBack: size=" << arr.size() << "\n";

    // setSize — grows (fills new slots with T()) or shrinks (calls ~T())
    arr.setSize(6);
    std::cout << "after setSize(6): size=" << arr.size() << "\n";

    // find
    int* p = arr.find(10);
    std::cout << "find(10): " << (p ? *p : -1) << "\n";

    // shallowClear — just zeroes the size counter, no destructors
    arr.shallowClear();
    std::cout << "shallowClear: size=" << arr.size() << "\n";

    // deepClear — calls T() on each slot and resets size
    arr.pushBack(99);
    arr.deepClear();
    std::cout << "deepClear: size=" << arr.size() << "  slot[0]=" << arr[0] << "\n";

    // Range constructor
    int init[] = {1, 2, 3};
    basic::StaticArray<int,8,uint32> arr2(init, 3);
    int sum = 0;
    for (int v : arr2) sum += v;
    std::cout << "range ctor sum: " << sum << "\n";  // 6
}

void threadsafe_example() {
    std::cout << "\n=== threadSafe::StaticArray<int, 1000, uint32> ===\n";

    threadSafe::StaticArray<int,1000,uint32> arr;

    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t)
        threads.emplace_back([&]{ for (int i = 0; i < 250; ++i) arr.pushBack(i); });
    for (auto& th : threads) th.join();

    std::cout << "size after 4×250 concurrent pushBack: " << arr.size() << "\n";  // 1000

    // Concurrent read
    std::vector<std::thread> readers;
    int totals[4] = {};
    for (int t = 0; t < 4; ++t)
        readers.emplace_back([&,t]{
            int s = 0;
            for (uint32 i = 0; i < arr.size(); ++i) s += arr[i];
            totals[t] = s;
        });
    for (auto& th : readers) th.join();
    std::cout << "concurrent reads completed (shared_lock, no blocking)\n";
}

int main() {
    basic_example();
    threadsafe_example();
}
