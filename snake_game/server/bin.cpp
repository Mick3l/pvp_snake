#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/websocket.hpp>

#include "networking.h"

#include <iostream>

int main() {
    boost::system::error_code ec;
    boost::asio::io_context context;

    constexpr int threadCount = 6;
    boost::asio::thread_pool tp(threadCount);

    auto workGuard = boost::asio::make_work_guard(context);

    for (int i = 0; i < threadCount - 3; ++i) {
        boost::asio::post(tp, [&context]() {
            context.run();
        });
    }

    GlobalStorage::Init(tp);

    NetworkManager networkManager(context, 8080);
    networkManager.Start();

    std::cout << "Server running. Press Enter to stop." << std::endl;
    std::cin.get();

    workGuard.reset();

    networkManager.Stop();

    tp.join();
    tp.stop();

    return 0;
}
