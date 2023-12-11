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
    concurrency::lock_guard<concurrency::mutex> lock(m_queue_mutex);
    if(m_stop_flag == true) {
        std::string err_msg = "io_service: ";
        err_msg += func_name;
        err_msg += " service was already stopped. It can not be populated";
        throw std::runtime_error(err_msg);
    }
}

void io_service::_m_clear_tasks() {
    using namespace concurrency;

    std::queue<invocable> empty_queue;
    { 
        unique_lock<mutex> lock(m_queue_mutex);
        // m_queue_cv.notify_all(); /*notify all*/
        m_queue.swap( empty_queue ); /*clear queue by swapping with empty queue*/
    }
}

void io_service::_m_process_tasks() {
    while(true) {
        invocable cur_task;

        // Wait for task
        {
            using namespace concurrency;

            unique_lock<mutex> lock(m_queue_mutex);

            // Check if stop was requested, right before entering condition variable, which could be missed
            // Some threads might starve so much, that they will execute run() only after service was stopped
            // if(m_stop_flag == true)
            //     return;
            
            m_thread_counters_ptr->threads_idle++; /*thread is idle when it waits for task*/
            m_queue_cv.wait(
                lock,
                [&] () {
                    /*stop_src does send notification to cond var so as to stop it from waiting tasks*/ 
                    return (m_queue.size() > 0) || (m_stop_flag == true); 
                }
            );
            m_thread_counters_ptr->threads_idle--; /*thread is not idle when it gets task*/
            
            // Check if stop was requested
            if(m_stop_flag == true)
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

    { // notify about stop using m_queue_cv, since workers are waiting on it
        unique_lock<mutex> lock(m_queue_mutex);
        m_stop_flag = true; /*set stop flag*/
        m_queue_cv.notify_all(); /*notify all*/
    }

    // clear tasks
    _m_clear_tasks();

    // TODO: Wait for all threads to terminate run()
    while(m_thread_counters_ptr->threads_total != 0)
        ;

    return true;
}

bool io_service::restart() {
    using namespace concurrency;
    
    { // notify about resetted stop using m_queue_cv, since workers are waiting on it
        unique_lock<mutex> lock(m_queue_mutex);
        m_stop_flag = false; /*reset stop flag*/
        m_queue_cv.notify_all(); /*notify all*/
    }
    
    // clear tasks
    _m_clear_tasks();

    return true;
}

} // namespace io_service