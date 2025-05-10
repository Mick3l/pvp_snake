#include "networking.h"

#include <iostream>
#include <sstream>

NetworkManager* NetworkManager::instance_ = nullptr;

NetworkManager::NetworkManager(boost::asio::io_context& context, int port)
    : ioContext_(context)
    , acceptor_(context, ip::tcp::endpoint(ip::tcp::v4(), port))
    , running_(false) {
    SetInstance(this);
}

NetworkManager::~NetworkManager() {
    Stop();
    if (instance_ == this) {
        instance_ = nullptr;
    }
}

void NetworkManager::SetInstance(NetworkManager* manager) {
    if (instance_) {
        throw std::runtime_error("NetworkManager instance already set");
    }
    instance_ = manager;
}

NetworkManager& NetworkManager::GetInstance() {
    if (!instance_) {
        throw std::runtime_error("NetworkManager instance not set");
    }
    return *instance_;
}

std::shared_ptr<PlayerConnection> NetworkManager::GetPlayerConnection(int userId) {
    boost::mutex::scoped_lock lock(instance_->connectionsMutex_);
    auto it = instance_->connections_.find(userId);
    if (it != instance_->connections_.end()) {
        return it->second;
    }
    return nullptr;
}

void NetworkManager::Start() {
    running_ = true;
    AcceptConnection();
}

void NetworkManager::Stop() {
    if (!running_) {
        return;
    }

    running_ = false;
    boost::system::error_code ec;
    acceptor_.close(ec);

    boost::mutex::scoped_lock lock(connectionsMutex_);
    for (auto& conn : connections_) {
        try {
            conn.second->socket->close(ws::close_code::normal);
        } catch (const std::exception& e) {
            std::cerr << "Error closing connection: " << e.what() << std::endl;
        }
    }
    connections_.clear();
}

void NetworkManager::AcceptConnection() {
    if (!running_) {
        return;
    }

    auto websocket = std::make_shared<ws::stream<boost::beast::tcp_stream>>(ioContext_);
    websocket->set_option(ws::stream_base::timeout::suggested(boost::beast::role_type::server));

    acceptor_.async_accept(
        boost::beast::get_lowest_layer(*websocket).socket(),
        [this, websocket](const boost::system::error_code& ec) {
            this->OnAccept(websocket, ec);
        });
}

void NetworkManager::OnAccept(std::shared_ptr<ws::stream<boost::beast::tcp_stream>> websocket,
                              const boost::system::error_code& ec) {
    if (ec) {
        std::cerr << "Accept error: " << ec.message() << std::endl;
        return;
    }

    auto player = std::make_shared<PlayerConnection>(websocket);

    websocket->async_accept(
        [this, player](const boost::system::error_code& ec) {
            this->OnHandshake(player, ec);
        });

    AcceptConnection();
}

void NetworkManager::OnHandshake(std::shared_ptr<PlayerConnection> player,
                                 const boost::system::error_code& ec) {
    if (ec) {
        std::cerr << "Handshake error: " << ec.message() << std::endl;
        return;
    }

    {
        boost::mutex::scoped_lock lock(connectionsMutex_);
        connections_.emplace(player->userId, player);
    }

    json welcomeMsg = {
        {"type", "welcome"},
        {"userId", player->userId}};
    SendMessage(player, welcomeMsg.dump());

    AddPlayerToQueue(player);

    player->socket->async_read(
        *player->buffer,
        [this, player](const boost::system::error_code& ec, std::size_t bytes_transferred) {
            this->OnRead(player, ec, bytes_transferred);
        });
}

void NetworkManager::OnRead(std::shared_ptr<PlayerConnection> player,
                            const boost::system::error_code& ec,
                            std::size_t bytes_transferred) {
    if (ec) {
        if (ec == ws::error::closed) {
            std::cout << "Connection closed for user " << player->userId << std::endl;
            RemovePlayerFromGame(player->userId);
        } else {
            std::cerr << "Read error: " << ec.message() << std::endl;
        }

        {
            boost::mutex::scoped_lock lock(connectionsMutex_);
            connections_.erase(player->userId);
        }
        return;
    }

    std::string message(static_cast<const char*>(player->buffer->data().data()), bytes_transferred);
    player->buffer->consume(bytes_transferred);

    ProcessMessage(player, message);

    player->socket->async_read(
        *player->buffer,
        [this, player](const boost::system::error_code& ec, std::size_t bytes_transferred) {
            this->OnRead(player, ec, bytes_transferred);
        });
}

