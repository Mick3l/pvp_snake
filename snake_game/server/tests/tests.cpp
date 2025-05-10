#include "main_processor.h"

#include "gtest/gtest.h"

#include <vector>
#include <queue>
#include <thread>

class TestableGameContext: public GameContext {
public:
    using GameContext::GameContext;

    using GameContext::CheckEndGame;
    using GameContext::CheckLose;
    using GameContext::UpdateField;

    mutable std::vector<std::string> notifications;
    void NotifyUsers() const override {
        notifications.push_back("Notification sent");
    }
};

TEST(StepTest, MovesCorrectlyInAllDirections) {
    Snake snake;
    snake.Body.push_front({5, 5});

    snake.Direction = 'w';
    auto newPos = Step(snake);
    EXPECT_EQ(newPos.first, 5);
    EXPECT_EQ(newPos.second, 6);

    snake.Direction = 's';
    newPos = Step(snake);
    EXPECT_EQ(newPos.first, 5);
    EXPECT_EQ(newPos.second, 4);

    snake.Direction = 'a';
    newPos = Step(snake);
    EXPECT_EQ(newPos.first, 4);
    EXPECT_EQ(newPos.second, 5);

    snake.Direction = 'd';
    newPos = Step(snake);
    EXPECT_EQ(newPos.first, 6);
    EXPECT_EQ(newPos.second, 5);
}

TEST(StepTest, DefaultBehaviorOnInvalidDirection) {
    Snake snake;
    snake.Body.push_front({5, 5});

    snake.Direction = 'x';
    auto newPos = Step(snake);
    EXPECT_EQ(newPos.first, 5);
    EXPECT_EQ(newPos.second, 5);
}

TEST(GameContextTest, ConstructorInitializesFieldCorrectly) {
    GameContext context;

    EXPECT_EQ(context.Field.size(), 64);
    for (const auto& row : context.Field) {
        EXPECT_EQ(row.size(), 64);
    }

    EXPECT_EQ(context.Snake1.Body.front().first, 0);
    EXPECT_EQ(context.Snake1.Body.front().second, 0);
    EXPECT_EQ(context.Snake1.Direction, 'w');

    EXPECT_EQ(context.Snake2.Body.front().first, 63);
    EXPECT_EQ(context.Snake2.Body.front().second, 63);
    EXPECT_EQ(context.Snake2.Direction, 's');

    EXPECT_TRUE(context.Field[0][0]);
    EXPECT_TRUE(context.Field[63][63]);

    EXPECT_EQ(context.Berry.first, 32);
    EXPECT_EQ(context.Berry.second, 32);

    EXPECT_FALSE(context.GameOver);
    EXPECT_EQ(context.Winner, 0);
}

TEST(GameContextTest, ProcessTurnChangesDirection) {
    GameContext context;
    context.UserId1 = 100;
    context.UserId2 = 200;

    context.ProcessTurn(100, 'd');
    EXPECT_EQ(context.Snake1.Direction, 'd');

    context.ProcessTurn(200, 'a');
    EXPECT_EQ(context.Snake2.Direction, 'a');
}

TEST(GameContextTest, ProcessTurnIgnoresInvalidUser) {
    GameContext context;
    context.UserId1 = 100;
    context.UserId2 = 200;

    char origDir1 = context.Snake1.Direction;
    char origDir2 = context.Snake2.Direction;

    context.ProcessTurn(300, 'd');
    EXPECT_EQ(context.Snake1.Direction, origDir1);
    EXPECT_EQ(context.Snake2.Direction, origDir2);
}

TEST(GameContextTest, ProcessTurnPreventsOppositeDirection) {
    GameContext context;
    context.UserId1 = 100;
    context.UserId2 = 200;

    EXPECT_EQ(context.Snake1.Direction, 'w');

    context.ProcessTurn(100, 's');
    EXPECT_EQ(context.Snake1.Direction, 'w');

    context.ProcessTurn(100, 'd');
    EXPECT_EQ(context.Snake1.Direction, 'd');

    context.ProcessTurn(100, 'a');
    EXPECT_EQ(context.Snake1.Direction, 'd');
}

