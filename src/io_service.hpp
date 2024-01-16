#ifndef IO_SERVICE_H
#define IO_SERVICE_H

#include <future>
#include <queue>
#include <mutex> /*std::lock*/
#include <atomic>

#include <exception>
#include <type_traits>

#include "thread.hpp"
#include "mutex.hpp"
#include "lock_guard.hpp"
#include "condition_variable.hpp"

#include "function.hpp"
#include "shared_ptr.hpp"

#include <future>
#include "invocable.hpp"


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
/*
	threadsafe_queue<invocable> global_queue;
	vector<local_queue_ptr> local_queues;
	std::atomic<int> counter; // of threads
	interrupt_flags global_int_flags;
*/

private: 
    std::queue<invocable> m_queue;
    concurrency::condition_variable m_queue_cv;
    concurrency::mutex m_queue_mutex;
    bool m_stop_flag; // tied to m_queue mutex and cond var

    // Used to hold io_service waiting for workers to finish
    concurrency::mutex m_stop_signal_mutex;
    concurrency::condition_variable m_stop_signal_cv;

public:
    io_service()
    {}

    ~io_service() {
        stop();
    }

public:
    void run();

    bool stop();

    bool restart();

public:
    template<typename Callable, typename ...Args>
	std::future<std::result_of_t<Callable()>>
    post(Callable func, Args ...args) {
        // _m_check_service_valid_state(__FUNCTION__);
		typedef std::result_of_t<Callable()> return_type;
		std::packaged_task<return_type()> new_task(
			pack_task_and_args(func, args...));

		// obtain future of task
		std::future<return_type> fut(new_task.get_future());

		invocable inv_task(new_task);
	
		{
			using namespace concurrency;
			lock_guard<mutex> lk(m_queue_mutex);
			m_queue.push(inv_task);
			m_queue_cv.notify_one();
		}
        /*notify about new task*/
        // lock_guard<mutex> lock(m_queue_mutex);

        return fut;
    }

    template<typename Callable, typename ...Args>
    bool dispatch(Callable func, Args ...args) {

        if( 1 /*is in pool*/ ) {
            /*if this_thread is among m_thread_pool, execute input task immediately*/
            func(args...);
        } else {
            post(func, args...);
        }

        return true;
    }

// Impl funcs
private:
	template<typename Callable, typename ...Args>
 	std::packaged_task<std::result_of_t<Callable()>()>
	pack_task_and_args(Callable calb, Args... args) {
		typedef std::result_of_t<Callable()> return_type;
		std::packaged_task<return_type()> new_task(
			// store args in lambda
			[calb, args...]() -> return_type {
				return calb(args...);
			});

		return new_task;
	}

}; // class io_service


} // namespace new_impl


} // namespace io_service

#endif
