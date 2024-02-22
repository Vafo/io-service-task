#ifndef ASIO_DETAIL_URING_ERROR_HPP
#define ASIO_DETAIL_URING_ERROR_HPP

#include "cerror_code.hpp"

#include <string>
#include <cstring>

namespace io_service {

inline
std::string uring_error_msg_gen(const char* prefix, int retval) {
    std::string msg(prefix);
    msg += ": ";
    msg += strerror(-retval);
    return msg;
}

class uring_error
    : public concurrency::util::cerror_code<int>
{
private:
    typedef concurrency::util::cerror_code<int> base_class;

public:
    uring_error()
        : base_class(
            "io_uring",
            uring_error_msg_gen,
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
