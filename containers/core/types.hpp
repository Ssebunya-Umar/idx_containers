#pragma once

namespace idx {

	// Fixed-width integer aliases used throughout the idx library.
	// Prefer these over raw C types for clarity and cross-platform consistency.
	using sint8  = char;                // signed 8-bit  (-128 to 127)
	using uint8  = unsigned char;       // unsigned 8-bit (0 to 255)
	using sint16 = signed short;        // signed 16-bit
	using uint16 = unsigned short;      // unsigned 16-bit
	using sint32 = signed int;          // signed 32-bit
	using uint32 = unsigned int;        // unsigned 32-bit
	using sint64 = signed long long int;   // signed 64-bit
	using uint64 = unsigned long long int; // unsigned 64-bit

}
