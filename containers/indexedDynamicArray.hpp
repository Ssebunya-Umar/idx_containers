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
// IndexedDynamicArray<T, sizeType>
//
// A slot-map: elements have stable indices that never change after insertion,
// even as other elements are erased. A BitMap tracks which slots are live
// and a free-list (mFreeslots) recycles vacated slots for O(1) insert/erase.
// ─────────────────────────────────────────────────────────────────────────────

#include"dynamicArray.hpp"
#include"bitMap.hpp"
#include"core/index.hpp"

#include<shared_mutex>
#include<mutex>

namespace idx {

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	template<typename T, typename sizeType>
	class IndexedDynamicArrayBase {

	private:

		using IDX = Index<T, sizeType>;

	protected:

		basic::DynamicArray<T, sizeType> mData;           // Dense storage (includes free slots)
		basic::DynamicArray<sizeType, sizeType> mFreeslots; // Stack of indices available for reuse
		basic::BitMap<sizeType, sizeType> mBitMap;    // Occupancy bitmap: bit[i] == 1 means mData[i] is live

	protected:

		// Removes and destroys the last element in mData, then clears its bitmap bit.
		// Used internally when the last slot is the one being erased (avoids a free-slot entry).
		void popBackInternal()
		{
			sizeType lastIndex = this->mData.size() - 1;
			this->mData.popBack();
			this->mBitMap.toggleOff(lastIndex);
		}

		// Inserts `data` into the next available slot and marks it as live in the bitmap.
		// If mFreeslots is non-empty, reuses a previously freed slot (O(1)).
		// Otherwise appends to the end of mData (amortised O(1)).
		// `data` – the value to copy-assign into the slot.
		// Returns an Index wrapping the slot that was used.
		IDX insertInternal(const T& data)
		{
			sizeType index;
			if (this->mFreeslots.size() == 0) {
				index = this->mData.size();
				this->mData.pushBack(data);
			}
			else {
				index = this->mFreeslots.back();
				this->mFreeslots.popBack();
				this->mData[index] = data;
			}
			this->mBitMap.toggleOn(index);
			return IDX(index);
		}

		// Marks slot `index` as free and adds it to mFreeslots for future reuse.
		// If `index` is the very last slot, pops the back instead (keeps mData compact).
		// Asserts in debug builds if `index` is already free (double-erase detection).
		// `index` – the raw slot index to free. Must be currently live (asserted).
		void eraseInternal(const sizeType index)
		{
			idxASSERT(this->mBitMap[index] == true, "Data at index is already erased");
			if (index == this->mData.size() - 1) {
				this->mData.popBack();
			}
			else {
				this->mData[index] = T();
				this->mFreeslots.pushBack(index);
			}
			this->mBitMap.toggleOff(index);
		}

		// Bounds-checked access by raw slot index.
		// `index` – the slot must be live (asserted via the bitmap).
		T& refInternal(const sizeType& index)
		{
			idxASSERT(this->mBitMap[index] == true, "index has no data");
			return this->mData[index];
		}

		// Bounds-checked access by raw slot index.
		// `index` – the slot must be live (asserted via the bitmap).
		const T& refInternal(const sizeType& index) const
		{
			idxASSERT(this->mBitMap[index] == true, "index has no data");
			return this->mData[index];
		}

		IndexedDynamicArrayBase() {}
		~IndexedDynamicArrayBase() {}

	public:

		// REQUIRED by ITERATOR
		// Returns the next live slot index after `index`, skipping free slots.
		// `index` – the current raw slot position.
		// Returns the next occupied slot index, or INVALID_IDX(sizeType) at the end.
		sizeType nextIndex(const sizeType& index)
		{
			sizeType idx = index + 1;
			const sizeType s = this->mData.size();
			while (idx < s) {
				if (this->mBitMap[idx] == true) {
					return idx;
				}
				++idx;
			}
			return INVALID_IDX(sizeType);
		}

