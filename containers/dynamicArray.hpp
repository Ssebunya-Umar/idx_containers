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

#include"memoryBuffer.hpp"
#include"iterator.hpp"

#include<mutex>

namespace idx {

	#define MINIMUM_ARRAY_CAPACITY  2
	#define ARRAY_GROWTH_FACTOR  1.5f

	// ─────────────────────────────────────────────────────────────────────────────
	// DynamicArrayBase<T, sizeType>
	// Shared implementation for basic:: and threadSafe:: DynamicArray.
	// ─────────────────────────────────────────────────────────────────────────────
	template<typename T, typename sizeType>
	class DynamicArrayBase {

	protected:

		MemoryBuffer<T, sizeType> mBuffer;
		sizeType mSize = 0;

	protected:

		// Ensures there is room for at least one more element.
		// If the buffer has never been allocated, creates a buffer of MINIMUM_ARRAY_CAPACITY.
		// If the buffer is full (mSize == capacity), grows by ARRAY_GROWTH_FACTOR,
		// with a minimum growth of +1 to handle edge cases with tiny capacities.
		void checkMemoryInternal()
		{
			if (this->mBuffer.capacity() == 0) {
				this->mBuffer = MemoryBuffer<T, sizeType>(MINIMUM_ARRAY_CAPACITY);
			}
			else if (this->mSize == this->mBuffer.capacity()) {
				sizeType newCapacity = (sizeType)(double(this->mBuffer.capacity()) * ARRAY_GROWTH_FACTOR);
				if (newCapacity <= this->mBuffer.capacity()) {
					newCapacity = this->mBuffer.capacity() + 1;
				}
				this->reserveInternal(newCapacity);
			}
		}

		// Reallocates the buffer to hold exactly `capacity` slots.
		// Live objects beyond the new capacity are destroyed first.
		// All surviving objects are move-constructed into the new buffer.
		// The old buffer is then released.
		// `capacity` – the desired new capacity. No-op if equal to current capacity or <= 0.
		void reserveInternal(const sizeType& capacity)
		{
			if (capacity <= 0) return;
			if (capacity == this->mBuffer.capacity()) return;

			sizeType newSize = this->mSize > capacity ? capacity : this->mSize;

			MemoryBuffer<T, sizeType> temp(capacity);
			for (sizeType x = 0; x < newSize; ++x) {
				new (&temp[x]) T(std::move(this->mBuffer[x]));
			}
			for (sizeType x = 0; x < this->mSize; ++x) {
				this->mBuffer[x].~T();
			}
			this->mBuffer.swap(temp);
			this->mSize = newSize;
		}

		// Adjusts the logical size of the array to `size`.
		// Shrinking: calls ~T() on elements that fall off the end.
		// Growing  : default-constructs new elements via placement-new (allocates more
		//            storage first if needed).
		// `size` – the desired number of live elements. No-op if equal to mSize.
		void resizeInternal(const sizeType& size)
		{
			if (size == this->mSize) return;
			if (size < this->mSize) {
				for (sizeType x = size; x < this->mSize; ++x) {
					this->mBuffer[x].~T();
				}
				this->mSize = size;
			}
			else {
				if (size > this->mBuffer.capacity()) {
					this->reserveInternal(size);
				}
				for (sizeType x = this->mSize; x < size; ++x) {
					new (&this->mBuffer[x]) T();
				}
				this->mSize = size;
			}
		}

		// Appends a copy of `data` at mSize using placement-new, then increments mSize.
		// Calls checkMemoryInternal() first to guarantee there is a free slot.
		// `data` – the value to copy-construct into the new slot.
		void pushBackInternalCopy(const T& data)
		{
			this->checkMemoryInternal();
			new (&this->mBuffer[this->mSize]) T(data);
			++this->mSize;
		}

		// Appends `data` by move at mSize using placement-new, then increments mSize.
		// Calls checkMemoryInternal() first to guarantee there is a free slot.
		// `data` – the value to move-construct into the new slot (left in a valid-but-unspecified state).
		void pushBackInternalMove(T&& data)
		{
			this->checkMemoryInternal();
			new (&this->mBuffer[this->mSize]) T(std::move(data));
			++this->mSize;
		}

