#ifndef ASIO_ASYNC_CONNECT_HPP
#define ASIO_ASYNC_CONNECT_HPP

#include "socket.hpp"
#include "resolver.hpp"

namespace io_service {
namespace ip {

namespace detail {

template<typename CompHandler>
class async_multi_connect_comp {
private:
    typedef resolver::results_type::iterator eps_it_t;
    typedef resolver::results_type::size_type idx_t;
private:
    socket& m_sock;
    eps_it_t m_cur;
    eps_it_t m_end;
    CompHandler m_comp;

public:
    template<typename
        std::enable_if_t<
            std::is_invocable_v<CompHandler, int>, 
        int> = 0>
    async_multi_connect_comp(
        socket& sock,
        eps_it_t eps_cur, eps_it_t eps_end,
        CompHandler&& comp
    )
        : m_sock(sock)
        , m_cur(eps_cur)
        , m_end(eps_end)
        , m_comp(std::forward<CompHandler>(comp))
    {}

public:
    void operator()(int cqe_res) {
        // failed to connect
        if(cqe_res != 0 && m_cur != m_end) {
            // retry connection on different endpoint
            do_async_connect();
            return;            
        }

        m_comp(cqe_res);
    }

public:
    void do_async_connect() {
        m_sock.async_connect(
            *m_cur,
            async_multi_connect_comp{
                m_sock,
                m_cur + 1, m_end,
                std::move(m_comp)});
    }

}; // class async_connect_comp


} // namespace detail


template<typename CompHandler>
void async_connect(
    socket& sock, resolver::results_type& eps,
    CompHandler&& comp
) {
    detail::async_multi_connect_comp<CompHandler> multi_con(
        sock, 
        eps.begin(), eps.end(),
        std::forward<CompHandler>(comp));
    
    multi_con.do_async_connect();
}


} // namespace ip
} // namespace io_service

#endif
