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
// String  (basic:: and threadSafe::)
//
// A Small String Optimised (SSO) string using sint8 characters.
// Strings up to idxSMALL_STRING_SIZE-1 (14) characters are stored inline in
// the object.  Longer strings are heap-allocated via RawPointer.
//
// INTERNAL LAYOUT (16 bytes total – same as the union)
//
//   Small (isHeap() == false):
//     mStack[0..14]  = character data + null terminator
//     mStackLen      = string length (high bit always 0 for small strings)
//
//   Heap (isHeap() == true):
//     mPointer       = RawPointer<sint8> pointing to:
//                        [ uint64 size ][ char data... ][ '\0' ]
//     mStackLen      = HEAP_FLAG (0x80) – signals heap mode
//
// The union overlaps mStack/mStackLen with mPointer, so the HEAP_FLAG in
// mStackLen is used as a discriminant.
//
// SUPPORTED FREE FUNCTIONS
//   length(const sint8*)          – strlen equivalent
//   toString1(sint64)             – integer to String
//   toString2(double)             – double to String (16 decimal places)
//   toString3(const void*)        – pointer address to hex String
//   operator+(sint8*, String)     – prepend a C-string literal
//
// KNOWN LIMITATION
//   No operator[] for mutable access – the string is effectively read-only
//   character-by-character unless you use cString() and a raw pointer.
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include"rawPointer.hpp"

#include <shared_mutex>
#include <mutex>
#include <cstdint>

