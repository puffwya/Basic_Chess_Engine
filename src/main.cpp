#include <emscripten.h>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <set>
#include "engine.h"
#include "main.h"
#include <iostream>

extern "C" {
    EMSCRIPTEN_KEEPALIVE void initBoard();
    EMSCRIPTEN_KEEPALIVE uint8_t* getBoard();
    EMSCRIPTEN_KEEPALIVE bool makeMove(int from, int to);
    EMSCRIPTEN_KEEPALIVE int getPendingPromotionSquare();
    EMSCRIPTEN_KEEPALIVE void promotePawn(int square, int newPieceCode);
    EMSCRIPTEN_KEEPALIVE int currentTurn();
    EMSCRIPTEN_KEEPALIVE bool isInCheck(bool white);
    EMSCRIPTEN_KEEPALIVE bool isCheckmate(bool white);
    EMSCRIPTEN_KEEPALIVE bool isStalemate();
    EMSCRIPTEN_KEEPALIVE bool isInsufficientMaterial();
}


// ------------Internal helper functions/vars-----------------//
//------------------------------------------------------------//

using u64 = uint64_t;

uint64_t whitePawns, blackPawns;
uint64_t whiteKnights, blackKnights;
uint64_t whiteBishops, blackBishops;
uint64_t whiteRooks, blackRooks;
uint64_t whiteQueens, blackQueens;
uint64_t whiteKing, blackKing;

uint64_t whitePieces, blackPieces, allPieces;

int board[64]; // Optional, but useful for UI/debugging

bool whiteToMove;

struct Bitboards {
    u64 whitePawns;
    u64 blackPawns;
    u64 whiteKnights;
    u64 blackKnights;
    u64 whiteBishops;
    u64 blackBishops;
    u64 whiteRooks;
    u64 blackRooks;
    u64 whiteQueens;
    u64 blackQueens;
    u64 whiteKing;
    u64 blackKing;

    u64 whitePieces() const {
        return whitePawns | whiteKnights | whiteBishops | whiteRooks | whiteQueens | whiteKing;
    }

    u64 blackPieces() const {
        return blackPawns | blackKnights | blackBishops | blackRooks | blackQueens | blackKing;
    }

    u64 allPieces() const {
        return whitePieces() | blackPieces();
    }
};

// Helper to convert square index to bitboard bit
inline u64 bit(int square) {
    return 1ULL << square;
}

// To get rank and file (0..7)
inline int rank(int sq) { return sq >> 3; }
inline int file(int sq) { return sq & 7; }

enum {
    EMPTY = 0,
    WHITE_PAWN = 1, BLACK_PAWN = 2,
    WHITE_KNIGHT = 3, BLACK_KNIGHT = 4,
    WHITE_BISHOP = 5, BLACK_BISHOP = 6,
    WHITE_ROOK = 7,   BLACK_ROOK = 8,
    WHITE_QUEEN = 9,  BLACK_QUEEN = 10,
    WHITE_KING = 11,  BLACK_KING = 12
};

typedef uint64_t Bitboard;

constexpr Bitboard FILE_A = 0x0101010101010101ULL;
constexpr Bitboard FILE_H = 0x8080808080808080ULL;
constexpr Bitboard RANK_1 = 0x00000000000000FFULL;
constexpr Bitboard RANK_8 = 0xFF00000000000000ULL;

// Helper macros
inline int popLSB(Bitboard &bb) {
    int sq = __builtin_ctzll(bb);
    bb &= bb - 1;
    return sq;
}

inline Bitboard squareBB(int square) {
    return 1ULL << square;
}

inline bool isSquareOccupied(Bitboard board, int square) {
    return board & (1ULL << square);
}

int getPieceAt(int square) {
    return board[square]; // Returns EMPTY or one of your defined enums
}

void updateAggregateBitboards() {
    whitePieces = whitePawns | whiteKnights | whiteBishops | whiteRooks | whiteQueens | whiteKing;
    blackPieces = blackPawns | blackKnights | blackBishops | blackRooks | blackQueens | blackKing;
    allPieces = whitePieces | blackPieces;
}

