// example: HashMap (basic:: and threadSafe::)
// Compile: g++ -std=c++17 -DidxDEBUG -I../containers -pthread ex_hashMap.cpp -o ex_hashMap

#include <iostream>
#include <thread>
#include <vector>
#include "hashMap.hpp"

using namespace idx;

void basic_example() {
    std::cout << "=== basic::HashMap<int, int, uint32> ===\n";

    basic::HashMap<int,int,uint32> m;
    std::cout << "empty: " << m.empty() << "\n";

    // insert() — returns a stable Index
    auto i1 = m.insert(1, 100);
    auto i2 = m.insert(2, 200);
    auto i3 = m.insert(3, 300);
    std::cout << "size=" << m.size() << "\n";

    // Access by Index
    std::cout << "value(i1)=" << m.value(i1) << "  key(i1)=" << m.key(i1) << "\n";

    // insert() does NOT update on duplicate key — returns existing Index unchanged
    auto dup = m.insert(1, 999);
    std::cout << "insert dup key=1: same index=" << (dup == i1 ? "yes" : "no")
              << "  value still=" << m.value(i1) << "\n";

    // operator[] — get-or-insert-default, then assign (like std::unordered_map)
    m[10] = 1000;
    std::cout << "m[10]=" << m[10] << "\n";
    m[1] = 111;   // updates existing key via reference
    std::cout << "m[1] after update=" << m[1] << "\n";

    // find
    auto fi = m.find(2);
    std::cout << "find(2): " << (fi.valid() ? "found" : "not found")
              << "  value=" << m.value(fi) << "\n";
    std::cout << "find(99): " << (m.find(99).valid() ? "found" : "not found") << "\n";

    // erase
    m.erase(2);
    std::cout << "after erase(2): find(2)=" << (m.find(2).valid() ? "found" : "not found") << "\n";

    // Iterate values (iterator visits value of each entry)
    int vsum = 0;
    for (int v : m) vsum += v;
    std::cout << "sum of values: " << vsum << "\n";

    // Iterate with key access
    std::cout << "entries: ";
    for (auto it = m.begin(); it != m.end(); ++it)
        std::cout << it.template key<int>() << "->" << *it << "  ";
    std::cout << "\n";

    // Rehash stress — table rehashes automatically at load factor 20
    basic::HashMap<int,int,uint32> big;
    for (int i = 0; i < 300; ++i) big.insert(i, i * 2);
    std::cout << "big map size=" << big.size() << "\n";
    bool ok = true;
    for (int i = 0; i < 300; ++i) {
        auto idx = big.find(i);
        if (!idx.valid() || big.value(idx) != i * 2) { ok = false; break; }
    }
    std::cout << "all 300 entries findable after rehash: " << (ok ? "yes" : "no") << "\n";

    m.deepClear();
    std::cout << "deepClear: empty=" << m.empty() << "\n";
}

void threadsafe_example() {
    std::cout << "\n=== threadSafe::HashMap<int, int, uint32> ===\n";

    threadSafe::HashMap<int,int,uint32> m;

    // Insert 0..99 across 4 threads
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t)
        threads.emplace_back([&,t]{
            for (int i = 0; i < 25; ++i) m.insert(t * 25 + i, i);
        });
    for (auto& th : threads) th.join();
    std::cout << "size after concurrent inserts: " << m.size() << "\n";  // 100

    // Concurrent read (shared_lock) + write (unique_lock)
    std::vector<std::thread> mix;
    mix.emplace_back([&]{ for (int i = 100; i < 150; ++i) m.insert(i, i); });
    mix.emplace_back([&]{ for (int i = 0; i < 50; ++i) m.find(i); });
    for (auto& th : mix) th.join();
    std::cout << "after concurrent insert+find: size=" << m.size() << "\n";
}

// String key example
void string_key_example() {
    std::cout << "\n=== HashMap<sint8*, int, uint32> (C-string keys) ===\n";

    basic::HashMap<sint8*,int,uint32> m;
    m.insert((sint8*)"health", 100);
    m.insert((sint8*)"speed",  50);
    m.insert((sint8*)"damage", 25);

    auto fi = m.find((sint8*)"speed");
    std::cout << "speed=" << (fi.valid() ? m.value(fi) : -1) << "\n";
    std::cout << "size=" << m.size() << "\n";
}

int main() {
    basic_example();
    threadsafe_example();
    string_key_example();
}
