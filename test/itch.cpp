#include "itch.h"
#include <gtest/gtest.h>
#include <sstream>
#include <fstream>
#include <filesystem>

TEST(itch, swap)
{
    uint16_t in16 = 0x1234;
    uint16_t out16 = itch::swap_endian_bytes<uint16_t>(in16);
    EXPECT_EQ(out16, 0x3412);
    uint32_t in32 = 0x12345678;
    uint32_t out32 = itch::swap_endian_bytes<uint32_t>(in32);
    EXPECT_EQ(out32, 0x78563412);
    uint64_t in64 = 0x0123456789ABCDEF;
    uint64_t out64 = itch::swap_endian_bytes<uint64_t>(in64);
    EXPECT_EQ(out64, 0xEFCDAB8967452301);
}

TEST(itch, test1)
{
    const uint8_t arr[] = { 'S', 0x01, 0x00, 0x02, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 'O'};
    itch::system_event msg(arr);
    const uint8_t* record = msg.get_record();
    for(int i = 0; i < 12; ++i)
        EXPECT_EQ(arr[i], record[i]);
    EXPECT_EQ(msg.get_raw_byte(0), 'S');
    EXPECT_EQ(msg.get_raw_byte(1), 0x01);
    EXPECT_EQ(msg.get_raw_byte(2), 0x00);
    EXPECT_EQ(msg.get_raw_byte(3), 0x02);
}

TEST(itch, construct)
{
    itch::system_event msg;
    for(int i = 1; i < itch::SYSTEM_EVENT_LEN; ++i)
        EXPECT_EQ(msg.get_raw_byte(i), 0);
    msg.set_int(msg.STOCK_LOCATE, 1);
    msg.set_int(msg.TRACKING_NUMBER, 2);
    msg.set_int(msg.TIMESTAMP, 6);
    msg.set_string(msg.EVENT_CODE, "O");
    const uint8_t* record = msg.get_record();
    const uint8_t arr[] = { 'S', 0x00, 0x01, 0x00, 0x02, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 'O'};
    EXPECT_EQ(arr[0], record[0]);
    EXPECT_EQ(arr[1], record[1]);
    EXPECT_EQ(arr[2], record[2]);
    for(int i = 0; i < 12; ++i)
        EXPECT_EQ(arr[i], record[i]);
    EXPECT_EQ(msg.get_int(msg.STOCK_LOCATE), 1);
    EXPECT_EQ(msg.get_string(msg.EVENT_CODE), "O");
    EXPECT_EQ(msg.get_raw_byte(1), 0x00);
    EXPECT_EQ(msg.get_raw_byte(2), 0x01);
}

TEST(itch, largeNumbers)
{
    /*
    POS Field
    0   Message Type
    1   Stock Locate
    3   Tracking number
    5   Timestamp
    11  Order reference number
    19  BUY_SELL_INDICATOR
    20  Shares
    24  Stock
    32  Price (4 bytes)
    */
    itch::add_order msg;
    int32_t price1 = 1000;
    msg.set_int(itch::add_order::PRICE, price1);
    int sz = msg.get_size();
    const uint8_t* record = msg.get_record();
    int32_t price2 = msg.get_int(itch::add_order::PRICE);
    EXPECT_EQ(price1, price2);
}

TEST(itch, parseFile)
{
    std::string fileName = "/Users/jmjatlanta/Downloads/12302019.NASDAQ_ITCH50";
    EXPECT_TRUE(std::filesystem::exists(fileName));
    std::ifstream in(fileName.c_str(), std::ios::binary | std::ios_base::in);
    if ( (in.rdstate() & std::ifstream::failbit) != 0)
    {
        std::cerr << "Unable to read file " << fileName << ": opening failed\n";
        FAIL();
    }
    std::vector<char> types{'S', 'R', 'H', 'Y', 'L', 'V', 'W', 
            'K', 'J', 'h', 'A', 'F', 'E', 'C', 'X', 'D', 'U', 
            'P', 'Q',  'B', 'I', 'N', 'O'};
    std::vector<int> sizes{ 
            itch::system_event().get_size(),  // S
            itch::stock_directory().get_size(), // R
            itch::stock_trading_action().get_size(), // H
            itch::reg_sho_restriction().get_size(), // Y
            itch::market_participant_position().get_size(), // L
            itch::mwcp_decline_level().get_size(), // V
            itch::mwcp_status().get_size(), // W
            itch::ipo_quoting_period_update().get_size(), // K
            itch::luld_auction_collar().get_size(), // J
            itch::operational_halt().get_size(), // h
            itch::add_order().get_size(), // A
            itch::add_order_with_mpid().get_size(), // F
            itch::order_executed().get_size(), // E
            itch::order_executed_with_price().get_size(), // C
            itch::order_cancel().get_size(), // X
            itch::order_delete().get_size(), // D
            itch::order_replace().get_size(), // U
            itch::trade().get_size(), // P
            itch::cross_trade().get_size(), // Q
            itch::broken_trade().get_size(), // B
            itch::noii().get_size(), // I
            itch::rpii().get_size(), // N
            itch::direct_listing_with_capital_raise_price_discovery().get_size()
    };
    std::map<char, int> record_sizes; // stores the record size for each type
    size_t max_size = 0;
    for(int i = 0; i < types.size(); ++i)
    {
        size_t curr_size = sizes[i];
        record_sizes[types[i]] = curr_size;
        if (curr_size > max_size)
            max_size = curr_size;
    }
    std::map<char, int> counts;
    // grab first character and then the rest of the record
    char* buf = (char*) malloc(max_size);
    char record_type;
    uint64_t record_number = 0;
    uint16_t incoming_size = 0;
    while(true)
    {
        in.read((char*)&incoming_size, 2);
        if ( (in.rdstate() & std::ifstream::eofbit) != 0)
            break;
        if ( (in.rdstate() & std::ifstream::failbit) != 0)
        {
            std::cerr << "Unable to read record " << record_number << ": reading of record size failed\n";
            FAIL();
        }
        incoming_size = itch::swap_endian_bytes(incoming_size);
        in.read(&record_type, 1);
        if ( (in.rdstate() & std::ifstream::eofbit) != 0)
            break;
        if ( (in.rdstate() & std::ifstream::failbit) != 0)
        {
            std::cerr << "Unable to read record " << record_number << ": reading of record type failed\n";
            FAIL();
        }
        if (record_sizes.find(record_type) == record_sizes.end())
        {
            std::cerr << "Unable to read record " << record_number << ": Invalid record type " << record_type << std::endl;
            FAIL();
        }
        if (record_sizes[record_type] != incoming_size)
        {
            std::cerr << "Size mismatch on record " << record_number << ": They say " << incoming_size << " and we say " 
                    << record_sizes[record_type] << " for record type " << record_type << "\n";
            FAIL();
        }
        counts[record_type]++;
        in.read(buf, incoming_size - 1);
        if ( (in.rdstate() & std::ifstream::eofbit) != 0)
            break;
        if ( (in.rdstate() & std::ifstream::failbit) != 0)
        {
            std::cerr << "Unable to read record " << record_number << ": reading of record failed\n";
            FAIL();
        }
        record_number++;
    }
    for(auto& i : counts)
    {
        std::cout << "Record " << i.first << ": " << i.second << "\n";
    }
    free(buf);
}
