// example: AVLTree (basic:: and threadSafe::)
// Compile: g++ -std=c++17 -DidxDEBUG -I../containers -pthread ex_AVLTree.cpp -o ex_AVLTree

#include <iostream>
#include <thread>
#include <vector>

#include "AVLTree.hpp"

using namespace idx;

void basic_example() {
    std::cout << "=== basic::AVLTree<int, uint32> ===\n";
    std::cout << "    (self-balancing: O(log n) insert/erase/find guaranteed)\n";

    basic::AVLTree<int,uint32> t;
    std::cout << "empty: " << t.empty() << "\n";

    // Sequential insert — a BSTree would degenerate to a list here, AVL stays balanced
    for (int i = 0; i <= 6; ++i) t.insert(i);
    std::cout << "size after insert 0..6: " << t.size() << "\n";  // 7

    // All values findable in O(log n) regardless of insertion order
    bool all_ok = true;
    for (int i = 0; i <= 6; ++i) if (!t.find(i).valid()) { all_ok = false; break; }
    std::cout << "all values findable: " << (all_ok ? "yes" : "no") << "\n";

    // Duplicate insert — silently ignored
    t.insert(3);
    std::cout << "after insert(3) again: size still=" << t.size() << "\n";

    // Access by typed Index
    auto fi = t.find(4);
    std::cout << "find(4): " << (fi.valid() ? "found" : "not found") << "  t[fi]=" << t[fi] << "\n";

    // Erase leaf
    t.erase(0);
    std::cout << "after erase leaf(0): find(0)=" << (t.find(0).valid() ? "found" : "not found") << "\n";

    // Erase node with two children (tree rebalances automatically)
    t.erase(3);
    std::cout << "after erase two-child(3): find(3)=" << (t.find(3).valid() ? "found" : "not found") << "\n";
    std::cout << "remaining: ";
    for (int v : t) std::cout << v << " ";   // slot order, not sorted
    std::cout << "\n";

    // Large sequential insert — demonstrates that AVL stays O(log n) even on sorted input
    basic::AVLTree<int,uint32> big;
    for (int i = 0; i < 200; ++i) big.insert(i);
    std::cout << "big tree size=" << big.size() << "\n";
    all_ok = true;
    for (int i = 0; i < 200; ++i) if (!big.find(i).valid()) { all_ok = false; break; }
    std::cout << "all 200 values findable: " << (all_ok ? "yes" : "no") << "\n";

    // Erase every other element, tree remains balanced
    for (int i = 0; i < 200; i += 2) big.erase(i);
    std::cout << "after erasing evens: size=" << big.size() << "\n";  // 100

    // shallowClear / deepClear
    t.shallowClear(true);
    std::cout << "shallowClear: empty=" << t.empty() << "\n";
    t.insert(1); t.deepClear();
    std::cout << "deepClear: empty=" << t.empty() << "\n";
}

void threadsafe_example() {
    std::cout << "\n=== threadSafe::AVLTree<int, uint32> ===\n";

    threadSafe::AVLTree<int,uint32> t;

    // Insert 0..99 across 4 threads
    std::vector<std::thread> threads;
    for (int tid = 0; tid < 4; ++tid)
        threads.emplace_back([&,tid]{
            for (int i = 0; i < 25; ++i) t.insert(tid * 25 + i);
        });
    for (auto& th : threads) th.join();

    std::cout << "size after concurrent inserts: " << t.size() << "\n";  // 100

    bool all_ok = true;
    for (int i = 0; i < 100; ++i) if (!t.find(i).valid()) { all_ok = false; break; }
    std::cout << "all values findable: " << (all_ok ? "yes" : "no") << "\n";

    // Concurrent insert + find — no data races
    std::vector<std::thread> mix;
    mix.emplace_back([&]{ for (int i=100;i<200;i++) t.insert(i); });
    mix.emplace_back([&]{ for (int i=0;i<100;i++) t.find(i); });
    for (auto& th : mix) th.join();
    std::cout << "after concurrent insert+find: size=" << t.size() << "\n";

    // Range-for under shared_lock
    int count = 0;
    for (int v : t) { (void)v; ++count; }
    std::cout << "iterated " << count << " elements\n";
}

int main() {
    basic_example();
    threadsafe_example();
}
