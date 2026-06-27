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

#include"core/core.hpp"
#include"core/assert.hpp"

#include <utility>
#include <shared_mutex>

namespace idx {

// ─────────────────────────────────────────────────────────────────────────────
// basic::Iterator<Container>
//
// A generic forward/backward iterator for any idx container that exposes:
//   value_type, size_type, nextIndex(sizeType), prevIndex(sizeType), ref(sizeType)
//
// INVALID_IDX(sizeType) is the "past-the-end" sentinel. An iterator holding
// INVALID_IDX as its current index is considered the end iterator.
//
// FACTORY FUNCTIONS
//   Constructing iterators is done through basic::factory free functions, not
//   through static methods on the iterator class itself. The container macros
//   BASIC_ITERATOR_FUNCTIONS(type) wire those factory calls into the standard
//   begin()/end()/rbegin()/rend()/cbegin()/cend() member functions.
//
// SPARSE CONTAINER SAFETY
//   begin() and rbegin() use nextIndex(INVALID_IDX) to find the first live slot
//   rather than unconditionally starting at slot 0. Because sizeType is unsigned,
//   INVALID_IDX + 1 == 0, so nextIndex(INVALID_IDX) scans forward from slot 0 —
//   identical to the old behaviour for dense containers (DynamicArray, StaticArray)
//   and correct for sparse ones (IndexedDynamicArray, AVLTree, BSTree, etc.)
//   where slot 0 may be free after erasures or rotations.
// ─────────────────────────────────────────────────────────────────────────────

	namespace basic {
		
		template<typename Container, typename valueType = typename Container::value_type, typename sizeType = typename Container::size_type>
		class Iterator {

		private:

			Container* mContainer = nullptr;
			sizeType mCurrentIndex = INVALID_IDX(sizeType);

		public:

			// End-iterator constructor.
			// Creates an iterator with no current position (mCurrentIndex = INVALID_IDX).
			// `container` – pointer to the container this iterator belongs to.
			Iterator(Container* container) : mContainer(container) {}

			// Positioned constructor.
			// Creates an iterator pointing at `index` inside `container`.
			// `container` – the container to iterate.
			// `index`     – the starting slot. Must be a valid occupied slot.
			Iterator(Container* container, const sizeType& index) : mContainer(container), mCurrentIndex(index) {}

			// Pre-increment. Advances to the next valid element by calling nextIndex().
			// When nextIndex() returns INVALID_IDX the iterator becomes the end iterator.
			// Returns *this after advancing.
			Iterator& operator++()
			{
				if (this->mContainer) {
					this->mCurrentIndex = mContainer->nextIndex(this->mCurrentIndex);
				}
				return *this;
			}

			// Post-increment. Returns a copy of the iterator before advancing, then advances.
			Iterator operator++(int)
			{
				Iterator old = *this;
				++(*this);
				return old;
			}

			// Pre-decrement. Moves to the previous slot by calling prevIndex().
			// When prevIndex() returns INVALID_IDX the iterator becomes the end iterator
			// (symmetric with rend()).
			// Returns *this after decrementing.
			Iterator& operator--()
			{
				if (this->mContainer) {
					this->mCurrentIndex = mContainer->prevIndex(this->mCurrentIndex);
				}
				return *this;
			}

			// Post-decrement. Returns a copy of the iterator before decrementing, then decrements.
			Iterator operator--(int)
			{
				Iterator old = *this;
				--(*this);
				return old;
			}

			// Dereferences the iterator.
			// Behaviour is undefined if the iterator is at end (isEnd() == true).
			// Returns a mutable reference to the element at mCurrentIndex.
			valueType& operator*()
			{
				return this->mContainer->ref(this->mCurrentIndex);
			}

			// Arrow operator. Returns a pointer to the current element.
			valueType* operator->()
			{
				return &(**this);
			}

			// Equality. Two iterators are equal when they point to the same container
			// AND the same index (including both being the end iterator).
			bool operator==(const Iterator& other) const
			{
				return this->mContainer == other.mContainer && this->mCurrentIndex == other.mCurrentIndex;
			}

			// Inequality. Returns true if the two iterators refer to different positions.
			bool operator!=(const Iterator& other) const
			{
				return !(*this == other);
			}

			// Returns true when mCurrentIndex equals the INVALID_IDX sentinel
			// (i.e. the iterator is past the last element).
			bool isEnd() const
			{
				return this->mCurrentIndex == INVALID_IDX(sizeType);
			}

