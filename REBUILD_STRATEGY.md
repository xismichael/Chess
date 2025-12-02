# Chess Engine Rebuild Strategy
**Goal: Maximize AI speed using GameState while preserving UI compatibility**

---

## Architecture Overview

```
┌─────────────────────────────────────────────────┐
│               Chess.cpp (UI Layer)              │
│  - Drag & Drop handling                         │
│  - Visual piece rendering                       │
│  - Game framework interface                     │
│  - Promotion UI                                 │
└─────────────────┬───────────────────────────────┘
                  │ Bridge Layer
                  ↓
┌─────────────────────────────────────────────────┐
│         GameState (Engine Core)                 │
│  - Unified bitboard array                       │
│  - State stack (fast undo)                      │
│  - Optimized move generation                    │
│  - Precomputed attack tables                    │
│  - Legal move filtering                         │
└─────────────────────────────────────────────────┘
```

---

## Key Changes

### 1. **Replace Individual Bitboards with GameState**

**OLD (Chess.cpp):**
```cpp
uint64_t _whitePawns, _blackPawns, _whiteKnights, ...;
```

**NEW:**
```cpp
GameState _engine;  // Contains _bitboards[NUM_BITBOARDS]
```

**How it works:**
- GameState maintains all bitboards internally
- We sync between GameState's internal state and visual board
- No duplicate storage!

---

### 2. **Move Generation: Use GameState**

**OLD (Chess.cpp):**
```cpp
std::vector<BitMove> generatePseudoLegalMoves(int playerNumber) {
    // 300 lines of move generation code
}
```

**NEW:**
```cpp
std::vector<BitMove> generateLegalMoves(int playerNumber) {
    _engine.color = (playerNumber == 0) ? WHITE : BLACK;
    return _engine.generateAllMoves();  // Already optimized!
}
```

**Speedup: ~2-3x** (GameState has precomputed attacks + better algorithms)

---

### 3. **Make/Unmake: Use State Stack**

**OLD (Chess.cpp):**
```cpp
CapturedPieceInfo makeMoveBitboard(const BitMove& move, int playerNumber) {
    // Save captured piece info
    // Update bitboards
    // Update castling rights
    // Update en passant
    // ... 50 lines
}

void unmakeMoveBitboard(..., const CapturedPieceInfo& captured) {
    // Restore everything manually
    // ... 40 lines
}
```

**NEW:**
```cpp
void makeMove(const BitMove& move) {
    _engine.pushMove(move);  // O(1) - just pushes state to stack
}

void unmakeMove() {
    _engine.popState();  // O(1) - just pops from stack
}
```

**Speedup: ~5-10x** (No complex restoration logic!)

---

### 4. **Add Transposition Table**

**OLD (Chess.cpp):**
```cpp
int negamax(int depth, int alpha, int beta, int playerNumber) {
    // No caching - always evaluates every position
}
```

**NEW:**
```cpp
int negamaxWithTT(int depth, int alpha, int beta, int playerNumber) {
    uint64_t hash = _engine._zobristHash[playerNumber];
    
    // 1. Check if we've seen this position
    TTEntry* entry = probeTT(hash);
    if (entry && entry->depth >= depth) {
        return entry->score;  // Hit! Skip entire subtree
    }
    
    // 2. Search
    int score = searchLogic();
    
    // 3. Store result
    storeTT(hash, score, depth, nodeType, bestMove);
    return score;
}
```

**Speedup: ~10-50x** (Massive! Can skip entire search branches)

---

## Implementation Steps

### **Step 1: Constructor & Initialization** (30 min)

```cpp
Chess::Chess() {
    _grid = new Grid(8, 8);
    
    // Initialize GameState engine
    char initialState[64];
    // Convert standard position to char array
    _engine.init(initialState, WHITE);
    
    // Initialize transposition table
    _transpositionTable = new TTEntry[TT_SIZE];
    clearTT();
    
    // Initialize Zobrist (one-time)
    if (!_zobristInitialized) {
        initZobrist();
    }
    
    _pendingPromotionSquare = -1;
    _pendingPromotionPlayer = -1;
    _aiMovedThisTurn = false;
    _lastAIPlayer = -1;
}
```

