#pragma once

#include <QWidget>

struct Snake {
    QVector<QPair<int, int>> body;
    QString direction;
};

struct GameState {
    Snake snake1;
    Snake snake2;
    QPair<int, int> berry;
    bool gameOver;
    int winner;
};

class GameCanvas: public QWidget {
    Q_OBJECT

public:
    explicit GameCanvas(QWidget* parent = nullptr);

    void resetGame();
    void updateState(const GameState& state);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    GameState m_state;
    int m_cellSize;

    QColor m_snake1Color;
    QColor m_snake2Color;
    QColor m_berryColor;
    QColor m_gridColor;
};
