#ifndef ASIO_URING_HPP
#define ASIO_URING_HPP

#include <algorithm>

#include <cassert>
#include <cerrno>
#include <cstring>
#include <liburing.h>
#include <liburing/io_uring.h>
#include <stdexcept>

#include "detail/scoped_uring.hpp"
#include "uring_error.hpp"

namespace io_service {

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

// Used by uring
private:
    uring_cqe(io_uring& owner, io_uring_cqe* cqe_ptr)
        : m_ring_owner(&owner)
        , m_cqe_ptr(cqe_ptr)
    {}

public:
    uring_cqe()
        : m_ring_owner(NULL)
        , m_cqe_ptr(NULL)
    {}

    // TODO: find out if it is a proper move cstr
    uring_cqe(uring_cqe&& other)
        : uring_cqe() // fill with nulls
    { swap(other); } // do primitive data types require explicit assignment?

    uring_cqe& operator=(uring_cqe&& other) {
        uring_cqe(std::move(other)).swap(*this);
        return *this;
    }

    ~uring_cqe() {
        if(operator bool())
            seen();
    }

public:
    void seen() {
        io_uring_cqe_seen(m_ring_owner, m_cqe_ptr);
        m_ring_owner = NULL;
        m_cqe_ptr = NULL;
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

    uint32_t get_flags() const {
        M_check_validity();
        return m_cqe_ptr->flags;
    }

    operator bool() const
    { return m_ring_owner && m_cqe_ptr; }

public:
    void swap(uring_cqe& other) {
        using std::swap;
        swap(m_ring_owner, other.m_ring_owner);
        swap(m_cqe_ptr, other.m_cqe_ptr);
    }

private:
    void M_check_validity() const {
        if(!operator bool())
            throw std::runtime_error("uring_cqe is empty");
    }

private:
    friend class uring;

}; // class uring_cqe


struct uring_shared_wq_t {};
constexpr uring_shared_wq_t uring_shared_wq;

// io_uring wrapper
class uring {
private:
    detail::scoped_uring m_scoped_ring;

private:
    uring(const uring& other) = delete;
    uring& operator=(const uring& other) = delete;

    uring(uring&& other) = delete;
    uring& operator=(uring&& other) = delete;

public:
    uring()
        : m_scoped_ring()
    {} 

    uring(uring_shared_wq_t)
        : m_scoped_ring(detail::uring_shared_wq) // TODO: ugly solution of common type with detail
    {}

public:
    inline uring_sqe get_sqe();

    inline bool try_get_cqe(uring_cqe& cqe);

public:
    inline void submit();

}; // class uring

} // namespace io_service

#include "impl/uring.ipp"

#endif

