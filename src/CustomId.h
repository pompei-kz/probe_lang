#pragma once
#include <cstdint>
#include <string>
#include <vector>

// Encode a byte sequence into a 64-symbol string (6 bits per symbol, MSB first).
// Alphabet: 0-9 A-Z a-z $ @  (indices 0-63).
// If total bits is not divisible by 6 the last group is padded with zeros.
// No padding character is appended — the caller knows the expected length.
std::string encode_id_bytes(const std::vector<uint8_t> &bytes);

// Generate a random ID string.
// timeBytes — number of bytes taken from the current time in microseconds
//             (big-endian, least-significant timeBytes bytes of the uint64 value).
// rndBytes  — number of cryptographically-random bytes appended after the time part.
// Returns a string of length ceil((timeBytes + rndBytes) * 8 / 6).
std::string new_custom_id(int timeBytes, int rndBytes);

// Equivalent to new_custom_id(5, 4) → 12-character ID.
std::string new_id();
