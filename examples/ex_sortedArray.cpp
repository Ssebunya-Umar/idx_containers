// example: SortedArray (basic:: and threadSafe::)
// Compile: g++ -std=c++17 -DidxDEBUG -I../containers -pthread ex_sortedArray.cpp -o ex_sortedArray

#include <iostream>
#include <thread>
#include <vector>
#include "sortedArray.hpp"

using namespace idx;

void basic_example() {
    std::cout << "=== basic::SortedArray<int, uint32> ===\n";

    basic::SortedArray<int,uint32> arr;
    std::cout << "empty: " << arr.empty() << "\n";

    // insert() maintains sorted order internally via a doubly-linked list
    arr.insert(5); arr.insert(2); arr.insert(8); arr.insert(1); arr.insert(9); arr.insert(4);
    std::cout << "size=" << arr.size() << "\n";

    // O(1) access to extremes
    std::cout << "minimum=" << arr.minimum() << "  maximum=" << arr.maximum() << "\n";

    // find — two-pointer scan from both ends
    auto fi = arr.find(8);
    std::cout << "find(8): " << (fi.valid() ? "found" : "not found") << "\n";
    std::cout << "arr[fi]=" << arr[fi] << "\n";
    std::cout << "find(99): " << (arr.find(99).valid() ? "found" : "not found") << "\n";

    // Duplicates are silently ignored
    arr.insert(5);
    std::cout << "after insert(5) again: size still=" << arr.size() << "\n";

    // erase by value — updates min/max if needed
    arr.erase(1);
    std::cout << "after erase(1): minimum=" << arr.minimum() << "\n";
    arr.erase(9);
    std::cout << "after erase(9): maximum=" << arr.maximum() << "\n";

    // erase by typed Index
    auto fi2 = arr.find(5);
    arr.erase(fi2);
    std::cout << "after erase by Index: size=" << arr.size() << "\n";

    // Iteration is in slot order (NOT sorted order)
    // For sorted traversal use minimum() and repeatedly find/erase or build your own walk
    std::cout << "iteration (slot/insertion order): ";
    for (int v : arr) std::cout << v << " ";
    std::cout << "\n";

    arr.deepClear();
    std::cout << "deepClear: empty=" << arr.empty() << "\n";
}

void threadsafe_example() {
    std::cout << "\n=== threadSafe::SortedArray<int, uint32> ===\n";

    threadSafe::SortedArray<int,uint32> arr;

    // Insert 0..99 across 4 threads
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t)
        threads.emplace_back([&,t]{
            for (int i = 0; i < 25; ++i) arr.insert(t * 25 + i);
        });
    for (auto& th : threads) th.join();

    std::cout << "size after concurrent inserts: " << arr.size() << "\n";  // 100
    std::cout << "minimum=" << arr.minimum() << "  maximum=" << arr.maximum() << "\n";

    // Concurrent erase + read
    std::vector<std::thread> mix;
    mix.emplace_back([&]{ for (int i = 0; i < 50; i += 2) arr.erase(i); });
    mix.emplace_back([&]{ (void)arr.find(51); });
    for (auto& th : mix) th.join();
    std::cout << "after concurrent erase+find: size=" << arr.size() << "\n";
}

int main() {
    basic_example();
    threadsafe_example();
}
