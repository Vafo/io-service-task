#ifndef ASIO_BUFFER_HPP
#define ASIO_BUFFER_HPP

#include <cstddef>
namespace io_service {

// buffer wrapper
// does not own underlying buffer
struct buffer {
    void* const m_mem_ptr;
    size_t const m_size;

    buffer(void* mem_ptr, size_t size)
        : m_mem_ptr(mem_ptr)
        , m_size(size)
    {}
}; 

} // namespace io_service

#endif
