#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/websocket.hpp>
#include <thread>
#include <future>
#include <nlohmann/json.hpp>
#include <chrono>

#include "networking.h"
#include "context.h"

using json = nlohmann::json;
namespace ip = boost::asio::ip;
namespace ws = boost::beast::websocket;

class NetworkManagerTest: public ::testing::Test {
protected:
    boost::asio::io_context io_context;
    std::unique_ptr<NetworkManager> network_manager;
    std::thread io_thread;
    const int TEST_PORT = 8081;

    void SetUp() override {
        static boost::asio::thread_pool tp(2);
        static bool init = false;
        if (!init) {
            GlobalStorage::Init(tp);
            init = true;
        }
        network_manager = std::make_unique<NetworkManager>(io_context, TEST_PORT);

        io_thread = std::thread([this]() {
            auto work_guard = boost::asio::make_work_guard(io_context);
            io_context.run();
        });

        network_manager->Start();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    void TearDown() override {
        network_manager->Stop();
        io_context.stop();
        if (io_thread.joinable()) {
            io_thread.join();
        }
    }

    class WebSocketClient {
    private:
        boost::asio::io_context io_context_;
        std::shared_ptr<ws::stream<boost::beast::tcp_stream>> ws_;
        std::thread io_thread_;
        boost::beast::flat_buffer buffer_;
        bool is_connected_ = false;

    public:
        WebSocketClient()
            : ws_(std::make_shared<ws::stream<boost::beast::tcp_stream>>(io_context_)) {
            io_thread_ = std::thread([this]() {
                auto work_guard = boost::asio::make_work_guard(io_context_);
                io_context_.run();
            });
        }

        ~WebSocketClient() {
            try {
                if (is_connected_) {
                    boost::beast::error_code ec;
                    ws_->close(ws::close_code::normal, ec);
                }
            } catch (...) {
            }

            io_context_.stop();
            if (io_thread_.joinable()) {
                io_thread_.join();
            }
        }

        bool Connect(const std::string& host, int port) {
            try {
                ip::tcp::resolver resolver(io_context_);
                const auto results = resolver.resolve(host, std::to_string(port));

                boost::beast::get_lowest_layer(*ws_).connect(results.begin()->endpoint());
                ws_->handshake(host, "/");
                is_connected_ = true;
                return true;
            } catch (const std::exception& e) {
                std::cerr << "Connection error: " << e.what() << std::endl;
                return false;
            }
        }

        std::string ReadMessage() {
            try {
                buffer_.clear();
                ws_->read(buffer_);
                std::string message(static_cast<const char*>(buffer_.data().data()), buffer_.data().size());
                buffer_.consume(buffer_.size());
                return message;
            } catch (const std::exception& e) {
                std::cerr << "Read error: " << e.what() << std::endl;
                return "";
            }
        }

        bool SendMessage(const std::string& message) {
            try {
                ws_->write(boost::asio::buffer(message));
                return true;
            } catch (const std::exception& e) {
                std::cerr << "Write error: " << e.what() << std::endl;
                return false;
            }
        }

        void Close() {
            if (is_connected_) {
                boost::beast::error_code ec;
                ws_->close(ws::close_code::normal, ec);
                is_connected_ = false;
            }
        }

        bool IsConnected() const {
            return is_connected_;
        }
    };
};

TEST_F(NetworkManagerTest, ConnectionEstablishment) {
    WebSocketClient client;
    ASSERT_TRUE(client.Connect("127.0.0.1", TEST_PORT));

    std::string welcome_message = client.ReadMessage();
    ASSERT_FALSE(welcome_message.empty());

    client.Close();
}

TEST_F(NetworkManagerTest, WelcomeMessage) {
    WebSocketClient client;
    ASSERT_TRUE(client.Connect("127.0.0.1", TEST_PORT));

    std::string welcome_message = client.ReadMessage();
    ASSERT_FALSE(welcome_message.empty());

    try {
        auto json_message = json::parse(welcome_message);
        ASSERT_EQ(json_message["type"], "welcome");
        ASSERT_TRUE(json_message.contains("userId"));
    } catch (const json::parse_error& e) {
        FAIL() << "Invalid JSON in welcome message: " << e.what();
    }

    client.Close();
}

TEST_F(NetworkManagerTest, SendKeyPress) {
    WebSocketClient client;
    ASSERT_TRUE(client.Connect("127.0.0.1", TEST_PORT));

    std::string welcome_message = client.ReadMessage();

    std::string queue_message = client.ReadMessage();

    json key_press = {
        {"type", "key_press"},
        {"key", "w"}};
    ASSERT_TRUE(client.SendMessage(key_press.dump()));

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    client.Close();
}

TEST_F(NetworkManagerTest, PlayerPairingAndGameCreation) {
    WebSocketClient client1;
    ASSERT_TRUE(client1.Connect("127.0.0.1", TEST_PORT));

    std::string welcome1 = client1.ReadMessage();
    std::string queue_status1 = client1.ReadMessage();

    int userId1 = -1;
    try {
        auto json_message = json::parse(welcome1);
        userId1 = json_message["userId"];
    } catch (const json::parse_error& e) {
        FAIL() << "Invalid JSON in welcome message: " << e.what();
    }

    WebSocketClient client2;
    ASSERT_TRUE(client2.Connect("127.0.0.1", TEST_PORT));

    std::string welcome2 = client2.ReadMessage();
    std::string queue_status2 = client2.ReadMessage();

    int userId2 = -1;
    try {
        auto json_message = json::parse(welcome2);
        userId2 = json_message["userId"];
    } catch (const json::parse_error& e) {
        FAIL() << "Invalid JSON in welcome message: " << e.what();
    }

    std::string game_start1 = client1.ReadMessage();
    std::string game_start2 = client2.ReadMessage();

    try {
        auto json1 = json::parse(game_start1);
        auto json2 = json::parse(game_start2);

        ASSERT_EQ(json1["type"], "game_start");
        ASSERT_EQ(json2["type"], "game_start");

        ASSERT_EQ(json1["opponent_id"], userId2);
        ASSERT_EQ(json2["opponent_id"], userId1);
    } catch (const json::parse_error& e) {
        FAIL() << "Invalid JSON in game start messages: " << e.what();
    }

    client1.Close();
    client2.Close();
}
