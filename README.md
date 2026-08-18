functions

movePieceRaw()
purely for moving a piece, and make the property hasMoved to true
it is mainly used in the function applyMove()


applyMove()
does everything that is needed to move a piece, and stores the information of the move into moveHistory[] for each turn.
what it stores:
1) intial and final move locations
2) type of piece moved
3) type of piece captured
4) type of promotion that occured

what it handles:
1) normal piece movement.
1) if it is a pawn, stores whether it had double stepped (will be used to determine the validity of en passant for the next move)
2) if it is pawn moving diagonally to an empty square = en passant. properly handles the capturing by removing the captured pawn as movePieceRaw dosent cover capturing pieces that arent on the final destination.
3) if it is a king moving 2 colums  = castling. properly handles the movement of the rook and king as there is 2 pieces moving at the same turn, and stores whether it is a king or queen side castle.
4) if it is a pawn moving towards the opponent's side, handles the promotion with function call choosePromotionPiece() and stores the promotion info.


bool isEnPassant()
used in isPawnMove() for the edge case of en passant.
return true if all the requirements for en passant passed


bool pawn to king move()
all the movement related to the pieces.
all these functions only checks where the piece is moving by its own rules. capturing same color etc check is not done here, but in isLegalPieceMove()
no piece movement is done. used in isLegalPieceMove().


bool isCastling()
used 2 functions calls, isSquareAttacked() and isKingCheck(), hence it must be declared before this function runs.
used in isKingMove() to check whether a castling attempt is valid.
unlike isEnPassant() where it is placed in isPawnMove(), this function is placed directly in isLegalPieceMove() as there are many conditions to be met
returns true if all conditions for castling is met which are:
1)only king can castle
2)king moved 2 sqaure
3)king and rook has not been moved
4)the king is not in checked while, during and after castling
   -for the initial state, the checking is done by isKingCheck()
   -for the squares that the king passes through, it is done by isSquareAttacked(). if one of the squared can be attacked, hence cheking the king, the castling attempt will not be valid.
   -no piece movement simulations is done here, which is important in isCheckmateStalemate() afterwards.


bool isLegalPieceMove()
the main functions for the legality of the chess piece movement that calls all the other piece move functions, from pawn to king.
note that this function DOES NOT move the pieces, and only handles the legality
the check is done in a few stages:
1) returns false if the piece is moving to the same place
2) the piece captures its ally
3) checks whether the move is a castling move, if not runs the normal piece movement check.


bool isSquareAttacked()
scnas the entire board to check if any opponent piece on the board could attack a certain square on the board.
it is almost the same to isLegalPieceMove(), but also allows pawn to "attack" blank pieces. This particular pawn movement is useful for isKingCheck() isCastling(), as after moving, the king mustnt be checked, and allows pawn to "attack" the blank square that the king is about to move into.


bool isColorMove()
checks whether the piece moved by the player is the player's piece


bool isKingCheck()
checks whether the current player's king is under check.
uses isSquareAttacked() to check whether there are any opponent pieces that could attack the king's square.
it is called in the main function 2 times:
1) it is then called again after the current player's move to verify whether the king is still in check. if yes, then undo the move and return to allow another input move and dosent increase the turn count.
2) after moving, checks if the next player's king is in checked, and prints out the message notifying.


bool isCheckmateStalemate()
checks whether the player is checkmated or in stalemate by running simulation moves.
it is done in a few stages
1) scans the entire board to see the available legal moves for the current player
2) tries all the possible moves available by running a simulation move(this is similar to when a player does a move). note: it dosent use movePieceRaw() for simulations moves as it will affect the piece propety hasMoved, which distrups castling and en passant.
3) if after the move the king has escaped check, undo the move and return false and stops the search. if not, continue until there is no moves left
4) if the search completes, it means there are no legal moves available. a function call isKingCheck() runs again. 
     A) if it is false it means the current player is in stalemate, but not in check.
     B) if it is true, the current player is checkmated
