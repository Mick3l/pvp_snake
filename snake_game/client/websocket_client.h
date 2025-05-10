#pragma once

#include <QObject>
#include <QWebSocket>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QThread>
#include <QMutex>
#include <QReadWriteLock>

#include "game_canvas.h"

class WebSocketWorker: public QObject {
    Q_OBJECT

public:
    explicit WebSocketWorker(QObject* parent = nullptr);
    ~WebSocketWorker();

public slots:
    void initAndConnect(const QString& url);
    void connectToServer(const QString& url);
    void sendKeyPress(const QString& key);
    void cleanup();

signals:
    void connected();
    void gameStart(int opponentId);
    void gameUpdate(const GameState& state);
    void gameEnd(const QString& reason);
    void connectionError(const QString& error);
    void userIdChanged(int userId);

private slots:
    void onConnected();
    void onDisconnected();
    void onTextMessageReceived(const QString& message);
    void onErrorOccurred(QAbstractSocket::SocketError error);

private:
    QWebSocket* m_webSocket;
    int m_userId;
    bool m_isConnected;
    QMutex m_mutex;
};

class WebSocketClient: public QObject {
    Q_OBJECT

public:
    explicit WebSocketClient(QObject* parent = nullptr);
    ~WebSocketClient();

    void connectToServer(const QString& url);
    void sendKeyPress(const QString& key);
    int getUserId() const;

signals:

    void connected();
    void gameStart(int opponentId);
    void gameUpdate(const GameState& state);
    void gameEnd(const QString& reason);
    void connectionError(const QString& error);

    void doConnect(const QString& url);
    void doSendKeyPress(const QString& key);
    void doCleanup();

private slots:
    void updateUserId(int userId);

private:
    QThread* m_workerThread;
    WebSocketWorker* m_worker;
    int m_userId;
    mutable QReadWriteLock m_userIdLock;
};