TEST(GameContextTest, UpdateFieldHandlesNormalMovement) {
    TestableGameContext context;
    context.Snake1.Body.clear();
    context.Snake1.Body.push_front({5, 5});

    context.Field = std::vector(64, std::vector<bool>(64, false));

    context.UpdateField(context.Snake1);

    EXPECT_TRUE(context.Field[5][5]);
}

TEST(GameContextTest, UpdateFieldHandlesBerryConsumption) {
    TestableGameContext context;
    context.Snake1.Body.clear();
    context.Snake1.Body.push_front({5, 5});

    context.Field = std::vector(64, std::vector<bool>(64, false));

    context.Berry = {5, 5};

    size_t originalLength = context.Snake1.Body.size();

    context.UpdateField(context.Snake1);

    EXPECT_EQ(context.Snake1.Body.size(), originalLength);

    EXPECT_NE(context.Berry, std::make_pair(5, 5));
}

TEST(GameContextTest, CheckLoseDetectsBoundaryCollisions) {
    TestableGameContext context;

    EXPECT_TRUE(context.CheckLose({-1, 0}));
    EXPECT_TRUE(context.CheckLose({0, -1}));
    EXPECT_TRUE(context.CheckLose({64, 0}));
    EXPECT_TRUE(context.CheckLose({0, 64}));
}

TEST(GameContextTest, CheckLoseDetectsCollisionWithSnake) {
    TestableGameContext context;

    context.Field = std::vector(64, std::vector<bool>(64, false));

    context.Field[10][10] = true;

    EXPECT_TRUE(context.CheckLose({10, 10}));

    EXPECT_FALSE(context.CheckLose({11, 11}));
}

TEST(GameContextTest, CheckEndGameHandlesDraw) {
    TestableGameContext context;
    context.UserId1 = 100;
    context.UserId2 = 200;

    bool result = context.CheckEndGame(true, true);

    EXPECT_TRUE(result);
    EXPECT_TRUE(context.GameOver);
    EXPECT_EQ(context.Winner, 0);
}

TEST(GameContextTest, CheckEndGameHandlesFirstPlayerLoss) {
    TestableGameContext context;
    context.UserId1 = 100;
    context.UserId2 = 200;

    bool result = context.CheckEndGame(true, false);

    EXPECT_TRUE(result);
    EXPECT_TRUE(context.GameOver);
    EXPECT_EQ(context.Winner, 200);
}

TEST(GameContextTest, CheckEndGameHandlesSecondPlayerLoss) {
    TestableGameContext context;
    context.UserId1 = 100;
    context.UserId2 = 200;

    bool result = context.CheckEndGame(false, true);

    EXPECT_TRUE(result);
    EXPECT_TRUE(context.GameOver);
    EXPECT_EQ(context.Winner, 100);
}

TEST(GameContextTest, CheckEndGameHandlesNoLoss) {
    TestableGameContext context;

    bool result = context.CheckEndGame(false, false);

    EXPECT_FALSE(result);
    EXPECT_FALSE(context.GameOver);
    EXPECT_EQ(context.Winner, 0);
}

TEST(GameContextTest, UpdateGameHandlesNormalMovement) {
    TestableGameContext context;
    context.UserId1 = 100;
    context.UserId2 = 200;

    auto initialPos1 = context.Snake1.Body.front();
    auto initialPos2 = context.Snake2.Body.front();

    context.UpdateGame();

    EXPECT_NE(context.Snake1.Body.front(), initialPos1);
    EXPECT_NE(context.Snake2.Body.front(), initialPos2);

    EXPECT_FALSE(context.GameOver);
}

TEST(GameContextTest, UpdateGameHandlesCollision) {
    TestableGameContext context;
    context.UserId1 = 100;
    context.UserId2 = 200;

    context.Snake1.Body.clear();
    context.Snake2.Body.clear();
    context.Snake1.Body.push_front({9, 10});
    context.Snake2.Body.push_front({11, 10});

    context.Snake1.Direction = 'd';
    context.Snake2.Direction = 'a';

    context.UpdateGame();

    EXPECT_TRUE(context.GameOver);
    EXPECT_EQ(context.Winner, 0);
}

