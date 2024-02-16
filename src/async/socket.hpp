#ifndef ASIO_SOCKET_HPP
#define ASIO_SOCKET_HPP

#include <liburing.h>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

#include "io_service.hpp"
#include "endpoint.hpp"
#include "uring.hpp"
#include "uring_async.hpp"

namespace io_service {
namespace ip {

namespace detail {

// Forward declaration for acceptor.hpp
template<typename CompHandler>
class async_accept_comp;


class async_connect_init {
private:
    int m_sock_fd;
    endpoint& m_ep;

public:
    async_connect_init(int sock_fd, endpoint& ep)
        : m_sock_fd(sock_fd)
        , m_ep(ep)
    {}

public:
    void operator() (uring_sqe& sqe) {
        io_uring_prep_connect(
            sqe.get(),
            m_sock_fd,
            &m_ep.get_addr(),
            m_ep.get_len());
    }

}; // async_connect_init

} // namespace detail


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

    ~socket()
    { 
        if(m_fd != invalid_fd)
            close(m_fd);
    }

public:
    template<typename CompHandler>
    void async_connect(endpoint& ep, CompHandler&& comp) {
        
        if(m_fd == invalid_fd) {
            m_fd = M_setup_socket(AF_INET/*ip4*/, SOCK_STREAM/*tcp*/);
        }

        uring_async_poster<io_service> poster(m_serv);
        poster.post(
            detail::async_connect_init{m_fd, ep},
            std::forward<CompHandler>(comp));
    }

public:
    io_service& get_executor()
    { return m_serv; }


// Impl funcs
private:
   int M_setup_socket(int family, int socktype); 

private:
    template<typename CompHandler>
    friend class detail::async_accept_comp;

}; // class socket


} // namespace ip
} // namespace io_service

#endif // ASIO_SOCKET_HPP
