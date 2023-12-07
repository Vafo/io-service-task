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
    typedef util::shared_ptr<std::atomic<int>> thread_counter_ptr_type;

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

    io_service(): 
        m_thread_counter_ptr( new std::atomic<int>(0) )
    {}


    ~io_service() {
        stop();
    }

    void run();

    bool stop();

    template<typename Callable, typename ...Args>
    bool post(Callable func, Args ...args) {
        using namespace concurrency;
        
        /*notify about new task*/
        unique_lock<mutex> lock(m_queue_mutex);
        
        invocable new_task(func, args...);
        m_queue.push(new_task);
        m_queue_cv.notify_one(); /*notify one. one task = one thread*/

        return true;
    }

    template<typename Callable, typename ...Args>
    bool dispatch(Callable func, Args ...args) {
        bool is_in_pool = false;

        if( _m_is_in_pool() ) {
            /*if this_thread is among m_thread_pool, execute input task immediately*/
            func(args...);
        } else {
            post(func, args...);
        }

        return true;
    }

    std::queue<invocable>::size_type task_size() {
        using namespace concurrency;

        lock_guard<mutex> lock(m_queue_mutex);
        return m_queue.size();
    }

    bool empty()
    { return task_size() == 0; }

private:
    void _m_insert_into_pool();
    bool _m_is_in_pool();
    void _m_release_from_pool();
    void _m_process_tasks();
    
    std::queue<invocable> m_queue;
    concurrency::condition_variable m_queue_cv;
    concurrency::mutex m_queue_mutex;

    std::stop_source m_stop_src;

    // TODO: consider alignment
    thread_counter_ptr_type m_thread_counter_ptr;

}; // class io_service

} // namespace io_service

#endif