uint64_t& getBitboard(int pieceCode) {
    switch (pieceCode) {
        case 1: return whitePawns;
        case 2: return blackPawns;
        case 3: return whiteKnights;
        case 4: return blackKnights;
        case 5: return whiteBishops;
        case 6: return blackBishops;
        case 7: return whiteRooks;
        case 8: return blackRooks;
        case 9: return whiteQueens;
        case 10: return blackQueens;
        case 11: return whiteKings;
        case 12: return blackKings;
        default:
            std::cerr << "Invalid piece code: " << pieceCode << "\n";
            exit(1); // or handle gracefully
    }
}

uint64_t& getSideBitboard(int pieceCode) {
    if (pieceCode % 2 == 1) {
        return whitePieces;
    } else {
        return blackPieces;
    }
}

// Bitmask layout: 0b0000WQWK BQBK (White Queen/Kingside, Black Queen/Kingside)
uint8_t castlingRights = 0b1111;

void disableCastlingRights(int pieceCode, int fromSquare = -1) {
    switch (pieceCode) {
        case 7: // White Rook
            if (fromSquare == 0) castlingRights &= 0b1110; // WQ
            else if (fromSquare == 7) castlingRights &= 0b1101; // WK
            break;
        case 8: // Black Rook
            if (fromSquare == 56) castlingRights &= 0b1011; // BQ
            else if (fromSquare == 63) castlingRights &= 0b0111; // BK
            break;
        case 11: // White King
            castlingRights &= 0b1100; // disable both white sides
            break;
        case 12: // Black King
            castlingRights &= 0b0011; // disable both black sides
            break;
    }
}

void updateOccupancy() {
    whitePieces = whitePawns | whiteKnights | whiteBishops | whiteRooks | whiteQueens | whiteKings;
    blackPieces = blackPawns | blackKnights | blackBishops | blackRooks | blackQueens | blackKings;
    allPieces   = whitePieces | blackPieces;
}

inline void setBit(uint64_t &bb, int square) {
    bb |= (1ULL << square);
}

inline void clearBit(uint64_t &bb, int square) {
    bb &= ~(1ULL << square);
}

            
// ---------------------------//


static bool whiteToMove = true;
static int enPassantTarget = -1; // -1 = no en passant possible
bool hasWhiteKingMoved = false;
bool hasBlackKingMoved = false;
bool hasWhiteKingsideRookMoved = false;
bool hasWhiteQueensideRookMoved = false;
bool hasBlackKingsideRookMoved = false;
bool hasBlackQueensideRookMoved = false;
int pendingPromotionSquare = -1; // -1 if no promotion is pending

// Utility to get rank (0-7) and file (0-7) from square index (0-63)
inline int getRank(int square) { return square / 8; }
inline int getFile(int square) { return square % 8; }
         
inline bool isSameColor(uint8_t p1, uint8_t p2) {
    if (p1 == 0 || p2 == 0) return false;
    return (p1 % 2) == (p2 % 2);
}
    
// Forward declaration for helper that checks if a piece at 'from' attacks 'to' on board (without king safety)
bool canAttack(int from, int to, const uint8_t b[64]);

bool isValidMove(int from, int to);
bool wouldKingBeInCheckAfterMove(int from, int to);
bool hasLegalMoves(bool white);
        
     
// Helper: find king position for color on board
int findKing(bool white, const uint8_t b[64]) {
    uint8_t kingCode = white ? 11 : 12;
    for (int i = 0; i < 64; i++) {
        if (b[i] == kingCode) return i;
    }
    return -1; // Should not happen
}

// Check if square 'sq' is attacked by color 'byWhite' on board 'b'
bool isSquareAttackedOnBoard(int sq, bool byWhite, const uint8_t b[64]) {
    for (int i = 0; i < 64; i++) {
        uint8_t p = b[i];
        if (p == 0) continue;
        bool pieceWhite = (p % 2) == 1;
        if (pieceWhite != byWhite) continue;
        if (canAttack(i, sq, b)) return true;
    }            
    return false;
}
                
// Convenience: check on current board
bool isSquareAttacked(int sq, bool byWhite) {
    return isSquareAttackedOnBoard(sq, byWhite, board);
}

