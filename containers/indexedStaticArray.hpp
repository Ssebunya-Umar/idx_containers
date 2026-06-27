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



#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// IndexedStaticArray<T, capacity, sizeType>
//
// Like IndexedDynamicArray but with compile-time fixed capacity and all
// storage inline (no heap allocation). The caller explicitly chooses which
// slot each element occupies via insert(data, index).
// ─────────────────────────────────────────────────────────────────────────────

#include"iterator.hpp"
#include"core/core.hpp"
#include"core/types.hpp"
#include"core/assert.hpp"
#include"core/index.hpp"

#include<shared_mutex>
#include<mutex>

namespace idx {

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	template<typename T, uint64 capacity, typename sizeType,
			sizeType bitWidth = sizeof(sizeType) * 8,
			sizeType bitMapSize = capacity % bitWidth == 0 ? (capacity / bitWidth) : (capacity / bitWidth + 1)>
	class IndexedStaticArrayBase {

	private:

		using IDX = Index<T, sizeType>;

	protected:

		T mData[capacity] = {};            // Inline storage for all capacity slots
		sizeType mBitMap[bitMapSize] = {}; // Packed occupancy bitmap
		sizeType mSize = 0;                // Number of currently live slots

	protected:

		// Returns true if slot `index` is currently occupied (its bitmap bit is set).
		// `index` – the raw slot position to check (must be < capacity).
		bool statusInternal(const sizeType& index) const
		{
			return (this->mBitMap[index / bitWidth] & static_cast<sizeType>(BIT(index % bitWidth))) != 0;
		}

		// Sets the bitmap bit for slot `index` to 1 (marks it as occupied).
		// `index` – the raw slot position to mark live.
		void toggleOnInternal(const sizeType& index)
		{
			this->mBitMap[index / bitWidth] |= static_cast<sizeType>(BIT(index % bitWidth));
		}

		// Clears the bitmap bit for slot `index` to 0 (marks it as free).
		// `index` – the raw slot position to mark free.
		void toggleOffInternal(const sizeType& index)
		{
			this->mBitMap[index / bitWidth] &= ~static_cast<sizeType>(BIT(index % bitWidth));
		}

		// Places `data` into slot `index` and marks it as occupied.
		// `data`  – the value to copy-assign into the slot.
		// `index` – the exact slot to use. Must be < capacity, currently unoccupied,
		//           and mSize must be < capacity (all asserted).
		void insertInternal(const T& data, const sizeType& index)
		{
			idxASSERT(index >= 0 && index < capacity, "index is out of range");
			idxASSERT(this->statusInternal(index) == false, "index is already occupied");
			idxASSERT(this->mSize < capacity, "array is already full");
			this->mData[index] = data;
			this->toggleOnInternal(index);
			++this->mSize;
		}

		// Clears slot `index` and marks it as free.
		// `index` – the slot to erase. Must be < capacity and currently occupied (asserted).
		void eraseInternal(const sizeType& index)
		{
			idxASSERT(index >= 0 && index < capacity, "index is out of range");
			idxASSERT(this->statusInternal(index) == true, "index is already empty");
			this->mData[index] = T();
			this->toggleOffInternal(index);
			--this->mSize;
		}

		// Linear O(n) search through occupied slots.
		// `data` – the value to search for.
		// Returns a valid IDX wrapping the first match, or an invalid IDX if not found.
		IDX findInternal(const T& data) const
		{
			for (sizeType x = 0; x < capacity; ++x) {
				if (this->statusInternal(x) && this->mData[x] == data) {
					return IDX(x);
				}
			}
			return IDX();
		}

		// Copies all capacity slots and the entire bitmap from `other`.
		// `other` – the array to copy from. No-op if this == &other.
		void cloneInternal(const IndexedStaticArrayBase<T, capacity, sizeType, bitWidth, bitMapSize>& other)
		{
			if (this == &other) return;
			for (sizeType x = 0; x < capacity; ++x) {
				this->mData[x] = other.mData[x];
			}
			for (sizeType x = 0; x < bitMapSize; ++x) {
				this->mBitMap[x] = other.mBitMap[x];
			}
			this->mSize = other.mSize;
		}

		// Move-assigns all capacity slots from `other` and clears other's bitmap and size.
		// `other` – the array to steal from. No-op if this == &other.
		void hijackInternal(IndexedStaticArrayBase<T, capacity, sizeType, bitWidth, bitMapSize>& other)
		{
			if (this == &other) return;
			for (sizeType x = 0; x < capacity; ++x) {
				this->mData[x] = std::move(other.mData[x]);
			}
			for (sizeType x = 0; x < bitMapSize; ++x) {
				this->mBitMap[x] = other.mBitMap[x];
				other.mBitMap[x] = 0;
			}
			this->mSize = other.mSize;
			other.mSize = 0;
		}

