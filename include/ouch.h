#pragma once
#include <cstdint>
#include <cstring>
#include <climits>
#include <string>
#include <memory>
#include <vector>

namespace ouch
{

template <typename T>
T swap_endian_bytes(T in)
{
    static_assert(CHAR_BIT == 8, "CHAR_BIT != 8");
    union
    {
        T u;
        unsigned char u8[sizeof(T)];
    } source, dest;
    source.u = in;

    for(size_t k = 0; k < sizeof(T); k++)
        dest.u8[k] = source.u8[sizeof(T) - k - 1];
    return dest.u;
}

struct message_record {
    enum class field_type {
        ALPHA = 0,
        INTEGER = 1,
        PRICE4 = 2,
        PRICE8 = 3,
        USER_REF_NUM = 4,
        VAR = 5,
        SIGNED = 6,
    };
    uint8_t offset = 0;
    uint8_t length = 0;
    field_type type;
};

/*****
 * Optional Fields
 * These can appear in appendages to many of the messages sent and received
 */

struct tag_value {
    uint8_t length = 0;
    uint8_t option_tag = 0;
    char* value;
};

struct tag_record {
    enum class tag_name : uint8_t {
        UNUSED0 = 0,
        SECONDARY_ORD_REF_NUM = 1,
        FIRM = 2,
        MIN_QTY = 3,
        CUSTOMER_TYPE = 4,
        MAX_FLOOR = 5,
        PRICE_TYPE = 6,
        PEG_OFFSET = 7,
        UNUSED8 = 8,
        DISCRETION_PRICE = 9,
        DISCRETION_PRICE_TYPE = 10,
        DISCRETION_PEG_OFFSET = 11,
        POST_ONLY = 12,
        RANDOM_RESERVES = 13,
        ROUTE = 14,
        EXPIRE_TIME = 15,
        TRADE_NOW = 16,
        HANDLE_INST = 17,
        BBO_WEIGHT_INDICATOR = 18,
    };
    uint8_t length = 0;
    message_record::field_type type;
};

template <typename E>
constexpr typename std::underlying_type<E>::type to_underlying(E e) noexcept {
    return static_cast<typename std::underlying_type<E>::type>(e);
}

static tag_record tag_records[] = {
    {0, message_record::field_type::ALPHA}, // unused0
    {8, message_record::field_type::INTEGER},
    {4, message_record::field_type::ALPHA},
    {4, message_record::field_type::INTEGER},
    {1, message_record::field_type::ALPHA},
    {4, message_record::field_type::INTEGER},
    {1, message_record::field_type::ALPHA},
    {4, message_record::field_type::SIGNED},
    {0, message_record::field_type::ALPHA}, // unused8
    {8, message_record::field_type::INTEGER},
    {1, message_record::field_type::ALPHA},
    {4, message_record::field_type::SIGNED},
    {1, message_record::field_type::ALPHA},
    {4, message_record::field_type::INTEGER},
    {4, message_record::field_type::ALPHA},
    {4, message_record::field_type::INTEGER},
    {1, message_record::field_type::ALPHA},
    {1, message_record::field_type::ALPHA},
    {1, message_record::field_type::ALPHA},
};

template<unsigned int SIZE>
struct message {
    const char message_type = ' ';
    const message_record* variable_field_record = nullptr;
    message(char message_type, const message_record* variable_field) 
            : message_type(message_type), variable_field_record(variable_field)
    {
        record[0] = message_type;
        memset( &record[1], 0, SIZE-1 );
    }
    message(const char* in, const message_record* variable_field) 
            : message_type(in[0]), variable_field_record(variable_field)
    {
        memcpy(record, in, SIZE);
    }
    ~message() {
        if (tag_values != nullptr)
            free(tag_values);
    }
    void set_tag_values(char* in) {
        if (variable_field_record != nullptr)
        {
            uint64_t sz = get_int(*variable_field_record); tag_values = (char*)malloc(sz); memcpy(tag_values, in, sz);
        }
    }
    const uint8_t get_raw_byte(uint8_t pos) const { return record[pos]; }
    void set_raw_byte(uint8_t pos, uint8_t in) { record[pos] = in; }
    int64_t get_int(const message_record& mr) {
        // how many bytes to grab
        switch(mr.length)
        {
            case 1:
                return (int64_t)&record[mr.offset];
            case 2:
                return (int64_t)swap_endian_bytes<uint16_t>(*(uint16_t*)&record[mr.offset]);
            case 4:
                return (int64_t)swap_endian_bytes<uint32_t>(*(uint32_t*)&record[mr.offset]);
            case 8:
                return (int64_t)swap_endian_bytes<uint64_t>(*(uint64_t*)&record[mr.offset]);
            default:
                break;
        }
        return 0;
    }
    void set_int(const message_record& mr, int64_t in)
    {
        int64_t tmp = 0;
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
        strncpy(&record[mr.offset], in.c_str(), mr.length);
    }
    const std::string get_string(const message_record& mr)
    {
        // get the section of the record we want
        char buf[mr.length+1];
        memset(buf, 0, mr.length+1);
        strncpy(buf, &record[mr.offset], mr.length);
        return buf;
    }
    const char* get_record() const { return record; }
    const char* get_tag_values() const { return tag_values; }
    void add_tag_value(const tag_record::tag_name& tn, int64_t in) {
        size_t orig_sz = get_int(*variable_field_record);
        int8_t length = tag_records[to_underlying(tn)].length;
        size_t sz = orig_sz + 2 + length;
        if (tag_values == nullptr)
            tag_values = (char*) malloc(sz);
        else
            tag_values = (char*) realloc(tag_values, sz);
        // copy in the values
        tag_values[orig_sz] = length + 1; // add 1 for OptionTag
        tag_values[orig_sz + 1] = to_underlying(tn);
        uint64_t tmp = in;
        switch(length)
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
        memcpy(&tag_values[orig_sz + 2], &tmp, length);
        // adjust the reported length of the appendages
        set_int(*variable_field_record, sz);
    }
    void add_tag_value(const tag_record::tag_name& tn, const std::string& in) {
        size_t orig_sz = get_int(*variable_field_record);
        int8_t length = tag_records[to_underlying(tn)].length;
        size_t sz = orig_sz + 2 + length;
        tag_values = (char*) realloc(tag_values, sz);
        // copy in the values
        tag_values[orig_sz] = length + 1; // add 1 for OptionTag
        tag_values[orig_sz + 1] = to_underlying(tn);
        memcpy(&tag_values[orig_sz + 2], in.c_str(), length);
        // adjust the reported length of the appendages
        set_int(*variable_field_record, sz);
    }
    std::string get_tag_value_string(const tag_record::tag_name& tn)
    {
        // loop through to find correct tag
        uint64_t pos = 0;
        while(pos < get_int(*variable_field_record))
        {
            if ( (uint8_t)tag_values[pos+1] == to_underlying(tn))
            {
                // cut out the part we want
                uint8_t data_len = tag_values[0] - 1;
                char buf[ data_len + 1];
                memset(buf, 0, data_len + 1);
                strncpy(buf, &tag_values[pos+2], data_len);
                return buf;
            }
            else
            {
                pos += ((uint8_t)tag_values[pos]) + 1;
            }
        }
        return "";
    }
    int64_t get_tag_value_int(const tag_record::tag_name& tn)
    {
        // loop through to find correct tag
        uint64_t pos = 0;
        while(pos < get_int(*variable_field_record))
        {
            if ( (uint8_t)tag_values[pos+1] == to_underlying(tn))
            {
                switch(tag_values[0]-1)
                {
                    case 1:
                        return (int64_t)&tag_values[pos+2];
                    case 2:
                        return (int64_t)swap_endian_bytes<uint16_t>(*(uint16_t*)&tag_values[pos+2]);
                    case 4:
                        return (int64_t)swap_endian_bytes<uint32_t>(*(uint32_t*)&tag_values[pos+2]);
                    case 8:
                        return (int64_t)swap_endian_bytes<uint64_t>(*(uint64_t*)&tag_values[pos+2]);
                    default:
                        break;
                }
                return 0;
            }
            else
            {
                pos += ((uint8_t)tag_values[pos]) + 1;
            }
        }
        return 0;
    }
    protected:
    char record[SIZE];
    char* tag_values = nullptr;
};

/*****
 * Outgoing messages (to NASDAQ)
 */

const static uint8_t ENTER_ORDER_FIXED_LEN = 47; // NOTE: has 1 variable length field at pos 47
struct enter_order : public message<ENTER_ORDER_FIXED_LEN> {
    static constexpr message_record MESSAGE_TYPE{0, 1, message_record::field_type::ALPHA};
    static constexpr message_record USER_REF_NUM{1, 4, message_record::field_type::USER_REF_NUM};
    static constexpr message_record SIDE{5, 1, message_record::field_type::ALPHA};
    static constexpr message_record QUANTITY{6, 4, message_record::field_type::INTEGER};
    static constexpr message_record SYMBOL{10, 8, message_record::field_type::ALPHA};
    static constexpr message_record PRICE{18, 8, message_record::field_type::PRICE8};
    static constexpr message_record TIME_IN_FORCE{26, 1, message_record::field_type::ALPHA};
    static constexpr message_record DISPLAY{27, 1, message_record::field_type::ALPHA};
    static constexpr message_record CAPACITY{28, 1, message_record::field_type::ALPHA};
    static constexpr message_record INTERMARKET_SWEEP_ELIGIBILITY{29, 1, message_record::field_type::ALPHA};
    static constexpr message_record CROSS_TYPE{30, 1, message_record::field_type::ALPHA};
    static constexpr message_record CI_ORD_ID{31, 14, message_record::field_type::ALPHA};
    static constexpr message_record APPENDAGE_LENGTH{45, 2, message_record::field_type::INTEGER};

