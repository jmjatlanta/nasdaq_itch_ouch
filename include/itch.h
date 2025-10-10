#pragma once
#include <cstdint>
#include <cstring>
#include <climits>
#include <string>
#include <memory>

namespace itch
{

struct message_record {
    enum class field_type {
        ALPHA = 0,
        INTEGER = 1,
        PRICE4 = 2,
        PRICE8 = 3,
    };
    uint8_t offset = 0;
    uint8_t length = 0;
    field_type type;
};

template <typename T>
constexpr T swap_endian_bytes(T value) noexcept {
    static_assert(std::is_integral_v<T> && !std::is_same_v<T, bool>,
                  "byteswap can only be used with integral types (excluding bool)");

    T result = 0;
    for (size_t i = 0; i < sizeof(T); ++i) {
        result |= ((value >> (i * 8)) & 0xFF) << ((sizeof(T) - 1 - i) * 8);
    }
    return result;
}

template<unsigned int SIZE>
struct message {
    char message_type = ' ';
    message(char message_type) : message_type(message_type) 
    {
        record[0] = message_type;
        memset( &record[1], 0, SIZE-1 );
    }
    message(uint8_t* in, bool readOnly = false)
    {
        if (readOnly)
        {
            record = in;
        }
        else
        {
            record = (uint8_t*)malloc(SIZE);
            memcpy(record, in, SIZE);
        }

    }
    message(const uint8_t* in)
    {
        allocated = true;
        memcpy(record, in, SIZE);
    }
    ~message()
    {
        if (allocated)
            free(record);
    }
    constexpr int get_size() const { return SIZE; }
    const uint8_t get_raw_byte(uint8_t pos) const { return record[pos]; }
    void set_raw_byte(uint8_t pos, uint8_t in) { record[pos] = in; }
    int64_t get_int(const message_record& mr) const {
        int64_t retVal = 0;
        // how many bytes to grab
        switch(mr.length)
        {
            case 2:
                retVal = (int64_t)swap_endian_bytes<uint16_t>(*(uint16_t*)&record[mr.offset]);
                break;
            case 4:
                retVal = (int64_t)swap_endian_bytes<uint32_t>(*(uint32_t*)&record[mr.offset]);
                break;
            case 8:
                retVal = (int64_t)swap_endian_bytes<uint64_t>(*(uint64_t*)&record[mr.offset]);
                break;
            default:
                break;
        }
        return retVal;
    }
    void set_int(const message_record& mr, int64_t in)
    {
        int64_t tmp = in;
        switch(mr.length)
        {
            case 1:
                break;
            case 2:
                tmp = swap_endian_bytes<uint16_t>(in);
                break;
            case 4:
                tmp = swap_endian_bytes<uint32_t>(in);
                break;
            case 8:
                tmp = swap_endian_bytes<uint64_t>(in);
                break;
            default:
                break;
        }
        memcpy(&record[mr.offset], &tmp, mr.length);
    }
    void set_string(const message_record& mr, const std::string& in)
    {
        strncpy((char*)&record[mr.offset], in.c_str(), mr.length);
    }
    const std::string get_string(const message_record& mr) const
    {
        // get the section of the record we want
        char buf[mr.length+1];
        memset(buf, 0, mr.length+1);
        strncpy(buf, (char*)&record[mr.offset], mr.length);
        return buf;
    }
    const uint8_t* get_record() const { return record; }
    protected:
    uint8_t* record;
    bool allocated = false;
};

const static int8_t SYSTEM_EVENT_LEN = 12;
struct system_event : public message<SYSTEM_EVENT_LEN> {
    static constexpr message_record MESSAGE_TYPE{0, 1, message_record::field_type::ALPHA};
    static constexpr message_record STOCK_LOCATE{1, 2, message_record::field_type::INTEGER};
    static constexpr message_record TRACKING_NUMBER{3, 2, message_record::field_type::INTEGER};
    static constexpr message_record TIMESTAMP{5, 6, message_record::field_type::INTEGER};
    static constexpr message_record EVENT_CODE{11, 1, message_record::field_type::ALPHA};

