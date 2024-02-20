#ifndef ASIO_URING_ASYNC_HPP
#define ASIO_URING_ASYNC_HPP


#include "uring.hpp"
#include "buffer.hpp"
#include "async_result.hpp"
#include "base_async.hpp"

#include <liburing.h>
#include <type_traits>
#include <utility>

namespace io_service {
namespace detail {

class uring_res_ent {
private:
    thread_local static int s_res_counter;

private:
    async_result<int> m_async_res;
    int m_res_id;

private:
    uring_res_ent(const uring_res_ent& other) = delete;
    uring_res_ent& operator=(const uring_res_ent& other) = delete;

public:
    uring_res_ent(uring_res_ent&& other)
        : m_async_res(std::move(other.m_async_res))
        , m_res_id(std::move(other.m_res_id))
    {}

    explicit
    uring_res_ent(async_result<int>&& async_res)
        : m_async_res(std::move(async_res))
        , m_res_id(S_get_next_id())
    {}

public:
    void set_res(int res)
    { m_async_res.set_result(res); }

public:
    int get_id()
    { return m_res_id; }

private:
    static int S_get_next_id()
    { return s_res_counter++; }

}; // class uring_res_ent



} // namespace detail


// per thread object, which tracks uring_async tasks
class uring_async_core {
public:
    typedef 
        std::list<detail::uring_res_ent>::size_type
        size_type;

private:
    uring m_ring;
    std::list<detail::uring_res_ent> m_res_entrs;


public:
    uring_async_core()
        : m_ring(uring_shared_wq)
    {}

public:
    void check_completions() {
        uring_cqe cqe;

        if(!m_ring.try_get_cqe(cqe))
            return;

        // TODO: is it suitable for any kind of io_uring op?
        if(cqe.get_flags() & IORING_CQE_F_MORE)
            return;

        int id = cqe.get_data();
        int res = cqe.get_res();
        cqe.seen(); /*erase cqe, so it is removed from io_uring*/

        typedef std::list<detail::uring_res_ent>::iterator ent_it;
        std::list<detail::uring_res_ent>& entries =
            m_res_entrs;

        // find entry among expected results
        ent_it iter = entries.begin();
        while(iter != entries.end()) {
            if(iter->get_id() == id)
                break;
            ++iter;
        }
        
        if(iter == entries.end())
            throw std::runtime_error(
                "entry was not present in expected uring results");

        detail::uring_res_ent res_ent = std::move(*iter);
        entries.erase(iter);

        res_ent.set_res(res);
    }

    size_type size() const
    { return m_res_entrs.size(); }

public:
    // Returns ID to be set in uring_sq as data field
    int push_result(async_result<int>&& res) {
        detail::uring_res_ent new_ent(std::forward<async_result<int>>(res));
        int id = new_ent.get_id();

        m_res_entrs.push_back(
            std::move(new_ent));

        return id;
    }

public:
    uring& get_ring()
    { return m_ring; }

}; // class uring_async_core


// uring_async_poster details
namespace detail {

template<typename Executor, typename AsyncOp,
    typename std::enable_if_t<
        std::is_invocable_v<AsyncOp, uring_sqe&>, int> = 0>
auto get_uring_async_op(Executor& exec, AsyncOp&& op) {
    return
    // Note: Storing Executor reference
    [&exec = exec, m_op(std::forward<AsyncOp>(op)) /*move into lambda*/]
    (async_result<int>&& res) mutable {
        uring_async_core& core = exec.get_local_uring_core();
        uring& ring = core.get_ring();
        uring_sqe sqe = ring.get_sqe();
        m_op(sqe);
         
        // add async_result entry to list
        int id = core.push_result(
            std::forward<async_result<int>>(res));
    
        // set id of uring completion
        sqe.set_data(id);
        ring.submit();
    };
}

template<typename CompHandler,
    typename std::enable_if_t<
        std::is_invocable_v<CompHandler, int>, int> = 0>
auto get_uring_async_comp(CompHandler&& comp) {
    return
    [m_comp(std::forward<CompHandler>(comp))]
    (async_result<int>&& res) mutable {
        int val = res.get_result();
        m_comp(val);
    };
}

} // namespace detail

// TODO: Check for refactoring
template<typename Executor>
class uring_async_poster
    : private base_async<Executor>
{
private:
    typedef base_async<Executor> base_class;

public:
    uring_async_poster(Executor& exec)
        : base_class(exec)
    {} 

public:
    // AsyncOp is expected to have one argument: uring_sqe&
    // CompHandler is expected to have one argument: int (cqe_res)
    template<typename AsyncOp, typename CompHandler>
    void post(AsyncOp&& op, CompHandler&& comp) {
        Executor& exec = base_class::get_executor();
        base_class::template post_async<int>(
            detail::get_uring_async_op(
                exec, std::forward<AsyncOp>(op)),
            detail::get_uring_async_comp(
                std::forward<CompHandler>(comp)));
    }

}; // class uring_async_poster

} // namespace io_service

#endif // ASIO_BASIC_SOCKET_HPP
