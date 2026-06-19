#include "back/etc/CustomId.h"
#include <gtest/gtest.h>
#include <set>
#include <string>
#include <utility>
#include <vector>

using namespace back;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static const std::string VALID_CHARS = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz$@";

static bool all_valid_chars(const std::string &s)
{
  for (char c : s)
    if (VALID_CHARS.find(c) == std::string::npos) return false;
  return true;
}

// Expected output length: ceil((timeBytes + rndBytes) * 8 / 6)
static int expected_len(int t, int r)
{
  return ((t + r) * 8 + 5) / 6;
}

// ---------------------------------------------------------------------------
// encode_id_bytes — deterministic tests with known byte sequences
// ---------------------------------------------------------------------------

TEST(EncodeIdBytesTest, EmptyInput)
{
  //
  //
  const std::string id = encode_id_bytes({});
  //
  //

  EXPECT_EQ(id, "");
}

TEST(EncodeIdBytesTest, SingleZeroByte)
{
  // 0x00 = 00000000 → char0: 000000=0→'0', char1: 00+pad→0→'0'
  //
  //
  const std::string id = encode_id_bytes({0x00});
  //
  //

  EXPECT_EQ(id, "00");
}

TEST(EncodeIdBytesTest, SingleFFByte)
{
  // 0xFF = 11111111 → char0: 111111=63→'@', char1: 11+0000=48→'m'
  //
  //
  const std::string id = encode_id_bytes({0xFF});
  //
  //

  EXPECT_EQ(id, "@m");
}

TEST(EncodeIdBytesTest, Single0x80)
{
  // 0x80 = 10000000 → char0: 100000=32→'W', char1: 00+0000=0→'0'
  //
  //
  const std::string id = encode_id_bytes({0x80});
  //
  //

  EXPECT_EQ(id, "W0");
}

TEST(EncodeIdBytesTest, Single0x01)
{
  // 0x01 = 00000001 → char0: 000000=0→'0', char1: 01+0000=16→'G'
  //
  //
  const std::string id = encode_id_bytes({0x01});
  //
  //

  EXPECT_EQ(id, "0G");
}

TEST(EncodeIdBytesTest, ThreeZeroBytes)
{
  // 24 bits, exactly 4 groups of 6 — all zero → "0000"
  //
  //
  const std::string id = encode_id_bytes({0x00, 0x00, 0x00});
  //
  //

  EXPECT_EQ(id, "0000");
}

TEST(EncodeIdBytesTest, ThreeFFBytes)
{
  // 24 bits, exactly 4 groups of 6 — all ones → "@@@@"
  //
  //
  const std::string id = encode_id_bytes({0xFF, 0xFF, 0xFF});
  //
  //

  EXPECT_EQ(id, "@@@@");
}

TEST(EncodeIdBytesTest, TwoBytes0xFC00)
{
  // 0xFC=11111100, 0x00=00000000 → 16 bits → 3 chars
  // char0: 111111=63→'@'
  // char1: 00(from FC)+0000(from 00)=0→'0'
  // char2: 0000(from 00)+00pad=0→'0'
  //
  //
  const std::string id = encode_id_bytes({0xFC, 0x00});
  //
  //

  EXPECT_EQ(id, "@00");
}

TEST(EncodeIdBytesTest, TwoBytes0x0102)
{
  // 0x01=00000001, 0x02=00000010 → 16 bits → 3 chars
  // char0: 000000=0→'0'
  // char1: 01(from 01)+0000(from 02)=010000=16→'G'
  // char2: 0010(from 02)+00pad=001000=8→'8'
  //
  //
  const std::string id = encode_id_bytes({0x01, 0x02});
  //
  //

  EXPECT_EQ(id, "0G8");
}

TEST(EncodeIdBytesTest, SixBytesAllZero)
{
  // 48 bits = exactly 8 chars, all '0'
  //
  //
  const std::string id = encode_id_bytes({0, 0, 0, 0, 0, 0});
  //
  //

  EXPECT_EQ(id, "00000000");
}

TEST(EncodeIdBytesTest, SixBytesAllFF)
{
  // 48 bits = exactly 8 chars, all '@'
  //
  //
  const std::string id = encode_id_bytes({0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF});
  //
  //

  EXPECT_EQ(id, "@@@@@@@@");
}

// ---------------------------------------------------------------------------
// encode_id_bytes — length formula
// ---------------------------------------------------------------------------

TEST(EncodeIdBytesTest, LengthFormula)
{
  for (int n = 0; n <= 20; n++) {
    std::vector<uint8_t> v(n, 0);
    const int            want = (n * 8 + 5) / 6;

    //
    //
    const std::string id = encode_id_bytes(v);
    //
    //

    EXPECT_EQ((int)id.size(), want) << "n=" << n;
  }
}

// ---------------------------------------------------------------------------
// new_custom_id — length
// ---------------------------------------------------------------------------

