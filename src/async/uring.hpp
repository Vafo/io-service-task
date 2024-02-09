#ifndef ASIO_URING_HPP
#define ASIO_URING_HPP

#include <cassert>
#include <cerrno>
#include <cstring>
#include <liburing.h>
#include <liburing/io_uring.h>
#include <stdexcept>

#include "cerror_code.hpp"

#define ASIO_URING_ENTRIES 4

namespace io_service {

inline
std::string uring_error_msg_gen(const char* prefix, int retval) {
    std::string msg(prefix);
    msg += ": ";
    msg += strerror(-retval);
    return msg;
}

class uring_error
    : public concurrency::util::cerror_code<int>
{
private:
    typedef concurrency::util::cerror_code<int> base_class;

public:
    uring_error()
        : base_class(
            "io_uring",
            uring_error_msg_gen,
            0)
    {}

public:
    void operator=(int retval)
    { base_class::operator=(retval); }

    operator bool() const
    { return base_class::operator bool(); }

}; // class uring_error

// Forward Declaration
class uring;

class uring_sqe {
private:
    io_uring_sqe* m_sqe_ptr;

private:
    uring_sqe() = delete;
    uring_sqe(const uring_sqe&) = delete;
    uring_sqe& operator=(const uring_sqe&) = delete;

private:
    // used by uring
    uring_sqe(io_uring_sqe* sqe_ptr)
        : m_sqe_ptr(sqe_ptr)
    {}

public:
    io_uring_sqe* get() const
    { return m_sqe_ptr; }

    void set_data(uint64_t data)
    { io_uring_sqe_set_data64(m_sqe_ptr, data); }

private:
    friend class uring;

}; // class uring_sqe

class uring_cqe {
private:
    io_uring* m_ring_owner;
    io_uring_cqe* m_cqe_ptr;

private:
    uring_cqe(const uring_cqe&) = delete;
    uring_cqe& operator=(const uring_cqe&) = delete;

public:
    uring_cqe()
        : m_ring_owner()
        , m_cqe_ptr()
    {}

// Used by uring
private:
    uring_cqe(io_uring& owner, io_uring_cqe* cqe_ptr)
        : m_ring_owner(&owner)
        , m_cqe_ptr(cqe_ptr)
    {}

    uring_cqe(uring_cqe&& other)
        : m_ring_owner(std::move(other.m_ring_owner))
        , m_cqe_ptr(std::move(other.m_cqe_ptr))
    {}

    uring_cqe& operator=(uring_cqe&& other) {
        uring_cqe(std::move(other)).swap(*this);
        return *this;
    }

public:
    ~uring_cqe() {
        if(*this) // if valid
            io_uring_cqe_seen(m_ring_owner, m_cqe_ptr);
    }

public:
    int get_res() const {
        M_check_validity();
        return m_cqe_ptr->res;
    }

    uint64_t get_data() const {
        M_check_validity();
        return io_uring_cqe_get_data64(m_cqe_ptr);
    }

    operator bool()
    { return m_ring_owner && m_cqe_ptr; }

public:
    void swap(uring_cqe& other) {
        using std::swap;
        swap(m_ring_owner, other.m_ring_owner);
        swap(m_cqe_ptr, other.m_cqe_ptr);
    }

private:
    void M_check_validity() const {
        if(!m_cqe_ptr || !m_ring_owner)
            throw std::runtime_error("uring_cqe is empty");
    }

private:
    friend class uring;

}; // class uring_cqe

// io_uring wrapper
class uring {
private:
    io_uring m_ring;    
/*
    int pending_async_ops_cnt;
*/
public:
    uring() {
        uring_error err;
        err = io_uring_queue_init(ASIO_URING_ENTRIES, &m_ring, 0);

        // if could not create io_uring
        if(err)
            err.throw_exception();
    }

    uring(const uring& other) {
        uring_error err;
        io_uring_params params = {
            .flags = IORING_SETUP_ATTACH_WQ,
            .wq_fd = static_cast<__u32>(other.m_ring.ring_fd) 
        };
        err = io_uring_queue_init_params(ASIO_URING_ENTRIES, &m_ring, &params);

        // if could not create io_uring
        if(err)
            err.throw_exception();
    }

public:
    uring_sqe get_sqe() {
        io_uring_sqe* sqe = io_uring_get_sqe(&m_ring);
        if(!sqe) {
            submit();
            sqe = io_uring_get_sqe(&m_ring);
        }

        return uring_sqe(sqe);
    }

    bool try_get_cqe(uring_cqe& cqe) {
        uring_error err;
        io_uring_cqe* cqe_ptr = NULL;
        err = io_uring_peek_cqe(&m_ring, &cqe_ptr);

        if(err) {
            if (err.value() == -EAGAIN) {
                return false;
            } else {
                err.throw_exception();
            }
        }
        
        assert(cqe_ptr != NULL && "cqe_ptr is null");
        cqe = uring_cqe(m_ring, cqe_ptr);
        return true;
    }

public:
    // check if io_uring has space for another task
    bool space_available() {
        return true;
    }

    // check if there is some unfinished task to be completed
    bool completion_pending() {
        return true;
    }

    void submit() {
        io_uring_submit(&m_ring);
    }

    // non-block checks if some async op completed,
    // then return its completion handler
    bool peek_completed_task() {
        return true;
    }

    // blocks until completion event, and returns completion handler
    void wait_completed_task() {
    }


}; // class uring

} // namespace io_service

#endif

