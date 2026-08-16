// ============================================================================
//  Console Chess Game in C++  (no <vector>, <string>, or <algorithm>)
//  Features: new game, load game, save game, board display, move pieces,
//  legal move validation, check/checkmate/stalemate detection, move history,
//  undo, game statistics, and a few extras (legal-move hints, resign/draw).
//
//  Uses only fixed-size C arrays and C-string functions (<cstring>, <fstream>)
//  in place of std::vector / std::string / <algorithm>.
// ============================================================================

#include <iostream>
#include <fstream>
#include <cstring>
#include <cctype>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace std;

// Makes the console understand UTF-8 so the Unicode chess figurines
// (kings, queens, etc.) render correctly instead of as garbled characters.
// On Linux/macOS terminals are UTF-8 by default, so this is a no-op there.
static void setupUnicodeConsole() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

// ----------------------------------------------------------------------------
// Basic types
// ----------------------------------------------------------------------------
enum class PType { EMPTY, PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING };
enum class Color { NONE, WHITE, BLACK };

struct Piece {
    PType type = PType::EMPTY;
    Color color = Color::NONE;
};

struct Pos { int r; int c; };

struct Move {
    int fromR = -1, fromC = -1, toR = -1, toC = -1;
    PType promotion = PType::EMPTY;
    bool castleK = false;
    bool castleQ = false;
    bool enPassant = false;
    bool doubleStep = false;
    PType movedType = PType::EMPTY;
    PType capturedType = PType::EMPTY;
    Color movedColor = Color::NONE;
};

struct GameState {
    Piece board[8][8];
    Color turn = Color::WHITE;
    bool wKingMoved = false, bKingMoved = false;
    bool wRookAMoved = false, wRookHMoved = false;
    bool bRookAMoved = false, bRookHMoved = false;
    int epRow = -1, epCol = -1;          // en-passant target square (square skipped over)
    int halfmoveClock = 0;               // for 50-move rule tracking
    int fullmoveNumber = 1;
};

struct Stats {
    int totalMoves = 0;
    int whiteCaptures = 0, blackCaptures = 0;
    int checksGiven = 0;
    int castles = 0;
    int promotions = 0;
    int enPassantCaptures = 0;
};

// ----------------------------------------------------------------------------
// Capacity limits (replacing dynamic containers)
// ----------------------------------------------------------------------------
const int MAX_HISTORY = 1024;   // max plies we can store/undo/replay
const int MAX_GEN = 512;        // max pseudo/legal moves generatable from one position

// ----------------------------------------------------------------------------
// Helper conversions
// ----------------------------------------------------------------------------
static bool inBounds(int r, int c) { return r >= 0 && r < 8 && c >= 0 && c < 8; }

// Writes at most one letter + null terminator into out (out must hold >=2 chars)
void pieceLetterStr(PType t, char* out) {
    switch (t) {
        case PType::KNIGHT: strcpy(out, "N"); break;
        case PType::BISHOP: strcpy(out, "B"); break;
        case PType::ROOK:   strcpy(out, "R"); break;
        case PType::QUEEN:  strcpy(out, "Q"); break;
        case PType::KING:   strcpy(out, "K"); break;
        default: out[0] = '\0'; break;
    }
}

// Writes a board glyph for a piece into out (out must hold >=8 bytes, UTF-8 symbol is up to 4 bytes).
// symbols=true -> Unicode chess figurines (works in most modern terminals/fonts).
// symbols=false -> plain ASCII letters (uppercase=White, lowercase=Black), the safe fallback.
void pieceGlyph(const Piece& p, bool symbols, char* out) {
    if (p.type == PType::EMPTY) { strcpy(out, "."); return; }
    if (!symbols) {
        char c;
        switch (p.type) {
            case PType::PAWN:   c = 'p'; break;
            case PType::KNIGHT: c = 'n'; break;
            case PType::BISHOP: c = 'b'; break;
            case PType::ROOK:   c = 'r'; break;
            case PType::QUEEN:  c = 'q'; break;
            case PType::KING:   c = 'k'; break;
            default: c = '?'; break;
        }
        if (p.color == Color::WHITE) c = (char)toupper(c);
        out[0] = c; out[1] = '\0';
        return;
    }
    if (p.color == Color::WHITE) {
        switch (p.type) {
            case PType::PAWN:   strcpy(out, "\u2659"); break;
            case PType::KNIGHT: strcpy(out, "\u2658"); break;
            case PType::BISHOP: strcpy(out, "\u2657"); break;
            case PType::ROOK:   strcpy(out, "\u2656"); break;
            case PType::QUEEN:  strcpy(out, "\u2655"); break;
            case PType::KING:   strcpy(out, "\u2654"); break;
            default: strcpy(out, "."); break;
        }
    } else {
        switch (p.type) {
            case PType::PAWN:   strcpy(out, "\u265F"); break;
            case PType::KNIGHT: strcpy(out, "\u265E"); break;
            case PType::BISHOP: strcpy(out, "\u265D"); break;
            case PType::ROOK:   strcpy(out, "\u265C"); break;
            case PType::QUEEN:  strcpy(out, "\u265B"); break;
            case PType::KING:   strcpy(out, "\u265A"); break;
            default: strcpy(out, "."); break;
        }
    }
}


