#pragma once

#include "main_processor.h"

#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/asio/post.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <boost/asio/io_context.hpp>

#include <map>
#include <vector>
#include <utility>
#include <deque>
#include <thread>
#include <memory>
#include <optional>

class GlobalStorage {
private:
    std::map<int, std::shared_ptr<GameContext>> Games_;
    boost::shared_mutex Mutex_;
    boost::asio::io_context ioContext_;
    boost::asio::thread_pool& Pool_;

    GlobalStorage(boost::asio::thread_pool& pool)
        : Pool_(pool) {
    }

    static GlobalStorage* Storage_;

public:
    static void Init(boost::asio::thread_pool& pool);
    static GlobalStorage& GetInstance();

    void AddTask(std::function<void()> f) {
        boost::asio::post(Pool_, std::move(f));
    }

    void AddGame(int userId1, int userId2);
    void RemoveGame(int userId1, int userId2);
    std::shared_ptr<GameContext> FindGameByUserId(int userId);
};
