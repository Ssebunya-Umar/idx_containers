// example: Queue (basic:: and threadSafe::)
// Compile: g++ -std=c++17 -DidxDEBUG -I../containers -pthread ex_queue.cpp -o ex_queue

#include <iostream>
#include <thread>
#include <vector>
#include "queue.hpp"

using namespace idx;

void basic_example() {
    std::cout << "=== basic::Queue<int, uint32> ===\n";

    basic::Queue<int,uint32> q;
    std::cout << "empty: " << q.empty() << "\n";

    // push() returns a stable Index for the enqueued element
    auto idx1 = q.push(10);
    auto idx2 = q.push(20);
    auto idx3 = q.push(30);
    std::cout << "size=" << q.size() << "  front=" << q.front() << "\n";

    // Access by Index
    std::cout << "q[idx2]=" << q[idx2] << "\n";

    // pop() dequeues the front (FIFO)
    q.pop();
    std::cout << "after pop: front=" << q.front() << "  size=" << q.size() << "\n";

    // erase(T) removes the FIRST element matching the value
    q.push(20);     // second 20
    q.erase(20);    // removes the first 20 (idx2's value); second 20 stays
    std::cout << "after erase(20) [first match]: size=" << q.size() << "\n";
    std::cout << "second 20 still present: " << (q.find(20).valid() ? "yes" : "no") << "\n";

    // find — returns a valid Index if found
    auto fi = q.find(30);
    std::cout << "find(30): " << (fi.valid() ? "found" : "not found") << "\n";
    std::cout << "find(99): " << (q.find(99).valid() ? "found" : "not found") << "\n";

    // Iteration is in slot order (NOT FIFO order) — use pop() for FIFO traversal
    std::cout << "iteration (slot order): ";
    for (int v : q) std::cout << v << " ";
    std::cout << "\n";

    // FIFO traversal: front then pop
    basic::Queue<int,uint32> q2;
    q2.push(1); q2.push(2); q2.push(3);
    std::cout << "FIFO traversal: ";
    while (!q2.empty()) {
        std::cout << q2.front() << " ";
        q2.pop();
    }
    std::cout << "\n";

    // shallowClear / deepClear
    q.deepClear();
    std::cout << "deepClear: empty=" << q.empty() << "\n";
}

void threadsafe_example() {
    std::cout << "\n=== threadSafe::Queue<int, uint32> ===\n";

    threadSafe::Queue<int,uint32> q;

    // Producer threads push concurrently
    std::vector<std::thread> producers;
    for (int t = 0; t < 4; ++t)
        producers.emplace_back([&]{ for (int i = 0; i < 250; ++i) q.push(i); });
    for (auto& th : producers) th.join();

    std::cout << "size after 4×250 concurrent push: " << q.size() << "\n";  // 1000

    // Consumer thread drains while producer adds more
    std::vector<std::thread> mix;
    mix.emplace_back([&]{ for (int i = 0; i < 100; ++i) q.push(999); });
    mix.emplace_back([&]{ for (int i = 0; i < 100; ++i) { if (!q.empty()) q.pop(); } });
    for (auto& th : mix) th.join();
    std::cout << "after concurrent push+pop: size=" << q.size() << "\n";
}

int main() {
    basic_example();
    threadsafe_example();
}
