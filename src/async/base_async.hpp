#ifndef ASIO_BASE_ASYNC_HPP
#define ASIO_BASE_ASYNC_HPP

#include "async_result.hpp"
#include "async_task.hpp"

namespace io_service {

template<typename Executor>
class base_async {
private:
    Executor& m_exec;

protected:
    base_async(Executor& exec)
        : m_exec(exec)
    {}

    ~base_async() {}

protected:
    template<
        typename ResT,
        typename AsyncOp, typename CompHandler>
    void post_async(AsyncOp&& op, CompHandler&& comp) {
        async_result<ResT> as_res(
            std::forward<CompHandler>(comp));

        async_task task(
            std::forward<AsyncOp>(op),
            std::move(as_res));

        // TODO: consider assigning a priority to async tasks.
        // Related to common work queue
        m_exec.post(std::move(task));
    }

    template<
        typename ResT,
        typename AsyncOp, typename CompHandler>
    void dispatch_async(AsyncOp&& op, CompHandler&& comp) {
        async_result<ResT> as_res(
            std::forward<CompHandler>(comp));

        async_task task(
            std::forward<AsyncOp>(op),
            std::move(as_res));

        // TODO: consider assigning a priority to async tasks.
        // Related to common work queue
        m_exec.dispatch(std::move(task));
    }


protected:
    Executor& get_executor()
    { return m_exec; }

}; // class base_async

}; // namespace io_service

#endif