// Helper to check if any legal moves exist for the side to move
bool hasLegalMoves(bool white) {
    for (int from = 0; from < 64; ++from) {
        if (board[from] == 0 || (board[from] % 2 == 1) != white) continue;

        for (int to = 0; to < 64; ++to) {
            if (!isValidMove(from, to)) continue;
            if (!wouldKingBeInCheckAfterMove(from, to)) return true;
        }
    }
    return false;
}

// Checks raw move attacks ignoring king safety (used for attack detection)
bool canAttack(int from, int to, const uint8_t b[64]) {
    uint8_t piece = b[from];
    if (piece == 0) return false;
            
    int fx = getFile(from), fy = getRank(from);
    int tx = getFile(to), ty = getRank(to);
                
    int dx = tx - fx;
    int dy = ty - fy;
        
    int absdx = std::abs(dx);
    int absdy = std::abs(dy);
            
    uint8_t destPiece = b[to];
    bool isWhite = (piece % 2) == 1;

    // Don't consider king safety here!
        
    switch(piece) {
        case 1: // White Pawn attack squares (captures)
            if (dy == 1 && absdx == 1) return true;
            break;  
        case 2: // Black Pawn attack squares (captures)
            if (dy == -1 && absdx == 1) return true;
            break;
            
        case 3: // White Knight
        case 4: // Black Knight
            if ((absdx == 2 && absdy == 1) || (absdx == 1 && absdy == 2)) return true;
            break;
        
        case 5: // White Bishop
        case 6: // Black Bishop
            if (absdx == absdy && absdx != 0) {
                int stepx = (dx > 0) ? 1 : -1;
                int stepy = (dy > 0) ? 1 : -1;
                for (int i = 1; i < absdx; i++) {
                    int ix = fx + stepx * i;
                    int iy = fy + stepy * i;
                    if (b[iy*8 + ix] != 0) return false;
                }
                return true;
            }
            break;
        case 7: // White Rook
        case 8: // Black Rook
            if ((dx == 0 && dy != 0) || (dy == 0 && dx != 0)) {
                int stepx = (dx == 0) ? 0 : (dx > 0 ? 1 : -1);
                int stepy = (dy == 0) ? 0 : (dy > 0 ? 1 : -1);
                int steps = std::max(absdx, absdy);
                for (int i = 1; i < steps; i++) {
                    int ix = fx + stepx * i;
                    int iy = fy + stepy * i;
                    if (b[iy*8 + ix] != 0) return false;
                }
                return true;
            }
            break;
        case 9: // White Queen
        case 10:// Black Queen
            if ((dx == 0 && dy != 0) || (dy == 0 && dx != 0)) {
                int stepx = (dx == 0) ? 0 : (dx > 0 ? 1 : -1);
                int stepy = (dy == 0) ? 0 : (dy > 0 ? 1 : -1);
                int steps = std::max(absdx, absdy);
                for (int i = 1; i < steps; i++) {
                    int ix = fx + stepx * i;
                    int iy = fy + stepy * i;
                    if (b[iy*8 + ix] != 0) return false;
                }
                return true;
            } else if (absdx == absdy && absdx != 0) {
                int stepx = (dx > 0) ? 1 : -1;
                int stepy = (dy > 0) ? 1 : -1;
                for (int i = 1; i < absdx; i++) {
                    int ix = fx + stepx * i;
                    int iy = fy + stepy * i;
                    if (b[iy*8 + ix] != 0) return false;
                }
                return true;
            }
            break;
            
        case 11: // White King
        case 12: // Black King
            if (absdx <= 1 && absdy <= 1) return true;
            break;
    }
    return false;
}