			// Named alternative to operator*(). Returns a mutable reference to the
			// current element. Useful in non-range-for loops for readability.
			valueType& data()
			{
				return this->mContainer->ref(this->mCurrentIndex);
			}

			// Const overload of data().
			const valueType& data() const
			{
				return this->mContainer->ref(this->mCurrentIndex);
			}

			// Returns the key at the current position for key-value containers (e.g. HashMap).
			// `key_type` must be specified explicitly: it.key<MyKeyType>()
			template<typename key_type>
			const key_type& key() const
			{
				return this->mContainer->key(this->mCurrentIndex);
			}

			// Returns a mutable reference to the value at the current position.
			// For key-value containers (e.g. HashMap) this is the value half of the pair.
			valueType& value()
			{
				return this->mContainer->value(this->mCurrentIndex);
			}

			// Const overload of value().
			const valueType& value() const
			{
				return this->mContainer->value(this->mCurrentIndex);
			}

			// Returns the raw slot index of the current position.
			// Returns INVALID_IDX(sizeType) when the iterator is at end.
			sizeType index() const
			{
				return this->mCurrentIndex;
			}
		};

		// ─────────────────────────────────────────────────────────────────────────────
		// basic::factory
		//
		// Free functions that construct basic::Iterator instances. These are called by
		// the BASIC_ITERATOR_FUNCTIONS macro to implement the standard begin/end/
		// rbegin/rend container interface.
		//
		// WHY A SEPARATE NAMESPACE INSTEAD OF STATIC METHODS
		//   Moving factories out of the Iterator class body allows them to be templated
		//   on the Container type independently, keeping the iterator class itself
		//   simpler and making the factory callable from the macro without needing a
		//   specific Iterator<Container> instantiation in scope first.
		// ─────────────────────────────────────────────────────────────────────────────
		namespace factory {
			
			// Returns an iterator at the first LIVE element of `container`.
			// Calls nextIndex(INVALID_IDX) — since sizeType is unsigned,
			// INVALID_IDX + 1 == 0, so this scans forward from slot 0 and skips
			// any free leading slots (safe for sparse containers).
			// Returns the end iterator if `container` is null or empty.
			template<typename Container>
			static Iterator<Container> begin(Container* container)
			{
				if (container && container->empty() == false) {
					typename Container::size_type first = container->nextIndex(INVALID_IDX(typename Container::size_type));
					if (first != INVALID_IDX(typename Container::size_type)) {
						return Iterator(container, first);
					}
				}
				return Iterator(container);
			}

			// Returns the end iterator (mCurrentIndex = INVALID_IDX).
			// Used as the termination sentinel in range-for and manual loops.
			template<typename Container>
			static Iterator<Container> end(Container* container)
			{
				return Iterator(container);
			}

			// Returns an iterator at the last LIVE element of `container`.
			// Scans forward with nextIndex to find the last occupied slot — O(n)
			// but correct for both dense and sparse containers.
			// Returns the end iterator if `container` is null or empty.
			template<typename Container>
			static Iterator<Container> rbegin(Container* container)
			{
				if (container && container->empty() == false) {
					typename Container::size_type last = INVALID_IDX(typename Container::size_type);
					typename Container::size_type cur  = container->nextIndex(INVALID_IDX(typename Container::size_type));
					while (cur != INVALID_IDX(typename Container::size_type)) {
						last = cur;
						cur  = container->nextIndex(cur);
					}
					if (last != INVALID_IDX(typename Container::size_type)) {
						return Iterator(container, last);
					}
				}
				return Iterator(container);
			}

			// Returns the reverse-end iterator (same INVALID_IDX sentinel as end()).
			// Decrementing rbegin() past the first element should produce rend().
			template<typename Container>
			static Iterator<Container> rend(Container* container)
			{
				return Iterator(container);
			}

			// Returns an iterator positioned at raw slot `index`.
			// Returns the end iterator if `container` is null or `index` >= size().
			// `index` – must be a valid occupied slot; no occupancy check is performed.
			template<typename Container>
			static Iterator<Container> at(Container* container, typename Container::size_type index)
			{
				if (container) {
					if (index < container->size()) {
						return Iterator(container, index);
					}
				}
				return Iterator(container);
			}
		}

