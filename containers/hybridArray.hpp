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
// HybridArray<T, stackCapacity, sizeType>
//
// A two-tier array: first stackCapacity elements live inline (no heap), all
// further elements overflow onto the heap via a DynamicArray.
// ─────────────────────────────────────────────────────────────────────────────

#include"staticArray.hpp"
#include"dynamicArray.hpp"

#include<shared_mutex>
#include<mutex>

namespace idx {

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	template<typename T, uint64 stackCapacity, typename sizeType>
	class HybridArrayBase {

	protected:

		basic::StaticArray<T, stackCapacity, sizeType> mStackData;
		basic::DynamicArray<T, sizeType> mHeapData;

	protected:

		// Returns a mutable reference to the element at the unified logical `index`.
		// Indices [0, stackCapacity) map to mStackData; indices >= stackCapacity map
		// to mHeapData at offset (index - stackCapacity).
		// `index` – the unified logical position. No bounds check is performed here.
		T& refInternal(const sizeType& index)
		{
			if (index < stackCapacity) {
				return this->mStackData[index];
			}
			else {
				return this->mHeapData[index - stackCapacity];
			}
		}

		// Const overload of refInternal().
		const T& refInternal(const sizeType& index) const
		{
			if (index < stackCapacity) {
				return this->mStackData[index];
			}
			else {
				return this->mHeapData[index - stackCapacity];
			}
		}

		HybridArrayBase() {}
		~HybridArrayBase() {}

	public:

		// REQUIRED by ITERATOR
		// Returns the next valid unified index after `index`, or INVALID_IDX at the end.
		// `index` – the current position.
		sizeType nextIndex(const sizeType& index)
		{
			sizeType idx = index + 1;
			return idx < (this->mStackData.size() + this->mHeapData.size()) ? idx : INVALID_IDX(sizeType);
		}

