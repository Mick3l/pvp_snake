#pragma once

#include <QWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QLabel>

#include "game_canvas.h"
#include "websocket_client.h"

class MainWindow: public QWidget {
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void startGame();
    void onGameStart(int opponentId);
    void onGameUpdate(const GameState& state);
    void onGameEnd(const QString& reason);
    void onConnectionError(const QString& error);
    void returnToMenu();

protected:
    void keyPressEvent(QKeyEvent* event) override;

private:
    WebSocketClient* m_client;

    QStackedWidget* m_stackedWidget;
    QWidget* m_menuPage;
    QWidget* m_queuePage;
    QWidget* m_gamePage;

    QPushButton* m_startButton;
    QLabel* m_queueStatusLabel;
    QLabel* m_gameStatusLabel;
    GameCanvas* m_gameCanvas;

    bool m_isInGame;
};
