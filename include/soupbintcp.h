#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace soupbintcp {

template <typename T>
T swap_endian_bytes(T in)
{
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
        NUMERIC = 2, // a number, but written in ASCII
    };
    uint8_t offset = 0;
    uint8_t length = 0;
    field_type type;
};

template <typename E>
constexpr typename std::underlying_type<E>::type to_underlying(E e) noexcept {
    return static_cast<typename std::underlying_type<E>::type>(e);
}

template<unsigned int SIZE>
struct message {
    const char message_type = ' ';
    message(char message_type) : message_type(message_type)
    {
        // we don't have the size yet, so allocate SIZE
        record = (char*)malloc(SIZE);
        if (record == nullptr)
            throw std::invalid_argument("Size too big");
        allocated_space = SIZE;
        // set the size to SIZE
        uint16_t swapped = swap_endian_bytes<uint16_t>(allocated_space-2);
        memcpy( &record[0], &swapped, 2);
        // set the message type
        record[2] = message_type;
        if (SIZE-3 > 0)
            memset( &record[3], 0, SIZE-3 );
    }
    message(const char* in) : message_type(in[0])
    {
        // calculate the size
        size_t sz = swap_endian_bytes<uint16_t>(*(uint16_t*)&in[0]);
        allocated_space = sz + 2;
        record = (char*)malloc(allocated_space);
        memcpy(record, in, allocated_space);
    }
    ~message() {
        free(record);
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
        memcpy(&record[mr.offset], &in, mr.length);
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
    void set_message(const std::vector<unsigned char> data)
    {
        // do we need to allocate space?
        size_t needed_space = SIZE + data.size();
        if (allocated_space <= needed_space) {
            record = (char*)realloc(record, needed_space);
            allocated_space = needed_space;
            const unsigned char* tmp = (unsigned char*)&allocated_space;
            uint16_t sz = swap_endian_bytes<uint16_t>(allocated_space - 2);
            memcpy(record, &sz, 2);
        }
        if (record != nullptr)
        {
            // copy in the vector
            std::copy(data.begin(), data.end(), &record[SIZE]);
        }
    }
    std::vector<unsigned char> get_message() 
    {
        size_t sz = allocated_space - SIZE;
        std::vector<unsigned char> vec(sz);
        vec.assign(&record[SIZE], &record[SIZE] + sz);
        return vec;
    }
    const char* get_record() const { return record; }
    protected:
    char *record = nullptr;
    size_t allocated_space = 0;
};

const static uint8_t DEBUG_PACKET_LEN = 3;
struct debug_packet : public message<DEBUG_PACKET_LEN>
{
    static constexpr message_record PACKET_LENGTH{0, 2, message_record::field_type::INTEGER};
    static constexpr message_record PACKET_TYPE{2, 1, message_record::field_type::ALPHA};
    
    debug_packet() : message('+') {}
    debug_packet(const char* in) : message(in) {}
};

/*****
 * Outgoing messages (to NASDAQ)
 */

const static uint8_t LOGIN_ACCEPTED_LEN = 33;
struct login_accepted : public message<LOGIN_ACCEPTED_LEN>
{
    static constexpr message_record PACKET_LENGTH{0, 2, message_record::field_type::INTEGER};
    static constexpr message_record PACKET_TYPE{2, 1, message_record::field_type::ALPHA};
    static constexpr message_record SESSION{3, 10, message_record::field_type::ALPHA};
    static constexpr message_record SEQUENCE_NUMBER{13, 20, message_record::field_type::NUMERIC};
    
    login_accepted() : message('A') {}
    login_accepted(const char* in) : message(in) {}
};

const static uint8_t LOGIN_REJECTED_LEN = 4;
struct login_rejected : public message<LOGIN_REJECTED_LEN>
{
    static constexpr message_record PACKET_LENGTH{0, 2, message_record::field_type::INTEGER};
    static constexpr message_record PACKET_TYPE{2, 1, message_record::field_type::ALPHA};
    // A (authorization) or S (session not available)
    static constexpr message_record REJECT_REASON_CODE{3, 1, message_record::field_type::ALPHA};
    