		// ─────────────────────────────────────────────────────────────────────────────
		// BASIC_ITERATOR_FUNCTIONS(type)
		//
		// Injects begin/end/rbegin/rend/cbegin/cend member functions into a container
		// class. Delegate to the basic::factory free functions above.
		// Requires `iterator` and `const_iterator` type aliases in the surrounding class.
		// ─────────────────────────────────────────────────────────────────────────────

		#define BASIC_ITERATOR_FUNCTIONS(type) \
		iterator begin() \
		{ \
			if(this->size() == 0){ \
				return factory::end(this); \
			} \
			return factory::begin(this); \
		} \
		const_iterator begin() const \
		{ \
			if(this->size() == 0){ \
				return factory::end(const_cast<type*>(this)); \
			} \
			return factory::begin(const_cast<type*>(this)); \
		} \
		const_iterator cbegin() const \
		{ \
			return begin(); \
		} \
		iterator rbegin() \
		{ \
			if(this->size() == 0){ \
				return factory::rend(this); \
			} \
			return factory::rbegin(this); \
		} \
		const_iterator rbegin() const \
		{ \
			if(this->size() == 0){ \
				return factory::rend(const_cast<type*>(this)); \
			} \
			return factory::rbegin(const_cast<type*>(this)); \
		} \
		iterator end() \
		{ \
			return factory::end(this); \
		} \
		const_iterator end() const \
		{ \
			return factory::end(const_cast<type*>(this)); \
		} \
		const_iterator cend() const \
		{ \
			return end(); \
		} \
		iterator rend() \
		{ \
			return end(); \
		} \
		const_iterator rend() const \
		{ \
			return factory::rend(const_cast<type*>(this)); \
		}
	}

	// ─────────────────────────────────────────────────────────────────────────────
	// threadSafe::Iterator<Container>
	//
	// A thread-safe iterator that holds a std::shared_lock on the container's
	// mutex for its ENTIRE lifetime — acquired on construction, released on
	// destruction.
	//
	// WHY THE LOCK IS HELD CONTINUOUSLY
	//   basic::Iterator calls nextIndex() and ref() as two separate container
	//   operations. Another thread can mutate the container in the gap between
	//   them, making the stored index stale. threadSafe::Iterator holds one
	//   shared_lock throughout, preventing any writer from acquiring a unique_lock
	//   until this iterator is destroyed.
	//
	// MANUAL UNLOCK / LOCK FOR CONTAINER MUTATIONS
	//   If you need to call a mutating method on the container from the same thread
	//   while this iterator is alive (e.g. erase the current element), call
	//   unlock() first, perform the mutation, then call lock() to restore the
	//   iterator's protected state. Never dereference or advance the iterator
	//   while it is unlocked — all element-access and traversal methods assert
	//   mLocked == true.
	//
	// EXAMPLE
	//   {
	//       auto it = arr.begin();         // shared_lock acquired
	//       for (; !it.isEnd(); ++it) {
	//           if (*it == target) {
	//               it.unlock();
	//               arr.erase(it.index()); // safe — lock released before mutation
	//               it.lock();
	//               break;
	//           }
	//       }
	//   }  // shared_lock released here
	//
	// COPY / MOVE
	//   Copy is deleted — shared_lock ownership cannot be shared.
	//   Move transfers the lock to the destination; the source is left unlocked.
	//
	// POST-INCREMENT
	//   operator++(int) is deleted. The lock cannot be copied, so returning a
	//   by-value pre-advance snapshot is not possible. Use pre-increment (++it).
	// ─────────────────────────────────────────────────────────────────────────────
	namespace threadSafe {
		
		template<typename Container, typename valueType = typename Container::value_type, typename sizeType  = typename Container::size_type>
		class Iterator {

		private:

			Container* mContainer = nullptr;
			sizeType mCurrentIndex = INVALID_IDX(sizeType);
			std::shared_lock<std::shared_mutex> mLock;
			bool mLocked = true;

		public:

			// ── Constructors / destructor ─────────────────────────────────────────

			// End-iterator constructor.
			// Acquires a shared lock on `mutex` immediately. Sets position to INVALID_IDX.
			// `container` – the container to lock. Must not be null and must outlive this iterator.
			// `mutex`     – the container's shared_mutex.
			explicit Iterator(Container* container, std::shared_mutex& mutex) : mContainer(container), mLock(mutex) {}