// Writes e.g. "e4" into out (out must hold >=3 chars)
void squareNameStr(int r, int c, char* out) {
    out[0] = char('a' + c);
    out[1] = char('1' + r);
    out[2] = '\0';
}

PType charToPromo(char c) {
    switch (tolower((unsigned char)c)) {
        case 'q': return PType::QUEEN;
        case 'r': return PType::ROOK;
        case 'b': return PType::BISHOP;
        case 'n': return PType::KNIGHT;
        default:  return PType::EMPTY;
    }
}

char promoToChar(PType t) {
    switch (t) {
        case PType::QUEEN:  return 'q';
        case PType::ROOK:   return 'r';
        case PType::BISHOP: return 'b';
        case PType::KNIGHT: return 'n';
        default: return ' ';
    }
}

Color opponent(Color c) { return c == Color::WHITE ? Color::BLACK : Color::WHITE; }

// ----------------------------------------------------------------------------
// Chess engine / game class
// ----------------------------------------------------------------------------
class ChessGame {
public:
    void run() {
        cout << "==================================\n";
        cout << "        C++ CONSOLE CHESS\n";
        cout << "==================================\n";
        bool running = true;
        while (running) {
            if (!gameActive) {
                running = mainMenuStep();
            } else {
                running = gameMenuStep();
            }
        }
        cout << "Goodbye!\n";
    }

private:
    GameState state;
    GameState undoStack[MAX_HISTORY];
    int undoTop = 0;                       // number of saved snapshots (== moveCount)
    Move moveList[MAX_HISTORY];
    char notationHistory[MAX_HISTORY][32];
    int moveCount = 0;
    Stats stats;
    bool gameActive = false;               // a game has been started/loaded
    bool gameOver = false;                 // checkmate / stalemate / resign / draw reached
#ifdef _WIN32
    // classic cmd.exe consoles usually lack chess-glyph coverage in their fonts,
    // even with UTF-8 output enabled, so default to the safe ASCII letters there.
    bool useSymbols = false;
#else
    bool useSymbols = true;                // display Unicode chess figurines vs ASCII letters
#endif

    // ---------------- Input helpers ----------------
    static void trimInPlace(char* s) {
        int len = (int)strlen(s);
        int start = 0;
        while (start < len && isspace((unsigned char)s[start])) start++;
        int end = len - 1;
        while (end >= start && isspace((unsigned char)s[end])) end--;
        int newLen = end - start + 1;
        if (newLen < 0) newLen = 0;
        memmove(s, s + start, newLen);
        s[newLen] = '\0';
    }

    static void readLine(char* buf, int size) {
        if (!cin.getline(buf, size)) {
            cin.clear();
            buf[0] = '\0';
        }
        trimInPlace(buf);
    }

    static void toLowerInPlace(char* s) {
        for (int i = 0; s[i]; i++) s[i] = (char)tolower((unsigned char)s[i]);
    }

    // ---------------- Menus ----------------
    bool mainMenuStep() {
        cout << "\n----------- MAIN MENU -----------\n";
        cout << "1. Start a new game\n";
        cout << "2. Load an existing game\n";
        cout << "0. Exit\n";
        cout << "Choice: ";
        char choice[16];
        readLine(choice, sizeof(choice));
        if (strcmp(choice, "1") == 0) newGame();
        else if (strcmp(choice, "2") == 0) loadGameMenu();
        else if (strcmp(choice, "0") == 0) return false;
        else cout << "Invalid choice, try again.\n";
        return true;
    }

    bool gameMenuStep() {
        cout << "\n----------- GAME MENU -----------\n";
        cout << "1. Display the chess board\n";
        cout << "2. Move a piece\n";
        cout << "3. Undo previous move\n";
        cout << "4. Display move history\n";
        cout << "5. Show game statistics\n";
        cout << "6. Show legal moves for a square\n";
        cout << "7. Save current game progress\n";
        cout << "8. Resign / declare draw\n";
        cout << "9. Return to main menu\n";
        cout << "10. Toggle chess symbols / letters (currently: " << (useSymbols ? "symbols" : "letters") << ")\n";
        cout << "0. Exit\n";
        cout << "Status: " << statusText() << "\n";
        cout << "Choice: ";
        char choice[16];
        readLine(choice, sizeof(choice));
        if (strcmp(choice, "1") == 0) printBoard();
        else if (strcmp(choice, "2") == 0) makeMoveMenu();
        else if (strcmp(choice, "3") == 0) undoMove();
        else if (strcmp(choice, "4") == 0) showHistory();
        else if (strcmp(choice, "5") == 0) showStatistics();
        else if (strcmp(choice, "6") == 0) showLegalMovesMenu();
        else if (strcmp(choice, "7") == 0) saveGameMenu();
        else if (strcmp(choice, "8") == 0) resignOrDraw();
        else if (strcmp(choice, "9") == 0) {
            cout << "Returning to main menu. (Save first if you want to keep this game!)\n";
            gameActive = false;
        }
        else if (strcmp(choice, "10") == 0) {
            useSymbols = !useSymbols;
            cout << "Display mode set to " << (useSymbols ? "chess symbols." : "plain letters.") << "\n";
            printBoard();
        }
        else if (strcmp(choice, "0") == 0) return false;
        else cout << "Invalid choice, try again.\n";
        return true;
    }

