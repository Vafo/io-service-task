#include <iostream>

#include "io_service.hpp"

namespace io_service {

namespace detail {

static inline void insert_cur_thread_to_pool(
    std::set<concurrency::thread::native_handle_type>& thread_pool,
    concurrency::mutex& thread_pool_mutex
) {
    using namespace concurrency;
    using insert_res = std::pair<
        std::set<concurrency::thread::native_handle_type>::iterator,
        bool
    >;

    lock_guard<mutex> lock(thread_pool_mutex);

    thread::native_handle_type 
        native_handle = concurrency::this_thread::get_native_id();
        
    insert_res res = thread_pool.insert(native_handle);
    if(!res.second) /*thread is already in m_thread_pool*/
        std::runtime_error("io_service: run: thread is already in thread pool");
}

} // namespace detail 

void io_service::run() {
    using namespace concurrency;

    /*add self to m_thread_pool*/
    detail::insert_cur_thread_to_pool(m_thread_pool, m_thread_pool_mutex);

    while(true) {
        invocable cur_task;

        /*wait for task*/
        {
            using namespace concurrency;

            // std::cout << "run: locking queue" << std::endl;
            unique_lock<mutex> lock(m_queue_mutex);
            // std::cout << "run: queue locked" << std::endl;
            m_queue_cv.wait(
                lock,
                [&] () { 
                    // std::cout << "wAITIN" << std::endl;
                    return (m_queue.size() > 0) || m_stop_src.stop_requested(); 
                }
            );

            if(m_stop_src.stop_requested())
                return;

            cur_task = m_queue.front();
            m_queue.pop();
        }

        /*execute task*/
        cur_task();

        if(m_stop_src.stop_requested())
            return;
    }
}


bool io_service::stop()
{ 
    using namespace concurrency;

    m_stop_src.request_stop();

    { // notify about stop using m_queue_cv, since workers are waiting on it
        unique_lock<mutex> lock(m_queue_mutex);
        m_queue_cv.notify_all(); /*notify all*/
    }

    /*erase threads pool and tasks queue*/
    std::queue<invocable> empty_queue;
    {
        /*TODO: use scoped_lock instead*/
        std::lock(m_thread_pool_mutex, m_queue_mutex);
        lock_guard<mutex> thread_lock(m_thread_pool_mutex, adopt_lock);
        lock_guard<mutex> task_lock(m_queue_mutex, adopt_lock);
        
        m_thread_pool.clear();
        m_queue.swap( empty_queue ); /*clear queue by swapping with empty queue*/
    }

    return true;
}

} // namespace io_service