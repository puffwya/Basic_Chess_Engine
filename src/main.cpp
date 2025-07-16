#include <emscripten.h>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <algorithm>

extern "C" {
    
    uint8_t board[64];
    
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

    EMSCRIPTEN_KEEPALIVE
    bool isInCheck(bool white) {
        int kingSquare = findKing(white, board);
        return isSquareAttacked(kingSquare, !white);
    }

    bool hasLegalMoves(bool white) {
        for (int from = 0; from < 64; from++) {
            uint8_t piece = board[from];
            if (piece == 0 || (piece % 2 == 1) != white) continue;

            for (int to = 0; to < 64; to++) {
                if (isValidMove(from, to) && !wouldKingBeInCheckAfterMove(from, to)) {
                    return true;
                }
            }
        }
        return false;
    }

    EMSCRIPTEN_KEEPALIVE
    bool isCheckmate(bool white) {
        return isInCheck(white) && !hasLegalMoves(white);
    }

    EMSCRIPTEN_KEEPALIVE
    int getKingSquare(bool white) {
        return findKing(white, board);
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
    
    bool makeMove(int from, int to) {
        uint8_t piece = board[from];   
        if (piece == 0) return false;

        bool isWhitePiece = (piece % 2) == 1;
        if (whiteToMove != isWhitePiece) return false;  

        if (!isValidMove(from, to)) return false;

        // Check if move leaves king in check
        if (wouldKingBeInCheckAfterMove(from, to)) return false;

        // ----- Handle en passant capture -----
        if ((piece == 1 || piece == 2) && to == enPassantTarget) {
            int capturedPawnSq = isWhitePiece ? to - 8 : to + 8;
            board[capturedPawnSq] = 0;
        }

        // ----- Handle castling rook movement -----
        if (piece == 11) { // White king
            if (from == 4 && to == 6) { // Kingside castling
                board[5] = board[7];
                board[7] = 0;
            } else if (from == 4 && to == 2) { // Queenside castling
                board[3] = board[0];
                board[0] = 0;
            }
        } else if (piece == 12) { // Black king
            if (from == 60 && to == 62) { // Kingside castling
                board[61] = board[63];
                board[63] = 0;
            } else if (from == 60 && to == 58) { // Queenside castling
                board[59] = board[56];
                board[56] = 0;
            }
        }

        // ----- Move the piece -----
        board[to] = board[from];
        board[from] = 0;

        // ----- Reset en passant target -----
        enPassantTarget = -1;

        // ----- Set en passant target for pawn double moves -----
        int fromRank = getRank(from);
        int toRank = getRank(to);
        if (piece == 1 && fromRank == 1 && toRank == 3) {
            enPassantTarget = from + 8;
        } else if (piece == 2 && fromRank == 6 && toRank == 4) {
            enPassantTarget = from - 8;
        }

        // ----- Track if kings or rooks move -----
        if (piece == 11) hasWhiteKingMoved = true;
        if (piece == 12) hasBlackKingMoved = true;
        if (from == 0) hasWhiteQueensideRookMoved = true;
        if (from == 7) hasWhiteKingsideRookMoved = true;
        if (from == 56) hasBlackQueensideRookMoved = true;
        if (from == 63) hasBlackKingsideRookMoved = true;

        // Check for promotion
        if ((piece == 1 && to / 8 == 7) || (piece == 2 && to / 8 == 0)) {
            pendingPromotionSquare = to;
            return true;  // Still allow move, but JS will now know to ask for promotion
        }

        whiteToMove = !whiteToMove;
        return true;
    }

    EMSCRIPTEN_KEEPALIVE
    int getPendingPromotionSquare() {
        return pendingPromotionSquare;
    }

    EMSCRIPTEN_KEEPALIVE
    void promotePawn(int square, int newPieceCode) {
        if (square == pendingPromotionSquare &&
            (newPieceCode == 9 || newPieceCode == 10 ||  // Queen
             newPieceCode == 7 || newPieceCode == 8 ||   // Rook
             newPieceCode == 5 || newPieceCode == 6 ||   // Bishop
             newPieceCode == 3 || newPieceCode == 4)) {  // Knight

            board[square] = newPieceCode;
            pendingPromotionSquare = -1;
            whiteToMove = !whiteToMove;  // Now switch turn after promotion is handled
        }
    }
    
    // Initialize board to standard chess starting position
    void initBoard() {
        uint8_t initialBoard[64] = {
            7,3,5,9,11,5,3,7,
            1,1,1,1,1,1,1,1,
            0,0,0,0,0,0,0,0,
            0,0,0,0,0,0,0,0,
            0,0,0,0,0,0,0,0,
            0,0,0,0,0,0,0,0,
            2,2,2,2,2,2,2,2,
            8,4,6,10,12,6,4,8
        };
        memcpy(board, initialBoard, 64);
        for(int i=0; i<64; i++) {
            board[i] = initialBoard[i];
        }
        whiteToMove = true;
        enPassantTarget = -1;
    }
    
    // Get board pointer (for JS rendering)
    uint8_t* getBoard() {
        return board;
    }
    
    // Return current turn: 1 = White, 2 = Black
    int currentTurn() {
        return whiteToMove ? 1 : 2;
    }

} // extern "C"
