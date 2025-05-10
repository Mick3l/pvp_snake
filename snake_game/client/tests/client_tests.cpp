#include <gtest/gtest.h>
#include <QTest>
#include <QSignalSpy>
#include <QApplication>
#include <QtTest/QTest>
#include <QTimer>
#include <QDebug>

#include "main_window.h"
#include "game_canvas.h"
#include "websocket_client.h"

class MainWindowTestHelper {
public:
    static QPushButton* findStartButton(MainWindow* window) {
        QList<QPushButton*> buttons = window->findChildren<QPushButton*>();
        for (QPushButton* button : buttons) {
            if (button->text().contains("Start", Qt::CaseInsensitive)) {
                return button;
            }
        }
        qDebug() << "Start button not found. Available buttons:";
        for (QPushButton* button : buttons) {
            qDebug() << "Button text:" << button->text() << "visible:" << button->isVisible();
        }
        return nullptr;
    }

    static QStackedWidget* findStackedWidget(MainWindow* window) {
        QStackedWidget* stack = window->findChild<QStackedWidget*>();
        if (!stack) {
            qDebug() << "Stacked widget not found";
        }
        return stack;
    }

    static GameCanvas* findGameCanvas(MainWindow* window) {
        return window->findChild<GameCanvas*>();
    }

    static void printWidgetHierarchy(MainWindow* window) {
        qDebug() << "Widget hierarchy:";
        QList<QObject*> children = window->findChildren<QObject*>();
        for (QObject* child : children) {
            qDebug() << "Object:" << child->metaObject()->className() << "Name:" << child->objectName();
            if (QWidget* widget = qobject_cast<QWidget*>(child)) {
                qDebug() << "  Visible:" << widget->isVisible() << "Size:" << widget->size();
            }
        }
    }
};

class SnakeClientTest: public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        static int argc = 1;
        static char* argv[] = {const_cast<char*>("snake_client_tests")};
        if (!QApplication::instance()) {
            appInstance = new QApplication(argc, argv);
        }
    }

    static void TearDownTestSuite() {
        delete appInstance;
        appInstance = nullptr;
    }

    static QApplication* appInstance;
};

QApplication* SnakeClientTest::appInstance = nullptr;

TEST_F(SnakeClientTest, MainWindowInitialState) {
    MainWindow window;
    window.show();
    QTest::qWait(100);

    MainWindowTestHelper::printWidgetHierarchy(&window);

    QPushButton* startButton = MainWindowTestHelper::findStartButton(&window);
    ASSERT_TRUE(startButton != nullptr) << "Start button not found";
    qDebug() << "Start button visibility:" << startButton->isVisible();
    qDebug() << "Start button text:" << startButton->text();
    qDebug() << "Start button parent visibility:" << startButton->parentWidget()->isVisible();

    EXPECT_TRUE(startButton->isVisible() || startButton->parentWidget()->isVisible());

    QStackedWidget* stack = MainWindowTestHelper::findStackedWidget(&window);
    ASSERT_TRUE(stack != nullptr) << "Stacked widget not found";

    EXPECT_GT(stack->count(), 0);
    EXPECT_EQ(stack->currentIndex(), 0);
}

TEST_F(SnakeClientTest, KeyPressEventHandling) {
    MainWindow window;
    window.show();
    QTest::qWait(100);

    QTest::keyClick(&window, Qt::Key_W);
    QTest::keyClick(&window, Qt::Key_A);
    QTest::keyClick(&window, Qt::Key_S);
    QTest::keyClick(&window, Qt::Key_D);

    SUCCEED();
}

TEST_F(SnakeClientTest, GameCanvasInitialState) {
    GameCanvas canvas;

    EXPECT_GE(canvas.minimumWidth(), 500);
    EXPECT_GE(canvas.minimumHeight(), 500);
}

TEST_F(SnakeClientTest, GameCanvasUpdateState) {
    GameCanvas canvas;

    GameState testState;
    testState.snake1.body.append(qMakePair(10, 10));
    testState.snake2.body.append(qMakePair(20, 20));
    testState.berry = qMakePair(15, 15);
    testState.gameOver = false;
    testState.winner = 0;

    canvas.updateState(testState);

    SUCCEED();
}

TEST_F(SnakeClientTest, WebSocketClientSignals) {
    WebSocketClient client;

    QSignalSpy connectedSpy(&client, &WebSocketClient::connected);
    QSignalSpy gameStartSpy(&client, &WebSocketClient::gameStart);
    QSignalSpy gameUpdateSpy(&client, &WebSocketClient::gameUpdate);
    QSignalSpy gameEndSpy(&client, &WebSocketClient::gameEnd);
    QSignalSpy connectionErrorSpy(&client, &WebSocketClient::connectionError);

    EXPECT_TRUE(connectedSpy.isValid());
    EXPECT_TRUE(gameStartSpy.isValid());
    EXPECT_TRUE(gameUpdateSpy.isValid());
    EXPECT_TRUE(gameEndSpy.isValid());
    EXPECT_TRUE(connectionErrorSpy.isValid());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
