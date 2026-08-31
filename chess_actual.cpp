#include <iostream>
#include <fstream>

#ifdef _WIN32 //enables the use of the unicode symbols
#include <windows.h>
#endif
static void setupUnicodeConsole() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_FONT_INFOEX cfi = {};
    cfi.cbSize = sizeof(cfi);
    cfi.dwFontSize.Y = 18;
    cfi.FontFamily = FF_DONTCARE;
    cfi.FontWeight = FW_NORMAL;
    wcscpy_s(cfi.FaceName, L"DejaVu Sans Mono");
    SetCurrentConsoleFontEx(hOut, FALSE, &cfi);
#endif
}

using namespace std;

enum class PieceType { EMPTY, PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING }; //initialise the types of pieces
enum class Color { NONE, WHITE, BLACK }; //pieces color
enum class GameResult { ONGOING, WHITE_RESIGNED, BLACK_RESIGNED, DRAW_AGREED }; //the game status for the save file


struct Piece // what a piece is made up of
{
    //default states of a piece
    PieceType type = PieceType::EMPTY;
    Color color = Color::NONE;
    bool hasMoved = false;
};

struct Move // information about a single move
{
    int fromrow, fromcol, torow, tocol; //the start and end position of the piece
    Piece movedPiece; //what piece had been moved
    Piece capturedPiece = {PieceType::EMPTY, Color::NONE}; //defaults to empty, if there is a piece captured, applyMove() will update it
    PieceType promotedTo = PieceType::EMPTY; //also defaults to empty, will be updated by loadGame() if there is a promotion happening, which prevents the prompt of asking the user for promotion piece

    bool isEnPassantCapture = false;   // captured pawn wasn't on the 'to' square
    bool isCastleKingside = false;
    bool isCastleQueenside = false;
    bool isPawnDoubleStep = false;     // enables en passant on the next move
    bool givesCheck = false;
    bool givesCheckmate = false;
};

Piece board[8][8]; // the array for the pieces in the board
const int MAX_MOVES = 1000;
Move moveHistory[MAX_MOVES]; // max 1k move history
int moveCount = 0;
int turn = 0;
GameResult gameResult = GameResult::ONGOING; //default game state in a save file
string groupName = ""; // the group/team name for this game session
bool useSymbolBoard = true;

string pieceChar(const Piece& p) // returns either a unicode chess symbol or a plain letter for each piece, used in printBoard()
{
    if (!useSymbolBoard) // plain-letter fallback (original representation)
    {
        char c;
        switch (p.type)
        {
        default: return " "; // empty spaces for no pieces
        case PieceType::PAWN:   c = 'p'; break;
        case PieceType::KNIGHT: c = 'n'; break;
        case PieceType::BISHOP: c = 'b'; break;
        case PieceType::ROOK:   c = 'r'; break;
        case PieceType::QUEEN:  c = 'q'; break;
        case PieceType::KING:   c = 'k'; break;
        }

        if (p.color == Color::WHITE) // uppercase for white side pieces
            c = toupper(c);

        return string(1, c);
    }

    if (p.color == Color::WHITE) // solid filled glyphs for white pieces, since terminal is usually black background unless settings changed
        switch (p.type)
        {
        default:                return " "; // empty square
        case PieceType::PAWN:   return "\u265F"; // ♟
        case PieceType::KNIGHT: return "\u265E"; // ♞
        case PieceType::BISHOP: return "\u265D"; // ♝
        case PieceType::ROOK:   return "\u265C"; // ♜
        case PieceType::QUEEN:  return "\u265B"; // ♛
        case PieceType::KING:   return "\u265A"; // ♚
        }

    switch (p.type)
    {
        default:                return " ";
        case PieceType::PAWN:   return "\u2659"; // ♙
        case PieceType::KNIGHT: return "\u2658"; // ♘
        case PieceType::BISHOP: return "\u2657"; // ♗
        case PieceType::ROOK:   return "\u2656"; // ♖
        case PieceType::QUEEN:  return "\u2655"; // ♕
        case PieceType::KING:   return "\u2654"; // ♔
    }
}

