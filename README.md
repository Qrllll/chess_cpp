functions


string pieceChar() <br />
takes a pieceType variable and returns either a unicode chess symbol or a plain letter for each piece, used in printBoard()


choosePromotionPiece() <br />
allows the player to choose the pawn promotion piece.


void setupStartPos() <br />
as the name implies.


void printboard() <br />
as the name implies.


bool checkRange <br />
check whether the input such as "e2e3" is a valid range in the board, and returns its respective coordinate in integer for the board[][].


string squareName() <br />
converts a integer coordinate such as "07" back to "a8"


string moveToNotation() <br />
generates the formal chess notation sch as "Nf3", "e4" and "e8=Q" to be used in displayHistory()


void displayHistory() <br />
prints out the move history <br />
a turn contains 2 moves, each from a playerm and uses the formal chess notation to display as opposed to something like "e2e3" from the user input.


int pieceValue() <br />
assigns a value to each pieces, which is used in displayStatistics()


void displayStatistics() <br />
display the statistics of the match after it had ended or when the user prompts it.


char promoTochar() and pieceType charToPromo <br />
changed a PieceType variable to char variable and vice versa


void undoMove() <br />
undo the move that a player made. <br />
1) gets all the moves that had been made up until before the last move <br />
2) resetups the whole board to its initial state <br />
3) replay all the moves again with tryMakeMove()


void saveGame()<br /> 
saves the current game <br />
1) the save files contains a header "CHESSSAVE1" as a identifier for the correct file type <br />
2) each line contains a move such as "e2e3". if there was a promotion, adds the promotion piece to the end eg. "e7e8Q". <br />
3) at the final line, shows whether the file is an ongoing game, resigned game or a draw game.



bool loadGame() <br />
load a save game. a few checks is done before loading the game <br />
1) checks the game state <br />
2) checks whether the file selected is available <br />
3) checks whether it is a valid format which has "CHESSSAVE1" as the header <br /> 
if both checks pass, initialize the chess board and starts reading the file line by line and checking whether the line contain a valid move such as "e2e3" or e7e8Q" <br />
if somewhere in the line contains a invalid move, it stop reading the file and returns where the malfunctioned line is located.


void readMove() <br />
the main function that reads the input from the player. shows the current player's turn first, and allow the player to input a few functions as available.

movePieceRaw() <br />
purely for moving a piece, and make the property hasMoved to true <br />
it is mainly used in the function applyMove()


applyMove() <br />
does everything that is needed to move a piece, and stores the information of the move into moveHistory[] for each turn. <br />
what it stores: <br />
1) intial and final move locations <br /> 
2) type of piece moved <br /> 
3) type of piece captured <br />
4) type of promotion that occured <br />
what it handles: <br />
1) normal piece movement. <br />
1) if it is a pawn, stores whether it had double stepped (will be used to determine the validity of en passant for the next move) <br />
2) if it is pawn moving diagonally to an empty square = en passant. properly handles the capturing by removing the captured pawn as movePieceRaw dosent cover capturing pieces that arent on the final destination. <br />
3) if it is a king moving 2 colums  = castling. properly handles the movement of the rook and king as there is 2 pieces moving at the same turn, and stores whether it is a king or queen side castle. <br />
4) if it is a pawn moving towards the opponent's side, handles the promotion with function call choosePromotionPiece() and stores the promotion info, or if it was from a save file, skips the prompt to ask for the promotion and instantly promotes <br />
note: the property giveCheck in only made true in tryMakeMove as it requires to check if the king is check in the next player's turn.


bool isEnPassant() <br />
used in isPawnMove() for the edge case of en passant. <br />
return true if all the requirements for en passant passed


bool pawn to king move() <br />
all the movement related to the pieces. <br />
all these functions only checks where the piece is moving by its own rules. capturing same color etc check is not done here, but in isLegalPieceMove() <br />
no piece movement is done. used in isLegalPieceMove().


bool isCastling() <br />
used 2 functions calls, isSquareAttacked() and isKingCheck(), hence it must be declared before this function runs. <br />
used in isKingMove() to check whether a castling attempt is valid. <br />
unlike isEnPassant() where it is placed in isPawnMove(), this function is placed directly in isLegalPieceMove() as there are many conditions to be met <br />
returns true if all conditions for castling is met which are: <br />
1)only king can castle <br />
2)king moved 2 sqaure <br />
3)king and rook has not been moved <br />
4)the king is not in checked while, during and after castling <br />
   -for the initial state, the checking is done by isKingCheck() <br /> 
   -for the squares that the king passes through, it is done by isSquareAttacked(). if one of the squared can be attacked, hence cheking the king, the castling attempt will not be valid. <br />
   -no piece movement simulations is done here, which is important in isCheckmateStalemate() afterwards.


bool isLegalPieceMove() <br />
the main functions for the legality of the chess piece movement that calls all the other piece move functions, from pawn to king. <br />
note that this function DOES NOT move the pieces, and only handles the legality <br />
the check is done in a few stages: <br />
1) returns false if the piece is moving to the same place <br />
2) the piece captures its ally <br />
3) checks whether the move is a castling move, if not runs the normal piece movement check. <br />


bool isSquareAttacked() <br />
scnas the entire board to check if any opponent piece on the board could attack a certain square on the board. <br />
it is almost the same to isLegalPieceMove(), but also allows pawn to "attack" blank pieces. This particular pawn movement is useful for isKingCheck() isCastling(), as after moving, the king mustnt be checked, and allows pawn to "attack" the blank square that the king is about to move into.


bool isColorMove() <br />
checks whether the piece moved by the player is the player's piece


bool isKingCheck() <br />
checks whether the current player's king is under check. <br />
uses isSquareAttacked() to check whether there are any opponent pieces that could attack the king's square. <br />
it is called in the main function 2 times: <br />
1) it is then called again after the current player's move to verify whether the king is still in check. if yes, then undo the move and return to allow another input move and dosent increase the turn count. <br />
2) after moving, checks if the next player's king is in checked, and prints out the message notifying.


bool isCheckmateStalemate() <br />
checks whether the player is checkmated or in stalemate by running simulation moves. <br />
it is done in a few stages <br />
1) scans the entire board to see the available legal moves for the current player <br />
2) tries all the possible moves available by running a simulation move(this is similar to when a player does a move). note: it dosent use movePieceRaw() for simulations moves as it will affect the piece propety hasMoved, which distrups castling and en passant. <br />
3) if after the move the king has escaped check, undo the move and return false and stops the search. if not, continue until there is no moves left <br />
4) if the search completes, it means there are no legal moves available. a function call isKingCheck() runs again.  <br />
     A) if it is false it means the current player is in stalemate, but not in check. <br />
     B) if it is true, the current player is checkmated


bool tryMakeMove() <br />
the main function for handling the player's move. <br />
a few basic checks is done before applymove() is called to make the move <br />
1) check whether the user had selected a piece and not am empty square <br />
2) check whether the piece movement is a legal piece move <br />
3) check whether the piece selected is the player's piece <br />
if after the move, the player's king is in check, undo the moves, revert the moveCount and returns false, which allows the playGame() function to reprompt the player to enter another input.


void playGame() <br />
handles the ingame menu such as <br />
1) running the actual game <br />
2) allows the player to save game midgame or after the game ends <br />
if the loaded game has already ended, shows the statistics and return to the main menu. <br />


void mainMemu() <br />
main menu of the game, which has the option to load a saved game or start a new game
