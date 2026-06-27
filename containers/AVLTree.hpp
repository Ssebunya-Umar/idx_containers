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
// AVLTree<T, sizeType>
//
// A self-balancing binary search tree (AVL variant).  The tree maintains the
// AVL invariant: the heights of the left and right subtrees of every node
// differ by at most 1.  This guarantees O(log n) for insert, erase, and find.
//
// BALANCE TRACKING
//   Each node stores a `height` (uint8).  After an insert or erase, a Stack
//   of ancestor indices is walked bottom-up to update heights and apply
//   rotations (LL, LR, RL, RR) where necessary.
//
// HEIGHT OVERFLOW WARNING
//   height is uint8 (max 255).  A perfectly balanced AVL tree of height 255
//   would hold approximately 2^255 nodes, which is astronomically large.
//   In practice this is not a concern, but if sizeType is uint8 and the tree
//   is heavily skewed (theoretically possible in adversarial input), the height
//   field could overflow and cause incorrect balance calculations.
//   Use uint16 or uint32 for sizeType in safety-critical contexts.
//
// ITERATION
//   Like BSTree, nextIndex() visits slots in insertion order, NOT in-order.
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include"indexedDynamicArray.hpp"
#include"stack.hpp"

#include<shared_mutex>
#include<mutex>

namespace idx {

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	template<typename T, typename sizeType>
	class AVLTreeBase {

	private:

		using IDX = Index<T, sizeType>;

	protected:

		struct Node {
			T data;
			sizeType right = INVALID_IDX(sizeType);
			sizeType left  = INVALID_IDX(sizeType);
			uint8 height = 0; // Height of this node's subtree (1 for a leaf).

			Node() : data(T()) {}
			Node(const T& d) : data(d), height(1) {}
		};

	protected:

		basic::IndexedDynamicArray<Node, sizeType> mData;
		sizeType mRootIndex = INVALID_IDX(sizeType);

	protected:

		// Returns the height of the node at `nodeIndex`.
		// Returns 0 if `nodeIndex` is INVALID_IDX (null child).
		// `nodeIndex` – the slot index of the node to query.
		uint8 getHeight(const sizeType& nodeIndex)
		{
			if (nodeIndex == INVALID_IDX(sizeType)) return 0;
			return this->mData[nodeIndex].height;
		}

		// Returns the balance factor of the node at `nodeIndex`.
		// Balance factor = height(left subtree) - height(right subtree).
		// Returns 0 if `nodeIndex` is INVALID_IDX.
		// A value > +1 means left-heavy; < -1 means right-heavy (rotation needed).
		// `nodeIndex` – the slot index of the node to query.
		sint8 getBalanceFactor(const sizeType& nodeIndex)
		{
			if (nodeIndex == INVALID_IDX(sizeType)) return 0;
			return getHeight(this->mData[nodeIndex].left) - getHeight(this->mData[nodeIndex].right);
		}

		// Recomputes and stores the height of the node at `nodeIndex`.
		// height = max(height(left), height(right)) + 1.
		// Must be called bottom-up after any structural change.
		// `nodeIndex` – the slot index of the node whose height to update.
		void updateHeight(const sizeType& nodeIndex)
		{
			uint8 heightR = getHeight(this->mData[nodeIndex].right);
			uint8 heightL = getHeight(this->mData[nodeIndex].left);
			this->mData[nodeIndex].height = (heightR > heightL ? heightR : heightL) + 1;
		}

		// Performs a left rotation around `nodeIndex` (handles right-heavy imbalance).
		//
		//     A (nodeIndex)           B (newRoot)
		//      \              →      / \
		//       B                   A   B.right
		//      / \                   \
		//   B.left  B.right         B.left
		//
		// `nodeIndex` – the slot index of the node to rotate around.
		// Returns the slot index of the new subtree root (formerly nodeIndex's right child).
		sizeType rotateLeft(const sizeType& nodeIndex)
		{
			sizeType newRoot = this->mData[nodeIndex].right;

			this->mData[nodeIndex].right = this->mData[newRoot].left;
			this->mData[newRoot].left = nodeIndex;

			updateHeight(nodeIndex);
			updateHeight(newRoot);

			return newRoot;
		}

		// Performs a right rotation around `nodeIndex` (handles left-heavy imbalance).
		//
		//       A (nodeIndex)         B (newRoot)
		//      /              →      / \
		//     B                B.left   A
		//    / \                       /
		// B.left B.right            B.right
		//
		// `nodeIndex` – the slot index of the node to rotate around.
		// Returns the slot index of the new subtree root (formerly nodeIndex's left child).
		sizeType rotateRight(const sizeType& nodeIndex)
		{
			sizeType newRoot = this->mData[nodeIndex].left;

			this->mData[nodeIndex].left = this->mData[newRoot].right;
			this->mData[newRoot].right = nodeIndex;

			updateHeight(nodeIndex);
			updateHeight(newRoot);

			return newRoot;
		}

