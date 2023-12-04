#include <iostream>

#include "io_service.hpp"
#include "thread"

#include <chrono>

void worker(io_service::io_service* service_ptr) {
    service_ptr->run();
}

int main(int argc, char* argv[]) {
    
    io_service::io_service service;

    concurrency::jthread work1(worker, &service);

    service.post([] () {
        std::cout << "ABOBA" << std::endl;
    });


    service.post([] () {
        std::cout << "KEKA" << std::endl;
    });


    using namespace std::chrono_literals;
    std::this_thread::sleep_for(10ms);

    service.stop();

    return 0;
}