#include "Chess.h"
#include "MagicBitboards.h"
#include <vector>
#include <algorithm>

Chess::Chess()
{
    _grid = new Grid(8, 8);
    
    // Initialize magic bitboards
    initMagicBitboards();
    
    // Initialize all bitboards
    initializeBitboards();
    
    // Initialize pawn move
    for (int i = 0; i < 64; i++) {
        _pawnMovesBitboards[i] = BitboardElement(0ULL);
    }
    
    // Initialize special move state
    _enPassantSquare = -1;
    _pendingPromotionSquare = -1;
    _pendingPromotionPlayer = -1;
    
    // Initialize castling
    _whiteKingMoved = false;
    _whiteKingsideRookMoved = false;
    _whiteQueensideRookMoved = false;
    _blackKingMoved = false;
    _blackKingsideRookMoved = false;
    _blackQueensideRookMoved = false;
    
    // Initialize AI
    _aiMovedThisTurn = false;
    _lastAIPlayer = -1;
}

Chess::~Chess()
{
    cleanupMagicBitboards();
    delete _grid;
}

void Chess::setUpBoard()
{
    setNumberOfPlayers(2);
    _gameOptions.rowX = 8;
    _gameOptions.rowY = 8;
    _grid->initializeChessSquares(pieceSize, "boardsquare.png");
    FENtoBoard("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR");
    // Synchronize bitboards with visual board
    updateBitboards();
    initializePawnMoves(0);
    startGame();
}

void Chess::FENtoBoard(const std::string& fen)
{
    int curIndex = 0;
    int curBoardIndex = 0;
    
    while (curBoardIndex < 64 && curIndex < fen.length())
    {
        char c = fen[curIndex];
        
        // Skip rank separators
        if (c == '/') {
            curIndex++;
            continue;
        }
        
        // Handle empty squares (numbers 1-8)
        if (c >= '1' && c <= '8') {
            curBoardIndex += (c - '0');
            curIndex++;
            continue;
        }
        int playerNumber;
        ChessPiece piece;
        
        if (c >= 'a' && c <= 'z') {
            playerNumber = 1;
            c = toupper(c);
        }
        else if (c >= 'A' && c <= 'Z') {
            playerNumber = 0;
        }
        else {
            curIndex++;
            continue;
        }
        
        // Map character to piece type
        switch (c) {
            case 'P': piece = Pawn; break;
            case 'N': piece = Knight; break;
            case 'B': piece = Bishop; break;
            case 'R': piece = Rook; break;
            case 'Q': piece = Queen; break;
            case 'K': piece = King; break;
            default:
                curIndex++;
                continue;
        }
        
        // Place piece on board (convert to x,y coordinates)
        int x = curBoardIndex % 8;
        int y = 7 - (curBoardIndex / 8);  // FEN starts from rank 8
        
        _grid->getSquare(x, y)->dropBitAtPoint(
            PieceForPlayer(playerNumber, piece),
            {(float)x, (float)y}
        );
        
        curIndex++;
        curBoardIndex++;
    }
}

void Chess::loadPositionFromFEN(const std::string& fen, int currentPlayer)
{
    _grid->forEachSquare([&](ChessSquare* square, int /*x*/, int /*y*/) {
        square->destroyBit();
        square->setBit(nullptr);
    });

    initializeBitboards();
    _enPassantSquare = -1;
    _pendingPromotionSquare = -1;
    _pendingPromotionPlayer = -1;
    _whiteKingMoved = false;
    _whiteKingsideRookMoved = false;
    _whiteQueensideRookMoved = false;
    _blackKingMoved = false;
    _blackKingsideRookMoved = false;
    _blackQueensideRookMoved = false;
    _aiMovedThisTurn = false;
    _lastAIPlayer = -1;

    FENtoBoard(fen);
    updateBitboards();
    _gameOptions.currentTurnNo = currentPlayer;
    initializePawnMoves(currentPlayer);
}

Bit* Chess::PieceForPlayer(const int playerNumber, ChessPiece piece)
{
    const char* pieces[] = {
        "pawn.png", "knight.png", "bishop.png", 
        "rook.png", "queen.png", "king.png"
    };
    
    Bit* bit = new Bit();
    std::string spritePath = std::string(playerNumber == 0 ? "w_" : "b_") + pieces[piece - 1];
    bit->LoadTextureFromFile(spritePath.c_str());
    bit->setOwner(getPlayerAt(playerNumber));
    bit->setSize(pieceSize, pieceSize);
    bit->setGameTag(piece + (playerNumber * 128));  // Encode player in high bit
    
    return bit;
}

bool Chess::canBitMoveFrom(Bit &bit, BitHolder &src)
{
    int currentPlayerMask = getCurrentPlayer()->playerNumber() * 128;
    int pieceMask = bit.gameTag() & 128;
    return (pieceMask == currentPlayerMask);
}

