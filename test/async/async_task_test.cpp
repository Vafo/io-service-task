#include <catch2/catch_all.hpp>

#include <iostream>

#include "async_task.hpp"

namespace io_service {

TEST_CASE("async_task creation") {
    auto set_val = [] (async_result<int> res) {
        res.set_result(1);
    };

    auto get_val = [] (async_result<int> res) {
        REQUIRE(res.get_result() == 1);
        REQUIRE_THROWS(res.set_result(2));
    };

    async_result<int> res(get_val);

    async_task task(set_val, std::move(res)); 

    task();
}

} // namespace io_service