    /**
     * Event codes:
     * O Start of messages, outside of time stamp messages, the start of day message is the first message sent in any
     *    trading day
     * S Start of system hours
     * Q Start of market hours
     * M End of market hours
     * E End of system hours
     * C End of messages (last sent in a day)
     */
    system_event() : message('S') { }
    system_event(const uint8_t* in) : message(in) {}
    system_event(uint16_t stock_locate, uint16_t tracking_number, uint64_t timestamp, char event_code) : system_event()
    {
        set_int(STOCK_LOCATE, stock_locate);
        set_int(TRACKING_NUMBER, tracking_number);
        set_int(TIMESTAMP, timestamp);
        set_string(EVENT_CODE, std::to_string(event_code));
    }
};

const static int8_t STOCK_DIRECTORY_LEN = 39;
struct stock_directory : public message<STOCK_DIRECTORY_LEN> {
    static constexpr message_record MESSAGE_TYPE{0, 1, message_record::field_type::ALPHA};
    static constexpr message_record STOCK_LOCATE{1, 2, message_record::field_type::INTEGER};
    static constexpr message_record TRACKING_NUMBER{3, 2, message_record::field_type::INTEGER};
    static constexpr message_record TIMESTAMP{5, 6, message_record::field_type::INTEGER};
    static constexpr message_record STOCK{11, 8, message_record::field_type::ALPHA};
    /***
     * Valid market categories:
     * Q Nasdaq Global Select
     * G Nasdaq Global Market
     * S Nasdaq Capital Market
     * N NYSE
     * A American
     * P ARCA
     * Z Bats Z
     * V Investors Exchange LLC
     * (space) Not available
     */
    static constexpr message_record MARKET_CATEGORY{19, 1, message_record::field_type::ALPHA};
    /***
     * Valid indicators:
     * D Deficient
     * E Delinquent
     * Q Bankrupt
     * S Suspended
     * G Deficient and bankrupt
     * H Deficient and delinquent
     * J Delincuent and bankrupt
     * K Deficient, Delinquent, and bankrupt
     * C Creations and/or redemptions suspended for exchange traded product
     * N Normal
     * (space) Not available (used for non-nasdaq)
     */
    static constexpr message_record FINANCIAL_STATUS_INDICATOR{20, 1, message_record::field_type::ALPHA};
    static constexpr message_record ROUND_LOT_SIZE{21, 4, message_record::field_type::INTEGER};
    static constexpr message_record ROUND_LOTS_ONLY{25, 1, message_record::field_type::ALPHA};
    static constexpr message_record ISSUE_CLASSIFICATION{26, 1, message_record::field_type::ALPHA};
    static constexpr message_record ISSUE_SUB_TYPE{27, 2, message_record::field_type::ALPHA};
    /***
     * P Live/production
     * T Test
     */
    static constexpr message_record AUTHENTICITY{29, 1, message_record::field_type::ALPHA};
    /***
     * Y restricted under 203(b)(3)
     * N Not restricted
     * (space) unknown
     */
    static constexpr message_record SHORT_SALE_THRESHOLD_INDICATOR{30, 1, message_record::field_type::ALPHA};
    /***
     * Y / N
     */
    static constexpr message_record IPO_FLAG{31, 1, message_record::field_type::ALPHA};
    /***
     * 1 Tier 1
     * 2 Tier 2
     * (space) Not available
     */
    static constexpr message_record LULDREFERENCE_PRICE_TIER{32, 1, message_record::field_type::ALPHA};
    /***
     * Y / N / (space)
     */
    static constexpr message_record ETP_FLAG{33, 1, message_record::field_type::ALPHA};
    /***
     * Note: always round this down
     */
    static constexpr message_record ETP_LEVERAGE_FACTOR{34, 4, message_record::field_type::INTEGER};
    /***
     * ETP is an inverse ETP (Y/N) i.e. if Y, the factor above should be treated as negative
     */
    static constexpr message_record INVERSE_INDICATOR{38, 1, message_record::field_type::ALPHA};
    