bool Chess::canBitMoveFromTo(Bit &bit, BitHolder &src, BitHolder &dst)
{
    ChessSquare* srcSquare = dynamic_cast<ChessSquare*>(&src);
    ChessSquare* dstSquare = dynamic_cast<ChessSquare*>(&dst);
    
    if (!srcSquare || !dstSquare) return false;
    
    int srcSquareIndex = srcSquare->getSquareIndex();
    int dstSquareIndex = dstSquare->getSquareIndex();
    int gameTag = bit.gameTag();
    int pieceType = gameTag % 128;
    int playerNumber = (gameTag & 128) ? 1 : 0;

    // Get bitboard masks for this position
    uint64_t friendlyPieces = getPlayerPieces(playerNumber);
    uint64_t occupied = getOccupiedSquares();
    uint64_t validMoves = 0ULL;
    
    // Generate valid moves based on piece type
    switch (pieceType) {
        case Pawn:
            validMoves = _pawnMovesBitboards[srcSquareIndex].getData();
            break;
            
        case Knight:
            validMoves = KnightAttacks[srcSquareIndex] & ~friendlyPieces;
            break;
        
        case Bishop:
            validMoves = getBishopAttacks(srcSquareIndex, occupied) & ~friendlyPieces;
            break;
            
        case Rook:
            validMoves = getRookAttacks(srcSquareIndex, occupied) & ~friendlyPieces;
            break;
            
        case Queen:
            validMoves = getQueenAttacks(srcSquareIndex, occupied) & ~friendlyPieces;
            break;
            
        case King:
            validMoves = KingAttacks[srcSquareIndex] & ~friendlyPieces;
            
            // Add castling moves
            if (abs(dstSquareIndex - srcSquareIndex) == 2) {
                if (dstSquareIndex > srcSquareIndex && canCastleKingside(playerNumber)) {
                    validMoves |= (1ULL << dstSquareIndex);
                } else if (dstSquareIndex < srcSquareIndex && canCastleQueenside(playerNumber)) {
                    validMoves |= (1ULL << dstSquareIndex);
                }
            }
            break;
            
        default:
            return false;
    }
    
    // Check if destination square is in pseudo-legal moves
    if ((validMoves & (1ULL << dstSquareIndex)) == 0) {
        return false;
    }
    
    // CRITICAL: Check if move leaves own king in check (legal move validation)
    BitMove move(srcSquareIndex, dstSquareIndex, static_cast<ChessPiece>(pieceType));
    return isMoveLegal(move, playerNumber);
}

bool Chess::actionForEmptyHolder(BitHolder &holder)
{
    return false;
}

void Chess::bitMovedFromTo(Bit &bit, BitHolder &src, BitHolder &dst)
{
    ChessSquare* srcSquare = dynamic_cast<ChessSquare*>(&src);
    ChessSquare* dstSquare = dynamic_cast<ChessSquare*>(&dst);
    
    if (!srcSquare || !dstSquare) return;
    
    int fromSquare = srcSquare->getSquareIndex();
    int toSquare = dstSquare->getSquareIndex();
    int gameTag = bit.gameTag();
    int pieceType = gameTag % 128;
    int playerNumber = (gameTag & 128) ? 1 : 0;
    
    Bit* capturedPiece = dstSquare->bit();
    if (capturedPiece) {
        int capturedTag = capturedPiece->gameTag();
        int capturedType = capturedTag % 128;
        int capturedPlayer = (capturedTag & 128) ? 1 : 0;
        
        handleRookCapture(toSquare, capturedType);
        
        if (!(pieceType == Pawn && toSquare == _enPassantSquare && _enPassantSquare != -1)) {
            removePieceFromBitboard(toSquare, static_cast<ChessPiece>(capturedType), capturedPlayer);
        }
    }
    
    // Move piece visually
    bit.setParent(dstSquare);
    srcSquare->setBit(nullptr);
    dstSquare->setBit(&bit);
    bit.moveTo(dstSquare->getPosition());
    
    if (pieceType == Pawn && toSquare == _enPassantSquare && _enPassantSquare != -1) {
        int capturedPawnSquare = (playerNumber == 0) ? toSquare - 8 : toSquare + 8;
        removePieceFromBitboard(capturedPawnSquare, Pawn, 1 - playerNumber);
        
        int capturedX = capturedPawnSquare % 8;
        int capturedY = capturedPawnSquare / 8;
        _grid->getSquare(capturedX, capturedY)->destroyBit();
    }
    
    movePieceOnBitboard(fromSquare, toSquare, static_cast<ChessPiece>(pieceType), playerNumber);
    updateBitboards();
    
    // Handle pawn promotion
    if (pieceType == Pawn) {
        int toRank = toSquare / 8;
        int promotionRank = (playerNumber == 0) ? 7 : 0;
        
        if (toRank == promotionRank) {
            Player* currentPlayer = getPlayerAt(playerNumber);
            if (currentPlayer && currentPlayer->isAIPlayer()) {
                handlePromotion(toSquare, Queen, playerNumber);
            } else {
                _pendingPromotionSquare = toSquare;
                _pendingPromotionPlayer = playerNumber;
                return;
            }
        }
    }
    
    // Handle castling
    if (pieceType == King && abs(toSquare - fromSquare) == 2) {
        performCastling(toSquare, fromSquare, playerNumber);
    }
    
    // Update castling rights
    updateCastlingRights(pieceType, fromSquare, playerNumber);
    
    // Update en passant state
    if (pieceType == Pawn && abs(toSquare - fromSquare) == 16) {
        _enPassantSquare = (playerNumber == 0) ? fromSquare + 8 : fromSquare - 8;
    } else {
        _enPassantSquare = -1;
    }
    
    endTurn();
    initializePawnMoves(getCurrentPlayer()->playerNumber());
}

void Chess::initializeBitboards()
{
    _whitePawns = 0ULL;
    _whiteKnights = 0ULL;
    _whiteBishops = 0ULL;
    _whiteRooks = 0ULL;
    _whiteQueens = 0ULL;
    _whiteKings = 0ULL;

    _blackPawns = 0ULL;
    _blackKnights = 0ULL;
    _blackBishops = 0ULL;
    _blackRooks = 0ULL;
    _blackQueens = 0ULL;
    _blackKings = 0ULL;
}

void Chess::updateBitboards()
{
    initializeBitboards();
    
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            Bit* bit = _grid->getSquare(x, y)->bit();
            if (bit) {
                int square = y * 8 + x;
                int gameTag = bit->gameTag();
                int pieceType = gameTag % 128;
                int playerNumber = (gameTag & 128) ? 1 : 0;
                addPieceToBitboard(square, static_cast<ChessPiece>(pieceType), playerNumber);
            }
        }
    }
}