    enter_order() : message('O', &APPENDAGE_LENGTH) { }
    enter_order(const char* in) : message(in, &APPENDAGE_LENGTH) {}
};

const static uint8_t REPLACE_ORDER_FIXED_LEN = 40; // NOTE: has 1 variable length field
struct replace_order : public message<REPLACE_ORDER_FIXED_LEN> {
    static constexpr message_record MESSAGE_TYPE{0, 1, message_record::field_type::ALPHA};
    static constexpr message_record ORIG_USER_REF_NUM{1, 4, message_record::field_type::USER_REF_NUM};
    static constexpr message_record USER_REF_NUM{5, 4, message_record::field_type::USER_REF_NUM};
    static constexpr message_record QUANTITY{9, 4, message_record::field_type::INTEGER};
    static constexpr message_record PRICE{13, 8, message_record::field_type::PRICE8};
    static constexpr message_record TIME_IN_FORCE{21, 1, message_record::field_type::ALPHA};
    static constexpr message_record DISPLAY{22, 1, message_record::field_type::ALPHA};
    static constexpr message_record INTERMARKET_SWEEP_ELIGIBILITY{23, 1, message_record::field_type::ALPHA};
    static constexpr message_record CI_ORD_ID{24, 14, message_record::field_type::ALPHA};
    static constexpr message_record APPENDAGE_LENGTH{38, 2, message_record::field_type::INTEGER};

