#pragma once
#include "soup_bin_timer.h"
#include "soupbintcp.h"
#include <vector>
#include <unordered_map>
#include <atomic>
#include <deque>
#include <string>
#include <boost/asio.hpp>

class SoupBinConnection;

class MessageRepeater
{
    public:
    virtual void repeat_from(SoupBinConnection* conn, uint64_t startPos) = 0;
};

/***
 * Represents a connected client
*/
class SoupBinConnection : public TimerListener
{
    public:
    enum class Status
    {
        CONNECTING,
        CONNECTED,
        DISCONNECTED  
    };

    /***
     * A connection to a server from a client
     */
    SoupBinConnection(const std::string& url, const std::string& username, const std::string& password,
            const std::string& sessionId = "", uint64_t nextSequenceNo = 0);
    /***
     * A connection from a client (this ctor used by a server
     */
    SoupBinConnection(boost::asio::ip::tcp::socket skt, MessageRepeater* parent);
    ~SoupBinConnection();

    /***
     * Stores message for repeats, plus sends it
    */
    virtual void send_sequenced(uint64_t seqNo, const std::vector<unsigned char>& bytes);
    virtual void send_sequenced(const std::vector<unsigned char>& bytes);
    void send_unsequenced(const std::vector<unsigned char>& bytes);
    uint64_t get_next_seq(bool increment = true);
    std::string get_session_id() { return sessionId; }

    // TimerListener implementation
    virtual void OnTimer(uint64_t msSince) override;

    public:
    Status status = Status::CONNECTING;

    protected:
    // these are called when messages come in
    virtual void on_debug(const soupbintcp::debug_packet& in) {}
    virtual void on_login_accepted(const soupbintcp::login_accepted& in) {}
    virtual void on_login_rejected(const soupbintcp::login_rejected& in) {}
    virtual void on_sequenced_data(const soupbintcp::sequenced_data& in) {}
    virtual void on_unsequenced_data(const soupbintcp::unsequenced_data&  in) {}
    virtual void on_login_request(const soupbintcp::login_request& in);
    virtual void on_logout_request(const soupbintcp::logout_request& in) {}
    virtual void on_server_heartbeat(const soupbintcp::server_heartbeat& in) {} 
    virtual void on_client_heartbeat(const soupbintcp::client_heartbeat& in) {}
    virtual void on_end_of_session(const soupbintcp::end_of_session& in) {}
    void send(const std::vector<unsigned char>& bytes);

    // boost asio
    void do_connect(const boost::asio::ip::tcp::resolver::results_type& endpoints);
    void do_read_header();
    void do_read_body();
    void do_write();
    void close_socket();

    protected:
    const std::string username;
    const std::string password;
    std::string sessionId;
    bool localIsServer = false;
    std::atomic<uint64_t> nextSeq = 0;
    Timer heartbeatTimer; // fires off a heartbeat packet if nothing sent for 1 minute
    std::unordered_map<uint64_t, std::vector<unsigned char> > messages;
    boost::asio::io_context io_context;
    boost::asio::ip::tcp::socket skt;
    std::thread readerThread;
    bool shuttingDown = false;
    std::deque<std::vector<unsigned char> > write_msgs;
    std::deque<std::vector<unsigned char> > read_msgs;
    soupbintcp::incoming_message currentIncoming;
    MessageRepeater* parent;
};