		// Removes the element at `index` using the swap-with-last trick (O(1)).
		// The last live element is move-constructed into `index`, then the vacated
		// last slot is destroyed. ORDER IS NOT PRESERVED.
		// `index` – the slot to erase. Must be < mSize (asserted).
		void eraseInternal(const sizeType index)
		{
			idxASSERT(this->mBuffer.data() != nullptr && index < this->mSize, "array is empty or index is out of range");
			this->mBuffer[index].~T();
			--this->mSize;
			if (index < this->mSize) {
				new (&this->mBuffer[index]) T(std::move(this->mBuffer[this->mSize]));
				this->mBuffer[this->mSize].~T();
			}
		}

		// Destroys the last live element and decrements mSize. O(1).
		// Asserts if the array is empty.
		void popBackInternal()
		{
			idxASSERT(this->mSize > 0, "array is empty");
			--this->mSize;
			this->mBuffer[this->mSize].~T();
		}

		// Resets mSize to 0, keeping the underlying buffer allocated for reuse.
		// `callDestructors` – if true, ~T() is called on every live element first.
		//                     Set to false only for trivially destructible types.
		void shallowClearInternal(bool callDestructors)
		{
			if (callDestructors) {
				for (sizeType x = 0; x < this->mSize; ++x) {
					this->mBuffer[x].~T();
				}
			}
			this->mSize = 0;
		}

		// Destroys all live elements and frees the underlying buffer.
		// After this call mSize == 0 and mBuffer.capacity() == 0.
		void deepClearInternal()
		{
			for (sizeType x = 0; x < this->mSize; ++x) {
				this->mBuffer[x].~T();
			}
			this->mBuffer.clear();
			this->mSize = 0;
		}

		// Deep-copies `other` into this array.
		// Destroys self first, allocates a fresh buffer matching other's capacity,
		// then copy-constructs each live element.
		// `other` – the array to copy from. No-op if this == &other.
		void cloneInternal(const DynamicArrayBase<T, sizeType>& other)
		{
			if (this == &other) return;
			this->deepClearInternal();
			if (other.mBuffer.capacity() > 0) {
				this->mBuffer = MemoryBuffer<T, sizeType>(other.mBuffer.capacity());
			}
			for (sizeType x = 0; x < other.mSize; ++x) {
				new (&this->mBuffer[x]) T(other.mBuffer[x]);
			}
			this->mSize = other.mSize;
		}

		// Steals `other`'s buffer and size (move semantics).
		// Destroys self first, then takes other's allocation. `other` is left empty.
		// `other` – the array to steal from. No-op if this == &other.
		void hijackInternal(DynamicArrayBase<T, sizeType>& other)
		{
			if (this == &other) return;
			this->deepClearInternal();
			this->mBuffer = std::move(other.mBuffer);
			this->mSize = other.mSize;
			other.mSize = 0;
		}

		// Bounds-checked element access.
		// `index` – zero-based index. Must be < size() (asserted).
		T& refInternal(const sizeType& index)
		{
			idxASSERT((this->mSize > 0) && (index < this->mSize), "array is empty or index is out of range");
			return this->mBuffer[index];
		}

		// Bounds-checked element access.
		// `index` – zero-based index. Must be < size() (asserted).
		const T& refInternal(const sizeType& index) const
		{
			idxASSERT((this->mSize > 0) && (index < this->mSize), "array is empty or index is out of range");
			return this->mBuffer[index];
		}

		DynamicArrayBase() {}

		~DynamicArrayBase()
		{
			this->deepClearInternal();
		}

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

		// REQUIRED by ITERATOR
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
		// basic::DynamicArray<T, sizeType>
		// A heap-resizing array without thread-safety overhead.
		// ─────────────────────────────────────────────────────────────────────────
		template<typename T, typename sizeType>
		class DynamicArray : public DynamicArrayBase<T, sizeType> {

		public:

			using value_type = T;
			using size_type = sizeType;
			using iterator = Iterator<DynamicArray<T, sizeType>>;
			using const_iterator = const iterator;

		public:

			// Default constructor. Creates an empty array with no allocation.
			DynamicArray() : DynamicArrayBase<T, sizeType>() {}