void Chess::addPieceToBitboard(int square, ChessPiece piece, int playerNumber)
{
    uint64_t mask = (1ULL << square);
    
    if (playerNumber == 0) {
        switch (piece) {
            case Pawn:   _whitePawns |= mask; break;
            case Knight: _whiteKnights |= mask; break;
            case Bishop: _whiteBishops |= mask; break;
            case Rook:   _whiteRooks |= mask; break;
            case Queen:  _whiteQueens |= mask; break;
            case King:   _whiteKings |= mask; break;
            default: break;
        }
    } else {
        switch (piece) {
            case Pawn:   _blackPawns |= mask; break;
            case Knight: _blackKnights |= mask; break;
            case Bishop: _blackBishops |= mask; break;
            case Rook:   _blackRooks |= mask; break;
            case Queen:  _blackQueens |= mask; break;
            case King:   _blackKings |= mask; break;
            default: break;
        }
    }
}

void Chess::removePieceFromBitboard(int square, ChessPiece piece, int playerNumber)
{
    uint64_t mask = ~(1ULL << square);
    
    if (playerNumber == 0) {
        switch (piece) {
            case Pawn:   _whitePawns &= mask; break;
            case Knight: _whiteKnights &= mask; break;
            case Bishop: _whiteBishops &= mask; break;
            case Rook:   _whiteRooks &= mask; break;
            case Queen:  _whiteQueens &= mask; break;
            case King:   _whiteKings &= mask; break;
            default: break;
        }
    } else {
        switch (piece) {
            case Pawn:   _blackPawns &= mask; break;
            case Knight: _blackKnights &= mask; break;
            case Bishop: _blackBishops &= mask; break;
            case Rook:   _blackRooks &= mask; break;
            case Queen:  _blackQueens &= mask; break;
            case King:   _blackKings &= mask; break;
            default: break;
        }
    }
}

void Chess::movePieceOnBitboard(int fromSquare, int toSquare, ChessPiece piece, int playerNumber)
{
    removePieceFromBitboard(fromSquare, piece, playerNumber);
    addPieceToBitboard(toSquare, piece, playerNumber);
}

uint64_t Chess::getOccupiedSquares() const
{
    return getPlayerPieces(0) | getPlayerPieces(1);
}

uint64_t Chess::getEmptySquares() const
{
    return ~getOccupiedSquares();
}

uint64_t Chess::getPlayerPieces(int playerNumber) const
{
    if (playerNumber == 0) {
        return _whitePawns | _whiteKnights | _whiteBishops | 
               _whiteRooks | _whiteQueens | _whiteKings;
    } else {
        return _blackPawns | _blackKnights | _blackBishops | 
               _blackRooks | _blackQueens | _blackKings;
    }
}

BitboardElement Chess::getPieceBitboard(ChessPiece piece, int playerNumber) const
{
    if (playerNumber == 0) {
        switch (piece) {
            case Pawn:   return BitboardElement(_whitePawns);
            case Knight: return BitboardElement(_whiteKnights);
            case Bishop: return BitboardElement(_whiteBishops);
            case Rook:   return BitboardElement(_whiteRooks);
            case Queen:  return BitboardElement(_whiteQueens);
            case King:   return BitboardElement(_whiteKings);
            default:     return BitboardElement(0ULL);
        }
    } else {
        switch (piece) {
            case Pawn:   return BitboardElement(_blackPawns);
            case Knight: return BitboardElement(_blackKnights);
            case Bishop: return BitboardElement(_blackBishops);
            case Rook:   return BitboardElement(_blackRooks);
            case Queen:  return BitboardElement(_blackQueens);
            case King:   return BitboardElement(_blackKings);
            default:     return BitboardElement(0ULL);
        }
    }
}

void Chess::initializePawnMoves(int playerNumber)
{
    uint64_t pawns = getPieceBitboard(Pawn, playerNumber).getData();
    uint64_t occupied = getOccupiedSquares();
    uint64_t empty = ~occupied;
    uint64_t enemies = getPlayerPieces(1 - playerNumber);
    
    int direction = (playerNumber == 0) ? 1 : -1;
    int shiftAmount = direction * 8;
    
    // Bulk operations for all pawns
    uint64_t singlePush = (playerNumber == 0) 
        ? (pawns << 8) & empty 
        : (pawns >> 8) & empty;
    
    uint64_t doublePush = (playerNumber == 0)
        ? ((singlePush & 0x0000000000FF0000ULL) << 8) & empty  
        : ((singlePush & 0x0000FF0000000000ULL) >> 8) & empty; 
    
    uint64_t leftCaptures = (playerNumber == 0)
        ? ((pawns & notAFile) << 7) & enemies
        : ((pawns & notAFile) >> 9) & enemies;
    
    uint64_t rightCaptures = (playerNumber == 0)
        ? ((pawns & notHFile) << 9) & enemies
        : ((pawns & notHFile) >> 7) & enemies;
    
    // Distribute moves to individual pawn caches
    for (int i = 0; i < 64; i++) {
        _pawnMovesBitboards[i] = BitboardElement(0ULL);
    }
    
    BitboardElement pawnsBB(pawns);
    pawnsBB.forEachBit([&](int fromSquare) {
        uint64_t moves = 0ULL;
        int file = fromSquare % 8;
        
        // Single push
        int singleTarget = fromSquare + shiftAmount;
        if (singleTarget >= 0 && singleTarget < 64 && (singlePush & (1ULL << singleTarget))) {
            moves |= (1ULL << singleTarget);
        }
        
        // Double push
        int doubleTarget = fromSquare + (shiftAmount * 2);
        if (doubleTarget >= 0 && doubleTarget < 64 && (doublePush & (1ULL << doubleTarget))) {
            moves |= (1ULL << doubleTarget);
        }
        
        // Left capture
        if (file > 0) {
        int leftTarget = fromSquare + shiftAmount - 1;
            if (leftTarget >= 0 && leftTarget < 64 && (leftCaptures & (1ULL << leftTarget))) {
            moves |= (1ULL << leftTarget);
            }
        }
        
        // Right capture
        if (file < 7) {
        int rightTarget = fromSquare + shiftAmount + 1;
            if (rightTarget >= 0 && rightTarget < 64 && (rightCaptures & (1ULL << rightTarget))) {
            moves |= (1ULL << rightTarget);
            }
        }
        
        // En passant
        if (_enPassantSquare != -1) {
            int epFile = _enPassantSquare % 8;
            int epRank = _enPassantSquare / 8;
            int rank = fromSquare / 8;
            
            if (abs(file - epFile) == 1 && rank == epRank - direction) {
                moves |= (1ULL << _enPassantSquare);
            }
        }
        
        _pawnMovesBitboards[fromSquare] = BitboardElement(moves);
    });
}

