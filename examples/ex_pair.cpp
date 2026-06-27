// example: Pair
// Compile: g++ -std=c++17 -DidxDEBUG -I../containers ex_pair.cpp -o ex_pair

#include <iostream>
#include "pair.hpp"
#include "sortedArray.hpp"

using namespace idx;

int main() {
    std::cout << "=== Pair<T1, T2> ===\n";

    // Default constructor — both members value-initialised
    Pair<int, int> p1;
    std::cout << "default: first=" << p1.first << "  second=" << p1.second << "\n";

    // Single-argument constructor — only first is set
    Pair<int, int> p2(10);
    std::cout << "single-arg(10): first=" << p2.first << "  second=" << p2.second << "\n";

    // Two-argument constructor
    Pair<int, const char*> p3(42, "hello");
    std::cout << "p3: " << p3.first << " -> \"" << p3.second << "\"\n";

    // Comparisons operate on `first` ONLY — `second` is ignored
    Pair<int,int> a(5, 999), b(5, 0), c(3, 999);
    std::cout << "a(5,999)==b(5,0): " << (a == b) << "\n";  // 1 — only first compared
    std::cout << "a(5,999)!=c(3,999): " << (a != c) << "\n";  // 1
    std::cout << "c(3)<a(5): " << (c < a) << "\n";            // 1
    std::cout << "a(5)>c(3): " << (a > c) << "\n";            // 1

    // Pair with SortedArray — sorts by first, carries second along
    std::cout << "\n--- Pair in SortedArray (sorts by first) ---\n";
    basic::SortedArray<Pair<int,const char*>, uint32> sorted;
    sorted.insert({5, "five"});
    sorted.insert({2, "two"});
    sorted.insert({8, "eight"});
    sorted.insert({1, "one"});

    std::cout << "min: " << sorted.minimum().first << " -> " << sorted.minimum().second << "\n";
    std::cout << "max: " << sorted.maximum().first << " -> " << sorted.maximum().second << "\n";
}
