#include "main_processor.h"

#include <iostream>
#include <random>

GameContext::GameContext() {
    Snake1.Body.push_back({0, 0});
    Snake1.Direction = 'w';
    Snake2.Body.push_back({63, 63});
    Snake2.Direction = 's';

    Field = std::vector(64, std::vector<bool>(64, false));

    Field[0][0] = true;
    Field[63][63] = true;

    Berry = {32, 32};
}

void GameContext::ProcessTurn(int userId, char key) {
    boost::mutex::scoped_lock lock(Mutex_);
    if (userId == UserId1) {
        if ((key == 'w' && Snake1.Direction == 's') ||
            (key == 's' && Snake1.Direction == 'w') ||
            (key == 'a' && Snake1.Direction == 'd') ||
            (key == 'd' && Snake1.Direction == 'a')) {
            return;
        }
        Snake1.Direction = key;
    } else if (userId == UserId2) {
        if ((key == 'w' && Snake2.Direction == 's') ||
            (key == 's' && Snake2.Direction == 'w') ||
            (key == 'a' && Snake2.Direction == 'd') ||
            (key == 'd' && Snake2.Direction == 'a')) {
            return;
        }
        Snake2.Direction = key;
    }
}

std::pair<int, int> Step(const Snake& snake) {
    auto cur = snake.Body.front();
    switch (snake.Direction) {
        case 'w':
            return {cur.first, cur.second + 1};
        case 's':
            return {cur.first, cur.second - 1};
        case 'a':
            return {cur.first - 1, cur.second};
        case 'd':
            return {cur.first + 1, cur.second};
        default:
            return cur;
    }
}

void GameContext::UpdateField(Snake& snake) {
    auto snakePos = snake.Body.front();

    if (snakePos == Berry) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dist(0, 63);

        int attempts = 0;
        while (attempts < 100) {
            int x = dist(gen);
            int y = dist(gen);
            if (!Field[x][y]) {
                Berry = {x, y};
                break;
            }
            attempts++;
        }

        if (attempts >= 100) {
            for (int x = 0; x < 64; x++) {
                for (int y = 0; y < 64; y++) {
                    if (!Field[x][y]) {
                        Berry = {x, y};
                        goto berry_placed;
                    }
                }
            }
        berry_placed:;
        }
    } else {
        auto back = snake.Body.back();
        snake.Body.pop_back();
        Field[back.first][back.second] = false;
    }

    Field[snakePos.first][snakePos.second] = true;
}

bool GameContext::CheckLose(std::pair<int, int> snakePos) const {
    if (snakePos.first < 0 || snakePos.second < 0 ||
        snakePos.first >= 64 || snakePos.second >= 64) {
        return true;
    }

    return Field[snakePos.first][snakePos.second];
}

bool GameContext::CheckEndGame(bool lose1, bool lose2) {
    if (lose1 && lose2) {
        GameOver = true;
        Winner = 0;
        return true;
    } else if (lose1) {
        GameOver = true;
        Winner = UserId2;
        return true;
    } else if (lose2) {
        GameOver = true;
        Winner = UserId1;
        return true;
    }
    return false;
}

void GameContext::UpdateGame() {
    boost::mutex::scoped_lock lock(Mutex_);

    if (GameOver) {
        return;
    }

    auto oldHead1 = Snake1.Body.front();
    auto oldHead2 = Snake2.Body.front();

    auto newHead1 = Step(Snake1);
    auto newHead2 = Step(Snake2);

    if (newHead1 == newHead2) {
        GameOver = true;
        Winner = 0;
        return;
    }

    bool lose1 = (newHead1.first < 0 || newHead1.second < 0 ||
                  newHead1.first >= 64 || newHead1.second >= 64);
    bool lose2 = (newHead2.first < 0 || newHead2.second < 0 ||
                  newHead2.first >= 64 || newHead2.second >= 64);

    if (!lose1 && Field[newHead1.first][newHead1.second] &&
        !(newHead1 == oldHead2 && Snake2.Body.size() == 1)) {
        lose1 = true;
    }

    if (!lose2 && Field[newHead2.first][newHead2.second] &&
        !(newHead2 == oldHead1 && Snake1.Body.size() == 1)) {
        lose2 = true;
    }

    if (CheckEndGame(lose1, lose2)) {
        return;
    }

    Snake1.Body.push_front(newHead1);
    Snake2.Body.push_front(newHead2);

    UpdateField(Snake1);
    UpdateField(Snake2);
}

void GameContext::NotifyUsers() const {
}