---

### **Step 2: Bridge Functions** (1 hour)

```cpp
// Sync GameState → Visual Board
void Chess::syncEngineToVisual() {
    for (int i = 0; i < 64; i++) {
        char piece = _engine.state[i];
        int x = i % 8;
        int y = i / 8;
        ChessSquare* square = _grid->getSquare(x, y);
        
        if (piece == '0') {
            square->setBit(nullptr);
        } else {
            // Convert char to visual piece
            square->setBit(charToPiece(piece));
        }
    }
}

// Sync Visual Board → GameState
void Chess::syncVisualToEngine() {
    for (int i = 0; i < 64; i++) {
        int x = i % 8;
        int y = i / 8;
        ChessSquare* square = _grid->getSquare(x, y);
        
        if (square->bit()) {
            _engine.state[i] = pieceToChar(square->bit());
        } else {
            _engine.state[i] = '0';
        }
    }
}
```

---

### **Step 3: UI Functions** (1 hour)

```cpp
void Chess::bitMovedFromTo(Bit &bit, BitHolder &src, BitHolder &dst) {
    ChessSquare* srcSquare = dynamic_cast<ChessSquare*>(&src);
    ChessSquare* dstSquare = dynamic_cast<ChessSquare*>(&dst);
    
    // 1. Convert UI move to BitMove
    int fromSquare = srcSquare->getSquareIndex();
    int toSquare = dstSquare->getSquareIndex();
    BitMove move = convertUIMoveToBitMove(fromSquare, toSquare, pieceType);
    
    // 2. Execute on engine
    _engine.pushMove(move);
    
    // 3. Update visual (already done by Game framework drag-drop)
    bit.setParent(dstSquare);
    srcSquare->setBit(nullptr);
    dstSquare->setBit(&bit);
    bit.moveTo(dstSquare->getPosition());
    
    // 4. Handle promotion
    if (needsPromotion) {
        _pendingPromotionSquare = toSquare;
        _pendingPromotionPlayer = playerNumber;
        return;
    }
    
    endTurn();
}
```

---

### **Step 4: AI with Transposition Table** (2 hours)

```cpp
void Chess::updateAI() {
    int currentPlayer = getCurrentPlayer()->playerNumber();
    
    if (_aiMovedThisTurn && _lastAIPlayer == currentPlayer) {
        return;
    }
    
    _aiMovedThisTurn = true;
    _lastAIPlayer = currentPlayer;
    
    // Search with TT (depth 8-10 now possible!)
    BitMove bestMove = findBestMove(8);
    
    // Execute visually
    executeVisualMove(bestMove);
    
    _aiMovedThisTurn = false;
}

int Chess::negamaxWithTT(int depth, int alpha, int beta, int playerNumber) {
    // Base case
    if (depth == 0) {
        return evaluatePosition(playerNumber);
    }
    
    // Probe TT
    uint64_t hash = _engine._zobristHash[playerNumber];
    TTEntry* entry = probeTT(hash);
    if (entry && entry->depth >= depth) {
        if (entry->nodeType == 0) return entry->score;  // EXACT
        if (entry->nodeType == 1) alpha = max(alpha, entry->score);  // LOWER
        if (entry->nodeType == 2) beta = min(beta, entry->score);    // UPPER
        if (alpha >= beta) return entry->score;
    }
    
    // Generate moves
    _engine.color = (playerNumber == 0) ? WHITE : BLACK;
    std::vector<BitMove> moves = _engine.generateAllMoves();
    
    if (moves.empty()) {
        // Check for checkmate/stalemate
        return isInCheck ? -999999 : 0;
    }
    
    // Search moves
    int maxScore = -999999;
    BitMove bestMove = moves[0];
    
    for (const BitMove& move : moves) {
        _engine.pushMove(move);
        int score = -negamaxWithTT(depth - 1, -beta, -alpha, 1 - playerNumber);
        _engine.popState();
        
        if (score > maxScore) {
            maxScore = score;
            bestMove = move;
        }
        
        alpha = max(alpha, score);
        if (alpha >= beta) break;  // Cutoff
    }
    
    // Store in TT
    uint8_t nodeType = (maxScore <= alpha) ? 2 : (maxScore >= beta) ? 1 : 0;
    storeTT(hash, maxScore, depth, nodeType, bestMove);
    
    return maxScore;
}
```

