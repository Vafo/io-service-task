#include "resolver.hpp"

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


void resolver::M_get_results(
    results_type& eps,
    std::string& host, std::string& port,
    int family, int socktype
) {
    addrinfo hints = {
        .ai_flags = AI_NUMERICSERV, // port is numeric
        .ai_family = family, 
        .ai_socktype = socktype,  
        .ai_protocol = 0
    };

    // TODO: consider extracting cerror_code into general util namespace
    concurrency::util::cerror_code<int> err("resolver", detail::resolver_err_gen, 0);

    addrinfo* results = NULL;
    err = getaddrinfo(host.c_str(), port.c_str(), &hints, &results);
    if(err)
        err.throw_exception();

    addrinfo* adrp;
    for(adrp = results; adrp != NULL; adrp = adrp->ai_next) {
        if(
            adrp->ai_family != family
            || adrp->ai_socktype != socktype
        )
            continue; // handle only specific family and socktype
        sockaddr addr;
        memcpy(&addr, adrp->ai_addr, adrp->ai_addrlen);
        eps.push_back(
            endpoint(addr, adrp->ai_addrlen));
    }

    freeaddrinfo(results);
}

} // namespace ip
} // namespace io_service
