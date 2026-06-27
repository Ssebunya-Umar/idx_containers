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

namespace idx {

// ─────────────────────────────────────────────────────────────────────────────
// Pair<T1, T2>
//
// A minimal key-value aggregate holding two values of (possibly different) types.
//
// NOTE ON COMPARISONS
//   All comparison operators compare ONLY the `first` member.
//   The `second` member is intentionally ignored, making Pair behave like a
//   key in sorted containers where `first` is the sort key.
// ─────────────────────────────────────────────────────────────────────────────
template<typename T1, typename T2>
struct Pair {

	T1 first;
	T2 second;

	// Default constructor.
	// Value-initialises both members (e.g. 0 for integers, nullptr for pointers).
	Pair() : first(T1()), second(T2()) {}

	// Single-argument constructor.
	// Sets `first` to `f`; `second` is value-initialised.
	// `f` – the value to assign to the first member.
	Pair(const T1& f) : first(f), second(T2()) {}

	// Two-argument constructor.
	// `f` – value for the first member (used as the comparison key).
	// `s` – value for the second member (carried along, not compared).
	Pair(const T1& f, const T2& s) : first(f), second(s) {}

	// Equality: returns true if this->first == other.first.
	// The `second` member is ignored.
	bool operator==(const Pair<T1, T2>& other) const { return this->first == other.first; }

	// Inequality: returns true if this->first != other.first.
	bool operator!=(const Pair<T1, T2>& other) const { return this->first != other.first; }

	// Greater-than: returns true if this->first > other.first.
	bool operator>(const Pair<T1, T2>& other) const { return this->first > other.first; }

	// Less-than: returns true if this->first < other.first.
	bool operator<(const Pair<T1, T2>& other) const { return this->first < other.first; }
};

}