    stock_directory() : message('R') {}
    stock_directory(const uint8_t* in) : message(in) {}
    stock_directory(uint16_t stock_locate, uint16_t tracking_number, uint64_t timestamp, const std::string& stock,
            char market_category,
            char financial_status_indicator, uint32_t round_lot_size, char round_lots_only, char issue_classification,
            const std::string& issue_sub_type, char authenticity, char short_sale_threshold_indicator, char ipo_flag,
            char luldreference_price_tier, char etp_flag, uint32_t etp_leverage_factor, char inverse_indicator)
            : stock_directory()
    {
        set_int(STOCK_LOCATE, stock_locate);
        set_int(TRACKING_NUMBER, tracking_number);
        set_int(TIMESTAMP, timestamp);
        set_string(STOCK, stock);
        set_string(MARKET_CATEGORY, std::to_string(market_category)); // important
        set_string(FINANCIAL_STATUS_INDICATOR, std::to_string(financial_status_indicator)); // important
        set_int(ROUND_LOT_SIZE, round_lot_size);
        set_string(ROUND_LOTS_ONLY, std::to_string(round_lots_only));
        set_string(ISSUE_CLASSIFICATION, std::to_string(issue_classification));
        set_string(ISSUE_SUB_TYPE, issue_sub_type);
        set_string(AUTHENTICITY, std::to_string(authenticity));
        set_string(SHORT_SALE_THRESHOLD_INDICATOR, std::to_string(short_sale_threshold_indicator));
        set_string(IPO_FLAG, std::to_string(ipo_flag));
        set_string(LULDREFERENCE_PRICE_TIER, std::to_string(luldreference_price_tier));
        set_string(ETP_FLAG, std::to_string(etp_flag));
        set_int(ETP_LEVERAGE_FACTOR, etp_leverage_factor);
        set_string(INVERSE_INDICATOR, std::to_string(inverse_indicator));
    }
};

const static int8_t STOCK_TRADING_ACTION_LEN = 25;
struct stock_trading_action : public message<STOCK_TRADING_ACTION_LEN> {
    static constexpr message_record MESSAGE_TYPE{0, 1, message_record::field_type::ALPHA};
    static constexpr message_record STOCK_LOCATE{1, 2, message_record::field_type::INTEGER};
    static constexpr message_record TRACKING_NUMBER{3, 2, message_record::field_type::INTEGER};
    static constexpr message_record TIMESTAMP{5, 6, message_record::field_type::INTEGER};
    static constexpr message_record STOCK{11, 8, message_record::field_type::ALPHA};
    /***
     * H Halted across all markets
     * P Paused across all markets
     * Q Quotation only period
     * T Trading
     */
    static constexpr message_record TRADING_STATE{19, 1, message_record::field_type::ALPHA};
    static constexpr message_record RESERVED{20, 1, message_record::field_type::ALPHA};
    static constexpr message_record REASON{21, 4, message_record::field_type::ALPHA};

    stock_trading_action() : message('H') {}
    stock_trading_action(const uint8_t* in) : message(in) {}
    stock_trading_action(uint16_t stock_locate, uint16_t tracking_number, uint64_t timestamp, const std::string& stock,
            char trading_state, char reserved, const std::string& reason) : stock_trading_action()
    {
        set_int(STOCK_LOCATE, stock_locate);
        set_int(TRACKING_NUMBER, tracking_number);
        set_int(TIMESTAMP, timestamp);
        set_string(STOCK, stock);
        set_string(TRADING_STATE, std::to_string(trading_state));
        set_string(RESERVED, std::to_string(reserved));
        set_string(REASON, reason);
    }
};

const static int8_t REG_SHO_RESTRICTION_LEN = 20;
struct reg_sho_restriction : public message<REG_SHO_RESTRICTION_LEN> {
    static constexpr message_record MESSAGE_TYPE{0, 1, message_record::field_type::ALPHA};
    static constexpr message_record STOCK_LOCATE{1, 2, message_record::field_type::INTEGER};
    static constexpr message_record TRACKING_NUMBER{3, 2, message_record::field_type::INTEGER};
    static constexpr message_record TIMESTAMP{5, 6, message_record::field_type::INTEGER};
    static constexpr message_record STOCK{11, 8, message_record::field_type::ALPHA};
    static constexpr message_record REG_SHO_ACTION{19, 1, message_record::field_type::ALPHA};

