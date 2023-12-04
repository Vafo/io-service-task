#ifndef IO_SERVICE_H
#define IO_SERVICE_H

#include <iostream>

#include <queue>
#include <stop_token>

#include <set>

#include "thread.hpp"
#include "mutex.hpp"
#include "condition_variable.hpp"

#include "function.hpp"

#include <mutex> /*std::lock*/

namespace io_service {

class io_service {

public:

    ~io_service() {
        stop();
    }

    void run();

    bool stop()
    { 
        using namespace concurrency;

        m_stop_src.request_stop();

        { // notify about stop using m_queue_cv, since workers are waiting on it
            unique_lock<mutex> lock(m_queue_mutex);
            m_queue_cv.notify_all(); /*notify all*/
        }

        /*erase threads pool and tasks queue*/
        /*TODO: use scoped_lock instead*/
        {
            std::lock(m_thread_pool_mutex, m_queue_mutex);
            lock_guard<mutex> thread_lock(m_thread_pool_mutex, adopt_lock);
            lock_guard<mutex> task_lock(m_queue_mutex, adopt_lock);
            
            m_thread_pool.clear();

            std::queue<invocable> empty_queue;
            m_queue.swap( empty_queue );
        }

        return true;
    }

    template<typename Callable, typename ...Args>
    bool post(Callable func, Args ...args) {
        using namespace concurrency;
        
        /*notify about new task*/
        // std::cout << "Trying to lock" << std::endl;
        unique_lock<mutex> lock(m_queue_mutex);
        // std::cout << "Locked" << std::endl;
        m_queue.push(func, args...);
        m_queue_cv.notify_all(); /*notify one by one, so that workers don't contend over single task*/
        // std::cout << "Notified" << std::endl;

        return true;
    }

    template<typename Callable, typename ...Args>
    bool dispatch(Callable func, Args ...args) {
        if(0) {/*if this_thread is among m_thread_pool*/
            func(args...);
        } else {
            post(func, args...);
        }

        return true;
    }

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

    /*TODO: add thread_id for reference in m_thread_pool*/
    std::set<concurrency::thread::native_handle_type> m_thread_pool; 
    concurrency::mutex m_thread_pool_mutex;
    
    std::queue<invocable> m_queue;
    concurrency::condition_variable m_queue_cv;
    concurrency::mutex m_queue_mutex;

    std::stop_source m_stop_src;

}; // class io_service

} // namespace io_service

#endif