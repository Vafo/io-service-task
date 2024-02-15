#ifndef ASIO_ACCEPTOR_HPP
#define ASIO_ACCEPTOR_HPP

#include <liburing.h>
#include <netinet/in.h>
#include <type_traits>
#include <utility>

#include "io_service.hpp"
#include "uring.hpp"
#include "socket.hpp"

namespace io_service {

namespace ip {

namespace detail {

int socket_setup_accept();
void socket_bind(int fd, in_port_t port);
void socket_listen(int fd, int num_connections);


class async_accept_init {
private:
    int m_acceptor_fd;
    // endpoint_info& m_peer;

public:
    async_accept_init(
        int acceptor_fd /*, endpoint_info& peer*/
    )
        : m_acceptor_fd(acceptor_fd)
    {}

public:
    void operator()(uring_sqe& sqe) {
        io_uring_prep_accept(
            sqe.get(),
            m_acceptor_fd,
            NULL,
            NULL,
            0);
    }

}; // class async_accept_init

template<typename CompHandler>
class async_accept_comp {
private:
    io_service& m_serv;
    CompHandler m_comp;

public:
    template<typename
        std::enable_if_t<
            std::is_invocable_v<CompHandler, int, socket>, 
        int> = 0>
    async_accept_comp(io_service& serv, CompHandler&& comp)
        : m_serv(serv)
        , m_comp(std::forward<CompHandler>(comp))
    {}

public:
    void operator()(int cqe_res) {
        if(cqe_res < 0)
            m_comp(cqe_res, socket{m_serv});
        else
            m_comp(0, socket{m_serv, cqe_res});
    }

}; // class async_accept_comp

} // namespace detail


class acceptor {
private:
    io_service& m_serv;
    int m_fd;

public:
    acceptor(io_service& serv)
        : m_serv(serv)
        , m_fd(-1)
    {
        // C code to init socket and etc
        m_fd = detail::socket_setup_accept();
    }

public:
    void bind(in_port_t port)
    { detail::socket_bind(m_fd, port); }

    void listen(int num_pending_con)
    { detail::socket_listen(m_fd, num_pending_con); }

    template<typename CompHandler>
    void async_accept(CompHandler&& comp) {
        uring_async_poster poster(m_serv);        
        poster.post(
            detail::async_accept_init{m_fd},
            detail::async_accept_comp<CompHandler>{
                m_serv, std::forward<CompHandler>(comp)});
    }

}; // class acceptor

} // namespace ip

} // namespace io_service

#endif
