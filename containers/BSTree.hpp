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
// BinarySearchTree<T, sizeType>  (BST / BSTree)
//
// An unbalanced binary search tree stored in an IndexedDynamicArray.
// Duplicate values are silently ignored (insert returns the existing index).
//
// COMPLEXITY
//   Best/average: O(log n).  Worst case: O(n) for sorted input (degenerates
//   to a linked list). Consider AVLTree if the input order is unknown.
//
// ERASE
//   Uses the in-order successor strategy: when deleting a node with two
//   children, the leftmost node of the right subtree replaces it.
//
// ITERATION
//   nextIndex() delegates to IndexedDynamicArray::nextIndex (slot order),
//   NOT in-order tree traversal.  Elements are visited in slot-insertion
//   order, not sorted order.
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include"indexedDynamicArray.hpp"

#include<shared_mutex>
#include<mutex>

namespace idx {

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	template<typename T, typename sizeType>
	class BinarySearchTreeBase {

	private:

		using IDX = Index<T, sizeType>;
	
	protected:

		struct Node {
			T data;
			sizeType right = INVALID_IDX(sizeType);
			sizeType left  = INVALID_IDX(sizeType);

			Node() {}
			explicit Node(const T& d) : data(d) {}
		};

	protected:

		basic::IndexedDynamicArray<Node, sizeType> mData;
		sizeType mRootIndex = INVALID_IDX(sizeType);

	protected:

		// Inserts `data` into the BST at the correct position according to operator< / operator>.
		// Descends the tree from mRootIndex: go left if current node > data, right if < data.
		// If `data` already exists, returns the existing node's Index (no duplicate is inserted).
		// `data` – the value to insert.
		// Returns a valid Index wrapping the (existing or new) node's slot.
		IDX insertInternal(const T& data)
		{
			sizeType currentIndex = this->mRootIndex;
			sizeType parentIndex = INVALID_IDX(sizeType);

			while (currentIndex != INVALID_IDX(sizeType)) {

				parentIndex = currentIndex;

				if (this->mData[currentIndex].data > data) {
					currentIndex = this->mData[currentIndex].left;
				}
				else if (this->mData[currentIndex].data < data) {
					currentIndex = this->mData[currentIndex].right;
				}
				else {
					return IDX(currentIndex); // Duplicate — return existing.
				}
			}

			auto index = this->mData.insert(Node(data));

			if (parentIndex == INVALID_IDX(sizeType)) {
				this->mRootIndex = index.ref(); // Tree was empty; new node is the root.
			}
			else if (this->mData[parentIndex].data < data) {
				this->mData[parentIndex].right = index.ref();
			}
			else {
				this->mData[parentIndex].left = index.ref();
			}

			return IDX(index.ref());
		}

		// Searches the BST for `data` using the standard BST descent.
		// `data` – the value to search for.
		// Returns a valid Index if found, or an invalid Index if not present.
		IDX findInternal(const T& data)
		{
			sizeType currentIndex = this->mRootIndex;

			while (currentIndex != INVALID_IDX(sizeType)) {

				if (this->mData[currentIndex].data > data) {
					currentIndex = this->mData[currentIndex].left;
				}
				else if (this->mData[currentIndex].data < data) {
					currentIndex = this->mData[currentIndex].right;
				}
				else {
					return IDX(currentIndex);
				}
			}

			return IDX();
		}

