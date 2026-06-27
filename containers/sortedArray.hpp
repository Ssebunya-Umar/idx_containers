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
// SortedArray<T, sizeType>  (basic:: and threadSafe::)
//
// A doubly-linked list maintained in sorted order inside an IndexedDynamicArray.
// Provides O(1) access to the minimum and maximum elements.
// ─────────────────────────────────────────────────────────────────────────────

#include"indexedDynamicArray.hpp"

#include<shared_mutex>
#include<mutex>

namespace idx {

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	template<typename T, typename sizeType>
	class SortedArrayBase {

	private:

		using IDX = Index<T, sizeType>;

	protected:

		struct Node {
			T data = T();
			sizeType prev = INVALID_IDX(sizeType); // Index of the predecessor (smaller value)
			sizeType next = INVALID_IDX(sizeType); // Index of the successor  (larger value)

			Node() {}
			Node(const T& d) : data(d) {}
		};

	protected:

		basic::IndexedDynamicArray<Node, sizeType> mData;
		sizeType mMinimum = INVALID_IDX(sizeType); // Slot index of the smallest element
		sizeType mMaximum = INVALID_IDX(sizeType); // Slot index of the largest element

	protected:

		// Inserts `data` into the sorted linked-list.
		// Uses a two-pointer convergence: currentMin walks forward from the minimum,
		// currentMax walks backward from the maximum, until the correct position is found.
		// If `data` already exists, returns the index of the existing node (no duplicate).
		// `data` – the value to insert.
		// Returns an Index for the new (or existing) node.
		IDX insertInternal(const T& data)
		{
			if (this->mMinimum == INVALID_IDX(sizeType)) {
				// First element: initialise both min and max to this node.
				this->mMinimum = this->mMaximum = this->mData.insert(Node(data)).ref();
				return IDX(this->mMinimum);
			}
			else {
				sizeType currentMin = this->mMinimum;
				sizeType currentMax = this->mMaximum;

				while (true) {
					if (this->mData[currentMin].data > data) {
						// New node belongs before currentMin.
						auto index = this->mData.insert(Node(data));
						if (this->mData[currentMin].prev == INVALID_IDX(sizeType)) {
							this->mMinimum = index.ref();
						}
						else {
							this->mData[index.ref()].prev = this->mData[currentMin].prev;
							this->mData[this->mData[currentMin].prev].next = index.ref();
						}
						this->mData[currentMin].prev = index.ref();
						this->mData[index.ref()].next = currentMin;
						return IDX(index.ref());
					}
					else if (this->mData[currentMax].data < data) {
						// New node belongs after currentMax.
						auto index = this->mData.insert(Node(data));
						if (this->mData[currentMax].next == INVALID_IDX(sizeType)) {
							this->mMaximum = index.ref();
						}
						else {
							this->mData[index.ref()].next = this->mData[currentMax].next;
							this->mData[this->mData[currentMax].next].prev = index.ref();
						}
						this->mData[currentMax].next = index.ref();
						this->mData[index.ref()].prev = currentMax;
						return IDX(index.ref());
					}
					else if (this->mData[currentMin].data == data) {
						return IDX(currentMin); // Duplicate: return existing.
					}
					else if (this->mData[currentMax].data == data) {
						return IDX(currentMax); // Duplicate: return existing.
					}
					currentMin = this->mData[currentMin].next;
					currentMax = this->mData[currentMax].prev;
				}
			}
			return IDX();
		}

		// Searches for the first node whose data equals `data` and erases it.
		// Uses the same two-pointer convergence as insertInternal.
		// `data` – the value to search for and remove.
		void eraseInternal(const T& data)
		{
			if (this->mMinimum == INVALID_IDX(sizeType)) return;
			sizeType currentMin = this->mMinimum;
			sizeType currentMax = this->mMaximum;
			while (true) {
				if (this->mData[currentMin].data == data) { this->eraseInternal(IDX(currentMin)); break; }
				if (currentMin == currentMax) break;
				if (this->mData[currentMax].data == data) { this->eraseInternal(IDX(currentMax)); break; }
				if (this->mData[currentMin].next == currentMax) break;
				currentMin = this->mData[currentMin].next;
				currentMax = this->mData[currentMax].prev;
			}
		}

