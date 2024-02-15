#ifndef ASIO_THREADSAFE_QUEUE_HPP
#define ASIO_THREADSAFE_QUEUE_HPP

#include "helgrind_annotations.hpp"

#include "mutex.hpp"
#include "condition_variable.hpp"
#include "lock_guard.hpp"
#include "unique_lock.hpp"
#include "false_func.hpp"

#include <memory> 
#include <mutex> // std::scoped_lock

namespace io_service {

template<typename T>
class threadsafe_queue {
private:
    struct node {
        T data;
        std::unique_ptr<node> next_node;
    };

private:
    std::unique_ptr<node> m_head;
    node* m_tail;

    concurrency::mutex m_head_mutex;
    concurrency::mutex m_tail_mutex;
    concurrency::condition_variable m_data_cv;
    
private:
    // TODO: Consider adding copying, depending on T
    threadsafe_queue(const threadsafe_queue& other) = delete;
    threadsafe_queue& operator=(const threadsafe_queue& other) = delete;

public:
    threadsafe_queue()
        : m_head(std::make_unique<node>()) /*dummy node*/
        , m_tail(m_head.get())
    {}

    // TODO: Find out if [other] should have appropriate state
    // Post: [other] is in consistent empty state
    threadsafe_queue(threadsafe_queue&& other)
        : threadsafe_queue()
    { swap(other); }

public:
    void push(T in_data) {
        using namespace concurrency;

        /* new dummy node */
        std::unique_ptr<node> new_node_ptr =
            std::make_unique<node>();
        node* new_tail = new_node_ptr.get();

        {
            lock_guard<mutex> lk(m_tail_mutex);
            m_tail->data = std::move(in_data);
            m_tail->next_node = std::move(new_node_ptr);
            m_tail = new_tail;
        }   

        // Note: helgrind might warn
        // on notifying cond_var without any lock
        m_data_cv.notify_one();
    }

public:
    bool try_pop(T& out_data) {
        using namespace concurrency;
        
        lock_guard<mutex> lk(m_head_mutex);
        if(m_head.get() == M_get_tail())
            return false;

        M_do_pop_head(out_data);
        return true;
    }

    template<typename Predicate = false_func>
    bool wait_and_pop(T& out_data, Predicate pred = Predicate()) {
        using namespace concurrency;

        unique_lock<mutex> lk(M_wait_for_data(pred));

        // if predicate is true, no data was fetched
        if(pred())
            return false;

        M_do_pop_head(out_data);
        return true;
    }

public:
    bool empty() {
        using namespace concurrency;
        lock_guard<mutex> lk(m_head_mutex);
        return m_head.get() == M_get_tail();
    }

    // External signal to unblock threads waiting for data
    void signal()
    { 
        using namespace concurrency;
        lock_guard<mutex> lk(m_head_mutex);
        m_data_cv.notify_all();
    }

public:
    void swap(threadsafe_queue& other) {
        using std::swap;

        std::scoped_lock lk(
            m_head_mutex, m_tail_mutex,
            other.m_head_mutex, other.m_tail_mutex);
        
        swap(m_head, other.m_head);
        swap(m_tail, other.m_tail);
    }

    friend
    void swap(threadsafe_queue& a, threadsafe_queue& b)
    { a.swap(b); }

// Impl funcs
private:
    node* M_get_tail() {
        using namespace concurrency;
        lock_guard<mutex> lk(m_tail_mutex);
        return m_tail;
    }

    // Blocking wait for data
    // which can be awaken by true predicate and external signal()
    template<typename Predicate = false_func>
    concurrency::unique_lock<concurrency::mutex>
    M_wait_for_data(Predicate pred = Predicate()) {
        using namespace concurrency;
        unique_lock<mutex> lk(m_head_mutex);
        // If both queue not empty AND pred is true
        // Then no data will be fetched
        m_data_cv.wait(lk,
            [this, &pred] () { 
                return (m_head.get() != M_get_tail())
                    || pred();
            });
        return lk;
    }

    // Prereq: head_mutex - locked
    void
    M_do_pop_head(T& out_data) {
        out_data = std::move(m_head->data);
        std::unique_ptr<node> old_head = std::move(m_head);
        m_head = std::move(old_head->next_node);
    }

}; // class threadsafe_queue

} // namespace io_service


#endif
