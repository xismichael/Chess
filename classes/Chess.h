#pragma once

#include "Game.h"
#include "Grid.h"
#include "Bitboard.h"
#include <cstdint>

constexpr int pieceSize = 80;

class Chess : public Game
{
public:
    Chess();
    ~Chess();

    void setUpBoard() override;

    bool canBitMoveFrom(Bit &bit, BitHolder &src) override;
    bool canBitMoveFromTo(Bit &bit, BitHolder &src, BitHolder &dst) override;
    bool actionForEmptyHolder(BitHolder &holder) override;
    void bitMovedFromTo(Bit &bit, BitHolder &src, BitHolder &dst) override;

    void stopGame() override;

    Player *checkForWinner() override;
    bool checkForDraw() override;

    std::string initialStateString() override;
    std::string stateString() override;
    void setStateString(const std::string &s) override;

    Grid* getGrid() override { return _grid; }
    
    // Generate all legal moves (for testing/AI)
    std::vector<BitMove> generateAllLegalMoves(int playerNumber);

private:
    Bit* PieceForPlayer(const int playerNumber, ChessPiece piece);
    Player* ownerAt(int x, int y) const;
    void FENtoBoard(const std::string& fen);
    char pieceNotation(int x, int y) const;
    
    // Bitboard management
    void initializeBitboards();
    void updateBitboards();
    void addPieceToBitboard(int square, ChessPiece piece, int playerNumber);
    void removePieceFromBitboard(int square, ChessPiece piece, int playerNumber);
    void movePieceOnBitboard(int fromSquare, int toSquare, ChessPiece piece, int playerNumber);
    
    uint64_t getOccupiedSquares() const;
    uint64_t getEmptySquares() const;
    uint64_t getPlayerPieces(int playerNumber) const;
    BitboardElement getPieceBitboard(ChessPiece piece, int playerNumber) const;
    
    // Move lookup tables
    void initializeKnightMoves();
    void initializeKingMoves();
    void initializePawnMoves(int playerNumber);
    
    // Pawn move generation helper
    uint64_t generatePawnMoves(int square, int playerNumber) const;

    Grid* _grid;
    
    // White pieces
    uint64_t _whitePawns;
    uint64_t _whiteKnights;
    uint64_t _whiteBishops;
    uint64_t _whiteRooks;
    uint64_t _whiteQueens;
    uint64_t _whiteKings;
    
    // Black pieces
    uint64_t _blackPawns;
    uint64_t _blackKnights;
    uint64_t _blackBishops;
    uint64_t _blackRooks;
    uint64_t _blackQueens;
    uint64_t _blackKings;
    
    // Pre computed move lookup tables
    BitboardElement _knightBitboards[64];
    BitboardElement _kingBitboards[64];
    BitboardElement _pawnMovesBitboards[64];  // Refreshed each turn
};