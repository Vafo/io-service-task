#ifndef ASIO_GENERIC_ASYNC_HPP
#define ASIO_GENERIC_ASYNC_HPP

#include "io_service.hpp"
#include <type_traits>

namespace io_service {


class generic_async_poster {
private:
    io_service& m_serv;

private:
    generic_async_poster(const generic_async_poster& other) = delete;
    generic_async_poster& operator=(const generic_async_poster& other) = delete;

public:
    generic_async_poster(io_service& serv)
        : m_serv(serv)
    {}

public:
    template<typename ResT,
        typename AsyncOp, typename CompHandler>
    void post(AsyncOp&& op, CompHandler&& comp) {
        m_serv.post_generic_async<ResT>(
            std::forward<AsyncOp>(op),
            std::forward<CompHandler>(comp));
    }

}; // class generic_async_poster

} // namespace io_service

#endif