char promoToChar(PieceType t) //changes a PieceType variable to a char variable. its similar to pieceChar() function, but excludes the character for pawn, since this function is used for moveHistory()
{
    switch (t)
    {
    default:                return 'q';
    case PieceType::ROOK:   return 'r'; break;
    case PieceType::BISHOP: return 'b'; break;
    case PieceType::KNIGHT: return 'n'; break;
    }
}
 
PieceType charToPromo(char c) //vice versa from the function above
{
    switch (toupper(c))
    {
    default:  return PieceType::QUEEN;
    case 'R': return PieceType::ROOK;   break;
    case 'B': return PieceType::BISHOP; break;
    case 'N': return PieceType::KNIGHT; break;
    }
}

PieceType choosePromotionPiece() // shows the prompt to allow to player to choose its pawn promotion piece.
{
    cout << "Pawn promotion! Choose a piece (Q/R/B/N): ";
    char choice;
    cin >> choice;
    choice = toupper(choice);
    switch (choice)
    {
    default:  return PieceType::QUEEN;
    case 'R': return PieceType::ROOK;   break;
    case 'B': return PieceType::BISHOP; break;
    case 'N': return PieceType::KNIGHT; break;
    }
}

void setupStartPos() //setup the starting position for the board
{
    PieceType backRank[8] = {
        PieceType::ROOK, PieceType::KNIGHT, PieceType::BISHOP, PieceType::QUEEN,
        PieceType::KING, PieceType::BISHOP, PieceType::KNIGHT, PieceType::ROOK }; //same back rank for white and black

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
    cout << "\n  ┌";

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

bool askYesNo(const string& prompt) // simple function for asking question with y/n answers
{
    cout << prompt << " (y/n): ";
    string answer;
    cin >> answer;
    return !answer.empty() && (answer[0] == 'y' || answer[0] == 'Y');
}

bool checkRange(char file, char rank, int& outrow, int& outcol) // check whether the input is within the range of the board, and returns the actual position of the piece in the board[][] coordinate such as "07" for "a8";
{
    int col = file - 'a';
    int row = rank - '1';

    if (col < 0 || col > 7 || row < 0 || row > 7)
        return false;

    outrow = row;
    outcol = col;
    return true;
}

string squareName(int row, int col) // converts back the integer coordinate such as 07 back to a8, used mainly in moveToNotation()
{
    string s;
    s += char('a' + col);
    s += char('1' + row);
    return s;
}

string moveToNotation(const Move& m) // generates the formal chess notation such as "Nf3", "e4", "e8=Q" etc. used in displayHistory()
{
    if (m.isCastleKingside)  //number of 0s represent the number of sqaures the rook had moved
        return "0-0";
    if (m.isCastleQueenside)
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
        default: break; // pawn has no letter
    }

    if (pieceLetter != ' ') 
        result += pieceLetter; // first letter shows the type of piece

    bool isCapture = (m.capturedPiece.type != PieceType::EMPTY) || m.isEnPassantCapture; // check whether there is a capture in the move

    if (m.movedPiece.type == PieceType::PAWN && isCapture)
        result += char('a' + m.fromcol); // pawn captures show the origin file of the capturing pawn
    
    if (isCapture) 
        result += "x"; // x represents capture

    result += squareName(m.torow, m.tocol); // adds the destination square

    if (m.promotedTo != PieceType::EMPTY) // check whether the move had made a promotion
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

    if (m.givesCheckmate) // checkmate takes priority over a normal check symbol
        result += '#';
    else if (m.givesCheck)
        result += '+';

    return result;
}


void displayHistory()//display the move history eg '1. f3 e5 2. g4 Qh4#' for a simple game.
{
    if (moveCount == 0) 
    {
        cout << "No moves played yet.\n"; 
        return; 
    }

    cout << "\n----- Move History -----\n";

    for (int i = 0; i < moveCount; i++)
    {
        if (i % 2 == 0) // a single turn consists of displaying both white and black moves, hence 2 turns in total for a single line of turn notation
            cout << (i / 2 + 1) << ". " << moveToNotation(moveHistory[i]) << "  "; //increase the turn count at even numbers
        else 
            cout << moveToNotation(moveHistory[i]) << "\n";
    }
    if (moveCount % 2 == 1) cout << "\n";
}

int pieceValue(PieceType t) // standard point values for each piece captured, used in the statistic display
{
    switch (t)
    {
    case PieceType::PAWN:   return 1;
    case PieceType::KNIGHT: return 3;
    case PieceType::BISHOP: return 3;
    case PieceType::ROOK:   return 5;
    case PieceType::QUEEN:  return 9;
    default:                return 0; // king and empty has no value
    }
}

