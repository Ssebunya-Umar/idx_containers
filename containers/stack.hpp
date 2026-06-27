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
// Stack<T, sizeType>  (basic:: and threadSafe::)
// A LIFO stack backed by a DynamicArray.
// ─────────────────────────────────────────────────────────────────────────────

#include"dynamicArray.hpp"

namespace idx {

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	namespace basic {

		template<typename T, typename sizeType>
		class Stack {

		public:

			using value_type = T;
			using size_type = sizeType;
			using iterator = Iterator<Stack<T, sizeType>>;
			using const_iterator = const iterator;

		private:

			DynamicArray<T, sizeType> mData;

		public:

			// Pushes a copy of `data` onto the top. Amortised O(1).
			// `data` – the value to add.
			void push(const T& data)
			{
				this->mData.pushBack(data);
			}

			// Removes and destroys the top element. O(1). Asserts if empty.
			void pop()
			{
				this->mData.popBack();
			}

			// O(n) search. `data` – value to find. Returns pointer or nullptr.
			T* find(const T& data)
			{
				return this->mData.find(data);
			}

			// Returns a mutable reference to the top element. Asserts if empty.
			T& top()
			{
				return this->mData.back();
			}

			// Const overload of top().
			const T& top() const
			{
				return this->mData.back();
			}

			// Direct indexed access. `index` – zero-based (0 = bottom). Must be < size().
			T& operator[](const sizeType& index)
			{
				return this->mData[index];
			}

			// Const overload of operator[].
			const T& operator[](const sizeType& index) const
			{
				return this->mData[index];
			}

			// Returns the number of elements on the stack.
			sizeType size() const
			{
				return this->mData.size();
			}

			// Returns true if the stack has no elements.
			bool empty() const
			{
				return this->mData.empty();
			}

			// Trims internal buffer to size(). Reclaims unused capacity.
			void wrap()
			{
				this->mData.wrap();
			}

			// Resets to empty, keeping buffer. `callDestructors` – invoke ~T() if true.
			void shallowClear(bool callDestructors)
			{
				this->mData.shallowClear(callDestructors);
			}

			// Destroys all elements and frees the internal buffer.
			void deepClear()
			{
				this->mData.deepClear();
			}

			// REQUIRED by ITERATOR
			// Returns the next valid index after `index`, or INVALID_IDX at the end.
			// `index` – the current position (0 = bottom of stack).
			sizeType nextIndex(const sizeType& index)
			{
				return this->mData.nextIndex(index);
			}

			// REQUIRED by ITERATOR
			// Returns the previous valid index before `index`, or INVALID_IDX at 0.
			// `index` – the current position (0 = bottom of stack).
			sizeType prevIndex(const sizeType& index)
			{
				return this->mData.prevIndex(index);
			}

			// REQUIRED by ITERATOR
			// Returns a reference to the element at `index` — required by Iterator.
			T& ref(const sizeType& index) { return this->mData[index]; }

			// REQUIRED by ITERATOR
			// Const overload of ref().
			const T& ref(const sizeType& index) const { return this->mData[index]; }

			#define type Stack<T, sizeType>
			BASIC_ITERATOR_FUNCTIONS(type)
			#undef type
		};
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	namespace threadSafe {

		template<typename T, typename sizeType>
		class Stack {

		public:

			using value_type = T;
			using size_type = sizeType;
			using iterator = Iterator<Stack<T, sizeType>>;
			using const_iterator = const iterator;

		private:

			basic::DynamicArray<T, sizeType> mData;
			mutable std::shared_mutex mMutex;

		public:

			// Pushes a copy of `data` onto the top of the stack. Amortised O(1).
			// `data` – the value to add to the top.
			void push(const T& data)
			{
				std::unique_lock lock(this->mMutex);
				this->mData.pushBack(data);
			}

			// Removes and destroys the top element. O(1).
			// Asserts if the stack is empty.
			void pop()
			{
				std::unique_lock lock(this->mMutex);
				this->mData.popBack();
			}

			// Linear O(n) search through all elements.
			// `data` – the value to look for.
			// Returns a pointer to the first match, or nullptr if not found.
			T* find(const T& data)
			{
				std::shared_lock lock(this->mMutex);
				return this->mData.find(data);
			}

			// Returns a mutable reference to the top element without removing it.
			// Asserts if the stack is empty.
			T& top()
			{
				std::shared_lock lock(this->mMutex);
				return this->mData.back();
			}

			// Const overload of top().
			const T& top() const
			{
				std::shared_lock lock(this->mMutex);
				return this->mData.back();
			}

			// Provides direct indexed access to any element (not just the top).
			// `index` – zero-based position (0 = bottom). Must be < size().
			T& operator[](const sizeType& index)
			{
				std::shared_lock lock(this->mMutex);
				return this->mData[index];
			}

			// Const overload of operator[].
			const T& operator[](const sizeType& index) const
			{
				std::shared_lock lock(this->mMutex);
				return this->mData[index];
			}

			// Returns the number of elements currently on the stack.
			sizeType size() const
			{
				std::shared_lock lock(this->mMutex);
				return this->mData.size();
			}

			// Returns true when the stack has no elements.
			bool empty() const
			{
				std::shared_lock lock(this->mMutex);
				return this->mData.empty();
			}

			// Trims the internal buffer capacity to exactly size().
			// Use after a period of heavy push/pop to reclaim unused memory.
			void wrap()
			{
				std::unique_lock lock(this->mMutex);
				this->mData.wrap();
			}

			// Resets the stack to empty, keeping the internal buffer allocated.
			// `callDestructors` – if true, ~T() is called on every element before reset.
			void shallowClear(bool callDestructors)
			{
				std::unique_lock lock(this->mMutex);
				this->mData.shallowClear(callDestructors);
			}

			// Destroys all elements and frees the internal buffer.
			void deepClear()
			{
				std::unique_lock lock(this->mMutex);
				this->mData.deepClear();
			}

			// REQUIRED by ITERATOR
			// Returns the next valid index after `index`, or INVALID_IDX at the end.
			// `index` – the current position (0 = bottom of stack).
			sizeType nextIndex(const sizeType& index)
			{
				return this->mData.nextIndex(index);
			}

			// REQUIRED by ITERATOR
			// Returns the previous valid index before `index`, or INVALID_IDX at 0.
			// `index` – the current position (0 = bottom of stack).
			sizeType prevIndex(const sizeType& index)
			{
				return this->mData.prevIndex(index);
			}

			// REQUIRED by ITERATOR
			// Returns a reference to the element at `index` — required by Iterator.
			T& ref(const sizeType& index) { return this->mData[index]; }

			// REQUIRED by ITERATOR
			// Const overload of ref().
			const T& ref(const sizeType& index) const { return this->mData[index]; }

			#define type Stack<T, sizeType>
			THREAD_SAFE_ITERATOR_FUNCTIONS(type)
			#undef type
		};
	}
}
