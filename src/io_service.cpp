#include <iostream>

#include "io_service.hpp"
#include <memory>

namespace io_service {

thread_local io_service::thread_counter_ptr_type thread_local_counter_ptr;

void io_service::_m_insert_into_pool() {
    // TODO
    /*modify thread local storage, so as to insert thread into pool*/
    thread_local_counter_ptr = m_thread_counter_ptr;
    (*thread_local_counter_ptr)++;
}

bool io_service::_m_is_in_pool() {
    // TODO
    return thread_local_counter_ptr == m_thread_counter_ptr;
}

void io_service::_m_release_from_pool() {
    // TODO
    /*modify thread local storage, so as to release thread from pool*/
    (*thread_local_counter_ptr)--;
    thread_local_counter_ptr = thread_counter_ptr_type(); /*swap with empty*/
}

void io_service::_m_process_tasks() {
    while(true) {
        invocable cur_task;

        // Wait for task
        {
            using namespace concurrency;

            unique_lock<mutex> lock(m_queue_mutex);
            m_queue_cv.wait(
                lock,
                [&] () {
                    /*stop_src does send notification to cond var so as to stop it from waiting tasks*/ 
                    return (m_queue.size() > 0) || m_stop_src.stop_requested(); 
                }
            );
            
            // Check if stop was requested
            if(m_stop_src.stop_requested())
                return;

            cur_task = m_queue.front();
            m_queue.pop();
        }

        // Execute task
        cur_task();
    }
}

void io_service::run() {
    _m_insert_into_pool(); /*add self to m_thread_pool*/
    _m_process_tasks();
    _m_release_from_pool();
}

bool io_service::stop()
{ 
    using namespace concurrency;

    m_stop_src.request_stop();

    { // notify about stop using m_queue_cv, since workers are waiting on it
        unique_lock<mutex> lock(m_queue_mutex);
        m_queue_cv.notify_all(); /*notify all*/
    }

    /*erase tasks queue*/
    std::queue<invocable> empty_queue;
    {
        lock_guard<mutex> task_lock(m_queue_mutex);
        m_queue.swap( empty_queue ); /*clear queue by swapping with empty queue*/
    }

    // TODO: Wait for all threads to finish
    while(*m_thread_counter_ptr != 0)
        ;

    return true;
}

} // namespace io_service