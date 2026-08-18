#include <iostream>
#include <fstream>

#ifdef _WIN32
#include <windows.h> // needed for SetConsoleOutputCP
#endif
static void setupUnicodeConsole() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
}


using namespace std;

enum class PieceType { EMPTY, PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING }; //initialize the types of pieces
enum class Color { NONE, WHITE, BLACK }; //pieces color

struct Piece // what a piece is made up of
{
    //default states
    PieceType type = PieceType::EMPTY;
    Color color = Color::NONE;
    bool hasMoved = false;
};

struct Move // what a move contains
{
    int fromrow, fromcol, torow, tocol;
    Piece movedPiece;
    Piece capturedPiece = {PieceType::EMPTY, Color::NONE};
    PieceType promotedTo = PieceType::EMPTY;

    bool isEnPassantCapture = false;   // captured pawn wasn't on the 'to' square
    bool isCastleKingside = false;
    bool isCastleQueenside = false;
    bool isPawnDoubleStep = false;     // enables en passant on the *next* move
};

Piece board[8][8]; // the array for the pieces in the board
const int MAX_MOVES = 1000;
Move moveHistory[MAX_MOVES];
int moveCount = 0;
int turn = 0;

char pieceChar(const Piece& p) // return the specific character for each pieces (also note to self "const" is just to make sure it dosent change the original object)
{
    char c;
    switch (p.type)
    {
    default: return ' '; // empty spaces for no pieces
    case PieceType::PAWN:
        c = 'p'; break;
    case PieceType::KNIGHT:
        c = 'n'; break;
    case PieceType::BISHOP:
        c = 'b'; break;
    case PieceType::ROOK:
        c = 'r'; break;
    case PieceType::QUEEN:
        c = 'q'; break;
    case PieceType::KING:
        c = 'k'; break;
    }

    if (p.color == Color::WHITE) // uppercase for white side pieces
        c = toupper(c);

    return c;
}

PieceType choosePromotionPiece()
{
    cout << "Pawn promotion! Choose a piece (Q/R/B/N): ";
    char choice;
    cin >> choice;
    choice = toupper(choice);
    switch (choice)
    {
    case 'R': return PieceType::ROOK;
    case 'B': return PieceType::BISHOP;
    case 'N': return PieceType::KNIGHT;
    default:  return PieceType::QUEEN;
    }
}

void setupStartPos()
{
    PieceType backRank[8] = {
        PieceType::ROOK, PieceType::KNIGHT, PieceType::BISHOP, PieceType::QUEEN,
        PieceType::KING, PieceType::BISHOP, PieceType::KNIGHT, PieceType::ROOK };

    for (int c = 0; c < 8; c++)
    {
        //give pieces to white side
        board[0][c] = Piece{ backRank[c], Color::WHITE };
        board[1][c] = Piece{ PieceType::PAWN, Color::WHITE };

        //give pieces to black side
        board[7][c] = Piece{ backRank[c], Color::BLACK };
        board[6][c] = Piece{ PieceType::PAWN, Color::BLACK };
    }
}

void printBoard(void)
{
    //top border ┌───┬───┬───┬───┬───┬───┬───┬───┐
    cout << "  ┌";

    for (int i = 0; i < 7; i++)
        cout << "───" << "┬";

    cout << "───" << "┐\n";

    //main segments of the board | p | p | p | p | p | p | p | p |
    for (int r = 7; r >= 0; r--)
    {
        cout << r + 1 << " |";

        for (int c = 0; c < 8; c++)
            cout << " " << pieceChar(board[r][c]) << " |";

        cout << '\n';

        //middle separator ├───┼───┼───┼───┼───┼───┼───┼───┤
        if (r > 0)
        {
            cout << "  ├";

            for (int i = 0; i < 7; i++)
                cout << "───┼";

            cout << "───┤\n";
        }
    }

    // bottom border └───┴───┴───┴───┴───┴───┴───┴───┘
    cout << "  └";

    for (int i = 0; i < 7; i++)
        cout << "───┴";

    cout << "───┘\n";

    // bottom file labels
    cout << "    a   b   c   d   e   f   g   h\n";
}