		// Resets all slots to T(), clears the entire bitmap, and sets mSize = 0.
		void deepClearInternal()
		{
			for (sizeType x = 0; x < capacity; ++x) {
				this->mData[x] = T();
			}
			for (sizeType x = 0; x < bitMapSize; ++x) {
				this->mBitMap[x] = 0;
			}
			this->mSize = 0;
		}

		// Access by typed IDX. Checks that `index` is in range (asserted).
		// Note: does NOT check the bitmap — the slot may be free.
		// `index` – typed index. Must be < capacity.
		// Returns a mutable reference to mData[index].
		T& refInternal(const sizeType& index)
		{
			idxASSERT(index >= 0 && index < capacity, "index is out of range");
			return this->mData[index];
		}

		// Access by typed IDX. Checks that `index` is in range (asserted).
		// Note: does NOT check the bitmap — the slot may be free.
		// `index` – typed index. Must be < capacity.
		// Returns a non mutable reference to mData[index].
		const T& refInternal(const sizeType& index) const
		{
			idxASSERT(index >= 0 && index < capacity, "index is out of range");
			return this->mData[index];
		}

		IndexedStaticArrayBase() {}
		~IndexedStaticArrayBase() {}

	public:
		
		// REQUIRED by ITERATOR
		// Returns the next occupied slot index after `index`, or INVALID_IDX at the end.
		// Iterates up to mSize (the high-water mark), not capacity.
		// `index` – the current raw slot position.
		sizeType nextIndex(const sizeType& index)
		{
			sizeType idx = index + 1;
			while (idx < this->mSize) {
				if (this->statusInternal(idx) == true) {
					return idx;
				}
				++idx;
			}
			return INVALID_IDX(sizeType);
		}

		// REQUIRED by ITERATOR
		// Returns the previous occupied slot index before `index`, or INVALID_IDX at 0.
		// `index` – the current raw slot position.
		sizeType prevIndex(const sizeType& index)
		{
			sizeType idx = index - 1;
			while (idx < this->mSize) {
				if (this->statusInternal(idx) == true) {
					return idx;
				}
				--idx;
			}
			return INVALID_IDX(sizeType);
		}

		// REQUIRED by ITERATOR
		// Returns a reference to the element at `index` — required by Iterator.
		T& ref(const sizeType& index) { return this->refInternal(index); }
		