		// Walks `indexStack` from TOP to BOTTOM (deepest ancestor first, root last),
		// updating heights and applying AVL rotations (LL, LR, RL, RR) where needed.
		//
		// The Stack is built during descent so index 0 = root, index size-1 = deepest
		// ancestor (the node directly above the insertion/deletion point).  Correct AVL
		// rebalancing MUST process deepest-first so that a child's balance factor and
		// height are accurate before its parent is inspected.
		//
		// `indexStack` – ancestor slot indices, index 0 = root, index size-1 = deepest.
		void balance(const basic::Stack<sizeType, sizeType>& indexStack)
		{
			// i walks from size-1 (deepest) down to 0 (root).
			for (sizeType i = indexStack.size(); i-- > 0;) {

				const sizeType& nodeIndex = indexStack[i];

				// Update this node's height from its (now-correct) children.
				updateHeight(nodeIndex);
				sizeType index = nodeIndex;

				const sint8 balanceFactor = getBalanceFactor(index);

				if (balanceFactor > 1) {
					// Left-heavy.
					if (getBalanceFactor(this->mData[index].left) >= 0) {
						// LL: single right rotation.
						index = rotateRight(index);
					}
					else {
						// LR: left-rotate the left child, then right-rotate here.
						this->mData[index].left = rotateLeft(this->mData[index].left);
						index = rotateRight(index);
					}
				}
				else if (balanceFactor < -1) {
					// Right-heavy.
					if (getBalanceFactor(this->mData[index].right) <= 0) {
						// RR: single left rotation.
						index = rotateLeft(index);
					}
					else {
						// RL: right-rotate the right child, then left-rotate here.
						this->mData[index].right = rotateRight(this->mData[index].right);
						index = rotateLeft(index);
					}
				}

				if (i == 0) {
					// Reached the root — update mRootIndex to the (possibly new) subtree root.
					this->mRootIndex = index;
				}
				else {
					// Wire the (possibly rotated) subtree back into its parent.
					// The parent is the ancestor one level closer to the root.
					const sizeType parent = indexStack[i - 1];

					if (this->mData[parent].right == nodeIndex) {
						this->mData[parent].right = index;
					}
					else {
						this->mData[parent].left = index;
					}
				}
			}
		}

