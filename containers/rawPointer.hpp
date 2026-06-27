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

#include"core/types.hpp"
#include"core/assert.hpp"

#include<utility>
#include<cstring>

namespace idx {

// ─────────────────────────────────────────────────────────────────────────────
// RawPointer<T>
//
// A thin RAII-flavoured wrapper around a heap-allocated T*.
// It is NOT a smart pointer — it does not free memory automatically on
// destruction. Instead the destructor ASSERTS that the pointer is already null,
// catching leaks in debug builds.
//
// The intent is that every allocation has a matching manual call to
// deallocate() (or clear()) before the wrapper goes out of scope.
// Higher-level containers (MemoryBuffer, String) use RawPointer internally
// and handle the lifetime automatically.
// ─────────────────────────────────────────────────────────────────────────────
template<typename T>
struct RawPointer {

    T* pointer = nullptr;

	// Default constructor. Sets pointer to nullptr (no allocation).
    RawPointer(){}

	// Destructor — asserts that pointer is null.
	// If this fires it means you forgot to call clear() / deallocate()
	// before the RawPointer went out of scope (memory leak).
	~RawPointer()
	{
		idxASSERT(this->pointer == nullptr, "FATAL!!! raw pointer not released");
	}

    // Copy and move are deleted — ownership must be transferred explicitly
    // via hijack() or swap() to prevent double-free and aliased ownership.
    RawPointer(const RawPointer&) = delete;
    RawPointer(RawPointer&&) = delete;
    RawPointer& operator=(const RawPointer&) = delete;
    RawPointer& operator=(RawPointer&&) = delete;

	// Exchanges the raw pointers of this and `other` without any allocation.
	// `other` – the RawPointer to swap with. No-op if this == &other.
	void swap(RawPointer& other)
	{
		if (this == &other) return;
		std::swap(this->pointer, other.pointer);
	}

    // Allocates `sizeInBytes` bytes through the idx Allocator and stores
    // the result in `pointer`.
    // `sizeInBytes` – number of bytes to allocate (must be > 0).
    // Precondition: pointer must be null (calling allocate on a live pointer
    //               would silently overwrite it and leak the old allocation).
    void allocate(const uint64& sizeInBytes)
    {
		this->pointer = new T[(sizeInBytes / sizeof(T))];
    }

    // Releases `sizeInBytes` bytes back to the idx Allocator.
    // Does NOT null the pointer — call clear() if you need the null-reset too.
    // `sizeInBytes` – must match the size passed to the original allocate() call.
    void deallocate(const uint64& sizeInBytes)
    {
		delete this->pointer;
    }

	// Steals ownership of `other`'s pointer.
	// After the call this->pointer holds other's old pointer, and other.pointer is null.
	// `other` – the pointer to steal from. No-op if this == &other.
	// Precondition: this->pointer must be null (asserted); otherwise the
	//               existing allocation would be leaked.
	void hijack(RawPointer& other)
	{
		if (this == &other) return;
		idxASSERT(this->pointer == nullptr, "FATAL!!! this pointer MUST be null");
		this->pointer = other.pointer;
		other.pointer = nullptr;
	}

	// Frees this pointer's current allocation (if any) and allocates fresh
	// storage of `sizeInBytes` bytes. Does NOT copy bytes from `other`.
	// `other`       – unused in the copy, only used for the self-check.
	// `sizeInBytes` – size of the new allocation in bytes.
	void clone(const RawPointer& other, const uint64& sizeInBytes)
	{
		if (this == &other) return;
        this->clear(sizeInBytes);
		if (sizeInBytes > 0) {
			this->allocate(sizeInBytes);
		}
	}

	// Frees this pointer's current allocation (if any), allocates fresh storage
	// of `sizeInBytes` bytes, and memcpy's `sizeInBytes` bytes from `other`.
	// `other`       – the source pointer to copy bytes from.
	// `sizeInBytes` – number of bytes to copy (must match the allocation size).
	void copy(const RawPointer& other, const uint64& sizeInBytes)
	{
		if (this == &other) return;
		this->clear(sizeInBytes);
		if (other.pointer != nullptr && sizeInBytes > 0) {
			this->allocate(sizeInBytes);
			memcpy(this->pointer, other.pointer, sizeInBytes);
		}
	}

    // Returns a reference to the T at `index` slots past the base pointer.
    // `index` – zero-based slot offset. No bounds checking is performed.
    T& ref(const uint64& index)
    {
        return this->pointer[index];
    }

    // Const overload of ref().
    // `index` – zero-based slot offset. No bounds checking is performed.
    const T& ref(const uint64& index) const
    {
        return this->pointer[index];
    }

    // Deallocates the memory and resets pointer to null.
    // Safe to call when pointer is already null (no-op).
    // `sizeInBytes` – must match the size passed to the original allocate() call.
    void clear(const uint64& sizeInBytes)
    {
		if(this->pointer == nullptr) return;
        this->deallocate(sizeInBytes);
        this->pointer = nullptr;
    }
};

}