std::vector<BitMove> Chess::generateAllLegalMoves(int playerNumber)
{
    std::vector<BitMove> moves;
    moves.reserve(100);  // Pre-allocate for typical position
    
    uint64_t occupied = getOccupiedSquares();
    uint64_t empty = ~occupied;
    uint64_t enemies = getPlayerPieces(1 - playerNumber);
    uint64_t validTargets = empty | enemies;
    
    //PAWN MOVES
    uint64_t pawns = getPieceBitboard(Pawn, playerNumber).getData();
    int direction = (playerNumber == 0) ? 1 : -1;
    int promotionRank = (playerNumber == 0) ? 7 : 0;
    
    // Single pushes
    uint64_t singlePush = (playerNumber == 0) 
        ? (pawns << 8) & empty 
        : (pawns >> 8) & empty;
    
    BitboardElement(singlePush).forEachBit([&](int toSquare) {
        int fromSquare = toSquare - (direction * 8);
        int toRank = toSquare / 8;
        
        if (toRank == promotionRank) {
            moves.emplace_back(fromSquare, toSquare, Pawn, Queen);
            moves.emplace_back(fromSquare, toSquare, Pawn, Rook);
            moves.emplace_back(fromSquare, toSquare, Pawn, Bishop);
            moves.emplace_back(fromSquare, toSquare, Pawn, Knight);
        } else {
        moves.emplace_back(fromSquare, toSquare, Pawn);
        }
    });
    
    // Double pushes
    uint64_t doublePush = (playerNumber == 0)
        ? ((singlePush & 0x0000000000FF0000ULL) << 8) & empty
        : ((singlePush & 0x0000FF0000000000ULL) >> 8) & empty;
    
    BitboardElement(doublePush).forEachBit([&](int toSquare) {
        int fromSquare = toSquare - (direction * 16);
        moves.emplace_back(fromSquare, toSquare, Pawn);
    });
    
    // Left captures
    uint64_t leftCaptures = (playerNumber == 0)
        ? ((pawns & notAFile) << 7) & enemies
        : ((pawns & notAFile) >> 9) & enemies;
    
    BitboardElement(leftCaptures).forEachBit([&](int toSquare) {
        int fromSquare = (playerNumber == 0) ? toSquare - 7 : toSquare + 9;
        int toRank = toSquare / 8;
        
        if (toRank == promotionRank) {
            moves.emplace_back(fromSquare, toSquare, Pawn, Queen);
            moves.emplace_back(fromSquare, toSquare, Pawn, Rook);
            moves.emplace_back(fromSquare, toSquare, Pawn, Bishop);
            moves.emplace_back(fromSquare, toSquare, Pawn, Knight);
        } else {
        moves.emplace_back(fromSquare, toSquare, Pawn);
        }
    });
    
    // Right captures
    uint64_t rightCaptures = (playerNumber == 0)
        ? ((pawns & notHFile) << 9) & enemies
        : ((pawns & notHFile) >> 7) & enemies;
    
    BitboardElement(rightCaptures).forEachBit([&](int toSquare) {
        int fromSquare = (playerNumber == 0) ? toSquare - 9 : toSquare + 7;
        int toRank = toSquare / 8;
        
        if (toRank == promotionRank) {
            moves.emplace_back(fromSquare, toSquare, Pawn, Queen);
            moves.emplace_back(fromSquare, toSquare, Pawn, Rook);
            moves.emplace_back(fromSquare, toSquare, Pawn, Bishop);
            moves.emplace_back(fromSquare, toSquare, Pawn, Knight);
        } else {
        moves.emplace_back(fromSquare, toSquare, Pawn);
        }
    });
    
    // En passant
    if (_enPassantSquare != -1) {
        int epFile = _enPassantSquare % 8;
        
        if (epFile > 0) {
            int leftPawnSquare = (playerNumber == 0) ? _enPassantSquare - 9 : _enPassantSquare + 7;
            if ((pawns & (1ULL << leftPawnSquare)) != 0) {
                moves.emplace_back(leftPawnSquare, _enPassantSquare, Pawn);
            }
        }
        
        if (epFile < 7) {
            int rightPawnSquare = (playerNumber == 0) ? _enPassantSquare - 7 : _enPassantSquare + 9;
            if ((pawns & (1ULL << rightPawnSquare)) != 0) {
                moves.emplace_back(rightPawnSquare, _enPassantSquare, Pawn);
            }
        }
    }
    
    //KNIGHT MOVES
    getPieceBitboard(Knight, playerNumber).forEachBit([&](int fromSquare) {
        uint64_t targets = KnightAttacks[fromSquare] & validTargets;
        BitboardElement(targets).forEachBit([&](int toSquare) {
            moves.emplace_back(fromSquare, toSquare, Knight);
        });
    });
    
    //KING MOVES
    getPieceBitboard(King, playerNumber).forEachBit([&](int fromSquare) {
        uint64_t targets = KingAttacks[fromSquare] & validTargets;
        BitboardElement(targets).forEachBit([&](int toSquare) {
            moves.emplace_back(fromSquare, toSquare, King);
        });
    });
    
    //CASTLING
    if (playerNumber == 0) {
        if (canCastleKingside(playerNumber)) {
            moves.emplace_back(4, 6, King);
        }
        if (canCastleQueenside(playerNumber)) {
            moves.emplace_back(4, 2, King);
        }
    } else {
        if (canCastleKingside(playerNumber)) {
            moves.emplace_back(60, 62, King);
        }
        if (canCastleQueenside(playerNumber)) {
            moves.emplace_back(60, 58, King);
        }
    }
    
    //ROOK MOVES
    getPieceBitboard(Rook, playerNumber).forEachBit([&](int fromSquare) {
        uint64_t targets = getRookAttacks(fromSquare, occupied) & validTargets;
        BitboardElement(targets).forEachBit([&](int toSquare) {
            moves.emplace_back(fromSquare, toSquare, Rook);
        });
    });
    
    //BISHOP MOVES
    getPieceBitboard(Bishop, playerNumber).forEachBit([&](int fromSquare) {
        uint64_t targets = getBishopAttacks(fromSquare, occupied) & validTargets;
        BitboardElement(targets).forEachBit([&](int toSquare) {
            moves.emplace_back(fromSquare, toSquare, Bishop);
        });
    });
    
    //QUEEN MOVES
    getPieceBitboard(Queen, playerNumber).forEachBit([&](int fromSquare) {
        uint64_t targets = getQueenAttacks(fromSquare, occupied) & validTargets;
        BitboardElement(targets).forEachBit([&](int toSquare) {
            moves.emplace_back(fromSquare, toSquare, Queen);
        });
    });
    
    // Filter out illegal moves
    return filterLegalMoves(moves, playerNumber);
}


