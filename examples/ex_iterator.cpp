// example: Iterator (basic:: and threadSafe::)
// Compile: g++ -std=c++17 -DidxDEBUG -I../containers -pthread ex_iterator.cpp -o ex_iterator

#include <iostream>
#include <thread>
#include <chrono>
#include "dynamicArray.hpp"
#include <atomic>
#include "indexedDynamicArray.hpp"
#include "hashMap.hpp"
#include "AVLTree.hpp"

using namespace idx;

// ── basic::Iterator ───────────────────────────────────────────────────────────
void basic_iterator_example() {
    std::cout << "=== basic::Iterator ===\n";

    basic::DynamicArray<int,uint32> arr;
    for (int i = 1; i <= 5; ++i) arr.pushBack(i);

    // Range-for (most common usage)
    int sum = 0;
    for (int v : arr) sum += v;
    std::cout << "range-for sum: " << sum << "\n";  // 15

    // Manual forward loop with pre-increment
    std::cout << "forward: ";
    for (auto it = arr.begin(); it != arr.end(); ++it) std::cout << *it << " ";
    std::cout << "\n";

    // Post-increment — returns copy before advancing
    auto it = arr.begin();
    auto old = it++;       // old points at 1, it now points at 2
    std::cout << "post-increment: old=" << *old << "  current=" << *it << "\n";

    // Reverse iteration with rbegin/rend
    std::cout << "reverse: ";
    for (auto it2 = arr.rbegin(); it2 != arr.rend(); --it2) std::cout << *it2 << " ";
    std::cout << "\n";

    // Modify through iterator
    for (auto it3 = arr.begin(); it3 != arr.end(); ++it3) *it3 *= 2;
    std::cout << "after doubling: ";
    for (int v : arr) std::cout << v << " ";
    std::cout << "\n";

    // isEnd() check in manual loop
    auto it4 = arr.begin();
    while (!it4.isEnd()) {
        std::cout << it4.index() << ":" << *it4 << " ";
        ++it4;
    }
    std::cout << "\n";

    // index() — raw slot of current position
    auto it5 = arr.begin(); ++it5;
    std::cout << "second element is at slot " << it5.index() << "\n";

    // data() — named alternative to operator*
    std::cout << "data()=" << it5.data() << "\n";

    // Sparse container — begin() finds first LIVE slot (not necessarily slot 0)
    std::cout << "\nSparse container (IndexedDynamicArray):\n";
    basic::IndexedDynamicArray<int,uint32> ida;
    auto i0 = ida.insert(10);
    auto i1 = ida.insert(20);
    ida.insert(30);
    ida.erase(i0);  // slot 0 is now FREE
    ida.erase(i1);  // slot 1 is now FREE

    std::cout << "only slot 2 (value 30) is live:\n";
    for (int v : ida) std::cout << "  v=" << v << "\n";  // must print only 30

    // HashMap iterator — visits values, use .key<K>() for keys
    std::cout << "\nHashMap iterator:\n";
    basic::HashMap<int,int,uint32> m;
    m.insert(1, 10); m.insert(2, 20); m.insert(3, 30);
    for (auto it6 = m.begin(); it6 != m.end(); ++it6)
        std::cout << "  key=" << it6.template key<int>() << "  value=" << *it6 << "\n";

    // cbegin / cend — const iterators
    const basic::DynamicArray<int,uint32>& carr = arr;
    int csum = 0;
    for (auto it7 = carr.cbegin(); it7 != carr.cend(); ++it7) csum += *it7;
    std::cout << "cbegin/cend sum: " << csum << "\n";
}

// ── threadSafe::Iterator ──────────────────────────────────────────────────────
void threadsafe_iterator_example() {
    std::cout << "\n=== threadSafe::Iterator ===\n";

    threadSafe::DynamicArray<int,uint32> arr;
    for (int i = 1; i <= 5; ++i) arr.pushBack(i);

    // Range-for — shared_lock held for the entire loop
    int sum = 0;
    for (int v : arr) sum += v;
    std::cout << "range-for sum: " << sum << "\n";

    // Manual loop with pre-increment
    // Post-increment (it++) is DELETED — the lock cannot be copied
    std::cout << "forward: ";
    for (auto it = arr.begin(); it != arr.end(); ++it) std::cout << *it << " ";
    std::cout << "\n";

    // Verify the lock blocks writers while the iterator is alive
    std::atomic<bool> writer_done{false};
    {
        auto it = arr.begin();  // shared_lock acquired here

        std::thread writer([&]{
            arr.pushBack(99);          // blocks until iterator is destroyed
            writer_done.store(true);
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        std::cout << "writer blocked while iterator alive: " << (!writer_done ? "yes" : "no") << "\n";

        // Advance to end — lock still held
        while (!it.isEnd()) ++it;
        // it goes out of scope here → lock released → writer unblocks
        writer.join();
    }
    std::cout << "writer unblocked after iterator destroyed: " << (writer_done ? "yes" : "no") << "\n";

    // unlock() / lock() — allow mutation while iterating from same thread
    std::cout << "\nManual unlock/lock for same-thread mutation:\n";
    threadSafe::DynamicArray<int,uint32> arr2;
    for (int i = 1; i <= 5; ++i) arr2.pushBack(i * 10);

    auto it = arr2.begin();
    while (!it.isEnd()) {
        if (*it == 30) {
            auto slot = it.index(); // save index while still locked
            it.unlock();               // release shared_lock
            arr2.erase(slot);          // mutate — acquires and releases unique_lock
            it.lock();                 // re-acquire shared_lock
            break;                 // index is now stale after erase; stop iterating
        }
        ++it;
    }
    std::cout << "after erasing 30: ";
    for (int v : arr2) std::cout << v << " ";
    std::cout << "\n";

    // AVLTree with threadSafe iterator
    std::cout << "\nthreadSafe::AVLTree iterator:\n";
    threadSafe::AVLTree<int,uint32> tree;
    for (int i = 0; i < 7; ++i) tree.insert(i);
    int tsum = 0;
    for (int v : tree) tsum += v;
    std::cout << "tree sum (slot order): " << tsum << "\n";  // 0+1+2+3+4+5+6 = 21
}

int main() {
    basic_iterator_example();
    threadsafe_iterator_example();
}
