#ifndef ASIO_THREAD_DATA_MNGR
#define ASIO_THREAD_DATA_MNGR

#include <memory>

#include "interrupt_flag.hpp"

namespace io_service {

// RAII manager of thread_local resources
template<typename T>
class thread_data_mngr {
    std::unique_ptr<T>& m_data_ptr_ref;

private:
    thread_data_mngr() = delete; /*explicit*/

    thread_data_mngr(const thread_data_mngr& other) = delete;
    thread_data_mngr& operator=(const thread_data_mngr& other) = delete;

    thread_data_mngr(const thread_data_mngr&& other) = delete;
    thread_data_mngr& operator=(const thread_data_mngr&& other) = delete;

public:
    thread_data_mngr(
        std::unique_ptr<T>& data_ptr_ref
    )
        : m_data_ptr_ref(data_ptr_ref)
    {}


    ~thread_data_mngr() {
        // Note: is reset() good enough?
        m_data_ptr_ref.reset();
    }

}; // class thread_data_mngr

}; // namespace io_service

#endif
