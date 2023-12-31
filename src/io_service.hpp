#ifndef IO_SERVICE_H
#define IO_SERVICE_H

#include <set>
#include <queue>
#include <stop_token>
#include <mutex> /*std::lock*/
#include <atomic>

#include "thread.hpp"
#include "mutex.hpp"
#include "condition_variable.hpp"

#include "function.hpp"
#include "shared_ptr.h"


namespace io_service {

class io_service {

public:
    struct thread_counters {
        thread_counters():
            threads_total(0),
            threads_idle(0)
        {}

        alignas(int) std::atomic<int> threads_total;
        alignas(int) std::atomic<int> threads_idle;
    }; // struct thread_counters

    typedef util::shared_ptr<thread_counters> thread_counters_ptr_type;


    io_service(): 
        m_thread_counters_ptr( new thread_counters() /*TODO: consider alignment*/)
    {}

    ~io_service() {
        stop();
    }

    void run();

    bool stop();

    template<typename Callable, typename ...Args>
    bool post(Callable func, Args ...args) {
        using namespace concurrency;
        _m_check_service_valid_state(__FUNCTION__);
        
        /*notify about new task*/
        unique_lock<mutex> lock(m_queue_mutex);
        
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

    std::size_t task_size() {
        using namespace concurrency;

        lock_guard<mutex> lock(m_queue_mutex);
        return m_queue.size();
    }

    bool empty()
    { return task_size() == 0; }

    bool all_idle()
    { return m_thread_counters_ptr->threads_total == m_thread_counters_ptr->threads_idle; }

private:
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
    void _m_process_tasks();
    void _m_check_service_valid_state(const char* func_name);
    
    std::queue<invocable> m_queue;
    concurrency::condition_variable m_queue_cv;
    concurrency::mutex m_queue_mutex;

    std::stop_source m_stop_src;
    thread_counters_ptr_type m_thread_counters_ptr;

}; // class io_service

} // namespace io_service

#endif