    reg_sho_restriction() : message('Y') {}
    reg_sho_restriction(const uint8_t* in) : message(in) {}
};

const static int8_t MARKET_PARTICIPANT_POSITION_LEN = 26;
struct market_participant_position : public message<MARKET_PARTICIPANT_POSITION_LEN> { 
    static constexpr message_record MESSAGE_TYPE{0, 1, message_record::field_type::ALPHA}; 
    static constexpr message_record STOCK_LOCATE{1, 2, message_record::field_type::INTEGER};
    static constexpr message_record TRACKING_NUMBER{3, 2, message_record::field_type::INTEGER};
    static constexpr message_record TIMESTAMP{5, 6, message_record::field_type::INTEGER};
    static constexpr message_record MPID{11, 4, message_record::field_type::ALPHA};
    static constexpr message_record STOCK{15, 8, message_record::field_type::ALPHA};
    /***
     * Y/N
     */
    static constexpr message_record PRIMARY_MARKET_MAKER{23, 1, message_record::field_type::ALPHA};
    /****
     * N Normal
     * P Passive
     * S Syndicate
     * R Pre-syndicate
     * L Penalty
     */
    static constexpr message_record MARKET_MAKER_MODE{24, 1, message_record::field_type::ALPHA};
    /***
     * A Active
     * E Excused/withdrawn
     * W Withdrawn
     * S Suspended
     * D Deleted
     */
    static constexpr message_record MARKET_PARTICIPANT_STATE{25, 1, message_record::field_type::ALPHA};

    market_participant_position() : message('L') {}
    market_participant_position(const uint8_t* in) : message(in) {}
};

const static int8_t MWCP_DECLINE_LEVEL_LEN = 35;
struct mwcp_decline_level : public message<MWCP_DECLINE_LEVEL_LEN> {
    static constexpr message_record MESSAGE_TYPE{0, 1, message_record::field_type::ALPHA}; 
    static constexpr message_record STOCK_LOCATE{1, 2, message_record::field_type::ALPHA}; 
    static constexpr message_record TRACKING_NUMBER{3, 2, message_record::field_type::INTEGER};
    static constexpr message_record TIMESTAMP{5, 6, message_record::field_type::INTEGER};
    static constexpr message_record LEVEL_1{11, 8, message_record::field_type::PRICE8};
    static constexpr message_record LEVEL_2{19, 8, message_record::field_type::PRICE8};
    static constexpr message_record LEVEL_3{27, 8, message_record::field_type::PRICE8};

    mwcp_decline_level() : message('V') {}
    mwcp_decline_level(const uint8_t* in) : message(in) {}
};

const static int8_t MWCP_STATUS_LEN = 12;
struct mwcp_status : public message<MWCP_STATUS_LEN> {
    static constexpr message_record MESSAGE_TYPE{0, 1, message_record::field_type::ALPHA}; 
    static constexpr message_record STOCK_LOCATE{1, 2, message_record::field_type::ALPHA}; 
    static constexpr message_record TRACKING_NUMBER{3, 2, message_record::field_type::INTEGER};
    static constexpr message_record TIMESTAMP{5, 6, message_record::field_type::INTEGER};
    static constexpr message_record BREACHED_LEVEL{11, 1, message_record::field_type::ALPHA};

    mwcp_status() : message('W') {}
    mwcp_status(const uint8_t* in) : message(in) {}
};

const static int8_t IPO_QUOTING_PERIOD_UPDATE_LEN = 28;
struct ipo_quoting_period_update : public message<IPO_QUOTING_PERIOD_UPDATE_LEN> {
    static constexpr message_record MESSAGE_TYPE{0, 1, message_record::field_type::ALPHA}; 
    static constexpr message_record STOCK_LOCATE{1, 2, message_record::field_type::ALPHA}; 
    static constexpr message_record TRACKING_NUMBER{3, 2, message_record::field_type::INTEGER};
    static constexpr message_record TIMESTAMP{5, 6, message_record::field_type::INTEGER};
    static constexpr message_record STOCK{11, 8, message_record::field_type::ALPHA};
    static constexpr message_record IPO_QUOTATION_RELEASE_TIME{19, 4, message_record::field_type::INTEGER};
    static constexpr message_record IPO_QUOTATION_RELEASE_QUALIFIER{23, 1, message_record::field_type::ALPHA};
    static constexpr message_record IPO_PRICE{24, 4, message_record::field_type::PRICE4};

