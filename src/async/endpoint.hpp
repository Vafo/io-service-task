#ifndef ASIO_ENDPOINT_HPP
#define ASIO_ENDPOINT_HPP


#include <cassert>
#include <netinet/in.h>
#include <sys/socket.h>
#include <utility>


namespace io_service {
namespace ip {

namespace detail {

// Forward declaration for socket.hpp
class async_connect_init;

} // namespace detail


class endpoint {
private:
    sockaddr m_addr;
    socklen_t m_len;

private:
    // used by resolver
    endpoint(sockaddr& addr, socklen_t& len)
        : m_addr(addr)
        , m_len(len) 
    {}

public:
    endpoint()
        : m_addr()
        , m_len(0)
    {}

    endpoint(in_port_t port)
        : endpoint()
    {
        assert(sizeof(sockaddr) >= sizeof(sockaddr_in));

        sockaddr_in* in_addr_ptr = reinterpret_cast<sockaddr_in*>(&m_addr);
        in_addr_ptr->sin_family = AF_INET;
        in_addr_ptr->sin_addr.s_addr = INADDR_ANY;
        in_addr_ptr->sin_port = htons(port);
        
        m_len = sizeof(sockaddr_in);
    }

    endpoint(const endpoint& other)
        : m_addr(other.m_addr)
        , m_len(other.m_len)
    {}

    endpoint& operator=(const endpoint& other) {
        endpoint(other).swap(*this);
        return *this;
    }

public:
    void swap(endpoint& other) {
        using std::swap;
        swap(m_addr, other.m_addr);
        swap(m_len, other.m_len);
    }

private:
    // Used by async_connect_init
    sockaddr& get_addr() // TODO: consider returning a copy
    { return m_addr; }

    socklen_t get_len()
    { return m_len; }

private:
friend class resolver;
friend class detail::async_connect_init;

}; // class endpoint

} // namespace ip
} // namespace io_service

#endif
