#include "soup_bin_server.h"
#include "soupbintcp.h"

SoupBinConnection::SoupBinConnection(boost::asio::ip::tcp::socket inSkt)
        : heartbeatTimer(this, 2000, Timer::get_time()), localIsServer(true), skt(std::move(inSkt))
{
    do_read_header();
}

SoupBinConnection::SoupBinConnection(const std::string& url, const std::string& user, const std::string& pw) 
        : heartbeatTimer(this, 2000, Timer::get_time()), localIsServer(false), skt(io_context), username(user), password(pw)
{
    try
    {
        std::string address = url;
        std::string port = "80";
        size_t pos = address.find(":");
        if (pos != std::string::npos)
        {
            port = address.substr(pos + 1);
            address = address.substr(0, pos);
        }
        boost::asio::ip::tcp::resolver resolver(io_context);
        do_connect(resolver.resolve(address, port));
        readerThread = std::thread([this]() { io_context.run(); });
    } 
    catch(const std::exception& e)
    {
        // TODO
    }
}

SoupBinConnection::~SoupBinConnection()
{
    try
    {
        skt.close();
        if (readerThread.joinable())
            readerThread.join();
    } catch(const std::exception& e) {
        // TODO: 
    }
}

void SoupBinConnection::do_connect(const boost::asio::ip::tcp::resolver::results_type& endpoints)
{
    boost::asio::async_connect(skt, endpoints, [this](boost::system::error_code ec, boost::asio::ip::tcp::endpoint) {
        if (!ec)
        {
            // attempt login
            soupbintcp::login_request req;
            req.set_string(soupbintcp::login_request::USERNAME, username);
            req.set_string(soupbintcp::login_request::PASSWORD, password);
            send(req.get_record_as_vec());
            do_read_header();
        }
    });
}
void SoupBinConnection::do_read_header()
{
    // read from network, placing first 3 bytes into buffer
    boost::asio::async_read(skt, boost::asio::buffer(currentIncoming.data(), 3),
            [this](boost::system::error_code ec, std::size_t /* length */) {
                if (!ec && currentIncoming.decode_header()) 
                {
                    do_read_body();
                }
                else
                {
                    skt.close();
                }
            });
}
void SoupBinConnection::do_read_body()
{
    boost::asio::async_read(skt, boost::asio::buffer(currentIncoming.body(), currentIncoming.body_length()),
            [this](boost::system::error_code ec, std::size_t /* length */) {
                if (!ec)
                {
                    // if this is a system message, handle it. Otherwise place it in queue
                    if (currentIncoming.decode_header()) {
                        switch(currentIncoming.data()[2])
                        {
                            // from server or client
                            case('+'): // debug packet
                                on_debug(soupbintcp::debug_packet(currentIncoming.data()));
                                break;
                            // from server
                            case('A'): // login accepted
                                on_login_accepted(soupbintcp::login_accepted(currentIncoming.data()));
                                break;
                            case('J'): // login rejected
                                on_login_rejected(soupbintcp::login_rejected(currentIncoming.data()));
                                break;
                            case('S'):
                                on_sequenced_data(soupbintcp::sequenced_data(currentIncoming.data()));
                                break;
                            case('H'): // heartbeat coming from server
                                on_server_heartbeat(soupbintcp::server_heartbeat(currentIncoming.data()));
                                break;
                            case('Z'): // server end of session
                                on_end_of_session(soupbintcp::end_of_session(currentIncoming.data()));
                                break;
                            // from client
                            case('L'): // login request
                                on_login_request(soupbintcp::login_request(currentIncoming.data()));
                                break;
                            case('U'):
                                on_unsequenced_data(soupbintcp::unsequenced_data(currentIncoming.data()));
                                break;
                            case('R'):
                                on_client_heartbeat(soupbintcp::client_heartbeat(currentIncoming.data()));
                                break;
                            case('O'):
                                on_logout_request(soupbintcp::logout_request(currentIncoming.data()));
                                break;
                            default:
                            {
                                // this should never happen
                                // place message in queue
                                std::vector<unsigned char> vec(currentIncoming.data(), currentIncoming.data() + currentIncoming.body_length() + 3);
                                read_msgs.push_back(vec);
                                break;
                            }
                        }
                        currentIncoming.clean();
                        do_read_header();
                    }
                    else
                        skt.close();
                }
                else
                    skt.close();
            });
}
void SoupBinConnection::do_write()
{
    boost::asio::async_write(skt, boost::asio::buffer(write_msgs.front().data(), write_msgs.front().size()),
            [this](boost::system::error_code ec, std::size_t /* length */) {
                if (!ec) {
                    write_msgs.pop_front();
                    if (!write_msgs.empty())
                        do_write();
                } else {
                    skt.close();
                }
            });
}

void SoupBinConnection::send_sequenced(uint64_t seqNo, const std::vector<unsigned char>& bytes)
{
    // add to map
    messages.emplace(seqNo, bytes);
    send(bytes);
}

void SoupBinConnection::send_sequenced(const std::vector<unsigned char>& bytes)
{
    send_sequenced(get_next_seq(), bytes);
}

void SoupBinConnection::send_unsequenced(const std::vector<unsigned char>& bytes)
{
    // just send
    send(bytes);
}

void SoupBinConnection::send(const std::vector<unsigned char>& bytes)
{
    if (localIsServer)
    {
        boost::asio::async_write(skt, boost::asio::buffer(bytes.data(), bytes.size()), 
                [this, bytes](boost::system::error_code ec, std::size_t /* length */) {
                    if (ec)
                        skt.close();
                });
    }
    else
    {
        boost::asio::post(io_context, [this, bytes]() {
            bool write_in_progress = !write_msgs.empty();
            write_msgs.push_back(bytes);
            if (!write_in_progress) {
                do_write();
            }
        });
    }
}

uint64_t SoupBinConnection::get_next_seq() { return nextSeq++; }

void SoupBinConnection::OnTimer(uint64_t msSince)
{
    // send heartbeat
    if (localIsServer)
    {
        soupbintcp::server_heartbeat hb;
        send_unsequenced(hb.get_record_as_vec());
    }
    else
    {
        soupbintcp::client_heartbeat hb;
        send_unsequenced(hb.get_record_as_vec());
    }
}
