#ifndef ASIO_SOCKET_HPP
#define ASIO_SOCKET_HPP

#include <unistd.h>

#include "io_service.hpp"
#include "uring_async.hpp"

namespace io_service {
namespace ip {

class socket {
private:
    io_service& m_serv;
    int m_fd;

public:
    socket(io_service& serv)
        : m_serv(serv)
        , m_fd(-1)
    {}

private:
    socket(io_service& serv, int fd)
        : m_serv(serv)
        , m_fd(fd)
    {}

public:
    ~socket()
    { close(m_fd); }

private:
    template<typename CompHandler>
    friend class async_accept_comp;

}; // class socket

} // namespace ip
} // namespace io_service

#endif // ASIO_SOCKET_HPP