    ipo_quoting_period_update() : message('K') {}
    ipo_quoting_period_update(const uint8_t* in) : message(in) {}
};
    
const static int8_t LULD_AUCTION_COLLAR_LEN = 35;
struct luld_auction_collar : public message<LULD_AUCTION_COLLAR_LEN> {
    static constexpr message_record MESSAGE_TYPE{0, 1, message_record::field_type::ALPHA}; 
    static constexpr message_record STOCK_LOCATE{1, 2, message_record::field_type::ALPHA}; 
    static constexpr message_record TRACKING_NUMBER{3, 2, message_record::field_type::INTEGER};
    static constexpr message_record TIMESTAMP{5, 6, message_record::field_type::INTEGER};
    static constexpr message_record STOCK{11, 8, message_record::field_type::ALPHA};
    static constexpr message_record AUCTION_COLLAR_REFERENCE_PRICE{19, 4, message_record::field_type::PRICE4};
    static constexpr message_record UPPER_AUCTION_COLLAR_PRICE{23, 4, message_record::field_type::PRICE4};
    static constexpr message_record LOWER_AUCTION_COLLAR_PRICE{27, 4, message_record::field_type::PRICE4};
    static constexpr message_record AUCTION_COLLAR_EXTENSION{31, 4, message_record::field_type::INTEGER};

    luld_auction_collar() : message('J') {}
    luld_auction_collar(const uint8_t* in) : message(in) {}
};

const static int8_t OPERATIONAL_HALT_LEN = 21;
struct operational_halt : public message<OPERATIONAL_HALT_LEN> {
    static constexpr message_record MESSAGE_TYPE{0, 1, message_record::field_type::ALPHA}; 
    static constexpr message_record STOCK_LOCATE{1, 2, message_record::field_type::ALPHA}; 
    static constexpr message_record TRACKING_NUMBER{3, 2, message_record::field_type::INTEGER};
    static constexpr message_record TIMESTAMP{5, 6, message_record::field_type::INTEGER};
    static constexpr message_record STOCK{11, 8, message_record::field_type::ALPHA};
    static constexpr message_record MARKET_CODE{19, 1, message_record::field_type::ALPHA};
    static constexpr message_record OPERATIONAL_HALT_ACTION{20, 1, message_record::field_type::ALPHA};

    operational_halt() : message('h') {}
    operational_halt(const uint8_t* in) : message(in) {}
};

const static int8_t ADD_ORDER_LEN = 36;
struct add_order : public message<ADD_ORDER_LEN> {
    static constexpr message_record MESSAGE_TYPE{0, 1, message_record::field_type::ALPHA}; 
    static constexpr message_record STOCK_LOCATE{1, 2, message_record::field_type::ALPHA}; 
    static constexpr message_record TRACKING_NUMBER{3, 2, message_record::field_type::INTEGER};
    static constexpr message_record TIMESTAMP{5, 6, message_record::field_type::INTEGER};
    static constexpr message_record ORDER_REFERENCE_NUMBER{11, 8, message_record::field_type::INTEGER};
    static constexpr message_record BUY_SELL_INDICATOR{19, 1, message_record::field_type::ALPHA};
    static constexpr message_record SHARES{20, 4, message_record::field_type::INTEGER};
    static constexpr message_record STOCK{24, 8, message_record::field_type::ALPHA};
    static constexpr message_record PRICE{32, 4, message_record::field_type::PRICE4};

    add_order() : message('A') {}
    add_order(const uint8_t* in) : message(in) {}
};

/****
 * The more common order notification
 */
const static int8_t ADD_ORDER_WITH_MPID_LEN = 40;
struct add_order_with_mpid : public message<ADD_ORDER_WITH_MPID_LEN> {
    static constexpr message_record MESSAGE_TYPE{0, 1, message_record::field_type::ALPHA}; 
    static constexpr message_record STOCK_LOCATE{1, 2, message_record::field_type::ALPHA}; 
    static constexpr message_record TRACKING_NUMBER{3, 2, message_record::field_type::INTEGER};
    static constexpr message_record TIMESTAMP{5, 6, message_record::field_type::INTEGER};
    /***
     * Assigned at order creation
     */
    static constexpr message_record ORDER_REFERENCE_NUMBER{11, 8, message_record::field_type::INTEGER};
    /***
     * B/S
     */
    static constexpr message_record BUY_SELL_INDICATOR{19, 1, message_record::field_type::ALPHA};
    static constexpr message_record SHARES{20, 4, message_record::field_type::INTEGER};
    static constexpr message_record STOCK{24, 8, message_record::field_type::ALPHA};
    static constexpr message_record PRICE{32, 4, message_record::field_type::PRICE4};
    static constexpr message_record ATTRIBUTION{36, 4, message_record::field_type::ALPHA};