		// Removes the node at `index` from the sorted list and from the backing store.
		// Updates mMinimum / mMaximum if the removed node was at an end.
		// `index` – the typed Index of the node to remove. Must be valid and live.
		void eraseInternal(const IDX& index)
		{
			sizeType idx  = index.ref();
			sizeType prev = this->mData[idx].prev;
			sizeType next = this->mData[idx].next;

			if (prev == INVALID_IDX(sizeType)) {
				this->mMinimum = next;
			} else {
				this->mData[prev].next = next;
			}

			if (next == INVALID_IDX(sizeType)) {
				this->mMaximum = prev;
			} else {
				this->mData[next].prev = prev;
			}

			this->mData.erase(IDX(idx));
		}

		// Searches for the first node whose data equals `data`.
		// Uses the two-pointer convergence from both ends of the sorted list.
		// `data` – the value to look for.
		// Returns a valid IDX on the first match, or an invalid IDX if not found.
		IDX findInternal(const T& data)
		{
			if (this->mMinimum == INVALID_IDX(sizeType)) return IDX();
			sizeType currentMin = this->mMinimum;
			sizeType currentMax = this->mMaximum;
			while (true) {
				if (this->mData[currentMin].data == data) return IDX(currentMin);
				if (currentMin == currentMax) break;
				if (this->mData[currentMax].data == data) return IDX(currentMax);
				if (this->mData[currentMin].next == currentMax) break;
				currentMin = this->mData[currentMin].next;
				currentMax = this->mData[currentMax].prev;
			}
			return IDX();
		}

		// Access by raw slot index.
		// `index` – raw slot in the backing IndexedDynamicArray. Must be live.
		T& refInternal(const sizeType& index)
		{
			return this->mData[index].data;
		}

		// Access by raw slot index.
		// `index` – raw slot in the backing IndexedDynamicArray. Must be live.
		const T& refInternal(const sizeType& index) const
		{
			return this->mData[index].data;
		}

		SortedArrayBase() {}
		~SortedArrayBase() {}

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
		T& ref(const sizeType& index) { return this->refInternal(index); }

