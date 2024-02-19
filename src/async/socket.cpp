#include "socket.hpp"

#include <stdexcept>

namespace io_service {
namespace ip {

int socket::M_setup_socket(int family, int socktype) {
    int fd;

    fd = ::socket(family, socktype, 0);
    if(fd < 0)
        throw std::runtime_error(
            "socket: could not create socket");

    return fd;
}

} // namespace ip
} // namespace io_service