		// REQUIRED by ITERATOR
		// Returns the previous live slot index before `index`, skipping free slots.
		// `index` – the current raw slot position.
		sizeType prevIndex(const sizeType& index)
		{
			sizeType idx = index - 1;
			const sizeType s = this->mData.size();
			while (idx < s) {
				if (this->mBitMap[idx] == true) {
					return idx;
				}
				--idx;
			}
			return INVALID_IDX(sizeType);
		}

		// REQUIRED by ITERATOR
		// Returns a reference to the element at `index` — required by Iterator.
		// `index` – raw slot index. Must be live.
		T& ref(const sizeType& index) { return this->refInternal(index); }

		// REQUIRED by ITERATOR
		// Const overload of ref().
		const T& ref(const sizeType& index) const { return this->refInternal(index); }
	};

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	namespace basic {

		template<typename T, typename sizeType>
		class IndexedDynamicArray : public IndexedDynamicArrayBase<T, sizeType> {

		public:

			using value_type = T;
			using size_type = sizeType;
			using iterator = Iterator<IndexedDynamicArray<T, sizeType>>;
			using const_iterator = const iterator;
			using IDX = Index<T, sizeType>;

		public:

			// Default constructor. Empty array, no allocation.
			IndexedDynamicArray() : IndexedDynamicArrayBase<T, sizeType>() {}

			// Copy constructor. Deep-copies mData, mFreeslots, and mBitMap from `other`.
			IndexedDynamicArray(const IndexedDynamicArray<T, sizeType>& other) : IndexedDynamicArrayBase<T, sizeType>()
			{
				this->mData = other.mData;
				this->mFreeslots = other.mFreeslots;
				this->mBitMap = other.mBitMap;
			}

			// Move constructor. Steals mData, mFreeslots, and mBitMap from `other`.
			IndexedDynamicArray(IndexedDynamicArray<T, sizeType>&& other) noexcept : IndexedDynamicArrayBase<T, sizeType>()
			{
				this->mData = std::move(other.mData);
				this->mFreeslots = std::move(other.mFreeslots);
				this->mBitMap = std::move(other.mBitMap);
			}

			// Copy assignment. Replaces all three member arrays with copies from `other`.
			// Returns *this.
			IndexedDynamicArray<T, sizeType>& operator=(const IndexedDynamicArray<T, sizeType>& other)
			{
				if (this == &other) return *this;
				this->mData = other.mData;
				this->mFreeslots = other.mFreeslots;
				this->mBitMap = other.mBitMap;
				return *this;
			}

			// Move assignment. Steals all three member arrays from `other`.
			// Returns *this.
			IndexedDynamicArray<T, sizeType>& operator=(IndexedDynamicArray<T, sizeType>&& other) noexcept
			{
				if (this == &other) return *this;
				this->mData = std::move(other.mData);
				this->mFreeslots = std::move(other.mFreeslots);
				this->mBitMap = std::move(other.mBitMap);
				return *this;
			}

			// Swaps all three member arrays between this and `other` in O(1).
			// `other` – the array to swap with. No-op if this == &other.
			void swap(IndexedDynamicArray<T, sizeType>& other)
			{
				if (this == &other) return;
				this->mData.swap(other.mData);
				this->mFreeslots.swap(other.mFreeslots);
				this->mBitMap.swap(other.mBitMap);
			}

			// Inserts `data` into the next available slot (reusing a free slot if possible).
			// `data` – the value to store.
			// Returns a stable Index that remains valid until the slot is explicitly erased.
			IDX insert(const T& data)
			{
				return this->insertInternal(data);
			}

			// Erases ALL slots whose stored value equals `data`.
			// WARNING: erases every matching occurrence, not just the first.
			// `data` – the value to search for and erase.
			void erase(const T& data)
			{
				for (sizeType x = 0, len = this->mData.size(); x < len; ++x) {
					if (this->mBitMap[x] == true) {
						if (this->mData[x] == data) {
							this->eraseInternal(x);
							return;
						}
					}
				}
			}

			// Erases the element at the slot indicated by `index`.
			// `index` – the stable index returned by a previous insert(). Must be live (asserted).
			void erase(const IDX index)
			{
				this->eraseInternal(index.ref());
			}

			// Removes and destroys the last element in the backing array.
			// Use sparingly — only safe when you know the last slot is live.
			void popBack()
			{
				this->popBackInternal();
			}

			// Linear O(n) search through live slots only.
			// `data` – the value to look for.
			// Returns a valid Index on the first match, or an invalid (default) Index if not found.
			IDX find(const T& data)
			{
				for (sizeType x = 0, len = this->mData.size(); x < len; ++x) {
					if (this->mBitMap[x] == true) {
						if (this->mData[x] == data) {
							return IDX(x);
						}
					}
				}
				return IDX();
			}

			// Resizes the backing array to `newSize` slots.
			// This affects the raw storage only; use insert() to create live elements.
			// `newSize` – the desired raw slot count.
			void resize(const sizeType& newSize)
			{
				this->mData.resize(newSize);
			}

			// Bounds-checked access by typed Index.
			// `index` – must be valid (asserted) and its slot must be live (asserted).
			// Returns a mutable reference to the element at `index`.
			T& operator[](const IDX& index)
			{
				idxASSERT(index.invalid() == false, "index is invalid");
				return this->refInternal(index.ref());
			}

			// Const overload of operator[](IDX).
			const T& operator[](const IDX& index) const
			{
				idxASSERT(index.invalid() == false, "index is invalid");
				return this->refInternal(index.ref());
			}

			// Bounds-checked access by raw slot index.
			// `index` – the slot must be live (asserted via the bitmap).
			// Returns a mutable reference to the element at `index`.
			T& operator[](const sizeType& index)
			{
				idxASSERT(this->mBitMap[index] == true, "index has no data");
				return this->refInternal(index);
			}

			// Const overload of operator[](sizeType).
			const T& operator[](const sizeType& index) const
			{
				idxASSERT(this->mBitMap[index] == true, "index has no data");
				return this->refInternal(index);
			}

			// Returns a reference to the last element in the raw backing array.
			// NOTE: the last raw slot may be free — check isIndexOccupied() if unsure.
			T& back()
			{
				return this->mData.back();
			}

			// Const overload of back().
			const T& back() const
			{
				return this->mData.back();
			}

			// Returns true when there are no live elements (size() == 0).
			bool empty() const
			{
				return this->size() == 0;
			}

			// Trims both the backing array and the free-slots array to their current sizes.
			void wrap()
			{
				this->mData.wrap();
				this->mFreeslots.wrap();
			}

			// Returns the number of live elements (raw slots minus free slots).
			sizeType size() const
			{
				return this->mData.size() - this->mFreeslots.size();
			}

			// Returns the total number of raw slots (live + free).
			// Useful for iteration over raw slot indices.
			sizeType internalSize() const
			{
				return this->mData.size();
			}

			// Returns true if the slot at `index` currently holds a live element.
			// `index` – a typed Index to check.
			bool isIndexOccupied(const IDX& index) const
			{
				return this->mBitMap[index.ref()];
			}

			// Returns true if the raw slot at `index` currently holds a live element.
			// `index` – a raw slot index.
			bool isIndexOccupied(const sizeType& index) const
			{
				return this->mBitMap[index];
			}

			// Resets the bitmap and resets both data and free-slots arrays to size 0.
			// Memory is kept allocated (no deallocation).
			// `callDestructors` – if true, ~T() is called on each live element first.
			void shallowClear(bool callDestructors)
			{
				this->mBitMap.reset();
				this->mData.shallowClear(callDestructors);
				this->mFreeslots.shallowClear(callDestructors);
			}

			// Destroys all elements, frees all memory, and resets the bitmap.
			void deepClear()
			{
				this->mData.deepClear();
				this->mFreeslots.deepClear();
				this->mBitMap.reset();
			}

			#define type IndexedDynamicArray<T, sizeType>
			BASIC_ITERATOR_FUNCTIONS(type)
			#undef type
		};
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	namespace threadSafe {

		template<typename T, typename sizeType>
		class IndexedDynamicArray : public IndexedDynamicArrayBase<T, sizeType> {

		public:

			using value_type = T;
			using size_type = sizeType;
			using iterator = Iterator<IndexedDynamicArray<T, sizeType>>;
			using const_iterator = const iterator;
			using IDX = Index<T, sizeType>;

		private:

			mutable std::shared_mutex mMutex;

		public:

			// Default constructor.
			IndexedDynamicArray() : IndexedDynamicArrayBase<T, sizeType>() {}

			// Copy constructor. Acquires a shared lock on `other`.
			IndexedDynamicArray(const IndexedDynamicArray<T, sizeType>& other) : IndexedDynamicArrayBase<T, sizeType>()
			{
				std::shared_lock lock(const_cast<IndexedDynamicArray<T, sizeType>&>(other).mMutex);
				this->mData = other.mData;
				this->mFreeslots = other.mFreeslots;
				this->mBitMap = other.mBitMap;
			}

			// Move constructor. Acquires a unique lock on `other`.
			IndexedDynamicArray(IndexedDynamicArray<T, sizeType>&& other) noexcept : IndexedDynamicArrayBase<T, sizeType>()
			{
				std::unique_lock lock(other.mMutex);
				this->mData = std::move(other.mData);
				this->mFreeslots = std::move(other.mFreeslots);
				this->mBitMap = std::move(other.mBitMap);
			}

			// Copy assignment. Locks both atomically. Returns *this.
			IndexedDynamicArray<T, sizeType>& operator=(const IndexedDynamicArray<T, sizeType>& other)
			{
				if (this == &other) return *this;
				std::scoped_lock lock(this->mMutex, const_cast<IndexedDynamicArray<T, sizeType>&>(other).mMutex);
				this->mData = other.mData;
				this->mFreeslots = other.mFreeslots;
				this->mBitMap = other.mBitMap;
				return *this;
			}

			// Move assignment. Locks both atomically, steals arrays. Returns *this.
			IndexedDynamicArray<T, sizeType>& operator=(IndexedDynamicArray<T, sizeType>&& other) noexcept
			{
				if (this == &other) return *this;
				std::scoped_lock lock(this->mMutex, other.mMutex);
				this->mData = std::move(other.mData);
				this->mFreeslots = std::move(other.mFreeslots);
				this->mBitMap = std::move(other.mBitMap);
				return *this;
			}

			// Thread-safe swap. Locks both atomically.
			void swap(IndexedDynamicArray<T, sizeType>& other)
			{
				if (this == &other) return;
				std::scoped_lock lock(this->mMutex, other.mMutex);
				this->mData.swap(other.mData);
				this->mFreeslots.swap(other.mFreeslots);
				this->mBitMap.swap(other.mBitMap);
			}

			// Thread-safe insert. Acquires a unique lock.
			// `data` – value to store. Returns a stable Index.
			IDX insert(const T& data)
			{
				std::unique_lock lock(this->mMutex);
				return this->insertInternal(data);
			}

			// Thread-safe erase by value. Acquires a unique lock.
			// Erases ALL live slots whose value equals `data`.
			void erase(const T& data)
			{
				std::unique_lock lock(this->mMutex);
				for (sizeType x = 0, len = this->mData.size(); x < len; ++x) {
					if (this->mBitMap[x] == true) {
						if (this->mData[x] == data) {
							this->eraseInternal(x);
							return;
						}
					}
				}
			}

			// Thread-safe erase by Index. Acquires a unique lock.
			// `index` – the stable index to erase. Must be live (asserted).
			void erase(const IDX index)
			{
				std::unique_lock lock(this->mMutex);
				this->eraseInternal(index.ref());
			}

			// Thread-safe popBack. Acquires a unique lock.
			void popBack()
			{
				std::unique_lock lock(this->mMutex);
				this->popBackInternal();
			}

			// Thread-safe find. Acquires a shared lock.
			// `data` – value to search for. Returns valid Index or invalid Index.
			IDX find(const T& data)
			{
				std::shared_lock lock(this->mMutex);
				for (sizeType x = 0, len = this->mData.size(); x < len; ++x) {
					if (this->mBitMap[x] == true) {
						if (this->mData[x] == data) {
							return IDX(x);
						}
					}
				}
				return IDX();
			}

			// Thread-safe resize. Acquires a unique lock.
			// `newSize` – desired raw slot count.
			void resize(const sizeType& newSize)
			{
				std::unique_lock lock(this->mMutex);
				this->mData.resize(newSize);
			}

			// Thread-safe operator[](IDX). Acquires a shared lock.
			// `index` – must be valid and live (asserted).
			T& operator[](const IDX& index)
			{
				std::shared_lock lock(this->mMutex);
				idxASSERT(index.invalid() == false, "index is invalid");
				return this->refInternal(index.ref());
			}

			// Const thread-safe operator[](IDX).
			const T& operator[](const IDX& index) const
			{
				std::shared_lock lock(this->mMutex);
				idxASSERT(index.invalid() == false, "index is invalid");
				return this->refInternal(index.ref());
			}

			// Thread-safe operator[](sizeType). Acquires a shared lock.
			// `index` – raw slot. Must be live (asserted).
			T& operator[](const sizeType& index)
			{
				std::shared_lock lock(this->mMutex);
				idxASSERT(this->mBitMap[index] == true, "index has no data");
				return this->refInternal(index);
			}

			// Const thread-safe operator[](sizeType).
			const T& operator[](const sizeType& index) const
			{
				std::shared_lock lock(this->mMutex);
				idxASSERT(this->mBitMap[index] == true, "index has no data");
				return this->refInternal(index);
			}

			// Thread-safe back(). Acquires a shared lock.
			T& back()
			{
				std::shared_lock lock(this->mMutex);
				return this->mData.back();
			}

			// Const thread-safe back().
			const T& back() const
			{
				std::shared_lock lock(this->mMutex);
				return this->mData.back();
			}

			// Thread-safe empty(). Acquires a shared lock.
			bool empty() const
			{
				std::shared_lock lock(this->mMutex);
				return this->size() == 0;
			}

			// Thread-safe wrap(). Acquires a unique lock.
			void wrap()
			{
				std::unique_lock lock(this->mMutex);
				this->mData.wrap();
				this->mFreeslots.wrap();
			}

			// Thread-safe size(). Acquires a shared lock. Returns live element count.
			sizeType size() const
			{
				std::shared_lock lock(this->mMutex);
				return this->mData.size() - this->mFreeslots.size();
			}

			// Thread-safe internalSize(). Acquires a shared lock. Returns raw slot count.
			sizeType internalSize() const
			{
				std::shared_lock lock(this->mMutex);
				return this->mData.size();
			}

			// Thread-safe isIndexOccupied(IDX). Acquires a shared lock.
			bool isIndexOccupied(const IDX& index) const
			{
				std::shared_lock lock(this->mMutex);
				return this->mBitMap[index.ref()];
			}

			// Thread-safe isIndexOccupied(sizeType). Acquires a shared lock.
			bool isIndexOccupied(const sizeType& index) const
			{
				std::shared_lock lock(this->mMutex);
				return this->mBitMap[index];
			}

			// Thread-safe shallowClear(). Acquires a unique lock.
			// `callDestructors` – if true, ~T() is called on live elements.
			void shallowClear(bool callDestructors)
			{
				std::unique_lock lock(this->mMutex);
				this->mBitMap.reset();
				this->mData.shallowClear(callDestructors);
				this->mFreeslots.shallowClear(callDestructors);
			}

			// Thread-safe deepClear(). Acquires a unique lock.
			// Destroys all elements and frees all memory.
			void deepClear()
			{
				std::unique_lock lock(this->mMutex);
				this->mData.deepClear();
				this->mFreeslots.deepClear();
				this->mBitMap.reset();
			}

			#define type IndexedDynamicArray<T, sizeType>
			THREAD_SAFE_ITERATOR_FUNCTIONS(type)
			#undef type
		};
	}
}
