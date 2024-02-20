#include "io_service.hpp"
#include "interrupt_flag.hpp"
#include "thread_data_mngr.hpp"

#include "uring_async.hpp"

#include <liburing/io_uring.h>
#include <memory> // std::unique_ptr
#include <stdexcept>
#include <thread> // std::this_thread::yield()

namespace io_service {

struct thread_data {
public:
    uring_async_core m_uring_core;
    interrupt_handle m_int_handle;

public:
    thread_data(
        interrupt_handle&& int_handle
    )
        : m_uring_core()
        , m_int_handle(std::forward<interrupt_handle>(int_handle))
    {}

}; // struct thread_data


thread_local std::unique_ptr<thread_data> local_thread_data;


void io_service::run() {
    // Check if it is valid to interact with io_service
    // Throws if io_service is stopped
    M_check_validity();

    // Store pool-related data in thread_locals
    // If io_service is stopped, handle will be empty
    // thus, won't execute any tasks and return from run()
    // Alternative to throwing exception ^^^^^^^^^^^^^^^^^
    local_thread_data = std::make_unique<thread_data>(
        m_manager.make_handle()); // looks ugly

    // RAII release of pool-related worker data
    thread_data_mngr data_mngr(local_thread_data);

    auto is_stopped =
        [this] () { return local_thread_data->m_int_handle.is_stopped(); };

    uring_async_core& local_uring_core
        = local_thread_data->m_uring_core;

    // TODO: test if it really stops regardless of uring async
    while(!is_stopped()) {
        task_type task;
        
        if(local_uring_core.size() > 0) {
            local_uring_core.check_completions();
            if(!M_try_fetch_task(task))
                continue;
        } else {
            if(!M_wait_and_pop_task(task, is_stopped))
                break;
        }

        /*got task, execute it*/
        task();
    }

    // Release thread related resources, as we leave run() 
    // Released by thread_data_mngr
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
    m_manager.signal_stop();
    m_manager.wait_all();

    // Clear task queues
    M_clear_tasks();
}

void io_service::restart() {
    stop();

    interrupt_flag sink;
    m_manager.swap(sink);

    // reason of immovability of io_service
    m_manager.add_callback_on_stop(
        [this] () { m_global_queue.signal(); });
}

bool io_service::M_try_fetch_task(invocable& task) {
    // Note: future addition
    // can fetch from local / others / global
    return m_global_queue.try_pop(task);
}

bool io_service::M_is_in_pool() {
    if(local_thread_data) // TODO: ugly solution
        return m_manager.owns(local_thread_data->m_int_handle);

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
}

uring_async_core& io_service::get_local_uring_core() {
    if(!M_is_in_pool())
        throw std::runtime_error(
            "this thread has no uring in io_service");
    
    return local_thread_data->m_uring_core;
}


} // namespace io_service
