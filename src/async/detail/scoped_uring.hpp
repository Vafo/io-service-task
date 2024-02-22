#ifndef ASIO_DETAIL_SCOPED_URING_HPP
#define ASIO_DETAIL_SCOPED_URING_HPP

#include "uring_storage.hpp"

namespace io_service {
namespace detail {

// Note: a copy of uring.hpp datatype, used for indicating a specific cstr
struct uring_shared_wq_t {};
constexpr uring_shared_wq_t uring_shared_wq;

class scoped_uring {
private:
    static uring_storage S_storage;

private:
    io_uring m_ring;    

public:
    scoped_uring()
    { S_storage.setup_solo_ring(m_ring); }

    scoped_uring(uring_shared_wq_t)
    { S_storage.attach(m_ring); }

    ~scoped_uring()
    { S_storage.detach(m_ring); }

public:
    io_uring& get_ring()
    { return m_ring; }

}; // class scoped_uring

} // namespace detail
} // namespace io_service

#endif