		// REQUIRED by ITERATOR
		// Returns the previous valid unified index before `index`, or INVALID_IDX at 0.
		// `index` – the current position.
		sizeType prevIndex(const sizeType& index)
		{
			sizeType idx = index - 1;
			return idx == INVALID_IDX(sizeType) ? INVALID_IDX(sizeType) : idx;
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

		template<typename T, uint64 stackCapacity, typename sizeType>
		class HybridArray : public HybridArrayBase<T, stackCapacity, sizeType> {

		public:

			using value_type = T;
			using size_type = sizeType;
			using iterator = Iterator<HybridArray<T, stackCapacity, sizeType>>;
			using const_iterator = const iterator;

		public:

			// Default constructor. Both tiers are empty; no heap allocation.
			HybridArray() : HybridArrayBase<T, stackCapacity, sizeType>() {}

			// Range constructor. Appends `size` elements from the raw C array `data`.
			// `data` – pointer to source elements.
			// `size` – number of elements to copy; first stackCapacity go to the stack
			//          tier, the rest to the heap tier.
			explicit HybridArray(const T* data, const sizeType& size) : HybridArrayBase<T, stackCapacity, sizeType>()
			{
				for (sizeType x = 0; x < size; ++x) {
					this->pushBack(data[x]);
				}
			}

			// Cross-type HybridArray copy constructor. Copies all elements from `other`
			// regardless of `other`'s stackCapacity or sizeType.
			template<sizeType otherStackCapacity, typename otherSizeType>
			HybridArray(const HybridArray<T, otherStackCapacity, otherSizeType>& other) : HybridArrayBase<T, stackCapacity, sizeType>()
			{
				for (sizeType x = 0, len = other.size(); x < len; ++x) {
					this->pushBack(other[x]);
				}
			}

			// Constructs from a basic::StaticArray of potentially different capacity.
			// `arr` – the StaticArray whose live elements are copied in order.
			template<sizeType otherStackCapacity, typename otherSizeType>
			explicit HybridArray(const StaticArray<T, otherStackCapacity, otherSizeType>& arr) : HybridArrayBase<T, stackCapacity, sizeType>()
			{
				for (sizeType x = 0, len = arr.size(); x < len; ++x) {
					this->pushBack(arr[x]);
				}
			}

			// Constructs from a basic::DynamicArray. Copies all live elements in order.
			// `arr` – the DynamicArray to copy from.
			explicit HybridArray(const DynamicArray<T, sizeType>& arr) : HybridArrayBase<T, stackCapacity, sizeType>()
			{
				for (sizeType x = 0, len = arr.size(); x < len; ++x) {
					this->pushBack(arr[x]);
				}
			}

			// Cross-type copy assignment. Clears self, then copies all elements from `other`.
			// No-op if this and &other are the same object.
			// Returns *this.
			template<sizeType otherStackCapacity, typename otherSizeType>
			HybridArray<T, stackCapacity, sizeType>& operator=(const HybridArray<T, otherStackCapacity, otherSizeType>& other)
			{
				if ((void*)this != (void*)&other) {
					this->deepClear();
					for (sizeType x = 0, len = other.size(); x < len; ++x) {
						this->pushBack(other[x]);
					}
				}
				return *this;
			}

			// Converts the hybrid array to a heap-only DynamicArray, copying all elements.
			// Returns a new DynamicArray<T, sizeType> containing all live elements in order.
			basic::DynamicArray<T, sizeType> toDynamicArray() const
			{
				DynamicArray<T, sizeType> arr;
				arr.resize(this->size());
				for (sizeType x = 0, len = this->size(); x < len; ++x) {
					arr[x] = this->operator[](x);
				}
				return arr;
			}

			// Appends a copy of `data`. Elements fill the stack tier first; once the
			// stack tier is full (mStackData.size() == stackCapacity), subsequent elements
			// go to the heap tier.
			// `data` – the value to append.
			void pushBack(const T& data)
			{
				if (this->mStackData.size() < stackCapacity) {
					this->mStackData.pushBack(data);
				}
				else {
					this->mHeapData.pushBack(data);
				}
			}

			// Removes the element at unified logical `index`.
			// If `index` is in the stack tier: erases from mStackData (swap-with-last),
			// then moves mHeapData.back() into the stack to keep the tier full.
			// If `index` is in the heap tier: erases from mHeapData (swap-with-last).
			// `index` – the logical position to erase. Must be < size().
			void erase(const sizeType index)
			{
				if (index < stackCapacity) {
					this->mStackData.erase(index);
					if (this->mHeapData.empty() == false) {
						this->mStackData.pushBack(this->mHeapData.back());
						this->mHeapData.popBack();
					}
				}
				else {
					this->mHeapData.erase(index - stackCapacity);
				}
			}

			// Linear O(n) search through both tiers.
			// Searches the stack tier first; if not found, searches the heap tier.
			// `data` – the value to look for.
			// Returns a pointer to the first match, or nullptr if not found.
			T* find(const T& data)
			{
				T* pointer = this->mStackData.find(data);
				if (pointer == nullptr) {
					return this->mHeapData.find(data);
				}
				return pointer;
			}

			// Unified indexed access. Dispatches to the correct tier via refInternal().
			// `index` – the unified logical position. Must be < size().
			T& operator[](const sizeType& index)
			{
				return this->refInternal(index);
			}

			// Const overload of operator[].
			const T& operator[](const sizeType& index) const
			{
				return this->refInternal(index);
			}

			// Returns a reference to the first element (always in the stack tier).
			// Asserts if the array is empty.
			T& front()
			{
				return this->mStackData.front();
			}

			// Const overload of front().
			const T& front() const
			{
				return this->mStackData.front();
			}

			// Returns a reference to the last element.
			// If the heap tier is non-empty, returns mHeapData.back();
			// otherwise returns mStackData.back().
			T& back()
			{
				if (this->mHeapData.size() > 0) {
					return this->mHeapData.back();
				}
				return this->mStackData.back();
			}

			// Const overload of back().
			const T& back() const
			{
				if (this->mHeapData.size() > 0) {
					return this->mHeapData.back();
				}
				return this->mStackData.back();
			}

			// Returns the total number of live elements across both tiers.
			sizeType size() const
			{
				return this->mStackData.size() + this->mHeapData.size();
			}

			// Sets the total logical size, distributing elements across both tiers.
			// If `size` <= stackCapacity: only the stack tier is resized; heap tier is untouched.
			// If `size` >  stackCapacity: the stack tier is filled to stackCapacity,
			//                              and the heap tier is resized to (size - stackCapacity).
			// `size` – the desired total element count.
			void setSize(const sizeType& size)
			{
				if (size <= stackCapacity) {
					this->mStackData.setSize(size);
				}
				else {
					this->mStackData.setSize(stackCapacity);
					this->mHeapData.resize(size - stackCapacity);
				}
			}

			// Returns true when the stack tier is empty (i.e. the whole array is empty,
			// because elements always fill the stack tier first).
			bool empty() const
			{
				return this->mStackData.empty();
			}

			// Trims the heap tier's capacity to its current size. No-op on the stack tier.
			void wrap()
			{
				this->mHeapData.wrap();
			}

			// Resets size to 0. Calls deepClear on the stack tier (resets to T()),
			// then shallowClear(callDestructors) on the heap tier (keeps heap memory).
			// `callDestructors` – passed to the heap tier's shallowClear.
			void shallowClear(bool callDestructors)
			{
				this->mStackData.deepClear();
				this->mHeapData.shallowClear(callDestructors);
			}

			// Destroys all elements in both tiers and frees heap memory.
			void deepClear()
			{
				this->mStackData.deepClear();
				this->mHeapData.deepClear();
			}

			#define type HybridArray<T, stackCapacity, sizeType>
			BASIC_ITERATOR_FUNCTIONS(type)
			#undef type
		};
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	namespace threadSafe {

		template<typename T, uint64 stackCapacity, typename sizeType>
		class HybridArray : public HybridArrayBase<T, stackCapacity, sizeType> {

		public:

			using value_type = T;
			using size_type = sizeType;
			using iterator = Iterator<HybridArray<T, stackCapacity, sizeType>>;
			using const_iterator = const iterator;

		private:

			mutable std::shared_mutex mMutex;

		public:

			// Default constructor.
			HybridArray() : HybridArrayBase<T, stackCapacity, sizeType>() {}

			// Range constructor. Acquires a shared lock on this (redundant but consistent).
			// `data` – source elements. `size` – number to copy.
			explicit HybridArray(const T* data, const sizeType& size)
			{
				std::shared_lock lock(this->mMutex);
				for (sizeType x = 0; x < size; ++x) {
					this->pushBack(data[x]);
				}
			}

			// Cross-type copy constructor. Locks both this and other atomically.
			template<sizeType otherStackCapacity, typename otherSizeType>
			HybridArray(const HybridArray<T, otherStackCapacity, otherSizeType>& other)
			{
				std::scoped_lock lock(this->mMutex, const_cast<HybridArray<T, otherStackCapacity, otherSizeType>&>(other).mMutex);
				for (sizeType x = 0, len = other.size(); x < len; ++x) {
					this->pushBack(other[x]);
				}
			}

			// Cross-type copy assignment. Locks both atomically, clears self, copies other.
			// Returns *this.
			template<sizeType otherStackCapacity, typename otherSizeType>
			HybridArray<T, stackCapacity, sizeType>& operator=(const HybridArray<T, otherStackCapacity, otherSizeType>& other)
			{
				std::scoped_lock lock(this->mMutex, const_cast<HybridArray<T, otherStackCapacity, otherSizeType>&>(other).mMutex);
				if ((void*)this != (void*)&other) {
					this->deepClear();
					for (sizeType x = 0, len = other.size(); x < len; ++x) {
						this->pushBack(other[x]);
					}
				}
				return *this;
			}

			// Constructs from a StaticArray. Acquires a shared lock on this.
			template<sizeType otherStackCapacity, typename otherSizeType>
			explicit HybridArray(const StaticArray<T, otherStackCapacity, otherSizeType>& arr)
			{
				std::shared_lock lock(this->mMutex);
				for (sizeType x = 0, len = arr.size(); x < len; ++x) {
					this->pushBack(arr[x]);
				}
			}

			// Constructs from a DynamicArray. Acquires a shared lock on this.
			explicit HybridArray(const DynamicArray<T, sizeType>& arr)
			{
				std::shared_lock lock(this->mMutex);
				for (sizeType x = 0, len = arr.size(); x < len; ++x) {
					this->pushBack(arr[x]);
				}
			}

			// Thread-safe toDynamicArray(). Acquires a shared lock.
			// Returns a heap-only DynamicArray with all live elements.
			DynamicArray<T, sizeType> toDynamicArray() const
			{
				std::shared_lock lock(this->mMutex);
				DynamicArray<T, sizeType> arr;
				arr.resize(this->size());
				for (sizeType x = 0, len = this->size(); x < len; ++x) {
					arr[x] = this->operator[](x);
				}
				return arr;
			}

			// Thread-safe pushBack. Acquires a unique lock.
			// `data` – value to append (stack tier first, heap tier on overflow).
			void pushBack(const T& data)
			{
				std::unique_lock lock(this->mMutex);
				if (this->mStackData.size() < stackCapacity) {
					this->mStackData.pushBack(data);
				}
				else {
					this->mHeapData.pushBack(data);
				}
			}

			// Thread-safe erase. Acquires a unique lock.
			// `index` – unified logical position to erase.
			void erase(const sizeType index)
			{
				std::unique_lock lock(this->mMutex);
				if (index < stackCapacity) {
					this->mStackData.erase(index);
					if (this->mHeapData.empty() == false) {
						this->mStackData.pushBack(this->mHeapData.back());
						this->mHeapData.popBack();
					}
				}
				else {
					this->mHeapData.erase(index - stackCapacity);
				}
			}

			// Thread-safe find. Acquires a shared lock.
			// `data` – value to search for. Returns pointer to first match or nullptr.
			T* find(const T& data)
			{
				std::shared_lock lock(this->mMutex);
				T* pointer = this->mStackData.find(data);
				if (pointer == nullptr) {
					return this->mHeapData.find(data);
				}
				return pointer;
			}

			// Thread-safe operator[]. Acquires a shared lock.
			// `index` – unified logical position.
			T& operator[](const sizeType& index)
			{
				std::shared_lock lock(this->mMutex);
				return refInternal(index);
			}

			// Const thread-safe operator[].
			const T& operator[](const sizeType& index) const
			{
				std::shared_lock lock(this->mMutex);
				return refInternal(index);
			}

			// Thread-safe front(). Acquires a shared lock.
			T& front()
			{
				std::shared_lock lock(this->mMutex);
				return this->mStackData.front();
			}

			// Const thread-safe front().
			const T& front() const
			{
				std::shared_lock lock(this->mMutex);
				return this->mStackData.front();
			}

			// Thread-safe back(). Acquires a shared lock.
			T& back()
			{
				std::shared_lock lock(this->mMutex);
				if (this->mHeapData.size() > 0) {
					return this->mHeapData.back();
				}
				return this->mStackData.back();
			}

			// Const thread-safe back().
			const T& back() const
			{
				std::shared_lock lock(this->mMutex);
				if (this->mHeapData.size() > 0) {
					return this->mHeapData.back();
				}
				return this->mStackData.back();
			}

			// Thread-safe size(). Acquires a shared lock. Returns combined element count.
			sizeType size() const
			{
				std::shared_lock lock(this->mMutex);
				return this->mStackData.size() + this->mHeapData.size();
			}

			// Thread-safe setSize(). Acquires a unique lock.
			// `size` – desired total element count.
			void setSize(const sizeType& size)
			{
				std::unique_lock lock(this->mMutex);
				if (size <= stackCapacity) {
					this->mStackData.setSize(size);
				}
				else {
					this->mStackData.setSize(stackCapacity);
					this->mHeapData.resize(size - stackCapacity);
				}
			}

			// Thread-safe empty(). Acquires a shared lock.
			bool empty() const
			{
				std::shared_lock lock(this->mMutex);
				return this->mStackData.empty();
			}

			// Thread-safe wrap(). Acquires a unique lock. Trims heap tier capacity.
			void wrap()
			{
				std::unique_lock lock(this->mMutex);
				this->mHeapData.wrap();
			}

			// Thread-safe shallowClear(). Acquires a unique lock.
			// `callDestructors` – passed to heap tier's shallowClear.
			void shallowClear(bool callDestructors)
			{
				std::unique_lock lock(this->mMutex);
				this->mStackData.deepClear();
				this->mHeapData.shallowClear(callDestructors);
			}

			// Thread-safe deepClear(). Acquires a unique lock.
			// Destroys all elements in both tiers and frees heap memory.
			void deepClear()
			{
				std::unique_lock lock(this->mMutex);
				this->mStackData.deepClear();
				this->mHeapData.deepClear();
			}

			#define type HybridArray<T, stackCapacity, sizeType>
			THREAD_SAFE_ITERATOR_FUNCTIONS(type)
			#undef type
		};
	}
}
