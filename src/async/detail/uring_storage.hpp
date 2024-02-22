#ifndef ASIO_DETAIL_URING_STORAGE_HPP
#define ASIO_DETAIL_URING_STORAGE_HPP

#include "uring_error.hpp"

#include <liburing.h>

#include <list>

#include <mutex.hpp>
#include <unique_lock.hpp>


#define ASIO_URING_ENTRIES 4

namespace io_service {
namespace detail {

class uring_storage {
private:
    concurrency::mutex s_rings_mut;
    std::list<int> s_rings;

public:
    void attach(io_uring& ring) {
        using namespace concurrency;
        unique_lock<mutex> lk(s_rings_mut);
        
        if(s_rings.empty()) {
            // create a ring and insert fd into list
            setup_solo_ring(ring);
            s_rings.push_back(ring.ring_fd);
            return;
        }

        int existing_fd = *s_rings.begin();
        uring_error err;
        io_uring_params params = {
            .flags = IORING_SETUP_ATTACH_WQ,
            .wq_fd = static_cast<__u32>(existing_fd) 
        };
        err = io_uring_queue_init_params(ASIO_URING_ENTRIES, &ring, &params);

        // if could not create io_uring
        if(err)
            err.throw_exception();
    }

    void detach(io_uring& ring) {
        using namespace concurrency;
        unique_lock<mutex> lk(s_rings_mut);

        int fd = ring.ring_fd;
        io_uring_queue_exit(&ring);

        s_rings.remove(fd);
    }

public:
    void setup_solo_ring(io_uring& ring) {
        uring_error err;
        err = io_uring_queue_init(ASIO_URING_ENTRIES, &ring, 0);

        if(err)
            err.throw_exception();
    }

}; // class uring_storage

} // namespace detail
} // namespace io_service

#endif
