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
// Queue<T, sizeType>  (basic:: and threadSafe::)
// A FIFO queue backed by a singly-linked list inside an IndexedDynamicArray.
// ─────────────────────────────────────────────────────────────────────────────

#include"indexedDynamicArray.hpp"

#include<shared_mutex>
#include<mutex>

namespace idx {

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	template<typename T, typename sizeType>
	class QueueBase {

	private:

		using IDX = Index<T, sizeType>;

	protected:

		struct Node {
			T data = T();
			sizeType next = INVALID_IDX(sizeType); // Index of the next node in FIFO order

			Node() {}
			Node(const T& d) : data(d) {}
		};

	protected:

		basic::IndexedDynamicArray<Node, sizeType> mData; // Slot-map holding all nodes
		sizeType mFront = INVALID_IDX(sizeType);          // Index of the front (oldest) node
		sizeType mBack  = INVALID_IDX(sizeType);          // Index of the back (newest) node

	protected:

		// Inserts `data` at the back of the queue.
		// Allocates a new Node in mData and links it after the current back node.
		// If the queue was empty, both mFront and mBack are set to the new node.
		// `data` – the value to enqueue.
		// Returns an Index wrapping the new node's slot.
		IDX pushInternal(const T& data)
		{
			auto index = this->mData.insert(Node(data));
			if (this->mFront == INVALID_IDX(sizeType)) {
				this->mFront = this->mBack = index.ref();
			}
			else {
				this->mData[this->mBack].next = index.ref();
				this->mBack = index.ref();
			}
			return IDX(index.ref());
		}

		// Removes the front node from the queue. O(1).
		// If the queue had only one node, both mFront and mBack become INVALID_IDX.
		// No-op if the queue is already empty.
		void popInternal()
		{
			if (this->mFront == INVALID_IDX(sizeType)) return;
			sizeType index = this->mFront;
			if (this->mFront == this->mBack) {
				this->mFront = this->mBack = INVALID_IDX(sizeType);
			}
			else {
				this->mFront = this->mData[this->mFront].next;
			}
			this->mData.erase(IDX(index));
		}

		// Removes the FIRST node whose data equals `data`. O(n).
		// Fixes up the linked-list pointers and updates mFront/mBack as needed.
		// `data` – the value to search for and remove.
		void eraseInternal(const T& data)
		{
			sizeType current = this->mFront;
			sizeType parent = INVALID_IDX(sizeType);
			while (current != INVALID_IDX(sizeType)) {
				if (this->mData[current].data == data) {
					if (parent == INVALID_IDX(sizeType)) {
						if (this->mFront == this->mBack) {
							this->mFront = this->mBack = INVALID_IDX(sizeType);
						}
						else {
							this->mFront = this->mData[this->mFront].next;
						}
					}
					else {
						if (current == this->mBack) {
							this->mBack = parent;
						}
						this->mData[parent].next = this->mData[current].next;
					}
					this->mData.erase(IDX(current));
					break;
				}
				parent = current;
				current = this->mData[current].next;
			}
		}