bool checkRange(char file, char rank, int& outrow, int& outcol) // check whether the input is within the range of the board, and returns the actual position of the piece in the board[][] coordinate;
{
    int col = file - 'a';
    int row = rank - '1';

    if (col < 0 || col > 7 || row < 0 || row > 7)
        return false;

    outrow = row;
    outcol = col;
    return true;
}

string squareName(int row, int col) // converts back the integer coordinate such as 07 back to a8
{
    string s;
    s += char('a' + col);
    s += char('1' + row);
    return s;
}

string moveToNotation(const Move& m) // generates the formal chess notation such as "Nf3", "e4" etc
{
    if (m.isCastleKingside)
        return "0-0";
    if (m.isCastleKingside)
        return "0-0-0";

    string result;
    char pieceLetter = ' ';
    switch (m.movedPiece.type)
    {
        case PieceType::KNIGHT: pieceLetter = 'N'; break;
        case PieceType::BISHOP: pieceLetter = 'B'; break;
        case PieceType::ROOK:   pieceLetter = 'R'; break;
        case PieceType::QUEEN:  pieceLetter = 'Q'; break;
        case PieceType::KING:   pieceLetter = 'K'; break;
        default: break; // pawn: no letter
    }

    if (pieceLetter != ' ') result += pieceLetter; // first letter shows the type of piece

    bool isCapture = (m.capturedPiece.type != PieceType::EMPTY) || m.isEnPassantCapture; // check whether there is a capture in the move

    if (m.movedPiece.type == PieceType::PAWN && isCapture)
        result += char('a' + m.fromcol); // pawn captures show the origin file
    
    if (isCapture) result += "x"; // x represents capture

    result += squareName(m.torow, m.tocol); // adds the destination square

    if (m.promotedTo != PieceType::EMPTY) // check whether the move had made a promotion. esult will be just destination and promotion type in the result
    {
        char promoLetter = ' ';
        switch (m.promotedTo)
        {
        case PieceType::QUEEN:  promoLetter = 'Q'; break;
        case PieceType::ROOK:   promoLetter = 'R'; break;
        case PieceType::BISHOP: promoLetter = 'B'; break;
        case PieceType::KNIGHT: promoLetter = 'N'; break;
        default: break;
        }
        result += '=';
        result += promoLetter;
    }

    return result;
}


void displayHistory()//display the move history eg 1. f3 e5 2. g4 Qh4#. 
{
    if (moveCount == 0) 
    {
        cout << "No moves played yet.\n"; 
        return; 
    }

    for (int i = 0; i < moveCount; i++)
    {
        if (i % 2 == 0) // a single turn consists of displaying both white and black moves
            cout << (i / 2 + 1) << ". " << moveToNotation(moveHistory[i]) << "  ";
        else cout << moveToNotation(moveHistory[i]) << "\n";
    }
    if (moveCount % 2 == 1) cout << "\n"; // 2 turns in total for a single line of turn notation
}

char promoToChar(PieceType t)
{
    switch (t)
    {
    case PieceType::ROOK:   return 'r';
    case PieceType::BISHOP: return 'b';
    case PieceType::KNIGHT: return 'n';
    default:                return 'q';
    }
}
 
PieceType charToPromo(char c)
{
    switch (toupper(c))
    {
    case 'R': return PieceType::ROOK;
    case 'B': return PieceType::BISHOP;
    case 'N': return PieceType::KNIGHT;
    default:  return PieceType::QUEEN;
    }
}


//function declaration for the tryMakeMove()
void applyMove(int fromrow, int fromcol, int torow, int tocol, PieceType forcedPromotion);
bool isLegalPieceMove(const Piece& piece, int fromrow, int fromcol, int torow, int tocol, int turn);
bool isColorMove(const Piece& piece, int& turn);
bool isKingCheck(int turn);

