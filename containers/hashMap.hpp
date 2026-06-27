// MIT License

// Copyright (c) 2026 Ssebunya Umar

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.



// ─────────────────────────────────────────────────────────────────────────────
// HashMap<keyType, valueType, sizeType>
//
// A hash table using separate chaining (each bucket is a singly-linked list
// stored inside an IndexedDynamicArray).
//
// HASH FUNCTION
//   toSizeType() converts the key to a sizeType integer.  Supported key types:
//   sint16/32/64, uint16/32/64, void* (pointer cast), and sint8* (sum of bytes).
//   For custom key types, add a toSizeType() overload in HashMapBase or ensure
//   your key is implicitly convertible to one of the above types.
//   WARNING: The string hash (sum of bytes) is very weak and will cluster badly
//   for common English strings.  Consider using a proper string hash (FNV, etc.)
//   if HashMap<sint8*, V, S> is used with many keys.
//
// GROWTH
//   When size() == mHeadIndicies.size() * WIDTH (WIDTH = 20), the table is
//   rebuilt with double the number of buckets.  All existing nodes are
//   re-inserted from a temporary copy.  This is O(n) and briefly doubles
//   memory usage.
//
// INSERT BEHAVIOUR
//   Inserting a key that already exists returns the existing index WITHOUT
//   updating its value.  Use operator[] to get or insert + update.
//
// operator[] creates the entry with a default-constructed value if the key
//   does not exist (same semantics as std::unordered_map).
//
// ITERATION
//   Iterates in slot order of the internal IndexedDynamicArray, not bucket order.
//   Use it.key<keyType>() and it.value() to access key-value pairs.
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include"indexedDynamicArray.hpp"

#include<shared_mutex>
#include<mutex>

namespace idx {
	
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	template<typename keyType, typename valueType, typename sizeType>
	class HashMapBase {

	private:
		
		using IDX = Index<valueType, sizeType>;

	protected:

#define WIDTH 20

		struct Node {
			keyType key;
			valueType value;
			sizeType next = INVALID_IDX(sizeType);

			Node() : key(keyType()), value(valueType()) {}
			Node(const keyType& k, const valueType& v) : key(k), value(v) {}
		};

	protected:

		basic::IndexedDynamicArray<Node, sizeType> mData;
		basic::DynamicArray<sizeType, sizeType> mHeadIndicies;

	protected:
		// FNV-1a 64-bit hash over `len` raw bytes starting at `data`.
		// Basis and prime from the FNV spec: http://www.isthe.com/chongo/tech/comp/fnv/
		// `data` – pointer to the byte sequence to hash.
		// `len`  – number of bytes to consume.
		// Returns a uint64 hash value suitable for modding into the bucket array.
		static uint64 fnv1a(const void* data, const uint64 len)
		{
			constexpr uint64 basis = 14695981039346656037ULL;
			constexpr uint64 prime = 1099511628211ULL;

			const uint8* bytes = (const uint8*)data;
			uint64 hash = basis;
			for (uint64 i = 0; i < len; ++i) {
				hash ^= bytes[i];
				hash *= prime;
			}
			return hash;
		}

		// Integer overloads — hash the raw bytes of the value directly.
		// For small integers (uint8, uint16) this is equivalent to identity hashing
		// but with better avalanche into the upper bits.
		sizeType toSizeType(const sint16& v)  { return (sizeType)fnv1a(&v, sizeof(v)); }
		sizeType toSizeType(const sint32& v)  { return (sizeType)fnv1a(&v, sizeof(v)); }
		sizeType toSizeType(const sint64& v)  { return (sizeType)fnv1a(&v, sizeof(v)); }
		sizeType toSizeType(const uint16& v)  { return (sizeType)fnv1a(&v, sizeof(v)); }
		sizeType toSizeType(const uint32& v)  { return (sizeType)fnv1a(&v, sizeof(v)); }
		sizeType toSizeType(const uint64& v)  { return (sizeType)fnv1a(&v, sizeof(v)); }

		// Pointer — hash the address value, not what it points to.
		sizeType toSizeType(const void* ptr)
		{
			uint64 addr = (uint64)ptr;
			return (sizeType)fnv1a(&addr, sizeof(addr));
		}

