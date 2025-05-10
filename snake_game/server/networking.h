#pragma once

#include "context.h"
#include "main_processor.h"

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/thread/mutex.hpp>

#include <deque>
#include <map>
#include <memory>
#include <string>
#include <queue>

#include <nlohmann/json.hpp>

namespace ip = boost::asio::ip;
namespace ws = boost::beast::websocket;
using json = nlohmann::json;

struct PlayerConnection {
    int userId;
    std::shared_ptr<ws::stream<boost::beast::tcp_stream>> socket;
    std::shared_ptr<boost::beast::flat_buffer> buffer;
    bool inGame;

    PlayerConnection(std::shared_ptr<ws::stream<boost::beast::tcp_stream>> s)
        : userId(GenerateUserId())
        , socket(s)
        , buffer(std::make_shared<boost::beast::flat_buffer>())
        , inGame(false) {
    }

    static int GenerateUserId() {
        static int nextId = 1;
        return nextId++;
    }
};

class NetworkManager {
public:
    NetworkManager(boost::asio::io_context& context, int port = 8080);
    ~NetworkManager();

    void Start();
    void Stop();

private:
    void AcceptConnection();
    void OnAccept(std::shared_ptr<ws::stream<boost::beast::tcp_stream>> websocket,
                  const boost::system::error_code& ec);
    void OnHandshake(std::shared_ptr<PlayerConnection> player,
                     const boost::system::error_code& ec);
    void OnRead(std::shared_ptr<PlayerConnection> player,
                const boost::system::error_code& ec,
                std::size_t bytes_transferred);
    void ProcessMessage(std::shared_ptr<PlayerConnection> player, const std::string& message);
    void SendMessage(std::shared_ptr<PlayerConnection> player, const std::string& message);
    void PairPlayersForGame();
    void AddPlayerToQueue(std::shared_ptr<PlayerConnection> player);
    void RemovePlayerFromGame(int userId);

    boost::asio::io_context& ioContext_;
    ip::tcp::acceptor acceptor_;
    std::map<int, std::shared_ptr<PlayerConnection>> connections_;
    std::queue<std::shared_ptr<PlayerConnection>> waitingPlayers_;
    boost::mutex connectionsMutex_;
    boost::mutex queueMutex_;
    bool running_;

public:
    static std::shared_ptr<PlayerConnection> GetPlayerConnection(int userId);
    static void SetInstance(NetworkManager* manager);
    static NetworkManager& GetInstance();

private:
    static NetworkManager* instance_;
};

class NetworkGameContext: public GameContext {
public:
    NetworkGameContext(std::shared_ptr<PlayerConnection> player1,
                       std::shared_ptr<PlayerConnection> player2);
    void NotifyUsers() const override;

private:
    std::weak_ptr<PlayerConnection> player1_;
    std::weak_ptr<PlayerConnection> player2_;
};
