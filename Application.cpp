#include "Application.h"
#include "imgui/imgui.h"
#include "classes/TicTacToe.h"
#include "classes/Checkers.h"
#include "classes/Othello.h"
#include "classes/Connect4.h"
#include "classes/Chess.h"

#define TOURNAMENT_IMPLEMENTATION
#include "classes/Tournament.h"

namespace ClassGame {
        //
        // our global variables
        //
        Game *game = nullptr;
        TournamentClient *client = nullptr;
        bool gameOver = false;
        int gameWinner = -1;

        //
        // game starting point
        // this is called by the main render loop in main.cpp
        //
        void GameStartUp() 
        {
            game = nullptr;
        }

        //
        // game render loop
        // this is called by the main render loop in main.cpp
        //
        void RenderGame() 
        {
                ImGui::DockSpaceOverViewport();

                //ImGui::ShowDemoWindow();

                ImGui::Begin("Settings");

                if (gameOver) {
                    ImGui::Text("Game Over!");
                    ImGui::Text("Winner: %d", gameWinner);
                    if (ImGui::Button("Reset Game")) {
                        game->stopGame();
                        game->setUpBoard();
                        gameOver = false;
                        gameWinner = -1;
                    }
                }
                if (!game) {
                    if (ImGui::Button("Start Tic-Tac-Toe")) {
                        game = new TicTacToe();
                        game->setUpBoard();
                    }
                    if (ImGui::Button("Start Checkers")) {
                        game = new Checkers();
                        game->setUpBoard();
                    }
                    if (ImGui::Button("Start Othello")) {
                        game = new Othello();
                        game->setUpBoard();
                    }
                    if (ImGui::Button("Start Connect 4")) {
                        game = new Connect4();
                        game->setUpBoard();
                    }
                    if (ImGui::Button("Start Chess")) {
                        game = new Chess();
                        game->setUpBoard();
                    }
                    if (ImGui::Button("AI vs AI"))
                    {
                        game = new Chess();
                        game->setUpBoard();
                        game->_gameOptions.AIvsAI = true;
                        // Mark both players as AI
                        game->getPlayerAt(0)->setAIPlayer(true);
                        game->getPlayerAt(1)->setAIPlayer(true);
                    }
                    if (ImGui::Button("Start Online Tournament")) {
                        game = new Chess();
                        game->setUpBoard();
                        client = new TournamentClient((Chess *)game, "stupid");       // THIS SHOULD BE YOUR BOT NAME
                        client->connect("13.223.80.180", 5000);
                    }
                } else {
                    ImGui::Text("Current Player Number: %d", game->getCurrentPlayer()->playerNumber());
                    std::string stateString = game->stateString();
                    int stride = game->_gameOptions.rowX;
                    int height = game->_gameOptions.rowY;

                    for(int y=0; y<height; y++) {
                        ImGui::Text("%s", stateString.substr(y*stride,stride).c_str());
                    }
                    ImGui::Text("Current Board State: %s", game->stateString().c_str());
                    
                    // AI Player Selection
                    if (game->gameHasAI())
                    {
                        ImGui::Separator();
                        ImGui::Text("=== AI SETTINGS ===");
                        
                        static bool player1AI = false;
                        if (ImGui::Checkbox("Player 1 AI (White)", &player1AI))
                        {
                            game->getPlayerAt(0)->setAIPlayer(player1AI);
                        }
                        ImGui::SameLine();
                        static bool player2AI = false;
                        if (ImGui::Checkbox("Player 2 AI (Black)", &player2AI))
                        {
                            game->getPlayerAt(1)->setAIPlayer(player2AI);
                        }
                        
                        if (gameOver)
                        {
                            player1AI = false;
                            player2AI = false;
                        }
                        ImGui::Separator();
                    }
                    
                    // Check if this is a Chess game and if promotion is pending
                    Chess* chessGame = dynamic_cast<Chess*>(game);
                    if (chessGame && chessGame->isPromotionPending()) {
                        ImGui::Separator();
                        ImGui::Text("=== PAWN PROMOTION ===");
                        ImGui::Text("Choose promotion piece:");
                        
                        // Create a 2x2 grid of buttons
                        if (ImGui::Button("Queen", ImVec2(120, 40))) {
                            chessGame->completePromotion(Queen);
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Rook", ImVec2(120, 40))) {
                            chessGame->completePromotion(Rook);
                        }
                        
                        if (ImGui::Button("Bishop", ImVec2(120, 40))) {
                            chessGame->completePromotion(Bishop);
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Knight", ImVec2(120, 40))) {
                            chessGame->completePromotion(Knight);
                        }
                        ImGui::Separator();
                    }
                }
                ImGui::End();

                ImGui::Begin("GameWindow");
                if (client) {
                    client->update();
                } else if (game) {
                    if (game->gameHasAI() && (game->getCurrentPlayer()->isAIPlayer() || game->_gameOptions.AIvsAI))
                    {
                        game->updateAI();
                    }
                    game->drawFrame();
                }
                ImGui::End();
        }

        //
        // end turn is called by the game code at the end of each turn
        // this is where we check for a winner
        //
        void EndOfTurn() 
        {
            Player *winner = game->checkForWinner();
            if (winner)
            {
                gameOver = true;
                gameWinner = winner->playerNumber();
            }
            if (game->checkForDraw()) {
                gameOver = true;
                gameWinner = -1;
            }
        }
}
