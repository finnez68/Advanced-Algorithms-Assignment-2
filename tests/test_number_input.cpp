#include "number_input.h"
#include <gtest/gtest.h>

TEST(ParseNumber, PlainDigits) {
    auto p = ParseNumber("561");
    ASSERT_TRUE(p.ok);
    EXPECT_EQ(p.value, 561u);
}

TEST(ParseNumber, SeparatorsIgnored) {
    auto p = ParseNumber("1,000,000,007");
    ASSERT_TRUE(p.ok);
    EXPECT_EQ(p.value, 1000000007u);
    EXPECT_EQ(ParseNumber("1 000_000'007").value, 1000000007u);
}

TEST(ParseNumber, MaxValueAccepted) {
    auto p = ParseNumber("18446744073709551615");
    ASSERT_TRUE(p.ok);
    EXPECT_EQ(p.value, 18446744073709551615ULL);
}

TEST(ParseNumber, OverflowRejected) {
    EXPECT_FALSE(ParseNumber("18446744073709551616").ok);
    EXPECT_FALSE(ParseNumber("99999999999999999999999").ok);
}

TEST(ParseNumber, ZeroRejected) {
    auto p = ParseNumber("0");
    EXPECT_FALSE(p.ok);
    EXPECT_FALSE(p.error.empty());
}

TEST(ParseNumber, EmptyIsNotAnErrorMessage) {
    // Empty box: Go! is disabled but no red text is shown yet.
    auto p = ParseNumber("");
    EXPECT_FALSE(p.ok);
    EXPECT_TRUE(p.error.empty());
}

TEST(ParseNumber, NonDigitsRejected) {
    EXPECT_FALSE(ParseNumber("12a").ok);
    EXPECT_FALSE(ParseNumber("-5").ok);
    EXPECT_FALSE(ParseNumber("1.5").ok);
    EXPECT_FALSE(ParseNumber(",,,").ok);
}