bool isValidMove(int from, int to) {
    if (from < 0 || from >= 64 || to < 0 || to >= 64) return false;
    uint8_t piece = board[from];
    if (piece == 0) return false;
    uint8_t destPiece = board[to];
                
    bool isWhitePiece = (piece % 2) == 1;
                
    if (isSameColor(piece, destPiece)) return false; // Can't capture own piece
         
    int fx = getFile(from);
    int fy = getRank(from);
    int tx = getFile(to);
    int ty = getRank(to);
            
    int dx = tx - fx;
    int dy = ty - fy;
    int absdx = std::abs(dx);
    int absdy = std::abs(dy);
                
    // Pawn logic with en passant and capturing
    if (piece == 1) { // White Pawn
        if (dx == 0) {
            if (dy == 1 && destPiece == 0) return true;
            if (dy == 2 && fy == 1 && destPiece == 0 && board[from + 8] == 0) return true;
        }
        if (dy == 1 && absdx == 1) {
            if (destPiece != 0 && !isSameColor(piece, destPiece)) return true; // capture
            if (to == enPassantTarget) return true; // en passant
        }
        return false;
    } else if (piece == 2) { // Black Pawn
        if (dx == 0) {
            if (dy == -1 && destPiece == 0) return true;
            if (dy == -2 && fy == 6 && destPiece == 0 && board[from - 8] == 0) return true;
        }
        if (dy == -1 && absdx == 1) {
            if (destPiece != 0 && !isSameColor(piece, destPiece)) return true; // capture
            if (to == enPassantTarget) return true; // en passant
        }
        return false;
    }

    // Knight
        if (piece == 3 || piece == 4) {
            if ((absdx == 2 && absdy == 1) || (absdx == 1 && absdy == 2)) return true;
            return false;
        }
                
        // Bishop
        if (piece == 5 || piece == 6) {
            if (absdx == absdy && absdx != 0) {
                int stepx = (dx > 0) ? 1 : -1;
                int stepy = (dy > 0) ? 1 : -1;
                for (int i = 1; i < absdx; i++) {
                    if (board[(fy + stepy*i)*8 + (fx + stepx*i)] != 0) return false;
                }
                return true;
            }
            return false;
        }
                
        // Rook
        if (piece == 7 || piece == 8) {
            if ((dx == 0 && dy != 0) || (dy == 0 && dx != 0)) {
                int stepx = (dx == 0) ? 0 : (dx > 0 ? 1 : -1);
                int stepy = (dy == 0) ? 0 : (dy > 0 ? 1 : -1);
                int steps = std::max(absdx, absdy);
                for (int i = 1; i < steps; i++) {
                    if (board[(fy + stepy*i)*8 + (fx + stepx*i)] != 0) return false;
                }
                return true;
            }
            return false;
        }

        // Queen
        if (piece == 9 || piece == 10) {
            if ((dx == 0 && dy != 0) || (dy == 0 && dx != 0)) {
                int stepx = (dx == 0) ? 0 : (dx > 0 ? 1 : -1);
                int stepy = (dy == 0) ? 0 : (dy > 0 ? 1 : -1);
                int steps = std::max(absdx, absdy);
                for (int i = 1; i < steps; i++) {
                    if (board[(fy + stepy*i)*8 + (fx + stepx*i)] != 0) return false;
                }
                return true;
            } else if (absdx == absdy && absdx != 0) {
                int stepx = (dx > 0) ? 1 : -1;
                int stepy = (dy > 0) ? 1 : -1;
                for (int i = 1; i < absdx; i++) {
                    if (board[(fy + stepy*i)*8 + (fx + stepx*i)] != 0) return false;
                }
                return true;
            }
            return false;
        }
                
        // King
        if (piece == 11 || piece == 12) {
            if (absdx <= 1 && absdy <= 1) {
                // Cannot capture a defended piece
                if (destPiece != 0 && !isSameColor(piece, destPiece)) {
                    // King cannot capture defended piece
                    if (isSquareAttacked(to, !isWhitePiece)) return false;
                }
                return true;
            }   

        // ----- Castling Logic -----
        if (dy == 0) {
            // White king
            if (isWhitePiece && from == 4 && !hasWhiteKingMoved) {
                if (dx == 2 && !hasWhiteKingsideRookMoved &&
                    board[5] == 0 && board[6] == 0 &&
                    !isSquareAttacked(4, false) &&
                    !isSquareAttacked(5, false) &&
                    !isSquareAttacked(6, false)) {
                    return true;
                }
                if (dx == -2 && !hasWhiteQueensideRookMoved &&
                    board[1] == 0 && board[2] == 0 && board[3] == 0 &&
                    !isSquareAttacked(4, false) &&
                    !isSquareAttacked(3, false) &&
                    !isSquareAttacked(2, false)) {
                    return true;
                }
            }
                
            // Black king
            if (!isWhitePiece && from == 60 && !hasBlackKingMoved) {
                if (dx == 2 && !hasBlackKingsideRookMoved &&
                    board[61] == 0 && board[62] == 0 &&
                    !isSquareAttacked(60, true) &&
                    !isSquareAttacked(61, true) &&
                    !isSquareAttacked(62, true)) {
                    return true;
                }
                if (dx == -2 && !hasBlackQueensideRookMoved &&
                    board[57] == 0 && board[58] == 0 && board[59] == 0 &&
                    !isSquareAttacked(60, true) &&
                    !isSquareAttacked(59, true) &&  
                    !isSquareAttacked(58, true)) {
                    return true;
                }
            }
        }
    
        return false;
    }
            
    return false;
}

