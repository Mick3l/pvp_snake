#include "game_canvas.h"

#include <QPainter>
#include <QPaintEvent>

GameCanvas::GameCanvas(QWidget* parent)
    : QWidget(parent)
    , m_cellSize(0)
    , m_snake1Color(Qt::green)
    , m_snake2Color(Qt::blue)
    , m_berryColor(Qt::red)
    , m_gridColor(Qt::lightGray) {
    setMinimumSize(500, 500);
    setFixedSize(640, 640);
    resetGame();
}

void GameCanvas::resetGame() {
    m_state = GameState{};

    m_state.berry = qMakePair(32, 32);

    m_state.snake1.body.clear();
    m_state.snake1.body.append(qMakePair(0, 0));
    m_state.snake1.direction = "w";

    m_state.snake2.body.clear();
    m_state.snake2.body.append(qMakePair(63, 63));
    m_state.snake2.direction = "s";

    m_cellSize = width() / 64;
    update();
}

void GameCanvas::updateState(const GameState& state) {
    m_state = state;
    update();
}

void GameCanvas::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    painter.fillRect(rect(), Qt::white);

    painter.setPen(QPen(m_gridColor, 0.5));
    for (int i = 0; i <= 64; ++i) {
        painter.drawLine(i * m_cellSize, 0, i * m_cellSize, height());
        painter.drawLine(0, i * m_cellSize, width(), i * m_cellSize);
    }

    if (m_state.berry.first >= 0 && m_state.berry.second >= 0) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(m_berryColor);
        painter.drawEllipse(m_state.berry.first * m_cellSize + 1,
                            (63 - m_state.berry.second) * m_cellSize + 1,
                            m_cellSize - 2, m_cellSize - 2);
    }

    painter.setBrush(m_snake1Color);
    for (const auto& segment : m_state.snake1.body) {
        painter.drawRect(segment.first * m_cellSize + 1,
                         (63 - segment.second) * m_cellSize + 1,
                         m_cellSize - 2, m_cellSize - 2);
    }

    painter.setBrush(m_snake2Color);
    for (const auto& segment : m_state.snake2.body) {
        painter.drawRect(segment.first * m_cellSize + 1,
                         (63 - segment.second) * m_cellSize + 1,
                         m_cellSize - 2, m_cellSize - 2);
    }
}
