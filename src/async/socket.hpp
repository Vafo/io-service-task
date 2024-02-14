#ifndef ASIO_SOCKET_HPP
#define ASIO_SOCKET_HPP

#include <unistd.h>

#include "io_service.hpp"
#include "uring_async.hpp"

namespace io_service {
namespace ip {

class socket {
private:
    const int invalid_fd = -1;
private:
    io_service& m_serv;
    int m_fd;

private:
    // used by async_accept_comp
    socket(io_service& serv, int fd)
        : m_serv(serv)
        , m_fd(fd)
    {}

public:
    // empty socket
    socket(io_service& serv)
        : m_serv(serv)
        , m_fd(invalid_fd)
    {}

public:
    ~socket()
    { 
        if(m_fd != invalid_fd)
            close(m_fd);
    }

private:
    template<typename CompHandler>
    friend class async_accept_comp;

}; // class socket

} // namespace ip
} // namespace io_service

#endif // ASIO_SOCKET_HPP