// Check if after move, king would be in check (illegal move)
bool wouldKingBeInCheckAfterMove(int from, int to) {
    uint8_t tempBoard[64];
    for (int i = 0; i < 64; i++) tempBoard[i] = board[i];
    
    uint8_t piece = tempBoard[from];
    bool white = (piece % 2) == 1;
    
    // Handle en passant capture in simulation
    if ((piece == 1 || piece == 2) && to == enPassantTarget) {
        int capturedPawnSq = white ? to - 8 : to + 8;
        tempBoard[capturedPawnSq] = 0;
    }
    
    tempBoard[to] = tempBoard[from];
    tempBoard[from] = 0;
    
    // Find king position
    int kingPos = -1;
    if (piece == 11 || piece == 12) {
        kingPos = to;
    } else {
        kingPos = findKing(white, tempBoard);
    }
    
    // Check if king is attacked by opponent
    return isSquareAttackedOnBoard(kingPos, !white, tempBoard);
}

//--------------------Global functions/vars--------------------//
//-------------------------------------------------------------//
    
extern "C" EMSCRIPTEN_KEEPALIVE bool isInCheck(bool white) {
    int kingSquare = findKing(white, board);
    return isSquareAttacked(kingSquare, !white);
}

extern "C" EMSCRIPTEN_KEEPALIVE bool isCheckmate(bool white) {
    return isInCheck(white) && !hasLegalMoves(white);
}

extern "C" EMSCRIPTEN_KEEPALIVE int getKingSquare(bool white) {
    return findKing(white, board);
}

extern "C" EMSCRIPTEN_KEEPALIVE int getBestAIMove(bool white) {
    return findBestMove(white);  // Returns from * 64 + to
}

extern "C" EMSCRIPTEN_KEEPALIVE bool isStalemate() {
    bool white = whiteToMove;
    return !isInCheck(white) && !hasLegalMoves(white);
}

extern "C" EMSCRIPTEN_KEEPALIVE bool isInsufficientMaterial() {
    std::vector<int> pieces;
    for (int i = 0; i < 64; ++i) {
        if (board[i] != 0) pieces.push_back(board[i]);
    }
     
    // Only kings   
    if (pieces.size() == 2) return true;
    
    // King + Bishop or Knight vs King
    if (pieces.size() == 3) {
        for (int p : pieces) {
            if (p != 11 && p != 12 && p != 3 && p != 4 && p != 5 && p != 6)
                return false;
        }
        return true;
    }
            
    // King + Bishop vs King + Bishop with same color bishops
    if (pieces.size() == 4) {
        int whiteBishop = -1, blackBishop = -1;
        for (int i = 0; i < 64; ++i) {
            int p = board[i];
            if (p == 5) whiteBishop = i;
            if (p == 6) blackBishop = i;
        }
        if (whiteBishop != -1 && blackBishop != -1) {
            bool whiteColor = (getFile(whiteBishop) + getRank(whiteBishop)) % 2 == 0;
            bool blackColor = (getFile(blackBishop) + getRank(blackBishop)) % 2 == 0;
            return whiteColor == blackColor;
        }
    }

    // King + Knight vs King + Knight
    if (pieces.size() == 4) {
        bool whiteKing = false, blackKing = false;
        int whiteKnights = 0, blackKnights = 0;

        for (int p : pieces) {
            if (p == 11) whiteKing = true;
            else if (p == 12) blackKing = true;
            else if (p == 3) whiteKnights++;
            else if (p == 4) blackKnights++;
        }

        if (whiteKing && blackKing && whiteKnights == 1 && blackKnights == 1) {
            return true;
        }
    }
    
    return false;
}

