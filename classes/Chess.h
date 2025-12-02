#pragma once

#include "Game.h"
#include "Grid.h"
#include "Bitboard.h"
#include <cstdint>

constexpr int pieceSize = 80;

/**
 * Chess Game Implementation using Bitboards for Optimized Move Generation
 * 
 * Key Features:
 * - Magic bitboards for O(1) sliding piece move generation
 * - Bulk bitboard operations for pawn moves
 * - Full chess rules: castling, en passant, promotion
 * - Optimized for AI move generation (minimax/alpha-beta)
 */
class Chess : public Game
{
public:
    // ========== LIFECYCLE ==========
    Chess();
    ~Chess();
    
    // ========== GAME INTERFACE ==========
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
    void loadPositionFromFEN(const std::string& fen, int currentPlayer = 0);
    
    Grid* getGrid() override { return _grid; }
    
    // ========== AI INTERFACE ==========
    bool gameHasAI() override { return true; }
    void updateAI() override;
    std::vector<BitMove> generateAllLegalMoves(int playerNumber);
    
    // ========== AI HELPERS ==========
    int negamax(int depth, int alpha, int beta, int playerNumber);
    int evaluatePosition(int playerNumber);
    
    // ========== PROMOTION INTERFACE ==========
    bool isPromotionPending() const { return _pendingPromotionSquare != -1; }
    void completePromotion(ChessPiece promotionPiece);

private:
    // ========== PIECE CREATION ==========
    Bit* PieceForPlayer(const int playerNumber, ChessPiece piece);
    
    // ========== BOARD STATE ==========
    Player* ownerAt(int x, int y) const;
    void FENtoBoard(const std::string& fen);
    char pieceNotation(int x, int y) const;
    
    // ========== BITBOARD MANAGEMENT ==========
    void initializeBitboards();
    void updateBitboards();
    void addPieceToBitboard(int square, ChessPiece piece, int playerNumber);
    void removePieceFromBitboard(int square, ChessPiece piece, int playerNumber);
    void movePieceOnBitboard(int fromSquare, int toSquare, ChessPiece piece, int playerNumber);
    
    // ========== BITBOARD QUERIES ==========
    uint64_t getOccupiedSquares() const;
    uint64_t getEmptySquares() const;
    uint64_t getPlayerPieces(int playerNumber) const;
    BitboardElement getPieceBitboard(ChessPiece piece, int playerNumber) const;
    
    // ========== MOVE GENERATION HELPERS ==========
    void initializePawnMoves(int playerNumber);
    uint64_t generatePawnMoves(int square, int playerNumber) const;
    
    // ========== CASTLING HELPERS ==========
    bool canCastleKingside(int playerNumber) const;
    bool canCastleQueenside(int playerNumber) const;
    void performCastling(int kingToSquare, int kingFromSquare, int playerNumber);
    void updateCastlingRights(int pieceType, int fromSquare, int playerNumber);
    void handleRookCapture(int toSquare, int capturedType);
    
    void handlePromotion(int square, ChessPiece promotionPiece, int playerNumber);
    
    bool isSquareAttacked(int square, int byPlayer) const;
    bool isKingInCheck(int playerNumber) const;
    
    struct CapturedPieceInfo {
        ChessPiece piece;
        int player;
        int square;
        bool wasCapture;
        int previousEnPassant;
        bool whiteKingMoved;
        bool whiteKingsideRookMoved;
        bool whiteQueensideRookMoved;
        bool blackKingMoved;
        bool blackKingsideRookMoved;
        bool blackQueensideRookMoved;
        
        CapturedPieceInfo()
            : piece(NoPiece),
              player(-1),
              square(-1),
              wasCapture(false),
              previousEnPassant(-1),
              whiteKingMoved(false),
              whiteKingsideRookMoved(false),
              whiteQueensideRookMoved(false),
              blackKingMoved(false),
              blackKingsideRookMoved(false),
              blackQueensideRookMoved(false) {}
    };
    
    CapturedPieceInfo makeMoveBitboard(const BitMove& move, int playerNumber);
    void unmakeMoveBitboard(const BitMove& move, int playerNumber, const CapturedPieceInfo& captured);
    bool isMoveLegal(const BitMove& move, int playerNumber);
    std::vector<BitMove> filterLegalMoves(const std::vector<BitMove>& moves, int playerNumber);
    
    // ========== FILE MASKS FOR EDGE DETECTION ==========
    static constexpr uint64_t notAFile = 0xFEFEFEFEFEFEFEFEULL;
    static constexpr uint64_t notHFile = 0x7F7F7F7F7F7F7F7FULL;
    
    // ========== MEMBER VARIABLES ==========
    Grid* _grid;
    
    // White piece bitboards
    uint64_t _whitePawns;
    uint64_t _whiteKnights;
    uint64_t _whiteBishops;
    uint64_t _whiteRooks;
    uint64_t _whiteQueens;
    uint64_t _whiteKings;
    
    // Black piece bitboards
    uint64_t _blackPawns;
    uint64_t _blackKnights;
    uint64_t _blackBishops;
    uint64_t _blackRooks;
    uint64_t _blackQueens;
    uint64_t _blackKings;
    
    // Pre-computed pawn move cache (refreshed each turn)
    BitboardElement _pawnMovesBitboards[64];
    
    // Special move state
    int _enPassantSquare;              // -1 if none available
    int _pendingPromotionSquare;       // -1 if no promotion pending
    int _pendingPromotionPlayer;       // Which player is promoting
    
    // Castling rights
    bool _whiteKingMoved;
    bool _whiteKingsideRookMoved;      // h1 rook
    bool _whiteQueensideRookMoved;     // a1 rook
    bool _blackKingMoved;
    bool _blackKingsideRookMoved;      // h8 rook
    bool _blackQueensideRookMoved;     // a8 rook
    
    // AI state
    bool _aiMovedThisTurn;             // Tracks if AI has already moved this turn
    int _lastAIPlayer;
};