			// Positioned constructor.
			// Acquires a shared lock and positions the iterator at `index`.
			// `container` – the container to lock.
			// `index`     – the starting slot. Must be a valid occupied slot.
			// `mutex`     – the container's shared_mutex.
			Iterator(Container* container, const sizeType& index, std::shared_mutex& mutex) : mContainer(container), mCurrentIndex(index), mLock(mutex) {}

			// Destructor. Releases the shared lock. Any writer blocked on a unique_lock
			// for this container may proceed after this point.
			~Iterator() = default;

			// ── Move semantics ────────────────────────────────────────────────────

			// Move constructor. Transfers lock ownership and position from `other`.
			// `other` is left with a released lock and INVALID_IDX as its index.
			Iterator(Iterator&& other) noexcept : mContainer(other.mContainer) , mCurrentIndex(other.mCurrentIndex) , mLock(std::move(other.mLock)) , mLocked(other.mLocked)
			{
				other.mContainer    = nullptr;
				other.mCurrentIndex = INVALID_IDX(sizeType);
				other.mLocked       = false;
			}

			// Move assignment. Releases this lock, then steals `other`'s lock, position,
			// and mLocked state. Returns *this.
			Iterator& operator=(Iterator&& other) noexcept
			{
				if (this == &other) return *this;
				this->mLock = std::move(other.mLock);
				this->mLocked = other.mLocked;
				this->mContainer = other.mContainer;
				this->mCurrentIndex = other.mCurrentIndex;
				other.mContainer = nullptr;
				other.mCurrentIndex = INVALID_IDX(sizeType);
				other.mLocked = false;
				return *this;
			}

			// Copy is deleted — shared_lock ownership cannot be shared.
			Iterator(const Iterator&) = delete;
			Iterator& operator=(const Iterator&) = delete;

			// Post-increment is deleted — returning a by-value copy requires copying
			// the lock, which is not possible. Use pre-increment (++it) in all loops.
			Iterator operator++(int) = delete;

			// ── Traversal ─────────────────────────────────────────────────────────

			// Pre-increment. Advances to the next valid element.
			// Asserts if the iterator is not currently locked.
			// Returns *this after advancing.
			Iterator& operator++()
			{
				if(this->mLocked == false) {
					idxASSERT(false, "the iterator is not LOCKED!!");
					return *this;
				}
				if (this->mContainer) {
					this->mCurrentIndex = this->mContainer->nextIndex(this->mCurrentIndex);
				}
				return *this;
			}

			// Pre-decrement. Moves to the previous valid slot.
			// Asserts if the iterator is not currently locked.
			// Returns *this after decrementing.
			Iterator& operator--()
			{
				if(this->mLocked == false) {
					idxASSERT(false, "the iterator is not LOCKED!!");
					return *this;
				}
				if (this->mContainer) {
					this->mCurrentIndex = mContainer->prevIndex(this->mCurrentIndex);
				}
				return *this;
			}

			// ── Element access ────────────────────────────────────────────────────

			// Dereferences the iterator. Asserts if the iterator is not locked.
			// The shared lock must be held to guarantee the element is still valid.
			// Behaviour is undefined if isEnd() == true.
			// Returns a mutable reference to the element at mCurrentIndex.
			valueType& operator*()
			{
				idxASSERT(this->mLocked == true, "the iterator is not LOCKED!!");
				return this->mContainer->ref(this->mCurrentIndex);
			}

			// Arrow operator. Asserts if not locked, returns nullptr if unlocked.
			// Returns a pointer to the current element.
			valueType* operator->()
			{
				if(this->mLocked == false) {
					idxASSERT(false, "the iterator is not LOCKED!!");
					return nullptr;
				}
				return &(**this);
			}

			// ── Comparison ────────────────────────────────────────────────────────

			// Returns true if both iterators point to the same container, the same index
			bool operator==(const Iterator& other) const
			{
				idxASSERT(this->mLocked == true, "the iterator is not LOCKED!!");
				return this->mContainer == other.mContainer && this->mCurrentIndex == other.mCurrentIndex;
			}

			// Returns true if the two iterators differ in container, index, or lock state.
			bool operator!=(const Iterator& other) const
			{
				return !(*this == other);
			}

			// ── Lock control ──────────────────────────────────────────────────────

