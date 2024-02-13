#include "acceptor.hpp"

#include <stdexcept>
#include "io_service_err.hpp"

namespace io_service {
namespace ip {
namespace detail {

int socket_setup_accept() {
    io_service_err err;
    int server_sock = ::socket(AF_INET, SOCK_STREAM, 0);

    if(server_sock == -1)
        throw std::runtime_error("could not create socket");

    int optval = 1;
    err = setsockopt(
        server_sock,
        SOL_SOCKET,
        SO_REUSEADDR,
        &optval, sizeof(optval));
    if(err)
        throw std::runtime_error(
            "could not set sockopt SO_REUSEADDR");

    err = setsockopt(
        server_sock,
        SOL_SOCKET,
        SO_REUSEPORT,
        &optval, sizeof(optval));
    if(err)
        throw std::runtime_error(
            "could not set sockopt SO_REUSEPORT");

    return server_sock;
}

void socket_bind(int fd, in_port_t port) {
    io_service_err err;

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    err = 
        bind(fd, (struct sockaddr*) &addr, sizeof(addr));

    if(err)
        throw std::runtime_error(
            "could not bind server socket to addr");

}

void socket_listen(int fd, int num_connections) {
    io_service_err err;

    err = 
        listen(fd, num_connections);

    if(err)
        throw std::runtime_error(
            "could not listen socket");
}

} // namespace detail
} // namespace ip
} // namespace io_service
