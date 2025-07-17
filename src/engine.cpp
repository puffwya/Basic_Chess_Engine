#include "engine.h"
#include "main.h"
#include <vector>
#include <cstdlib>

extern int pendingPromotionSquare; 

// Simple piece values
const int pieceValues[13] = {
    0,   // Empty
    100, // White Pawn
    100, // Black Pawn
    320, // White Knight
    320, // Black Knight
    330, // White Bishop
    330, // Black Bishop
    500, // White Rook
    500, // Black Rook
    900, // White Queen
    900, // Black Queen
    20000, // White King
    20000  // Black King
};

// PST arrays

const int pawnPST[64] = {
   0,  0,  0,  0,  0,  0,  0,  0,
  50, 50, 50, 50, 50, 50, 50, 50,
  10, 10, 20, 30, 30, 20, 10, 10,
   5,  5, 10, 25, 25, 10,  5,  5,
   0,  0,  0, 20, 20,  0,  0,  0,
   5, -5,-10,  0,  0,-10, -5,  5,
   5, 10, 10,-20,-20, 10, 10,  5,
   0,  0,  0,  0,  0,  0,  0,  0
};

const int knightPST[64] = {
 -50,-40,-30,-30,-30,-30,-40,-50,
 -40,-20,  0,  5,  5,  0,-20,-40,
 -30,  5, 10, 15, 15, 10,  5,-30,
 -30,  0, 15, 20, 20, 15,  0,-30,
 -30,  5, 15, 20, 20, 15,  5,-30,
 -30,  0, 10, 15, 15, 10,  0,-30,
 -40,-20,  0,  0,  0,  0,-20,-40,
 -50,-40,-30,-30,-30,-30,-40,-50
};

const int bishopPST[64] = {
 -20,-10,-10,-10,-10,-10,-10,-20,
 -10,  5,  0,  0,  0,  0,  5,-10,
 -10, 10, 10, 10, 10, 10, 10,-10,
 -10,  0, 10, 10, 10, 10,  0,-10,
 -10,  5,  5, 10, 10,  5,  5,-10,
 -10,  0,  5, 10, 10,  5,  0,-10,
 -10,  0,  0,  0,  0,  0,  0,-10,
 -20,-10,-10,-10,-10,-10,-10,-20
};

const int rookPST[64] = {
   0,  0,  0,  0,  0,  0,  0,  0,
   5, 10, 10, 10, 10, 10, 10,  5,
  -5,  0,  0,  0,  0,  0,  0, -5,
  -5,  0,  0,  0,  0,  0,  0, -5,
  -5,  0,  0,  0,  0,  0,  0, -5,
  -5,  0,  0,  0,  0,  0,  0, -5,
  -5,  0,  0,  0,  0,  0,  0, -5,
   0,  0,  0,  5,  5,  0,  0,  0
};

const int queenPST[64] = {
 -20,-10,-10, -5, -5,-10,-10,-20,
 -10,  0,  0,  0,  0,  0,  0,-10,
 -10,  0,  5,  5,  5,  5,  0,-10,
  -5,  0,  5,  5,  5,  5,  0, -5,
   0,  0,  5,  5,  5,  5,  0, -5,
 -10,  5,  5,  5,  5,  5,  0,-10,
 -10,  0,  5,  0,  0,  0,  0,-10,
 -20,-10,-10, -5, -5,-10,-10,-20
};

const int kingPST[64] = {
 -30,-40,-40,-50,-50,-40,-40,-30,
 -30,-40,-40,-50,-50,-40,-40,-30,
 -30,-40,-40,-50,-50,-40,-40,-30,
 -30,-40,-40,-50,-50,-40,-40,-30,
 -20,-30,-30,-40,-40,-30,-30,-20,
 -10,-20,-20,-20,-20,-20,-20,-10,
  20, 20,  0,  0,  0,  0, 20, 20,
  20, 30, 10,  0,  0, 10, 30, 20
};

// Mirror vertically for black pieces (flip ranks)
inline int mirrorIndex(int idx) {
    int rank = idx / 8;
    int file = idx % 8;
    int mirroredRank = 7 - rank;
    return mirroredRank * 8 + file;
}

int pstScoreForPiece(int piece, int square) {
    if (piece == 0) return 0;
    bool isWhite = (piece % 2 == 1);
    int pstIndex = isWhite ? mirrorIndex(square) : square;

    switch (piece) {
        case 1:  return pawnPST[pstIndex];
        case 2:  return -pawnPST[mirrorIndex(pstIndex)];
        case 3:  return knightPST[pstIndex];
        case 4:  return -knightPST[mirrorIndex(pstIndex)];
        case 5:  return bishopPST[pstIndex];
        case 6:  return -bishopPST[mirrorIndex(pstIndex)];
        case 7:  return rookPST[pstIndex];
        case 8:  return -rookPST[mirrorIndex(pstIndex)];
        case 9:  return queenPST[pstIndex];
        case 10: return -queenPST[mirrorIndex(pstIndex)];
        case 11: return kingPST[pstIndex];
        case 12: return -kingPST[mirrorIndex(pstIndex)];
        default: return 0;
    }
}