bool tryMakeMove(int fromrow, int fromcol, int torow, int tocol, PieceType forcedPromotion = PieceType::EMPTY)
{
    Piece frompiece = board[fromrow][fromcol];
    Piece destpiece = board[torow][tocol];
    Piece epCapturedPiece = board[fromrow][tocol];
 
    if (frompiece.type == PieceType::EMPTY)
        {
            cout << "There is no piece on that square.\n";
            return false;
        }

        if (!isLegalPieceMove(frompiece, fromrow, fromcol, torow, tocol, turn))
        {
            cout << "That is not a legal move.\n";
            return false;
        }

        if (!isColorMove(frompiece, turn))
        {
            return false;
        }
 
    int oldMoveCount = moveCount;
    applyMove(fromrow, fromcol, torow, tocol, forcedPromotion);
 
    if(isKingCheck(turn)) // check if the king is still in check after the current player's move
        {
            board[fromrow][fromcol] = frompiece;
            board[torow][tocol] = destpiece; //undo move as king is still in check
            board[fromrow][tocol] = epCapturedPiece; // undo move if it was en passant capture move
            printBoard();
            moveCount = oldMoveCount;
            if(turn % 2 == 0)
            {
                cout << "The White king is still in check.\n";
                return false;
            }
            else
            {
                cout << "The Black king is still in check.\n";
                return false;
            }
        }
 
    turn++;
    return true;
}

void undoMove()
{
    if (moveCount == 0) 
    {
        cout << "Nothing to undo.\n";
        return;
    }
 
    int targetCount = moveCount - 1;
    Move replayMoves[MAX_MOVES];
    for (int i = 0; i < targetCount; i++) 
        replayMoves[i] = moveHistory[i];
 
    for (int r = 0; r < 8; r++) 
        for (int c = 0; c < 8; c++) 
            board[r][c] = Piece{}; //re-setup the board
            setupStartPos();
            moveCount = 0;
            turn = 0;
 
    for (int i = 0; i < targetCount; i++) //redo all the previous moves
        tryMakeMove(replayMoves[i].fromrow, replayMoves[i].fromcol, replayMoves[i].torow, replayMoves[i].tocol, replayMoves[i].promotedTo);
 
    cout << "Undid the last move.\n";
    printBoard();
}

void saveGame(const string& filename)
{
    ofstream f(filename);
    if (!f) { cout << "Could not open file for writing: " << filename << "\n"; return; }
 
    f << "CHESSSAVE1\n"; //as the file format
    for (int i = 0; i < moveCount; i++)
    {
        const Move& m = moveHistory[i]; //saves a move by outputting eg "e2e3" or "e7e8Q" if there is a promotion
        f << squareName(m.fromrow, m.fromcol) << squareName(m.torow, m.tocol);
        if (m.promotedTo != PieceType::EMPTY) 
            f << promoToChar(m.promotedTo);
        f << "\n"; //one line for one move
    }
    cout << "Game saved to " << filename << " (" << moveCount << " moves).\n";
}

void loadGame(const string& filename)
{
    ifstream f(filename);
    if (!f) 
    {
        cout << "Could not open file: " << filename << "\n";
        return;
    } // returns error that it cant find the file
 
    string header;
    getline(f, header);
    if (header != "CHESSSAVE1") { cout << "Unrecognized save file format.\n"; return; } // returns error that its an unknown file type
 
    // Reset to a fresh game before replaying.
    for (int r = 0; r < 8; r++) 
        for (int c = 0; c < 8; c++) 
            board[r][c] = Piece{};
            setupStartPos();
            moveCount = 0;
            turn = 0;
 
    string line;
    int nummoves = 0;
    while (getline(f, line)) //reads every line of the file until it cant read any more lines
    {
        if (line.empty()) 
            continue;

        if (line.size() != 4 && line.size() != 5) //check whether it is a valid move stored eg "e2e3"  or "e7e8Q"
        {
            cout << "Warning: malformed line '" << line << "' in save file. Stopping replay.\n";
            break;
        }
 
        int fromrow, fromcol, torow, tocol;
        if (!checkRange(line[0], line[1], fromrow, fromcol) || !checkRange(line[2], line[3], torow, tocol)) //check whether the moves in the file is a valid range, something like "z9" will be rejected
        {
            cout << "Warning: invalid square in '" << line << "'. Stopping replay.\n";
            break;
        }
 
        PieceType forcedPromotion;
        if((line.size() == 5))
            charToPromo(line[4]);
        else
            PieceType::EMPTY;
 
        if (!tryMakeMove(fromrow, fromcol, torow, tocol, forcedPromotion))
        {
            cout << "Warning: move '" << line << "' from save file is illegal. Stopping replay.\n";
            break;
        }
        nummoves++;
    }
 
    cout << "Loaded " << filename << ": " << nummoves << " moves replayed successfully.\n";
    printBoard();
}

