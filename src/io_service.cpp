#include <iostream>

#include "io_service.hpp"

namespace io_service {

void io_service::run() {
    using namespace concurrency;

    /*add self to m_thread_pool*/
    /*TODO: add this_thread namespace*/
    using insert_res = std::pair<
        std::set<concurrency::thread::native_handle_type>::iterator,
        bool
    >;

    concurrency::thread::native_handle_type 
        native_handle = concurrency::this_thread::get_native_id();

    {   /*insert thread id into pool*/
        // std::cout << "Trying to lock thread pool" << std::endl;
        lock_guard<mutex> lock(m_thread_pool_mutex);
        // std::cout << "thread pool locked" << std::endl;
        insert_res res = m_thread_pool.insert(native_handle);
        if(!res.second) /*thread is already in m_thread_pool*/
            std::runtime_error("io_service: run: thread is already in thread pool");
    }

    while(true) {
        invocable cur_task;

        /*wait for task*/
        {
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

} // namespace io_service