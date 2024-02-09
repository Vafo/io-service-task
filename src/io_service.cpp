#include "io_service.hpp"
#include "interrupt_flag.hpp"
#include "thread_data_mngr.hpp"

#include "async_result.hpp"

#include <memory> // std::unique_ptr
#include <stdexcept>
#include <thread> // std::this_thread::yield()
#include <list>

namespace io_service {

class uring_res_ent {
private:
    thread_local static int s_res_counter;

private:
    async_result<int> m_async_res;
    int m_res_id;

private:
    uring_res_ent(const uring_res_ent& other) = delete;
    uring_res_ent& operator=(const uring_res_ent& other) = delete;

public:
    uring_res_ent(uring_res_ent&& other)
        : m_async_res(std::move(other.m_async_res))
        , m_res_id(std::move(other.m_res_id))
    {}

    explicit
    uring_res_ent(async_result<int>&& async_res)
        : m_async_res(std::move(async_res))
        , m_res_id(S_get_next_id())
    {}

public:
    void set_res(int res)
    { m_async_res.set_result(res); }

public:
    int get_id()
    { return m_res_id; }

private:
    static int S_get_next_id()
    { return s_res_counter++; }

}; // class uring_res_ent

thread_local int uring_res_ent::s_res_counter = 0;

struct thread_data {
public:
    interrupt_handle m_int_handle;
    uring m_ring;
    std::list<uring_res_ent> m_uring_res_entrs;

public:
    thread_data(
        interrupt_handle&& int_handle,
        uring&& ring,
        std::list<uring_res_ent>&& res_entrs
    )
        : m_int_handle(std::forward<interrupt_handle>(int_handle))
        , m_ring(std::forward<uring>(ring))
        , m_uring_res_entrs(std::forward<std::list<uring_res_ent>>(res_entrs))
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
        m_manager.make_handle(),
        uring(),
        std::list<uring_res_ent>()); // looks ugly

    // RAII release of pool-related worker data
    thread_data_mngr data_mngr(local_thread_data);

    auto is_stopped =
        [this] () { return local_thread_data->m_int_handle.is_stopped(); };

    // TODO: test if it really stops regardless of uring async
    while(!is_stopped()) {
        task_type task;
        
        if(local_thread_data->m_uring_res_entrs.size() > 0) {
            M_uring_check_completion();
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

// TODO: Learn if perfect forwarding could be suitable here
bool io_service::M_try_fetch_task(invocable& task) {
    // TODO: fetch from local / others / global
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

uring& io_service::M_uring_get_local_ring() {
    if(!M_is_in_pool())
        throw std::runtime_error(
            "this thread has no uring in io_service");
    
    return local_thread_data->m_ring;
}

void io_service::M_uring_check_completion() {
    uring_cqe cqe;
    if(!M_uring_get_local_ring().try_get_cqe(cqe))
        return;    

    typedef std::list<uring_res_ent>::iterator ent_it;
    std::list<uring_res_ent>& entries =
        local_thread_data->m_uring_res_entrs;

    int id = cqe.get_data();
    // find entry among expected results
    ent_it iter = entries.begin();
    while(iter != entries.end()) {
        if(iter->get_id() == id)
            break;
        ++iter;
    }
    
    if(iter == entries.end())
        throw std::runtime_error(
            "entry was not present in expected uring results");

    iter->set_res(cqe.get_res());
    entries.erase(iter);
}

// Returns ID to be set in uring_sq as data field
int io_service::M_uring_push_result(async_result<int>&& res) {
    if(!M_is_in_pool())
        throw std::runtime_error(
            "this thread has no uring in io_service");

    uring_res_ent new_ent(std::forward<async_result<int>>(res));
    int id = new_ent.get_id();

    local_thread_data->m_uring_res_entrs.push_back(
        std::move(new_ent));

    return id;
}

} // namespace io_service
