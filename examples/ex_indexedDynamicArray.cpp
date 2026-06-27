// example: IndexedDynamicArray (basic:: and threadSafe::)
// Compile: g++ -std=c++17 -DidxDEBUG -I../containers -pthread ex_indexedDynamicArray.cpp -o ex_indexedDynamicArray

#include <iostream>
#include <thread>
#include <vector>
#include "indexedDynamicArray.hpp"

using namespace idx;

// A simple game-entity type to demonstrate stable handles
struct Entity {
    bool operator==(const Entity& o) const { return name==o.name && health==o.health; }
    const char* name;
    int health;
};

void basic_example() {
    std::cout << "=== basic::IndexedDynamicArray<Entity, uint32> ===\n";

    basic::IndexedDynamicArray<Entity,uint32> entities;
    std::cout << "empty: " << entities.empty() << "\n";

    // insert() returns a stable typed handle
    auto idAlice = entities.insert({"Alice", 100});
    auto idBob   = entities.insert({"Bob",   80});
    auto idCarol = entities.insert({"Carol", 90});
    std::cout << "inserted 3 entities, size=" << entities.size() << "\n";

    // Access by handle — O(1)
    std::cout << "Alice health: " << entities[idAlice].health << "\n";
    std::cout << "Bob   health: " << entities[idBob].health   << "\n";

    // Erase Bob — Alice and Carol handles remain valid, unchanged
    entities.erase(idBob);
    std::cout << "after erase Bob: size=" << entities.size() << "\n";
    std::cout << "Alice still valid: " << entities.isIndexOccupied(idAlice) << "\n";
    std::cout << "Bob   still valid: " << entities.isIndexOccupied(idBob)   << "\n";

    // Insert reuses Bob's freed slot
    auto idDave = entities.insert({"Dave", 70});
    std::cout << "Dave's slot == Bob's old slot: " << (idDave == idBob ? "yes" : "no") << "\n";
    std::cout << "size after reuse: " << entities.size() << "\n";

    // find — linear scan over live slots only
    auto found = entities.find({"Alice", 100});   // uses operator==
    std::cout << "find Alice: " << (found.valid() ? "found" : "not found") << "\n";

    // internalSize — total raw slots (live + free)
    std::cout << "size=" << entities.size() << "  internalSize=" << entities.internalSize() << "\n";

    // Iteration visits only live slots (free slots are skipped)
    std::cout << "live entities: ";
    for (const Entity& e : entities) std::cout << e.name << " ";
    std::cout << "\n";

    // erase(T) removes the FIRST matching slot
    entities.insert({"Alice", 100});  // second Alice
    entities.erase(Entity{"Alice", 100});
    std::cout << "after erase-by-value Alice (first match removed): size=" << entities.size() << "\n";

    // shallowClear — resets without freeing memory
    entities.shallowClear(true);
    std::cout << "shallowClear: empty=" << entities.empty() << "\n";

    entities.insert({"X", 1});
    entities.deepClear();
    std::cout << "deepClear: empty=" << entities.empty()
              << "  internalSize=" << entities.internalSize() << "\n";
}

void threadsafe_example() {
    std::cout << "\n=== threadSafe::IndexedDynamicArray<Entity, uint32> ===\n";

    threadSafe::IndexedDynamicArray<Entity,uint32> entities;

    // Four threads insert concurrently — each insert is atomic
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t)
        threads.emplace_back([&,t]{
            for (int i = 0; i < 10; ++i)
                entities.insert({"entity", t * 10 + i});
        });
    for (auto& th : threads) th.join();
    std::cout << "size after 4×10 concurrent inserts: " << entities.size() << "\n";  // 40

    // Range-for holds shared_lock for entire traversal
    int count = 0;
    for (const Entity& e : entities) { (void)e; ++count; }
    std::cout << "iterated " << count << " entities\n";
}

int main() {
    basic_example();
    threadsafe_example();
}
