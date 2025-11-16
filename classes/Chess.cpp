#include "Chess.h"
#include <vector>

Chess::Chess()
{
    _grid = new Grid(8, 8);
    initializeBitboards();
    initializeKnightMoves();
    initializeKingMoves();
    initializePawnMoves(0);
    
    // Initialize pawn move bitboards (empty until first turn)
    for (int i = 0; i < 64; i++) {
        _pawnMovesBitboards[i] = BitboardElement(0ULL);
    }
}

Chess::~Chess()
{
    delete _grid;
}

char Chess::pieceNotation(int x, int y) const
{
    const char *wpieces = { "0PNBRQK" };
    const char *bpieces = { "0pnbrqk" };
    Bit *bit = _grid->getSquare(x, y)->bit();
    char notation = '0';
    if (bit) {
        notation = bit->gameTag() < 128 ? wpieces[bit->gameTag()] : bpieces[bit->gameTag()-128];
    }
    return notation;
}

Bit* Chess::PieceForPlayer(const int playerNumber, ChessPiece piece)
{
    const char* pieces[] = { "pawn.png", "knight.png", "bishop.png", "rook.png", "queen.png", "king.png" };

    Bit* bit = new Bit();
    // should possibly be cached from player class?
    const char* pieceName = pieces[piece - 1];
    std::string spritePath = std::string("") + (playerNumber == 0 ? "w_" : "b_") + pieceName;
    bit->LoadTextureFromFile(spritePath.c_str());
    bit->setOwner(getPlayerAt(playerNumber));
    bit->setSize(pieceSize, pieceSize);
    
    // Set the gameTag so canBitMoveFrom knows which player owns this piece
    bit->setGameTag(piece + (playerNumber * 128));

    return bit;
}

void Chess::setUpBoard()
{
    setNumberOfPlayers(2);
    _gameOptions.rowX = 8;
    _gameOptions.rowY = 8;

    _grid->initializeChessSquares(pieceSize, "boardsquare.png");

    FENtoBoard("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR");

    updateBitboards();
    initializePawnMoves(0);
    
    std::vector<BitMove> initialMoves = generateAllLegalMoves(0);
    //for the HW SS

    startGame();
}

void Chess::FENtoBoard(const std::string& fen) {
    // convert a FEN string to a board
    // FEN is a space delimited string with 6 fields
    // 1: piece placement (from white's perspective)

    int curIndex = 0;
    int curBoardIndex = 0;
    int playerNumber;
    ChessPiece curChessPiece;
    while (curBoardIndex < 64)
    {
        //never supposed to be here
        if (fen[curIndex] == ' ')
        {
            std::cout << "error in FEN string" << std::endl;
            return;
        }
        //skip line indexs
        if (fen[curIndex] == '/')
        {
            curIndex++;
            continue;
        }
        //48 is 0, 56 is 8
        if (fen[curIndex] > 48 && fen[curIndex] <= 56)
        {
            curBoardIndex += fen[curIndex] - 48;
            curIndex++;
            continue;
        }

        
        switch (fen[curIndex])
        {
            case 'p':
                playerNumber = 1;
                curChessPiece = Pawn;
                break;
            case 'n':
                playerNumber = 1;
                curChessPiece = Knight;
                break;
            case 'b':
                playerNumber = 1;
                curChessPiece = Bishop;
                break;

            case 'r':
                playerNumber = 1;
                curChessPiece = Rook;
                break;

            case 'q':
                playerNumber = 1;
                curChessPiece = Queen;
                break;
            case 'k':
                playerNumber = 1;
                curChessPiece = King;
                break;
            case 'P':
                playerNumber = 0;
                curChessPiece = Pawn;
                break;
            case 'N':
                playerNumber = 0;
                curChessPiece = Knight;
                break;
            case 'B':
                playerNumber = 0;
                curChessPiece = Bishop;
                break;

            case 'R':
                playerNumber = 0;
                curChessPiece = Rook;
                break;

            case 'Q':
                playerNumber = 0;
                curChessPiece = Queen;
                break;
            case 'K':
                playerNumber = 0;
                curChessPiece = King;
                break;
            default:
            //for invalid
                playerNumber = -1;
                break;
        }

        if (playerNumber < 0)
        {
            std::cout << "FEN string contains unknown letter" << std::endl;
            return;
        }

        int x_i = curBoardIndex % 8;
        int y_i =  7 - curBoardIndex / 8;

        _grid->getSquare(x_i, y_i)->dropBitAtPoint(
            PieceForPlayer(playerNumber, curChessPiece),
            { (float)x_i, (float) y_i}
        );
        curIndex++;
        curBoardIndex++;
    }



    // NOT PART OF THIS ASSIGNMENT BUT OTHER THINGS THAT CAN BE IN A FEN STRING
    // ARE BELOW3
    // 2: active color (W or B)
    // 3: castling availability (KQkq or -)
    // 4: en passant target square (in algebraic notation, or -)
    // 5: halfmove clock (number of halfmoves since the last capture or pawn advance)
}

