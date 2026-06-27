// example: BinarySearchTree (basic:: and threadSafe::)
// Compile: g++ -std=c++17 -DidxDEBUG -I../containers -pthread ex_BSTree.cpp -o ex_BSTree

#include <iostream>
#include <thread>
#include <vector>
#include "BSTree.hpp"

using namespace idx;

void basic_example() {
    std::cout << "=== basic::BinarySearchTree<int, uint32> ===\n";

    basic::BinarySearchTree<int,uint32> t;
    std::cout << "empty: " << t.empty() << "\n";

    t.insert(5); t.insert(3); t.insert(7); t.insert(1); t.insert(4); t.insert(6); t.insert(8);
    std::cout << "size=" << t.size() << "\n";

    // Duplicate insert — silently ignored
    t.insert(5);
    std::cout << "after insert(5) again: size still=" << t.size() << "\n";

    // find
    auto fi = t.find(4);
    std::cout << "find(4): " << (fi.valid() ? "found" : "not found") << "\n";
    std::cout << "t[fi]=" << t[fi] << "\n";
    std::cout << "find(99): " << (t.find(99).valid() ? "found" : "not found") << "\n";

    // Erase leaf
    t.erase(1);
    std::cout << "after erase leaf(1): find(1)=" << (t.find(1).valid() ? "found" : "not found") << "\n";

    // Erase node with one child
    t.erase(4);
    std::cout << "after erase one-child(4): find(4)=" << (t.find(4).valid() ? "found" : "not found") << "\n";

    // Erase node with two children (in-order successor replaces it)
    t.erase(5);
    std::cout << "after erase two-child root(5): find(5)=" << (t.find(5).valid() ? "found" : "not found") << "\n";
    std::cout << "remaining: ";
    for (int v : t) std::cout << v << " ";  // slot order, not sorted
    std::cout << "\n";

    // Iteration is in slot order, NOT sorted order
    // For a sorted walk, do an in-order traversal manually or use SortedArray/AVLTree
    std::cout << "NOTE: iteration order is insertion-slot order, not sorted\n";

    t.shallowClear(true);
    std::cout << "shallowClear: empty=" << t.empty() << "\n";

    t.insert(10); t.insert(5);
    t.deepClear();
    std::cout << "deepClear: empty=" << t.empty() << "\n";
}

void threadsafe_example() {
    std::cout << "\n=== threadSafe::BinarySearchTree<int, uint32> ===\n";

    threadSafe::BinarySearchTree<int,uint32> t;

    // Insert 0..99 across 4 threads
    std::vector<std::thread> threads;
    for (int t2 = 0; t2 < 4; ++t2)
        threads.emplace_back([&,t2]{
            for (int i = 0; i < 25; ++i) t.insert(t2 * 25 + i);
        });
    for (auto& th : threads) th.join();

    std::cout << "size after concurrent inserts: " << t.size() << "\n";  // 100

    // Verify all values are findable
    bool all_ok = true;
    for (int i = 0; i < 100; ++i)
        if (!t.find(i).valid()) { all_ok = false; break; }
    std::cout << "all 100 values findable: " << (all_ok ? "yes" : "no") << "\n";

    // Concurrent find + erase
    std::vector<std::thread> mix;
    mix.emplace_back([&]{ for (int i=0;i<50;i+=2) t.erase(i); });
    mix.emplace_back([&]{ for (int i=1;i<100;i+=2) t.find(i); });
    for (auto& th : mix) th.join();
    std::cout << "after concurrent erase+find: size=" << t.size() << "\n";
}

int main() {
    basic_example();
    threadsafe_example();
}
