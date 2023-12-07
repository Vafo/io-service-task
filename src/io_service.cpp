#include <iostream>

#include "io_service.hpp"
#include <memory>

namespace io_service {

thread_local io_service::thread_counters_ptr_type local_thread_counters_ptr;

void io_service::_m_insert_into_pool() {
    local_thread_counters_ptr = m_thread_counters_ptr;
    local_thread_counters_ptr->threads_total++;
}

bool io_service::_m_is_in_pool() {
    return local_thread_counters_ptr == m_thread_counters_ptr;
}

void io_service::_m_release_from_pool() {
    local_thread_counters_ptr->threads_total--;
    local_thread_counters_ptr = thread_counters_ptr_type(); /*swap with empty*/
}

void io_service::_m_check_service_valid_state(const char* func_name) {
    if(m_stop_src.stop_requested()) {
        std::string err_msg = func_name;
        err_msg += "io_service: service was already stopped. It can not be populated";
        throw std::runtime_error(err_msg);
    }
}

void io_service::_m_process_tasks() {
    while(true) {
        invocable cur_task;

        // Wait for task
        {
            using namespace concurrency;

            unique_lock<mutex> lock(m_queue_mutex);
            
            m_thread_counters_ptr->threads_idle++; /*thread is idle when it waits for task*/
            m_queue_cv.wait(
                lock,
                [&] () {
                    /*stop_src does send notification to cond var so as to stop it from waiting tasks*/ 
                    return (m_queue.size() > 0) || m_stop_src.stop_requested(); 
                }
            );
            m_thread_counters_ptr->threads_idle--; /*thread is not idle when it gets task*/
            
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

    // TODO: Wait for all threads to terminate run()
    while(m_thread_counters_ptr->threads_total != 0)
        ;

    return true;
}

} // namespace io_service