		// Linear O(n) search following the linked-list chain from mFront.
		// `data` – the value to look for.
		// Returns a valid IDX on the first match, or an invalid IDX if not found.
		IDX findInternal(const T& data)
		{
			sizeType current = this->mFront;
			while (current != INVALID_IDX(sizeType)) {
				if (this->mData[current].data == data) {
					return IDX(current);
				}
				current = this->mData[current].next;
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

		QueueBase() {}
		~QueueBase() {}

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
		class Queue : QueueBase<T, sizeType> {

		public:

			using value_type = T;
			using size_type = sizeType;
			using iterator = Iterator<Queue<T, sizeType>>;
			using const_iterator = const iterator;
			using IDX = Index<T, sizeType>;

		public:

			// Enqueues a copy of `data` at the back. Amortised O(1).
			// `data` – the value to add to the back of the queue.
			// Returns a stable Index for this node (can be used with operator[] later).
			IDX push(const T& data)
			{
				return this->pushInternal(data);
			}

			// Dequeues and destroys the front element. O(1).
			// No-op if the queue is empty.
			void pop()
			{
				this->popInternal();
			}

			// Removes the first element whose value equals `data`. O(n).
			// Only the first match is removed.
			// `data` – the value to search for and dequeue.
			void erase(const T& data)
			{
				this->eraseInternal(data);
			}

			// Linear O(n) search following FIFO order.
			// `data` – the value to look for.
			// Returns a valid IDX on the first match, or an invalid IDX if not found.
			IDX find(const T& data)
			{
				return this->findInternal(data);
			}

			// Access by typed IDX (obtained from push() or find()).
			// `index` – stable node index. Must be currently live.
			// Returns a mutable reference to the node's data.
			T& operator[](const IDX& index)
			{
				return this->refInternal(index.ref());
			}

			// Const overload of operator[](IDX).
			const T& operator[](const IDX& index) const
			{
				return this->refInternal(index.ref());
			}

			// Access by raw slot index.
			// `index` – raw slot in the internal IndexedDynamicArray.
			T& operator[](const sizeType& index)
			{
				return this->refInternal(index);
			}

			// Const overload of operator[](sizeType).
			const T& operator[](const sizeType& index) const
			{
				return this->refInternal(index);
			}

			// Returns a mutable reference to the front (oldest) element.
			// Behaviour is undefined if the queue is empty.
			T& front()
			{
				return this->mData[this->mFront].data;
			}

			// Const overload of front().
			const T& front() const
			{
				return this->mData[this->mFront].data;
			}

			// Returns the number of elements currently in the queue.
			sizeType size() const
			{
				return this->mData.size();
			}

			// Returns true when the queue has no elements.
			bool empty() const
			{
				return this->mFront == INVALID_IDX(sizeType);
			}

			// Trims the internal slot-map's capacity to size(). Reclaims unused memory.
			void wrap()
			{
				this->mData.wrap();
			}

			// Resets the queue to empty, keeping the backing allocation.
			// Resets mFront and mBack to INVALID_IDX.
			// `callDestructors` – if true, ~T() is called on each node's data first.
			void shallowClear(bool callDestructors)
			{
				this->mData.shallowClear(callDestructors);
				this->mFront = INVALID_IDX(sizeType);
				this->mBack  = INVALID_IDX(sizeType);
			}

			// Destroys all nodes, frees backing memory, and resets mFront/mBack.
			void deepClear()
			{
				this->mData.deepClear();
				this->mFront = INVALID_IDX(sizeType);
				this->mBack  = INVALID_IDX(sizeType);
			}

			#define type Queue<T, sizeType>
			BASIC_ITERATOR_FUNCTIONS(type)
			#undef type
		};
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	namespace threadSafe {

		template<typename T, typename sizeType>
		class Queue : QueueBase<T, sizeType> {

		public:

			using value_type = T;
			using size_type = sizeType;
			using iterator = Iterator<Queue<T, sizeType>>;
			using const_iterator = const iterator;
			using IDX = Index<T, sizeType>;

		private:

			mutable std::shared_mutex mMutex;

		public:

			// Thread-safe push. Acquires a unique lock.
			// `data` – value to enqueue at the back.
			// Returns a stable Index for the new node.
			IDX push(const T& data)
			{
				std::unique_lock lock(this->mMutex);
				return this->pushInternal(data);
			}

			// Thread-safe pop. Acquires a unique lock.
			// Dequeues and destroys the front element. No-op if empty.
			void pop()
			{
				std::unique_lock lock(this->mMutex);
				this->popInternal();
			}

			// Thread-safe erase by value. Acquires a unique lock.
			// Removes the first element matching `data`.
			void erase(const T& data)
			{
				std::unique_lock lock(this->mMutex);
				this->eraseInternal(data);
			}

			// Thread-safe find. Acquires a shared lock.
			// `data` – value to search for. Returns valid IDX or invalid IDX.
			IDX find(const T& data)
			{
				std::shared_lock lock(this->mMutex);
				return this->findInternal(data);
			}

			// Thread-safe operator[](IDX). Acquires a shared lock.
			// `index` – stable node index from push() or find().
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
			// `index` – raw slot in the backing store.
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

			// Thread-safe front(). Acquires a shared lock.
			// Returns a reference to the front element. Undefined if empty.
			T& front()
			{
				std::shared_lock lock(this->mMutex);
				return this->mData[this->mFront].data;
			}

			// Const thread-safe front().
			const T& front() const
			{
				std::shared_lock lock(this->mMutex);
				return this->mData[this->mFront].data;
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
				return this->mFront == INVALID_IDX(sizeType);
			}

			// Thread-safe wrap(). Acquires a unique lock. Trims backing capacity.
			void wrap()
			{
				std::unique_lock lock(this->mMutex);
				this->mData.wrap();
			}

			// Thread-safe shallowClear(). Acquires a unique lock.
			// `callDestructors` – invoke ~T() on each node's data if true.
			void shallowClear(bool callDestructors)
			{
				std::unique_lock lock(this->mMutex);
				this->mData.shallowClear(callDestructors);
				this->mFront = INVALID_IDX(sizeType);
				this->mBack  = INVALID_IDX(sizeType);
			}

			// Thread-safe deepClear(). Acquires a unique lock.
			// Destroys all nodes and frees backing memory.
			void deepClear()
			{
				std::unique_lock lock(this->mMutex);
				this->mData.deepClear();
				this->mFront = INVALID_IDX(sizeType);
				this->mBack  = INVALID_IDX(sizeType);
			}

			#define type Queue<T, sizeType>
			THREAD_SAFE_ITERATOR_FUNCTIONS(type)
			#undef type
		};
	}
}