bool Chess::actionForEmptyHolder(BitHolder &holder)
{
    return false;
}

bool Chess::canBitMoveFrom(Bit &bit, BitHolder &src)
{
    // need to implement friendly/unfriendly in bit so for now this hack
    int currentPlayer = getCurrentPlayer()->playerNumber() * 128;
    int pieceColor = bit.gameTag() & 128;
    if (pieceColor == currentPlayer) return true;
    return false;
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

    
    // Validate move based on piece type
    switch (pieceType) {
        case Pawn: {
            uint64_t validMoves = _pawnMovesBitboards[srcSquareIndex].getData();
            return (validMoves & (1ULL << dstSquareIndex)) != 0;
        }
            
        case Knight: {
            uint64_t validMoves = _knightBitboards[srcSquareIndex].getData();
            return (validMoves & (1ULL << dstSquareIndex)) != 0;
        }
        
        case Bishop:
            return true;
            
        case Rook:
            return true;
            
        case Queen:
            return true;
            
        case King: {
            uint64_t validKingMoves = _kingBitboards[srcSquareIndex].getData();
            return (validKingMoves & (1ULL << dstSquareIndex)) != 0;
        }
            
        default:
            return false;
    }
}

void Chess::bitMovedFromTo(Bit &bit, BitHolder &src, BitHolder &dst)
{
    // Update bitboards when a piece moves
    ChessSquare* srcSquare = dynamic_cast<ChessSquare*>(&src);
    ChessSquare* dstSquare = dynamic_cast<ChessSquare*>(&dst);
    
    if (srcSquare && dstSquare) {
        int fromSquare = srcSquare->getSquareIndex();
        int toSquare = dstSquare->getSquareIndex();
        int gameTag = bit.gameTag();
        int pieceType = gameTag % 128;
        int playerNumber = (gameTag & 128) ? 1 : 0;
        
        // remove the captured piece from bitboards
        if (dstSquare->bit()) {
            int capturedTag = dstSquare->bit()->gameTag();
            int capturedType = capturedTag % 128;
            int capturedPlayer = (capturedTag & 128) ? 1 : 0;
            removePieceFromBitboard(toSquare, static_cast<ChessPiece>(capturedType), capturedPlayer);
        }
        
        // Move the piece on bitboards
        movePieceOnBitboard(fromSquare, toSquare, static_cast<ChessPiece>(pieceType), playerNumber);
    }
    
    endTurn();
    
    // generate next player's pawn moves
    int nextPlayer = getCurrentPlayer()->playerNumber();
    initializePawnMoves(nextPlayer);
}

// ========== BITBOARD MANAGEMENT ==========

void Chess::initializeBitboards()
{
    //white
    _whitePawns = 0ULL;
    _whiteKnights = 0ULL;
    _whiteBishops = 0ULL;
    _whiteRooks = 0ULL;
    _whiteQueens = 0ULL;
    _whiteKings = 0ULL;

    //black
    _blackPawns = 0ULL;
    _blackKnights = 0ULL;
    _blackBishops = 0ULL;
    _blackRooks = 0ULL;
    _blackQueens = 0ULL;
    _blackKings = 0ULL;
}