			// Capacity constructor. Pre-allocates `capacity` slots without constructing elements.
			// `capacity` – number of slots to reserve upfront. Clamped to MINIMUM_ARRAY_CAPACITY.
			explicit DynamicArray(const sizeType& capacity) : DynamicArrayBase<T, sizeType>()
			{
				this->mBuffer = MemoryBuffer<T, sizeType>(capacity < MINIMUM_ARRAY_CAPACITY ? MINIMUM_ARRAY_CAPACITY : capacity);
			}

			// Raw-array constructor. Copy-constructs `size` elements from `data`.
			// `data` – pointer to the source elements to copy.
			// `size` – number of elements to copy from `data`.
			explicit DynamicArray(const T* data, const sizeType& size) : DynamicArrayBase<T, sizeType>()
			{
				this->mBuffer = MemoryBuffer<T, sizeType>(size < MINIMUM_ARRAY_CAPACITY ? MINIMUM_ARRAY_CAPACITY : size);
				for (sizeType x = 0; x < size; ++x) {
					this->pushBackInternalCopy(data[x]);
				}
			}

			// Copy constructor. Deep-copies all live elements from `other`.
			DynamicArray(const DynamicArray<T, sizeType>& other) : DynamicArrayBase<T, sizeType>()
			{
				this->cloneInternal(other);
			}

			// Move constructor. Steals `other`'s buffer; `other` is left empty.
			DynamicArray(DynamicArray<T, sizeType>&& other) noexcept : DynamicArrayBase<T, sizeType>()
			{
				this->hijackInternal(other);
			}

			// Copy assignment. Destroys self, then deep-copies from `other`.
			// Returns *this.
			DynamicArray<T, sizeType>& operator=(const DynamicArray<T, sizeType>& other)
			{
				if (this == &other) return *this;
				this->cloneInternal(other);
				return *this;
			}

			// Move assignment. Destroys self, then steals `other`'s buffer.
			// Returns *this.
			DynamicArray<T, sizeType>& operator=(DynamicArray<T, sizeType>&& other) noexcept
			{
				if (this == &other) return *this;
				this->hijackInternal(other);
				return *this;
			}

			// Swaps the contents of this array and `other` in O(1) — no element copying.
			// `other` – the array to swap with. No-op if this == &other.
			void swap(DynamicArray<T, sizeType>& other)
			{
				if (this == &other) return;
				this->mBuffer.swap(other.mBuffer);
				std::swap(this->mSize, other.mSize);
			}

			// Appends a copy of `data` at the back. Amortised O(1).
			// Grows the buffer if needed.
			// `data` – the value to copy-construct into the new back slot.
			void pushBack(const T& data)
			{
				this->pushBackInternalCopy(data);
			}

			// Appends `data` by move at the back. Amortised O(1).
			// Grows the buffer if needed.
			// `data` – the value to move-construct into the new back slot (left unspecified after move).
			void pushBack(T&& data)
			{
				this->pushBackInternalMove(std::move(data));
			}

			// Appends `size` elements from a raw C array.
			// `data` – pointer to source elements to copy.
			// `size` – number of elements to copy.
			void pushBack(const T* data, const sizeType& size)
			{
				for (sizeType x = 0; x < size; ++x) {
					this->pushBackInternalCopy(data[x]);
				}
			}

			// Appends all live elements of another DynamicArray.
			// Self-append is handled safely: snapshots mSize before iterating
			// to avoid infinite growth.
			// `arr` – the array whose elements are appended.
			void pushBack(const DynamicArray<T, sizeType>& arr)
			{
				if (this == &arr) {
					sizeType count = this->mSize;
					for (sizeType x = 0; x < count; ++x) {
						T copy = this->mBuffer[x];
						this->pushBackInternalCopy(copy);
					}
					return;
				}
				sizeType count = arr.mSize;
				for (sizeType x = 0; x < count; ++x) {
					this->pushBackInternalCopy(arr.mBuffer[x]);
				}
			}

			// Removes the element at `index` using the swap-with-last trick. O(1), unordered.
			// The last element fills the gap; the erased slot is destroyed.
			// `index` – the slot to erase. Must be < size() (asserted).
			void erase(const sizeType index)
			{
				this->eraseInternal(index);
			}

			// Removes and destroys the last element. O(1).
			// Asserts if the array is empty.
			void popBack()
			{
				this->popBackInternal();
			}