		// REQUIRED by ITERATOR
		// Const overload of ref().
		const T& ref(const sizeType& index) const { return this->refInternal(index); }
	};

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	namespace basic {

		template<typename T, uint64 capacity, typename sizeType,
				sizeType bitWidth = sizeof(sizeType) * 8,
				sizeType bitMapSize = capacity % bitWidth == 0 ? (capacity / bitWidth) : (capacity / bitWidth + 1)>
		class IndexedStaticArray : public IndexedStaticArrayBase<T, capacity, sizeType> {

		public:

			using value_type = T;
			using size_type = sizeType;
			using iterator = Iterator<IndexedStaticArray<T, capacity, sizeType>>;
			using const_iterator = const iterator;
			using IDX = Index<T, sizeType>;

		public:

			// Default constructor. All slots empty; bitmap zeroed; mSize = 0.
			IndexedStaticArray() : IndexedStaticArrayBase<T, capacity, sizeType>() {}

			// Copy constructor.
			IndexedStaticArray(const IndexedStaticArray<T, capacity, sizeType, bitWidth, bitMapSize>& other) : IndexedStaticArrayBase<T, capacity, sizeType>()
			{
				this->cloneInternal(other);
			}

			// Move constructor.
			IndexedStaticArray(IndexedStaticArray<T, capacity, sizeType, bitWidth, bitMapSize>&& other) noexcept : IndexedStaticArrayBase<T, capacity, sizeType>()
			{
				this->hijackInternal(other);
			}

			// Copy assignment. Returns *this.
			IndexedStaticArray<T, capacity, sizeType, bitWidth, bitMapSize>& operator=(const IndexedStaticArray<T, capacity, sizeType, bitWidth, bitMapSize>& other)
			{
				if (this == &other) return *this;
				this->cloneInternal(other);
				return *this;
			}

			// Move assignment. Returns *this.
			IndexedStaticArray<T, capacity, sizeType, bitWidth, bitMapSize>& operator=(IndexedStaticArray<T, capacity, sizeType, bitWidth, bitMapSize>&& other) noexcept
			{
				if (this == &other) return *this;
				this->hijackInternal(other);
				return *this;
			}

			// Inserts `data` into the slot indicated by `index`.
			// `data`  – value to store.
			// `index` – typed IDX wrapping the desired slot. Must be unoccupied and in range.
			void insert(const T& data, const IDX& index)
			{
				this->insertInternal(data, index.ref());
			}

			// Inserts `data` into slot `index` (raw integer overload).
			// `data`  – value to store.
			// `index` – raw slot position. Must be < capacity, unoccupied, and mSize < capacity.
			void insert(const T& data, const sizeType& index)
			{
				this->insertInternal(data, index);
			}

			// Erases the slot indicated by typed `index`.
			// `index` – must be currently occupied (asserted).
			void erase(const IDX index)
			{
				this->eraseInternal(index.ref());
			}

			// Erases the raw slot `index`.
			// `index` – must be < capacity and currently occupied (asserted).
			void erase(const sizeType index)
			{
				this->eraseInternal(index);
			}

			// Linear O(n) search over occupied slots.
			// `data` – value to look for.
			// Returns a valid IDX on the first match, or an invalid IDX if not found.
			IDX find(const T& data)
			{
				return this->findInternal(data);
			}

			// Const overload of find().
			IDX find(const T& data) const
			{
				return this->findInternal(data);
			}

			// Access by typed IDX. Checks that `index` is in range (asserted).
			// Note: does NOT check the bitmap — the slot may be free.
			// `index` – typed index. Must be < capacity.
			// Returns a mutable reference to mData[index.ref()].
			T& operator[](const IDX& index)
			{
				return this->refInternal(index.ref());
			}

			// Const overload of operator[](IDX).
			const T& operator[](const IDX& index) const
			{
				return this->refInternal(index.ref());
			}

			// Access by raw slot index. Checks range against capacity (asserted).
			// `index` – raw slot. Must be < capacity.
			T& operator[](const sizeType& index)
			{
				return this->refInternal(index);
			}

			// Const overload of operator[](sizeType).
			const T& operator[](const sizeType& index) const
			{
				return this->refInternal(index);
			}

			// Returns a reference to mData[0]. Asserts if mSize == 0.
			T& front()
			{
				idxASSERT(this->mSize >= 1, "array is empty");
				return this->mData[0];
			}

			// Const overload of front().
			const T& front() const
			{
				idxASSERT(this->mSize >= 1, "array is empty");
				return this->mData[0];
			}

			// Returns a reference to mData[mSize - 1]. Asserts if mSize == 0.
			// NOTE: this may point to a free slot in a sparse array.
			T& back()
			{
				idxASSERT(this->mSize >= 1, "array is empty");
				return this->mData[this->mSize - 1];
			}

			// Const overload of back().
			const T& back() const
			{
				idxASSERT(this->mSize >= 1, "array is empty");
				return this->mData[this->mSize - 1];
			}

			// Returns the number of currently occupied slots.
			sizeType size() const { return this->mSize; }

			// Returns a raw pointer to the first slot.
			T* data()             { return this->mData; }
			const T* data() const { return this->mData; }

			// Returns true when mSize == 0.
			bool empty() const { return this->mSize == 0; }

			// Resets all slots to T(), zeroes the bitmap, and sets mSize = 0.
			void deepClear()
			{
				this->deepClearInternal();
			}

			#define type IndexedStaticArray<T, capacity, sizeType>
			BASIC_ITERATOR_FUNCTIONS(type)
			#undef type
		};
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	namespace threadSafe {

		template<typename T, uint64 capacity, typename sizeType,
				sizeType bitWidth = sizeof(sizeType) * 8,
				sizeType bitMapSize = capacity % bitWidth == 0 ? (capacity / bitWidth) : (capacity / bitWidth + 1)>
		class IndexedStaticArray : public IndexedStaticArrayBase<T, capacity, sizeType> {

		public:

			using value_type = T;
			using size_type = sizeType;
			using iterator = Iterator<IndexedStaticArray<T, capacity, sizeType>>;
			using const_iterator = const iterator;
			using IDX = Index<T, sizeType>;

		private:

			mutable std::shared_mutex mMutex;

		public:

			// Default constructor.
			IndexedStaticArray() : IndexedStaticArrayBase<T, capacity, sizeType>() {}

			// Copy constructor. Acquires shared lock on `other`.
			IndexedStaticArray(const IndexedStaticArray<T, capacity, sizeType, bitWidth, bitMapSize>& other) : IndexedStaticArrayBase<T, capacity, sizeType>()
			{
				std::shared_lock otherLock(other.mMutex);
				this->cloneInternal(other);
			}

			// Move constructor. Acquires unique lock on `other`.
			IndexedStaticArray(IndexedStaticArray<T, capacity, sizeType, bitWidth, bitMapSize>&& other) noexcept : IndexedStaticArrayBase<T, capacity, sizeType>()
			{
				std::unique_lock otherLock(other.mMutex);
				this->hijackInternal(other);
			}

			// Copy assignment. Locks both atomically. Returns *this.
			IndexedStaticArray<T, capacity, sizeType, bitWidth, bitMapSize>& operator=(const IndexedStaticArray<T, capacity, sizeType, bitWidth, bitMapSize>& other)
			{
				if (this == &other) return *this;
				std::scoped_lock lock(this->mMutex, other.mMutex);
				this->cloneInternal(other);
				return *this;
			}

			// Move assignment. Locks both atomically. Returns *this.
			IndexedStaticArray<T, capacity, sizeType, bitWidth, bitMapSize>& operator=(IndexedStaticArray<T, capacity, sizeType, bitWidth, bitMapSize>&& other) noexcept
			{
				if (this == &other) return *this;
				std::scoped_lock lock(this->mMutex, other.mMutex);
				this->hijackInternal(other);
				return *this;
			}

			// Thread-safe insert by IDX. Acquires a unique lock.
			void insert(const T& data, const IDX& index)
			{
				std::unique_lock lock(this->mMutex);
				this->insertInternal(data, index.ref());
			}

			// Thread-safe insert by raw slot index. Acquires a unique lock.
			void insert(const T& data, const sizeType& index)
			{
				std::unique_lock lock(this->mMutex);
				this->insertInternal(data, index);
			}

			// Thread-safe erase by IDX. Acquires a unique lock.
			void erase(const IDX index)
			{
				std::unique_lock lock(this->mMutex);
				this->eraseInternal(index.ref());
			}

			// Thread-safe erase by raw slot. Acquires a unique lock.
			void erase(const sizeType index)
			{
				std::unique_lock lock(this->mMutex);
				this->eraseInternal(index);
			}

			// Thread-safe find. Acquires a shared lock.
			IDX find(const T& data)
			{
				std::shared_lock lock(this->mMutex);
				return this->findInternal(data);
			}

			// Const thread-safe find. Acquires a shared lock.
			IDX find(const T& data) const
			{
				std::shared_lock lock(this->mMutex);
				return this->findInternal(data);
			}

			// Thread-safe operator[](IDX). Acquires a shared lock.
			T& operator[](const IDX& index)
			{
				std::shared_lock lock(this->mMutex);
				return this->refInternal(index.ref());
			}

			// Const thread-safe operator[](IDX).
			const T& operator[](const IDX& index) const
			{
				std::shared_lock lock(this->mMutex);
				return this->refInternal(index.ref());
			}

			// Thread-safe operator[](sizeType). Acquires a shared lock.
			T& operator[](const sizeType& index)
			{
				std::shared_lock lock(this->mMutex);
				return this->refInternal(index);
			}

			// Const thread-safe operator[](sizeType).
			const T& operator[](const sizeType& index) const
			{
				std::shared_lock lock(this->mMutex);
				return this->refInternal(index);
			}

			// Thread-safe front(). Acquires a shared lock. Asserts if empty.
			T& front()
			{
				std::shared_lock lock(this->mMutex);
				idxASSERT(this->mSize >= 1, "array is empty");
				return this->mData[0];
			}

			// Const thread-safe front().
			const T& front() const
			{
				std::shared_lock lock(this->mMutex);
				idxASSERT(this->mSize >= 1, "array is empty");
				return this->mData[0];
			}

			// Thread-safe back(). Acquires a shared lock. Asserts if empty.
			T& back()
			{
				std::shared_lock lock(this->mMutex);
				idxASSERT(this->mSize >= 1, "array is empty");
				return this->mData[this->mSize - 1];
			}

			// Const thread-safe back().
			const T& back() const
			{
				std::shared_lock lock(this->mMutex);
				idxASSERT(this->mSize >= 1, "array is empty");
				return this->mData[this->mSize - 1];
			}

			// Thread-safe size(). Acquires a shared lock.
			sizeType size() const
			{
				std::shared_lock lock(this->mMutex);
				return this->mSize;
			}

			// Thread-safe data(). Acquires a shared lock.
			T* data()
			{
				std::shared_lock lock(this->mMutex);
				return this->mData;
			}

			// Const thread-safe data().
			const T* data() const
			{
				std::shared_lock lock(this->mMutex);
				return this->mData;
			}

			// Thread-safe empty(). Acquires a shared lock.
			bool empty() const
			{
				std::shared_lock lock(this->mMutex);
				return this->mSize == 0;
			}

			// Thread-safe deepClear(). Acquires a unique lock.
			void deepClear()
			{
				std::unique_lock lock(this->mMutex);
				this->deepClearInternal();
			}

			#define type IndexedStaticArray<T, capacity, sizeType>
			THREAD_SAFE_ITERATOR_FUNCTIONS(type)
			#undef type
		};
	}
}