void Chess::updateAI()
{
    int currentPlayer = getCurrentPlayer()->playerNumber();
    std::vector<BitMove> legalMoves = generateAllLegalMoves(currentPlayer);
    
    if (legalMoves.empty()) {
        return;
    }
    
    BitMove bestMove = legalMoves[0];
    int bestScore = -999999;
    int alpha = -999999;
    int beta = 999999;
    int depth = 7;
    
    for (const BitMove& move : legalMoves) {
        CapturedPieceInfo captured = makeMoveBitboard(move, currentPlayer);
        int score = -negamax(depth - 1, -beta, -alpha, 1 - currentPlayer);
        unmakeMoveBitboard(move, currentPlayer, captured);
        
        if (score > bestScore) {
            bestScore = score;
            bestMove = move;
        }
        
        alpha = std::max(alpha, score);
        if (alpha >= beta) {
            break;
        }
    }
    
    int fromX = bestMove.from % 8;
    int fromY = bestMove.from / 8;
    int toX = bestMove.to % 8;
    int toY = bestMove.to / 8;
    
    ChessSquare* fromSquare = _grid->getSquare(fromX, fromY);
    ChessSquare* toSquare = _grid->getSquare(toX, toY);
    
    if (fromSquare && fromSquare->bit() && toSquare) {
        Bit* bit = fromSquare->bit();
        
        bitMovedFromTo(*bit, *fromSquare, *toSquare);
    }
}

int Chess::negamax(int depth, int alpha, int beta, int playerNumber)
{
    if (depth == 0) {
        return evaluatePosition(playerNumber);
    }
    
    std::vector<BitMove> legalMoves = generateAllLegalMoves(playerNumber);
    
    if (legalMoves.empty()) {
        if (isKingInCheck(playerNumber)) {
            return -999999;
        } else {
            return 0;
        }
    }
    
    int maxScore = -999999;
    
    for (const BitMove& move : legalMoves) {
        CapturedPieceInfo captured = makeMoveBitboard(move, playerNumber);
        int score = -negamax(depth - 1, -beta, -alpha, 1 - playerNumber);
        unmakeMoveBitboard(move, playerNumber, captured);
        maxScore = std::max(maxScore, score);
        alpha = std::max(alpha, score);
        if (alpha >= beta) {
            break;
        }
    }
    
    return maxScore;
}

int Chess::evaluatePosition(int playerNumber)
{
    const int PAWN_VALUE = 100;
    const int KNIGHT_VALUE = 320;
    const int BISHOP_VALUE = 330;
    const int ROOK_VALUE = 500;
    const int QUEEN_VALUE = 900;
    
    int whiteMaterial = 0;
    whiteMaterial += countOnes(_whitePawns) * PAWN_VALUE;
    whiteMaterial += countOnes(_whiteKnights) * KNIGHT_VALUE;
    whiteMaterial += countOnes(_whiteBishops) * BISHOP_VALUE;
    whiteMaterial += countOnes(_whiteRooks) * ROOK_VALUE;
    whiteMaterial += countOnes(_whiteQueens) * QUEEN_VALUE;
    
    int blackMaterial = 0;
    blackMaterial += countOnes(_blackPawns) * PAWN_VALUE;
    blackMaterial += countOnes(_blackKnights) * KNIGHT_VALUE;
    blackMaterial += countOnes(_blackBishops) * BISHOP_VALUE;
    blackMaterial += countOnes(_blackRooks) * ROOK_VALUE;
    blackMaterial += countOnes(_blackQueens) * QUEEN_VALUE;
    
    int score = (playerNumber == 0) ? (whiteMaterial - blackMaterial) : (blackMaterial - whiteMaterial);
    
    //value center control more
    const uint64_t CENTER = 0x0000001818000000ULL;
    const uint64_t EXTENDED_CENTER = 0x00003C3C3C3C0000ULL;
    
    int whiteCenter = countOnes(((_whitePawns | _whiteKnights) & CENTER)) * 10;
    whiteCenter += countOnes(((_whitePawns | _whiteKnights) & EXTENDED_CENTER)) * 5;
    
    int blackCenter = countOnes(((_blackPawns | _blackKnights) & CENTER)) * 10;
    blackCenter += countOnes(((_blackPawns | _blackKnights) & EXTENDED_CENTER)) * 5;
    
    score += (playerNumber == 0) ? (whiteCenter - blackCenter) : (blackCenter - whiteCenter);
    

    //value pawn advancement more
    uint64_t whitePawns = _whitePawns;
    while (whitePawns) {
        int square = getFirstBit(whitePawns);
        int rank = square / 8;
        int advancement = rank * 5;
        score += (playerNumber == 0) ? advancement : -advancement;
        whitePawns &= whitePawns - 1;
    }
    
    uint64_t blackPawns = _blackPawns;
    while (blackPawns) {
        int square = getFirstBit(blackPawns);
        int rank = square / 8;
        int advancement = (7 - rank) * 5;
        score += (playerNumber == 1) ? advancement : -advancement;
        blackPawns &= blackPawns - 1;
    }
    
    return score;
}

