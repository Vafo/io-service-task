#include "io_service.hpp"
#include "interrupt_flag.hpp"

#include <memory>
#include <thread>

namespace io_service {

thread_local std::unique_ptr<interrupt_handle> local_int_handle_ptr;

void io_service::run() {
	// Check if it is valid to interact with io_service
	// Throws if io_service is stopped
	M_check_validity();

	// Store pool-related data in thread_locals
	// interrupt_handle handle(m_manager);
	// If io_service is stopped, handle will be empty
	// thus, won't execute any tasks and return from run()
	// Alternative to throwing exception ^^^^^^^^^^^^^^^^^
	local_int_handle_ptr =
		std::make_unique<interrupt_handle>(m_manager.make_handle());

	while(!local_int_handle_ptr->is_stopped())
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
	m_manager.signal_stop();
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

} // namespace io_service
