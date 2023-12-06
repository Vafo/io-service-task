#ifndef SHARED_PTR_H
#define SHARED_PTR_H

#include <cassert>

#include "checked_delete.hpp"

namespace io_service::util {

template<typename T>
class shared_ptr {
public:
    shared_ptr(T *ptr = NULL): impl(NULL) {
        if(ptr != NULL)
            impl = new shared_ptr_impl(ptr);
    }

    // allocate copy of obj
    shared_ptr(const T& obj): impl(NULL) {
        // TODO: Reduce to 1 allocator call
        T *ptr = allocator.allocate(1);
        allocator.construct(ptr, obj);
        impl = new shared_ptr_impl(ptr);
    }

    // TODO: Check for availability of conversion. It should not be only derived -> base

    // allocate copy of obj
    // Argument is derived object
    template<typename D>
    shared_ptr(const D& obj): impl(NULL) {
        static_assert(std::is_base_of<T, D>::value);

        std::allocator<D> d_allocator;
        D *ptr = d_allocator.allocate(1);
        d_allocator.construct(ptr, obj);
        impl = new shared_ptr_impl(ptr);
    }

    // Take ownership of obj
    // Argument is pointer to derived object
    template<typename D>
    shared_ptr(const D* obj): impl(NULL) {
        static_assert(std::is_base_of<T, D>::value);

        if(obj != NULL)
            impl = new shared_ptr_impl(obj);
    }
    

    shared_ptr(const shared_ptr& other): impl(other.impl) {
        if(impl != NULL) {
            ++impl->ref_count;
        }
    }

    T&
    operator*() {
        assert(impl != NULL);
        return *impl->obj;
    }

    const T&
    operator*() const {
        assert(impl != NULL);
        return *impl->obj;
    }

    T*
    operator->() {
        assert(impl != NULL);
        return impl->obj;
    }

    const T*
    operator->() const {
        assert(impl != NULL);
        return impl->obj;
    }

    shared_ptr&
    operator= (shared_ptr other) {
        // copy and swap
        swap(*this, other);
        
        return *this;
    }

    ~shared_ptr() {
        dec_n_check();
    }

private:
    class shared_ptr_impl {
    public:
        shared_ptr_impl(T *ptr): obj(ptr), ref_count(1) {}

        ~shared_ptr_impl() {
            // checked delete
            check_if_deletable(obj);        

            allocator.destroy(obj);
            allocator.deallocate(obj, 1);
        }

        T *obj;
        int ref_count;
        std::allocator<T> allocator;
    };

    shared_ptr_impl* impl;
    std::allocator<T> allocator;

    void dec_n_check() {
        if(impl != NULL) {
            --impl->ref_count;
            if(impl->ref_count == 0)
                delete impl;
        }
    }


public:
    friend void swap(shared_ptr &a, shared_ptr &b) {
        using std::swap;

        swap(a.impl, b.impl);
        // Is there need for swapping allocator?
        swap(a.allocator, b.allocator);
    }

    bool operator== (const shared_ptr &b) const {
        return impl->obj == b.impl->obj;
    }

    bool operator!= (const shared_ptr &b) const {
        return !(*this == b);
    }
};

} // namespace util

#endif