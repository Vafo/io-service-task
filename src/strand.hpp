#ifndef ASIO_STRAND_HPP
#define ASIO_STRAND_HPP

#include <queue>
#include <functional>

#include "monitor.hpp"
#include "callstack.hpp"
#include "function.hpp"

namespace io_service {

template<typename Processor>
/*
 * Processor should implement following interface
 * can_dispatch() - test if currently in worker thread of processor
 * post(Callable) - post strand::run() task to processor
 */
class strand {
private:
    struct strand_data {
        bool is_running;
        std::queue<func::function<void()>> que;
    };

private:
    monitor<strand_data> m_data;
    Processor& m_proc;

private:
    strand(const strand& other) = delete;
    strand& operator=(const strand& other) = delete;

public:
    strand(Processor& proc)
        : m_proc(proc)
    {}
    
public:
    template<typename Callable>
    void post(Callable handle) {
        bool trigger = m_data(
            [&handle] (strand_data& data ) {
                data.que.push(handle);
                if(data.is_running)
                    return false;
                /*strand is now running*/
                data.is_running = true;
                return true;
            });

        if(trigger) /*check if there is need to push run() to processor*/
            m_proc.post(std::bind(&strand::run, this));
    }

    template<typename Callable>
    void dispatch(Callable handle) {
        // if not in worker thread
        if(!m_proc.can_dispatch()) {
            post(handle);
            return;
        }

        // if in strand handle, execute immediately
        if(in_running_handle()) {
            handle();
            return;
        }

        bool trigger = m_data(
            [&handle] (strand_data& data) {
                // if it is a worker thread, and strand is being runned
                // just push handle into queue
                if(data.is_running) {
                    data.que.push(handle);
                    return false;
                }

                // if it is a worker thread and strand is not runned
                data.is_running = true;
                return true;
            });

        // if this thread is going to run the strand 
        if(trigger) {
            // set callstack context for handle
            typename callstack<strand>::context cntx(this);
            // execute handle immediately
            handle();

            // continue executing strand handlers
            run();
        }

    }

private:
    // Execute all available handlers of strand, then return
    void run() {
        // executing run(), let handles know about it
        typename callstack<strand>::context cntx(this);

        func::function<void()> handle;
        while(true) {
            bool trigger = m_data(
                [&handle] (strand_data& data) {
                    if(data.que.size()) {
                        handle = std::move(data.que.front());
                        data.que.pop();
                        return true;
                    }

                    data.is_running = false;
                    return false;
                });

            if(trigger)
                handle();
            else
                return;
        }
    }

private:
    bool in_running_handle()
    { return callstack<strand>::contains(this) != nullptr; }

}; // class strand

} // namespace io_service

#endif
