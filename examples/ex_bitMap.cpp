// example: BitMap (basic:: and threadSafe::)
// Compile: g++ -std=c++17 -DidxDEBUG -I../containers -pthread ex_bitMap.cpp -o ex_bitMap

#include <iostream>
#include <thread>
#include <vector>
#include "bitMap.hpp"

using namespace idx;

void basic_example() {
    std::cout << "=== basic::BitMap<uint64, uint32> ===\n";
    std::cout << "    (1 bit per element, backing word = uint64 = 64 bits per word)\n";

    basic::BitMap<uint64,uint32> bits;

    // Set individual bits — backing array grows automatically
    bits.toggleOn(0);
    bits.toggleOn(10);
    bits.toggleOn(63);
    bits.toggleOn(64);   // crosses to a second uint64 word
    bits.toggleOn(200);  // auto-grows

    // Read bits — returns false (not an error) if index is beyond allocated range
    std::cout << "bit[0]   = " << bits[0]   << "\n";  // 1
    std::cout << "bit[1]   = " << bits[1]   << "\n";  // 0
    std::cout << "bit[10]  = " << bits[10]  << "\n";  // 1
    std::cout << "bit[63]  = " << bits[63]  << "\n";  // 1
    std::cout << "bit[64]  = " << bits[64]  << "\n";  // 1
    std::cout << "bit[200] = " << bits[200] << "\n";  // 1
    std::cout << "bit[999] = " << bits[999] << "\n";  // 0 (beyond allocation, no crash)

    // Clear a bit
    bits.toggleOff(10);
    std::cout << "bit[10] after toggleOff: " << bits[10] << "\n";  // 0

    // Toggle the same bit on again
    bits.toggleOn(10);
    std::cout << "bit[10] after toggleOn: " << bits[10] << "\n";   // 1

    // reset() — zeroes all bits, keeps backing memory
    bits.reset();
    std::cout << "after reset: bit[0]=" << bits[0] << "  bit[200]=" << bits[200] << "\n";

    // clear() — zeroes all bits and frees backing memory
    bits.toggleOn(5);
    bits.clear();
    std::cout << "after clear: bit[5]=" << bits[5] << "\n";  // 0

    // uint32 backing word — 32 bits per word instead of 64
    std::cout << "\n=== basic::BitMap<uint32, uint32> ===\n";
    basic::BitMap<uint32,uint32> bits32;
    bits32.toggleOn(0); bits32.toggleOn(31); bits32.toggleOn(32);
    std::cout << "bit[0]=" << bits32[0] << " bit[31]=" << bits32[31]
              << " bit[32]=" << bits32[32] << "\n";
}

void threadsafe_example() {
    std::cout << "\n=== threadSafe::BitMap<uint64, uint32> ===\n";

    threadSafe::BitMap<uint64,uint32> bits;

    // Threads toggle non-overlapping ranges of bits concurrently
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t)
        threads.emplace_back([&,t]{
            for (int i = t * 64; i < (t + 1) * 64; ++i)
                bits.toggleOn((uint32)i);
        });
    for (auto& th : threads) th.join();

    // Verify
    bool all_set = true;
    for (uint32 i = 0; i < 256; ++i)
        if (!bits[i]) { all_set = false; break; }
    std::cout << "all 256 bits set after concurrent toggleOn: " << (all_set ? "yes" : "no") << "\n";

    // Concurrent toggle off
    std::vector<std::thread> off_threads;
    for (int t = 0; t < 4; ++t)
        off_threads.emplace_back([&,t]{
            for (int i = t * 64; i < (t + 1) * 64; ++i)
                bits.toggleOff((uint32)i);
        });
    for (auto& th : off_threads) th.join();

    bool all_clear = true;
    for (uint32 i = 0; i < 256; ++i)
        if (bits[i]) { all_clear = false; break; }
    std::cout << "all 256 bits cleared after concurrent toggleOff: " << (all_clear ? "yes" : "no") << "\n";
}

int main() {
    basic_example();
    threadsafe_example();
}