bool readMove(int& fromrow, int& fromcol, int& torow, int& tocol)
{
    cout << "Enter move (e.g. e2e4), 'history', 'undo', 'save <file>' or 'load <file>: ";
    string input;
    cin >> input;

    if (input == "history")
    {
        displayHistory();
        return readMove(fromrow, fromcol, torow, tocol);
    }
    if (input == "undo")
    {
        undoMove();
        return readMove(fromrow, fromcol, torow, tocol);
    }
    if (input == "save")
    {
        string filename; cin >> filename;
        saveGame(filename);
        return readMove(fromrow, fromcol, torow, tocol);
    }
    if (input == "load")
    {
        string filename; cin >> filename;
        loadGame(filename);
        return readMove(fromrow, fromcol, torow, tocol);
    }
    
    if (input.size() != 4)
        return false;
    if (!checkRange(input[0], input[1], fromrow, fromcol))
        return false;
    if (!checkRange(input[2], input[3], torow, tocol))
        return false;

    return true;
}

void movePieceRaw(int fromrow, int fromcol, int torow, int tocol)
{
    board[torow][tocol] = board[fromrow][fromcol];
    board[torow][tocol].hasMoved = true;
    board[fromrow][fromcol] = Piece{};
}

void applyMove(int fromrow, int fromcol, int torow, int tocol, PieceType forcedPromotion = PieceType::EMPTY) // used in main function to move the pieces, and store the move history on each turn
{
    Piece frompiece = board[fromrow][fromcol];
    Piece destpiece = board[torow][tocol];

    Move m;
    m.fromrow = fromrow;
    m.fromcol = fromcol;
    m.torow = torow;
    m.tocol = tocol;
    m.movedPiece = {frompiece.type, frompiece.color};
    m.capturedPiece = {destpiece.type, destpiece.color};

    if(frompiece.type == PieceType::PAWN && abs(torow - fromrow) == 2) //check if the current pawn had moved 2 steps, if so, it can be en passant'ed
        m.isPawnDoubleStep = true;

    if(frompiece.type == PieceType::PAWN && fromcol != tocol && destpiece.type == PieceType::EMPTY) // en passant move
    {
        m.isEnPassantCapture = true;
        Piece& capturedPawn = board[fromrow][tocol]; //captured pawn is at the same row of the "from" row, and same column of the destination column
        m.capturedPiece = {capturedPawn.type, capturedPawn.color};
        capturedPawn = Piece{}; // remove the captured pawn from the board
    }

    if(frompiece.type == PieceType::KING && fromrow == torow && abs(fromcol - tocol) == 2)
    {
        if(tocol > fromcol) //kingside
        {
            m.isCastleKingside = true;
            movePieceRaw(fromrow, 7, torow, 5); //moves the rook
        }
        else
        {
            m.isCastleQueenside = true;
            movePieceRaw(fromrow, 0, torow, 3);
        }

    }
    
    movePieceRaw(fromrow, fromcol, torow, tocol);
    
    if (frompiece.type == PieceType::PAWN && ((frompiece.color == Color::WHITE && torow == 7) || (frompiece.color == Color::BLACK && torow == 0))) // if it is white pawn, promotion happens at top row, and bottom row for black
    {
        PieceType chosen;
        if((forcedPromotion != PieceType::EMPTY)) //if it is from a save file, instanly promotes the piece without askin
            chosen = forcedPromotion;
        else
            choosePromotionPiece();

        board[torow][tocol].type = chosen;
        m.promotedTo = chosen;
    }
    
    if(moveCount < MAX_MOVES)
        moveHistory[moveCount++] = m;
    else
        cout << "Move history full, cannot record further moves.\n";
}