			// Linear O(n) search through live elements.
			// `data`   – the value to look for (compared with operator==).
			// Returns a pointer to the first matching element, or nullptr if not found.
			T* find(const T& data)
			{
				return this->mBuffer.find(data);
			}

			// Sets the logical size to `size`, constructing or destroying elements as needed.
			// Grows the buffer if `size` exceeds current capacity.
			// `size` – the target number of live elements.
			void resize(const sizeType& size)
			{
				this->resizeInternal(size);
			}

			// Ensures the buffer holds at least `capacity` slots without changing mSize.
			// No-op if capacity <= current capacity.
			// `capacity` – the minimum number of slots to allocate.
			void reserve(const sizeType& capacity)
			{
				this->reserveInternal(capacity);
			}

			// Returns a raw pointer to the first element (useful for C APIs or memcpy).
			// Returns nullptr if the array is empty.
			T* data()
			{
				return this->mBuffer.data();
			}

			// Const overload of data().
			const T* data() const
			{
				return this->mBuffer.data();
			}

			// Bounds-checked element access.
			// `index` – zero-based index. Must be < size() (asserted).
			// Returns a mutable reference to the element at `index`.
			T& operator[](const sizeType& index)
			{
				return this->refInternal(index);
			}

			// Const overload of operator[].
			// `index` – zero-based index. Must be < size() (asserted).
			const T& operator[](const sizeType& index) const
			{
				return this->refInternal(index);
			}

			// Returns a mutable reference to the first element.
			// Asserts if the array is empty.
			T& front()
			{
				idxASSERT(this->mSize >= 1, "array is empty");
				return this->mBuffer[0];
			}

			// Const overload of front().
			const T& front() const
			{
				idxASSERT(this->mSize >= 1, "array is empty");
				return this->mBuffer[0];
			}

			// Returns a mutable reference to the last element.
			// Asserts if the array is empty.
			T& back()
			{
				idxASSERT(this->mSize >= 1, "array is empty");
				return this->mBuffer[this->mSize - 1];
			}

			// Const overload of back().
			const T& back() const
			{
				idxASSERT(this->mSize >= 1, "array is empty");
				return this->mBuffer[this->mSize - 1];
			}

			// Returns the number of live (constructed) elements.
			sizeType size() const
			{
				return this->mSize;
			}

			// Returns the number of slots in the underlying buffer (>= size()).
			sizeType capacity() const
			{
				return this->mBuffer.capacity();
			}

			// Returns true when there are no live elements (size() == 0).
			bool empty() const
			{
				return this->mSize == 0;
			}

			// Shrinks the buffer to exactly mSize, freeing unused capacity.
			// If the array is empty, frees the buffer entirely.
			void wrap()
			{
				if (this->mSize == 0) {
					this->deepClearInternal();
				}
				else {
					this->reserveInternal(this->mSize);
				}
			}

			// Resets size to 0, keeping the underlying buffer allocated.
			// `callDestructors` – pass true to invoke ~T() on each live element before reset.
			//                     Pass false only for trivially destructible types.
			void shallowClear(bool callDestructors)
			{
				this->shallowClearInternal(callDestructors);
			}

			// Destroys all live elements and frees all memory. Size and capacity both become 0.
			void deepClear()
			{
				this->deepClearInternal();
			}

