#include "helgrind_annotations.hpp"
#include <catch2/catch_all.hpp>

#include "invocable.hpp"

namespace io_service {

TEST_CASE("invocable cstr & call") {
    const int var1 = 123;
    const int var2 = 5123;

    std::packaged_task<int()> task(
        [var1, var2] () -> int {
            return var1 + var2;
        });

    std::future<int> fut = task.get_future();
    invocable inv(std::move(task));

    inv();
    fut.wait();
    REQUIRE((var1 + var2) == fut.get());
}

TEST_CASE("make_invocable") {
    const int var1 = 4124;
    const int var2 = 2412;
    auto func = [] (int a, int b) -> int {
        return a - b;
    };
    const int answer = var1 - var2;

    SECTION("move cstr") {
        std::packaged_task<int(int,int)> task(func);
        std::future<int> fut = task.get_future();
        invocable inv(std::move(task), var1, var2);
        
        std::jthread tr(std::move(inv));
        fut.wait();
        REQUIRE(fut.get() == answer);
    }

    SECTION("move assignment") {
        std::packaged_task<int(int,int)> task(func);
        std::future<int> fut = task.get_future();
        invocable inv;
        inv = invocable(std::move(task), var1, var2);

        std::jthread tr(std::move(inv));
        fut.wait();
        REQUIRE(fut.get() == answer);
    }

    SECTION("vector of invocables") {
        const int iterations = 10;
        std::vector<invocable> ivec;
        std::vector<std::future<int>> fvec;
        for(int i = 0; i < iterations; ++i) {
            std::packaged_task<int(int,int)> task(func);
            fvec.push_back(task.get_future());
            ivec.push_back(
                invocable(
                    std::move(task),
                    var1, var2));
        }

        typedef std::vector<invocable>::iterator ivec_iter;
        auto exec_invocables = 
            [] (ivec_iter begin, ivec_iter end) {
                for(; begin != end; ++begin)
                    (*begin)();
            };

        typedef std::vector<std::future<int>>::iterator fvec_iter;
        auto check_futures =
            [] (fvec_iter begin, fvec_iter end) {
                for(; begin != end; ++begin) {
                    begin->wait();
                    REQUIRE(begin->get() == (answer));
                }
            };

        std::jthread tr1(exec_invocables, ivec.begin(), ivec.end());
        std::jthread tr2(check_futures, fvec.begin(), fvec.end());
    }
}

} // namespace io_service