bool isEnPassant(Color color, int fromrow, int fromcol, int torow, int tocol)
{
    if (moveCount == 0)
        return false;
    const Move& last = moveHistory[moveCount - 1];

    if(!last.isPawnDoubleStep)
        return false;
    if(last.torow != fromrow) // captured pawn must be in the same row before moving
            return false;
    if(last.tocol != tocol) // moving pawn must be at the same column as the captured pawn after capturing
        return false;
    if(abs(last.tocol - fromcol) != 1) // capturing from adjacent column
        return false;
    
    int direction;
    if(color == Color::WHITE)
        direction = 1; // only moves upward
    else
        direction = -1;
    
    if(torow - fromrow == direction)
        return true;

    return false;
}

bool isPawnMove(Color color, int fromrow, int fromcol, int torow, int tocol)
{
    int direction, startrow;

    if(color == Color::WHITE)
    {
        direction = 1; // only moves upward
        startrow = 1;
    }
    else
    {
        direction = -1; // only move downwards for black
        startrow = 6;
    }

    int dc = tocol - fromcol;
    int dr = torow - fromrow;

    if(dc == 0) //move without capturing
    {
        if (dr == direction && board[torow][tocol].type == PieceType::EMPTY) //move 1 step
            return true;

        if (dr == 2 * direction && board[torow][tocol].type == PieceType::EMPTY && board[fromrow + direction][fromcol].type == PieceType::EMPTY && fromrow == startrow) //move 2 steps from the starting row only without pieces blocking
            return true;

        return false;
    }

    if(abs(dc) == 1 && dr == direction) //capture piece
    {
        const Piece& dest = board[torow][tocol];
        if(dest.type != PieceType::EMPTY) //normal capture
            return true;
    }

    return isEnPassant(color, fromrow, fromcol, torow, tocol);

    return false;
}

bool isKnightMove(int fromrow, int fromcol, int torow, int tocol)
{
    int dc = abs(tocol - fromcol);
    int dr = abs(torow - fromrow);

    return (dc == 1 && dr == 2) || (dc == 2 && dr == 1);
}

bool isBishopMove(int fromrow, int fromcol, int torow, int tocol)
{
    int dc = tocol - fromcol;
    int dr = torow - fromrow;

    if (abs(dr) != abs(dc)) // x and y direction must be same
        return false;

    int stepcol, steprow;
    if (dc > 0) stepcol = 1;
    else stepcol = -1;
    if (dr > 0) steprow = 1;
    else steprow = -1;

    while (abs(steprow) != abs(dr)) // check whether there are pieces blocking the move
    {
        if (board[fromrow + steprow][fromcol + stepcol].type != PieceType::EMPTY)
            return false;

        if (steprow > 0) steprow++;
        else steprow--;
        if (stepcol > 0) stepcol++;
        else stepcol--;
    }

    return true;
}

bool isRookMove(int fromrow, int fromcol, int torow, int tocol)
{
    int dc = tocol - fromcol;
    int dr = torow - fromrow;

    if (!((dr != 0) && dc == 0 || (dc != 0) && dr == 0)) //check if its only moving through the colomn or the row
        return false;

    int step;
    if (dc > 0 || dr > 0) step = 1; //check direciton
    else step = -1;

    if (dr == 0)
    {
        while (abs(step) != abs(dc)) // check whether there are pieces blocking the move
        {
            if (board[fromrow][fromcol + step].type != PieceType::EMPTY)
                return false;

            if (step > 0) step++;
            else step--;
        }
    }

    if (dc == 0)
    {
        while (abs(step) != abs(dr))
        {
            if (board[fromrow + step][fromcol].type != PieceType::EMPTY)
                return false;

            if (step > 0) step++;
            else step--;
        }
    }

    return true;
}