void NetworkManager::ProcessMessage(std::shared_ptr<PlayerConnection> player, const std::string& message) {
    try {
        auto data = json::parse(message);
        std::string type = data["type"].get<std::string>();

        if (type == "key_press") {
            char key = data["key"].get<std::string>()[0];

            auto gameContext = GlobalStorage::GetInstance().FindGameByUserId(player->userId);
            if (gameContext) {
                gameContext->ProcessTurn(player->userId, key);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error processing message: " << e.what() << std::endl;
        std::cerr << "Message: " << message << std::endl;
    }
}

void NetworkManager::SendMessage(std::shared_ptr<PlayerConnection> player, const std::string& message) {
    try {
        player->socket->text(true);
        player->socket->write(boost::asio::buffer(message));
    } catch (const std::exception& e) {
        std::cerr << "Error sending message: " << e.what() << std::endl;
    }
}

void NetworkManager::PairPlayersForGame() {
    boost::mutex::scoped_lock lock(queueMutex_);

    if (waitingPlayers_.size() < 2) {
        return;
    }
    auto player1 = waitingPlayers_.front();
    waitingPlayers_.pop();
    auto player2 = waitingPlayers_.front();
    waitingPlayers_.pop();

    player1->inGame = true;
    player2->inGame = true;

    GlobalStorage::GetInstance().AddGame(player1->userId, player2->userId);

    json startGameMsg = {
        {"type", "game_start"},
        {"opponent_id", player2->userId}};
    SendMessage(player1, startGameMsg.dump());

    startGameMsg["opponent_id"] = player1->userId;
    SendMessage(player2, startGameMsg.dump());
}

void NetworkManager::AddPlayerToQueue(std::shared_ptr<PlayerConnection> player) {
    {
        boost::mutex::scoped_lock lock(queueMutex_);
        waitingPlayers_.push(player);

        json queueMsg = {
            {"type", "queue_status"},
            {"status", "waiting_for_opponent"}};
        SendMessage(player, queueMsg.dump());
    }

    PairPlayersForGame();
}

void NetworkManager::RemovePlayerFromGame(int userId) {
    auto gameContext = GlobalStorage::GetInstance().FindGameByUserId(userId);
    if (!gameContext) {
        return;
    }
    int otherUserId = (gameContext->UserId1 == userId) ? gameContext->UserId2 : gameContext->UserId1;
    GlobalStorage::GetInstance().RemoveGame(userId, otherUserId);

    boost::mutex::scoped_lock lock(connectionsMutex_);
    auto it = connections_.find(otherUserId);
    if (it != connections_.end()) {
        json gameEndMsg = {
            {"type", "game_end"},
            {"reason", "opponent_disconnected"}};
        SendMessage(it->second, gameEndMsg.dump());

        it->second->inGame = false;
        AddPlayerToQueue(it->second);
    }
}

NetworkGameContext::NetworkGameContext(
    std::shared_ptr<PlayerConnection> player1,
    std::shared_ptr<PlayerConnection> player2)
    : player1_(player1)
    , player2_(player2) {
    UserId1 = player1->userId;
    UserId2 = player2->userId;
}

void NetworkGameContext::NotifyUsers() const {
    json gameState = {
        {"type", "game_update"},
        {"snake1", {{"body", json::array()}, {"direction", std::string(1, Snake1.Direction)}}},
        {"snake2", {{"body", json::array()}, {"direction", std::string(1, Snake2.Direction)}}},
        {"berry", {Berry.first, Berry.second}},
        {"gameOver", GameOver},
        {"winner", Winner}};

    auto& snake1 = gameState["snake1"]["body"];
    for (const auto& segment : Snake1.Body) {
        snake1.push_back({segment.first, segment.second});
    }

    auto& snake2 = gameState["snake2"]["body"];
    for (const auto& segment : Snake2.Body) {
        snake2.push_back({segment.first, segment.second});
    }

    std::string message = gameState.dump();

    std::cout << message << std::endl;
    std::cout.flush();

    auto player1 = player1_.lock();
    auto player2 = player2_.lock();

    if (player1) {
        player1->socket->text(true);
        try {
            player1->socket->write(boost::asio::buffer(message));
        } catch (const std::exception& e) {
            std::cerr << "Error notifying player 1: " << e.what() << std::endl;
        }
    }

    if (player2) {
        player2->socket->text(true);
        try {
            player2->socket->write(boost::asio::buffer(message));
        } catch (const std::exception& e) {
            std::cerr << "Error notifying player 2: " << e.what() << std::endl;
        }
    }
}
