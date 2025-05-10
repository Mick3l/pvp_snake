#include "websocket_client.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

#include <iostream>

WebSocketWorker::WebSocketWorker(QObject* parent)
    : QObject(parent)
    , m_userId(0)
    , m_isConnected(false)
    , m_webSocket(nullptr) {
    qDebug() << "WebSocketWorker created in thread:" << QThread::currentThreadId();
}

WebSocketWorker::~WebSocketWorker() {
    if (m_webSocket) {
        m_webSocket->close();
        delete m_webSocket;
        m_webSocket = nullptr;
    }
    qDebug() << "WebSocketWorker destroyed in thread:" << QThread::currentThreadId();
}

void WebSocketWorker::connectToServer(const QString& url) {
    QMetaObject::invokeMethod(this, "initAndConnect", Qt::QueuedConnection, Q_ARG(QString, url));
}

void WebSocketWorker::initAndConnect(const QString& url) {
    QMutexLocker locker(&m_mutex);
    qDebug() << "WebSocketWorker initAndConnect in thread:" << QThread::currentThreadId();

    if (m_webSocket) {
        m_webSocket->disconnect();
        m_webSocket->close();
        m_webSocket->deleteLater();
        m_webSocket = nullptr;
    }

    m_webSocket = new QWebSocket("", QWebSocketProtocol::VersionLatest, this);

    connect(m_webSocket, &QWebSocket::connected, this, &WebSocketWorker::onConnected);
    connect(m_webSocket, &QWebSocket::disconnected, this, &WebSocketWorker::onDisconnected);
    connect(m_webSocket, &QWebSocket::textMessageReceived, this, &WebSocketWorker::onTextMessageReceived);
    connect(m_webSocket, &QWebSocket::errorOccurred, this, &WebSocketWorker::onErrorOccurred);

    qDebug() << "Opening WebSocket connection to:" << url;
    m_webSocket->open(QUrl(url));
}

void WebSocketWorker::sendKeyPress(const QString& key) {
    QMutexLocker locker(&m_mutex);

    if (!m_isConnected || !m_webSocket) {
        qDebug() << "Cannot send key press: not connected or not initialized";
        return;
    }

    QJsonObject message;
    message["type"] = "key_press";
    message["key"] = key;

    QJsonDocument doc(message);
    m_webSocket->sendTextMessage(doc.toJson(QJsonDocument::Compact));
}

void WebSocketWorker::cleanup() {
    QMutexLocker locker(&m_mutex);
    qDebug() << "Cleaning up WebSocket in thread:" << QThread::currentThreadId();

    if (m_webSocket) {
        m_webSocket->close();
    }
}

void WebSocketWorker::onConnected() {
    QMutexLocker locker(&m_mutex);
    qDebug() << "WebSocket connected in thread:" << QThread::currentThreadId();
    m_isConnected = true;
    emit connected();
}

void WebSocketWorker::onDisconnected() {
    QMutexLocker locker(&m_mutex);
    qDebug() << "WebSocket disconnected in thread:" << QThread::currentThreadId();
    m_isConnected = false;
    emit gameEnd(tr("Disconnected from server"));
}

void WebSocketWorker::onTextMessageReceived(const QString& message) {
    QMutexLocker locker(&m_mutex);

    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (!doc.isObject()) {
        return;
    }

    QJsonObject json = doc.object();
    QString messageType = json["type"].toString();

    if (messageType == "welcome") {
        m_userId = json["userId"].toInt();
        emit userIdChanged(m_userId);
    } else if (messageType == "queue_status") {
    } else if (messageType == "game_start") {
        int opponentId = json["opponent_id"].toInt();
        emit gameStart(opponentId);
    } else if (messageType == "game_update") {
        std::cout << message.toStdString() << std::endl;
        std::cout.flush();
        GameState state;

        QJsonObject snake1Json = json["snake1"].toObject();
        QJsonArray snake1Body = snake1Json["body"].toArray();
        state.snake1.direction = snake1Json["direction"].toString();

        state.snake1.body.clear();
        for (const QJsonValue& value : snake1Body) {
            if (value.isArray()) {
                QJsonArray pos = value.toArray();
                if (pos.size() >= 2) {
                    state.snake1.body.append(qMakePair(pos[0].toInt(), pos[1].toInt()));
                }
            }
        }

        QJsonObject snake2Json = json["snake2"].toObject();
        QJsonArray snake2Body = snake2Json["body"].toArray();
        state.snake2.direction = snake2Json["direction"].toString();

        state.snake2.body.clear();
        for (const QJsonValue& value : snake2Body) {
            if (value.isArray()) {
                QJsonArray pos = value.toArray();
                if (pos.size() >= 2) {
                    state.snake2.body.append(qMakePair(pos[0].toInt(), pos[1].toInt()));
                }
            }
        }

        if (json["berry"].isArray()) {
            QJsonArray berryPos = json["berry"].toArray();
            if (berryPos.size() >= 2) {
                state.berry = qMakePair(berryPos[0].toInt(), berryPos[1].toInt());
            }
        }

        state.gameOver = json["gameOver"].toBool();
        state.winner = json["winner"].toInt();

        emit gameUpdate(state);
    } else if (messageType == "game_end") {
        QString reason = json["reason"].toString();
        emit gameEnd(reason);
    }
}