		// C-string — hash every character up to (not including) the null terminator.
		sizeType toSizeType(const sint8* str)
		{
			uint64 len = 0;
			while (str[len] != '\0') ++len;
			return (sizeType)fnv1a(str, len);
		}
		
		// Computes the bucket index for `key` by mapping its sizeType representation
		// into [0, mHeadIndicies.size()) via modulo.
		// `key` – the key to hash.
		// Returns the bucket index for this key.
		sizeType hashValue(const keyType& key) { return (toSizeType(key) % this->mHeadIndicies.size()); }

		// Resizes the bucket array to `mapSize` and initialises every head to INVALID_IDX.
		// Called on first insert (initial size = 5) and on rehash (double the bucket count).
		// `mapSize` – the desired number of buckets.
		void setHeadIndicies(const sizeType& mapSize)
		{
			this->mHeadIndicies.resize(mapSize);
			for (sizeType x = 0; x < mapSize; ++x) {
				this->mHeadIndicies[x] = INVALID_IDX(sizeType);
			}
		}

		// Inserts `key`/`value` into the hash table.
		// If `key` already exists, returns the existing node's Index (value is NOT updated).
		// If size() reaches mHeadIndicies.size() * WIDTH, the table is rehashed to double
		// the bucket count before inserting (using `size` as the pre-rehash element count).
		// `key`   – the lookup key.
		// `value` – the value to store if the key is new.
		// `size`  – the current element count (must be the live count, not post-rehash).
		// Returns an Index wrapping the slot of the (existing or new) node.
		IDX insertInternal(const keyType& key, const valueType& value, const sizeType& size)
		{
			if (this->mHeadIndicies.size() == 0) {
				this->setHeadIndicies(5);
			}
			else if (size == this->mHeadIndicies.size() * WIDTH) {

				basic::IndexedDynamicArray<Node, sizeType> temp;
				this->mData.swap(temp);

				this->setHeadIndicies(this->mHeadIndicies.size() * 2);

				for (auto it = temp.begin(); !it.isEnd(); ++it) {
					this->insertInternal(it.data().key, it.data().value, size);
				}
			}

			sizeType hash = hashValue(key);

			sizeType temp_idx = this->mHeadIndicies[hash];
			sizeType last = INVALID_IDX(sizeType);
			while (temp_idx != INVALID_IDX(sizeType)) {

				if (this->mData[temp_idx].key == key) {
					return IDX(temp_idx); // Key already exists — return without updating.
				}

				last = temp_idx;
				temp_idx = this->mData[temp_idx].next;
			}

			auto index = this->mData.insert(Node(key, value));

			if (this->mHeadIndicies[hash] == INVALID_IDX(sizeType)) {
				this->mHeadIndicies[hash] = index.ref();
			}
			else {
				this->mData[last].next = index.ref();
			}

			return IDX(index.ref());
		}

		// Removes the entry with `key` from the hash table, if it exists.
		// Repairs the chain by linking the predecessor to the successor.
		// Clears the erased node's fields before freeing the slot.
		// No-op if `key` is not found or the bucket array is empty.
		// `key` – the key of the entry to remove.
		void eraseInternal(const keyType& key)
		{
			if (this->mHeadIndicies.size() == 0) return;

			sizeType hash = hashValue(key);

			sizeType index = this->mHeadIndicies[hash];
			sizeType prev = INVALID_IDX(sizeType);
			while (index != INVALID_IDX(sizeType)) {

				if (this->mData[index].key == key) {

					if (index == this->mHeadIndicies[hash]) {
						this->mHeadIndicies[hash] = this->mData[this->mHeadIndicies[hash]].next;
					}
					else {
						this->mData[prev].next = this->mData[index].next;
					}

					this->mData[index].value = valueType();
					this->mData[index].key = keyType();
					this->mData[index].next = INVALID_IDX(sizeType);

					this->mData.erase(Index<Node, sizeType>(index));

					return;
				}

				prev = index;
				index = this->mData[index].next;
			}
		}

