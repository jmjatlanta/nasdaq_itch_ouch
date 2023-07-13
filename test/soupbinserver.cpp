#include <gtest/gtest.h>
#include "soup_bin_server.h"
#include "soup_bin_client.h"
#include <thread>

class MyConnection : public SoupBinConnection
{
    public:
    MyConnection(boost::asio::ip::tcp::socket socket, MessageRepeater* parent) : SoupBinConnection(std::move(socket), parent) {}
    MyConnection(const std::string& url, const std::string& username, const std::string& password, 
            const std::string& sessionId, uint64_t seqNum) 
            : SoupBinConnection(url, username, password, sessionId, seqNum) {}
    virtual void on_client_heartbeat(const soupbintcp::client_heartbeat& in) override
    {
        numClientHeartbeats++;
    }
    virtual void on_server_heartbeat(const soupbintcp::server_heartbeat& in) override
    {
        numServerHeartbeats++;
    }
    virtual void on_login_accepted(const soupbintcp::login_accepted& in) override
    {
        nextSeq = in.get_int(soupbintcp::login_accepted::SEQUENCE_NUMBER);
        sessionId = in.get_string(soupbintcp::login_accepted::SESSION);
    }
    void on_sequenced_data(const soupbintcp::sequenced_data& in) override
    {
        uint64_t seq = get_next_seq();
        messages.emplace(seq, in.get_message());
    }
    uint32_t numClientHeartbeats = 0;
    uint32_t numServerHeartbeats = 0;
    std::unordered_map<uint64_t, std::vector<unsigned char>> messages;
};
class MySoupBinServer : public SoupBinServer<MyConnection>
{
    public:
    MySoupBinServer(uint32_t port) : SoupBinServer(port)
    {
    }
    uint32_t GetNumClientHeartbeats()
    {
        uint32_t total = 0;
        for(auto c : connections)
            total += c->numClientHeartbeats;
        return total;
    }
    uint32_t GetNumServerHeartbeats()
    {
        uint32_t total = 0;
        for(auto c : connections)
            total += c->numServerHeartbeats;
        return total;
    }
};
class MySoupBinClient
{
    public:
    MySoupBinClient(const std::string& url, const std::string& user, const std::string& pw, const std::string& sessionId = "", uint64_t nextSeqNum = 0)
            : connection(url, user, pw, sessionId, nextSeqNum)
    {
    }
    uint32_t GetNumClientHeartbeats()
    {
        return connection.numClientHeartbeats;
    }
    uint32_t GetNumServerHeartbeats()
    {
        return connection.numServerHeartbeats;
    }
    uint64_t GetCurrentSequenceNo() { return connection.get_next_seq(false); }
    std::string GetSessionId() { return connection.get_session_id(); }
    std::string GetMessage(uint64_t msgNo) 
    { 
        std::vector<unsigned char> vec = connection.messages[msgNo]; 
        return std::string(vec.begin(), vec.end()); 
    } 
    std::unordered_map<uint64_t, std::vector<unsigned char> > GetMessages() { return connection.messages; }
    MyConnection connection;
};


TEST(SoupBinServer, timer)
{
    class MyClass : public TimerListener
    {
        public:
        MyClass() : timer(this, 1000, Timer::get_time())
        {
        }

        virtual void OnTimer(uint64_t msSince)
        {
            numFires++;
        }
        uint16_t numFires = 0;
        Timer timer;
    };

    MyClass myClass;
    // check the timer, it should not have gone off
    EXPECT_EQ(myClass.numFires, 0);
    // wait for half of the timeout and then reset
    uint64_t start = Timer::get_time();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    EXPECT_EQ(myClass.numFires, 0);
    myClass.timer.reset();
    // now wait for the other half of the first timeout
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    EXPECT_EQ(myClass.numFires, 0);
    // now wait for the other half of the second timeout (plus a little)
    std::this_thread::sleep_for(std::chrono::milliseconds(510));
    EXPECT_EQ(myClass.numFires, 1);
}

TEST(SoupBinServer, ServerStartStop)
{
    MySoupBinServer server(9012);
    std::this_thread::sleep_for(std::chrono::seconds(2));
}

TEST(SoupBinServer, OneClient)
{
    MySoupBinServer server(9012);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    MySoupBinClient client("127.0.0.1:9012", "test1", "password");
    // wait 2.5 seconds
    std::this_thread::sleep_for(std::chrono::milliseconds(2500));
    // we should have sent 1 heartbeat request and received 1 heartbeat response
    EXPECT_EQ(server.GetNumClientHeartbeats(), 1);
    EXPECT_EQ(server.GetNumServerHeartbeats(), 0);
    EXPECT_EQ(client.GetNumClientHeartbeats(), 0);
    EXPECT_EQ(client.GetNumServerHeartbeats(), 1);
    // if the server sends data, it should not bother sending heartbeat requests
    // the client always sends heartbeat requests
}

TEST(SoupBinServer, ServerReconnect)
{
    MySoupBinServer server(9012);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    std::shared_ptr<MySoupBinClient> client = std::make_shared<MySoupBinClient>("127.0.0.1:9012", "test1", "password");
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // send sequenced packets to the client
    for(int i = 0; i < 3; ++i)
    {
        std::string msg = "Hello" + std::to_string(i);
        soupbintcp::sequenced_data data;
        data.set_message(std::vector<unsigned char>(msg.begin(), msg.end()));
        server.send_sequenced(data.get_record_as_vec());
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    // client should have 3 messages
    auto msgs = client->GetMessages();
    EXPECT_EQ(msgs.size(), 3);
    // next sequence number should be 4
    uint64_t nextSequenceNo = client->GetCurrentSequenceNo();
    EXPECT_EQ(nextSequenceNo, 4);
    // disconnect abruptly and ask for messages from 2
    std::string sessionId = client->GetSessionId();
    client = std::make_shared<MySoupBinClient>("127.0.0.1:9012", "test1", "password", sessionId, 2);
    // client should only have messages 2 and 3
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    EXPECT_EQ(client->GetCurrentSequenceNo(), 4);
}