    replace_order() : message('U', &APPENDAGE_LENGTH) { }
    replace_order(const char* in) : message(in, &APPENDAGE_LENGTH) {}
};

const static uint8_t CANCEL_ORDER_FIXED_LEN = 9;
struct cancel_order : public message<CANCEL_ORDER_FIXED_LEN> {
    static constexpr message_record MESSAGE_TYPE{0, 1, message_record::field_type::ALPHA};
    static constexpr message_record USER_REF_NUM{1, 4, message_record::field_type::USER_REF_NUM};
    static constexpr message_record QUANTITY{5, 4, message_record::field_type::INTEGER};

    cancel_order() : message('X', nullptr) { }
    cancel_order(const char* in) : message(in, nullptr) {}
};

const static uint8_t MODIFY_ORDER_FIXED_LEN = 10;
struct modify_order : public message<MODIFY_ORDER_FIXED_LEN> {
    static constexpr message_record MESSAGE_TYPE{0, 1, message_record::field_type::ALPHA};
    static constexpr message_record USER_REF_NUM{1, 4, message_record::field_type::USER_REF_NUM};
    static constexpr message_record SIDE{5, 1, message_record::field_type::ALPHA};
    static constexpr message_record QUANTITY{6, 4, message_record::field_type::INTEGER};

    modify_order() : message('M', nullptr) { }
    modify_order(const char* in) : message(in, nullptr) {}
};

const static uint8_t ACCOUNT_QUERY_FIXED_LEN = 10;
struct account_query : public message<ACCOUNT_QUERY_FIXED_LEN> {
    static constexpr message_record MESSAGE_TYPE{0, 1, message_record::field_type::ALPHA};

    account_query() : message('Q', nullptr) { }
    account_query(const char* in) : message(in, nullptr) {}
};

/****
 * Incoming messages (from NASDAQ)
 */

const static uint8_t SYSTEM_EVENT_FIXED_LEN = 10;
struct system_event : public message<SYSTEM_EVENT_FIXED_LEN> {
    static constexpr message_record MESSAGE_TYPE{0, 1, message_record::field_type::ALPHA};
    static constexpr message_record TIMESTAMP{1, 8, message_record::field_type::INTEGER};
    static constexpr message_record EVENT_CODE{9, 1, message_record::field_type::ALPHA};