    add_order_with_mpid() : message('F') {}
    add_order_with_mpid(const uint8_t* in) : message(in) {}
    uint64_t order_reference_number() const { return get_int(ORDER_REFERENCE_NUMBER); };
    std::string attribution() const { return get_string(ATTRIBUTION); }
};

const static int8_t ORDER_EXECUTED_LEN = 31;
struct order_executed : public message<ORDER_EXECUTED_LEN> {
    static constexpr message_record MESSAGE_TYPE{0, 1, message_record::field_type::ALPHA}; 
    static constexpr message_record STOCK_LOCATE{1, 2, message_record::field_type::ALPHA}; 
    static constexpr message_record TRACKING_NUMBER{3, 2, message_record::field_type::INTEGER};
    static constexpr message_record TIMESTAMP{5, 6, message_record::field_type::INTEGER};
    static constexpr message_record ORDER_REFERENCE_NUMBER{11, 8, message_record::field_type::INTEGER};
    static constexpr message_record EXECUTED_SHARES{19, 4, message_record::field_type::INTEGER};
    static constexpr message_record MATCH_NUMBER{23, 8, message_record::field_type::INTEGER};

    order_executed() : message('E') {}
    order_executed(const uint8_t* in) : message(in) {}
};

const static int8_t ORDER_EXECUTED_WITH_PRICE_LEN = 36;
struct order_executed_with_price : public message<ORDER_EXECUTED_WITH_PRICE_LEN> {
    static constexpr message_record MESSAGE_TYPE{0, 1, message_record::field_type::ALPHA}; 
    static constexpr message_record STOCK_LOCATE{1, 2, message_record::field_type::ALPHA}; 
    static constexpr message_record TRACKING_NUMBER{3, 2, message_record::field_type::INTEGER};
    static constexpr message_record TIMESTAMP{5, 6, message_record::field_type::INTEGER};
    static constexpr message_record ORDER_REFERENCE_NUMBER{11, 8, message_record::field_type::INTEGER};
    static constexpr message_record EXECUTED_SHARES{19, 4, message_record::field_type::INTEGER};
    static constexpr message_record MATCH_NUMBER{23, 8, message_record::field_type::INTEGER};
    static constexpr message_record PRINTABLE{31, 1, message_record::field_type::ALPHA};
    static constexpr message_record EXECUTION_PRICE{32, 4, message_record::field_type::PRICE4};

    order_executed_with_price() : message('C') {}
    order_executed_with_price(const uint8_t* in) : message(in) {}
};

const static int8_t ORDER_CANCEL_LEN = 23;
struct order_cancel : public message<ORDER_CANCEL_LEN> {
    static constexpr message_record MESSAGE_TYPE{0, 1, message_record::field_type::ALPHA}; 
    static constexpr message_record STOCK_LOCATE{1, 2, message_record::field_type::ALPHA}; 
    static constexpr message_record TRACKING_NUMBER{3, 2, message_record::field_type::INTEGER};
    static constexpr message_record TIMESTAMP{5, 6, message_record::field_type::INTEGER};
    static constexpr message_record ORDER_REFERENCE_NUMBER{11, 8, message_record::field_type::INTEGER};
    static constexpr message_record CANCELLED_SHARES{19, 4, message_record::field_type::INTEGER};

    order_cancel() : message('X') {}
    order_cancel(const uint8_t* in) : message(in) {}
};

const static int8_t ORDER_DELETE_LEN = 19;
struct order_delete : public message<ORDER_DELETE_LEN> {
    static constexpr message_record MESSAGE_TYPE{0, 1, message_record::field_type::ALPHA}; 
    static constexpr message_record STOCK_LOCATE{1, 2, message_record::field_type::ALPHA}; 
    static constexpr message_record TRACKING_NUMBER{3, 2, message_record::field_type::INTEGER};
    static constexpr message_record TIMESTAMP{5, 6, message_record::field_type::INTEGER};
    static constexpr message_record ORDER_REFERENCE_NUMBER{11, 8, message_record::field_type::INTEGER};

