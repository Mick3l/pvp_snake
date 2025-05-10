#pragma once

#include <boost/thread/mutex.hpp>

#include <vector>
#include <utility>
#include <deque>

struct Snake {
    std::deque<std::pair<int, int>> Body;
    char Direction; // wasd
};

struct GameContext {
    std::vector<std::vector<bool>> Field;
    int UserId1;
    int UserId2;
    Snake Snake1;
    Snake Snake2;
    std::pair<int, int> Berry;
    boost::mutex Mutex_;
    int Winner = 0;
    bool GameOver = false;

    GameContext();

    virtual void ProcessTurn(int userId, char key);
    void UpdateField(Snake& snake);
    bool CheckLose(std::pair<int, int> snake) const;
    bool CheckEndGame(bool lose1, bool lose2);
    virtual void UpdateGame();
    virtual void NotifyUsers() const;
};

std::pair<int, int> Step(const Snake& snake);
