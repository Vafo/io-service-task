#ifndef ASIO_CALLSTACK_HPP
#define ASIO_CALLSTACK_HPP

namespace io_service {


template<typename Key, typename Value = unsigned char>
class callstack {
public:
    class context {
    private:
        Key* m_key;
        Value* m_val;

        context* next;

    private:
        context(const context& other) = delete;
        context& operator=(const context& other) = delete;

    public:
        explicit
        context(Key* key_ptr)
            : m_key(key_ptr)
            , m_val(reinterpret_cast<Value*>(this)) /*put in [this], so val is not empty*/
            , next(callstack::m_top)
        {
            callstack::m_top = this;
        }

        context(Key* key_ptr, Value& val)
            : m_key(key_ptr)
            , m_val(&val)
            , next(callstack::m_top)
        { callstack::m_top = this; }

        ~context() {
            callstack::m_top = next;
        }

        Key* get_key()
        { return m_key; }

        Value* get_value()
        { return m_val; }

    private:
        friend class callstack;

    }; // class context

private:
    callstack() = delete; /*static class*/
    callstack(callstack& other) = delete;
    callstack& operator=(callstack& other) = delete;

public:
    static Value* contains(Key* key) {
        context* cntx = m_top;
        while(cntx) {
            if(cntx->get_key() == key)
                return cntx->get_value();
            cntx = cntx->next;
        }

        return nullptr;
    }

private:
    // ptr to last context in stack
    static thread_local context* m_top;

private:
    friend class context;

}; // class callstack

template<typename Key, typename Value>
thread_local typename callstack<Key, Value>::context*
    callstack<Key, Value>::m_top = nullptr;

}; // namespace io_service

#endif
