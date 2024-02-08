#ifndef IO_SERVICE_H
#define IO_SERVICE_H

#include "helgrind_annotations.hpp"

#include <iostream>
#include <stdexcept>
#include <type_traits>

#include <future>
#include <utility>
#include "invocable.hpp"
#include "interrupt_flag.hpp"
#include "threadsafe_queue.hpp"
#include "false_func.hpp"

#include "async_result.hpp"
#include "async_task.hpp"
#include "uring.hpp"

namespace io_service {

// Service is stopped
class service_stopped_error: public std::logic_error {
public:
    service_stopped_error(std::string in_str): std::logic_error(in_str)
    {}

    virtual ~service_stopped_error() throw() /*according to std exceptions*/{}
}; // class service_stopped_error


class io_service {
private:
    typedef invocable task_type;

private:
    // async tasks and general tasks are pushed here
    // TODO: check if it impacts efficiency of async
    threadsafe_queue<task_type> m_global_queue;

    interrupt_flag m_manager;
   
private:
    io_service(const io_service& other) = delete;
    io_service& operator=(const io_service& other) = delete;

    io_service(io_service&& other) = delete;
    io_service& operator=(io_service&& other) = delete;

public:
    io_service() {
        m_manager.add_callback_on_stop(
            [this] () { m_global_queue.signal(); });
    }

    ~io_service() {
        stop();
    }

public:
    void run();

    void run_pending_task();

public:
    template<typename Callable, typename ...Args,
        typename return_type = std::result_of_t<Callable(Args...)>,
        typename Signature = return_type(Args...)>
    std::future<return_type>
    post_waitable(Callable&& func, Args ...args) {
        // Check validity of io_service state before proceeding 
        M_check_validity();

        std::packaged_task<Signature> new_task(
            std::forward<Callable>(func));
        // obtain future of task
        std::future<return_type> fut(new_task.get_future());

        M_post_task(std::move(new_task), args...);

        return fut;
    }

    template<typename Callable, typename ...Args,
        typename return_type = std::result_of_t<Callable(Args...)>,
        typename Signature = return_type(Args...)>
    std::future<return_type>
    dispatch_waitable(Callable&& func, Args ...args) {
        std::future<return_type> fut_res;

        if( M_is_in_pool() ) {
            /*if this_thread is among m_thread_pool, execute input task immediately*/
            // func(args...);
            std::packaged_task<Signature> task(
                std::forward<Callable>(func));
            fut_res = task.get_future();
            // No need to create task_type, invoke packaged_task directly
            task(args...);
        } else {
            fut_res = post_waitable(
                std::forward<Callable>(func), args...);
        }

        return fut_res;
    }

public:
    // Post/Dispatch tasks without future

    template<typename Callable, typename ...Args,
        typename return_type = std::result_of_t<Callable(Args...)>,
        typename Signature = return_type(Args...)>
    void
    post(Callable&& func, Args ...args) {
        // Check validity of io_service state before proceeding 
        M_check_validity();

        std::packaged_task<Signature> new_task(
            std::forward<Callable>(func));

        M_post_task(std::move(new_task), args...);
    }

    template<typename Callable, typename ...Args,
        typename return_type = std::result_of_t<Callable(Args...)>,
        typename Signature = return_type(Args...)>
    void
    dispatch(Callable&& func, Args ...args) {
        if( M_is_in_pool() ) {
            /*if this_thread is among m_thread_pool, execute input task immediately*/
            std::packaged_task<Signature> task(
                std::forward<Callable>(func));
            // No need to create task_type, invoke packaged_task directly
            task(args...);
        } else {
            post(std::forward<Callable>(func), args...);
        }
    }

public:
    void stop();

    void restart();

public:
    // requirement of strand
    bool can_dispatch()
    { return M_is_in_pool(); }

// Impl funcs
private:

    template<typename SignatureT, typename ...Args>
    void M_post_task(std::packaged_task<SignatureT>&& pack_task, Args... args) {
        invocable new_task(
            std::forward<
                std::packaged_task<SignatureT>>(pack_task),
            args...);

        // TODO: in order to reduce std::move, make argument rval ref?
        // push to global
        m_global_queue.push(std::move(new_task));
    }

    bool M_try_fetch_task(task_type& out_task);

    // Returns true if task was fetched
    // Otherwise, predicate has disrupted it
    template<typename Predicate = false_func>
    bool M_wait_and_pop_task(task_type& out_task, Predicate pred = Predicate()) {
        return m_global_queue.wait_and_pop(out_task, pred);
    }

    bool M_is_in_pool();

    void M_check_validity() noexcept(false);
    void M_clear_tasks();

// async related
private:

    template<
        typename AsyncOp, typename CompHandler,
        typename ResT>
    void do_post_async(AsyncOp&& op, CompHandler&& comp)
    {
        async_result<ResT> as_res(
            std::forward<CompHandler>(comp));

        async_task task(
                std::forward<AsyncOp>(op),
                std::move(as_res));

        dispatch(task);
    }

    template<typename AsyncOp, typename CompHandler>
    void post_uring_async(
        AsyncOp&& as_op,
        CompHandler&& handler
    ) {
        do_post_async(
            get_uring_async_op(std::forward<AsyncOp>(as_op)),
            std::forward<CompHandler>(handler));
    }

private:
    template<typename AsyncOp>
    auto get_uring_async_op(AsyncOp&& op) {
        return
        [this, m_op(std::forward<AsyncOp>(op))]
        (async_result<int>&& res) {
            // TODO: add checking of uring CQ
            uring& ring = get_uring();
            m_op(ring);
        };
    }

    template<typename AsyncOp>
    auto get_generic_async_op() {
        return
        [this] (AsyncOp&& op) {
            dispatch(std::forward<AsyncOp>(op));
        };
    }

    template<typename ResT, typename CompHandler>
    auto get_generic_async_comp(CompHandler&& comp) {
        return
        [this, m_comp(std::forward<CompHandler>(comp))]
        (ResT& res) {
            dispatch(m_comp, res);
        };
    }

private:
    uring& get_uring();

private:
    template<typename UringInit, typename ComplHandler>
    friend void uring_async_poster();
    friend void inline_async_poster();

}; // class io_service

} // namespace io_service

#endif