    void statusTextInto(char* out) const {
        if (gameOver) { strcpy(out, "Game over"); return; }
        const char* side = (state.turn == Color::WHITE) ? "White" : "Black";
        strcpy(out, side);
        strcat(out, inCheck(state, state.turn) ? " to move (IN CHECK)" : " to move");
    }
    // convenience wrapper returning a temporary via static buffer (single-threaded, fine for this use)
    const char* statusText() const {
        static char buf[64];
        statusTextInto(buf);
        return buf;
    }

    // ---------------- Board setup ----------------
    void initBoard(GameState& st) {
        for (int r = 0; r < 8; r++)
            for (int c = 0; c < 8; c++)
                st.board[r][c] = Piece{PType::EMPTY, Color::NONE};

        PType backRank[8] = {PType::ROOK, PType::KNIGHT, PType::BISHOP, PType::QUEEN,
                              PType::KING, PType::BISHOP, PType::KNIGHT, PType::ROOK};
        for (int c = 0; c < 8; c++) {
            st.board[0][c] = Piece{backRank[c], Color::WHITE};
            st.board[1][c] = Piece{PType::PAWN, Color::WHITE};
            st.board[6][c] = Piece{PType::PAWN, Color::BLACK};
            st.board[7][c] = Piece{backRank[c], Color::BLACK};
        }
        st.turn = Color::WHITE;
        st.wKingMoved = st.bKingMoved = false;
        st.wRookAMoved = st.wRookHMoved = st.bRookAMoved = st.bRookHMoved = false;
        st.epRow = st.epCol = -1;
        st.halfmoveClock = 0;
        st.fullmoveNumber = 1;
    }

    void newGame() {
        initBoard(state);
        undoTop = 0;
        moveCount = 0;
        stats = Stats();
        gameActive = true;
        gameOver = false;
        cout << "\nNew game started. White moves first.\n";
        printBoard();
    }

    void printBoard() {
        cout << "\n";
        for (int r = 7; r >= 0; r--) {
            cout << (r + 1) << " | ";
            for (int c = 0; c < 8; c++) {
                char g[8];
                pieceGlyph(state.board[r][c], useSymbols, g);
                cout << g << " ";
            }
            cout << "|\n";
        }
        cout << "   -----------------\n";
        cout << "    a b c d e f g h\n";
        cout << statusText() << "\n";
    }

