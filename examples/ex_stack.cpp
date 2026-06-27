// example: Stack (basic:: and threadSafe::)
// Compile: g++ -std=c++17 -DidxDEBUG -I../containers -pthread ex_stack.cpp -o ex_stack

#include <iostream>
#include <thread>
#include <vector>
#include "stack.hpp"

using namespace idx;

void basic_example() {
    std::cout << "=== basic::Stack<int, uint32> ===\n";

    basic::Stack<int,uint32> s;
    std::cout << "empty: " << s.empty() << "\n";

    s.push(1); s.push(2); s.push(3);
    std::cout << "size=" << s.size() << "  top=" << s.top() << "\n";

    // Direct indexed access (0 = bottom, size-1 = top)
    std::cout << "s[0]=" << s[0] << " (bottom)  s[2]=" << s[2] << " (top)\n";

    s.pop();
    std::cout << "after pop: top=" << s.top() << "  size=" << s.size() << "\n";

    // Find — linear scan
    s.push(99);
    int* p = s.find(99);
    std::cout << "find(99): " << (p ? *p : -1) << "\n";
    std::cout << "find(0):  " << (s.find(0) ? "found" : "nullptr") << "\n";

    // Iteration (bottom to top)
    int sum = 0;
    for (int v : s) sum += v;
    std::cout << "sum bottom-to-top: " << sum << "\n";

    // shallowClear — resets size, keeps memory
    s.shallowClear(true);

    s.push(5); s.push(6);
    s.deepClear();
    std::cout << "deepClear: empty=" << s.empty() << "\n";
}

void threadsafe_example() {
    std::cout << "\n=== threadSafe::Stack<int, uint32> ===\n";

    threadSafe::Stack<int,uint32> s;

    // Producer threads push concurrently
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t)
        threads.emplace_back([&]{ for (int i = 0; i < 250; ++i) s.push(i); });
    for (auto& th : threads) th.join();

    std::cout << "size after 4×250 concurrent push: " << s.size() << "\n";  // 1000

    // Consumer thread pops all
    std::thread consumer([&]{
        while (!s.empty()) s.pop();
    });
    consumer.join();
    std::cout << "after concurrent pop-all: empty=" << s.empty() << "\n";
}

int main() {
    basic_example();
    threadsafe_example();
}
