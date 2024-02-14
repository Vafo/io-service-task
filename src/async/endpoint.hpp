#ifndef ASIO_ENDPOINT_HPP
#define ASIO_ENDPOINT_HPP


#include <experimental/bits/fs_fwd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <tuple>


namespace io_service {
namespace ip {

class endpoint {
private:
    sockaddr m_addr;
    socklen_t m_len;

private:
    // used by resolver
    endpoint(sockaddr&& addr, socklen_t&& len)
        : m_addr(std::forward<sockaddr>(addr))
        , m_len(std::forward<socklen_t>(len)) 
    {}

public:
    endpoint()
        : m_addr()
        , m_len(0)
    {}

    endpoint(in_port_t port)
    {}

    endpoint(const endpoint& other)
    {}

    endpoint& operator=(const endpoint& other) {
        return *this;
    }

private:
friend class resolver;

}; // class endpoint

} // namespace ip
} // namespace io_service

#endif