void displayStatistics()
{
    int whiteCaptures = 0, blackCaptures = 0, castles = 0, promotions = 0, enPassantCaptures = 0, checksGiven = 0;

    for (int i = 0; i < moveCount; i++)
    {
        const Move& m = moveHistory[i];
        bool isCapture = (m.capturedPiece.type != PieceType::EMPTY) || m.isEnPassantCapture;
        if (isCapture)
        {
            if (m.movedPiece.color == Color::WHITE) whiteCaptures++;
            else blackCaptures++;
        }
        if (m.isCastleKingside || m.isCastleQueenside) castles++;
        if (m.promotedTo != PieceType::EMPTY) promotions++;
        if (m.isEnPassantCapture) enPassantCaptures++;
        if (m.givesCheck) checksGiven++;
    }

    int whiteMaterial = 0, blackMaterial = 0;
    for (int r = 0; r < 8; r++) for (int c = 0; c < 8; c++)
    {
        int value = pieceValue(board[r][c].type);
        if (board[r][c].color == Color::WHITE) whiteMaterial += value;
        else if (board[r][c].color == Color::BLACK) blackMaterial += value;
    }

    cout << "\n----- Game Statistics -----\n";
    if (!groupName.empty()) cout << "Group name           : " << groupName << "\n";
    cout << "Total moves played   : " << moveCount << "\n";
    cout << "White captures       : " << whiteCaptures << "\n";
    cout << "Black captures       : " << blackCaptures << "\n";
    cout << "Castles performed    : " << castles << "\n";
    cout << "Pawn promotions      : " << promotions << "\n";
    cout << "En passant captures  : " << enPassantCaptures << "\n";
    cout << "Checks given         : " << checksGiven << "\n";
    cout << "White material       : " << whiteMaterial << "\n";
    cout << "Black material       : " << blackMaterial << "\n";
    cout << "Material balance     : " << (whiteMaterial - blackMaterial) << " (positive favors White)\n";
}

//function declaraction for undoMove() and loadGame()
bool tryMakeMove(int fromrow, int fromcol, int torow, int tocol, PieceType forcedPromotion);
void undoMove()
{
    if (moveCount == 0) 
    {
        printBoard();
        cout << "Nothing to undo.\n";
        return;
    }
 
    int targetCount = moveCount - 1;
    Move replayMoves[MAX_MOVES];
    for (int i = 0; i < targetCount; i++) 
        replayMoves[i] = moveHistory[i]; // get all the moves done up until the previous last move
 
    //re-setup the board
    for (int r = 0; r < 8; r++) 
        for (int c = 0; c < 8; c++) 
            board[r][c] = Piece{};
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
 
    f << "CHESSSAVE1\n"; //use this header as the file format
    if (!groupName.empty()) // checks if theres group name, if so then save it
        f << "GROUP:" << groupName << "\n";
    for (int i = 0; i < moveCount; i++)
    {
        const Move& m = moveHistory[i]; //saves a move by outputting eg "e2e3" or "e7e8Q" if there is a promotion
        f << squareName(m.fromrow, m.fromcol) << squareName(m.torow, m.tocol);
        if (m.promotedTo != PieceType::EMPTY) 
            f << promoToChar(m.promotedTo);
        f << "\n"; //one line for one move
    }

    switch (gameResult)
    {
    case GameResult::WHITE_RESIGNED: f << "RESULT:WHITE_RESIGNED\n"; break;
    case GameResult::BLACK_RESIGNED: f << "RESULT:BLACK_RESIGNED\n"; break;
    case GameResult::DRAW_AGREED:    f << "RESULT:DRAW_AGREED\n";    break;
    default: break; // basically ONGOING as gameResult was defaulted to it
    }

    cout << "Game saved to " << filename << " (" << moveCount << " moves).\n";
}

