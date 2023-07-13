#include <gtest/gtest.h>
#include "../include/soupbintcp.h"

TEST(SoupTests, ExtraData)
{
    std::vector<unsigned char> vec{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    soupbintcp::debug_packet dbg;
    EXPECT_EQ(dbg.get_int(soupbintcp::debug_packet::PACKET_LENGTH), soupbintcp::DEBUG_PACKET_LEN - 2);
    dbg.set_message(vec);
    EXPECT_EQ(dbg.get_string(soupbintcp::debug_packet::PACKET_TYPE), "+");
    EXPECT_EQ(dbg.get_int(soupbintcp::debug_packet::PACKET_LENGTH), soupbintcp::DEBUG_PACKET_LEN + 8);
    char expected[] = {0, 0x0B, 43, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    const unsigned char* rec = dbg.get_record();
    for(int i = 0; i < 13; ++i)
        EXPECT_EQ( rec[i], expected[i] );
}