    system_event() : message('S', nullptr) { }
    system_event(const char* in) : message(in, nullptr) {}
};

const static uint8_t ORDER_ACCEPTED_FIXED_LEN = 64; // NOTE: has 1 variable length field
struct order_accepted : public message<ORDER_ACCEPTED_FIXED_LEN> {
    static constexpr message_record MESSAGE_TYPE{0, 1, message_record::field_type::ALPHA};
    static constexpr message_record TIMESTAMP{1, 8, message_record::field_type::INTEGER};
    static constexpr message_record USER_REF_NUM{9, 4, message_record::field_type::USER_REF_NUM};
    static constexpr message_record SIDE{13, 1, message_record::field_type::ALPHA};
    static constexpr message_record QUANTITY{14, 4, message_record::field_type::INTEGER};
    static constexpr message_record SYMBOL{18, 8, message_record::field_type::ALPHA};
    static constexpr message_record PRICE{26, 8, message_record::field_type::PRICE8};
    static constexpr message_record TIME_IN_FORCE{34, 1, message_record::field_type::ALPHA};
    static constexpr message_record DISPLAY{35, 1, message_record::field_type::ALPHA};
    static constexpr message_record ORDER_REFERENCE_NUMBER{36, 8, message_record::field_type::INTEGER};
    static constexpr message_record CAPACITY{44, 1, message_record::field_type::ALPHA};
    static constexpr message_record INTERMARKET_SWEEP_ELIGIBILITY{45, 1, message_record::field_type::ALPHA};
    static constexpr message_record CROSS_TYPE{46, 1, message_record::field_type::ALPHA};
    static constexpr message_record ORDER_STATE{47, 1, message_record::field_type::ALPHA};
    static constexpr message_record CI_ORD_ID{24, 14, message_record::field_type::ALPHA};
    static constexpr message_record APPENDAGE_LENGTH{38, 2, message_record::field_type::INTEGER};

    order_accepted() : message('A', &APPENDAGE_LENGTH) { }
    order_accepted(const char* in) : message(in, &APPENDAGE_LENGTH) {}
};

const static uint8_t ORDER_REPLACED_FIXED_LEN = 68; // NOTE: has 1 variable length field
struct order_replaced : public message<ORDER_REPLACED_FIXED_LEN> {
    static constexpr message_record MESSAGE_TYPE{0, 1, message_record::field_type::ALPHA};
    static constexpr message_record TIMESTAMP{1, 8, message_record::field_type::INTEGER};
    static constexpr message_record ORIG_USER_REF_NUM{9, 4, message_record::field_type::USER_REF_NUM};
    static constexpr message_record USER_REF_NUM{13, 4, message_record::field_type::USER_REF_NUM};
    static constexpr message_record SIDE{17, 1, message_record::field_type::ALPHA};
    static constexpr message_record QUANTITY{18, 4, message_record::field_type::INTEGER};
    static constexpr message_record SYMBOL{22, 8, message_record::field_type::ALPHA};
    static constexpr message_record PRICE{30, 8, message_record::field_type::PRICE8};
    static constexpr message_record TIME_IN_FORCE{38, 1, message_record::field_type::ALPHA};
    static constexpr message_record DISPLAY{39, 1, message_record::field_type::ALPHA};
    static constexpr message_record ORDER_REFERENCE_NUMBER{40, 8, message_record::field_type::INTEGER};
    static constexpr message_record CAPACITY{48, 1, message_record::field_type::ALPHA};
    static constexpr message_record INTERMARKET_SWEEP_ELIGIBILITY{49, 1, message_record::field_type::ALPHA};
    static constexpr message_record CROSS_TYPE{50, 1, message_record::field_type::ALPHA};
    static constexpr message_record ORDER_STATE{51, 1, message_record::field_type::ALPHA};
    static constexpr message_record CI_ORD_ID{52, 14, message_record::field_type::ALPHA};
    static constexpr message_record APPENDAGE_LENGTH{66, 2, message_record::field_type::INTEGER};