bool isQueenMove(int fromrow, int fromcol, int torow, int tocol)
{
    return isBishopMove(fromrow, fromcol, torow, tocol) || isRookMove(fromrow, fromcol, torow, tocol);
}

bool isKingMove(int fromrow, int fromcol, int torow, int tocol)
{
    int dc = tocol - fromcol;
    int dr = torow - fromrow;

    if (abs(dr) > 1 || abs(dc) > 1)
        return false;

    return true;
}


//function call for the isCastling()
bool isSquareAttacked(int torow, int tocol, int turn);
bool isKingCheck(int turn);

bool isCastling(const Piece& piece, int fromrow, int fromcol, int torow, int tocol, int turn)
{
    if (piece.type != PieceType::KING) // only the king can castle
        return false;
    if (fromrow != torow || abs(tocol - fromcol) != 2) // King must move exactly two squares horizontally
        return false;
    if (piece.hasMoved) // King must not have moved before
        return false;
    if (isKingCheck(turn))
        return false;

    bool kingside = (tocol > fromcol); //true = kingside, false = queenside
    
    int rookcol;
    if(kingside)
        rookcol = 7;
    else
        rookcol = 0;

    if (board[fromrow][rookcol].type != PieceType::ROOK) //must be rook
        return false;
    if (board[fromrow][rookcol].color != piece.color) //must be same color as king
        return false;
    if (board[fromrow][rookcol].hasMoved) //must not have moved
        return false;

    if(kingside)
    {
        if(board[fromrow][5].type != PieceType::EMPTY || board[fromrow][6].type != PieceType::EMPTY) //the 2 squares must be empty
            return false;
        if(isSquareAttacked(fromrow, 5, turn) || isSquareAttacked(fromrow, 6, turn)) // king dosent moves through squares that can be attacked
            return false;
    }
    else
    {
        if(board[fromrow][1].type != PieceType::EMPTY || board[fromrow][2].type != PieceType::EMPTY || board[fromrow][3].type != PieceType::EMPTY) //the 3 squares must be empty
            return false;
        if(isSquareAttacked(fromrow, 2, turn) || isSquareAttacked(fromrow, 3, turn)) // king dosent moves through squares that can be attacked
            return false;
    }
    
    return true;
}

bool isLegalPieceMove(const Piece& piece, int fromrow, int fromcol, int torow, int tocol, int turn)
{
    if (fromrow == torow && fromcol == tocol) //moving to the same place
        return false;

    const Piece& dest = board[torow][tocol];
    if (dest.color == piece.color) // capturing same color piece
        return false;

    if(piece.type == PieceType::KING && abs(tocol - fromcol) == 2) //castling
    {
        return isCastling(piece, fromrow, fromcol, torow, tocol,turn);
    }

    switch (piece.type)
    {
    default:
        return false;

    case PieceType::PAWN:
        return isPawnMove(piece.color, fromrow, fromcol, torow, tocol);
    case PieceType::KNIGHT:
        return isKnightMove(fromrow, fromcol, torow, tocol);
    case PieceType::BISHOP:
        return isBishopMove(fromrow, fromcol, torow, tocol);
    case PieceType::ROOK:
        return isRookMove(fromrow, fromcol, torow, tocol);
    case PieceType::QUEEN:
        return isQueenMove(fromrow, fromcol, torow, tocol);
    case PieceType::KING:
        return isKingMove(fromrow, fromcol, torow, tocol);
    }
}