void Chess::updateBitboards()
{
    // Clear all bitboards
    initializeBitboards();
    
    // Scan the grid and populate bitboards
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

// ========== BITBOARD QUERIES ==========

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
            default: return BitboardElement(0ULL);
        }
    } else {
        switch (piece) {
            case Pawn:   return BitboardElement(_blackPawns);
            case Knight: return BitboardElement(_blackKnights);
            case Bishop: return BitboardElement(_blackBishops);
            case Rook:   return BitboardElement(_blackRooks);
            case Queen:  return BitboardElement(_blackQueens);
            case King:   return BitboardElement(_blackKings);
            default: return BitboardElement(0ULL);
        }
    }
}

// ========== KNIGHT MOVE GENERATION ==========

void Chess::initializeKnightMoves()
{
    // Pre-compute all possible knight moves for each square
    int knightMoves[8][2] = {
        {-2, -1}, {-2, 1}, {-1, -2}, {-1, 2},
        {1, -2}, {1, 2}, {2, -1}, {2, 1}
    };
    
    for (int square = 0; square < 64; square++) {
        uint64_t moves = 0ULL;
        int file = square % 8;
        int rank = square / 8;
        
        for (int i = 0; i < 8; i++) {
            int newFile = file + knightMoves[i][0];
            int newRank = rank + knightMoves[i][1];
            
            // Check if the new position is on the board
            if (newFile >= 0 && newFile < 8 && newRank >= 0 && newRank < 8) {
                int targetSquare = newRank * 8 + newFile;
                moves |= (1ULL << targetSquare);
            }
        }
        
        _knightBitboards[square] = BitboardElement(moves);
    }
}

void Chess::initializeKingMoves()
{
    // Pre-compute all possible king moves for each square
    // King moves: 8 possible directions (one square in any direction)
    int kingMoves[8][2] = {
        {-1, -1}, {-1, 0}, {-1, 1},
        { 0, -1}, { 0, 1},
        { 1, -1}, { 1, 0}, { 1, 1}
    };
    
    for (int square = 0; square < 64; square++) {
        uint64_t moves = 0ULL;
        int file = square % 8;
        int rank = square / 8;
        
        for (int i = 0; i < 8; i++) {
            int newFile = file + kingMoves[i][0];
            int newRank = rank + kingMoves[i][1];
            
            if (newFile >= 0 && newFile < 8 && newRank >= 0 && newRank < 8) {
                int targetSquare = newRank * 8 + newFile;
                moves |= (1ULL << targetSquare);
            }
        }
        
        _kingBitboards[square] = BitboardElement(moves);
    }
}


uint64_t Chess::generatePawnMoves(int square, int playerNumber) const
{
    uint64_t moves = 0ULL;
    int rank = square / 8;
    int file = square % 8;
    
    // Direction depends on player
    int direction = (playerNumber == 0) ? 1 : -1;
    int startRank = (playerNumber == 0) ? 1 : 6;
    

    int forwardSquare = square + (direction * 8);
    if (forwardSquare >= 0 && forwardSquare < 64) {
        //Can only move forward to empty square
        int forwardX = forwardSquare % 8;
        int forwardY = forwardSquare / 8;
        if (!_grid->getSquare(forwardX, forwardY)->bit()) {
            moves |= (1ULL << forwardSquare);
            
            // Double square forward from starting position
            if (rank == startRank) {
                int doubleForwardSquare = square + (direction * 16);
                int doubleX = doubleForwardSquare % 8;
                int doubleY = doubleForwardSquare / 8;
                if (!_grid->getSquare(doubleX, doubleY)->bit()) {
                    moves |= (1ULL << doubleForwardSquare);
                }
            }
        }
    }
    
    //left captures
    if (file > 0) {
        int leftCapture = square + (direction * 8) - 1;
        if (leftCapture >= 0 && leftCapture < 64) {
            int captureX = leftCapture % 8;
            int captureY = leftCapture / 8;
            Bit* targetBit = _grid->getSquare(captureX, captureY)->bit();
            if (targetBit) {
                int targetPlayer = (targetBit->gameTag() & 128) ? 1 : 0;
                if (targetPlayer != playerNumber) {
                    moves |= (1ULL << leftCapture);
                }
            }
        }
    }
    
    //rihgt captures
    if (file < 7) {
        int rightCapture = square + (direction * 8) + 1;
        if (rightCapture >= 0 && rightCapture < 64) {
            int captureX = rightCapture % 8;
            int captureY = rightCapture / 8;
            Bit* targetBit = _grid->getSquare(captureX, captureY)->bit();
            if (targetBit) {
                int targetPlayer = (targetBit->gameTag() & 128) ? 1 : 0;
                if (targetPlayer != playerNumber) {
                    moves |= (1ULL << rightCapture);
                }
            }
        }
    }
    
    return moves;
}

