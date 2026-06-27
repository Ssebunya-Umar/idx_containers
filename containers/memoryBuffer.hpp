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

#include"rawPointer.hpp"

#include<new>

namespace idx {

// ─────────────────────────────────────────────────────────────────────────────
// MemoryBuffer<T, sizeType>
//
// A fixed-capacity block of raw (uninitialised) memory large enough to hold
// `capacity` objects of type T. It manages the lifetime of the allocation but
// does NOT construct or destroy T objects — that is left to the caller via
// placement-new and explicit destructor calls (see DynamicArray).
// ─────────────────────────────────────────────────────────────────────────────
template<typename T, typename sizeType>
class MemoryBuffer {

private:

	RawPointer<T> mPointer;
	sizeType mCapacity = 0;

private:

	// Takes ownership of `other`'s allocation. Clears *this first to avoid leaks.
	// `other` – the buffer to steal from. No-op if this == &other.
	// After the call: this holds other's memory; other is empty (capacity = 0).
	void hijack(MemoryBuffer<T, sizeType>& other)
	{
		if (this == &other) return;
		this->clear();
		this->mCapacity = other.mCapacity;
		this->mPointer.hijack(other.mPointer);
		other.mCapacity = 0;
	}

	// Allocates fresh storage matching `other`'s capacity.
	// Does NOT copy object bytes — the caller must construct objects separately.
	// `other` – the buffer whose capacity is mirrored. No-op if this == &other.
	void clone(const MemoryBuffer<T, sizeType>& other)
	{
		if (this == &other) return;
		this->clear();
		if (other.mCapacity > 0) {
			this->mCapacity = other.mCapacity;
			this->mPointer.clone(other.mPointer, sizeof(T) * this->mCapacity);
		}
	}

public:

	// Default constructor. Creates an empty buffer with no allocation.
	MemoryBuffer() {}

	// Destructor. Frees the underlying allocation.
	// NOTE: does NOT call destructors on any T objects that may be living in
	//       the buffer. The caller must destroy all live objects before the
	//       buffer goes out of scope.
	~MemoryBuffer()
	{
		this->clear();
	}

	// Capacity constructor. Allocates space for exactly `capacity` T-objects.
	// Objects are NOT constructed; the memory is raw.
	// `capacity` – number of T-sized slots to allocate. Must be >= 0.
	explicit MemoryBuffer(const sizeType& capacity)
	{
		idxASSERT(capacity >= 0, "MemoryBuffer capacity must not be negative");
        this->mCapacity = capacity;
		if(this->mCapacity > 0) {
			this->mPointer.allocate(sizeof(T) * this->mCapacity);
		}
	}

	// Data + capacity constructor. Allocates space and byte-copies `capacity`
	// T-objects from `data`. Assumes T is trivially copyable.
	// `data`     – pointer to the source data to copy.
	// `capacity` – number of T-objects to copy. Must be >= 0.
	explicit MemoryBuffer(const T* data, const sizeType& capacity)
	{
		idxASSERT(capacity >= 0, "MemoryBuffer capacity must not be negative");
        this->mCapacity = capacity;
		if (this->mCapacity > 0) {
			this->mPointer.allocate(sizeof(T) * this->mCapacity);
			memcpy(this->mPointer.pointer, data, sizeof(T) * this->mCapacity);
		}
	}

	// Copy constructor. Allocates new storage matching `other`'s capacity.
	// Note: does not copy live objects — see DynamicArray for that logic.
	MemoryBuffer(const MemoryBuffer<T, sizeType>& other)
	{
		clone(other);
	}

	// Move constructor. Steals `other`'s allocation without copying.
	MemoryBuffer(MemoryBuffer<T, sizeType>&& other) noexcept
	{
		hijack(other);
	}

	// Copy assignment. Frees this buffer and allocates a fresh one matching `other`.
	// Returns *this for chaining.
	MemoryBuffer<T, sizeType>& operator=(const MemoryBuffer<T, sizeType>& other)
	{
		clone(other);
		return *this;
	}

	// Move assignment. Steals `other`'s allocation. Returns *this for chaining.
	MemoryBuffer<T, sizeType>& operator=(MemoryBuffer<T, sizeType>&& other) noexcept
	{
		hijack(other);
		return *this;
	}