		// Inserts `data` into the AVL tree, then rebalances affected ancestors.
		// Descends from mRootIndex, recording ancestors in `indexStack`.
		// If `data` already exists, returns the existing Index without inserting.
		// After insertion, calls balance() on the ancestor stack to restore the AVL invariant.
		// `data` – the value to insert.
		// Returns a valid Index wrapping the (existing or new) node's slot.
		IDX insertInternal(const T& data)
		{
			sizeType currentIndex = this->mRootIndex;
			basic::Stack<sizeType, sizeType> indexStack;

			while (currentIndex != INVALID_IDX(sizeType)) {

				indexStack.push(currentIndex);

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

			if (indexStack.empty()) {
				this->mRootIndex = index.ref(); // Tree was empty; new node is the root.
			}
			else {
				sizeType parent = indexStack.top();

				if (this->mData[parent].data < data) {
					this->mData[parent].right = index.ref();
				}
				else {
					this->mData[parent].left = index.ref();
				}

				balance(indexStack);
			}

			return IDX(index.ref());
		}

		// Removes the node holding `data` and rebalances the tree bottom-up.
		// Uses the in-order successor strategy for two-child nodes (same as BSTree),
		// then calls balance() on the ancestor stack to restore the AVL invariant.
		// `data` – the value to remove. No-op if not found.
		void eraseInternal(const T& data)
		{
			sizeType currentIndex = this->mRootIndex;
			basic::Stack<sizeType, sizeType> indexStack;

			while (currentIndex != INVALID_IDX(sizeType)) {

				if (this->mData[currentIndex].data > data) {
					indexStack.push(currentIndex);
					currentIndex = this->mData[currentIndex].left;
				}
				else if (this->mData[currentIndex].data < data) {
					indexStack.push(currentIndex);
					currentIndex = this->mData[currentIndex].right;
				}
				else {

					sizeType successor = INVALID_IDX(sizeType);

					if (this->mData[currentIndex].left == INVALID_IDX(sizeType) || this->mData[currentIndex].right == INVALID_IDX(sizeType)) {
						// Leaf or single-child node.
						successor = this->mData[currentIndex].left != INVALID_IDX(sizeType) ? this->mData[currentIndex].left : this->mData[currentIndex].right;
					}
					else {
						// Two-child node: find in-order successor (leftmost of right subtree).
						sizeType successorParent = currentIndex;
						successor = this->mData[currentIndex].right;

						while (this->mData[successor].left != INVALID_IDX(sizeType)) {
							successorParent = successor;
							successor = this->mData[successor].left;
						}

						if (successorParent != currentIndex) {
							// Detach successor from its parent and adopt its right child.
							if (this->mData[successor].right != INVALID_IDX(sizeType)) {
								this->mData[successorParent].left = this->mData[successor].right;
							}
							else {
								this->mData[successorParent].left = INVALID_IDX(sizeType);
							}

							this->mData[successor].right = this->mData[currentIndex].right;
						}

						this->mData[successor].left = this->mData[currentIndex].left;
					}

					// Link the successor into the removed node's position.
					if (indexStack.empty()) {
						this->mRootIndex = successor;
					}
					else {
						sizeType parent = indexStack.top();
						if (this->mData[parent].right == currentIndex) {
							this->mData[parent].right = successor;
						}
						else {
							this->mData[parent].left = successor;
						}
					}

					// Push the successor so balance() can update its height too.
					if (successor != INVALID_IDX(sizeType)) {
						indexStack.push(successor);
					}

					balance(indexStack);

					this->mData.erase(Index<Node, sizeType>(currentIndex));
					return;
				}
			}
		}

		// Searches the AVL tree for `data`. O(log n) guaranteed.
		// `data` – the value to look for.
		// Returns a valid Index if found, or an invalid Index if not.
		IDX findInternal(const T& data)
		{
			sizeType currentIndex = this->mRootIndex;

			while (currentIndex != INVALID_IDX(sizeType)) {

				if (this->mData[currentIndex].data < data) {
					currentIndex = this->mData[currentIndex].right;
				}
				else if (this->mData[currentIndex].data > data) {
					currentIndex = this->mData[currentIndex].left;
				}
				else {
					return IDX(currentIndex);
				}
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

		AVLTreeBase() {}
		~AVLTreeBase() {}

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
		class AVLTree : public AVLTreeBase<T, sizeType> {

		public:

			using value_type = T;
			using size_type = sizeType;
			using iterator = Iterator<AVLTree<T, sizeType>>;
			using const_iterator = const iterator;
			using IDX = Index<T, sizeType>;

		public:

			// Default constructor. Empty tree; no allocation.
			AVLTree() : AVLTreeBase<T, sizeType>() {}

			// Swaps the node stores and root indices of two AVL trees in O(1).
			// `other` – the AVLTree to swap with.
			void swap(AVLTree<T, sizeType>& other)
			{
				this->mData.swap(other.mData);
				std::swap(this->mRootIndex, other.mRootIndex);
			}

			// Inserts `data` into the AVL tree, rebalancing as needed. O(log n).
			// Duplicates are silently ignored.
			// `data` – the value to insert.
			// Returns the Index of the (existing or new) node.
			IDX insert(const T& data)
			{
				return this->insertInternal(data);
			}

			// Removes the node holding `data`, rebalancing as needed. O(log n).
			// No-op if `data` is not in the tree.
			// `data` – the value to remove.
			void erase(const T& data)
			{
				this->eraseInternal(data);
			}

			// Searches for `data`. Guaranteed O(log n).
			// `data` – the value to look for.
			// Returns a valid Index if found, or an invalid Index if not.
			IDX find(const T& data)
			{
				return this->findInternal(data);
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

			// Returns true when the tree has no nodes.
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
			
			#define type AVLTree<T, sizeType>
			BASIC_ITERATOR_FUNCTIONS(type)
			#undef type
		};
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	namespace threadSafe {
			
		template<typename T, typename sizeType>
		class AVLTree : public AVLTreeBase<T, sizeType> {

		public:

			using value_type = T;
			using size_type = sizeType;
			using iterator = Iterator<AVLTree<T, sizeType>>;
			using const_iterator = const iterator;
			using IDX = Index<T, sizeType>;

		private:

			mutable std::shared_mutex mMutex;

		public:

			// Default constructor.
			AVLTree() : AVLTreeBase<T, sizeType>() {}

			// Thread-safe swap. Acquires a scoped lock on both trees.
			// `other` – the AVLTree to swap with. No-op if this == &other.
			void swap(AVLTree<T, sizeType>& other)
			{
				if (this == &other) return;
				std::scoped_lock lock(this->mMutex, other.mMutex);
				
				this->mData.swap(other.mData);
				std::swap(this->mRootIndex, other.mRootIndex);
			}

			// Thread-safe insert. Acquires a unique lock.
			// Inserts `data` and rebalances. Duplicates are ignored.
			// Returns the Index of the (existing or new) node.
			IDX insert(const T& data)
			{
				std::unique_lock lock(this->mMutex);
				return this->insertInternal(data);
			}

			// Thread-safe erase. Acquires a unique lock.
			// Removes `data` and rebalances. No-op if not found.
			void erase(const T& data)
			{
				std::unique_lock lock(this->mMutex);
				eraseInternal(data);
			}

			// Thread-safe find. Acquires a shared lock.
			// `data` – the value to search for.
			// Returns a valid Index if found, or an invalid Index if not.
			IDX find(const T& data)
			{
				std::shared_lock lock(this->mMutex);
				return this->findInternal(data);
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
			
			#define type AVLTree<T, sizeType>
			THREAD_SAFE_ITERATOR_FUNCTIONS(type)
			#undef type
		};
	}
}
