#ifndef ASIO_IMPL_URING_IPP
#define ASIO_IMPL_URING_IPP

#include "uring.hpp"

namespace io_service {


uring_sqe uring::get_sqe() {
    io_uring& ring = m_scoped_ring.get_ring();
    io_uring_sqe* sqe = io_uring_get_sqe(&ring);
    if(!sqe) {
        submit();
        sqe = io_uring_get_sqe(&ring);
    }

    return uring_sqe(sqe);
}

bool uring::try_get_cqe(uring_cqe& cqe) {
    uring_error err;
    io_uring& ring = m_scoped_ring.get_ring();

    io_uring_cqe* cqe_ptr = NULL;
    err = 
        io_uring_peek_cqe(&ring, &cqe_ptr);

    if(err) {
        if (err.value() == -EAGAIN) {
            return false;
        } else {
            err.throw_exception();
        }
    }
    
    assert(cqe_ptr != NULL);
    cqe = uring_cqe(ring, cqe_ptr);
    return true;
}

void uring::submit() {
    io_uring& ring = m_scoped_ring.get_ring();
    io_uring_submit(&ring);
}

} // namespace io_service

#endif