int evaluateBoard() {
    int score = 0;
    for (int i = 0; i < 64; ++i) {
        int piece = board[i];
        if (piece == 0) continue;

        int pieceValue = pieceValues[piece];
        int pstValue = pstScoreForPiece(piece, i);

        bool isWhite = (piece % 2 == 1);
        if (isWhite) {
            score += pieceValue + pstValue;
        } else {
            score -= pieceValue + pstValue;
        }
    }
    return score;
}

int minimax(int depth, int alpha, int beta, bool maximizingPlayer) {
    if (depth == 0) {
        return evaluateBoard();
    }
  
    int bestScore = maximizingPlayer ? -1000000 : 1000000;
    bool moveFound = false;

    struct Move {
        int from;
        int to;
        int score;
    };
    std::vector<Move> moves;

    // Generate moves with scores for ordering
    for (int from = 0; from < 64; ++from) {
        if (board[from] == 0 || (board[from] % 2 == 1) != maximizingPlayer) continue;

        for (int to = 0; to < 64; ++to) {
            if (!isValidMove(from, to)) continue;
            if (wouldKingBeInCheckAfterMove(from, to)) continue;

            int moveScore = 0;
            if (board[to] != 0) {
                // Capture: victim value - attacker value (higher better)
                moveScore = pieceValues[board[to]] - pieceValues[board[from]];
            } else {
                // Non-capture: use PST difference
                int piece = board[from];
                int pstFrom = pstScoreForPiece(piece, from);
                int pstTo = pstScoreForPiece(piece, to);
                moveScore = pstTo - pstFrom;
            }

            moves.push_back({from, to, moveScore});
        }
    }

    // Sort moves descending by score for better pruning
    std::sort(moves.begin(), moves.end(), [](const Move& a, const Move& b) {
        return a.score > b.score;
    });

    // Search moves in order
    for (const Move& move : moves) {
        moveFound = true;

        // Save board state
        uint8_t backupTo = board[move.to];
        board[move.to] = board[move.from];
        board[move.from] = 0;

        int score = minimax(depth - 1, alpha, beta, !maximizingPlayer);

        // Undo move
        board[move.from] = board[move.to];
        board[move.to] = backupTo;

        if (maximizingPlayer) {
            bestScore = std::max(bestScore, score);
            alpha = std::max(alpha, score);
        } else {
            bestScore = std::min(bestScore, score);
            beta = std::min(beta, score);
        }

        if (beta <= alpha) break;
    }

    if (!moveFound) {
        bool inCheck = isInCheck(maximizingPlayer);
        if (inCheck) {
            return maximizingPlayer ? -1000000 : 1000000;
        } else {
            return 0;
        }
    }

    return bestScore;
}

int findBestMove(bool white, int depth) {
    int bestScore = white ? -1000000 : 1000000;
    int bestMove = -1;

    // Store the first legal move as fallback
    int fallbackMove = -1;

    for (int from = 0; from < 64; ++from) {
        if (board[from] == 0 || (board[from] % 2 == 1) != white) continue;

        for (int to = 0; to < 64; ++to) {
            if (!isValidMove(from, to)) continue;
            if (wouldKingBeInCheckAfterMove(from, to)) continue;

            // Save first legal move in case all scores are bad
            if (fallbackMove == -1) fallbackMove = from * 64 + to;

            // Save board state
            uint8_t backupTo = board[to];
            board[to] = board[from];
            board[from] = 0;

            int score = minimax(depth, -1000000, 1000000, !white);

            // Undo move
            board[from] = board[to];
            board[to] = backupTo;

            if ((white && score > bestScore) || (!white && score < bestScore)) {
                bestScore = score;
                bestMove = from * 64 + to;
            }
        }
    }

    // If no good move was found, fall back to a legal one
    if (bestMove == -1) bestMove = fallbackMove;

    return bestMove;
}

extern "C" {

    bool makeAIMove() {
        pendingPromotionSquare = -1;  // Clear any leftover promotion state

        int move = findBestMove(false, 4); // false = black
        if (move == -1) return false;

        int from = move / 64;
        int to = move % 64;

        return makeMove(from, to);  // Calls the real makeMove from main.cpp
    }

}