			#define type DynamicArray<T, sizeType>
			BASIC_ITERATOR_FUNCTIONS(type)
			#undef type
		};
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	namespace threadSafe {

		// ─────────────────────────────────────────────────────────────────────────
		// threadSafe::DynamicArray<T, sizeType>
		// Same as basic::DynamicArray but every public method holds mMutex.
		// Read-only methods acquire a shared (read) lock; mutating methods acquire
		// a unique (write) lock.
		// ─────────────────────────────────────────────────────────────────────────
		template<typename T, typename sizeType>
		class DynamicArray : public DynamicArrayBase<T, sizeType> {

		public:

			using value_type = T;
			using size_type = sizeType;
			using iterator = Iterator<DynamicArray<T, sizeType>>;
			using const_iterator = const iterator;

		private:

			mutable std::shared_mutex mMutex;

		public:

			// Default constructor. Creates an empty array with no allocation.
			DynamicArray() : DynamicArrayBase<T, sizeType>() {}

			// Capacity constructor. Pre-allocates `capacity` slots.
			// `capacity` – number of slots to reserve. Clamped to MINIMUM_ARRAY_CAPACITY.
			explicit DynamicArray(const sizeType& capacity) : DynamicArrayBase<T, sizeType>()
			{
				this->mBuffer = MemoryBuffer<T, sizeType>(capacity < MINIMUM_ARRAY_CAPACITY ? MINIMUM_ARRAY_CAPACITY : capacity);
			}

			// Raw-array constructor. Copy-constructs `size` elements from `data`.
			// `data` – source elements. `size` – number of elements.
			explicit DynamicArray(const T* data, const sizeType& size) : DynamicArrayBase<T, sizeType>()
			{
				this->mBuffer = MemoryBuffer<T, sizeType>(size < MINIMUM_ARRAY_CAPACITY ? MINIMUM_ARRAY_CAPACITY : size);
				for (sizeType x = 0; x < size; ++x) {
					this->pushBackInternalCopy(data[x]);
				}
			}

			// Copy constructor. Acquires a shared lock on `other` while copying.
			DynamicArray(const DynamicArray<T, sizeType>& other) : DynamicArrayBase<T, sizeType>()
			{
				std::shared_lock otherLock(other.mMutex);
				this->cloneInternal(other);
			}

			// Move constructor. Acquires a unique lock on `other` while stealing its buffer.
			DynamicArray(DynamicArray<T, sizeType>&& other) noexcept : DynamicArrayBase<T, sizeType>()
			{
				std::unique_lock otherLock(other.mMutex);
				this->hijackInternal(other);
			}

			// Copy assignment. Locks both arrays atomically via scoped_lock to prevent deadlock.
			// Returns *this.
			DynamicArray<T, sizeType>& operator=(const DynamicArray<T, sizeType>& other)
			{
				if (this == &other) return *this;
				std::scoped_lock lock(this->mMutex, other.mMutex);
				this->cloneInternal(other);
				return *this;
			}

			// Move assignment. Locks both arrays atomically, then steals `other`'s buffer.
			// Returns *this.
			DynamicArray<T, sizeType>& operator=(DynamicArray<T, sizeType>&& other) noexcept
			{
				if (this == &other) return *this;
				std::scoped_lock lock(this->mMutex, other.mMutex);
				this->hijackInternal(other);
				return *this;
			}

			// Swaps contents with `other` under a scoped lock on both. O(1).
			// `other` – the array to swap with.
			void swap(DynamicArray<T, sizeType>& other)
			{
				if (this == &other) return;
				std::scoped_lock lock(this->mMutex, other.mMutex);
				this->mBuffer.swap(other.mBuffer);
				std::swap(this->mSize, other.mSize);
			}

			// Thread-safe pushBack (copy). Acquires a unique lock before appending.
			// `data` – value to copy-construct at the back.
			void pushBack(const T& data)
			{
				std::unique_lock lock(this->mMutex);
				this->pushBackInternalCopy(data);
			}

			// Thread-safe pushBack (move). Acquires a unique lock before appending.
			// `data` – value to move-construct at the back.
			void pushBack(T&& data)
			{
				std::unique_lock lock(this->mMutex);
				this->pushBackInternalMove(std::move(data));
			}

			// Thread-safe bulk pushBack from a raw array. Acquires a unique lock.
			// `data` – source elements. `size` – number to copy.
			void pushBack(const T* data, const sizeType& size)
			{
				std::unique_lock lock(this->mMutex);
				for (sizeType x = 0; x < size; ++x) {
					this->pushBackInternalCopy(data[x]);
				}
			}

			// Thread-safe pushBack from another DynamicArray.
			// Self-append locks only this; cross-append locks both via scoped_lock.
			// `arr` – the array whose elements are appended.
			void pushBack(const DynamicArray<T, sizeType>& arr)
			{
				if (this == &arr) {
					std::unique_lock lock(this->mMutex);
					sizeType count = this->mSize;
					for (sizeType x = 0; x < count; ++x) {
						T copy = this->mBuffer[x];
						this->pushBackInternalCopy(copy);
					}
					return;
				}
				std::scoped_lock lock(this->mMutex, arr.mMutex);
				sizeType count = arr.mSize;
				for (sizeType x = 0; x < count; ++x) {
					this->pushBackInternalCopy(arr.mBuffer[x]);
				}
			}

			// Thread-safe erase. Acquires a unique lock, then swap-with-last removes `index`.
			// `index` – the slot to erase. Must be < size() (asserted).
			void erase(const sizeType index)
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

			// Thread-safe linear search. Acquires a shared lock.
			// `data` – value to search for. Returns pointer to first match or nullptr.
			T* find(const T& data)
			{
				std::shared_lock lock(this->mMutex);
				return this->mBuffer.find(data);
			}

			// Thread-safe resize. Acquires a unique lock.
			// `size` – the target number of live elements.
			void resize(const sizeType& size)
			{
				std::unique_lock lock(this->mMutex);
				this->resizeInternal(size);
			}

			// Thread-safe reserve. Acquires a unique lock.
			// `capacity` – minimum number of slots to allocate.
			void reserve(const sizeType& capacity)
			{
				std::unique_lock lock(this->mMutex);
				this->reserveInternal(capacity);
			}

			// Thread-safe data(). Acquires a shared lock.
			// Returns raw pointer to first element (or nullptr if empty).
			T* data()
			{
				std::shared_lock lock(this->mMutex);
				return this->mBuffer.data();
			}

			// Const overload of data() with shared lock.
			const T* data() const
			{
				std::shared_lock lock(this->mMutex);
				return this->mBuffer.data();
			}

			// Thread-safe operator[]. Acquires a shared lock.
			// `index` – zero-based index. Must be < size() (asserted).
			T& operator[](const sizeType& index)
			{
				std::shared_lock lock(this->mMutex);
				return this->refInternal(index);
			}

			// Const overload of operator[] with shared lock.
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
				return this->mBuffer[0];
			}

			// Const overload of front() with shared lock.
			const T& front() const
			{
				std::shared_lock lock(this->mMutex);
				idxASSERT(this->mSize >= 1, "array is empty");
				return this->mBuffer[0];
			}

			// Thread-safe back(). Acquires a shared lock. Asserts if empty.
			T& back()
			{
				std::shared_lock lock(this->mMutex);
				idxASSERT(this->mSize >= 1, "array is empty");
				return this->mBuffer[this->mSize - 1];
			}

			// Const overload of back() with shared lock.
			const T& back() const
			{
				std::shared_lock lock(this->mMutex);
				idxASSERT(this->mSize >= 1, "array is empty");
				return this->mBuffer[this->mSize - 1];
			}

			// Thread-safe size(). Acquires a shared lock. Returns number of live elements.
			sizeType size() const
			{
				std::shared_lock lock(this->mMutex);
				return this->mSize;
			}

			// Thread-safe capacity(). Acquires a shared lock. Returns buffer slot count.
			sizeType capacity() const
			{
				std::shared_lock lock(this->mMutex);
				return this->mBuffer.capacity();
			}

			// Thread-safe empty(). Acquires a shared lock. Returns true if size() == 0.
			bool empty() const
			{
				std::shared_lock lock(this->mMutex);
				return this->mSize == 0;
			}

			// Thread-safe wrap(). Acquires a unique lock.
			// Trims capacity to mSize, freeing unused slots.
			void wrap()
			{
				std::unique_lock lock(this->mMutex);
				if (this->mSize == 0) {
					this->deepClearInternal();
				}
				else {
					this->reserveInternal(this->mSize);
				}
			}

			// Thread-safe shallowClear(). Acquires a unique lock.
			// `callDestructors` – true to invoke ~T() on each element before resetting size.
			void shallowClear(bool callDestructors)
			{
				std::unique_lock lock(this->mMutex);
				this->shallowClearInternal(callDestructors);
			}

			// Thread-safe deepClear(). Acquires a unique lock.
			// Destroys all elements and frees all memory.
			void deepClear()
			{
				std::unique_lock lock(this->mMutex);
				this->deepClearInternal();
			}

			#define type DynamicArray<T, sizeType>
			THREAD_SAFE_ITERATOR_FUNCTIONS(type)
			#undef type
		};
	}
}