bool loadGame(const string& filename)
{
    ifstream f(filename);
    if (!f) // returns error that it cant find the file
    {
        cout << "Could not open file: " << filename << "\n";
        return false;
    } 
 
    string header;
    getline(f, header);
    if (header != "CHESSSAVE1") // returns error that its an unknown file type
    {
        cout << "Unrecognized save file format.\n"; 
        return false; 
    } 
 
    // reset to a fresh new game before replaying
    for (int r = 0; r < 8; r++) 
        for (int c = 0; c < 8; c++) 
            board[r][c] = Piece{};
            setupStartPos();
            moveCount = 0;
            turn = 0;
            gameResult = GameResult::ONGOING;
            groupName = "";
 
    string line;
    int nummoves = 0;
    /*
    this loop runs to reads every line of the file to verify it is all valid, 
    which ends after getline returns false when there are no lines left, or there is an malformed line, which the replay ends there and shows the error.
    */
    while (getline(f, line)) 
    {
        if (line.empty()) //reruns the loop to read next line
            continue;

        if (line.rfind("GROUP:", 0) == 0) //.rfind = read from the reverse, which is the bottom of the file. if found, it will return 0
        {
            groupName = line.substr(6);  // read the line starting from the position 6(note: first position is 0).
            continue;
        }

        if (line.rfind("RESULT:", 0) == 0)
        {
            string resultCode = line.substr(7);
            if (resultCode == "WHITE_RESIGNED") gameResult = GameResult::WHITE_RESIGNED;
            else if (resultCode == "BLACK_RESIGNED") gameResult = GameResult::BLACK_RESIGNED;
            else if (resultCode == "DRAW_AGREED") gameResult = GameResult::DRAW_AGREED;
            continue;
        }    

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
 
        PieceType forcedPromotion; //check the promotion piece
        if((line.size() == 5))
            forcedPromotion = charToPromo(line[4]);
        else
            forcedPromotion = PieceType::EMPTY;
 
        if (!tryMakeMove(fromrow, fromcol, torow, tocol, forcedPromotion)) //runs the moves, and additionally, check if one of the movse cant be done
        {
            cout << "Warning: move '" << line << "' from save file is illegal. Stopping replay.\n";
            break;
        }
        nummoves++;
    }
 
    cout << "Loaded " << filename << ": " << nummoves << " moves replayed successfully.\n";
    return true;
}

bool readMove(int& fromrow, int& fromcol, int& torow, int& tocol)
{
    if(turn % 2 == 0)
        cout << "It is White's turn.\n";
    else
        cout << "It is Blacks's turn.\n";

    cout << "1) 'symbols' \t- Switch between symbol and characters on the board.\n2) 'e2e3' \t- To enter a move.\n3) 'stats' \t- Show the game statistics.\n4) 'undo' \t- Undo a move.\n5) 'save <file>'- Save the current game.\n6) 'resign' \t- Resign from your opponent.\n7) 'draw' \t- Request a draw with your opponent.\n8) 'quit' \t- Quit the game.\nEnter a command: ";
    string input;
    cin >> input;
    if (input == "quit" || input == "exit")
        return false;
    if (input == "symbols") // toggle between chess symbols and plain letters on the board
    {
        useSymbolBoard = !useSymbolBoard;
        cout << (useSymbolBoard ? "Symbol board enabled.\n" : "Plain-letter board enabled.\n");
        printBoard();
        return readMove(fromrow, fromcol, torow, tocol);
    }
    if (input == "history")
    {
        printBoard();
        displayHistory();
        return readMove(fromrow, fromcol, torow, tocol);
    }
    if (input == "stats")
    {
        printBoard();
        displayStatistics();
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
        printBoard();
        return readMove(fromrow, fromcol, torow, tocol);
    }
    if (input == "resign")
    {
        printBoard();
        if((turn % 2 == 0))
        {
            gameResult = GameResult::WHITE_RESIGNED;
            cout << "White resigns. Black wins.\n";
        }
        else
        {
            gameResult = GameResult::BLACK_RESIGNED;
            cout << "Black resigns. White wins.\n";
        }

        displayHistory();
        displayStatistics();
        return false;
    }
    if (input == "draw")
    {
        if((turn % 2 == 0)) // white turn
        {
            cout << "White offers a draw.\n";
            if(askYesNo("Does Black accept the draw?"))
            {
                gameResult = GameResult::DRAW_AGREED;
                cout << "Draw agreed.\n";
            }
            else
            {
                cout << "Draw declined.\n";
                return readMove(fromrow, fromcol, torow, tocol);  
            }
        }
        else
        {
            cout << "Black offers a draw.\n";
            if(askYesNo("Does White accept the draw?"))
                {
                    gameResult = GameResult::DRAW_AGREED;
                    cout << "Draw agreed.\n";
                }
            else
            {
                cout << "Draw declined.\n";
                return readMove(fromrow, fromcol, torow, tocol);  
            }
        }
    }
    
    if(input.size() != 4 || !checkRange(input[0], input[1], fromrow, fromcol) || !checkRange(input[2], input[3], torow, tocol))
    {
        printBoard();
        cout << "Invalid input.\n";
        return readMove(fromrow, fromcol, torow, tocol);
    }

    return true;
}