		// Removes the node holding `data` from the BST using the in-order successor strategy.
		//
		// Cases handled:
		//   - Leaf (no children):        simply unlinks from parent.
		//   - One child:                 parent adopts the surviving child.
		//   - Two children:              replaces with the in-order successor
		//                                (leftmost node of the right subtree),
		//                                then repairs the successor's parent link.
		//
		// `data` – the value to remove. No-op if not found.
		void eraseInternal(const T& data)
		{
			sizeType currentIndex = this->mRootIndex;
			sizeType parentIndex = INVALID_IDX(sizeType);

			while (currentIndex != INVALID_IDX(sizeType)) {

				if (this->mData[currentIndex].data < data) {
					parentIndex = currentIndex;
					currentIndex = this->mData[currentIndex].right;
				}
				else if (this->mData[currentIndex].data > data) {
					parentIndex = currentIndex;
					currentIndex = this->mData[currentIndex].left;
				}
				else {
					
					sizeType successor = INVALID_IDX(sizeType);

					if (this->mData[currentIndex].left == INVALID_IDX(sizeType) || this->mData[currentIndex].right == INVALID_IDX(sizeType)) {
						// Zero or one child: pick whichever child exists (or INVALID_IDX for leaf).
						successor = this->mData[currentIndex].left != INVALID_IDX(sizeType) ? this->mData[currentIndex].left : this->mData[currentIndex].right;
					}
					else {
						// Two children: find the leftmost node of the right subtree.
						sizeType parent = currentIndex;
						successor = this->mData[currentIndex].right;

						while (this->mData[successor].left != INVALID_IDX(sizeType)) {
							parent = successor;
							successor = this->mData[successor].left;
						}

						if (parent != currentIndex) {
							// Detach successor from its parent and adopt its right child.
							if (this->mData[successor].right != INVALID_IDX(sizeType)) {
								this->mData[parent].left = this->mData[successor].right;
							}
							else {
								this->mData[parent].left = INVALID_IDX(sizeType);
							}

							this->mData[successor].right = this->mData[currentIndex].right;
						}

						this->mData[successor].left = this->mData[currentIndex].left;
					}

					// Relink the successor into the position of the removed node.
					if (parentIndex == INVALID_IDX(sizeType)) {
						this->mRootIndex = successor;
					}
					else {
						if (this->mData[parentIndex].right == currentIndex) {
							this->mData[parentIndex].right = successor;
						}
						else {
							this->mData[parentIndex].left = successor;
						}
					}

					this->mData.erase(currentIndex);
					return;
				}
			}
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

		BinarySearchTreeBase() {}
		~BinarySearchTreeBase() {}

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
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	namespace basic {
			
		template<typename T, typename sizeType>
		class BinarySearchTree : public BinarySearchTreeBase<T, sizeType> {

		public:

			using value_type = T;
			using size_type = sizeType;
			using iterator = Iterator<BinarySearchTree<T, sizeType>>;
			using const_iterator = const iterator;
			using IDX = Index<T, sizeType>;

		public:

			// Inserts `data` into the BST. Duplicates are ignored.
			// `data` – the value to insert.
			// Returns the Index of the (existing or new) node.
			IDX insert(const T& data)
			{
				return insertInternal(data);
			}

			// Searches the BST for `data`. O(log n) average, O(n) worst.
			// `data` – the value to look for.
			// Returns a valid Index if found, or an invalid Index if not.
			IDX find(const T& data)
			{
				return findInternal(data);
			}

			// Removes the node holding `data`. No-op if not found.
			// `data` – the value to remove from the BST.
			void erase(const T& data)
			{
				eraseInternal(data);
			}

			// Access by typed IDX.
			// `index` – a valid Index from insert() or find(). Must be live.
			// Returns a mutable reference to the stored value.
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
			// `index` – raw slot in the backing IndexedDynamicArray. Must be live.
			T& operator[](const sizeType& index)
			{
				return this->refInternal(index);
			}

			// Const overload of operator[](sizeType).
			const T& operator[](const sizeType& index) const
			{
				return this->refInternal(index);
			}

			// Returns the number of live nodes.
			sizeType size() const
			{
				return this->mData.size();
			}

			// Returns true when the tree is empty.
			bool empty() const
			{
				return this->mData.empty();
			}

			// Trims the backing store's capacity to size(). Reclaims unused memory.
			void wrap()
			{
				this->mData.wrap();
			}

			// Resets the tree to empty, keeping the backing allocation.
			// Resets mRootIndex to INVALID_IDX.
			// `callDestructors` – if true, ~T() is called on each node's data first.
			void shallowClear(bool callDestructors)
			{
				this->mData.shallowClear(callDestructors);
				this->mRootIndex = INVALID_IDX(sizeType);
			}

			// Destroys all nodes, frees the backing store, and resets mRootIndex.
			void deepClear()
			{
				this->mData.deepClear();
				this->mRootIndex = INVALID_IDX(sizeType);
			}
			
			#define type BinarySearchTree<T, sizeType>
			BASIC_ITERATOR_FUNCTIONS(type)
			#undef type
		};
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	namespace threadSafe {
			
		template<typename T, typename sizeType>
		class BinarySearchTree : public BinarySearchTreeBase<T, sizeType> {

		public:

			using value_type = T;
			using size_type = sizeType;
			using iterator = Iterator<BinarySearchTree<T, sizeType>>;
			using const_iterator = const iterator;
			using IDX = Index<T, sizeType>;

		private:

			mutable std::shared_mutex mMutex;

		public:

			// Thread-safe insert. Acquires a unique lock.
			// `data` – value to insert. Duplicates are ignored.
			// Returns the Index of the (existing or new) node.
			IDX insert(const T& data)
			{
				std::unique_lock lock(this->mMutex);
				return insertInternal(data);
			}

			// Thread-safe find. Acquires a shared lock.
			// `data` – value to look for.
			// Returns a valid Index if found, or an invalid Index if not.
			IDX find(const T& data)
			{
				std::shared_lock lock(this->mMutex);
				return findInternal(data);
			}

			// Thread-safe erase. Acquires a unique lock.
			// `data` – value to remove. No-op if not found.
			void erase(const T& data)
			{
				std::unique_lock lock(this->mMutex);
				eraseInternal(data);
			}

			// Thread-safe operator[](IDX). Acquires a shared lock.
			// `index` – typed Index from insert() or find(). Must be live.
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
			// `index` – raw slot. Must be live.
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
				return this->mData.empty();
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
				this->mRootIndex = INVALID_IDX(sizeType);
			}

			// Thread-safe deepClear(). Acquires a unique lock.
			// Destroys all nodes and frees the backing store.
			void deepClear()
			{
				std::unique_lock lock(this->mMutex);
				this->mData.deepClear();
				this->mRootIndex = INVALID_IDX(sizeType);
			}
			
			#define type BinarySearchTree<T, sizeType>
			THREAD_SAFE_ITERATOR_FUNCTIONS(type)
			#undef type
		};
	}
}
