#include "main_window.h"

#include <QKeyEvent>
#include <QLabel>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QApplication>

MainWindow::MainWindow(QWidget* parent)
    : QWidget(parent)
    , m_client(new WebSocketClient(this))
    , m_stackedWidget(new QStackedWidget(this))
    , m_menuPage(new QWidget(this))
    , m_queuePage(new QWidget(this))
    , m_gamePage(new QWidget(this))
    , m_startButton(new QPushButton(this))
    , m_queueStatusLabel(new QLabel(this))
    , m_gameStatusLabel(new QLabel(this))
    , m_gameCanvas(new GameCanvas(this))
    , m_isInGame(false) {
    setWindowTitle(tr("Snake PvP"));
    setMinimumSize(800, 600);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(m_stackedWidget);
    setLayout(mainLayout);

    QVBoxLayout* menuLayout = new QVBoxLayout(m_menuPage);
    QLabel* titleLabel = new QLabel(tr("Snake PvP Game"), m_menuPage);
    titleLabel->setAlignment(Qt::AlignCenter);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(24);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);

    m_startButton->setText(tr("Start Game"));
    m_startButton->setStyleSheet(QString::fromUtf8(
        "QPushButton {"
        "background-color: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 white, stop: 1 green);"
        "border-style: solid;"
        "border-color: black;"
        "border-width: 2px;"
        "border-radius: 10px;"
        "width: 200px;"
        "height: 50px;"
        "font-size: 18px;"
        "}"
        "QPushButton:pressed {"
        "border-style: inset;"
        "background-color: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 green, stop: 1 white);"
        "}"));
    m_startButton->setFixedSize(200, 50);

    menuLayout->addStretch();
    menuLayout->addWidget(titleLabel, 0, Qt::AlignCenter);
    menuLayout->addSpacing(50);
    menuLayout->addWidget(m_startButton, 0, Qt::AlignCenter);
    menuLayout->addStretch();

    QVBoxLayout* queueLayout = new QVBoxLayout(m_queuePage);
    m_queueStatusLabel->setText(tr("Waiting for opponent..."));
    m_queueStatusLabel->setAlignment(Qt::AlignCenter);
    QFont queueFont = m_queueStatusLabel->font();
    queueFont.setPointSize(18);
    m_queueStatusLabel->setFont(queueFont);

    queueLayout->addStretch();
    queueLayout->addWidget(m_queueStatusLabel, 0, Qt::AlignCenter);
    queueLayout->addStretch();

    QVBoxLayout* gameLayout = new QVBoxLayout(m_gamePage);
    m_gameStatusLabel->setText(tr("Game in progress"));
    m_gameStatusLabel->setAlignment(Qt::AlignCenter);

    gameLayout->addWidget(m_gameStatusLabel);
    gameLayout->addWidget(m_gameCanvas);

    m_stackedWidget->addWidget(m_menuPage);
    m_stackedWidget->addWidget(m_queuePage);
    m_stackedWidget->addWidget(m_gamePage);
    m_stackedWidget->setCurrentWidget(m_menuPage);

    connect(m_startButton, &QPushButton::clicked, this, &MainWindow::startGame);
    connect(m_client, &WebSocketClient::connected, [this]() {
        m_stackedWidget->setCurrentWidget(m_queuePage);
    });
    connect(m_client, &WebSocketClient::gameStart, this, &MainWindow::onGameStart);
    connect(m_client, &WebSocketClient::gameUpdate, this, &MainWindow::onGameUpdate);
    connect(m_client, &WebSocketClient::gameEnd, this, &MainWindow::onGameEnd);
    connect(m_client, &WebSocketClient::connectionError, this, &MainWindow::onConnectionError);
}

MainWindow::~MainWindow() {
}

void MainWindow::startGame() {
    m_client->connectToServer("ws://localhost:8080");
}

void MainWindow::onGameStart(int opponentId) {
    m_isInGame = true;
    m_gameStatusLabel->setText(tr("Playing against opponent #%1").arg(opponentId));
    m_stackedWidget->setCurrentWidget(m_gamePage);
    m_gameCanvas->resetGame();
}

void MainWindow::onGameUpdate(const GameState& state) {
    m_gameCanvas->updateState(state);

    if (state.gameOver) {
        QString resultText;
        if (state.winner == 0) {
            resultText = tr("Game Over! It's a draw!");
        } else if (state.winner == m_client->getUserId()) {
            resultText = tr("Game Over! You won!");
        } else {
            resultText = tr("Game Over! You lost!");
        }
        m_gameStatusLabel->setText(resultText);
    }
}

void MainWindow::onGameEnd(const QString& reason) {
    m_isInGame = false;
    QMessageBox::information(this, tr("Game Ended"), reason);
    returnToMenu();
}

void MainWindow::onConnectionError(const QString& error) {
    QMessageBox::critical(this, tr("Connection Error"), error);
    returnToMenu();
}

void MainWindow::returnToMenu() {
    m_isInGame = false;
    m_stackedWidget->setCurrentWidget(m_menuPage);
}

void MainWindow::keyPressEvent(QKeyEvent* event) {
    if (!m_isInGame) {
        QWidget::keyPressEvent(event);
        return;
    }

    QString key;
    switch (event->key()) {
        case Qt::Key_W:
            key = "w";
            break;
        case Qt::Key_A:
            key = "a";
            break;
        case Qt::Key_S:
            key = "s";
            break;
        case Qt::Key_D:
            key = "d";
            break;
        default:
            QWidget::keyPressEvent(event);
            return;
    }

    m_client->sendKeyPress(key);
    event->accept();
}