    order_replaced() : message('U', &APPENDAGE_LENGTH) { }
    order_replaced(const char* in) : message(in, &APPENDAGE_LENGTH) {}
};

const static uint8_t ORDER_CANCELED_FIXED_LEN = 18;
struct order_canceled : public message<ORDER_CANCELED_FIXED_LEN> {
    static constexpr message_record MESSAGE_TYPE{0, 1, message_record::field_type::ALPHA};
    static constexpr message_record TIMESTAMP{1, 8, message_record::field_type::INTEGER};
    static constexpr message_record USER_REF_NUM{9, 4, message_record::field_type::USER_REF_NUM};
    static constexpr message_record QUANTITY{13, 4, message_record::field_type::INTEGER};
    static constexpr message_record REASON{17, 1, message_record::field_type::INTEGER};

    order_canceled() : message('C', nullptr) { }
    order_canceled(const char* in) : message(in, nullptr) {}
};

const static uint8_t AIQ_CANCELED_FIXED_LEN = 31;
struct aiq_canceled : public message<AIQ_CANCELED_FIXED_LEN> {
    static constexpr message_record MESSAGE_TYPE{0, 1, message_record::field_type::ALPHA};
    static constexpr message_record TIMESTAMP{1, 8, message_record::field_type::INTEGER};
    static constexpr message_record USER_REF_NUM{9, 4, message_record::field_type::USER_REF_NUM};
    static constexpr message_record DECREMENT_SHARES{13, 4, message_record::field_type::INTEGER};
    static constexpr message_record REASON{17, 1, message_record::field_type::INTEGER};
    static constexpr message_record QUANTITY_PREVENTED_FROM_TRADING{18, 4, message_record::field_type::INTEGER};
    static constexpr message_record EXECUTION_PRICE{22, 8, message_record::field_type::PRICE8};
    static constexpr message_record LIQUIDITY_FLAG{30, 1, message_record::field_type::ALPHA};

    aiq_canceled() : message('D', nullptr) { }
    aiq_canceled(const char* in) : message(in, nullptr) {}
};

const static uint8_t ORDER_EXECUTED_FIXED_LEN = 36; // NOTE: has 1 variable length field
struct order_executed : public message<ORDER_EXECUTED_FIXED_LEN> {
    static constexpr message_record MESSAGE_TYPE{0, 1, message_record::field_type::ALPHA};
    static constexpr message_record TIMESTAMP{1, 8, message_record::field_type::INTEGER};
    static constexpr message_record USER_REF_NUM{9, 4, message_record::field_type::USER_REF_NUM};
    static constexpr message_record QUANTITY{13, 4, message_record::field_type::INTEGER};
    static constexpr message_record PRICE{17, 8, message_record::field_type::PRICE8};
    static constexpr message_record LIQUIDITY_FLAG{25, 1, message_record::field_type::ALPHA};
    static constexpr message_record MATCH_NUMBER{26, 8, message_record::field_type::INTEGER};
    static constexpr message_record APPENDAGE_LENGTH{34, 2, message_record::field_type::INTEGER};

    order_executed() : message('E', &APPENDAGE_LENGTH) { }
    order_executed(const char* in) : message(in, &APPENDAGE_LENGTH) {}
};

const static uint8_t BROKEN_TRADE_FIXED_LEN = 36;
struct broken_trade : public message<BROKEN_TRADE_FIXED_LEN> {
    static constexpr message_record MESSAGE_TYPE{0, 1, message_record::field_type::ALPHA};
    static constexpr message_record TIMESTAMP{1, 8, message_record::field_type::INTEGER};
    static constexpr message_record USER_REF_NUM{9, 4, message_record::field_type::USER_REF_NUM};
    static constexpr message_record MATCH_NUMBER{13, 8, message_record::field_type::INTEGER};
    static constexpr message_record REASON{21, 1, message_record::field_type::ALPHA};
    static constexpr message_record CI_ORD_ID{22, 14, message_record::field_type::ALPHA};

    broken_trade() : message('B', nullptr) { }
    broken_trade(const char* in) : message(in, nullptr) {}
};

const static uint8_t REJECTED_ORDER_FIXED_LEN = 29;
struct rejected_order : public message<REJECTED_ORDER_FIXED_LEN> {
    static constexpr message_record MESSAGE_TYPE{0, 1, message_record::field_type::ALPHA};
    static constexpr message_record TIMESTAMP{1, 8, message_record::field_type::INTEGER};
    static constexpr message_record USER_REF_NUM{9, 4, message_record::field_type::USER_REF_NUM};
    static constexpr message_record REASON{13, 2, message_record::field_type::INTEGER};
    static constexpr message_record CI_ORD_ID{15, 14, message_record::field_type::ALPHA};