    order_delete() : message('D') {}
    order_delete(const uint8_t* in) : message(in) {}
};

const static int8_t ORDER_REPLACE_LEN = 35;
struct order_replace : public message<ORDER_REPLACE_LEN> {
    static constexpr message_record MESSAGE_TYPE{0, 1, message_record::field_type::ALPHA}; 
    static constexpr message_record STOCK_LOCATE{1, 2, message_record::field_type::ALPHA}; 
    static constexpr message_record TRACKING_NUMBER{3, 2, message_record::field_type::INTEGER};
    static constexpr message_record TIMESTAMP{5, 6, message_record::field_type::INTEGER};
    static constexpr message_record ORIGINAL_ORDER_REFERENCE_NUMBER{11, 8, message_record::field_type::INTEGER};
    static constexpr message_record NEW_ORDER_REFERENCE_NUMBER{19, 8, message_record::field_type::INTEGER};
    static constexpr message_record SHARES{27, 4, message_record::field_type::INTEGER};
    static constexpr message_record PRICE{31, 4, message_record::field_type::PRICE4};

    order_replace() : message('U') {}
    order_replace(const uint8_t* in) : message(in) {}
};

const static int8_t TRADE_LEN = 44;
struct trade : public message<TRADE_LEN> {
    static constexpr message_record MESSAGE_TYPE{0, 1, message_record::field_type::ALPHA}; 
    static constexpr message_record STOCK_LOCATE{1, 2, message_record::field_type::ALPHA}; 
    static constexpr message_record TRACKING_NUMBER{3, 2, message_record::field_type::INTEGER};
    static constexpr message_record TIMESTAMP{5, 6, message_record::field_type::INTEGER};
    static constexpr message_record ORDER_REFERENCE_NUMBER{11, 8, message_record::field_type::INTEGER};
    static constexpr message_record BUY_SELL_INDICATOR{19, 1, message_record::field_type::ALPHA};
    static constexpr message_record SHARES{20, 4, message_record::field_type::INTEGER};
    static constexpr message_record STOCK{24, 8, message_record::field_type::ALPHA};
    static constexpr message_record PRICE{32, 4, message_record::field_type::PRICE4};
    static constexpr message_record MATCH_NUMBER{36, 8, message_record::field_type::INTEGER};

    trade() : message('P') {}
    trade(const uint8_t* in) : message(in) {}
};

const static int8_t CROSS_TRADE_LEN = 40;
struct cross_trade : public message<CROSS_TRADE_LEN> {
    static constexpr message_record MESSAGE_TYPE{0, 1, message_record::field_type::ALPHA}; 
    static constexpr message_record STOCK_LOCATE{1, 2, message_record::field_type::ALPHA}; 
    static constexpr message_record TRACKING_NUMBER{3, 2, message_record::field_type::INTEGER};
    static constexpr message_record TIMESTAMP{5, 6, message_record::field_type::INTEGER};
    static constexpr message_record SHARES{11, 4, message_record::field_type::INTEGER};
    static constexpr message_record STOCK{19, 8, message_record::field_type::ALPHA};
    static constexpr message_record CROSS_PRICE{27, 4, message_record::field_type::PRICE4};
    static constexpr message_record MATCH_NUMBER{31, 8, message_record::field_type::INTEGER};
    static constexpr message_record CROSS_TYPE{39, 1, message_record::field_type::ALPHA};

    cross_trade() : message('Q') {}
    cross_trade(const uint8_t* in) : message(in) {}
};

const static int8_t BROKEN_TRADE_LEN = 19;
struct broken_trade : public message<BROKEN_TRADE_LEN> {
    static constexpr message_record MESSAGE_TYPE{0, 1, message_record::field_type::ALPHA}; 
    static constexpr message_record STOCK_LOCATE{1, 2, message_record::field_type::ALPHA}; 
    static constexpr message_record TRACKING_NUMBER{3, 2, message_record::field_type::INTEGER};
    static constexpr message_record TIMESTAMP{5, 6, message_record::field_type::INTEGER};
    static constexpr message_record MATCH_NUMBER{11, 8, message_record::field_type::INTEGER};

