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
// BitMap<T, sizeType, bitLength>
//
// A compact boolean array where each element takes only one bit.
// Backed by a basic::DynamicArray<T, sizeType>
// Bit i lives at mData[i / bitLength] at bit position (i % bitLength).
// ─────────────────────────────────────────────────────────────────────────────

#include"dynamicArray.hpp"

#include<shared_mutex>
#include<mutex>

namespace idx {

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename T, typename sizeType, uint64 bitLength = sizeof(T) * 8>
class BitMapBase {

protected:

	basic::DynamicArray<T, sizeType> mData;

protected:

	// Returns the bitmask for logical bit `index` within its word.
	// `index` – the logical bit index.
	// Returns a uint64 with only the relevant bit set (1ULL << (index % bitLength)).
	uint64 bitValue(const sizeType& index) const
	{
		return 1ULL << (index % bitLength);
	}

	// Returns a mutable reference to the word that contains logical bit `index`.
	// If the word does not yet exist, the underlying array is resized to include it.
	// `index` – the logical bit index.
	// Returns a reference to the T word at position (index / bitLength).
	T& dataRefInternal(const sizeType& index)
	{
		if ((this->mData.size() * bitLength) <= index) {
			this->mData.resize((index / bitLength) + 1);
		}
		return this->mData[(uint64)(index / bitLength)];
	}

	// Sets bit `index` to 1. Grows the backing array if needed.
	// `index` – the logical bit to turn on.
	void toggleOnInternal(const sizeType& index)
	{
		this->dataRefInternal(index) |= (T)this->bitValue(index);
	}

	// Clears bit `index` to 0. Grows the backing array if needed.
	// `index` – the logical bit to turn off.
	void toggleOffInternal(const sizeType& index)
	{
		this->dataRefInternal(index) &= (T)(~this->bitValue(index));
	}

	BitMapBase() {}
	~BitMapBase() {}
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
namespace basic {

	template<typename T, typename sizeType, uint64 bitLength = sizeof(T) * 8>
	class BitMap : public BitMapBase<T, sizeType> {

	public:

		// Default constructor. Creates an empty bitfield with no allocation.
		BitMap() : BitMapBase<T, sizeType>() {}

		// Copy constructor. Deep-copies the backing array from `other`.
		BitMap(const BitMap<T, sizeType, bitLength>& other) : BitMapBase<T, sizeType>()
		{
			this->mData = other.mData;
		}

		// Move constructor. Steals the backing array from `other`.
		BitMap(BitMap<T, sizeType, bitLength>&& other) noexcept : BitMapBase<T, sizeType>()
		{
			this->mData = std::move(other.mData);
		}

		// Copy assignment. Replaces this bitfield's data with a copy of `other`'s.
		// Returns *this.
		BitMap<T, sizeType, bitLength>& operator=(const BitMap<T, sizeType, bitLength>& other)
		{
			if (this == &other) return *this;
			this->mData = other.mData;
			return *this;
		}

		// Move assignment. Steals `other`'s backing array.
		// Returns *this.
		BitMap<T, sizeType, bitLength>& operator=(BitMap<T, sizeType, bitLength>&& other) noexcept
		{
			if (this == &other) return *this;
			this->mData = std::move(other.mData);
			return *this;
		}

		// Swaps the backing arrays of two BitMaps in O(1).
		// `other` – the BitMap to swap with. No-op if this == &other.
		void swap(BitMap<T, sizeType, bitLength>& other)
		{
			if (this == &other) return;
			this->mData.swap(other.mData);
		}

		// Sets logical bit `index` to 1. Grows the backing array automatically.
		// `index` – the zero-based bit position to turn on.
		void toggleOn(const sizeType& index)
		{
			this->toggleOnInternal(index);
		}

		// Clears logical bit `index` to 0. Grows the backing array automatically.
		// `index` – the zero-based bit position to turn off.
		void toggleOff(const sizeType& index)
		{
			this->toggleOffInternal(index);
		}

