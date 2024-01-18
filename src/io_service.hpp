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
#include "thread_manager.hpp"
#include "threadsafe_queue.hpp"

namespace io_service {

class service_stopped_error: public std::logic_error {
public:
    service_stopped_error(std::string in_str): std::logic_error(in_str)
    {}

    virtual ~service_stopped_error() throw() /*according to std exceptions*/{}
}; // class service_stopped_error


class io_service {

public:
    struct thread_counters {
        thread_counters():
            threads_total(0)
        {}

        alignas(int) std::atomic<int> threads_total;
    }; // struct thread_counters

    typedef memory::shared_ptr<thread_counters> thread_counters_ptr_type;


    io_service(): 
        m_thread_counters_ptr( new thread_counters() /*TODO: consider alignment*/),
        m_stop_flag(false)
    {}

    ~io_service() {
        stop();
    }

    void run();

    bool stop();

    bool restart();

    template<typename Callable, typename ...Args>
    bool post(Callable func, Args ...args) {
        using namespace concurrency;
        _m_check_service_valid_state(__FUNCTION__);

        /*notify about new task*/
        lock_guard<mutex> lock(m_queue_mutex);
        
        invocable new_task(func, args...);
        m_queue.push(new_task);
        m_queue_cv.notify_one(); /*notify one. one task = one thread*/

        return true;
    }

    template<typename Callable, typename ...Args>
    bool dispatch(Callable func, Args ...args) {

        if( _m_is_in_pool() ) {
            /*if this_thread is among m_thread_pool, execute input task immediately*/
            func(args...);
        } else {
            post(func, args...);
        }

        return true;
    }

private:

    struct pool_inserter {

        pool_inserter(io_service& serv): m_serv(serv)
        { m_serv._m_insert_into_pool(); }

        ~pool_inserter()
        { m_serv._m_release_from_pool(); }
        
    private:
        io_service& m_serv;
    }; // struct pool_inserter

    struct invocable {
        invocable(): m_lambda() {}

        template<typename Callable, typename ...Args>
        invocable(Callable func, Args ...args) {
            m_lambda = [=] () -> void { func(args...); };
        }

        void operator()() {
            m_lambda();
        }

    private:
        func::function<void()> m_lambda;
    }; // struct invocable


    void _m_insert_into_pool();
    bool _m_is_in_pool();
    void _m_release_from_pool();
    void _m_clear_tasks();
    void _m_process_tasks();
    void _m_check_service_valid_state(const char* func_name);
    
    std::queue<invocable> m_queue;
    concurrency::condition_variable m_queue_cv;
    concurrency::mutex m_queue_mutex;
    bool m_stop_flag; // tied to m_queue mutex and cond var

    // Used to hold io_service waiting for workers to finish
    concurrency::mutex m_stop_signal_mutex;
    concurrency::condition_variable m_stop_signal_cv;

    thread_counters_ptr_type m_thread_counters_ptr;

}; // class io_service


namespace new_impl {

class io_service {
private:
	typedef invocable task_type;

private:
	threadsafe_queue<task_type> m_global_queue;
	thread_manager m_manager;
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
    bool stop();

    bool restart();

// Impl funcs
private:
	bool M_try_fetch_task(invocable& out_task);
	void M_post_task(invocable new_task);
	bool M_is_in_pool();

	void M_clear_tasks();

}; // class io_service


} // namespace new_impl


} // namespace io_service

#endif