			// Releases the shared lock and sets mLocked = false.
			// Call this before performing any mutating operation on the container from
			// the same thread. Re-acquire with lock() before using the iterator again.
			// WARNING: while unlocked, a writer may modify the container. The iterator's
			// stored index may become stale. Use with care.
			void unlock()
			{
				this->mLock.unlock();
				this->mLocked = false;
			}

			// Re-acquires the shared lock and sets mLocked = true.
			// Must be called after unlock() before using operator*, operator++, or
			// any other element-access method.
			void lock()
			{
				this->mLock.lock();
				this->mLocked = true;
			}

			// ── State queries ─────────────────────────────────────────────────────

			// Returns true when mCurrentIndex equals INVALID_IDX (iterator is at end).
			// Asserts if the iterator is not currently locked.
			bool isEnd() const
			{
				if(this->mLocked == false) {
					idxASSERT(false, "the iterator is not LOCKED!!");
					return false;
				}
				return this->mCurrentIndex == INVALID_IDX(sizeType);
			}

			// Named alternative to operator*(). Returns a mutable reference to the
			// current element. Asserts if not locked.
			valueType& data()
			{
				idxASSERT(this->mLocked == true, "the iterator is not LOCKED!!");
				return this->mContainer->ref(this->mCurrentIndex);
			}

			// Const overload of data(). Asserts if not locked.
			const valueType& data() const
			{
				idxASSERT(this->mLocked == true, "the iterator is not LOCKED!!");
				return this->mContainer->ref(this->mCurrentIndex);
			}

			// Returns the key at the current position for key-value containers.
			// `key_type` must be specified explicitly: it.key<MyKeyType>()
			// Asserts if not locked.
			template<typename key_type>
			const key_type& key() const
			{
				idxASSERT(this->mLocked == true, "the iterator is not LOCKED!!");
				return this->mContainer->key(this->mCurrentIndex);
			}

			// Returns a mutable reference to the value at the current position.
			// For key-value containers (e.g. HashMap), this is the value half.
			// Asserts if not locked.
			valueType& value()
			{
				idxASSERT(this->mLocked == true, "the iterator is not LOCKED!!");
				return this->mContainer->value(this->mCurrentIndex);
			}

			// Const overload of value(). Asserts if not locked.
			const valueType& value() const
			{
				idxASSERT(this->mLocked == true, "the iterator is not LOCKED!!");
				return this->mContainer->value(this->mCurrentIndex);
			}

			// Returns the raw slot index, or INVALID_IDX(sizeType) when at end.
			// Asserts if not locked.
			sizeType index() const 
			{ 
				if(this->mLocked == false) {
					idxASSERT(false, "the iterator is not LOCKED!!");
					return INVALID_IDX(sizeType);
				}
				return this->mCurrentIndex;
			}
		};
			
		// ─────────────────────────────────────────────────────────────────────────────
		// threadSafe::factory
		//
		// Free functions that construct threadSafe::Iterator instances. All factory
		// functions take a reference to the container's shared_mutex so the iterator
		// can acquire it immediately on construction.
		//
		// begin() uses the same nextIndex(INVALID_IDX) trick as basic::factory::begin()
		// to correctly find the first live slot in sparse containers. It acquires a
		// temporary shared_lock to call nextIndex() safely, releases it, then
		// constructs the iterator which acquires its own permanent shared_lock.
		//
		// rbegin() scans forward with nextIndex to locate the last live slot (O(n)).
		// This scan runs without a lock — the iterator constructor acquires the lock
		// for the lifetime of the iterator. There is a brief TOCTOU window between
		// the scan and the constructor, but mutating a container while constructing
		// an iterator on it is already undefined behaviour.
		// ─────────────────────────────────────────────────────────────────────────────
		namespace factory {

			// Returns a threadSafe::Iterator at the first LIVE element.
			// Temporarily acquires a shared_lock to call nextIndex(INVALID_IDX), then
			// constructs the iterator which acquires its own persistent shared_lock.
			// Returns the end iterator if `container` is null or empty.
			// `mutex` – the container's shared_mutex (same one the iterator will hold).
			// `empty` – the result of container->empty() checked by the caller; used to
			//           skip the nextIndex scan and return early for empty containers.
			template<typename Container>
			static Iterator<Container> begin(Container* container, std::shared_mutex& mutex, bool empty)
			{
				if (container && empty == false) {
					typename Container::size_type first;
					{
						std::shared_lock<std::shared_mutex> tmp(mutex);
						first = container->nextIndex(INVALID_IDX(typename Container::size_type));
					}
					if (first != INVALID_IDX(typename Container::size_type)) {
						return Iterator(container, first, mutex);
					}
				}
				return Iterator(container, mutex);
			}