    login_rejected() : message('J') {}
    login_rejected(const char* in) : message(in) {}
};

const static uint8_t SEQUENCED_DATA_LEN = 3;
struct sequenced_data : public message<SEQUENCED_DATA_LEN>
{
    static constexpr message_record PACKET_LENGTH{0, 2, message_record::field_type::INTEGER};
    static constexpr message_record PACKET_TYPE{2, 1, message_record::field_type::ALPHA};
    
    sequenced_data() : message('S') {}
    sequenced_data(const char* in) : message(in) {}
};

const static uint8_t SERVER_HEARTBEAT_LEN = 3;
struct server_heartbeat : public message<SERVER_HEARTBEAT_LEN>
{
    static constexpr message_record PACKET_LENGTH{0, 2, message_record::field_type::INTEGER};
    static constexpr message_record PACKET_TYPE{2, 1, message_record::field_type::ALPHA};
    
    server_heartbeat() : message('H') {}
    server_heartbeat(const char* in) : message(in) {}
};

const static uint8_t END_OF_SESSION_LEN = 3;
struct end_of_session : public message<END_OF_SESSION_LEN>
{
    static constexpr message_record PACKET_LENGTH{0, 2, message_record::field_type::INTEGER};
    static constexpr message_record PACKET_TYPE{2, 1, message_record::field_type::ALPHA};
    
    end_of_session() : message('Z') {}
    end_of_session(const char* in) : message(in) {}
};

/****
 * Client packets
 */

const static uint8_t LOGIN_REQUEST_LEN = 49;
struct login_request : public message<LOGIN_REQUEST_LEN>
{
    static constexpr message_record PACKET_LENGTH{0, 2, message_record::field_type::INTEGER};
    static constexpr message_record PACKET_TYPE{2, 1, message_record::field_type::ALPHA};
    static constexpr message_record USERNAME{3, 6, message_record::field_type::ALPHA};
    static constexpr message_record PASSWORD{9, 10, message_record::field_type::ALPHA};
    static constexpr message_record REQUESTED_SESSION{19, 10, message_record::field_type::ALPHA};
    static constexpr message_record REQUESTED_SEQUENCE_NUMBER{29, 20, message_record::field_type::NUMERIC};
    
    login_request() : message('L') {}
    login_request(const char* in) : message(in) {}
};

const static uint8_t UNSEQUENCED_LEN = 3;
struct unsequenced : public message<UNSEQUENCED_LEN>
{
    static constexpr message_record PACKET_LENGTH{0, 2, message_record::field_type::INTEGER};
    static constexpr message_record PACKET_TYPE{2, 1, message_record::field_type::ALPHA};
    
    unsequenced() : message('U') {}
    unsequenced(const char* in) : message(in) {}
};

const static uint8_t CLIENT_HEARTBEAT_LEN = 3;
struct client_heartbeat : public message<CLIENT_HEARTBEAT_LEN>
{
    static constexpr message_record PACKET_LENGTH{0, 2, message_record::field_type::INTEGER};
    static constexpr message_record PACKET_TYPE{2, 1, message_record::field_type::ALPHA};
    
    client_heartbeat() : message('R') {}
    client_heartbeat(const char* in) : message(in) {}
};

const static uint8_t LOGOUT_REQUEST_LEN = 3;
struct logout_request : public message<LOGOUT_REQUEST_LEN>
{
    static constexpr message_record PACKET_LENGTH{0, 2, message_record::field_type::INTEGER};
    static constexpr message_record PACKET_TYPE{2, 1, message_record::field_type::ALPHA};
    
    logout_request() : message('O') {}
    logout_request(const char* in) : message(in) {}
};

} // end namespace soupbintcp