namespace idx {

#define idxSMALL_STRING_SIZE 15
#define HEAP_FLAG             0x80

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Heap block layout (pointed to by mPointer):
    //
    //   [ uint64 size (8 bytes) ][ char data... (size + 1 bytes) ]
    //
    // mPointer ───────────────────────┐
    //                             ▼
    //                    [ size ][ d a t a \0 ]
    //
    //
    //   Small: [ mStack[0..14] ][ mStackLen ]   — high bit of mStackLen = 0
    //   Heap:  [ mPointer (8 bytes) ][ pad 7B   ][ mStackLen=HEAP_FLAG ]
    //           offsets 0-7        offsets 8-14   offset 15
    //
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    class StringBase {

    protected:

        union {
            struct {
                sint8  mStack[idxSMALL_STRING_SIZE];
                uint8  mStackLen;
            };
            RawPointer<sint8> mPointer;
        };

    protected:

        // Returns true when the string data lives on the heap (mStackLen has HEAP_FLAG set).
        // Returns false when the string fits in the inline mStack buffer.
        bool isHeap() const
        {
            return (this->mStackLen & HEAP_FLAG) != 0;
        }

        // Returns a mutable reference to the uint64 stored at the very start of the
        // heap allocation, which records the string's character count (not including '\0').
        // Only valid to call when isHeap() == true.
        uint64& heapSize() const
        {
            return *((uint64*)this->mPointer.pointer);
        }

        // Returns the number of characters in the string (not including the null terminator).
        // Reads from mStackLen for small strings, from heapSize() for heap strings.
        uint64 lengthInternal() const
        {
            return this->isHeap() ? this->heapSize() : this->mStackLen;
        }

        // Returns a const pointer to the first character of the string data.
        // Points into mStack for small strings, or to the character region of the
        // heap block (immediately after the uint64 size header) for heap strings.
        const sint8* dataInternal() const
        {
            return this->isHeap() ? this->heapData() : this->mStack;
        }

        // Returns a pointer to the character region of the heap allocation
        // (sizeof(uint64) bytes past the start of mPointer, skipping the size header).
        // Only valid to call when isHeap() == true.
        sint8* heapData() const
        {
            return this->mPointer.pointer + sizeof(uint64);
        }

        // If the string is currently in heap mode, releases the heap allocation,
        // zeroes mStack, and resets mStackLen to 0 (small-string empty state).
        // No-op if the string is already in small-string mode.
        void freeHeapIfActive()
        {
            if (this->isHeap()) {
                this->mPointer.clear(this->heapSize()+sizeof(uint64)+1);
                memset(this->mStack, 0, idxSMALL_STRING_SIZE);
                this->mStackLen = 0;
            }
        }

        // Stores `len` characters from `data` into the inline mStack buffer.
        // Frees any existing heap allocation first.
        // Appends a null terminator and sets mStackLen to `len`.
        // Precondition: `len` + 1 <= idxSMALL_STRING_SIZE (caller must check).
        // `data` – pointer to the source characters.
        // `len`  – number of characters to copy (not including '\0').
        void setSmallContent(const sint8* data, const uint64& len)
        {
            this->freeHeapIfActive();

            memcpy(this->mStack, data, len);
            this->mStack[len] = '\0';

            this->mStackLen = (uint8)len;
        }

        // Allocates a heap block of (sizeof(uint64) + len + 1) bytes, writes the
        // size header, copies `len` characters from `data`, appends '\0', and
        // sets mStackLen = HEAP_FLAG to indicate heap mode.
        // Frees any existing heap allocation first.
        // `data` – pointer to the source characters.
        // `len`  – number of characters to copy (not including '\0').
        void setHeapContent(const sint8* data, const uint64& len)
        {
            this->freeHeapIfActive();
            
            this->mPointer.allocate(sizeof(uint64) + len + 1);
            
            this->heapSize() = len;

            sint8* str = this->heapData();
            memcpy(str, data, len);
            str[len] = '\0';

            this->mStackLen = HEAP_FLAG;
        }

        // Resets the string to the empty state, freeing heap memory if needed.
        // After this call: mStack is zeroed, mStackLen == 0, no allocation.
        void clearInternal()
        {
            this->freeHeapIfActive();
            memset(this->mStack, 0, idxSMALL_STRING_SIZE);
            this->mStackLen = 0;
        }

        // Copies `len` characters from `data` into the string, choosing between
        // small-string and heap storage based on whether `len` + 1 fits in idxSMALL_STRING_SIZE.
        // `data` – pointer to the source characters.
        // `len`  – number of characters to store (not including '\0').
        void assignContent(const sint8* data, const uint64& len)
        {
            if (len + 1 <= idxSMALL_STRING_SIZE)
                this->setSmallContent(data, len);
            else
                this->setHeapContent(data, len);
        }

        // Appends the content of `other` to this string.
        // If this == &other, the content is doubled (self-append is safe).
        // Uses mStack if the combined length fits in idxSMALL_STRING_SIZE;
        // otherwise allocates a new heap block, copies both halves, and frees the old block.
        // `other` – the string to append. May be the same object (self-append).
        void appendInternal(const StringBase& other)
        {
            uint64 thisLen = this->lengthInternal();
            uint64 otherLen = other.lengthInternal();
            uint64 newLen = thisLen + otherLen;

            const sint8* thisData = this->dataInternal();
            const sint8* otherData = other.dataInternal();

            if (newLen + 1 <= idxSMALL_STRING_SIZE) {
                sint8 buf[idxSMALL_STRING_SIZE];
                memcpy(buf, thisData, thisLen);
                memcpy(buf + thisLen, otherData, otherLen);
                this->setSmallContent(buf, newLen);
            }
            else {
                RawPointer<sint8> temp;
                if(this->isHeap()) {
                    temp.hijack(this->mPointer); // Save the old heap block; prevents use-after-free when this == &other.
                }

                this->mPointer.allocate(sizeof(uint64) + newLen + 1);
                this->heapSize() = newLen;

                sint8* buf = this->heapData();
                memcpy(buf, thisData, thisLen);
                memcpy(buf + thisLen, otherData, otherLen);
                buf[newLen] = '\0';

                this->mStackLen = HEAP_FLAG;

                temp.clear(sizeof(uint64)+thisLen+1);
            }
        }

        // Copies `other`'s content into this string via assignContent().
        // No-op if this == &other.
        // `other` – the string to copy from.
        void cloneInternal(const StringBase& other)
        {
            if (this == &other) return;
            this->assignContent(other.dataInternal(), other.lengthInternal());
        }

        // Steals `other`'s raw union bytes into this string, then zeros `other`.
        // Works for both small and heap strings by memcpy-ing the entire union.
        // After the call: this holds other's content; other is an empty small string.
        // No-op if this == &other. Frees any existing allocation in this first.
        // `other` – the string to steal from.
        void hijackInternal(StringBase& other)
        {
            if (this == &other) return;

            this->freeHeapIfActive();

            memcpy(this->mStack, other.mStack, idxSMALL_STRING_SIZE);
            this->mStackLen = other.mStackLen;
            memset(other.mStack, 0, idxSMALL_STRING_SIZE);
            other.mStackLen = 0;
        }

        // Resizes the string to exactly `newSize` characters.
        // If shrinking: content is truncated to `newSize` characters (+ '\0').
        // If growing  : new characters are zero-initialised.
        // Storage mode (small vs heap) is re-evaluated for the new size.
        // `newSize` – the desired character count (not including '\0').
        void resizeInternal(const uint64& newSize)
        {
            uint64 oldLen  = this->lengthInternal();
            if (newSize == oldLen) return;

            uint64 copyLen = newSize < oldLen ? newSize : oldLen;

            if (newSize + 1 <= idxSMALL_STRING_SIZE) {
                sint8 buf[idxSMALL_STRING_SIZE] = {};
                memcpy(buf, this->dataInternal(), copyLen);
                this->setSmallContent(buf, newSize);
            }
            else {
                RawPointer<sint8> temp;
                temp.hijack(this->mPointer);

                this->mPointer.allocate(sizeof(uint64) + newSize + 1);
                this->heapSize() = newSize;

                memcpy(this->heapData(), temp.pointer+sizeof(uint64), copyLen);
                memset(this->heapData() + copyLen, 0, newSize - copyLen);
                this->heapData()[newSize] = '\0';

                this->mStackLen = HEAP_FLAG;

                temp.clear(sizeof(uint64)+oldLen+1);
            }
        }

        // Default constructor. Initialises to an empty small string (all bytes zeroed).
        StringBase()
        {
            memset(this->mStack, 0, idxSMALL_STRING_SIZE);
            this->mStackLen = 0;
        }

        // Length constructor. Creates a zero-filled string of exactly `length` characters.
        // Chooses small or heap storage based on `length`.
        // `length` – the desired number of characters (not including '\0').
        explicit StringBase(const uint64& length)
        {
            memset(this->mStack, 0, idxSMALL_STRING_SIZE);
            this->mStackLen = 0;
            this->resizeInternal(length);
        }

        // C-string constructor. Copies from null-terminated `str`.
        // If `str` is nullptr or empty, creates an empty string.
        // `str` – null-terminated C-string to copy from.
        explicit StringBase(const sint8* str)
        {
            memset(this->mStack, 0, idxSMALL_STRING_SIZE);
            this->mStackLen = 0;

            if (str == nullptr) return;

            uint64 len = 0;
            while (str[len] != '\0') len++;

            if (len > 0) this->assignContent(str, len);
        }

        // Destructor. Frees the heap allocation if one is active.
        ~StringBase()
        {
            this->freeHeapIfActive();
        }

    public:

        // Copy/move are deleted at the base level.
        // Subclasses (basic::String, threadSafe::String) define their own with correct locking.
        StringBase(const StringBase&) = delete;
        StringBase& operator=(const StringBase&) = delete;
        StringBase(StringBase&&) = delete;
        StringBase& operator=(StringBase&&) = delete;
    };

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    namespace basic {

        class String : public StringBase {

        public:

            // Default constructor. Creates an empty string.
            String() : StringBase() {}

            // Length constructor. Creates a zero-filled string of `length` characters.
            // `length` – desired character count (not including '\0').
            explicit String(const uint64& length) : StringBase(length) {}

            // C-string constructor. Copies from null-terminated `str`.
            // `str` – source string. Copies until the null terminator.
            explicit String(const sint8* str) : StringBase(str) {}

            // Destructor. StringBase::~StringBase() handles the heap free.
            ~String()
            {
                // StringBase destructor handles the free; listed here for clarity
            }

            // Copy constructor. Deep-copies `other`'s content.
            String(const String& other) : StringBase()
            {
                this->cloneInternal(other);
            }

            // Move constructor. Steals `other`'s content; `other` becomes empty.
            String(String&& other) noexcept : StringBase()
            {
                this->hijackInternal(other);
            }

            // Copy assignment. Replaces this string's content with a copy of `other`'s.
            // Returns *this.
            String& operator=(const String& other)
            {
                if (this == &other) return *this;
                this->cloneInternal(other);
                return *this;
            }

            // Move assignment. Steals `other`'s content; `other` becomes empty.
            // Returns *this.
            String& operator=(String&& other) noexcept
            {
                if (this == &other) return *this;
                this->hijackInternal(other);
                return *this;
            }

            // Equality comparison. Returns true if both strings have the same length
            // and the same byte content (memcmp).
            // Self-comparison always returns true.
            bool operator==(const String& other) const
            {
                if (this == &other) return true;
                uint64 len1 = this->lengthInternal();
                uint64 len2 = other.lengthInternal();
                if (len1 != len2) return false;
                if (len1 == 0)    return true;
                return memcmp(this->dataInternal(), other.dataInternal(), len1) == 0;
            }

            // Inequality comparison. Returns true if the strings differ in length or content.
            bool operator!=(const String& other) const
            {
                return !(*this == other);
            }

            // Appends `other`'s content to the end of this string in-place.
            // `other` – the String to append. Self-append (other == *this) is handled safely.
            void operator+=(const String& other)
            {
                this->appendInternal(other);
            }

            // Appends a C-string `str` to the end of this string in-place.
            // Constructs a temporary String from `str` and appends it.
            // `str` – null-terminated C-string to append.
            void operator+=(const sint8* str)
            {
                *this += String(str);
            }

            // Returns a new String that is the concatenation of this string and `other`.
            // `other` – the String to concatenate on the right.
            // Returns a new String containing this + other.
            String operator+(const String& other) const
            {
                String result(*this);
                result += other;
                return result;
            }

            // Returns a new String that is the concatenation of this string and C-string `str`.
            // `str` – null-terminated C-string to append on the right.
            // Returns a new String containing this + str.
            String operator+(const sint8* str) const
            {
                String result(*this);
                result += str;
                return result;
            }

            // Read-only indexed access to a single character.
            // `index` – zero-based character position. Must be < length() (asserted).
            // Returns a const reference to the character at position `index`.
            const sint8& operator[](const uint64& index) const
            {
                uint64 len = this->lengthInternal();
                idxASSERT((len > 0) && (index < len), "index out of range or string is empty");
                return this->dataInternal()[index];
            }

            // Returns the number of characters in the string (not including the null terminator).
            uint64 length() const
            {
                return this->lengthInternal();
            }

            // Returns a const pointer to the null-terminated character data.
            // The pointer is valid until the string is modified or destroyed.
            const sint8* cString() const
            {
                return this->dataInternal();
            }

            // Returns true when the string has no characters (length() == 0).
            bool empty() const
            {
                return this->lengthInternal() == 0;
            }

            // Resizes the string to exactly `size` characters.
            // Shrinking truncates; growing zero-initialises the new characters.
            // `size` – the desired character count (not including '\0').
            void resize(const uint64& size)
            {
                this->resizeInternal(size);
            }

            // Resets the string to empty, freeing heap memory if active.
            // After this call: empty(), length() == 0, no allocation.
            void clear()
            {
                this->clearInternal();
            }
        };
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    namespace threadSafe {

        class String : public StringBase {

        private:

            mutable std::shared_mutex mMutex;

        public:

            // Default constructor. Creates an empty string with no allocation.
            String() : StringBase() {}

            // Length constructor. Creates a zero-filled string of `length` characters.
            // `length` – desired character count (not including '\0').
            explicit String(const uint64& length) : StringBase(length) {}

            // C-string constructor. Copies from null-terminated `str`.
            // `str` – source string. Copies until the null terminator.
            explicit String(const sint8* str) : StringBase(str) {}

            // Destructor. StringBase::~StringBase() handles the heap free.
            ~String()
            {
                // StringBase destructor handles the free
            }

            // Copy constructor. Acquires a shared lock on `other` while copying.
            String(const String& other) : StringBase()
            {
                std::shared_lock lock(other.mMutex);
                this->cloneInternal(other);
            }

            // Move constructor. Acquires a unique lock on `other` while stealing its content.
            String(String&& other) noexcept : StringBase()
            {
                std::unique_lock lock(other.mMutex);
                this->hijackInternal(other);
            }

            // Copy assignment. Locks both strings atomically via scoped_lock.
            // Returns *this.
            String& operator=(const String& other)
            {
                if (this == &other) return *this;
                std::scoped_lock lock(this->mMutex, other.mMutex);
                this->cloneInternal(other);
                return *this;
            }

            // Move assignment. Locks both strings atomically, then steals other's content.
            // Returns *this.
            String& operator=(String&& other) noexcept
            {
                if (this == &other) return *this;
                std::scoped_lock lock(this->mMutex, other.mMutex);
                this->hijackInternal(other);
                return *this;
            }

            // Thread-safe equality. Locks both strings atomically.
            // Returns true if both have the same length and byte content.
            bool operator==(const String& other) const
            {
                if (this == &other) return true;
                std::scoped_lock lock(this->mMutex, other.mMutex);
                uint64 len1 = this->lengthInternal();
                uint64 len2 = other.lengthInternal();
                if (len1 != len2) return false;
                if (len1 == 0)    return true;
                return memcmp(this->dataInternal(), other.dataInternal(), len1) == 0;
            }

            // Thread-safe inequality. Returns true if the strings differ.
            bool operator!=(const String& other) const
            {
                return !(*this == other);
            }

            // Thread-safe append of a String. Unique-locks this for self-append;
            // scoped_lock on both for cross-object append.
            // `other` – the String to append. Self-append is handled safely.
            void operator+=(const String& other)
            {
                if (this == &other) {
                    std::unique_lock lock(this->mMutex);
                    this->appendInternal(other);
                    return;
                }
                std::scoped_lock lock(this->mMutex, other.mMutex);
                this->appendInternal(other);
            }

            // Thread-safe append of a C-string. Constructs a temporary String and appends.
            // `str` – null-terminated C-string to append.
            void operator+=(const sint8* str)
            {
                *this += String(str);
            }

            // Thread-safe concatenation. Acquires a shared lock on this while copying,
            // then appends `other` to the copy.
            // `other` – the String to concatenate on the right.
            // Returns a new String containing this + other.
            String operator+(const String& other) const
            {
                String result;
                {
                    std::shared_lock lock(this->mMutex);
                    result.cloneInternal(*this);
                }
                result += other;
                return result;
            }

            // Thread-safe concatenation with a C-string.
            // `str` – null-terminated C-string to append.
            // Returns a new String containing this + str.
            String operator+(const sint8* str) const
            {
                String result;
                {
                    std::shared_lock lock(this->mMutex);
                    result.cloneInternal(*this);
                }
                result += str;
                return result;
            }

            // Thread-safe read-only indexed access. Acquires a shared lock.
            // `index` – zero-based position. Must be < length() (asserted).
            // Returns a const reference to the character at `index`.
            const sint8& operator[](const uint64& index) const
            {
                std::shared_lock lock(this->mMutex);
                uint64 len = this->lengthInternal();
                idxASSERT((len > 0) && (index < len), "index out of range or string is empty");
                return this->dataInternal()[index];
            }

            // Thread-safe length(). Acquires a shared lock.
            // Returns the number of characters (not including '\0').
            uint64 length() const
            {
                std::shared_lock lock(this->mMutex);
                return this->lengthInternal();
            }

            // Thread-safe cString(). Acquires a shared lock.
            // Returns a const pointer to the null-terminated data.
            // WARNING: the pointer is only valid while the lock is held; do not cache it.
            const sint8* cString() const
            {
                std::shared_lock lock(this->mMutex);
                return this->dataInternal();
            }

            // Thread-safe empty(). Delegates to length() (which takes a shared lock).
            // Returns true when the string has no characters.
            bool empty() const
            {
                return this->length() == 0;
            }

            // Thread-safe resize(). Acquires a unique lock.
            // `size` – desired character count. Truncates or zero-extends.
            void resize(const uint64& size)
            {
                std::unique_lock lock(this->mMutex);
                this->resizeInternal(size);
            }

            // Thread-safe clear(). Acquires a unique lock.
            // Resets to empty, freeing heap memory if active.
            void clear()
            {
                std::unique_lock lock(this->mMutex);
                this->clearInternal();
            }
        };
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    // Free function: prepend a C-string literal to a basic::String.
    // Returns a new String equal to lhs + rhs.
    // `lhs` – null-terminated C-string on the left.
    // `rhs` – the String to append on the right.
    static basic::String operator+(const sint8* lhs, const basic::String& rhs)
    {
        return basic::String(lhs) + rhs;
    }

    // Free function: prepend a C-string literal to a threadSafe::String.
    // Returns a new threadSafe::String equal to lhs + rhs.
    // `lhs` – null-terminated C-string on the left.
    // `rhs` – the threadSafe::String to append on the right.
    static threadSafe::String operator+(const sint8* lhs, const threadSafe::String& rhs)
    {
        return threadSafe::String(lhs) + rhs;
    }

    // Returns the number of characters in the null-terminated C-string `str`
    // (equivalent to strlen). Does NOT include the null terminator.
    // `str` – a null-terminated C-string.
    // Returns the character count.
    static uint64 length(const sint8* str)
    {
        uint64 len = 0;
        while (str[len] != '\0') len++;
        return len;
    }

    // Converts a signed 64-bit integer to its decimal string representation.
    // Handles zero and negative values correctly. No trailing whitespace or sign for positives.
    // `number` – the integer to convert.
    // Returns a basic::String containing the decimal digits (with a leading '-' if negative).
    static basic::String toString1(const sint64& number)
    {
        if (number == 0) return basic::String("0");

        bool   negative  = number < 0;
        uint64 magnitude = negative ? (uint64(0) - uint64(number)) : uint64(number);

        uint64 numDigits = 0;
        uint64 temp      = magnitude;
        while (temp != 0) { temp /= 10; numDigits++; }

        sint8  str[32];
        uint64 index = numDigits;

        if (negative) {
            str[0] = '-';
            str[numDigits + 1] = '\0';
            index++;
        }
        else {
            str[numDigits] = '\0';
        }

        temp = magnitude;
        while (temp != 0) {
            str[--index] = '0' + (temp % 10);
            temp /= 10;
        }

        return basic::String(str);
    }

    // Converts a double-precision floating-point number to a string.
    // Always produces exactly 16 decimal places after the dot.
    // Leading '-' is added for negative values.
    // `number` – the double to convert.
    // Returns a basic::String such as "-3.1415926535897932".
    static basic::String toString2(const double& number)
    {
        double n = number < 0 ? -number : number;

        sint64 intPart = (sint64)n;
        basic::String str;

        if (intPart == 0) {
            str = basic::String("0");
        }
        else {
            sint64 tmp = intPart;
            while (tmp > 0) {
                sint8 digit = (sint8)(tmp % 10);
                str = toString1(digit) + str;
                tmp /= 10;
            }
        }

        str += basic::String(".");

        double frac = n - (double)(sint64)(n);
        for (sint32 i = 0; i < 16; ++i) {
            frac *= 10;
            int digit = (int)(frac);
            str += toString1(digit);
            frac -= digit;
        }

        if (number < 0) str = basic::String("-") + str;

        return str;
    }

    // Converts a pointer address to a fixed-width lowercase hexadecimal string.
    // Always produces sizeof(void*) * 2 hex digits (16 characters on 64-bit).
    // No "0x" prefix is added.
    // `address` – the pointer whose numeric address to format.
    // Returns a basic::String such as "00007ffee8a3b2c0".
    static basic::String toString3(const void* address)
    {
        const sint8 hexDigits[] = "0123456789abcdef";
        uint64      addrValue   = (uint64)address;

        sint8  str[30];
        uint64 index = 0;
        for (sint32 x = sizeof(addrValue) * 2 - 1; x >= 0; --x)
            str[index++] = hexDigits[(addrValue >> (x * 4)) & 0xF];
        str[index] = '\0';

        return basic::String(str);
    }
}