void WebSocketWorker::onErrorOccurred(QAbstractSocket::SocketError error) {
    QMutexLocker locker(&m_mutex);
    qDebug() << "WebSocket error in thread:" << QThread::currentThreadId() << ":"
             << (m_webSocket ? m_webSocket->errorString() : "WebSocket not initialized");
    Q_UNUSED(error);

    if (m_webSocket) {
        emit connectionError(m_webSocket->errorString());
    } else {
        emit connectionError("WebSocket error: not initialized");
    }
}

WebSocketClient::WebSocketClient(QObject* parent)
    : QObject(parent)
    , m_userId(0) {
    qDebug() << "WebSocketClient created in main thread:" << QThread::currentThreadId();

    m_workerThread = new QThread(this);
    m_worker = new WebSocketWorker();

    m_worker->moveToThread(m_workerThread);

    connect(this, &WebSocketClient::doConnect, m_worker, &WebSocketWorker::connectToServer, Qt::QueuedConnection);
    connect(this, &WebSocketClient::doSendKeyPress, m_worker, &WebSocketWorker::sendKeyPress, Qt::QueuedConnection);
    connect(this, &WebSocketClient::doCleanup, m_worker, &WebSocketWorker::cleanup, Qt::QueuedConnection);

    connect(m_worker, &WebSocketWorker::connected, this, &WebSocketClient::connected, Qt::QueuedConnection);
    connect(m_worker, &WebSocketWorker::gameStart, this, &WebSocketClient::gameStart, Qt::QueuedConnection);
    connect(m_worker, &WebSocketWorker::gameUpdate, this, &WebSocketClient::gameUpdate, Qt::QueuedConnection);
    connect(m_worker, &WebSocketWorker::gameEnd, this, &WebSocketClient::gameEnd, Qt::QueuedConnection);
    connect(m_worker, &WebSocketWorker::connectionError, this, &WebSocketClient::connectionError, Qt::QueuedConnection);
    connect(m_worker, &WebSocketWorker::userIdChanged, this, &WebSocketClient::updateUserId, Qt::QueuedConnection);

    connect(m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);

    m_workerThread->start();
    qDebug() << "Worker thread started with ID:" << m_workerThread->currentThreadId();
}

WebSocketClient::~WebSocketClient() {
    qDebug() << "WebSocketClient destroying, cleaning up threads...";

    emit doCleanup();
    m_workerThread->quit();

    if (!m_workerThread->wait(1000)) {
        qDebug() << "Thread didn't quit gracefully, forcing termination";
        m_workerThread->terminate();
        m_workerThread->wait();
    }

    qDebug() << "WebSocketClient destroyed";
}

void WebSocketClient::connectToServer(const QString& url) {
    qDebug() << "WebSocketClient requesting connection in main thread:" << QThread::currentThreadId();
    emit doConnect(url);
}

void WebSocketClient::sendKeyPress(const QString& key) {
    emit doSendKeyPress(key);
}

int WebSocketClient::getUserId() const {
    QReadLocker locker(&m_userIdLock);
    return m_userId;
}

void WebSocketClient::updateUserId(int userId) {
    QWriteLocker locker(&m_userIdLock);
    m_userId = userId;
    qDebug() << "User ID updated to:" << m_userId;
}
