#include <iostream>
#include <vector>

#include "condition_variable.hpp"
#include "jthread.hpp"

#include "io_service.hpp"
#include "mutex.hpp"
#include "resolver.hpp"
#include "endpoint.hpp"
#include "socket.hpp"

#include "async_connect.hpp"
#include "unique_lock.hpp"

void worker_func(io_service::io_service* service_ptr) {
    service_ptr->run();
}

int main(int argc, char* argv[]) {

    if(argc != 3) {
        std::cerr << "Usage: " << argv[0]
            << " host port" << std::endl;
        return 1;
    }

    const int num_threads = 1;
    io_service::io_service serv;
    std::vector<concurrency::jthread> threads;
    
    for(int i = 0; i < num_threads; ++i)
        threads.emplace_back(worker_func, &serv);

    
    io_service::ip::resolver res(serv);

    std::vector<io_service::ip::endpoint> eps = 
        res.resolve(argv[1], argv[2]);

    std::cout << "aboba " << eps.size() << std::endl;

    io_service::ip::socket sock(serv);

    concurrency::condition_variable cond_var;
    concurrency::mutex mut;
    bool done = false;

    async_connect(sock, eps,
        [&cond_var, &mut, &done] (int cqe_res) {
            std::cout << "i got " << cqe_res << std::endl;

            using namespace concurrency;
            unique_lock<mutex> lk(mut);

            done = true;
            cond_var.notify_one();
        });

    {
        using namespace concurrency;
        unique_lock<mutex> lk(mut);
        cond_var.wait(lk, [&done] () { return done; });
    }

    std::cout << "terminating" << std::endl;
    serv.stop();
    return 0;
}
