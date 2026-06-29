# idx containers

A C++17 containers library built for game engines and real-time systems.

Every container ships in two identical-API flavours: `basic::` (no locking) and `threadSafe::` (shared/unique mutex). Switch between them by changing one word. All storage flows through a single pluggable `Allocator`, and every integer type is explicit — you choose `uint8`, `uint32`, or `uint64` as the `sizeType` template parameter.

---

## Why not just use `std::`?

| Feature | idx | std |
|---|---|---|
| Typed stable index handles | ✅ `Index<T, sizeType>` | ❌ raw integers / iterators |
| Zero-cost threadSafe drop-in | ✅ change one word | ❌ requires external locking |
| SSO string in exactly 16 bytes | ✅ | ❌ implementation-defined |
| Inline-first hybrid array | ✅ `HybridArray<T, N>` | ❌ |
| O(1) min/max sorted structure | ✅ `SortedArray` | ❌ |
| Explicit `sizeType` per container | ✅ | ❌ always `size_t` |
| Custom allocator without template boilerplate | ✅ global `Allocator` | ❌ per-container template param |

---

## Containers at a glance

| Header | Container | Description |
|---|---|---|
| `dynamicArray.hpp` | `DynamicArray<T, S>` | Heap-resizing array. O(1) amortised push, O(1) erase (swap-with-last). |
| `staticArray.hpp` | `StaticArray<T, N, S>` | Compile-time fixed capacity, fully inline — zero heap. |
| `hybridArray.hpp` | `HybridArray<T, N, S>` | First N elements inline, overflow to heap. No allocation until N exceeded. |
| `indexedDynamicArray.hpp` | `IndexedDynamicArray<T, S>` | Slot-map: O(1) insert/erase with stable typed `Index` handles. |
| `indexedStaticArray.hpp` | `IndexedStaticArray<T, N, S>` | Fixed-capacity slot-map; caller picks the exact slot per insert. |
| `stack.hpp` | `Stack<T, S>` | LIFO stack backed by `DynamicArray`. |
| `queue.hpp` | `Queue<T, S>` | FIFO queue. Each `push()` returns a stable `Index` for O(1) direct access. |
| `sortedArray.hpp` | `SortedArray<T, S>` | Sorted doubly-linked list in a slot-map. O(1) min/max, O(n/2) insert/find. |
| `hashMap.hpp` | `HashMap<K, V, S>` | Separate-chaining hash table with FNV-1a, stable `Index` per entry. |
| `BSTree.hpp` | `BinarySearchTree<T, S>` | Unbalanced BST. O(log n) average, O(n) worst case on sorted input. |
| `AVLTree.hpp` | `AVLTree<T, S>` | Self-balancing BST. O(log n) guaranteed for all operations. |
| `bitMap.hpp` | `BitMap<T, S>` | Compact boolean array, 1 bit per element, grows automatically. |
| `string.hpp` | `String` | SSO string: ≤14 chars inline, longer on heap. Always 16 bytes. |

All containers exist in both `basic::` and `threadSafe::` namespaces.

---

## The typed Index handle

The most distinctive feature of this library. Every indexed container returns `Index<T, sizeType>` instead of a raw integer:

```cpp
basic::IndexedDynamicArray<Player, uint32> players;
auto playerIdx  = players.insert(Player{"Alice"});
auto enemyArr   = basic::IndexedDynamicArray<Enemy, uint32>{};
auto enemyIdx   = enemyArr.insert(Enemy{});

players[playerIdx];  // ✅
players[enemyIdx];   // ❌ compile error — wrong type
```

Passing an `Index<Enemy, uint32>` where `Index<Player, uint32>` is expected is a **compile error**, not a silent out-of-bounds access. You get type-safe handles at zero runtime cost.

Indices returned by `IndexedDynamicArray`, `IndexedStaticArray`, `Queue`, `SortedArray`, `HashMap`, `BSTree`, and `AVLTree` remain **valid even after other elements are inserted or erased** — only erasing the element itself invalidates its handle.

---

## Zero-cost dual API

Every container exists in both namespaces with an identical public interface:

```cpp
// Prototype with no locking overhead:
basic::DynamicArray<int, uint32> local;

// Ship with full thread safety — change one word:
threadSafe::DynamicArray<int, uint32> shared;
```

`threadSafe::` containers use `std::shared_mutex`: reads acquire a shared lock (concurrent reads never block each other), writes acquire a unique lock.

---

## sizeType is your choice

Every container is templated on `sizeType`. Pick the smallest type that fits your data to save memory in index tables and free-list arrays:

```cpp
// A pool that will never hold more than 255 entries
basic::IndexedDynamicArray<Particle, uint8> particles;

// A global asset registry that might hold millions
basic::IndexedDynamicArray<Asset, uint64> assets;
```

---

## HybridArray — inline first, heap on overflow

```cpp
// First 4 elements live on the stack frame — zero allocation
basic::HybridArray<Transform, 4, uint32> transforms;

transforms.pushBack(t1); // inline
transforms.pushBack(t2); // inline
transforms.pushBack(t3); // inline
transforms.pushBack(t4); // inline
transforms.pushBack(t5); // spills to heap, invisibly
```

