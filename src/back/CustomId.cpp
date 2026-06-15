#include "CustomId.h"
#include <chrono>
#include <random>

namespace back {

  // 64-symbol alphabet: 0-9 A-Z a-z $ @
  static constexpr char ALPHA[65] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                    "abcdefghijklmnopqrstuvwxyz"
                                    "$@";

  std::string encode_id_bytes(const std::vector<uint8_t> &bytes)
  {
    const int totalBits = static_cast<int>(bytes.size()) * 8;
    const int numChars  = (totalBits + 5) / 6; // ceil(totalBits / 6)

    std::string result;
    result.reserve(numChars);

    for (int ci = 0; ci < numChars; ci++) {
      int val = 0;
      for (int b = 0; b < 6; b++) {
        const int bitIdx    = ci * 6 + b;
        const int byteIdx   = bitIdx / 8;
        const int bitInByte = 7 - (bitIdx % 8);                                               // 7 = MSB
        const int bitVal    = (bitIdx < totalBits) ? ((bytes[byteIdx] >> bitInByte) & 1) : 0; // zero-pad the last group
        val                 = (val << 1) | bitVal;
      }
      result += ALPHA[val];
    }
    return result;
  }

  std::string new_custom_id(int timeBytes, int rndBytes)
  {
    const int            totalBytes = timeBytes + rndBytes;
    std::vector<uint8_t> buf(totalBytes, 0);

    // Time part: current microseconds, big-endian, least-significant timeBytes bytes
    if (timeBytes > 0) {
      const std::chrono::nanoseconds       time_since_epoch      = std::chrono::system_clock::now().time_since_epoch();
      const std::chrono::microseconds      enable_if_is_duration = std::chrono::duration_cast<std::chrono::microseconds>(time_since_epoch);
      const std::chrono::microseconds::rep count                 = enable_if_is_duration.count();
      const uint64_t                       us                    = static_cast<uint64_t>(count);
      const int                            effective             = (timeBytes < 8) ? timeBytes : 8;

      for (int i = 0; i < effective; i++) {
        buf[i] = static_cast<uint8_t>((us >> ((effective - 1 - i) * 8)) & 0xFF);
      }
    }

    // Random part: bytes from /dev/urandom (via std::random_device on Linux)
    if (rndBytes > 0) {
      std::random_device            rd;
      std::uniform_int_distribution dist(0, 255);
      for (int i = timeBytes; i < totalBytes; i++) {
        buf[i] = static_cast<uint8_t>(dist(rd));
      }
    }

    return encode_id_bytes(buf);
  }

  std::string new_id()
  {
    return new_custom_id(5, 4);
  }

} // namespace back