	// Swaps the allocations and capacities of two buffers in O(1) — no copying.
	// `other` – the buffer to swap with. No-op if this == &other.
	void swap(MemoryBuffer<T, sizeType>& other)
	{
		if (this == &other) return;
		this->mPointer.swap(other.mPointer);
		std::swap(this->mCapacity, other.mCapacity);
	}

	// Bounds-checked slot access. Asserts in debug builds if the buffer is empty
	// or `index` is out of range.
	// `index`   – zero-based slot index. Must be < capacity.
	// Returns a reference to the T-sized slot at `index` (object may not be constructed).
	T& operator[](const sizeType& index)
	{
		idxASSERT((this->mCapacity > 0) && (index < this->mCapacity), "buffer is empty or index is out of range");
		return this->mPointer.ref(index);
	}

	// Const overload of operator[].
	// `index` – zero-based slot index. Must be < capacity.
	const T& operator[](const sizeType& index) const
	{
		idxASSERT((this->mCapacity > 0) && (index < this->mCapacity), "buffer is empty or index is out of range");
		return this->mPointer.ref(index);
	}

	// Linear search over ALL capacity slots (not just live objects).
	// `data`   – the value to search for (compared with operator==).
	// Returns a pointer to the first matching slot, or nullptr if not found.
	T* find(const T& data)
	{
		for (sizeType x = 0; x < this->mCapacity; ++x) {
			if (this->mPointer.ref(x) == data) {
				return &this->mPointer.ref(x);
			}
		}
		return nullptr;
	}

	// Const overload of find().
	// `data`   – the value to search for.
	// Returns a const pointer to the first matching slot, or nullptr.
	const T* find(const T& data) const
	{
		for (sizeType x = 0; x < this->mCapacity; ++x) {
			if (this->mPointer.ref(x) == data) {
				return &this->mPointer.ref(x);
			}
		}
		return nullptr;
	}

	// Returns a raw pointer to the start of the allocation.
	// Useful for passing to C APIs or memcpy.
	// Returns nullptr if the buffer has no allocation.
	T* data()
	{
		return (T*)this->mPointer.pointer;
	}

	// Const overload of data().
	const T* data() const
	{
		return (T*)this->mPointer.pointer;
	}

	// Returns the number of T-sized slots in this buffer.
	// This is the allocated capacity, NOT the number of live objects.
	sizeType capacity() const
	{
		return this->mCapacity;
	}

	// Returns a new MemoryBuffer with a fresh allocation containing a byte-copy
	// of all slots in this buffer.
	// Fast, but only safe for trivially copyable T.
	// Returns an independent buffer with the same capacity and raw bytes.
	MemoryBuffer<T, sizeType> shallowCopy() const
	{
		MemoryBuffer<T, sizeType> result;
		if (this->mCapacity > 0) {
			result.mCapacity = this->mCapacity;
			result.mPointer.allocate(sizeof(T) * result.mCapacity);
			memcpy(result.mPointer.pointer, this->mPointer.pointer, sizeof(T) * result.mCapacity);
		}
		return result;
	}

	// Returns a new MemoryBuffer where each element is move-constructed from
	// the corresponding element in this buffer.
	// Correct for types with non-trivial move constructors.
	// WARNING: source objects are left in a valid-but-unspecified state after the move.
	// Returns an independent buffer of the same capacity with moved-into objects.
	MemoryBuffer<T, sizeType> deepCopy() const
	{
		MemoryBuffer<T, sizeType> result;
		if (this->mCapacity > 0) {
			result.mCapacity = this->mCapacity;
			result.mPointer.allocate(sizeof(T) * result.mCapacity);
			for(sizeType x = 0; x < this->mCapacity; ++x) {
				new (&result.mPointer.ref(x)) T(std::move(this->mPointer.ref(x)));
			}
		}
		return result;
	}

	// Frees the allocation and resets capacity to 0.
	// Does NOT call T destructors — destroy all live objects before calling this.
	void clear()
	{
		this->mPointer.clear(sizeof(T) * this->mCapacity);
		this->mCapacity = 0;
	}
};

}