bool Chess::canCastleKingside(int playerNumber) const
{
    if (playerNumber == 0) {
        if (_whiteKingMoved || _whiteKingsideRookMoved) return false;
        
        uint64_t occupied = getOccupiedSquares();
        if ((occupied & 0x0000000000000060ULL) != 0) return false;
        
        int enemy = 1;
        if (isSquareAttacked(4, enemy)) return false;
        if (isSquareAttacked(5, enemy)) return false;
        if (isSquareAttacked(6, enemy)) return false;
        
        return true;
    } else {
        if (_blackKingMoved || _blackKingsideRookMoved) return false;
        
        uint64_t occupied = getOccupiedSquares();
        if ((occupied & 0x6000000000000000ULL) != 0) return false;
        
        int enemy = 0;
        if (isSquareAttacked(60, enemy)) return false;
        if (isSquareAttacked(61, enemy)) return false;
        if (isSquareAttacked(62, enemy)) return false;
        
        return true;
    }
}

bool Chess::canCastleQueenside(int playerNumber) const
{
    if (playerNumber == 0) {
        if (_whiteKingMoved || _whiteQueensideRookMoved) return false;
        
        uint64_t occupied = getOccupiedSquares();
        if ((occupied & 0x000000000000000EULL) != 0) return false;
        
        int enemy = 1;
        if (isSquareAttacked(4, enemy)) return false;
        if (isSquareAttacked(3, enemy)) return false;
        if (isSquareAttacked(2, enemy)) return false;
        
        return true;
    } else {
        if (_blackKingMoved || _blackQueensideRookMoved) return false;
        
        uint64_t occupied = getOccupiedSquares();
        if ((occupied & 0x0E00000000000000ULL) != 0) return false;
        
        int enemy = 0;
        if (isSquareAttacked(60, enemy)) return false;
        if (isSquareAttacked(59, enemy)) return false;
        if (isSquareAttacked(58, enemy)) return false;
        
        return true;
    }
}

void Chess::performCastling(int kingToSquare, int kingFromSquare, int playerNumber)
{
    int rookFromSquare, rookToSquare;
    
    if (kingToSquare > kingFromSquare) {
        rookFromSquare = (playerNumber == 0) ? 7 : 63;
        rookToSquare = (playerNumber == 0) ? 5 : 61;
    } else {
        rookFromSquare = (playerNumber == 0) ? 0 : 56;
        rookToSquare = (playerNumber == 0) ? 3 : 59;
    }
    
    movePieceOnBitboard(rookFromSquare, rookToSquare, Rook, playerNumber);
    
    int rookFromX = rookFromSquare % 8;
    int rookFromY = rookFromSquare / 8;
    int rookToX = rookToSquare % 8;
    int rookToY = rookToSquare / 8;
    
    Bit* rook = _grid->getSquare(rookFromX, rookFromY)->bit();
    if (rook) {
        ChessSquare* rookFromHolder = _grid->getSquare(rookFromX, rookFromY);
        ChessSquare* rookToHolder = _grid->getSquare(rookToX, rookToY);
        
        rook->setParent(rookToHolder);
        rookFromHolder->draggedBitTo(rook, rookToHolder);
        rookToHolder->setBit(rook);
        rook->moveTo(rookToHolder->getPosition());
    }
}

void Chess::updateCastlingRights(int pieceType, int fromSquare, int playerNumber)
{
    if (pieceType == King) {
        if (playerNumber == 0) {
            _whiteKingMoved = true;
        } else {
            _blackKingMoved = true;
        }
    } else if (pieceType == Rook) {
        if (playerNumber == 0) {
            if (fromSquare == 0) _whiteQueensideRookMoved = true;  // a1
            if (fromSquare == 7) _whiteKingsideRookMoved = true;   // h1
        } else {
            if (fromSquare == 56) _blackQueensideRookMoved = true;
            if (fromSquare == 63) _blackKingsideRookMoved = true;
        }
    }
}

void Chess::handleRookCapture(int toSquare, int capturedType)
{
    if (capturedType == Rook) {
        if (toSquare == 0) _whiteQueensideRookMoved = true;
        if (toSquare == 7) _whiteKingsideRookMoved = true;
        if (toSquare == 56) _blackQueensideRookMoved = true;
        if (toSquare == 63) _blackKingsideRookMoved = true;
    }
}

