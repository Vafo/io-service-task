#ifndef ASIO_GENERIC_ASYNC_HPP
#define ASIO_GENERIC_ASYNC_HPP

#include "async_result.hpp"
#include "io_service.hpp"
#include <type_traits>

namespace io_service {

template<typename ResT, typename AsyncOp, typename 
    std::enable_if_t<std::is_same_v<ResT, std::result_of_t<AsyncOp()>>,
int> = 0>
class generic_async_op_wrapper {
private:
    AsyncOp m_op;

public:
    generic_async_op_wrapper(AsyncOp&& op)
        : m_op(std::forward<AsyncOp>(op))
    {}

public:
    void operator() (async_result<ResT> res) {
        res.set_result(
            m_op());
    }
};

// Helper function
template<typename ResT, typename AsyncOp>
generic_async_op_wrapper<ResT, std::decay_t<AsyncOp>>
make_async_op_wrapper(AsyncOp&& op) {
    return generic_async_op_wrapper<ResT, AsyncOp>(std::forward<AsyncOp>(op));
}

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