---

### **Step 5: Evaluation** (30 min)

```cpp
int Chess::evaluatePosition(int playerNumber) const {
    return materialScore(playerNumber) + 
           positionalScore(playerNumber);
}

int Chess::materialScore(int playerNumber) const {
    // Use GameState's bitboards directly
    int myPawns = countOnes(_engine._bitboards[playerNumber == 0 ? WHITE_PAWNS : BLACK_PAWNS].getData());
    int myKnights = countOnes(_engine._bitboards[playerNumber == 0 ? WHITE_KNIGHTS : BLACK_KNIGHTS].getData());
    // ... etc
    
    int oppPawns = countOnes(_engine._bitboards[playerNumber == 1 ? WHITE_PAWNS : BLACK_PAWNS].getData());
    // ... etc
    
    return (myPawns - oppPawns) * 100 + 
           (myKnights - oppKnights) * 320 + 
           // ... etc
}
```

---

## Expected Performance

| Metric | OLD (Current Chess.cpp) | NEW (GameState + TT) | Improvement |
|--------|------------------------|----------------------|-------------|
| **Move Generation** | ~1000 ns/pos | ~300 ns/pos | **3.3x faster** |
| **Make/Unmake** | ~500 ns | ~50 ns | **10x faster** |
| **Position Evaluation** | ~2000 ns | ~500 ns | **4x faster** |
| **Search Speed** | ~50k nodes/sec | ~500k-2M nodes/sec | **10-40x faster** |
| **Search Depth** | 6 ply | 10-12 ply | **+4-6 ply** |
| **Playing Strength** | ~1400 Elo | ~1800-2000 Elo | **+400-600 Elo** |

---

## Timeline

| Day | Task | Hours | Status |
|-----|------|-------|--------|
| 1 | Setup & Initialization | 2 | ⬜ |
| 1 | Bridge Functions | 2 | ⬜ |
| 2 | UI Integration | 4 | ⬜ |
| 2 | Move Validation | 2 | ⬜ |
| 3 | Zobrist Hashing | 2 | ⬜ |
| 3 | Transposition Table | 3 | ⬜ |
| 4 | AI Integration | 3 | ⬜ |
| 4 | Testing & Tuning | 3 | ⬜ |

**Total: ~21 hours over 4 days**

---

## Testing Strategy

1. **Unit Tests**: Move generation matches old implementation
2. **Perft Tests**: Count nodes at each depth (should match standard chess perft)
3. **AI vs AI**: New engine vs old engine (should win 80%+)
4. **UI Tests**: All drag-drop, promotion, castling works visually
5. **Performance Tests**: Measure nodes/second

---

## Rollback Plan

Keep both implementations side-by-side:
- `Chess.cpp` (old, stable)
- `ChessNew.cpp` (new, optimized)

Switch by changing `CMakeLists.txt`:
```cmake
# Old version
# add_executable(demo Chess.cpp ...)

# New version
add_executable(demo ChessNew.cpp ...)
```

---

## Next Steps

1. Review `GameState.cpp` - understand the API
2. Implement `ChessNew.cpp` constructor
3. Implement bridge functions
4. Test basic move execution
5. Integrate AI search
6. Add transposition table
7. Test & tune

**Ready to start? I can generate the full `ChessNew.cpp` implementation!**