		// Walks the chain for `key`'s bucket to find the matching node.
		// Returns an invalid Index if the bucket array is empty or the key is not found.
		// `key` – the key to look up.
		// Returns a valid Index wrapping the matching slot, or an invalid Index.
		IDX findInternal(const keyType& key)
		{
			if (this->mHeadIndicies.size() > 0) {
				sizeType hash = (toSizeType(key) % this->mHeadIndicies.size());
				sizeType index = this->mHeadIndicies[hash];

				while (index != INVALID_IDX(sizeType)) {
					if (this->mData[index].key == key) {
						return IDX(index);
					}

					index = this->mData[index].next;
				}
			}

			return IDX();
		}

		// Returns a mutable reference to the value for `key`.
		// If `key` does not exist, inserts it with a default-constructed value first
		// (same semantics as std::unordered_map::operator[]).
		// `key`  – the key to look up or insert.
		// `size` – the current element count, used if a rehash is triggered on insert.
		// Returns a reference to the value associated with `key`.
		valueType& valueRef(const keyType& key, const sizeType& size)
		{
			if(this->mHeadIndicies.size() > 0) {
				sizeType index = this->mHeadIndicies[hashValue(key)];
				while (index != INVALID_IDX(sizeType)) {
					if (this->mData[index].key == key) {
						return this->mData[index].value;
					}
					index = this->mData[index].next;
				}
			}
			
			auto index = this->insertInternal(key, valueType(), size);
			return this->mData[index.ref()].value;
		}

		// Default constructor. Bucket array and node store start empty.
		HashMapBase() {}

		// Capacity constructor. Pre-allocates `mapSize` buckets, all set to INVALID_IDX.
		// Use when the expected element count is known to avoid early rehashes.
		// `mapSize` – the initial number of buckets to allocate.
		HashMapBase(const sizeType& mapSize)
		{
			this->setHeadIndicies(mapSize);
		}

		~HashMapBase() {}

	public:
	
		// REQUIRED by ITERATOR
		// Returns the next live raw slot after `index`. Used internally by Iterator.
		// NOTE: visits nodes in slot-insertion order, NOT in-order (sorted) traversal.
		// `index` – the current raw slot position.
		sizeType nextIndex(const sizeType& index)
		{
			return this->mData.nextIndex(index);
		}

		// REQUIRED by ITERATOR
		// Returns the previous live raw slot before `index`.
		// NOTE: visits nodes in slot-insertion order, NOT in-order (sorted) traversal.
		// `index` – the current raw slot position.
		sizeType prevIndex(const sizeType& index)
		{
			return this->mData.prevIndex(index);
		}

		// REQUIRED by ITERATOR
		// Returns a reference to the node data at raw slot `index` — required by Iterator.
		valueType& ref(const sizeType& index) { return this->mData[index].value; }

