#ifndef ASIO_GENERIC_ASYNC_HPP
#define ASIO_GENERIC_ASYNC_HPP

#include "async_result.hpp"
#include "base_async.hpp"
#include <type_traits>

namespace io_service {

namespace detail {
 
// TODO: Find out if it is redundant
// Dispatches OpT into Executor upon invocation of lambda
template<typename ResT, typename Executor, typename OpT>
auto get_generic_async_dispatcher(Executor& exec, OpT&& op) {
    return
    [&exec = exec, m_op(std::forward<OpT>(op))]
    (async_result<ResT>&& res) {
        exec.dispatch(
            std::move(m_op),
            std::forward<async_result<ResT>>(res));
    };
}

// Allows user to have AsyncOp
// signature to be simply ResT(),
// as opposed to void(async_result<ResT>)
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

} // namespace detail


template<typename Executor>
class generic_async_poster
    : private base_async<Executor>
{
private:
    typedef base_async<Executor> base_class;

private:
    generic_async_poster(const generic_async_poster& other) = delete;
    generic_async_poster& operator=(const generic_async_poster& other) = delete;

public:
    generic_async_poster(Executor& exec)
        : base_class(exec)
    {}

public:
    template<typename ResT,
        typename AsyncOp, typename CompHandler>
    void post(AsyncOp&& op, CompHandler&& comp) {
        Executor& exec = base_class::get_executor();
        base_class::template post_async<ResT>(
            detail::get_generic_async_dispatcher<ResT>(
                exec, std::forward<AsyncOp>(op)),
            detail::get_generic_async_dispatcher<ResT>(
                exec, std::forward<CompHandler>(comp)));
    }

}; // class generic_async_poster

} // namespace io_service

#endif