    broken_trade() : message('B') {}
    broken_trade(const uint8_t* in) : message(in) {}
};

const static int8_t NOII_LEN = 50;
struct noii : public message<NOII_LEN> {
    static constexpr message_record MESSAGE_TYPE{0, 1, message_record::field_type::ALPHA}; 
    static constexpr message_record STOCK_LOCATE{1, 2, message_record::field_type::ALPHA}; 
    static constexpr message_record TRACKING_NUMBER{3, 2, message_record::field_type::INTEGER};
    static constexpr message_record TIMESTAMP{5, 6, message_record::field_type::INTEGER};
    static constexpr message_record PAIRED_SHARES{11, 8, message_record::field_type::INTEGER};
    static constexpr message_record IMBALANCE_SHARES{19, 8, message_record::field_type::INTEGER};
    static constexpr message_record IMBALANCE_DIRECTION{27, 1, message_record::field_type::ALPHA};
    static constexpr message_record STOCK{28, 8, message_record::field_type::ALPHA};
    static constexpr message_record FAR_PRICE{36, 4, message_record::field_type::PRICE4};
    static constexpr message_record NEAR_PRICE{40, 4, message_record::field_type::PRICE4};
    static constexpr message_record CURRENT_REFERENCE_PRICE{44, 4, message_record::field_type::PRICE4};
    static constexpr message_record CROSS_TYPE{48, 1, message_record::field_type::ALPHA};
    static constexpr message_record PRICE_VARIATION_INDICATOR{49, 1, message_record::field_type::ALPHA};

    noii() : message('I') {}
    noii(const uint8_t* in) : message(in) {}
};

const static int8_t RPII_LEN = 20;
struct rpii : public message<RPII_LEN> {
    static constexpr message_record MESSAGE_TYPE{0, 1, message_record::field_type::ALPHA}; 
    static constexpr message_record STOCK_LOCATE{1, 2, message_record::field_type::ALPHA}; 
    static constexpr message_record TRACKING_NUMBER{3, 2, message_record::field_type::INTEGER};
    static constexpr message_record TIMESTAMP{5, 6, message_record::field_type::INTEGER};
    static constexpr message_record STOCK{11, 8, message_record::field_type::ALPHA};
    static constexpr message_record INTEREST_FLAG{19, 1, message_record::field_type::ALPHA};

    rpii() : message('N') {}
    rpii(const uint8_t* in) : message(in) {}
};

const static int8_t DIRECT_LISTING_WITH_CAPITAL_RAISE_PRICE_DISCOVERY_LEN = 48;
struct direct_listing_with_capital_raise_price_discovery : 
        public message<DIRECT_LISTING_WITH_CAPITAL_RAISE_PRICE_DISCOVERY_LEN> {
    static constexpr message_record MESSAGE_TYPE{0, 1, message_record::field_type::ALPHA}; 
    static constexpr message_record STOCK_LOCATE{1, 2, message_record::field_type::ALPHA}; 
    static constexpr message_record TRACKING_NUMBER{3, 2, message_record::field_type::INTEGER};
    static constexpr message_record TIMESTAMP{5, 6, message_record::field_type::INTEGER};
    static constexpr message_record STOCK{11, 8, message_record::field_type::ALPHA};
    static constexpr message_record OPEN_ELIGIBILITY_STATUS{19, 1, message_record::field_type::ALPHA};
    static constexpr message_record MINIMUM_ALLOWABLE_PRICE{20, 4, message_record::field_type::PRICE4};
    static constexpr message_record MAXIMUM_ALLOWABLE_PRICE{24, 4, message_record::field_type::PRICE4};
    static constexpr message_record NEAR_EXECUTION_PRICE{28, 4, message_record::field_type::PRICE4};
    static constexpr message_record NEAR_EXECUTION_TIME{32, 8, message_record::field_type::INTEGER};
    static constexpr message_record LOWER_PRICE_RANGE_COLLAR{40, 4, message_record::field_type::PRICE4};
    static constexpr message_record UPPER_PRICE_RANGE_COLLAR{44, 4, message_record::field_type::PRICE4};

    direct_listing_with_capital_raise_price_discovery() : message('O') {}
    direct_listing_with_capital_raise_price_discovery(const uint8_t* in) : message(in) {}
};

} // end namespace itch