		// Reads logical bit `index`.
		// `index` – the zero-based bit position to read.
		// Returns true if the bit is set; returns false (without asserting) if
		// `index` is beyond the currently allocated range.
		bool operator[](const sizeType& index) const
		{
			if ((this->mData.size() * bitLength) <= index) {
				return false;
			}
			return (this->mData[(uint64)(index / bitLength)] & (T)this->bitValue(index)) != 0;
		}

		// Zeroes all bits in every word, keeping the backing array allocated.
		// Use this to reuse the bitfield without releasing memory.
		void reset()
		{
			this->mData.shallowClear(true);
		}

		// Zeroes all bits AND frees the backing array.
		// After this call the bitfield is fully empty (no allocation).
		void clear()
		{
			this->mData.deepClear();
		}
	};
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
namespace threadSafe {

	template<typename T, typename sizeType, uint64 bitLength = sizeof(T) * 8>
	class BitMap : public BitMapBase<T, sizeType> {

	private:

		mutable std::shared_mutex mMutex;

	public:

		// Default constructor. Creates an empty bitfield with no allocation.
		BitMap() : BitMapBase<T, sizeType>() {}

		// Copy constructor. Acquires a shared lock on `other` while copying.
		BitMap(const BitMap<T, sizeType, bitLength>& other) : BitMapBase<T, sizeType>()
		{
			std::shared_lock otherLock(other.mMutex);
			this->mData = other.mData;
		}

		// Move constructor. Acquires a unique lock on `other` while stealing its array.
		BitMap(BitMap<T, sizeType, bitLength>&& other) noexcept : BitMapBase<T, sizeType>()
		{
			std::unique_lock otherLock(other.mMutex);
			this->mData = std::move(other.mData);
		}

		// Copy assignment. Locks both under scoped_lock to prevent deadlock.
		// Returns *this.
		BitMap<T, sizeType, bitLength>& operator=(const BitMap<T, sizeType, bitLength>& other)
		{
			if (this == &other) return *this;
			std::scoped_lock lock(this->mMutex, other.mMutex);
			this->mData = other.mData;
			return *this;
		}

		// Move assignment. Locks both under scoped_lock, then steals other's array.
		// Returns *this.
		BitMap<T, sizeType, bitLength>& operator=(BitMap<T, sizeType, bitLength>&& other) noexcept
		{
			if (this == &other) return *this;
			std::scoped_lock lock(this->mMutex, other.mMutex);
			this->mData = std::move(other.mData);
			return *this;
		}

		// Thread-safe swap. Locks both under scoped_lock.
		// `other` – the BitMap to swap with.
		void swap(BitMap<T, sizeType, bitLength>& other)
		{
			if (this == &other) return;
			std::scoped_lock lock(this->mMutex, other.mMutex);
			this->mData.swap(other.mData);
		}

		// Thread-safe toggleOn. Acquires a unique lock.
		// `index` – the zero-based bit position to set to 1.
		void toggleOn(const sizeType& index)
		{
			std::unique_lock lock(this->mMutex);
			this->toggleOnInternal(index);
		}

		// Thread-safe toggleOff. Acquires a unique lock.
		// `index` – the zero-based bit position to clear to 0.
		void toggleOff(const sizeType& index)
		{
			std::unique_lock lock(this->mMutex);
			this->toggleOffInternal(index);
		}

		// Thread-safe bit read. Acquires a shared lock.
		// `index` – the zero-based bit position to read.
		// Returns true if the bit is set; false if clear or out of range.
		bool operator[](const sizeType& index) const
		{
			std::shared_lock lock(this->mMutex);
			if ((this->mData.size() * bitLength) <= index) {
				return false;
			}
			return (this->mData[(uint64)(index / bitLength)] & (T)this->bitValue(index)) != 0;
		}

		// Thread-safe reset. Acquires a unique lock.
		// Zeroes all bits, keeps the backing array allocated.
		void reset()
		{
			std::unique_lock lock(this->mMutex);
			this->mData.shallowClear(true);
		}

		// Thread-safe clear. Acquires a unique lock.
		// Zeroes all bits and frees the backing array.
		void clear()
		{
			std::unique_lock lock(this->mMutex);
			this->mData.deepClear();
		}
	};
}

}
