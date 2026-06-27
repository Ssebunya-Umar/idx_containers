#pragma once

namespace idx {

// BIT(n) evaluates to an unsigned 64-bit mask with only bit n set.
// Example: BIT(3) == 0b1000 == 8
#define BIT(n) 1ULL<<n

// INVALID_IDX(type) gives the sentinel "no valid index" value for a given integer type.
// It casts the all-ones 64-bit pattern to `type`, giving the maximum unsigned value
// for that type (e.g. 0xFF for uint8, 0xFFFF for uint16, 0xFFFFFFFFFFFFFFFF for uint64).
// Any index equal to this value is treated as "not found" or "empty".
#define INVALID_IDX(type) (type)(0xffffffffffffffffULL)

}