    // ---------------- Attack / check detection ----------------
    bool isAttacked(const GameState& st, int r, int c, Color by) const {
        // Pawn attacks
        if (by == Color::WHITE) {
            if (inBounds(r - 1, c - 1) && st.board[r - 1][c - 1].type == PType::PAWN && st.board[r - 1][c - 1].color == Color::WHITE) return true;
            if (inBounds(r - 1, c + 1) && st.board[r - 1][c + 1].type == PType::PAWN && st.board[r - 1][c + 1].color == Color::WHITE) return true;
        } else {
            if (inBounds(r + 1, c - 1) && st.board[r + 1][c - 1].type == PType::PAWN && st.board[r + 1][c - 1].color == Color::BLACK) return true;
            if (inBounds(r + 1, c + 1) && st.board[r + 1][c + 1].type == PType::PAWN && st.board[r + 1][c + 1].color == Color::BLACK) return true;
        }
        // Knight attacks
        static const int kn[8][2] = {{1,2},{2,1},{-1,2},{-2,1},{1,-2},{2,-1},{-1,-2},{-2,-1}};
        for (int i = 0; i < 8; i++) {
            int rr = r + kn[i][0], cc = c + kn[i][1];
            if (inBounds(rr, cc) && st.board[rr][cc].type == PType::KNIGHT && st.board[rr][cc].color == by) return true;
        }
        // King attacks
        for (int dr = -1; dr <= 1; dr++)
            for (int dc = -1; dc <= 1; dc++) {
                if (dr == 0 && dc == 0) continue;
                int rr = r + dr, cc = c + dc;
                if (inBounds(rr, cc) && st.board[rr][cc].type == PType::KING && st.board[rr][cc].color == by) return true;
            }
        // Sliding: rook/queen (straight)
        static const int straight[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
        for (int i = 0; i < 4; i++) {
            int rr = r + straight[i][0], cc = c + straight[i][1];
            while (inBounds(rr, cc)) {
                const Piece& p = st.board[rr][cc];
                if (p.type != PType::EMPTY) {
                    if (p.color == by && (p.type == PType::ROOK || p.type == PType::QUEEN)) return true;
                    break;
                }
                rr += straight[i][0]; cc += straight[i][1];
            }
        }
        // Sliding: bishop/queen (diagonal)
        static const int diag[4][2] = {{1,1},{1,-1},{-1,1},{-1,-1}};
        for (int i = 0; i < 4; i++) {
            int rr = r + diag[i][0], cc = c + diag[i][1];
            while (inBounds(rr, cc)) {
                const Piece& p = st.board[rr][cc];
                if (p.type != PType::EMPTY) {
                    if (p.color == by && (p.type == PType::BISHOP || p.type == PType::QUEEN)) return true;
                    break;
                }
                rr += diag[i][0]; cc += diag[i][1];
            }
        }
        return false;
    }

    Pos findKing(const GameState& st, Color color) const {
        for (int r = 0; r < 8; r++)
            for (int c = 0; c < 8; c++)
                if (st.board[r][c].type == PType::KING && st.board[r][c].color == color)
                    return Pos{r, c};
        return Pos{-1, -1};
    }

    bool inCheck(const GameState& st, Color color) const {
        Pos kp = findKing(st, color);
        if (kp.r < 0) return false; // shouldn't happen
        return isAttacked(st, kp.r, kp.c, opponent(color));
    }

    // ---------------- Move generation ----------------
    void addSliding(const GameState& st, Move* out, int& count, int r, int c, Color color, const int dirs[][2], int numDirs) const {
        for (int i = 0; i < numDirs; i++) {
            int rr = r + dirs[i][0], cc = c + dirs[i][1];
            while (inBounds(rr, cc)) {
                const Piece& target = st.board[rr][cc];
                if (target.type == PType::EMPTY) {
                    if (count < MAX_GEN) {
                        Move m; m.fromR=r; m.fromC=c; m.toR=rr; m.toC=cc;
                        m.movedType = st.board[r][c].type; m.movedColor = color;
                        out[count++] = m;
                    }
                } else {
                    if (target.color != color) {
                        if (count < MAX_GEN) {
                            Move m; m.fromR=r; m.fromC=c; m.toR=rr; m.toC=cc;
                            m.movedType = st.board[r][c].type; m.movedColor = color;
                            m.capturedType = target.type;
                            out[count++] = m;
                        }
                    }
                    break;
                }
                rr += dirs[i][0]; cc += dirs[i][1];
            }
        }
    }

    // Fills out[] (capacity MAX_GEN) with pseudo-legal moves, returns count.
    int genPseudoMoves(const GameState& st, Color color, Move* out) const {
        int count = 0;
        for (int r = 0; r < 8; r++) {
            for (int c = 0; c < 8; c++) {
                const Piece& p = st.board[r][c];
                if (p.color != color) continue;
                switch (p.type) {
                    case PType::PAWN: {
                        int dir = (color == Color::WHITE) ? 1 : -1;
                        int startRow = (color == Color::WHITE) ? 1 : 6;
                        int lastRow = (color == Color::WHITE) ? 7 : 0;
                        static const PType promoOpts[4] = {PType::QUEEN, PType::ROOK, PType::BISHOP, PType::KNIGHT};
                        // single forward
                        int r1 = r + dir;
                        if (inBounds(r1, c) && st.board[r1][c].type == PType::EMPTY) {
                            if (r1 == lastRow) {
                                for (int pi = 0; pi < 4 && count < MAX_GEN; pi++) {
                                    Move m; m.fromR=r; m.fromC=c; m.toR=r1; m.toC=c;
                                    m.movedType = PType::PAWN; m.movedColor = color; m.promotion = promoOpts[pi];
                                    out[count++] = m;
                                }
                            } else {
                                if (count < MAX_GEN) {
                                    Move m; m.fromR=r; m.fromC=c; m.toR=r1; m.toC=c;
                                    m.movedType = PType::PAWN; m.movedColor = color;
                                    out[count++] = m;
                                }
                                int r2 = r + 2*dir;
                                if (r == startRow && st.board[r2][c].type == PType::EMPTY && count < MAX_GEN) {
                                    Move m2; m2.fromR=r; m2.fromC=c; m2.toR=r2; m2.toC=c;
                                    m2.movedType = PType::PAWN; m2.movedColor = color; m2.doubleStep = true;
                                    out[count++] = m2;
                                }
                            }
                        }
                        // captures
                        for (int dc = -1; dc <= 1; dc += 2) {
                            int rr = r + dir, cc = c + dc;
                            if (!inBounds(rr, cc)) continue;
                            const Piece& target = st.board[rr][cc];
                            if (target.type != PType::EMPTY && target.color != color) {
                                if (rr == lastRow) {
                                    for (int pi = 0; pi < 4 && count < MAX_GEN; pi++) {
                                        Move m; m.fromR=r; m.fromC=c; m.toR=rr; m.toC=cc;
                                        m.movedType = PType::PAWN; m.movedColor = color; m.promotion = promoOpts[pi];
                                        m.capturedType = target.type;
                                        out[count++] = m;
                                    }
                                } else if (count < MAX_GEN) {
                                    Move m; m.fromR=r; m.fromC=c; m.toR=rr; m.toC=cc;
                                    m.movedType = PType::PAWN; m.movedColor = color;
                                    m.capturedType = target.type;
                                    out[count++] = m;
                                }
                            } else if (target.type == PType::EMPTY && st.epRow != -1 && rr == st.epRow && cc == st.epCol && count < MAX_GEN) {
                                Move m; m.fromR=r; m.fromC=c; m.toR=rr; m.toC=cc;
                                m.movedType = PType::PAWN; m.movedColor = color;
                                m.enPassant = true; m.capturedType = PType::PAWN;
                                out[count++] = m;
                            }
                        }
                        break;
                    }
                    case PType::KNIGHT: {
                        static const int kn[8][2] = {{1,2},{2,1},{-1,2},{-2,1},{1,-2},{2,-1},{-1,-2},{-2,-1}};
                        for (int i = 0; i < 8; i++) {
                            int rr = r + kn[i][0], cc = c + kn[i][1];
                            if (!inBounds(rr, cc)) continue;
                            const Piece& target = st.board[rr][cc];
                            if ((target.type == PType::EMPTY || target.color != color) && count < MAX_GEN) {
                                Move m; m.fromR=r; m.fromC=c; m.toR=rr; m.toC=cc;
                                m.movedType = PType::KNIGHT; m.movedColor = color;
                                if (target.type != PType::EMPTY) m.capturedType = target.type;
                                out[count++] = m;
                            }
                        }
                        break;
                    }
                    case PType::BISHOP: {
                        static const int diag[4][2] = {{1,1},{1,-1},{-1,1},{-1,-1}};
                        addSliding(st, out, count, r, c, color, diag, 4);
                        break;
                    }
                    case PType::ROOK: {
                        static const int straight[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
                        addSliding(st, out, count, r, c, color, straight, 4);
                        break;
                    }
                    case PType::QUEEN: {
                        static const int all[8][2] = {{1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1}};
                        addSliding(st, out, count, r, c, color, all, 8);
                        break;
                    }
                    case PType::KING: {
                        for (int dr = -1; dr <= 1; dr++) {
                            for (int dc = -1; dc <= 1; dc++) {
                                if (dr == 0 && dc == 0) continue;
                                int rr = r + dr, cc = c + dc;
                                if (!inBounds(rr, cc)) continue;
                                const Piece& target = st.board[rr][cc];
                                if ((target.type == PType::EMPTY || target.color != color) && count < MAX_GEN) {
                                    Move m; m.fromR=r; m.fromC=c; m.toR=rr; m.toC=cc;
                                    m.movedType = PType::KING; m.movedColor = color;
                                    if (target.type != PType::EMPTY) m.capturedType = target.type;
                                    out[count++] = m;
                                }
                            }
                        }
                        // Castling
                        bool kingMoved = (color == Color::WHITE) ? st.wKingMoved : st.bKingMoved;
                        if (!kingMoved && !inCheck(st, color)) {
                            int homeRow = (color == Color::WHITE) ? 0 : 7;
                            if (r == homeRow && c == 4) {
                                bool rookHMoved = (color == Color::WHITE) ? st.wRookHMoved : st.bRookHMoved;
                                if (!rookHMoved && st.board[homeRow][7].type == PType::ROOK && st.board[homeRow][7].color == color
                                    && st.board[homeRow][5].type == PType::EMPTY && st.board[homeRow][6].type == PType::EMPTY
                                    && !isAttacked(st, homeRow, 5, opponent(color)) && !isAttacked(st, homeRow, 6, opponent(color))
                                    && count < MAX_GEN) {
                                    Move m; m.fromR=r; m.fromC=c; m.toR=homeRow; m.toC=6;
                                    m.movedType = PType::KING; m.movedColor = color; m.castleK = true;
                                    out[count++] = m;
                                }
                                bool rookAMoved = (color == Color::WHITE) ? st.wRookAMoved : st.bRookAMoved;
                                if (!rookAMoved && st.board[homeRow][0].type == PType::ROOK && st.board[homeRow][0].color == color
                                    && st.board[homeRow][1].type == PType::EMPTY && st.board[homeRow][2].type == PType::EMPTY && st.board[homeRow][3].type == PType::EMPTY
                                    && !isAttacked(st, homeRow, 2, opponent(color)) && !isAttacked(st, homeRow, 3, opponent(color))
                                    && count < MAX_GEN) {
                                    Move m; m.fromR=r; m.fromC=c; m.toR=homeRow; m.toC=2;
                                    m.movedType = PType::KING; m.movedColor = color; m.castleQ = true;
                                    out[count++] = m;
                                }
                            }
                        }
                        break;
                    }
                    default: break;
                }
            }
        }
        return count;
    }

    void applyMoveInPlace(GameState& st, const Move& m) const {
        Piece moving = st.board[m.fromR][m.fromC];
        bool wasPawnOrCapture = (moving.type == PType::PAWN) || (st.board[m.toR][m.toC].type != PType::EMPTY) || m.enPassant;

        if (m.enPassant) {
            st.board[m.fromR][m.toC] = Piece{PType::EMPTY, Color::NONE};
        }

        Piece placed = moving;
        if (m.promotion != PType::EMPTY) placed.type = m.promotion;

        st.board[m.toR][m.toC] = placed;
        st.board[m.fromR][m.fromC] = Piece{PType::EMPTY, Color::NONE};

        if (m.castleK || m.castleQ) {
            int homeRow = m.fromR;
            if (m.castleK) {
                st.board[homeRow][5] = st.board[homeRow][7];
                st.board[homeRow][7] = Piece{PType::EMPTY, Color::NONE};
            } else {
                st.board[homeRow][3] = st.board[homeRow][0];
                st.board[homeRow][0] = Piece{PType::EMPTY, Color::NONE};
            }
        }

        if (moving.type == PType::KING) {
            if (moving.color == Color::WHITE) st.wKingMoved = true; else st.bKingMoved = true;
        }
        if (moving.type == PType::ROOK) {
            if (moving.color == Color::WHITE) {
                if (m.fromR == 0 && m.fromC == 0) st.wRookAMoved = true;
                if (m.fromR == 0 && m.fromC == 7) st.wRookHMoved = true;
            } else {
                if (m.fromR == 7 && m.fromC == 0) st.bRookAMoved = true;
                if (m.fromR == 7 && m.fromC == 7) st.bRookHMoved = true;
            }
        }
        if (m.toR == 0 && m.toC == 0) st.wRookAMoved = true;
        if (m.toR == 0 && m.toC == 7) st.wRookHMoved = true;
        if (m.toR == 7 && m.toC == 0) st.bRookAMoved = true;
        if (m.toR == 7 && m.toC == 7) st.bRookHMoved = true;

        if (m.doubleStep) {
            st.epRow = (m.fromR + m.toR) / 2;
            st.epCol = m.fromC;
        } else {
            st.epRow = -1;
            st.epCol = -1;
        }

        if (wasPawnOrCapture) st.halfmoveClock = 0; else st.halfmoveClock++;

        if (moving.color == Color::BLACK) st.fullmoveNumber++;
        st.turn = opponent(moving.color);
    }

    // Fills out[] (capacity MAX_GEN) with legal moves, returns count.
    int genLegalMoves(const GameState& st, Color color, Move* out) const {
        Move pseudo[MAX_GEN];
        int pcount = genPseudoMoves(st, color, pseudo);
        int lcount = 0;
        for (int i = 0; i < pcount; i++) {
            GameState copy = st;
            applyMoveInPlace(copy, pseudo[i]);
            if (!inCheck(copy, color) && lcount < MAX_GEN) out[lcount++] = pseudo[i];
        }
        return lcount;
    }

    // ---------------- Notation ----------------
    // Decides whether a non-pawn move needs its origin file and/or rank written in front of the
    // destination square, per standard algebraic notation's disambiguation rule: only add what's
    // needed to tell this piece apart from another piece of the same type that could also
    // legally reach the same destination square.
    void disambiguation(const Move& m, const Move* legalMoves, int legalCount, bool& needFile, bool& needRank) const {
        needFile = false;
        needRank = false;
        bool sameFileExists = false, sameRankExists = false, otherExists = false;
        for (int i = 0; i < legalCount; i++) {
            const Move& o = legalMoves[i];
            if (o.movedType != m.movedType || o.movedColor != m.movedColor) continue;
            if (o.toR != m.toR || o.toC != m.toC) continue;
            if (o.fromR == m.fromR && o.fromC == m.fromC) continue; // this is the move itself
            otherExists = true;
            if (o.fromC == m.fromC) sameFileExists = true;
            if (o.fromR == m.fromR) sameRankExists = true;
        }
        if (!otherExists) return;
        if (!sameFileExists) { needFile = true; return; }        // file alone tells them apart
        if (!sameRankExists) { needRank = true; return; }        // file is shared, rank tells them apart
        needFile = true; needRank = true;                        // both shared with different others: need both
    }

    // Builds standard algebraic notation (SAN), e.g. "e4", "Nf3", "Bxc6", "exd6", "Nbd2",
    // "e8=Q", "0-0", "0-0-0", with "+" / "#" appended for check / checkmate.
    // legalMoves/legalCount must be the mover's legal moves generated BEFORE this move was applied,
    // so disambiguation can be computed correctly.
    void moveToNotation(const Move& m, bool isCheck, bool isMate, const Move* legalMoves, int legalCount, char* out) const {
        const char* suffix = isMate ? "#" : (isCheck ? "+" : "");
        if (m.castleK) { strcpy(out, "0-0"); strcat(out, suffix); return; }
        if (m.castleQ) { strcpy(out, "0-0-0"); strcat(out, suffix); return; }

        char toSq[3]; squareNameStr(m.toR, m.toC, toSq);
        bool isCapture = (m.capturedType != PType::EMPTY) || m.enPassant;

        out[0] = '\0';
        if (m.movedType == PType::PAWN) {
            // Pawns never get a piece letter. A capture is written as the origin file, then 'x',
            // then the destination square (e.g. "exd5"). A quiet move is just the destination.
            if (isCapture) {
                char fromFile[2] = { char('a' + m.fromC), '\0' };
                strcat(out, fromFile);
                strcat(out, "x");
            }
            strcat(out, toSq);
            if (m.promotion != PType::EMPTY) {
                char promoLetter[2] = { (char)toupper(promoToChar(m.promotion)), '\0' };
                strcat(out, "=");
                strcat(out, promoLetter);
            }
        } else {
            char pl[2]; pieceLetterStr(m.movedType, pl); // always letters: K/Q/R/B/N, per standard notation
            strcat(out, pl);
            bool needFile = false, needRank = false;
            disambiguation(m, legalMoves, legalCount, needFile, needRank);
            if (needFile) { char f[2] = { char('a' + m.fromC), '\0' }; strcat(out, f); }
            if (needRank) { char rk[2] = { char('1' + m.fromR), '\0' }; strcat(out, rk); }
            if (isCapture) strcat(out, "x");
            strcat(out, toSq);
        }
        strcat(out, suffix);
    }

    // ---------------- Move input / execution ----------------
    void makeMoveMenu() {
        if (gameOver) { cout << "The game has ended. You can undo, save, or return to the main menu.\n"; return; }
        cout << "Enter move in coordinate form (e.g. e2e4, or e7e8q for promotion), or 'cancel': ";
        char line[64];
        readLine(line, sizeof(line));
        if (strcmp(line, "cancel") == 0 || line[0] == '\0') { cout << "Cancelled.\n"; return; }
        char err[128];
        if (!tryMakeMove(line, err)) {
            cout << "Illegal move: " << err << "\n";
        } else {
            printBoard();
        }
    }

    bool tryMakeMove(const char* input, char* err) {
        if (!gameActive) { strcpy(err, "no game in progress"); return false; }
        if (gameOver) { strcpy(err, "game has already ended"); return false; }

        char s[64];
        strncpy(s, input, sizeof(s) - 1);
        s[sizeof(s) - 1] = '\0';
        // strip any embedded whitespace
        {
            char cleaned[64];
            int j = 0;
            for (int i = 0; s[i] && j < (int)sizeof(cleaned) - 1; i++)
                if (!isspace((unsigned char)s[i])) cleaned[j++] = s[i];
            cleaned[j] = '\0';
            strcpy(s, cleaned);
        }
        if (strlen(s) < 4) { strcpy(err, "format should be like e2e4"); return false; }

        int fromC = tolower((unsigned char)s[0]) - 'a';
        int fromR = s[1] - '1';
        int toC = tolower((unsigned char)s[2]) - 'a';
        int toR = s[3] - '1';
        PType promo = PType::EMPTY;
        if (strlen(s) >= 5) promo = charToPromo(s[4]);

        if (!inBounds(fromR, fromC) || !inBounds(toR, toC)) { strcpy(err, "square off board"); return false; }

        Move legal[MAX_GEN];
        int n = genLegalMoves(state, state.turn, legal);
        Move* found = nullptr;
        for (int i = 0; i < n; i++) {
            Move& m = legal[i];
            if (m.fromR == fromR && m.fromC == fromC && m.toR == toR && m.toC == toC) {
                if (m.promotion != PType::EMPTY) {
                    PType want = (promo != PType::EMPTY) ? promo : PType::QUEEN; // default to queen
                    if (m.promotion == want) { found = &m; break; }
                } else {
                    found = &m; break;
                }
            }
        }
        if (!found) { strcpy(err, "not a legal move"); return false; }

        if (undoTop >= MAX_HISTORY) { strcpy(err, "move history limit reached"); return false; }
        undoStack[undoTop++] = state;

        Move applied = *found;
        applyMoveInPlace(state, applied);
        if (moveCount < MAX_HISTORY) moveList[moveCount] = applied;

        // Update stats
        stats.totalMoves++;
        if (applied.capturedType != PType::EMPTY || applied.enPassant) {
            if (applied.movedColor == Color::WHITE) stats.whiteCaptures++; else stats.blackCaptures++;
        }
        if (applied.enPassant) stats.enPassantCaptures++;
        if (applied.castleK || applied.castleQ) stats.castles++;
        if (applied.promotion != PType::EMPTY) stats.promotions++;

        Color nowTurn = state.turn; // opponent of the mover
        bool chk = inCheck(state, nowTurn);
        Move replies[MAX_GEN];
        int rcount = genLegalMoves(state, nowTurn, replies);
        bool mate = chk && rcount == 0;
        bool stalemate = !chk && rcount == 0;
        if (chk) stats.checksGiven++;

        char notation[32];
        moveToNotation(applied, chk, mate, legal, n, notation);
        if (moveCount < MAX_HISTORY) { strncpy(notationHistory[moveCount], notation, 31); notationHistory[moveCount][31] = '\0'; }
        moveCount++;

        cout << "Move played: " << notation << "\n";

        if (mate) {
            cout << "CHECKMATE! " << (applied.movedColor == Color::WHITE ? "White" : "Black") << " wins!\n";
            gameOver = true;
        } else if (stalemate) {
            cout << "STALEMATE! The game is a draw.\n";
            gameOver = true;
        } else if (chk) {
            cout << ((nowTurn == Color::WHITE) ? "White" : "Black") << " is in check!\n";
        }
        if (!gameOver && state.halfmoveClock >= 100) {
            cout << "50-move rule reached. The game is a draw.\n";
            gameOver = true;
        }
        return true;
    }

    void undoMove() {
        if (undoTop <= 0) { cout << "Nothing to undo.\n"; return; }
        state = undoStack[--undoTop];
        if (moveCount > 0) moveCount--;
        gameOver = false; // in case game had ended
        cout << "Move undone.\n";
        printBoard();
    }

    void showHistory() {
        if (moveCount == 0) { cout << "No moves played yet.\n"; return; }
        cout << "\n--- Move History ---\n";
        for (int i = 0; i < moveCount; i++) {
            if (i % 2 == 0) cout << (i / 2 + 1) << ". ";
            cout << notationHistory[i] << "  ";
            if (i % 2 == 1) cout << "\n";
        }
        cout << "\n";
    }

    void showStatistics() {
        cout << "\n--- Game Statistics ---\n";
        cout << "Total half-moves played : " << stats.totalMoves << "\n";
        cout << "White captures          : " << stats.whiteCaptures << "\n";
        cout << "Black captures          : " << stats.blackCaptures << "\n";
        cout << "Checks given             : " << stats.checksGiven << "\n";
        cout << "Castles performed        : " << stats.castles << "\n";
        cout << "Pawn promotions          : " << stats.promotions << "\n";
        cout << "En passant captures      : " << stats.enPassantCaptures << "\n";

        int whiteMaterial = 0, blackMaterial = 0;
        for (int r = 0; r < 8; r++) {
            for (int c = 0; c < 8; c++) {
                int v = pieceValue(state.board[r][c].type);
                if (state.board[r][c].color == Color::WHITE) whiteMaterial += v;
                else if (state.board[r][c].color == Color::BLACK) blackMaterial += v;
            }
        }
        cout << "White material           : " << whiteMaterial << "\n";
        cout << "Black material           : " << blackMaterial << "\n";
        cout << "Material balance         : " << (whiteMaterial - blackMaterial) << " (positive favors White)\n";
    }

    int pieceValue(PType t) const {
        switch (t) {
            case PType::PAWN: return 1;
            case PType::KNIGHT: return 3;
            case PType::BISHOP: return 3;
            case PType::ROOK: return 5;
            case PType::QUEEN: return 9;
            default: return 0;
        }
    }

    void showLegalMovesMenu() {
        cout << "Enter square to inspect (e.g. e2): ";
        char line[16];
        readLine(line, sizeof(line));
        if (strlen(line) < 2) { cout << "Invalid square.\n"; return; }
        int c = tolower((unsigned char)line[0]) - 'a';
        int r = line[1] - '1';
        if (!inBounds(r, c)) { cout << "Invalid square.\n"; return; }
        if (state.board[r][c].type == PType::EMPTY) { cout << "That square is empty.\n"; return; }
        if (state.board[r][c].color != state.turn) { cout << "That piece belongs to the other side.\n"; return; }

        Move legal[MAX_GEN];
        int n = genLegalMoves(state, state.turn, legal);
        cout << "Legal destinations from " << line << ": ";
        bool any = false;
        for (int i = 0; i < n; i++) {
            const Move& m = legal[i];
            if (m.fromR == r && m.fromC == c) {
                char sq[3]; squareNameStr(m.toR, m.toC, sq);
                cout << sq;
                if (m.promotion != PType::EMPTY) cout << "(" << (char)toupper(promoToChar(m.promotion)) << ")";
                cout << " ";
                any = true;
            }
        }
        if (!any) cout << "(none)";
        cout << "\n";
    }

    void resignOrDraw() {
        cout << "Type 'resign' to resign, 'draw' to declare a draw, or anything else to cancel: ";
        char line[16];
        readLine(line, sizeof(line));
        toLowerInPlace(line);
        if (strcmp(line, "resign") == 0) {
            cout << ((state.turn == Color::WHITE) ? "White" : "Black") << " resigns. "
                 << ((state.turn == Color::WHITE) ? "Black" : "White") << " wins!\n";
            gameOver = true;
        } else if (strcmp(line, "draw") == 0) {
            cout << "The game is declared a draw by agreement.\n";
            gameOver = true;
        } else {
            cout << "Cancelled.\n";
        }
    }

    // ---------------- Save / Load ----------------
    void saveGameMenu() {
        cout << "Enter filename to save (e.g. mygame.chess): ";
        char fname[256];
        readLine(fname, sizeof(fname));
        if (fname[0] == '\0') { cout << "Cancelled.\n"; return; }
        saveGame(fname);
    }

    void saveGame(const char* filename) {
        ofstream f(filename, ios::out | ios::trunc);
        if (!f) { cout << "Could not open file for writing: " << filename << "\n"; return; }
        f << "CHESSSAVE1\n";
        for (int i = 0; i < moveCount; i++) {
            const Move& m = moveList[i];
            char from[3]; squareNameStr(m.fromR, m.fromC, from);
            char to[3]; squareNameStr(m.toR, m.toC, to);
            f << from << to;
            if (m.promotion != PType::EMPTY) f << promoToChar(m.promotion);
            f << "\n";
        }
        f.close();
        cout << "Game saved to " << filename << " (" << moveCount << " moves).\n";
    }

    void loadGameMenu() {
        cout << "Enter filename to load (e.g. mygame.chess): ";
        char fname[256];
        readLine(fname, sizeof(fname));
        if (fname[0] == '\0') { cout << "Cancelled.\n"; return; }
        loadGame(fname);
    }

    bool loadGame(const char* filename) {
        ifstream f(filename, ios::in);
        if (!f) { cout << "Could not open file: " << filename << "\n"; return false; }

        char header[64];
        if (!f.getline(header, sizeof(header))) { cout << "Empty or invalid save file.\n"; return false; }
        trimInPlace(header);
        if (strcmp(header, "CHESSSAVE1") != 0) { cout << "Unrecognized save file format.\n"; return false; }

        initBoard(state);
        undoTop = 0;
        moveCount = 0;
        stats = Stats();
        gameActive = true;
        gameOver = false;

        char line[64];
        int applied = 0;
        while (f.getline(line, sizeof(line))) {
            trimInPlace(line);
            if (line[0] == '\0') continue;
            char err[128];
            if (!tryMakeMove(line, err)) {
                cout << "Warning: could not apply move '" << line << "' from save file (" << err << "). Stopping replay.\n";
                break;
            }
            applied++;
        }
        cout << "Loaded " << filename << ": " << applied << " moves replayed successfully.\n";
        printBoard();
        return true;
    }
};

int main() {
    setupUnicodeConsole();
    ChessGame game;
    game.run();
    return 0;
}