		// REQUIRED by ITERATOR
		// Const overload of ref().
		const T& ref(const sizeType& index) const { return this->refInternal(index); }
	};

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	namespace basic {

		template<typename T, typename sizeType>
		class SortedArray : public SortedArrayBase<T, sizeType> {

		public:

			using value_type = T;
			using size_type = sizeType;
			using iterator = Iterator<SortedArray<T, sizeType>>;
			using const_iterator = const iterator;
			using IDX = Index<T, sizeType>;

		public:

			// Inserts `data` in sorted position.
			// If `data` already exists, returns the existing node's index (no duplicate added).
			// `data` – the value to insert.
			// Returns the Index of the inserted (or already-existing) node.
			IDX insert(const T& data)
			{
				return insertInternal(data);
			}

			// Removes the first element equal to `data`. No-op if not found.
			// `data` – the value to search for and remove.
			void erase(const T& data)
			{
				eraseInternal(data);
			}

			// Removes the element identified by `index`.
			// `index` – a typed Index previously returned by insert() or find(). Must be live.
			void erase(const IDX& index)
			{
				this->eraseInternal(index);
			}

			// Searches for `data` in the sorted list.
			// `data` – the value to look for.
			// Returns a valid IDX on the first match, or an invalid IDX if not found.
			IDX find(const T& data)
			{
				return findInternal(data);
			}

			// Returns a mutable reference to the smallest element.
			// Behaviour is undefined if the array is empty.
			T& minimum()
			{
				return this->mData[this->mMinimum].data;
			}

			// Const overload of minimum().
			const T& minimum() const
			{
				return this->mData[this->mMinimum].data;
			}

			// Returns a mutable reference to the largest element.
			// Behaviour is undefined if the array is empty.
			T& maximum()
			{
				return this->mData[this->mMaximum].data;
			}

			// Const overload of maximum().
			const T& maximum() const
			{
				return this->mData[this->mMaximum].data;
			}

			// Access by typed IDX.
			// `index` – a typed Index from insert() or find(). Must be live.
			// Returns a mutable reference to the element's data.
			T& operator[](const IDX& index)
			{
				return this->refInternal(index.ref());
			}

			// Const overload of operator[](IDX).
			const T& operator[](const IDX& index) const
			{
				return this->refInternal(index.ref());
			}

			// Access by raw slot index in the backing IndexedDynamicArray.
			// `index` – raw slot. Must be a live slot in the backing store.
			T& operator[](const sizeType& index)
			{
				return this->refInternal(index);
			}

			// Const overload of operator[](sizeType).
			const T& operator[](const sizeType& index) const
			{
				return this->refInternal(index);
			}

			// Returns the number of live elements.
			sizeType size() const
			{
				return this->mData.size();
			}

			// Returns true when there are no elements (mMinimum is INVALID_IDX).
			bool empty() const
			{
				return this->mMinimum == INVALID_IDX(sizeType);
			}

			// Trims the backing IndexedDynamicArray's capacity to size().
			void wrap()
			{
				this->mData.wrap();
			}

			// Resets to empty, keeping backing memory allocated.
			// Resets mMinimum and mMaximum to INVALID_IDX.
			// `callDestructors` – if true, ~T() is called on each node's data first.
			void shallowClear(bool callDestructors)
			{
				this->mData.shallowClear(callDestructors);
				this->mMinimum = INVALID_IDX(sizeType);
				this->mMaximum = INVALID_IDX(sizeType);
			}

			// Destroys all nodes, frees backing memory, and resets mMinimum/mMaximum.
			void deepClear()
			{
				this->mData.deepClear();
				this->mMinimum = INVALID_IDX(sizeType);
				this->mMaximum = INVALID_IDX(sizeType);
			}

			#define type SortedArray<T, sizeType>
			BASIC_ITERATOR_FUNCTIONS(type)
			#undef type
		};
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	namespace threadSafe {

		template<typename T, typename sizeType>
		class SortedArray : public SortedArrayBase<T, sizeType> {

		public:

			using value_type = T;
			using size_type = sizeType;
			using iterator = Iterator<SortedArray<T, sizeType>>;
			using const_iterator = const iterator;
			using IDX = Index<T, sizeType>;

		private:

			mutable std::shared_mutex mMutex;

		public:

			// Thread-safe insert. Acquires a unique lock.
			// Returns the Index of the inserted (or existing) node.
			IDX insert(const T& data)
			{
				std::unique_lock lock(this->mMutex);
				return insertInternal(data);
			}

			// Thread-safe erase by value. Acquires a unique lock.
			void erase(const T& data)
			{
				std::unique_lock lock(this->mMutex);
				eraseInternal(data);
			}

			// Thread-safe erase by Index. Acquires a unique lock.
			void erase(const IDX& index)
			{
				std::unique_lock lock(this->mMutex);
				this->eraseInternal(index);
			}

			// Thread-safe find. Acquires a shared lock.
			// Returns valid IDX on match, or invalid IDX if not found.
			IDX find(const T& data)
			{
				std::shared_lock lock(this->mMutex);
				return findInternal(data);
			}

			// Thread-safe minimum(). Acquires a shared lock.
			T& minimum()
			{
				std::shared_lock lock(this->mMutex);
				return this->mData[this->mMinimum].data;
			}

			// Const thread-safe minimum().
			const T& minimum() const
			{
				std::shared_lock lock(this->mMutex);
				return this->mData[this->mMinimum].data;
			}

			// Thread-safe maximum(). Acquires a shared lock.
			T& maximum()
			{
				std::shared_lock lock(this->mMutex);
				return this->mData[this->mMaximum].data;
			}

			// Const thread-safe maximum().
			const T& maximum() const
			{
				std::shared_lock lock(this->mMutex);
				return this->mData[this->mMaximum].data;
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

			// Thread-safe size(). Acquires a shared lock.
			sizeType size() const
			{
				std::shared_lock lock(this->mMutex);
				return this->mData.size();
			}

			// Thread-safe empty(). Acquires a shared lock.
			bool empty() const
			{
				std::shared_lock lock(this->mMutex);
				return this->mMinimum == INVALID_IDX(sizeType);
			}

			// Thread-safe wrap(). Acquires a unique lock.
			void wrap()
			{
				std::unique_lock lock(this->mMutex);
				this->mData.wrap();
			}

			// Thread-safe shallowClear(). Acquires a unique lock.
			// `callDestructors` – invoke ~T() on each element's data if true.
			void shallowClear(bool callDestructors)
			{
				std::unique_lock lock(this->mMutex);
				this->mData.shallowClear(callDestructors);
				this->mMinimum = INVALID_IDX(sizeType);
				this->mMaximum = INVALID_IDX(sizeType);
			}

			// Thread-safe deepClear(). Acquires a unique lock.
			void deepClear()
			{
				std::unique_lock lock(this->mMutex);
				this->mData.deepClear();
				this->mMinimum = INVALID_IDX(sizeType);
				this->mMaximum = INVALID_IDX(sizeType);
			}

			#define type SortedArray<T, sizeType>
			THREAD_SAFE_ITERATOR_FUNCTIONS(type)
			#undef type
		};
	}
}
