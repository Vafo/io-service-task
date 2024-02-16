#ifndef ASIO_RESOLVER_HPP
#define ASIO_RESOLVER_HPP

#include <iostream>

#include <netdb.h>
#include <sys/socket.h>
#include <type_traits>
#include <vector>
#include <string>

#include "cerror_code.hpp"
#include "endpoint.hpp"
#include "generic_async.hpp"
#include "io_service.hpp"

namespace io_service {
namespace ip {

class resolver {
public:
    typedef std::vector<endpoint> results_type;

private:
    io_service& m_serv;

public:
    resolver(io_service& serv)
        : m_serv(serv)
    {}

public:
    std::vector<endpoint>
    resolve(std::string host, std::string port) {
        results_type eps;
        // TODO: handle all ai_family and ai_socktype in general way
        M_get_results(
            eps, host, port,
            AF_INET, /*IP4*/
            SOCK_STREAM /*sequenced reliable byte stream*/);

        return eps;
    }

    template<typename CompHandler, typename std::enable_if_t<
        std::is_invocable_v<CompHandler,async_result<results_type> >,
        int> = 0>
    void async_resolve(std::string host, std::string port, CompHandler&& comp) {
        generic_async_poster<io_service> poster(m_serv);
        poster.post<results_type>(
            ::io_service::detail::make_async_op_wrapper<results_type>(
                std::bind(&resolver::resolve, this, std::move(host), std::move(port))),
            std::forward<CompHandler>(comp)); 
    }

private:
   void M_get_results(
        results_type& eps,
        std::string& host, std::string& port,
        int family, int socktype);

}; // class resolver

} // namespace ip
} // namespace io_service

#endif