extern "C" EMSCRIPTEN_KEEPALIVE
void makeMove(Move move) {
    int from = move.from;
    int to = move.to;
    uint64_t fromMask = 1ULL << from;
    uint64_t toMask   = 1ULL << to;

    int movingPiece = getPieceAt(from);
    int capturedPiece = getPieceAt(to);

    // Remove piece from 'from' square and add to 'to' square
    switch (movingPiece) {
        case WHITE_PAWN:
            whitePawns &= ~fromMask;
            whitePawns |= toMask;
            break;
        case WHITE_KNIGHT:
            whiteKnights &= ~fromMask;
            whiteKnights |= toMask;
            break;
        case WHITE_BISHOP:
            whiteBishops &= ~fromMask;
            whiteBishops |= toMask;
            break;
        case WHITE_ROOK:
            whiteRooks &= ~fromMask;
            whiteRooks |= toMask;
            break;
        case WHITE_QUEEN:
            whiteQueens &= ~fromMask;
            whiteQueens |= toMask;
            break;
        case WHITE_KING:
            whiteKing &= ~fromMask;
            whiteKing |= toMask;
            break;
        case BLACK_PAWN:
            blackPawns &= ~fromMask;
            blackPawns |= toMask;
            break;
        case BLACK_KNIGHT:
            blackKnights &= ~fromMask;
            blackKnights |= toMask;
            break;
        case BLACK_BISHOP:
            blackBishops &= ~fromMask;
            blackBishops |= toMask;
            break;
        case BLACK_ROOK:
            blackRooks &= ~fromMask;
            blackRooks |= toMask;
            break;
        case BLACK_QUEEN:
            blackQueens &= ~fromMask;
            blackQueens |= toMask;
            break;
        case BLACK_KING:
            blackKing &= ~fromMask;
            blackKing |= toMask;
            break;
    }

    // Remove captured piece
    if (capturedPiece != EMPTY) {
        switch (capturedPiece) {
            case WHITE_PAWN:
                whitePawns &= ~toMask;
                break;
            case WHITE_KNIGHT:
                whiteKnights &= ~toMask;
                break;
            case WHITE_BISHOP:
                whiteBishops &= ~toMask;
                break;
            case WHITE_ROOK:
                whiteRooks &= ~toMask;
                break;
            case WHITE_QUEEN:
                whiteQueens &= ~toMask;
                break;
            case WHITE_KING:
                whiteKing &= ~toMask;
                break;
            case BLACK_PAWN:
                blackPawns &= ~toMask;
                break;
            case BLACK_KNIGHT:
                blackKnights &= ~toMask;
                break;
            case BLACK_BISHOP:
                blackBishops &= ~toMask;
                break;
            case BLACK_ROOK:
                blackRooks &= ~toMask;
                break;
            case BLACK_QUEEN:
                blackQueens &= ~toMask;
                break;
            case BLACK_KING:
                blackKing &= ~toMask;
                break;
        }
    }

// Remove piece from origin
    clearBit(getBitboard(piece), fromSquare);
    clearBit(allPieces, fromSquare);
    clearBit(getSideBitboard(piece), fromSquare);

    // Handle castling
    if ((piece == 11 || piece == 12) && abs(toSquare - fromSquare) == 2) {
        // King is moving two squares — castling
        if (toSquare == 62) { // White kingside (e1 -> g1)
            clearBit(whiteRooks, 63);
            setBit(whiteRooks, 61);
            clearBit(whitePieces, 63);
            setBit(whitePieces, 61);
        } else if (toSquare == 58) { // White queenside (e1 -> c1)
            clearBit(whiteRooks, 56);
            setBit(whiteRooks, 59);
            clearBit(whitePieces, 56);
            setBit(whitePieces, 59);
        } else if (toSquare == 6) { // Black kingside (e8 -> g8)
            clearBit(blackRooks, 7);
            setBit(blackRooks, 5);
            clearBit(blackPieces, 7);
            setBit(blackPieces, 5);
        } else if (toSquare == 2) { // Black queenside (e8 -> c8)
            clearBit(blackRooks, 0);
            setBit(blackRooks, 3);
            clearBit(blackPieces, 0);
            setBit(blackPieces, 3);
        }
        // Update castling rights
        disableCastlingRights(piece);
    }

    // Remove captured piece
    if (capturedPiece != 0) {
        clearBit(getBitboard(capturedPiece), toSquare);
        clearBit(allPieces, toSquare);
        clearBit(getSideBitboard(capturedPiece), toSquare);
    }

    // Promotion
    if ((piece == 1 && toSquare >= 56) || (piece == 2 && toSquare <= 7)) {
        // White pawn reaching 8th rank or black reaching 1st
        uint64_t& newPieceBoard = getBitboard(promotion);
        setBit(newPieceBoard, toSquare);
        setBit(getSideBitboard(promotion), toSquare);
    } else {
        // Normal move
        setBit(getBitboard(piece), toSquare);
        setBit(getSideBitboard(piece), toSquare);
    }

    // Update all pieces mask
    updateOccupancy();

    // Update board[64] array for compatibility/debugging if needed
    board[from] = EMPTY;
    board[to] = movingPiece;

    // Update whitePieces and blackPieces bitboards
    updateAggregateBitboards();

    // Flip turn
    whiteToMove = !whiteToMove;
}
                        