TEST(GameContextTest, UpdateGameHandlesWallCollision) {
    TestableGameContext context;
    context.UserId1 = 100;
    context.UserId2 = 200;

    context.Snake1.Body.clear();
    context.Snake1.Body.push_front({0, 0});
    context.Snake1.Direction = 'a';

    context.Snake2.Body.clear();
    context.Snake2.Body.push_front({30, 30});

    context.UpdateGame();

    EXPECT_TRUE(context.GameOver);
    EXPECT_EQ(context.Winner, 200);
}

TEST(GameContextTest, UpdateGameHandlesSelfCollision) {
    TestableGameContext context;
    context.UserId1 = 100;
    context.UserId2 = 200;

    context.Snake1.Body.clear();
    context.Snake1.Body.push_front({10, 10});
    context.Snake1.Body.push_back({10, 9});
    context.Snake1.Body.push_back({9, 9});
    context.Snake1.Body.push_back({9, 10});
    context.Snake1.Direction = 'a';

    context.Field = std::vector(64, std::vector<bool>(64, false));
    for (const auto& segment : context.Snake1.Body) {
        context.Field[segment.first][segment.second] = true;
    }

    context.Snake2.Body.clear();
    context.Snake2.Body.push_front({30, 30});
    context.Field[30][30] = true;

    context.UpdateGame();

    EXPECT_TRUE(context.GameOver);
    EXPECT_EQ(context.Winner, 200);
}

TEST(GameContextTest, UpdateGameHandlesBerryCollection) {
    TestableGameContext context;

    context.Field = std::vector(64, std::vector<bool>(64, false));

    context.Snake1.Body.clear();
    context.Snake1.Body.push_front({31, 32});
    context.Field[31][32] = true;
    context.Snake1.Direction = 'd';

    context.Berry = {32, 32};

    size_t initialLength = context.Snake1.Body.size();

    context.UpdateGame();

    EXPECT_EQ(context.Snake1.Body.size(), initialLength + 1);

    EXPECT_NE(context.Berry, std::make_pair(32, 32));
}

TEST(GameContextTest, UpdateGameDoesNothingWhenGameOver) {
    TestableGameContext context;
    context.GameOver = true;

    auto snake1Pos = context.Snake1.Body.front();
    auto snake2Pos = context.Snake2.Body.front();

    context.UpdateGame();

    EXPECT_EQ(context.Snake1.Body.front(), snake1Pos);
    EXPECT_EQ(context.Snake2.Body.front(), snake2Pos);
}

TEST(GameContextIntegrationTest, PathToCollision) {
    TestableGameContext context;
    context.UserId1 = 100;
    context.UserId2 = 200;

    context.Snake1.Body.clear();
    context.Snake1.Body.push_front({10, 10});
    context.Snake1.Direction = 'd';

    context.Snake2.Body.clear();
    context.Snake2.Body.push_front({20, 10});
    context.Snake2.Direction = 'a';

    context.Field = std::vector(64, std::vector<bool>(64, false));
    context.Field[10][10] = true;
    context.Field[20][10] = true;

    int steps = 0;
    const int maxSteps = 15;

    while (!context.GameOver && steps < maxSteps) {
        context.UpdateGame();
        steps++;
    }

    EXPECT_TRUE(context.GameOver);
    EXPECT_EQ(context.Winner, 0);
    EXPECT_LT(steps, maxSteps);
}

TEST(GameContextPerformanceTest, MultipleGameUpdates) {
    TestableGameContext context;
    context.UserId1 = 100;
    context.UserId2 = 200;

    const int updateCount = 1000;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < updateCount && !context.GameOver; ++i) {
        context.UpdateGame();
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;

    std::cout << "Performed " << updateCount << " game updates in "
              << elapsed.count() << " ms" << std::endl;

    SUCCEED();
}

TEST(GameContextConcurrencyTest, ParallelProcessTurn) {
    GameContext context;
    context.UserId1 = 100;
    context.UserId2 = 200;

    const int threadCount = 4;
    const int operationsPerThread = 100;

    std::vector<std::thread> threads;

    for (int t = 0; t < threadCount; ++t) {
        threads.emplace_back([&context, t, operationsPerThread]() {
            for (int i = 0; i < operationsPerThread; ++i) {
                int userId = (t % 2 == 0) ? 100 : 200;
                char direction = "wasd"[i % 4];
                context.ProcessTurn(userId, direction);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    SUCCEED();
}
