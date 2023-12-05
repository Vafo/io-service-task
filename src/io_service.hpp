#ifndef IO_SERVICE_H
#define IO_SERVICE_H

#include <iostream>

#include <set>
#include <queue>
#include <stop_token>
#include <mutex> /*std::lock*/

#include "thread.hpp"
#include "mutex.hpp"
#include "condition_variable.hpp"

#include "function.hpp"


namespace io_service {

class io_service {

public:

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

        { /*query if current thread is already in pool*/
            using namespace concurrency;

            concurrency::thread::native_handle_type 
                native_handle = concurrency::this_thread::get_native_id();
            lock_guard<mutex> lock(m_thread_pool_mutex);

            is_in_pool = m_thread_pool.contains(native_handle);
        }

        if( is_in_pool ) {
            /*if this_thread is among m_thread_pool, execute input task immediately*/
            std::cout << "in pool called" << std::endl;
            func(args...);
        } else {
            std::cout << "out pool called" << std::endl;
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