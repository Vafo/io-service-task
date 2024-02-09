#ifndef ASIO_ASYNC_RESULT
#define ASIO_ASYNC_RESULT

#include <memory>
#include <atomic>
#include <stdexcept>
#include <type_traits>
#include <utility>


namespace io_service {

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
    template<typename D>
    std::enable_if_t<std::is_same_v<T,std::decay_t<D>>>
    set_val(D&& val) {
        if(m_is_set)
            throw std::runtime_error(
                "async_result_base: value is already set");

        new ( reinterpret_cast<T*>(std::addressof(m_buf)) )
            T(std::forward<D>(val)); 

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
    explicit
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
    explicit
    async_result(CompHandler&& handler)
        : m_base_ptr(
            std::make_unique<async_result_comp<T, CompHandler>>(
                std::forward<CompHandler>(handler)))
    {}

    async_result(async_result&& other)
        : m_base_ptr( std::move(other.m_base_ptr) )
    {}

public:
    // TODO: is there a better way to pass [res] as &&
    // in order to reduce redundant construction 
    // Should D and T be really the same?
    template<typename D>
    std::enable_if_t<std::is_same_v<T,std::decay_t<D>>>
    set_result(D&& res) {
        m_base_ptr->set_val(std::forward<D>(res));
        // TODO: find out if it is okay to std::move(*this)
        m_base_ptr->execute_comp_handler(std::move(*this));
    }

    T& get_result() {
        return m_base_ptr->get_val();
    }

}; // class async_result

} // namespace io_service

#endif