void Chess::handlePromotion(int square, ChessPiece promotionPiece, int playerNumber)
{
    removePieceFromBitboard(square, Pawn, playerNumber);
    addPieceToBitboard(square, promotionPiece, playerNumber);
    
    int x = square % 8;
    int y = square / 8;
    ChessSquare* promotionSquare = _grid->getSquare(x, y);
    
    promotionSquare->destroyBit();
    Bit* promotedPiece = PieceForPlayer(playerNumber, promotionPiece);
    promotionSquare->setBit(promotedPiece);
    promotedPiece->setPosition(promotionSquare->getPosition());
}

void Chess::completePromotion(ChessPiece promotionPiece)
{
    if (_pendingPromotionSquare == -1) return;
    
    if (promotionPiece != Queen && promotionPiece != Rook && 
        promotionPiece != Bishop && promotionPiece != Knight) {
        promotionPiece = Queen;
    }
    
    handlePromotion(_pendingPromotionSquare, promotionPiece, _pendingPromotionPlayer);
    
    _pendingPromotionSquare = -1;
    _pendingPromotionPlayer = -1;
    _enPassantSquare = -1;
    
    endTurn();
    initializePawnMoves(getCurrentPlayer()->playerNumber());
}

bool Chess::isSquareAttacked(int square, int byPlayer) const
{
    if (square < 0 || square >= 64) return false;
    
    uint64_t occupied = getOccupiedSquares();
    uint64_t attackerPawns = getPieceBitboard(Pawn, byPlayer).getData();
    uint64_t attackerKnights = getPieceBitboard(Knight, byPlayer).getData();
    uint64_t attackerBishops = getPieceBitboard(Bishop, byPlayer).getData();
    uint64_t attackerRooks = getPieceBitboard(Rook, byPlayer).getData();
    uint64_t attackerQueens = getPieceBitboard(Queen, byPlayer).getData();
    uint64_t attackerKing = getPieceBitboard(King, byPlayer).getData();
    
    if (byPlayer == 0) {
        int leftPawnSquare = square - 9;
        int rightPawnSquare = square - 7;
        
        if (leftPawnSquare >= 0 && (square % 8) > 0) {
            if (attackerPawns & (1ULL << leftPawnSquare)) return true;
        }
        if (rightPawnSquare >= 0 && (square % 8) < 7) {
            if (attackerPawns & (1ULL << rightPawnSquare)) return true;
        }
    } else {
        int leftPawnSquare = square + 7; 
        int rightPawnSquare = square + 9;
        
        if (leftPawnSquare < 64 && (square % 8) > 0) {
            if (attackerPawns & (1ULL << leftPawnSquare)) return true;
        }
        if (rightPawnSquare < 64 && (square % 8) < 7) {
            if (attackerPawns & (1ULL << rightPawnSquare)) return true;
        }
    }
    
    if (KnightAttacks[square] & attackerKnights) {
        return true;
    }
    
    if (KingAttacks[square] & attackerKing) {
        return true;
    }
    
    uint64_t diagonalAttackers = attackerBishops | attackerQueens;
    if (getBishopAttacks(square, occupied) & diagonalAttackers) {
        return true;
    }
    
    uint64_t orthogonalAttackers = attackerRooks | attackerQueens;
    if (getRookAttacks(square, occupied) & orthogonalAttackers) {
        return true;
    }
    
    return false;
}

bool Chess::isKingInCheck(int playerNumber) const
{
    uint64_t kingBitboard = getPieceBitboard(King, playerNumber).getData();
    if (kingBitboard == 0) return false;
    int kingSquare = getFirstBit(kingBitboard);
    return isSquareAttacked(kingSquare, 1 - playerNumber);
}

Chess::CapturedPieceInfo Chess::makeMoveBitboard(const BitMove& move, int playerNumber)
{
    CapturedPieceInfo captured;
    ChessPiece movingPiece = static_cast<ChessPiece>(move.piece);
    
    captured.previousEnPassant = _enPassantSquare;
    captured.whiteKingMoved = _whiteKingMoved;
    captured.whiteKingsideRookMoved = _whiteKingsideRookMoved;
    captured.whiteQueensideRookMoved = _whiteQueensideRookMoved;
    captured.blackKingMoved = _blackKingMoved;
    captured.blackKingsideRookMoved = _blackKingsideRookMoved;
    captured.blackQueensideRookMoved = _blackQueensideRookMoved;
    
    // Check for capture at destination
    for (int piece = Pawn; piece <= King; piece++) {
        uint64_t enemyPieces = getPieceBitboard(static_cast<ChessPiece>(piece), 1 - playerNumber).getData();
        if (enemyPieces & (1ULL << move.to)) {
            captured.piece = static_cast<ChessPiece>(piece);
            captured.player = 1 - playerNumber;
            captured.square = move.to;
            captured.wasCapture = true;
            removePieceFromBitboard(move.to, captured.piece, captured.player);
            break;
        }
    }
    
    if (movingPiece == Pawn && move.to == _enPassantSquare && _enPassantSquare != -1) {
        int capturedPawnSquare = (playerNumber == 0) ? move.to - 8 : move.to + 8;
        
        if (!captured.wasCapture) {
            captured.piece = Pawn;
            captured.player = 1 - playerNumber;
            captured.square = capturedPawnSquare;
            captured.wasCapture = true;
            removePieceFromBitboard(capturedPawnSquare, Pawn, 1 - playerNumber);
        }
    }
    
    if (captured.wasCapture) {
        handleRookCapture(captured.square, captured.piece);
    }
    
    ChessPiece pieceToPlace = move.isPromotion() 
        ? static_cast<ChessPiece>(move.promotionPiece) 
        : movingPiece;
    
    removePieceFromBitboard(move.from, movingPiece, playerNumber);
    addPieceToBitboard(move.to, pieceToPlace, playerNumber);
    
    if (movingPiece == King && abs(move.to - move.from) == 2) {
        int rookFrom, rookTo;
        if (move.to > move.from) {
            rookFrom = (playerNumber == 0) ? 7 : 63;
            rookTo = (playerNumber == 0) ? 5 : 61;
        } else {
            rookFrom = (playerNumber == 0) ? 0 : 56;
            rookTo = (playerNumber == 0) ? 3 : 59;
        }
        movePieceOnBitboard(rookFrom, rookTo, Rook, playerNumber);
    }
    
    updateCastlingRights(movingPiece, move.from, playerNumber);
    
    if (movingPiece == Pawn && abs(move.to - move.from) == 16) {
        _enPassantSquare = (playerNumber == 0) ? move.from + 8 : move.from - 8;
    } else {
        _enPassantSquare = -1;
    }
    
    return captured;
}

