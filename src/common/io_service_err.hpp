#ifndef ASIO_IO_SERVICE_ERR_HPP
#define ASIO_IO_SERVICE_ERR_HPP

#include "cerror_code.hpp"
#include <cstring>

namespace io_service {

inline
std::string io_service_err_msg_gen(const char* prefix, int retval) {
    std::string msg(prefix);
    msg += ": ";
    msg += strerror(retval);
    return msg;
}

class io_service_err
    : public concurrency::util::cerror_code<int>
{
private:
    typedef concurrency::util::cerror_code<int> base_class;

public:
    io_service_err()
        : base_class(
            "io_service",
            io_service_err_msg_gen,
            0)
    {}

public:
    void operator=(int retval)
    { base_class::operator=(retval); }

    operator bool() const
    { return base_class::operator bool(); }

}; // class uring_error

} // namespace io_service

#endif
