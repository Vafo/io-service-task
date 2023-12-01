#ifndef IO_SERVICE_H
#define IO_SERVICE_H

#include <queue>
#include <stop_token>

#include "thread.hpp"
#include "mutex.hpp"
#include "condition_variable.hpp"

#include "function.hpp"

namespace io_service {

class io_service {

public:

    void run();

    bool stop();

    template<typename Callable, typename ...Args>
    bool post(Callable func, Args ...args) {
        using namespace concurrency;
        
        /*notify about new task*/
        unique_lock<mutex> lock(m_queue_mutex);
        m_queue.push(func, args...);
        m_queue_cv.notify_all();
    }

    template<typename Callable, typename ...Args>
    bool dispatch(Callable func, Args ...args) {
        if(0) {/*if this_thread is among m_thread_pool*/
            func(args...);
        } else {
            post(func, args...);
        }
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

    std::vector<concurrency::thread> m_thread_pool;
    concurrency::mutex m_thread_pool_mutex;
    
    std::queue<invocable> m_queue;
    concurrency::condition_variable m_queue_cv;
    concurrency::mutex m_queue_mutex;

}; // class io_service

} // namespace io_service

#endif