bool isSquareAttacked(int torow, int tocol,int turn)
{
    Color attackerColor;

    if (turn % 2 == 0)
        attackerColor = Color::BLACK;
    else
        attackerColor = Color::WHITE;

    for (int r = 0; r < 8; r++)
    {
        for (int c = 0; c < 8; c++)
        {
            if (board[r][c].color != attackerColor)
                continue;

            PieceType type = board[r][c].type;

            switch (type)
            {
            case PieceType::PAWN: //cant use isPawnMove() as it dosent detect attacking empty squares
            {
                int direction;

                if (attackerColor == Color::WHITE)
                    direction = 1;
                else
                    direction = -1;

                int dr = torow - r;
                int dc = tocol - c;

                if (dr == direction && abs(dc) == 1)
                    return true;

                break;
            }

            case PieceType::KNIGHT:
                if (isKnightMove(r, c, torow, tocol))
                    return true;
                break;

            case PieceType::BISHOP:
                if (isBishopMove(r, c, torow, tocol))
                    return true;
                break;

            case PieceType::ROOK:
                if (isRookMove(r, c, torow, tocol))
                    return true;
                break;

            case PieceType::QUEEN:
                if (isQueenMove(r, c, torow, tocol))
                    return true;
                break;

            case PieceType::KING:
                if (isKingMove(r, c, torow, tocol))
                    return true;
                break;

            default:
                break;
            }
        }
    }

    return false;
}

bool isColorMove(const Piece& piece, int& turn)
{
    int validcolor = turn % 2; // 0 = white's turn, 1 = black's turn

    if (validcolor == 0)
    {
        if (piece.color == Color::WHITE) //check whether the selected piece is white
            return true;
        else
        {
            cout << "It is white's turn.\n";
            return false;
        }
    }
    else
    {
        if (piece.color == Color::BLACK) //check whether the selected piece is black
            return true;
        else
        {
            cout << "It is black's turn.\n";
            return false;
        }
    }
}

bool isKingCheck(int turn)
{
    int validcolor = turn % 2; // 0 = white's turn, 1 = black's turn
    int kingrow, kingcol;
    Color kingColor;

    if (validcolor == 0)
    {
        kingColor = Color::WHITE;
        for (int r = 0; r < 8; r++)
        {
            for (int c = 0; c < 8; c++)
            {
                if (board[r][c].color == kingColor && board[r][c].type == PieceType::KING) //find the position of the king
                {
                    kingrow = r;
                    kingcol = c;
                }
            }
        }

        if(isSquareAttacked(kingrow, kingcol, turn)) // check if any pieces could attack the king
            return true;
    }
    else
    {
        kingColor = Color::BLACK;
        for (int r = 0; r < 8; r++)
        {
            for (int c = 0; c < 8; c++)
            {
                if (board[r][c].color == kingColor && board[r][c].type == PieceType::KING)
                {
                    kingrow = r;
                    kingcol = c;
                }
            }
        }

        if(isSquareAttacked(kingrow, kingcol, turn))
            return true;
    }

    return false;
}