void movePieceRaw(int fromrow, int fromcol, int torow, int tocol) // purely for moving a piece and making its status "moved", used in applyMove()
{
    board[torow][tocol] = board[fromrow][fromcol];
    board[fromrow][fromcol] = Piece{};
    board[torow][tocol].hasMoved = true;
}

void applyMove(int fromrow, int fromcol, int torow, int tocol, PieceType forcedPromotion = PieceType::EMPTY) // used in tryMakeMove() to move the pieces, and store the move history on each turn
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
    
    if (frompiece.type == PieceType::PAWN && ((frompiece.color == Color::WHITE && torow == 7) || (frompiece.color == Color::BLACK && torow == 0))) // if it is white pawn, promotion happens at top row, bottom row for black
    {
        PieceType chosen;
        if((forcedPromotion != PieceType::EMPTY)) //if it is from a save file, instanly promotes the piece without prompting 
            chosen = forcedPromotion;
        else
            chosen = choosePromotionPiece();

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
        direction = 1; // only moves upward for white
        startrow = 1;
    }
    else
    {
        direction = -1; // only move downwards for black
        startrow = 6;
    }

    int dc = tocol - fromcol;
    int dr = torow - fromrow;

    if(dc == 0) //normal forward move
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

    return isEnPassant(color, fromrow, fromcol, torow, tocol); // en passant move

    return false;
}