void Chess::initializePawnMoves(int playerNumber)
{
    // Clear all pawn move bitboards
    for (int i = 0; i < 64; i++) {
        _pawnMovesBitboards[i] = BitboardElement(0ULL);
    }
    
    // Generate moves for all pawns of this player
    BitboardElement pawns = getPieceBitboard(Pawn, playerNumber);
    pawns.forEachBit([&](int square) {
        _pawnMovesBitboards[square] = BitboardElement(generatePawnMoves(square, playerNumber));
    });
}


//generate all legal moves for the current player
std::vector<BitMove> Chess::generateAllLegalMoves(int playerNumber)
{
    std::vector<BitMove> moves;
    
    uint64_t enemyPieces = getPlayerPieces(1 - playerNumber);
    uint64_t emptySquares = getEmptySquares();
    
    // Generate pawn moves
    BitboardElement pawns = getPieceBitboard(Pawn, playerNumber);
    pawns.forEachBit([&](int fromSquare) {
        BitboardElement validMoves(_pawnMovesBitboards[fromSquare].getData());
        validMoves.forEachBit([&](int toSquare) {
            moves.emplace_back(fromSquare, toSquare, Pawn);
        });
    });
    
    // Generate knight moves
    BitboardElement knights = getPieceBitboard(Knight, playerNumber);
    knights.forEachBit([&](int fromSquare) {
        uint64_t knightTargets = (_knightBitboards[fromSquare].getData()) & (emptySquares | enemyPieces);
        BitboardElement validMoves(knightTargets);
        validMoves.forEachBit([&](int toSquare) {
            moves.emplace_back(fromSquare, toSquare, Knight);
        });
    });
    
    // Generate king moves
    BitboardElement kings = getPieceBitboard(King, playerNumber);
    kings.forEachBit([&](int fromSquare) {
        uint64_t kingTargets = (_kingBitboards[fromSquare].getData()) & (emptySquares | enemyPieces);
        BitboardElement validMoves(kingTargets);
        validMoves.forEachBit([&](int toSquare) {
            moves.emplace_back(fromSquare, toSquare, King);
        });
    });
    

    //Bishop
    //Rook
    //Queen
    
    return moves;
}

void Chess::stopGame()
{
    _grid->forEachSquare([](ChessSquare* square, int x, int y) {
        square->destroyBit();
    });
}

Player* Chess::ownerAt(int x, int y) const
{
    if (x < 0 || x >= 8 || y < 0 || y >= 8) {
        return nullptr;
    }

    auto square = _grid->getSquare(x, y);
    if (!square || !square->bit()) {
        return nullptr;
    }
    return square->bit()->getOwner();
}

Player* Chess::checkForWinner()
{
    return nullptr;
}

bool Chess::checkForDraw()
{
    return false;
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
            s += pieceNotation( x, y );
        }
    );
    return s;}

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
