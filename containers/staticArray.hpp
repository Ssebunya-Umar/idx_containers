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

#include"iterator.hpp"
#include"core/types.hpp"
#include"core/assert.hpp"

#include<cstring>
#include<type_traits>
#include<shared_mutex>
#include<mutex>

namespace idx {

	// ─────────────────────────────────────────────────────────────────────────────
	// StaticArrayBase<T, capacity, sizeType>
	// Inline fixed-capacity array with no heap allocation.
	// ─────────────────────────────────────────────────────────────────────────────
	template<typename T, uint64 capacity, typename sizeType>
	class StaticArrayBase {

	protected:

		T mData[capacity] = {};
		sizeType mSize = 0;

	protected:

		// Appends a copy of `data` at mSize and increments mSize.
		// `data` – the value to copy-assign into the new slot.
		// Asserts if the array is already at capacity.
		void pushBackInternal(const T& data)
		{
			idxASSERT(this->mSize < capacity, "array is already full");
			this->mData[this->mSize] = data;
			++this->mSize;
		}

		// Appends `size` elements from the raw C array `data`.
		// Uses memcpy for trivially copyable T; falls back to element-wise assignment.
		// `data` – pointer to the source elements.
		// `size` – number of elements to append. Asserts if they won't fit.
		void pushBackRangeInternal(const T* data, const sizeType& size)
		{
			idxASSERT(this->mSize + size <= capacity, "data can not fit into the array!!");
			if constexpr (std::is_trivially_copyable_v<T>) {
				std::memcpy(&this->mData[this->mSize], data, size * sizeof(T));
			}
			else {
				for (sizeType x = 0; x < size; ++x) {
					this->mData[this->mSize + x] = data[x];
				}
			}
			this->mSize += size;
		}

		// Removes the element at `index` using the swap-with-last trick (O(1), unordered).
		// The vacated last slot is reset to T().
		// `index` – the slot to erase. Must satisfy 0 <= index < mSize (asserted).
		void eraseInternal(const sizeType& index)
		{
			idxASSERT(index >= 0 && index < this->mSize, "index is out of range");
			--this->mSize;
			if (index < this->mSize) {
				this->mData[index] = this->mData[this->mSize];
			}
			this->mData[this->mSize] = T();
		}

		// Removes the last element and resets its slot to T().
		// Asserts if the array is empty.
		void popBackInternal()
		{
			idxASSERT(this->mSize >= 1, "array is empty");
			--this->mSize;
			this->mData[this->mSize] = T();
		}

		// Sets the logical size directly to `size`.
		// Growing:   value-initialises slots [mSize, size).
		// Shrinking: calls ~T() on slots [size, mSize).
		// `size` – the target number of live elements. Must be <= capacity (asserted).
		void setSizeInternal(const sizeType& size)
		{
			idxASSERT(size <= capacity, "size exceeds capacity");
			if (size > this->mSize) {
				for (sizeType x = this->mSize; x < size; ++x) {
					this->mData[x] = T();
				}
			}
			else if (size < this->mSize) {
				for (sizeType x = size; x < this->mSize; ++x) {
					this->mData[x].~T();
				}
			}
			this->mSize = size;
		}

		// Copies all capacity slots from `other` (not just live elements) and mirrors mSize.
		// `other` – the array to copy from. No-op if this == &other.
		void cloneInternal(const StaticArrayBase<T, capacity, sizeType>& other)
		{
			if (this == &other) return;
			for (sizeType x = 0; x < capacity; ++x) {
				this->mData[x] = other.mData[x];
			}
			this->mSize = other.mSize;
		}

		// Move-assigns all capacity slots from `other`, then resets other's mSize to 0.
		// `other` – the array to steal from. No-op if this == &other.
		void hijackInternal(StaticArrayBase<T, capacity, sizeType>& other)
		{
			if (this == &other) return;
			for (sizeType x = 0; x < capacity; ++x) {
				this->mData[x] = std::move(other.mData[x]);
			}
			this->mSize = other.mSize;
			other.mSize = 0;
		}

		// Bounds-checked element access.
		// `index` – zero-based index. Must be < size() (asserted).
		T& refInternal(const sizeType& index)
		{
			idxASSERT((this->mSize > 0) && (index < this->mSize), "array is empty or index is out of range");
			return this->mData[index];
		}

		// Bounds-checked element access.
		// `index` – zero-based index. Must be < size() (asserted).
		const T& refInternal(const sizeType& index) const
		{
			idxASSERT((this->mSize > 0) && (index < this->mSize), "array is empty or index is out of range");
			return this->mData[index];
		}

		StaticArrayBase() {}
		~StaticArrayBase() {}
		
	public:

