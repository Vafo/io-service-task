#ifndef ASIO_ASYNC_TASK
#define ASIO_ASYNC_TASK

#include "async_result.hpp"

#include <memory>
#include <type_traits>


namespace io_service {

namespace detail {

class async_task_base {
public:
    virtual void exec() = 0;
    virtual ~async_task_base() {}

}; // class async_task_base

template<typename AsyncOp, typename ResT>
class async_task_impl
    : public async_task_base
{
private:
    AsyncOp m_op;
    async_result<ResT> m_res;

public:
    async_task_impl(AsyncOp&& op, async_result<ResT>&& res)
        : m_op(std::forward<AsyncOp>(op))
        , m_res(std::forward<async_result<ResT>>(res))
    {}

public:
    std::enable_if_t<std::is_invocable_v<AsyncOp, async_result<ResT>>>
    exec()
    { m_op(std::move(m_res)); /*transfer ownership to async op*/ }

}; // class async_task_impl

} // namespace detail


class async_task {
private:
    std::unique_ptr<detail::async_task_base> m_ptr;

private:
    async_task(const async_task& other) = delete;
    async_task& operator=(const async_task& other) = delete;

public:
    async_task() {}

    template<typename AsyncOp, typename ResT>
    async_task(AsyncOp&& op, async_result<ResT>&& res)
        : m_ptr(
            std::make_unique<detail::async_task_impl<AsyncOp, ResT>>(
                std::forward<AsyncOp>(op),
                std::forward<async_result<ResT>>(res)))
    {}

    async_task(async_task&& other)
        : m_ptr( std::move(other.m_ptr) )
    {}

    async_task& operator=(async_task&& other) {
        async_task(std::move(other)).swap(*this);
        return *this;
    }

public:
    void operator()() {
        if(m_ptr)
            m_ptr->exec();
    }

public:
    void swap(async_task& other) {
        using std::swap;
        swap(m_ptr, other.m_ptr);
    }

}; // class async_task

} // namespace io_service

#endif
