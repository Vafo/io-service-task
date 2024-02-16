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

namespace detail {

inline
std::string resolver_err_gen(const char* prefix, int err) {
    std::string res(prefix);
    res += ": ";
    res += gai_strerror(err);
    return res;
}

} // namespace detail

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

        addrinfo hints = {
            .ai_flags = AI_NUMERICSERV, // port is numeric
            .ai_family = AF_INET, // IP4
            .ai_socktype = SOCK_STREAM, // sequenced reliable byte stream 
            .ai_protocol = 0
        };

        addrinfo* results = NULL;
        // TODO: consider extracting cerror_code into general util namespace
        concurrency::util::cerror_code<int> err("resolver", detail::resolver_err_gen, 0);

        err = getaddrinfo(host.c_str(), port.c_str(), &hints, &results);
        if(err)
            err.throw_exception();

        addrinfo* adrp;
        for(adrp = results; adrp != NULL; adrp = adrp->ai_next) {
            // TODO: handle all ai_family and ai_socktype in general way
            if(
                adrp->ai_family != AF_INET
                || adrp->ai_socktype != SOCK_STREAM
            )
                continue; // handle only IP and sock stream
            sockaddr addr;
            memcpy(&addr, adrp->ai_addr, adrp->ai_addrlen);
            eps.push_back(
                endpoint(addr, adrp->ai_addrlen));
        }

        freeaddrinfo(results);
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

}; // class resolver

} // namespace ip
} // namespace io_service

#endif
