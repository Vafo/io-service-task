#ifndef ASIO_THREAD_DATA_MNGR
#define ASIO_THREAD_DATA_MNGR

#include <memory>

#include "interrupt_flag.hpp"

namespace io_service {

// RAII manager of thread_local resources
class thread_data_mngr {
	std::unique_ptr<interrupt_handle>& m_int_hndl_ref;

private:
	thread_data_mngr() = delete; /*explicit*/

	thread_data_mngr(const thread_data_mngr& other) = delete;
	thread_data_mngr& operator=(const thread_data_mngr& other) = delete;

	thread_data_mngr(const thread_data_mngr&& other) = delete;
	thread_data_mngr& operator=(const thread_data_mngr&& other) = delete;

public:
	thread_data_mngr(
		std::unique_ptr<interrupt_handle>& int_hndl,
		std::unique_ptr<interrupt_handle> allocated_handle
	)
		: m_int_hndl_ref(int_hndl)
	{
		m_int_hndl_ref = std::move(allocated_handle);	
	}


	~thread_data_mngr() {
		// TODO: decide if unique_ptr.reset() is better or not
		m_int_hndl_ref = std::unique_ptr<interrupt_handle>();
	}

}; // class thread_data_mngr

}; // namespace io_service

#endif