bool isKnightMove(int fromrow, int fromcol, int torow, int tocol)
{
    int dc = abs(tocol - fromcol);
    int dr = abs(torow - fromrow);

    return (dc == 1 && dr == 2) || (dc == 2 && dr == 1); // L shaped move
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

    while (abs(steprow) != abs(dr)) // check whether there are pieces blocking the move by running a loop to check every squares inbetween
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
        while (abs(step) != abs(dc)) // check whether there are pieces blocking the move, same logic as the bishop
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
    return isBishopMove(fromrow, fromcol, torow, tocol) || isRookMove(fromrow, fromcol, torow, tocol); //combination of bishop + rook
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

bool isLegalPieceMove(const Piece& piece, int fromrow, int fromcol, int torow, int tocol, int turn) // the main function that compiles all the piece movement rulesets into a single function
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

bool isSquareAttacked(int torow, int tocol,int turn) // scans the entire board to check if any opponent piece on the board could attack a certain square on the board.
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

bool isColorMove(const Piece& piece, int& turn) //checks whether the piece moved by the player is the player's piece
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

bool isKingCheck(int turn) //checks whether the current player's king is under check.
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

bool isCheckmateStalemate(int turn) //checks whether the player is checkmated or in stalemate by running simulation moves.
{
    int validcolor = turn % 2; // 0 = white's turn, 1 = black's turn

    if (validcolor == 0)
    {
        for (int r = 0; r < 8; r++) for (int c = 0; c < 8; c++) for (int x = 0; x < 8; x++) for (int y = 0; y < 8; y++)//checks all the possible moves for the player's turn to escape check
            {
                if (isLegalPieceMove(board[r][c], r, c, x, y, turn) && board[r][c].color == Color::WHITE) 
                {
                    Piece frompiece = board[r][c]; // read the piece on the square thats about to move
                    Piece destpiece = board[x][y]; // read the destination piece on the square
                    
                    bool isEnPassant = (board[r][c].type == PieceType::PAWN && c != y && board[x][y].type == PieceType::EMPTY);
                    Piece epCapturedPiece = board[r][y]; // read the pawn captured via en passant
                    if(isEnPassant)
                        board[r][y] = Piece{}; // clear the pawn captured via en passant

                    board[x][y] = board[r][c]; // dosent use the movePieceRaw() as it will flag the simulation piece as "moved"
                    board[r][c] = Piece{};

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
    else
    {
        for (int r = 0; r < 8; r++) for (int c = 0; c < 8; c++) for (int x = 0; x < 8; x++) for (int y = 0; y < 8; y++)
            {
                if (isLegalPieceMove(board[r][c], r, c, x, y, turn) && board[r][c].color == Color::BLACK)
                {
                    Piece frompiece = board[r][c];
                    Piece destpiece = board[x][y];

                    bool isEnPassant = (board[r][c].type == PieceType::PAWN && c != y && board[x][y].type == PieceType::EMPTY);
                    Piece epCapturedPiece = board[r][y];
                    if(isEnPassant)
                        board[r][y] = Piece{};

                    board[x][y] = board[r][c];
                    board[r][c] = Piece{};

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

    if (!isKingCheck(turn)) //king is not in check, but no legal moves for the player too
    {
        cout << "Stalemate. The game is a draw.\n";
        return true;
    }

    moveHistory[moveCount - 1].givesCheckmate = true; //sets the previous move done by the previous player as a checkmate move

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

bool tryMakeMove(int fromrow, int fromcol, int torow, int tocol, PieceType forcedPromotion = PieceType::EMPTY) //the main function for handling the player's move.
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
 
    int oldMoveCount = moveCount; //stores the moveCount in case the the move that the player made still leaves the king in check, which the move will be undo'd
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

    if (isKingCheck(turn)) // checks if the player's move had check the opponents, hence added after turn++
        moveHistory[moveCount - 1].givesCheck = true;
    return true;
}

void playGame()
{
    printBoard();
    // checks the game state first, as it could be from a loaded game where the game has already ended
    if (isCheckmateStalemate(turn) || gameResult != GameResult::ONGOING)
    {
        if (gameResult == GameResult::WHITE_RESIGNED) cout << "This game already ended: White resigned.\n";
        else if (gameResult == GameResult::BLACK_RESIGNED) cout << "This game already ended: Black resigned.\n";
        else if (gameResult == GameResult::DRAW_AGREED) cout << "This game already ended: draw agreed.\n";

        if (askYesNo("Want to see the move history and statistics of the game?"))
        {
            displayHistory();
            displayStatistics();
        }
        return;
    }
 
    while (true)
    {
        int fromrow, fromcol, torow, tocol;
        if (!readMove(fromrow, fromcol, torow, tocol)) //if exiting program
        {
            if (askYesNo("Save before returning to the menu?"))\
            {
                cout << "Filename: ";
                string filename; cin >> filename;
                saveGame(filename);
            }
            return; //stops the loop
        }
 
        if (!tryMakeMove(fromrow, fromcol, torow, tocol))
            continue;
 
        printBoard();

        if(isCheckmateStalemate(turn)) // check if the next player's turn is checkmate or stalemate, if yes then stop game
        {
            displayHistory();
            displayStatistics();

            if (askYesNo("Save this game before returning to the menu?"))
            {
                cout << "Filename: ";
                string filename; cin >> filename;
                saveGame(filename);
            }
            return;
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
}

void mainMenu()
{
    while (true)
    {
        cout << "\n===== CHESS =====\n";
        cout << "1. New Game\n";
        cout << "2. Load Game\n";
        cout << "3. Exit\n";
        cout << "Choice: ";
 
        string choice;
        cin >> choice;
 
        if (choice == "1")
        {
            for (int r = 0; r < 8; r++)  //setup the chess board
                for (int c = 0; c < 8; c++) 
                    board[r][c] = Piece{};
            setupStartPos();
            moveCount = 0;
            turn = 0;
            gameResult = GameResult::ONGOING;
            cout << "Enter a group name for this game: ";
            cin.ignore(150, '\n');
            getline(cin, groupName);
            playGame();
        }
        else if (choice == "2")
        {
            cout << "Filename: ";
            string filename; 
            cin >> filename;
            if (loadGame(filename))
                playGame();
        }
        else if (choice == "3")
        {
            cout << "Goodbye!\n";
            return;
        }
        else
        {
            cout << "Invalid choice.\n";
        }
    }
}

int main(void)
{
    setupUnicodeConsole();
    mainMenu();
    return 111;
}
