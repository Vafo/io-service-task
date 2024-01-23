#include <catch2/catch_all.hpp>

#include "callstack.hpp"

#include "jthread.hpp"
#include "mutex.hpp"
#include "lock_guard.hpp"

namespace io_service {

concurrency::mutex require_mutex;

class stack_check {
public:
    void test_in_stack(bool is_true)
    { 
        using namespace concurrency; 
        // trying to avoid helgrind warnings
        lock_guard<mutex> lk(require_mutex);
        REQUIRE(in_stack() == is_true);
    }

    void execute_in_stack() {
        callstack<stack_check>::context cntx(this);
        test_in_stack(true);
    }

public:
    bool in_stack()
    { return callstack<stack_check>::contains(this) != nullptr; }

}; // class stack_check

TEST_CASE("basic callstack") {
    stack_check obj;

    obj.test_in_stack(false/*no stack marker*/);

    obj.execute_in_stack();

    obj.test_in_stack(false/*no stack marker*/);

    {
        callstack<stack_check>::context cntx(&obj);
        obj.test_in_stack(true/*stack marker placed*/);
        obj.execute_in_stack();
    }

    obj.test_in_stack(false/*no stack marker*/);
}

TEST_CASE("callstack & threads") {
    stack_check obj;

    callstack<stack_check>::context cntx(&obj); 
    obj.test_in_stack(true);

    concurrency::jthread tr1([] () { 
        stack_check obj;
        obj.test_in_stack(false);
    });
    
    concurrency::jthread tr2([] () { 
        stack_check obj;
        obj.execute_in_stack();
    });

    concurrency::jthread tr3([] () { 
        stack_check obj;
        callstack<stack_check>::context cntx(&obj);
        obj.test_in_stack(true);
    });
}

} // namespace io_service
