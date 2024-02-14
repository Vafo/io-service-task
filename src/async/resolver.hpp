#ifndef ASIO_RESOLVER_HPP
#define ASIO_RESOLVER_HPP

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
private:
    io_service& m_serv;

public:
    resolver(io_service& serv)
        : m_serv(serv)
    {}

public:
    std::vector<endpoint>
    resolve(std::string host, std::string port) {
        std::vector<endpoint> eps;

        addrinfo hints = {
            .ai_flags = AI_NUMERICSERV, // port is numeric
            .ai_family = AF_INET, // IP4
            .ai_socktype = SOCK_STREAM, // sequenced reliable byte stream 
            .ai_protocol = 0
        };

        addrinfo* results = NULL;
        // TODO: consider extracting cerror_code into general util namespace
        concurrency::util::cerror_code<int> err("resolver", gai_strerror, 0);

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

            eps.push_back(
                endpoint(
                    std::move(*adrp->ai_addr),
                    std::move(adrp->ai_addrlen)));
        }

        return eps;
    }

    template<typename CompHandler, typename std::enable_if_t<
        std::is_invocable_v<CompHandler,async_result<std::vector<endpoint>> >,
        int> = 0>
    void async_resolve(std::string host, std::string port, CompHandler&& comp) {
        generic_async_poster poster(m_serv);
        poster.post<std::vector<endpoint>>(
            make_async_op_wrapper<std::vector<endpoint>>(
                std::bind(&resolver::resolve, this, std::move(host), std::move(port))),
            std::forward<CompHandler>(comp)); 
    }

}; // class resolver

} // namespace ip
} // namespace io_service

#endif
