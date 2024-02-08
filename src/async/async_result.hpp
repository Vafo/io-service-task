#ifndef ASIO_ASYNC_RESULT
#define ASIO_ASYNC_RESULT

#include <memory>
#include <atomic>
#include <stdexcept>
#include <utility>


namespace io_service {

namespace old_ver {

namespace detail {

template<typename ResT>
class async_result_base {
protected:
    // TODO: consider turning to uninit buffer
    // so as to support cstr on set_res
    ResT m_res;

public:
    async_result_base() {}
    virtual ~async_result_base() {}

public:
    void set_res(ResT&& val) {
        // Could have been like
        // new (m_res_buf) (std::forward<T>(val));
        m_res = std::forward<ResT>(val);
        exec(m_res);
    }

protected:
    virtual void exec(ResT&) = 0;

}; // class async_result_base

// async result with completion handler
template<typename CompHandler, typename ResT>
class async_result_comp
    : public async_result_base<ResT>
{
private:
    CompHandler m_comp;
    using async_result_base<ResT>::m_res;

public:
    async_result_comp(CompHandler&& comp)
        : m_comp(std::forward<CompHandler>(comp))
    {}

protected:
    std::enable_if_t<
        std::is_invocable_v<CompHandler, async_result_base<ResT>>>
    exec(ResT& res)
    { m_comp(res); /*pass ownership to handler*/}

}; // class async_result_comp

} // namespace detail


// type erasure of completion handler
template<typename ResT>
class async_result {
private:
    typedef detail::async_result_base<ResT> as_base;

private:
    std::unique_ptr<as_base> m_ptr;

private:
    async_result() = delete;
    async_result(const async_result& other) = delete;
    async_result& operator=(const async_result& other) = delete;

public:
    template<typename CompHandler>
    async_result(CompHandler&& handler)
        : m_ptr(
            std::make_unique<
            detail::async_result_comp<CompHandler, ResT>>(
                std::forward<CompHandler>(handler)))
    {}

    // TODO: check if default works
    async_result(async_result&& other) = default;
    async_result& operator=(async_result&& other) = default;

public:
    void set_result(ResT&& res) {
        if(m_ptr)
            m_ptr->set_res(std::forward<ResT>(res));

        m_ptr.reset();
    }

public:
    operator bool()
    { return static_cast<bool>(m_ptr); }

}; // class async_result

} // namespace old_ver


/* 
 * async_res should invoke completion handler on being set
 * there is got to be a way to set and get result of async op
 *
 * on creation of async_res, completion handler
 * and type of value should be set
 *
 */

// Forward Declaration
template<typename T>
class async_result;


template<typename T>
class async_result_base {
private:
    alignas(alignof(T))
    unsigned char m_buf[sizeof(T)];
    std::atomic<bool> m_is_set;

public:
    async_result_base()
        : m_is_set(false)
    {}

    virtual ~async_result_base() {}

public:
    void set_val(T&& val) {
        if(m_is_set)
            throw std::runtime_error(
                "async_result_base: value is already set");

        new ( reinterpret_cast<T*>(std::addressof(m_buf)) )
            T(std::forward<T>(val)); 

        m_is_set = true;
    }

    T& get_val() {
        if(!m_is_set)
            throw std::runtime_error(
                "async_result_base: value not set");

        T* ret = reinterpret_cast<T*>(std::addressof(m_buf));
        return *ret;
    }

public:
    virtual void execute_comp_handler(async_result<T>&& result) = 0;

}; // class async_result_base

template<typename T, typename CompHandler>
class async_result_comp
    : public async_result_base<T>
{
private:
    CompHandler m_comp;

public:
    async_result_comp(CompHandler&& comp)
        : m_comp(std::forward<CompHandler>(comp))
    {}

public:
    void
    execute_comp_handler(async_result<T>&& result) {
        // transfer ownership to completion handler 
        m_comp(std::forward<async_result<T>>(result));
    }

}; // class async_result_comp

template<typename T>
class async_result {
private:
    std::unique_ptr<async_result_base<T>> m_base_ptr; 

private:
    async_result(const async_result& other) = delete;
    async_result& operator=(const async_result& other) = delete;

public:
    template<typename CompHandler>
    async_result(CompHandler&& handler)
        : m_base_ptr(
            std::make_unique<async_result_comp<T, CompHandler>>(
                std::forward<CompHandler>(handler)))
                
    {}

    async_result(async_result&& other)
        : m_base_ptr( std::move(other.m_base_ptr) )
    {}

public:
    void set_result(T&& res) {
        m_base_ptr->set_val(std::forward<T>(res));
        m_base_ptr->execute_comp_handler(std::move(*this));
    }

    T& get_result() {
        return m_base_ptr->get_val();
    }

}; // class async_result

} // namespace io_service

#endif