		// REQUIRED by ITERATOR
		// Const overload of ref().
		const valueType& ref(const sizeType& index) const { return this->mData[index].value; }
	};

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	namespace basic {
			
		template<typename keyType, typename valueType, typename sizeType>
		class HashMap : public HashMapBase<keyType, valueType, sizeType> {

		public:
			
			using value_type = valueType;
			using key_type = keyType;
			using size_type = sizeType;
			using iterator = Iterator<HashMap<keyType, valueType, sizeType>>;
			using const_iterator = const iterator;
			using IDX = Index<valueType, sizeType>;

		public:
			
			// Default constructor. Bucket array and node store start empty.
			HashMap() : HashMapBase<keyType, valueType, sizeType>() {}

			// Capacity constructor. Pre-allocates `mapSize` buckets to reduce rehash frequency.
			// `mapSize` – the initial number of hash buckets.
			HashMap(const sizeType& mapSize) : HashMapBase<keyType, valueType, sizeType>(mapSize) {}

			// Swaps the node store and bucket array between this and `other` in O(1).
			// `other` – the HashMap to swap with. No-op if this == &other.
			void swap(HashMap<keyType, valueType, sizeType>& other)
			{
				if (this == &other) return;
				this->mData.swap(other.mData);
				this->mHeadIndicies.swap(other.mHeadIndicies);
			}

			// Inserts `key`/`value` into the table.
			// If `key` already exists, returns its existing Index WITHOUT updating the value.
			// `key`   – the lookup key.
			// `value` – the value to store if the key is new.
			// Returns an Index wrapping the (existing or new) node's slot.
			IDX insert(const keyType& key, const valueType& value)
			{
				return this->insertInternal(key, value, this->size());
			}

			// Removes the entry with `key` from the table. No-op if key is not found.
			// `key` – the key of the entry to remove.
			void erase(const keyType& key)
			{
				this->eraseInternal(key);
			}

			// Looks up `key` in the table.
			// `key` – the key to search for.
			// Returns a valid Index if found, or an invalid Index if not found.
			IDX find(const keyType& key)
			{
				return this->findInternal(key);
			}

			// Returns a mutable reference to the value for `key`.
			// If `key` is absent, inserts it with a default-constructed value (like std::unordered_map).
			// `key` – the key to look up or create.
			// Returns valueType& – a reference to the value (existing or newly default-constructed).
			valueType& operator[](const keyType& key)
			{
				return this->valueRef(key, this->size());
			}

			// Returns the key stored at the slot indicated by typed `index`.
			// `index` – a typed Index from insert() or find(). Must be live.
			// Returns a const reference to the key at that slot.
			const keyType& key(const IDX& index) const
			{
				return this->mData[index.ref()].key;
			}

			// Returns the key at raw slot `index`.
			// `index` – raw slot in the backing IndexedDynamicArray. Must be live.
			// Returns a const reference to the key at that slot.
			const keyType& key(const sizeType& index) const
			{
				return this->mData[index].key;
			}

			// Returns a mutable reference to the value at typed `index`.
			// `index` – a typed Index from insert() or find(). Must be live.
			valueType& value(const IDX& index)
			{
				return this->mData[index.ref()].value;
			}

			// Const overload: returns the value at typed `index`.
			const valueType& value(const IDX& index) const
			{
				return this->mData[index.ref()].value;
			}

			// Returns a mutable reference to the value at raw slot `index`.
			// `index` – raw slot in the backing store. Must be live.
			valueType& value(const sizeType& index)
			{
				return this->mData[index].value;
			}

			// Const overload: returns the value at raw slot `index`.
			const valueType& value(const sizeType& index) const
			{
				return this->mData[index].value;
			}

			// Returns true when there are no entries in the table.
			bool empty() const
			{
				return this->size() == 0;
			}

			// Returns the number of live key-value entries.
			sizeType size() const
			{
				return this->mData.size();
			}

			// Removes all entries but keeps the bucket array allocated and reset to INVALID_IDX.
			// `callDestructors` – if true, ~valueType() is called on each value before reset.
			void shallowClear(bool callDestructors)
			{
				this->mData.shallowClear(callDestructors);
				for (sizeType x = 0, len = this->mHeadIndicies.size(); x < len; ++x) {
					this->mHeadIndicies[x] = INVALID_IDX(sizeType);
				}
			}

			// Destroys all entries and frees both the node store and the bucket array.
			void deepClear()
			{
				this->mData.deepClear();
				this->mHeadIndicies.deepClear();
			}
			
			#define type HashMap<keyType, valueType, sizeType>
			BASIC_ITERATOR_FUNCTIONS(type)
			#undef type
		};
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	namespace threadSafe {
			
		template<typename keyType, typename valueType, typename sizeType>
		class HashMap : public HashMapBase<keyType, valueType, sizeType> {

		public:
			
			using value_type = valueType;
			using key_type = keyType;
			using size_type = sizeType;
			using iterator = Iterator<HashMap<keyType, valueType, sizeType>>;
			using const_iterator = const iterator;
			using IDX = Index<valueType, sizeType>;

		private:

			mutable std::shared_mutex mMutex;

		public:
			
			// Default constructor.
			HashMap() : HashMapBase<keyType, valueType, sizeType>() {}

			// Capacity constructor. Pre-allocates `mapSize` buckets.
			// `mapSize` – the initial number of hash buckets.
			HashMap(const sizeType& mapSize) : HashMapBase<keyType, valueType, sizeType>(mapSize) {}

			// Thread-safe swap. Acquires a scoped lock on both maps.
			// `other` – the HashMap to swap with. No-op if this == &other.
			void swap(HashMap<keyType, valueType, sizeType>& other)
			{
				if (this == &other) return;
				std::scoped_lock lock(this->mMutex, other.mMutex);
				this->mData.swap(other.mData);
				this->mHeadIndicies.swap(other.mHeadIndicies);
			}

			// Thread-safe insert. Acquires a unique lock.
			// If `key` already exists, returns its index WITHOUT updating the value.
			// `key`   – the lookup key. `value` – the value to store if key is new.
			// Returns the Index of the (existing or new) node.
			IDX insert(const keyType& key, const valueType& value)
			{
				std::unique_lock lock(this->mMutex);
				return this->insertInternal(key, value, this->size());
			}

			// Thread-safe erase. Acquires a unique lock.
			// Removes the entry with `key`. No-op if not found.
			void erase(const keyType& key)
			{
				std::unique_lock lock(this->mMutex);
				this->eraseInternal(key);
			}

			// Thread-safe find. Acquires a shared lock.
			// `key` – the key to look up.
			// Returns a valid Index on match, or an invalid Index if not found.
			IDX find(const keyType& key)
			{
				std::shared_lock lock(this->mMutex);
				return this->findInternal(key);
			}

			// Thread-safe operator[]. Acquires a unique lock.
			// Inserts key with a default value if absent (like std::unordered_map).
			// `key` – the key to get or create.
			// Returns a reference to the associated value.
			valueType& operator[](const keyType& key)
			{
				std::unique_lock lock(this->mMutex);
				return this->valueRef(key, this->size());
			}

			// Thread-safe key access by typed IDX. Acquires a shared lock.
			// `index` – a typed Index from insert() or find(). Must be live.
			// Returns a const reference to the key at that slot.
			const keyType& key(const IDX& index) const
			{
				std::shared_lock lock(this->mMutex);
				return this->mData[index.ref()].key;
			}

			// Thread-safe key access by raw slot. Acquires a shared lock.
			// `index` – raw slot. Must be live.
			const keyType& key(const sizeType& index) const
			{
				std::shared_lock lock(this->mMutex);
				return this->mData[index].key;
			}

			// Thread-safe value access by typed IDX. Acquires a shared lock.
			valueType& value(const IDX& index)
			{
				std::shared_lock lock(this->mMutex);
				return this->mData[index.ref()].value;
			}

			// Const thread-safe value access by typed IDX.
			const valueType& value(const IDX& index) const
			{
				std::shared_lock lock(this->mMutex);
				return this->mData[index.ref()].value;
			}

			// Thread-safe value access by raw slot. Acquires a shared lock.
			valueType& value(const sizeType& index)
			{
				std::shared_lock lock(this->mMutex);
				return this->mData[index].value;
			}

			// Const thread-safe value access by raw slot.
			const valueType& value(const sizeType& index) const
			{
				std::shared_lock lock(this->mMutex);
				return this->mData[index].value;
			}

			// Thread-safe empty(). Acquires a shared lock.
			bool empty() const
			{
				std::shared_lock lock(this->mMutex);
				return this->size() == 0;
			}

			// Thread-safe size(). Acquires a shared lock. Returns live entry count.
			sizeType size() const
			{
				std::shared_lock lock(this->mMutex);
				return this->mData.size();
			}

			// Thread-safe shallowClear(). Acquires a unique lock.
			// `callDestructors` – invoke ~valueType() on each value if true.
			void shallowClear(bool callDestructors)
			{
				std::unique_lock lock(this->mMutex);
				this->mData.shallowClear(callDestructors);
				for (sizeType x = 0, len = this->mHeadIndicies.size(); x < len; ++x) {
					this->mHeadIndicies[x] = INVALID_IDX(sizeType);
				}
			}

			// Thread-safe deepClear(). Acquires a unique lock.
			// Destroys all entries and frees both the node store and bucket array.
			void deepClear()
			{
				std::unique_lock lock(this->mMutex);
				this->mData.deepClear();
				this->mHeadIndicies.deepClear();
			}
			
			// Returns the next live raw slot after `index`, or INVALID_IDX at the end.
			sizeType nextIndex(const sizeType& index)
			{
				return this->mData.nextIndex(index);
			}
			
			valueType& ref(const sizeType& index) { return this->mData[index].value; }
			const valueType& ref(const sizeType& index) const { return this->mData[index].value; }
			
			#define type HashMap<keyType, valueType, sizeType>
			THREAD_SAFE_ITERATOR_FUNCTIONS(type)
			#undef type
		};
	}
}