extern "C" EMSCRIPTEN_KEEPALIVE int getPendingPromotionSquare() {
    return pendingPromotionSquare;
}

extern "C" EMSCRIPTEN_KEEPALIVE void promotePawn(int square, int newPieceCode) {
    if (square == pendingPromotionSquare &&
        (newPieceCode == 9 || newPieceCode == 10 ||  // Queen
         newPieceCode == 7 || newPieceCode == 8 ||   // Rook
         newPieceCode == 5 || newPieceCode == 6 ||   // Bishop
         newPieceCode == 3 || newPieceCode == 4)) {  // Knight

        board[square] = newPieceCode;
        pendingPromotionSquare = -1;
        whiteToMove = !whiteToMove;  // Switch turn after promotion is handled
        EM_ASM({
          console.log("Promoting at " + $0 + " to " + $1);
        }, index, newPieceCode);

    }
}
    
// Initialize board to standard chess starting position
extern "C" EMSCRIPTEN_KEEPALIVE void initBitboards(Bitboards &bb) {
    // Clear all
    bb = {};

    // Set pieces (example, based on standard starting position squares)
    // White pawns on rank 2 (squares 8..15)
    for (int sq = 8; sq <= 15; ++sq) bb.whitePawns |= bit(sq);

    // Black pawns on rank 7 (squares 48..55)
    for (int sq = 48; sq <= 55; ++sq) bb.blackPawns |= bit(sq);

    // White pieces
    bb.whiteRooks = bit(0) | bit(7);
    bb.whiteKnights = bit(1) | bit(6);
    bb.whiteBishops = bit(2) | bit(5);
    bb.whiteQueens = bit(3);
    bb.whiteKing = bit(4);

    // Black pieces
    bb.blackRooks = bit(56) | bit(63);
    bb.blackKnights = bit(57) | bit(62);
    bb.blackBishops = bit(58) | bit(61);
    bb.blackQueens = bit(59);
    bb.blackKing = bit(60);
}

    
// Get board pointer (for JS rendering)
extern "C" EMSCRIPTEN_KEEPALIVE uint8_t* getBoard() {
    return board;
}
    
// Return current turn: 1 = White, 2 = Black
extern "C" EMSCRIPTEN_KEEPALIVE int currentTurn() {
    return whiteToMove ? 1 : 2;
}

extern "C" EMSCRIPTEN_KEEPALIVE void setCurrentTurn(int turn) {
    whiteToMove = (turn == 1);
}