Ideal for small collections in hot paths (component lists, child node arrays) that occasionally grow large.

---

## Iterators

All containers support range-for and the standard begin/end interface:

```cpp
basic::AVLTree<int, uint32> tree;
for (int v : tree) { /* ... */ }
```

`basic::iterator` has no locking. `threadSafe::iterator` holds a `shared_lock` for its full lifetime — writers on the same container block until the iterator is destroyed:

```cpp
{
    auto it = arr.begin();   // shared_lock acquired
    for (; !it.isEnd(); ++it) {
        process(*it);
    }
}   // shared_lock released — writers may now proceed
```

If you need to mutate the container from the same thread while iterating, call `it.unlock()` first, perform the mutation, then call `it.lock()`:

```cpp
for (auto it = arr.begin(); it != arr.end(); ++it) {
    if (*it == target) {
        it.unlock();
        arr.erase(it.index());
        it.lock();
        break;
    }
}
```

**Note:** `threadSafe::iterator` post-increment (`it++`) is deleted — the lock cannot be copied into a by-value return. Use pre-increment (`++it`) in all loops.

---

## String

```cpp
basic::String a("hello");              // SSO — 16 bytes total, no allocation
basic::String b("longer than 14 chars"); // heap-allocated transparently

basic::String c = a + b;              // concatenation
a += basic::String(" world");          // append in place
(sint8*)"prefix " + a;                // prepend via free function

// Free functions
basic::String n = toString1((sint64)-42);  // "-42"
basic::String d = toString2(3.14);         // "3.1400000000000001" (16 decimals)
basic::String p = toString3((void*)ptr);   // hex address, always 16 chars
```

---

## Assertions

In debug builds (`idxCONFIG_DEBUG`), out-of-bounds accesses, use-after-erase, and double-erases trigger `idxASSERT` — which prints the file and line, then fires a platform-specific debug break so you can inspect the call stack.

In release builds, a failed assertion prints the message and **spins forever** (intentional hard stop) rather than silently corrupting state.

---

## Things to know before you use this

**The library uses `new` and `delete` operators in the file rawPointer.hpp. If you need to use an allocaotr of your own, just edit the allocate
and deallocate functions in the RawPointer to use your allocator, this is the ONLY place in the whole lib where allocation/deallocation happens**

**`erase(T)` on `IndexedDynamicArray` removes the first match only**, not all matching elements. This differs from the comment in some earlier versions of the header. If you need to remove all matches, iterate and erase by `Index`.

**`BinarySearchTree` is unbalanced.** Inserting sorted data degenerates to O(n). Use `AVLTree` when insertion order is not controlled.

**`SortedArray` iteration is not in sorted order.** The iterator visits slots in backing-store order, not the linked sorted order. To consume elements in sorted order, repeatedly call `minimum()` and `erase(minimum())`.

**`HashMap` does not update the value on duplicate insert.** `insert(key, newValue)` returns the existing index and leaves the stored value unchanged. Use `operator[]` or retrieve the index and write through it directly.

**`HashMap` with struct keys requires care around padding.** The FNV-1a hash operates on raw bytes. Two structs that are logically equal but have different padding bytes will hash differently. Zero-initialise structs or hash field-by-field.

**Iterator post-increment is deleted on `threadSafe::iterator`.** The `shared_lock` cannot be copied into a by-value return. Use `++it`.

---

## Building

The library is header-only. Include the headers you need

Compile with C++17 or later and link with pthreads when using `threadSafe::` containers

---

## Examples

Each container has a standalone example in the `examples/` directory. Every example compiles with:

```sh
g++ -std=c++17 -I path/to/containers examples/ex_NAME.cpp -pthread -o example_NAME
```

| File | Covers |
|---|---|
| `ex_dynamicArray.cpp` | pushBack, erase, find, resize, reserve, wrap, copy, move, iteration |
| `ex_staticArray.cpp` | fixed capacity, setSize, inline storage, thread-safe concurrent push |
| `ex_hybridArray.cpp` | inline-first growth, tier-crossing erase, cross-type copy |
| `ex_indexedDynamicArray.cpp` | stable handles, free-list reuse, typed Index safety, sparse iteration |
| `ex_indexedStaticArray.cpp` | caller-chosen slots, bitmap occupancy, find by Index |
| `ex_stack.cpp` | push, pop, top, indexed access, bottom-to-top iteration |
| `ex_queue.cpp` | push with Index return, FIFO pop, erase by value, task dispatch pattern |
| `ex_sortedArray.cpp` | O(1) min/max, duplicate rejection, priority-like usage |
| `ex_hashMap.cpp` | insert/find/erase, operator[], key() accessor, rehash stress |
| `ex_BSTree.cpp` | leaf/one-child/two-child erase, in-order successor strategy |
| `ex_AVLTree.cpp` | sequential insert safety, all four rotation types, guaranteed O(log n) |
| `ex_bitMap.cpp` | toggleOn/Off, packed storage, entity active-flag pattern |
| `ex_string.cpp` | SSO, heap path, concatenation, self-append, toString free functions |
| `ex_iterator.cpp` | all iterator patterns: range-for, reverse, unlock/lock, sparse containers |
