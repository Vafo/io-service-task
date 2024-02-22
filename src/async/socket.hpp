#ifndef ASIO_SOCKET_HPP
#define ASIO_SOCKET_HPP

#include <liburing.h>
#include <sys/socket.h>
#include <unistd.h>
#include <utility>

#include "io_service.hpp"
#include "endpoint.hpp"
#include "buffer.hpp"
#include "uring.hpp"
#include "uring_async.hpp"

#include "uring_error.hpp"

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

class async_io_init {
protected:
    int m_sock_fd;
    buffer m_buf;
    size_t m_size;

protected:
    async_io_init(int sock_fd, buffer&& buf)
        : m_sock_fd(sock_fd)
        , m_buf(std::forward<buffer>(buf))
    {}

}; // class async_io_init

class async_write_init
    : protected async_io_init
{
public:
    async_write_init(int sock_fd, buffer&& buf)
        : async_io_init(sock_fd, std::forward<buffer>(buf))
    {}

public:
   void operator() (uring_sqe& sqe) {
        io_uring_prep_write(
            sqe.get(),
            m_sock_fd,
            m_buf.base(),
            m_buf.size(),
            0
        );
   }

}; // class async_write_init

class async_read_init
    : protected async_io_init
{
public:
    async_read_init(int sock_fd, buffer&& buf)
        : async_io_init(sock_fd, std::forward<buffer>(buf))
    {}

public:
   void operator() (uring_sqe& sqe) {
        io_uring_prep_read(
            sqe.get(),
            m_sock_fd,
            m_buf.base(),
            m_buf.size(),
            0
        );
   }

}; // class async_read_init

template<typename CompHandler,
    typename std::enable_if_t<
        std::is_invocable_v<CompHandler, uring_error, int>, int> = 0>
class async_io_completer {
private:
    CompHandler m_comp;

public:
    async_io_completer(CompHandler&& comp)
        : m_comp(std::forward<CompHandler>(comp))
    {}

public:
    void operator()(int cqe_res) {
        uring_error err;
        if(cqe_res < 0) {
            err = cqe_res;
        }
        m_comp(err, cqe_res);
    }

}; // class base_io_completer

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

    socket(socket&& other)
        : socket(other.m_serv)
    {
        m_fd = other.m_fd;
        other.m_fd = invalid_fd;
    }

    ~socket()
    { 
        if(m_fd != invalid_fd)
            close(m_fd);
    }

public:
    void setup() {
        m_fd = M_setup_socket(AF_INET/*ip4*/, SOCK_STREAM/*tcp*/);
    }

public:
    template<typename CompHandler>
    void async_connect(endpoint& ep, CompHandler&& comp) {
        M_check_validity();

        uring_async_poster<io_service> poster(m_serv);
        poster.post(
            detail::async_connect_init{m_fd, ep},
            std::forward<CompHandler>(comp));
    }

public:
    template<typename CompHandler>
    void async_read_some(buffer&& buf, CompHandler&& comp) {
        M_check_validity();

        uring_async_poster<io_service> poster(m_serv);
        poster.post(
            detail::async_read_init{m_fd, std::forward<buffer>(buf)},
            detail::async_io_completer{
                std::forward<CompHandler>(comp)});
    }

    template<typename CompHandler>
    void async_write_some(buffer&& buf, CompHandler&& comp) {
        M_check_validity();

        uring_async_poster<io_service> poster(m_serv);
        poster.post(
            detail::async_write_init{m_fd, std::forward<buffer>(buf)},
            detail::async_io_completer{
                std::forward<CompHandler>(comp)});
    }

public:
    io_service& get_executor()
    { return m_serv; }

// Impl funcs
private:
   int M_setup_socket(int family, int socktype); 

   void M_check_validity() {
       if(m_fd == invalid_fd)
           throw std::runtime_error("invalid socket is used");
   }

private:
    template<typename CompHandler>
    friend class detail::async_accept_comp;

}; // class socket


} // namespace ip
} // namespace io_service

#endif // ASIO_SOCKET_HPP
