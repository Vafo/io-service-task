#ifndef ASIO_URING_ASYNC_HPP
#define ASIO_URING_ASYNC_HPP

#include "io_service.hpp"

#include "uring.hpp"
#include "async_task.hpp"
#include "buffer.hpp"

#include <liburing.h>
#include <utility>

namespace io_service {

class base_async_io_init {
protected:
    int m_fd;
    off_t m_offset;
    buffer m_buf;

private:
    base_async_io_init() = delete;
    base_async_io_init(const base_async_io_init&) = delete;
    base_async_io_init& operator=(const base_async_io_init&) = delete;

protected:
    base_async_io_init(int fd, buffer buf, off_t offset)
        : m_fd(fd)
        , m_buf(buf)
        , m_offset(offset)
    {}

}; // class base_async_io_init

class async_read_init
    : private base_async_io_init
{
public:
    async_read_init(int fd, buffer buf, off_t offset)
        : base_async_io_init(fd, buf, offset)
    {}

public:
    void operator()(uring& ring) {
        uring_sqe sqe = ring.get_sqe();

        io_uring_prep_read(
            sqe.get(),
            m_fd,
            m_buf.m_mem_ptr, m_buf.m_size,
            m_offset);

        ring.submit(); 
    }

}; // class async_read_init

class async_write_init
    : private base_async_io_init
{
public:
    async_write_init(int fd, buffer buf, off_t offset)
        : base_async_io_init(fd, buf, offset)
    {}

public:
    void operator()(uring& ring) {
        uring_sqe sqe = ring.get_sqe();

        io_uring_prep_write(
            sqe.get(),
            m_fd,
            m_buf.m_mem_ptr, m_buf.m_size,
            m_offset);

        ring.submit();
    }

}; // class async_write_init

template<typename CompletionHandler>
class base_io_completer {

    void operator()(int cqe_res) {
    }

}; // class base_io_completer

class async_accept_init {
private:
    int m_fd;
    // endpoint_info& m_peer;

public:
    async_accept_init(
        int fd /*, endpoint_info& peer*/
    )
        : m_fd(fd)
    {}

public:
    void operator()(uring& ring) {
        uring_sqe sqe = ring.get_sqe();

        io_uring_prep_accept(
            sqe.get(),
            m_fd,
            /*&m_peer.addr*/,
            /*&m_peer.addrlen*/,
            0);

        ring.submit();
    }

}; // class async_accept_init
 
// TODO: Check for refactoring
class uring_async_poster {
private:
    io_service& m_serv;

public:
    uring_async_poster(io_service& serv)
        : m_serv(serv)
    {} 

public:
    template<typename AsyncOp, typename CompHandler>
    void post(AsyncOp&& op, CompHandler&& comp) {
        m_serv.post_uring_async(
            std::forward<AsyncOp>(op),
            std::forward<CompHandler>(comp));
    }

}; // class uring_async_poster

} // namespace io_service

#endif // ASIO_BASIC_SOCKET_HPP
