#ifndef IO_SERVICE_H
#define IO_SERVICE_H

#include "helgrind_annotations.hpp"

#include <stdexcept>
#include <type_traits>

#include <future>
#include <utility>
#include "invocable.hpp"
#include "interrupt_flag.hpp"
#include "threadsafe_queue.hpp"
#include "false_func.hpp"

#include "uring_async.hpp"

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
        typename return_type =
            std::result_of_t<std::decay_t<Callable>(std::decay_t<Args>...)>,
        typename Signature = return_type(std::decay_t<Args>...)>
    std::future<return_type>
    post_waitable(Callable&& func, Args&& ...args) {
        // Check validity of io_service state before proceeding 
        M_check_validity();

        std::packaged_task<Signature> new_task(
            std::forward<Callable>(func));
        // obtain future of task
        std::future<return_type> fut(new_task.get_future());

        M_post_task(std::move(new_task), std::forward<Args>(args)...);

        return fut;
    }

    template<typename Callable, typename ...Args,
        typename return_type =
            std::result_of_t<std::decay_t<Callable>(std::decay_t<Args>...)>,
        typename Signature = return_type(std::decay_t<Args>...)>
    std::future<return_type>
    dispatch_waitable(Callable&& func, Args&& ...args) {
        std::future<return_type> fut_res;

        if( M_is_in_pool() ) {
            /*if this_thread is among m_thread_pool, execute input task immediately*/
            // func(args...);
            std::packaged_task<Signature> task(
                std::forward<Callable>(func));
            fut_res = task.get_future();
            // No need to create task_type, invoke packaged_task directly
            task(std::forward<Args>(args)...);
        } else {
            fut_res = post_waitable(
                std::forward<Callable>(func), std::forward<Args>(args)...);
        }

        return fut_res;
    }

public:
    // Post/Dispatch tasks without future
    template<typename Callable, typename ...Args,
        typename return_type =
            std::result_of_t<std::decay_t<Callable>(std::decay_t<Args>...)>,
        typename Signature = return_type(std::decay_t<Args>...)>
    void
    post(Callable&& func, Args&& ...args) {
        // Check validity of io_service state before proceeding 
        M_check_validity();

        std::packaged_task<Signature> new_task(
            std::forward<Callable>(func));

        M_post_task(std::move(new_task), std::forward<Args>(args)...);
    }

    template<typename Callable, typename ...Args,
        typename return_type =
            std::result_of_t<std::decay_t<Callable>(std::decay_t<Args>...)>,
        typename Signature = return_type(std::decay_t<Args>...)>
    void
    dispatch(Callable&& func, Args&& ...args) {
        if( M_is_in_pool() ) {
            /*if this_thread is among m_thread_pool, execute input task immediately*/
            std::packaged_task<Signature> task(
                std::forward<Callable>(func));
            // No need to create task_type, invoke packaged_task directly
            task(std::forward<Args>(args)...);
        } else {
            post(std::forward<Callable>(func), std::forward<Args>(args)...);
        }
    }

public:
    void stop();

    void restart();

public:
    // requirement of strand
    bool can_dispatch()
    { return M_is_in_pool(); }

    // requirement of uring_async_poster
    uring_async_core&
    get_local_uring_core();

// Impl funcs
private:

    template<typename SignatureT, typename ...Args>
    void M_post_task(std::packaged_task<SignatureT>&& pack_task, Args&&... args) {
        invocable new_task(
            std::forward<
                std::packaged_task<SignatureT>>(pack_task),
            std::forward<Args>(args)...);

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

}; // class io_service

} // namespace io_service

#endif