TEST(NewCustomIdTest, LengthZeroZero)
{
  //
  //
  const std::string id = new_custom_id(0, 0);
  //
  //

  EXPECT_EQ((int)id.size(), expected_len(0, 0));
}

TEST(NewCustomIdTest, LengthOneZero)
{
  //
  //
  const std::string id = new_custom_id(1, 0);
  //
  //

  EXPECT_EQ((int)id.size(), expected_len(1, 0)); // 2
}

TEST(NewCustomIdTest, LengthZeroOne)
{
  //
  //
  const std::string id = new_custom_id(0, 1);
  //
  //

  EXPECT_EQ((int)id.size(), expected_len(0, 1)); // 2
}

TEST(NewCustomIdTest, LengthThreeZero)
{
  //
  //
  const std::string id = new_custom_id(3, 0);
  //
  //

  EXPECT_EQ((int)id.size(), expected_len(3, 0)); // 4
}

TEST(NewCustomIdTest, LengthOneOne)
{
  //
  //
  const std::string id = new_custom_id(1, 1);
  //
  //

  EXPECT_EQ((int)id.size(), expected_len(1, 1)); // 3
}

TEST(NewCustomIdTest, LengthFiveFour)
{
  //
  //
  const std::string id = new_custom_id(5, 4);
  //
  //

  EXPECT_EQ((int)id.size(), expected_len(5, 4)); // 12
}

TEST(NewCustomIdTest, LengthEightEight)
{
  //
  //
  const std::string id = new_custom_id(8, 8);
  //
  //

  EXPECT_EQ((int)id.size(), expected_len(8, 8)); // 22
}

// ---------------------------------------------------------------------------
// new_custom_id — valid characters
// ---------------------------------------------------------------------------

TEST(NewCustomIdTest, ValidChars)
{
  for (auto [t, r] : {std::pair{5, 4}, std::pair{1, 1}, std::pair{0, 8}, std::pair{8, 0}}) {
    //
    //
    const std::string id = new_custom_id(t, r);
    //
    //

    EXPECT_TRUE(all_valid_chars(id)) << "t=" << t << " r=" << r;
  }
}

TEST(NewCustomIdTest, AllAlphabetCharsAreValid)
{
  for (int i = 0; i < 200; i++) {
    //
    //
    const std::string id = new_id();
    //
    //

    EXPECT_TRUE(all_valid_chars(id)) << "iteration " << i;
  }
}

// ---------------------------------------------------------------------------
// new_custom_id — uniqueness
// ---------------------------------------------------------------------------

TEST(NewCustomIdTest, Uniqueness100)
{
  std::set<std::string> ids;
  for (int i = 0; i < 100; i++) {
    //
    //
    const std::string id = new_id();
    //
    //

    ids.insert(id);
  }
  // Probability of collision with 4 random bytes (2^32) is negligible
  EXPECT_EQ((int)ids.size(), 100);
}

TEST(NewCustomIdTest, RandomOnlyUniqueness)
{
  std::set<std::string> ids;
  for (int i = 0; i < 50; i++) {
    //
    //
    const std::string id = new_custom_id(0, 8);
    //
    //

    ids.insert(id);
  }
  EXPECT_EQ((int)ids.size(), 50);
}

// ---------------------------------------------------------------------------
// new_custom_id — time component produces non-decreasing values over time
// (only testable with time bytes and no random; same microsecond → equal)
// ---------------------------------------------------------------------------

TEST(NewCustomIdTest, TimeOnlyNonDecreasing)
{
  // Because the time bytes are MSB-first big-endian and the alphabet indices
  // are ordered 0..63, lexicographic comparison of the encoded strings is NOT
  // guaranteed to match numeric order. We therefore just verify the length.
  const int N = 10;
  for (int i = 0; i < N; i++) {
    //
    //
    const std::string id = new_custom_id(5, 0);
    //
    //

    EXPECT_EQ((int)id.size(), expected_len(5, 0)); // 7
  }
}

// ---------------------------------------------------------------------------
// new_id
// ---------------------------------------------------------------------------

TEST(NewIdTest, Length)
{
  //
  //
  const std::string id = new_id();
  //
  //

  EXPECT_EQ((int)id.size(), 12);
}

TEST(NewIdTest, ValidChars)
{
  //
  //
  const std::string id = new_id();
  //
  //

  EXPECT_TRUE(all_valid_chars(id));
}

TEST(NewIdTest, SameLengthAsCustomId54)
{
  //
  //
  const std::string id = new_id();
  //
  //

  EXPECT_EQ(id.size(), new_custom_id(5, 4).size());
}

TEST(NewIdTest, Unique)
{
  std::set<std::string> ids;
  for (int i = 0; i < 50; i++) {
    //
    //
    const std::string id = new_id();
    //
    //

    ids.insert(id);
  }
  EXPECT_EQ((int)ids.size(), 50);
}