    rejected_order() : message('J', nullptr) { }
    rejected_order(const char* in) : message(in, nullptr) {}
};

const static uint8_t CANCEL_PENDING_FIXED_LEN = 13;
struct cancel_pending : public message<CANCEL_PENDING_FIXED_LEN> {
    static constexpr message_record MESSAGE_TYPE{0, 1, message_record::field_type::ALPHA};
    static constexpr message_record TIMESTAMP{1, 8, message_record::field_type::INTEGER};
    static constexpr message_record USER_REF_NUM{9, 4, message_record::field_type::USER_REF_NUM};

    cancel_pending() : message('P', nullptr) { }
    cancel_pending(const char* in) : message(in, nullptr) {}
};

const static uint8_t CANCEL_REJECT_FIXED_LEN = 13;
struct cancel_reject : public message<CANCEL_REJECT_FIXED_LEN> {
    static constexpr message_record MESSAGE_TYPE{0, 1, message_record::field_type::ALPHA};
    static constexpr message_record TIMESTAMP{1, 8, message_record::field_type::INTEGER};
    static constexpr message_record USER_REF_NUM{9, 4, message_record::field_type::USER_REF_NUM};

    cancel_reject() : message('I', nullptr) { }
    cancel_reject(const char* in) : message(in, nullptr) {}
};

const static uint8_t ORDER_PRIORITY_UPDATE_FIXED_LEN = 30;
struct order_priority_update : public message<ORDER_PRIORITY_UPDATE_FIXED_LEN> {
    static constexpr message_record MESSAGE_TYPE{0, 1, message_record::field_type::ALPHA};
    static constexpr message_record TIMESTAMP{1, 8, message_record::field_type::INTEGER};
    static constexpr message_record USER_REF_NUM{9, 4, message_record::field_type::USER_REF_NUM};
    static constexpr message_record PRICE{13, 8, message_record::field_type::PRICE8};
    static constexpr message_record DISPLAY{21, 1, message_record::field_type::ALPHA};
    static constexpr message_record ORDER_REFERENCE_NUMBER{22, 8, message_record::field_type::INTEGER};

    order_priority_update() : message('T', nullptr) { }
    order_priority_update(const char* in) : message(in, nullptr) {}
};

const static uint8_t ORDER_MODIFIED_FIXED_LEN = 30;
struct order_modified : public message<ORDER_MODIFIED_FIXED_LEN> {
    static constexpr message_record MESSAGE_TYPE{0, 1, message_record::field_type::ALPHA};
    static constexpr message_record TIMESTAMP{1, 8, message_record::field_type::INTEGER};
    static constexpr message_record USER_REF_NUM{9, 4, message_record::field_type::USER_REF_NUM};
    static constexpr message_record SIDE{13, 1, message_record::field_type::ALPHA};
    static constexpr message_record QUANTITY{14, 4, message_record::field_type::INTEGER};
    static constexpr message_record ORDER_REFERENCE_NUMBER{22, 8, message_record::field_type::INTEGER};

    order_modified() : message('M', nullptr) { }
    order_modified(const char* in) : message(in, nullptr) {}
};

const static uint8_t ORDER_RESTATED_FIXED_LEN = 16; // NOTE: has 1 variable length field
struct order_restated : public message<ORDER_RESTATED_FIXED_LEN> {
    static constexpr message_record MESSAGE_TYPE{0, 1, message_record::field_type::ALPHA};
    static constexpr message_record TIMESTAMP{1, 8, message_record::field_type::INTEGER};
    static constexpr message_record USER_REF_NUM{9, 4, message_record::field_type::USER_REF_NUM};
    static constexpr message_record REASON{13, 1, message_record::field_type::ALPHA};
    static constexpr message_record APPENDAGE_LENGTH{14, 2, message_record::field_type::INTEGER};

    order_restated() : message('R', &APPENDAGE_LENGTH) { }
    order_restated(const char* in) : message(in, &APPENDAGE_LENGTH) {}
};

const static uint8_t ACCOUNT_QUERY_RESPONSE_FIXED_LEN = 13;
struct account_query_response : public message<ACCOUNT_QUERY_RESPONSE_FIXED_LEN> {
    static constexpr message_record MESSAGE_TYPE{0, 1, message_record::field_type::ALPHA};
    static constexpr message_record TIMESTAMP{1, 8, message_record::field_type::INTEGER};
    static constexpr message_record NEXT_USER_REF_NUM{9, 4, message_record::field_type::USER_REF_NUM};

    account_query_response() : message('Q', nullptr) { }
    account_query_response(const char* in) : message(in, nullptr) {}
};

} // end namespace ouch