		// REQUIRED by ITERATOR
		// Returns the next valid index after `index`, or INVALID_IDX if `index` was the last.
		// Used internally by Iterator to advance through the array.
		// `index` – the current position.
		// Returns the next slot index, or INVALID_IDX(sizeType) at the end.
		sizeType nextIndex(const sizeType& index)
		{
			sizeType idx = index + 1;
			return idx < this->mSize ? idx : INVALID_IDX(sizeType);
		}

		// REQUIRED by ITERATOR
		// Returns the valid index before `index`, or INVALID_IDX if `index` was 0.
		// Used internally by Iterator to advance through the array.
		// `index` – the current position.
		// Returns the previous slot before index, or INVALID_IDX(sizeType) if index was 0.
		sizeType prevIndex(const sizeType& index)
		{
			sizeType idx = index - 1;
			return idx == INVALID_IDX(sizeType) ? INVALID_IDX(sizeType) : idx;
		}

		// Returns a reference to the element at `index` — required by Iterator<DynamicArray>.
		// `index` – zero-based index. Must be < size().
		T& ref(const sizeType& index) { return this->refInternal(index); }

		// REQUIRED by ITERATOR
		// Const overload of ref().
		const T& ref(const sizeType& index) const { return this->refInternal(index); }
	};

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	namespace basic {

		// ─────────────────────────────────────────────────────────────────────────
		// basic::StaticArray<T, capacity, sizeType>
		// ─────────────────────────────────────────────────────────────────────────
		template<typename T, uint64 capacity, typename sizeType>
		class StaticArray : public StaticArrayBase<T, capacity, sizeType> {

		public:

			using value_type = T;
			using size_type = sizeType;
			using iterator = Iterator<StaticArray<T, capacity, sizeType>>;
			using const_iterator = const iterator;

		public:

			// Default constructor. All slots are value-initialised; mSize = 0.
			StaticArray() : StaticArrayBase<T, capacity, sizeType>() {}

			// Fill constructor. Assigns `data` to every slot and sets mSize = capacity.
			// `data` – the value to assign to all capacity slots.
			explicit StaticArray(const T& data) : StaticArrayBase<T, capacity, sizeType>()
			{
				for (sizeType x = 0; x < capacity; ++x) {
					this->mData[x] = data;
				}
				this->mSize = capacity;
			}

			// Range constructor. Copies `count` elements from the raw array `data`.
			// `data`  – pointer to source elements.
			// `count` – number of elements to copy. Must be <= capacity (asserted).
			explicit StaticArray(const T* data, const sizeType& count) : StaticArrayBase<T, capacity, sizeType>()
			{
				idxASSERT(count <= capacity, "count should be less than or equal to the capacity!!");
				for (sizeType x = 0; x < count; ++x) {
					this->pushBackInternal(data[x]);
				}
			}

			// Copy constructor. Copies all capacity slots and mSize from `other`.
			StaticArray(const StaticArray<T, capacity, sizeType>& other) : StaticArrayBase<T, capacity, sizeType>()
			{
				this->cloneInternal(other);
			}

			// Move constructor. Move-assigns all slots from `other`; other's mSize becomes 0.
			StaticArray(StaticArray<T, capacity, sizeType>&& other) noexcept : StaticArrayBase<T, capacity, sizeType>()
			{
				this->hijackInternal(other);
			}

			// Copy assignment. Copies all capacity slots and mSize from `other`.
			// Returns *this.
			StaticArray<T, capacity, sizeType>& operator=(const StaticArray<T, capacity, sizeType>& other)
			{
				if (this == &other) return *this;
				this->cloneInternal(other);
				return *this;
			}

			// Move assignment. Move-assigns all slots from `other`; other's mSize becomes 0.
			// Returns *this.
			StaticArray<T, capacity, sizeType>& operator=(StaticArray<T, capacity, sizeType>&& other) noexcept
			{
				if (this == &other) return *this;
				this->hijackInternal(other);
				return *this;
			}

			// Appends a copy of `data` at the back. Asserts if the array is full.
			// `data` – the value to copy into the new back slot.
			void pushBack(const T& data)
			{
				this->pushBackInternal(data);
			}

			// Appends `size` elements from raw array `data`. Asserts if they won't fit.
			// `data` – source elements. `size` – number to append.
			void pushBack(const T* data, const sizeType& size)
			{
				this->pushBackRangeInternal(data, size);
			}

			// Removes the element at `index` using swap-with-last. O(1), unordered.
			// `index` – the slot to erase. Must be < mSize (asserted).
			void erase(const sizeType& index)
			{
				this->eraseInternal(index);
			}

			// Removes and resets the last element. Asserts if the array is empty.
			void popBack()
			{
				this->popBackInternal();
			}

			// Sets the logical size directly, constructing or destroying elements as needed.
			// `size` – the target number of live elements. Must be <= capacity (asserted).
			void setSize(const sizeType& size)
			{
				this->setSizeInternal(size);
			}

			// Linear O(n) search through live elements [0, mSize).
			// `data`   – the value to search for.
			// Returns a pointer to the first matching element, or nullptr if not found.
			T* find(const T& data)
			{
				for (sizeType x = 0; x < this->mSize; ++x) {
					if (this->mData[x] == data) {
						return &this->mData[x];
					}
				}
				return nullptr;
			}

			// Const overload of find().
			const T* find(const T& data) const
			{
				for (sizeType x = 0; x < this->mSize; ++x) {
					if (this->mData[x] == data) {
						return &this->mData[x];
					}
				}
				return nullptr;
			}

			// Bounds-checked element access. Checks against `capacity`, not mSize,
			// so slots beyond the logical end can be read (they hold T() after erase/clear).
			// `index` – zero-based slot. Must be < capacity (asserted).
			// Returns a mutable reference to the slot at `index`.
			T& operator[](const sizeType& index)
			{
				idxASSERT(index >= 0 && index < capacity, "index is out of range");
				return this->mData[index];
			}

			// Const overload of operator[]. Checks against capacity.
			const T& operator[](const sizeType& index) const
			{
				idxASSERT(index >= 0 && index < capacity, "index is out of range");
				return this->mData[index];
			}

			// Returns a mutable reference to mData[0]. Asserts if mSize == 0.
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

			// Returns a mutable reference to mData[mSize - 1]. Asserts if mSize == 0.
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

			// Returns the number of live elements (NOT capacity).
			sizeType size() const { return this->mSize; }

			// Returns a raw pointer to the first slot (useful for C APIs).
			T* data()             { return this->mData; }
			const T* data() const { return this->mData; }

			// Returns true when mSize == 0.
			bool empty() const { return this->mSize == 0; }

			// Resets mSize to 0 WITHOUT calling destructors or zeroing memory.
			// Safe only for trivially destructible types.
			void shallowClear()
			{
				this->mSize = 0;
			}

			// Resets every slot to T() and sets mSize = 0. Calls T() via assignment.
			void deepClear()
			{
				for (sizeType x = 0; x < capacity; ++x) {
					this->mData[x] = T();
				}
				this->mSize = 0;
			}

			// Returns the next valid index after `index`, or INVALID_IDX at the end.
			// Used internally by Iterator.
			// `index` – the current position.
			// Returns index + 1 if it is < mSize, otherwise INVALID_IDX(sizeType).
			sizeType nextIndex(const sizeType& index)
			{
				sizeType idx = index + 1;
				return idx < this->mSize ? idx : INVALID_IDX(sizeType);
			}

			// Returns a reference to the element at `index` — required by Iterator.
			// `index` – must be < capacity (checked by operator[]).
			T& ref(const sizeType& index) { return this->operator[](index); }
			const T& ref(const sizeType& index) const { return this->operator[](index); }

			#define type StaticArray<T, capacity, sizeType>
			BASIC_ITERATOR_FUNCTIONS(type)
			#undef type
		};
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	namespace threadSafe {

		// ─────────────────────────────────────────────────────────────────────────
		// threadSafe::StaticArray<T, capacity, sizeType>
		// Same as basic::StaticArray but every public method holds mMutex.
		// ─────────────────────────────────────────────────────────────────────────
		template<typename T, uint64 capacity, typename sizeType>
		class StaticArray : public StaticArrayBase<T, capacity, sizeType> {

		public:

			using value_type = T;
			using size_type = sizeType;
			using iterator = Iterator<StaticArray<T, capacity, sizeType>>;
			using const_iterator = const iterator;

		private:

			mutable std::shared_mutex mMutex;

		public:

			// Default constructor. All slots value-initialised; mSize = 0.
			StaticArray() : StaticArrayBase<T, capacity, sizeType>() {}

			// Fill constructor. Assigns `data` to all slots; mSize = capacity.
			// `data` – the value to fill all capacity slots with.
			explicit StaticArray(const T& data) : StaticArrayBase<T, capacity, sizeType>()
			{
				for (sizeType x = 0; x < capacity; ++x) {
					this->mData[x] = data;
				}
				this->mSize = capacity;
			}

			// Range constructor. Copies `count` elements from `data`.
			// `data`  – source elements. `count` – must be <= capacity (asserted).
			explicit StaticArray(const T* data, const sizeType& count) : StaticArrayBase<T, capacity, sizeType>()
			{
				idxASSERT(count <= capacity, "count should be less than or equal to the capacity!!");
				for (sizeType x = 0; x < count; ++x) {
					this->pushBackInternal(data[x]);
				}
			}

			// Copy constructor. Acquires a shared lock on `other` while copying.
			StaticArray(const StaticArray<T, capacity, sizeType>& other) : StaticArrayBase<T, capacity, sizeType>()
			{
				std::shared_lock otherLock(other.mMutex);
				this->cloneInternal(other);
			}

			// Move constructor. Acquires a unique lock on `other` while stealing slots.
			StaticArray(StaticArray<T, capacity, sizeType>&& other) noexcept : StaticArrayBase<T, capacity, sizeType>()
			{
				std::unique_lock otherLock(other.mMutex);
				this->hijackInternal(other);
			}

			// Copy assignment. Locks both atomically. Returns *this.
			StaticArray<T, capacity, sizeType>& operator=(const StaticArray<T, capacity, sizeType>& other)
			{
				if (this == &other) return *this;
				std::scoped_lock lock(this->mMutex, other.mMutex);
				this->cloneInternal(other);
				return *this;
			}

			// Move assignment. Locks both atomically, steals slots. Returns *this.
			StaticArray<T, capacity, sizeType>& operator=(StaticArray<T, capacity, sizeType>&& other) noexcept
			{
				if (this == &other) return *this;
				std::scoped_lock lock(this->mMutex, other.mMutex);
				this->hijackInternal(other);
				return *this;
			}

			// Thread-safe pushBack. Acquires a unique lock.
			// `data` – value to append. Asserts if full.
			void pushBack(const T& data)
			{
				std::unique_lock lock(this->mMutex);
				this->pushBackInternal(data);
			}

			// Thread-safe bulk pushBack. Acquires a unique lock.
			// `data` – source elements. `size` – count to append.
			void pushBack(const T* data, const sizeType& size)
			{
				std::unique_lock lock(this->mMutex);
				this->pushBackRangeInternal(data, size);
			}

			// Thread-safe erase. Acquires a unique lock.
			// `index` – slot to remove via swap-with-last. Must be < mSize.
			void erase(const sizeType& index)
			{
				std::unique_lock lock(this->mMutex);
				this->eraseInternal(index);
			}

			// Thread-safe popBack. Acquires a unique lock.
			void popBack()
			{
				std::unique_lock lock(this->mMutex);
				this->popBackInternal();
			}

			// Thread-safe setSize. Acquires a unique lock.
			// `size` – target live element count. Must be <= capacity.
			void setSize(const sizeType& size)
			{
				std::unique_lock lock(this->mMutex);
				this->setSizeInternal(size);
			}

			// Thread-safe find. Acquires a shared lock.
			// `data` – value to search for. Returns pointer to first match or nullptr.
			T* find(const T& data)
			{
				std::shared_lock lock(this->mMutex);
				for (sizeType x = 0; x < this->mSize; ++x) {
					if (this->mData[x] == data) {
						return &this->mData[x];
					}
				}
				return nullptr;
			}

			// Const thread-safe find. Acquires a shared lock.
			const T* find(const T& data) const
			{
				std::shared_lock lock(this->mMutex);
				for (sizeType x = 0; x < this->mSize; ++x) {
					if (this->mData[x] == data) {
						return &this->mData[x];
					}
				}
				return nullptr;
			}

			// Thread-safe operator[]. Acquires a shared lock. Checks against capacity.
			// `index` – zero-based slot. Must be < capacity (asserted).
			T& operator[](const sizeType& index)
			{
				std::shared_lock lock(this->mMutex);
				idxASSERT(index >= 0 && index < capacity, "index is out of range");
				return this->mData[index];
			}

			// Const thread-safe operator[].
			const T& operator[](const sizeType& index) const
			{
				std::shared_lock lock(this->mMutex);
				idxASSERT(index >= 0 && index < capacity, "index is out of range");
				return this->mData[index];
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

			// Thread-safe empty(). Acquires a shared lock. Returns true if mSize == 0.
			bool empty() const
			{
				std::shared_lock lock(this->mMutex);
				return this->mSize == 0;
			}

			// Thread-safe shallowClear(). Acquires a unique lock.
			// Resets mSize to 0 without calling destructors.
			void shallowClear()
			{
				std::unique_lock lock(this->mMutex);
				this->mSize = 0;
			}

			// Thread-safe deepClear(). Acquires a unique lock.
			// Resets all slots to T() and sets mSize = 0.
			void deepClear()
			{
				std::unique_lock lock(this->mMutex);
				for (sizeType x = 0; x < capacity; ++x) {
					this->mData[x] = T();
				}
				this->mSize = 0;
			}

			// Returns next valid slot after `index`, or INVALID_IDX at the end.
			sizeType nextIndex(const sizeType& index)
			{
				sizeType idx = index + 1;
				return idx < this->mSize ? idx : INVALID_IDX(sizeType);
			}

			T& ref(const sizeType& index) { return this->operator[](index); }
			const T& ref(const sizeType& index) const { return this->operator[](index); }

			#define type StaticArray<T, capacity, sizeType>
			THREAD_SAFE_ITERATOR_FUNCTIONS(type)
			#undef type
		};
	}
}
