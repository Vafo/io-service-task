#ifndef ASIO_ASYNC_TASK
#define ASIO_ASYNC_TASK

#include <memory>

namespace io_service {

class async_result;

namespace detail {

// async_init interface
class async_init_base {
public:
    virtual void operator()() = 0;
    virtual ~async_init_base() {}
}; // class base_async_init

// async_init concrete obj
// AsyncOp  async operation
// callStrat defines how to call async op (e.g. pass required args)
// ResultT an obj on which async op sets result/failure/done
template<typename AsyncOp, typename callStrat>
class async_init_owner
    : public async_init_base {

private:
    AsyncOp m_as_op;
    callStrat m_strat;
    
public:
    async_init_owner(AsyncOp&& as_op, callStrat&& strat)
        : m_as_op(std::move(as_op))
        , m_strat(std::move(strat))
    {}

public:
    void operator()()
    { m_strat(m_as_op); }
};

} // namespace detail


// type erasure of async op initiator
class async_init {
private:
    std::unique_ptr<detail::async_init_base> m_base_ptr;

public:
    async_init()
        : m_base_ptr()
    {}

    template<typename AsyncOp, typename callStrat>
    async_init(AsyncOp&& as_op, callStrat&& strat)
        : m_base_ptr(
            std::make_unique<detail::async_init_owner>(
                std::forward<AsyncOp>(as_op),
                std::forward<callStrat>(strat)))
    {}

public:
    void operator()() {
        if(m_base_ptr)
            m_base_ptr->operator()();
    }

}; // class async_init

// type erasure of async op completor
class async_compl {

};

class async_task {
    async_init m_init;
    async_compl m_compl;
}; // class async_task

class async_result {
    async_compl m_compl;
}; // class async_result

} // namespace io_service

#endif
