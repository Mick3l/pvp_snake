#include "context.h"

#include "networking.h"

#include <boost/asio/steady_timer.hpp>
#include <iostream>

#include <chrono>

GlobalStorage* GlobalStorage::Storage_ = {};

void GlobalStorage::Init(boost::asio::thread_pool& pool) {
    if (Storage_) {
        throw std::runtime_error("GlobalStorage is already initialized");
    }
    Storage_ = new GlobalStorage(pool);
}

GlobalStorage& GlobalStorage::GetInstance() {
    if (!Storage_) {
        throw std::runtime_error("GlobalStorage is not initialized");
    }
    return *Storage_;
}

void ProcessGame(std::shared_ptr<GameContext> game, std::shared_ptr<boost::asio::steady_timer> timer) {
    if (!game) {
        std::cout << "null game, stopping game loop" << std::endl;
        std::cout.flush();
        return;
    }
    if (game->GameOver) {
        std::cout << "Game over, stopping game loop" << std::endl;
        std::cout.flush();
        return;
    }

    std::cout << "Processing game tick" << std::endl;
    std::cout.flush();

    game->UpdateGame();
    game->NotifyUsers();

    using namespace std::chrono_literals;
    timer->expires_after(333ms);
    timer->async_wait([game, timer](const boost::system::error_code& ec) {
        if (ec) {
            std::cerr << "Timer error: " << ec.message() << std::endl;
            std::cerr.flush();
            return;
        }
        std::cout << "gfhjkl" << std::endl;
        std::cout.flush();
        ProcessGame(game, timer);
    });
}

void GlobalStorage::AddGame(int userId1, int userId2) {
    auto player1Connection = NetworkManager::GetPlayerConnection(userId1);
    auto player2Connection = NetworkManager::GetPlayerConnection(userId2);

    if (!player1Connection || !player2Connection) {
        return;
    }

    auto newGame = std::make_shared<NetworkGameContext>(player1Connection, player2Connection);
    newGame->UserId1 = userId1;
    newGame->UserId2 = userId2;

    {
        boost::unique_lock lock(Mutex_);
        Games_.emplace(userId1, newGame);
        Games_.emplace(userId2, newGame);
    }

    auto timer = std::make_shared<boost::asio::steady_timer>(Pool_.executor());

    boost::asio::post(
        Pool_, [game = std::move(newGame), timer = std::move(timer)]() {
            ProcessGame(game, std::move(timer));
        });
}

void GlobalStorage::RemoveGame(int userId1, int userId2) {
    boost::unique_lock lock(Mutex_);
    Games_.erase(userId1);
    Games_.erase(userId2);
}

std::shared_ptr<GameContext> GlobalStorage::FindGameByUserId(int userId) {
    boost::shared_lock lock(Mutex_);
    auto it = Games_.find(userId);
    if (it == Games_.end()) {
        return nullptr;
    }
    return it->second;
}