			// Returns the end iterator (position = INVALID_IDX) with the shared lock held.
			// `mutex` – the container's shared_mutex.
			template<typename Container>
			static Iterator<Container> end(Container* container, std::shared_mutex& mutex)
			{
				return Iterator(container, mutex);
			}

			// Returns a threadSafe::Iterator at the last LIVE element.
			// Scans forward with nextIndex (without holding the lock) to find the last
			// occupied slot — O(n) but correct for both dense and sparse containers.
			// The iterator constructor then acquires the persistent shared_lock.
			// Returns the end iterator if `container` is null or empty.
			// `mutex` – the container's shared_mutex.
			template<typename Container>
			static Iterator<Container> rbegin(Container* container, std::shared_mutex& mutex)
			{
				if (container && container->empty() == false) {
					typename Container::size_type last = INVALID_IDX(typename Container::size_type);
					typename Container::size_type cur  = container->nextIndex(INVALID_IDX(typename Container::size_type));
					while (cur != INVALID_IDX(typename Container::size_type)) {
						last = cur;
						cur  = container->nextIndex(cur);
					}
					if (last != INVALID_IDX(typename Container::size_type)) {
						return Iterator(container, last, mutex);
					}
				}
				return Iterator(container, mutex);
			}

			// Returns the reverse-end iterator (same INVALID_IDX sentinel as end()).
			// `mutex` – the container's shared_mutex.
			template<typename Container>
			static Iterator<Container> rend(Container* container, std::shared_mutex& mutex)
			{
				return Iterator(container, mutex);
			}

			// Returns a threadSafe::Iterator at raw slot `index`.
			// Returns the end iterator if `container` is null or `index` >= size().
			// `mutex` – the container's shared_mutex.
			// `index` – must be a valid occupied slot; no occupancy check is performed.
			template<typename Container>
			static Iterator<Container> at(Container* container, std::shared_mutex& mutex, typename Container::size_type index)
			{
				if (container) {
					if (index < container->size()) {
						return Iterator(container, index, mutex);
					}
				}
				return Iterator(container, mutex);
			}
		}
	
		// ─────────────────────────────────────────────────────────────────────────────
		// THREAD_SAFE_ITERATOR_FUNCTIONS(type)
		//
		// Injects begin/end/rbegin/rend/cbegin/cend member functions into a threadSafe
		// container class. Delegates to threadSafe::factory free functions, passing
		// `this->mMutex` so the iterator can lock it on construction.
		// Requires `iterator`, `const_iterator`, and `mMutex` in the surrounding class.
		// ─────────────────────────────────────────────────────────────────────────────

		#define THREAD_SAFE_ITERATOR_FUNCTIONS(type) \
		iterator begin() \
		{ \
			if(this->size() == 0){ \
				return factory::end(this, this->mMutex); \
			} \
			return factory::begin(this, this->mMutex, this->empty()); \
		} \
		const_iterator begin() const \
		{ \
			if(this->size() == 0){ \
				return factory::end(const_cast<type*>(this), this->mMutex); \
			} \
			return factory::begin(const_cast<type*>(this), this->mMutex, this->empty()); \
		} \
		const_iterator cbegin() const \
		{ \
			return begin(); \
		} \
		iterator rbegin() \
		{ \
			if(this->size() == 0){ \
				return factory::rend(this, this->mMutex); \
			} \
			return factory::rbegin(this, this->mMutex); \
		} \
		const_iterator rbegin() const \
		{ \
			if(this->size() == 0){ \
				return factory::rend(const_cast<type*>(this), this->mMutex); \
			} \
			return factory::rbegin(const_cast<type*>(this), this->mMutex); \
		} \
		iterator end() \
		{ \
			return factory::end(this, this->mMutex); \
		} \
		const_iterator end() const \
		{ \
			return factory::end(const_cast<type*>(this), this->mMutex); \
		} \
		const_iterator cend() const \
		{ \
			return end(); \
		} \
		iterator rend() \
		{ \
			return end(); \
		} \
		const_iterator rend() const \
		{ \
			return factory::rend(const_cast<type*>(this), this->mMutex); \
		}
	}
}