bool isCheckmateStalemate(int turn)
{
    int validcolor = turn % 2; // 0 = white's turn, 1 = black's turn

    if (validcolor == 0)
    {
        for (int r = 0; r < 8; r++)
        {
            for (int c = 0; c < 8; c++)
            {
                for (int x = 0; x < 8; x++)
                {
                    for (int y = 0; y < 8; y++)
                    {
                        if (isLegalPieceMove(board[r][c], r, c, x, y, turn) && board[r][c].color == Color::WHITE) //checks all the possible moves for the player's turn to escape check
                        {
                            Piece frompiece = board[r][c]; // read the piece on the square thats about to move
                            Piece destpiece = board[x][y]; // read the destination piece on the square
                            
                            bool isEnPassant = (board[r][c].type == PieceType::PAWN && c != y && board[x][y].type == PieceType::EMPTY);
                            Piece epCapturedPiece = board[r][y]; // read the pawn captured via en passant
                            if(isEnPassant)
                                board[r][y] = Piece{}; // clear the pawn captured via en passant

                            movePieceRaw(r, c, x, y);

                            if(!isKingCheck(turn)) //succeed in escaping check
                            {
                                board[r][c] = frompiece; //undo moves
                                board[x][y] = destpiece;
                                board[r][y] = epCapturedPiece; //return back the pawn 
                                return false;
                            }
                            else
                            {
                                board[r][c] = frompiece;
                                board[x][y] = destpiece;
                                board[r][y] = epCapturedPiece;
                            }
                        }
                    }
                }
            }
        }
    }
    else
    {
        for (int r = 0; r < 8; r++)
        {
            for (int c = 0; c < 8; c++)
            {
                for (int x = 0; x < 8; x++)
                {
                    for (int y = 0; y < 8; y++)
                    {
                        if (isLegalPieceMove(board[r][c], r, c, x, y, turn) && board[r][c].color == Color::BLACK)
                        {
                            Piece frompiece = board[r][c];
                            Piece destpiece = board[x][y];

                            bool isEnPassant = (board[r][c].type == PieceType::PAWN && c != y && board[x][y].type == PieceType::EMPTY);
                            Piece epCapturedPiece = board[r][y];
                            if(isEnPassant)
                                board[r][y] = Piece{};

                            movePieceRaw(r, c, x, y);

                            if(!isKingCheck(turn))
                            {
                                board[r][c] = frompiece;
                                board[x][y] = destpiece;
                                board[r][y] = epCapturedPiece;
                                return false;
                            }
                            else
                            {
                                board[r][c] = frompiece;
                                board[x][y] = destpiece;
                                board[r][y] = epCapturedPiece;
                            }
                        }
                    }
                }
            }
        }
    }

    if (!isKingCheck(turn)) //king is not in check, but no legal moves for the player too
    {
        cout << "Stalemate. The game is a draw.\n";
        return true;
    }

    if (validcolor == 0)
    {
        cout << "It is checkmate for White's king. Black wins.\n";
        return true;
    }
    else
    {
        cout << "It is checkmate for Black's king. White wins.\n";
        return true;
    }
}


int main(void)
{
    setupUnicodeConsole(); // must run before any special characters are printed

    setupStartPos();
    printBoard();

    while (true)
    {
        int fromrow, fromcol, torow, tocol;
        if (!readMove(fromrow, fromcol, torow, tocol))
        {
            cout << "Exiting.\n";
            break;
        }

        Piece frompiece = board[fromrow][fromcol]; // read the piece on the square thats about to move
        Piece destpiece = board[torow][tocol]; // read the destination piece on the square
        Piece epCapturedPiece = board[fromrow][tocol]; // read the captured piece if it was en passant
        
        if (frompiece.type == PieceType::EMPTY)
        {
            cout << "There is no piece on that square.\n";
            continue;
        }

        if (!isLegalPieceMove(frompiece, fromrow, fromcol, torow, tocol, turn))
        {
            cout << "That is not a legal move.\n";
            continue;
        }

        if (!isColorMove(frompiece, turn))
        {
            continue;
        }


        int oldMoveCount = moveCount; // if a check still exist after moving for the current players turn, reinitiallize the movecount after applyMove()
        applyMove(fromrow, fromcol, torow, tocol);

        if(isKingCheck(turn)) // check if the king is still in check after the current player's move
        {
            board[fromrow][fromcol] = frompiece;
            board[torow][tocol] = destpiece; //undo move as king is still in check
            board[fromrow][tocol] = epCapturedPiece; // undo move if it was en passant capture move
            printBoard();
            moveCount = oldMoveCount;
            if(turn % 2 == 0)
            {
                cout << "The White king is still in check.\n";
                continue;
            }
            else
            {
                cout << "The Black king is still in check.\n";
                continue;
            }
        }

        printBoard();
        turn++;

        if(isCheckmateStalemate(turn)) // check if the next player's turn is checkmate or stalemate, if yes then stop game
        {
            cout << "Exiting.\n";
            break;
        }

        if(isKingCheck(turn)) // check if the next player's turn's king is in check
        {
            if(turn % 2 == 0)
            {
                cout << "The White king is in check.\n";
                continue;
            }
            else
            {
                cout << "The Black king is in check.\n";
                continue;
            }
        }
    }

    return 111;
}