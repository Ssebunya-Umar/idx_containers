#pragma once

#include"core.hpp"

namespace idx {

// ─────────────────────────────────────────────────────────────────────────────
// Index<T, sizeType>
//
// A strongly-typed wrapper around a raw integer index used by idx containers
// (IndexedDynamicArray, SortedArray, HashMap, Queue, BSTree, AVLTree …).
//
// WHY USE THIS INSTEAD OF A PLAIN INTEGER?
//   A plain `uint32` index for an array of Players and an array of Enemies look
//   identical to the compiler. Accidentally using a player-index to look up an
//   enemy is a silent bug.  Index<Player, uint32> and Index<Enemy, uint32> are
//   completely different types, so the compiler will catch the mix-up.
//
// INVALID SENTINEL
//   A default-constructed Index (or one returned when an element is not found)
//   holds the INVALID_IDX sentinel value.  Always check .valid() / .invalid()
//   before using the underlying integer.
//
// ARITHMETIC IS DELETED
//   Indices are opaque handles, not offsets.  Adding two indices or negating one
//   makes no semantic sense, so those operators are deleted.  Only comparisons
//   are provided.
// ─────────────────────────────────────────────────────────────────────────────
template<typename T, typename sizeType>
class Index {

private:

	sizeType mIdx = INVALID_IDX(sizeType);

public:

	// Default constructor.
	// Creates an invalid index — mIdx is set to the INVALID_IDX sentinel.
	// Always check .valid() before using an index produced this way.
	Index(){}

	// Explicit value constructor.
	// Wraps an existing raw integer as a typed index.
	// `index`  – the raw slot number to wrap (must be a valid slot in the target container).
	explicit Index(const sizeType& index) : mIdx(index) {}

	// Returns the raw integer value stored inside this index.
	// Use sparingly — prefer passing the Index object directly to container methods.
	const sizeType& ref() const { return this->mIdx; }

	// Returns true when this index does NOT point to any valid element
	// (i.e. mIdx equals the INVALID_IDX sentinel).
	bool invalid() const { return this->mIdx == INVALID_IDX(sizeType); }

	// Returns true when this index points to a valid element
	// (i.e. mIdx does not equal the INVALID_IDX sentinel).
	bool valid() const { return this->mIdx != INVALID_IDX(sizeType); }

	// Arithmetic between indices is deleted — indices are handles, not offsets.
	Index<T, sizeType> operator+(const Index<T, sizeType>&) const = delete;
	Index<T, sizeType> operator-(const Index<T, sizeType>&) const = delete;
	Index<T, sizeType> operator*(const Index<T, sizeType>&) const = delete;
	Index<T, sizeType> operator/(const Index<T, sizeType>&) const = delete;
	Index<T, sizeType> operator-() const = delete;
	void operator+=(const Index<T, sizeType>&) = delete;
	void operator-=(const Index<T, sizeType>&) = delete;
	void operator*=(const Index<T, sizeType>&) = delete;
	void operator/=(const Index<T, sizeType>&) = delete;

	// Equality: returns true if both indices wrap the same raw integer value.
	bool operator==(const Index<T, sizeType>& other) const { return this->mIdx == other.mIdx; }

	// Inequality: returns true if the two indices wrap different raw integers.
	bool operator!=(const Index<T, sizeType>& other) const { return this->mIdx != other.mIdx; }

	// Less-than: compares raw integer values. Useful when storing indices in sorted structures.
	bool operator<(const Index<T, sizeType>& other) const { return this->mIdx < other.mIdx; }

	// Greater-than: compares raw integer values.
	bool operator>(const Index<T, sizeType>& other) const { return this->mIdx > other.mIdx; }

	// Less-than-or-equal: compares raw integer values.
	bool operator<=(const Index<T, sizeType>& other) const { return this->mIdx <= other.mIdx; }

	// Greater-than-or-equal: compares raw integer values.
	bool operator>=(const Index<T, sizeType>& other) const { return this->mIdx >= other.mIdx; }
};

}
