#pragma once
#include "soup_bin_connection.h"
#include <vector>
#include <memory>
#include <string>

/***
 * A SoupBin client
*/
template<typename CONNECTION>
class SoupBinClient
{
    public:
    SoupBinClient(CONNECTION conn) : connection(std::move(conn))
    {
    }

    void send_unsequenced(const std::vector<unsigned char>& bytes)
    {
    }

    protected:
    CONNECTION connection;
};
