#ifndef SHARED_PTR_H
#define SHARED_PTR_H

#include <atomic>

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
        using alloc_traits = std::allocator_traits< std::allocator<T> >; 
        // TODO: Reduce to 1 allocator call
        T *ptr = alloc_traits::allocate(allocator, 1);
        alloc_traits::construct(allocator, ptr, obj);
        impl = new shared_ptr_impl(ptr);
    }

    // TODO: Check for availability of conversion. It should not be only derived -> base

    // allocate copy of obj
    // Argument is derived object
    template<typename D>
    shared_ptr(const D& obj): impl(NULL) {
        static_assert(std::is_base_of<T, D>::value);
        using alloc_traits = std::allocator_traits< std::allocator<D> >; 

        std::allocator<D> d_allocator;
        D *ptr = alloc_traits::allocate(d_allocator, 1);
        alloc_traits::construct(d_allocator, ptr, obj);
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
            using alloc_traits = std::allocator_traits< std::allocator<T> >; 
            // checked delete
            check_if_deletable(obj);        

            alloc_traits::destroy(allocator, obj);
            alloc_traits::deallocate(allocator, obj, 1);
        }

        T *obj;
        alignas(int) std::atomic<int> ref_count;
        std::allocator<T> allocator;
    };

    shared_ptr_impl* impl;
    std::allocator<T> allocator;

    void dec_n_check() {
        if(impl != NULL) {
            int stored_val = impl->ref_count.load();
            while(!impl->ref_count.compare_exchange_weak(stored_val, stored_val - 1))
                ;

            if(stored_val - 1 == 0)
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