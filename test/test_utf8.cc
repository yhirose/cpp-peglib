#include <gtest/gtest.h>
#include <peglib.h>

using namespace peg;

// --- codepoint_length ---

TEST(Utf8Test, Codepoint_length_ascii) {
  const char *s = "A";
  EXPECT_EQ(codepoint_length(s, 1), 1u);
}

TEST(Utf8Test, Codepoint_length_2byte) {
  // U+00E9 (é) = 0xC3 0xA9
  const char s[] = "\xC3\xA9";
  EXPECT_EQ(codepoint_length(s, 2), 2u);
}

TEST(Utf8Test, Codepoint_length_3byte) {
  // U+3042 (あ) = 0xE3 0x81 0x82
  const char s[] = "\xE3\x81\x82";
  EXPECT_EQ(codepoint_length(s, 3), 3u);
}

TEST(Utf8Test, Codepoint_length_4byte) {
  // U+1F600 (😀) = 0xF0 0x9F 0x98 0x80
  const char s[] = "\xF0\x9F\x98\x80";
  EXPECT_EQ(codepoint_length(s, 4), 4u);
}

TEST(Utf8Test, Codepoint_length_empty) {
  EXPECT_EQ(codepoint_length("", 0), 0u);
}

TEST(Utf8Test, Codepoint_length_truncated) {
  // 3-byte sequence but only 2 bytes available
  const char s[] = "\xE3\x81";
  EXPECT_EQ(codepoint_length(s, 2), 0u);
}

// --- codepoint_count ---

TEST(Utf8Test, Codepoint_count_ascii) {
  const char *s = "hello";
  EXPECT_EQ(codepoint_count(s, 5), 5u);
}

TEST(Utf8Test, Codepoint_count_mixed) {
  // "aあb" = 'a' + 3-byte + 'b' = 5 bytes, 3 codepoints
  const char s[] = "a\xE3\x81\x82"
                   "b";
  EXPECT_EQ(codepoint_count(s, 5), 3u);
}

TEST(Utf8Test, Codepoint_count_empty) { EXPECT_EQ(codepoint_count("", 0), 0u); }

TEST(Utf8Test, Codepoint_count_emoji) {
  // "😀😀" = 2 x 4-byte = 8 bytes, 2 codepoints
  const char s[] = "\xF0\x9F\x98\x80\xF0\x9F\x98\x80";
  EXPECT_EQ(codepoint_count(s, 8), 2u);
}

// --- encode_codepoint ---

TEST(Utf8Test, Encode_codepoint_ascii) {
  char buff[4];
  auto len = encode_codepoint(U'A', buff);
  EXPECT_EQ(len, 1u);
  EXPECT_EQ(buff[0], 'A');
}

TEST(Utf8Test, Encode_codepoint_2byte) {
  auto s = encode_codepoint(U'\u00E9'); // é
  EXPECT_EQ(s.size(), 2u);
  EXPECT_EQ(s, "\xC3\xA9");
}

TEST(Utf8Test, Encode_codepoint_3byte) {
  auto s = encode_codepoint(U'\u3042'); // あ
  EXPECT_EQ(s.size(), 3u);
  EXPECT_EQ(s, "\xE3\x81\x82");
}

TEST(Utf8Test, Encode_codepoint_4byte) {
  auto s = encode_codepoint(U'\U0001F600'); // 😀
  EXPECT_EQ(s.size(), 4u);
  EXPECT_EQ(s, "\xF0\x9F\x98\x80");
}

TEST(Utf8Test, Encode_codepoint_surrogate_returns_zero) {
  // Surrogates (U+D800-U+DFFF) are invalid
  char buff[4];
  auto len = encode_codepoint(0xD800, buff);
  EXPECT_EQ(len, 0u);
}

TEST(Utf8Test, Encode_codepoint_beyond_unicode_returns_zero) {
  char buff[4];
  auto len = encode_codepoint(0x110000, buff);
  EXPECT_EQ(len, 0u);
}

// --- decode_codepoint ---

TEST(Utf8Test, Decode_codepoint_ascii) {
  const char *s = "A";
  size_t bytes;
  char32_t cp;
  EXPECT_TRUE(decode_codepoint(s, 1, bytes, cp));
  EXPECT_EQ(bytes, 1u);
  EXPECT_EQ(cp, U'A');
}

TEST(Utf8Test, Decode_codepoint_2byte) {
  const char s[] = "\xC3\xA9";
  size_t bytes;
  char32_t cp;
  EXPECT_TRUE(decode_codepoint(s, 2, bytes, cp));
  EXPECT_EQ(bytes, 2u);
  EXPECT_EQ(cp, U'\u00E9');
}

TEST(Utf8Test, Decode_codepoint_3byte) {
  const char s[] = "\xE3\x81\x82";
  size_t bytes;
  char32_t cp;
  EXPECT_TRUE(decode_codepoint(s, 3, bytes, cp));
  EXPECT_EQ(bytes, 3u);
  EXPECT_EQ(cp, U'\u3042');
}

TEST(Utf8Test, Decode_codepoint_4byte) {
  const char s[] = "\xF0\x9F\x98\x80";
  size_t bytes;
  char32_t cp;
  EXPECT_TRUE(decode_codepoint(s, 4, bytes, cp));
  EXPECT_EQ(bytes, 4u);
  EXPECT_EQ(cp, U'\U0001F600');
}

TEST(Utf8Test, Decode_codepoint_empty) {
  size_t bytes;
  char32_t cp;
  EXPECT_FALSE(decode_codepoint("", 0, bytes, cp));
}

TEST(Utf8Test, Decode_codepoint_convenience_with_bytes) {
  const char s[] = "\xE3\x81\x82";
  char32_t cp;
  auto bytes = decode_codepoint(s, 3, cp);
  EXPECT_EQ(bytes, 3u);
  EXPECT_EQ(cp, U'\u3042');
}

TEST(Utf8Test, Decode_codepoint_convenience_simple) {
  const char s[] = "\xE3\x81\x82";
  auto cp = decode_codepoint(s, 3);
  EXPECT_EQ(cp, U'\u3042');
}

// --- decode (full string) ---

TEST(Utf8Test, Decode_full_string) {
  const char s[] = "a\xE3\x81\x82"
                   "b";
  auto u32 = decode(s, 5);
  EXPECT_EQ(u32.size(), 3u);
  EXPECT_EQ(u32[0], U'a');
  EXPECT_EQ(u32[1], U'\u3042');
  EXPECT_EQ(u32[2], U'b');
}

TEST(Utf8Test, Decode_empty_string) {
  auto u32 = decode("", 0);
  EXPECT_EQ(u32.size(), 0u);
}

// --- roundtrip ---

TEST(Utf8Test, Encode_decode_roundtrip) {
  std::vector<char32_t> codepoints = {U'A', U'\u00E9', U'\u3042',
                                      U'\U0001F600'};
  for (auto cp : codepoints) {
    auto encoded = encode_codepoint(cp);
    auto decoded = decode_codepoint(encoded.c_str(), encoded.size());
    EXPECT_EQ(decoded, cp);
  }
}
