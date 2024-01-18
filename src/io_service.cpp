#include "io_service.hpp"
#include "interrupt_flag.hpp"

#include <memory>
#include <thread>

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

    // maybe change to atomic compare_exchange
    if(local_thread_counters_ptr->threads_total == 0) {
        // release io_service from stop()
        using namespace concurrency;
        unique_lock<mutex> lock(m_stop_signal_mutex);
        m_stop_signal_cv.notify_one();
    }

    local_thread_counters_ptr = thread_counters_ptr_type(); /*swap with empty*/
}

void io_service::_m_check_service_valid_state(const char* func_name) {
    concurrency::lock_guard<concurrency::mutex> lock(m_queue_mutex);
    if(m_stop_flag == true) {
        std::string err_msg = "io_service: ";
        err_msg += func_name;
        err_msg += " service was already stopped. It can not be populated";
        throw service_stopped_error(err_msg);
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
            
            m_queue_cv.wait(
                lock,
                [&] () {
                    /*stop_src does send notification to cond var so as to stop it from waiting tasks*/ 
                    return (m_queue.size() > 0) || (m_stop_flag == true); 
                }
            );
            
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
    pool_inserter inserter(*this);
    _m_process_tasks();
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

    // Wait for all threads to terminate run()
    {
        unique_lock<mutex> lock(m_stop_signal_mutex);
        m_stop_signal_cv.wait(
            lock,
            [&] () {
                return m_thread_counters_ptr->threads_total == 0;        
            }
        );
    }

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


namespace new_impl {

thread_local std::unique_ptr<interrupt_handle> local_int_handle_ptr;

void io_service::run() {
	// Check if it is valid to interact with io_service
	M_check_validity();
	// Store pool-related data in thread_locals
	// interrupt_handle handle(m_manager);
	local_int_handle_ptr =
		std::make_unique<interrupt_handle>(m_manager.make_handle());
/*
	if(local_int_handle_ptr->empty())
		throw service_stopped_error("Service is stopped");
*/

	while(!m_manager.is_stopped())
		run_pending_task();

	// release handle
}

void io_service::run_pending_task() {
	invocable task;
	if(M_try_fetch_task(task)) {
		task();
	} else {
		std::this_thread::yield();
	}
}

void io_service::stop() {
	m_manager.stop_all();
	m_manager.wait_all();

	// Clear task queues
	M_clear_tasks();
}

void io_service::restart() {
	// set new interrupt manager
	interrupt_flag sink;
	m_manager.swap(sink);

	M_clear_tasks();
}

// TODO: Learn if perfect forwarding could be suitable here
bool io_service::M_try_fetch_task(invocable& task) {
	// TODO: fetch from local / others / global
	return m_global_queue.try_pop(task);
}

void io_service::M_post_task(invocable new_task) {
	if( 0 /*local_queue present*/) {
		// push to local
	} else {
		// TODO: in order to reduce std::move, make argument rval ref?
		// push to global
		m_global_queue.push(std::move(new_task));
	}
}

bool io_service::M_is_in_pool() {
	if(local_int_handle_ptr) // TODO: ugly solution
		return m_manager.owns(*local_int_handle_ptr);

	return false;
}

void io_service::M_check_validity() {
	if(m_manager.is_stopped())	
		throw service_stopped_error("Service is stopped");
}

void io_service::M_clear_tasks() {
	// clear global queue
	threadsafe_queue<task_type> sink(
			std::move(m_global_queue));

	// TODO: clear local queues
}

} // namespace new_impl

} // namespace io_service