void Chess::unmakeMoveBitboard(const BitMove& move, int playerNumber, const CapturedPieceInfo& captured)
{
    ChessPiece movingPiece = static_cast<ChessPiece>(move.piece);
    
    if (movingPiece == King && abs(move.to - move.from) == 2) {
        int rookFrom, rookTo;
        if (move.to > move.from) {
            rookFrom = (playerNumber == 0) ? 7 : 63;
            rookTo = (playerNumber == 0) ? 5 : 61;
        } else {
            rookFrom = (playerNumber == 0) ? 0 : 56;
            rookTo = (playerNumber == 0) ? 3 : 59;
        }
        movePieceOnBitboard(rookTo, rookFrom, Rook, playerNumber);
    }
    
    ChessPiece pieceAtDestination = move.isPromotion() 
        ? static_cast<ChessPiece>(move.promotionPiece) 
        : movingPiece;
    removePieceFromBitboard(move.to, pieceAtDestination, playerNumber);
    addPieceToBitboard(move.from, movingPiece, playerNumber);
    
    if (captured.wasCapture) {
        addPieceToBitboard(captured.square, captured.piece, captured.player);
    }
    
    _enPassantSquare = captured.previousEnPassant;
    _whiteKingMoved = captured.whiteKingMoved;
    _whiteKingsideRookMoved = captured.whiteKingsideRookMoved;
    _whiteQueensideRookMoved = captured.whiteQueensideRookMoved;
    _blackKingMoved = captured.blackKingMoved;
    _blackKingsideRookMoved = captured.blackKingsideRookMoved;
    _blackQueensideRookMoved = captured.blackQueensideRookMoved;
}

bool Chess::isMoveLegal(const BitMove& move, int playerNumber)
{
    CapturedPieceInfo captured = makeMoveBitboard(move, playerNumber);
    bool legal = !isKingInCheck(playerNumber);
    unmakeMoveBitboard(move, playerNumber, captured);
    return legal;
}

std::vector<BitMove> Chess::filterLegalMoves(const std::vector<BitMove>& moves, int playerNumber)
{
    std::vector<BitMove> legalMoves;
    legalMoves.reserve(moves.size());
    
    for (const BitMove& move : moves) {
        if (isMoveLegal(move, playerNumber)) {
            legalMoves.push_back(move);
        }
    }
    
    return legalMoves;
}

void Chess::stopGame()
{
    _grid->forEachSquare([](ChessSquare* square, int x, int y) {
        square->destroyBit();
    });
    
    _whiteKingMoved = false;
    _whiteKingsideRookMoved = false;
    _whiteQueensideRookMoved = false;
    _blackKingMoved = false;
    _blackKingsideRookMoved = false;
    _blackQueensideRookMoved = false;
    
    _enPassantSquare = -1;
    _pendingPromotionSquare = -1;
    _pendingPromotionPlayer = -1;
    
    _aiMovedThisTurn = false;
    _lastAIPlayer = -1;
}

Player* Chess::ownerAt(int x, int y) const
{
    if (x < 0 || x >= 8 || y < 0 || y >= 8) return nullptr;
    auto square = _grid->getSquare(x, y);
    if (!square || !square->bit()) return nullptr;
    
    return square->bit()->getOwner();
}

Player* Chess::checkForWinner()
{
    int currentPlayer = getCurrentPlayer()->playerNumber();
    std::vector<BitMove> legalMoves = generateAllLegalMoves(currentPlayer);
    
    if (legalMoves.empty() && isKingInCheck(currentPlayer)) {
        return getPlayerAt(1 - currentPlayer);
    }
    
    return nullptr;
}

bool Chess::checkForDraw()
{
    int currentPlayer = getCurrentPlayer()->playerNumber();
    std::vector<BitMove> legalMoves = generateAllLegalMoves(currentPlayer);
    
    if (legalMoves.empty() && !isKingInCheck(currentPlayer)) {
        return true;
    }
    
    int whitePieceCount = countOnes(getPlayerPieces(0));
    int blackPieceCount = countOnes(getPlayerPieces(1));
    
    if (whitePieceCount == 1 && blackPieceCount == 1) {
        return true;
    }
    
    return false;
}

char Chess::pieceNotation(int x, int y) const
{
    const char* wpieces = "0PNBRQK";
    const char* bpieces = "0pnbrqk";
    
    Bit* bit = _grid->getSquare(x, y)->bit();
    if (!bit) return '0';
    
    int tag = bit->gameTag();
    return (tag < 128) ? wpieces[tag] : bpieces[tag - 128];
}

std::string Chess::initialStateString()
{
    return stateString();
}

std::string Chess::stateString()
{
    std::string s;
    s.reserve(64);
    
    _grid->forEachSquare([&](ChessSquare* square, int x, int y) {
        s += pieceNotation(x, y);
    });
    
    return s;
}

void Chess::setStateString(const std::string &s)
{
    _grid->forEachSquare([&](ChessSquare* square, int x, int y) {
        int index = y * 8 + x;
        char playerNumber = s[index] - '0';
        
        if (playerNumber) {
            square->setBit(PieceForPlayer(playerNumber - 1, Pawn));
        } else {
            square->setBit(nullptr);
        }
    });
}
