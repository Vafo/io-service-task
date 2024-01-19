#ifndef IO_SERVICE_H
#define IO_SERVICE_H

#include "helgrind_annotations.hpp"
#include <queue>
#include <atomic>

#include <stdexcept>
#include <type_traits>

#include "mutex.hpp"
#include "lock_guard.hpp"
#include "condition_variable.hpp"

#include "function.hpp"
#include "shared_ptr.hpp"

#include <future>
#include "invocable.hpp"
#include "interrupt_flag.hpp"
#include "threadsafe_queue.hpp"

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
	threadsafe_queue<task_type> m_global_queue;
	interrupt_flag m_manager;
/*
	vector<local_work_steal_queue_ptr> local_queues;
	interrupt_flags global_int_flags;
*/
    
private:
	io_service(const io_service& other) = delete;
	io_service& operator=(const io_service& other) = delete;

	// TODO: Find out if it makes sense to have moveable io_service
	io_service(io_service&& other) = delete;
	io_service& operator=(io_service&& other) = delete;

public:
    io_service()
    {}

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
    post(Callable func, Args ...args) {
        // TODO: Check validity of io_service state before proceeding 
		M_check_validity();

		std::packaged_task<Signature> new_task(func);
		// obtain future of task
		std::future<return_type> fut(new_task.get_future());
		task_type inv_task(std::move(new_task), args...);
		M_post_task(std::move(inv_task));
        return fut;
    }

    template<typename Callable, typename ...Args,
		typename return_type = std::result_of_t<Callable(Args...)>,
		typename Signature = return_type(Args...)>
	std::future<return_type>
    dispatch(Callable func, Args ...args) {
		std::future<return_type> fut_res;

        if( M_is_in_pool() ) {
            /*if this_thread is among m_thread_pool, execute input task immediately*/
            // func(args...);
			std::packaged_task<Signature> task(func);
			fut_res = task.get_future();
			// No need to create task_type, invoke packaged_task directly
			task(args...);
        } else {
            fut_res = post(func, args...);
        }

        return fut_res;
    }

public:
    void stop();

    void restart();

// Impl funcs
private:
	bool M_try_fetch_task(invocable& out_task);
	void M_post_task(invocable new_task);
	bool M_is_in_pool();

	void M_check_validity() noexcept(false);
	void M_clear_tasks();

}; // class io_service

} // namespace io_service

#endif
