# chess.cpp — Every Function Explained In Detail

This walks through the program in the order things happen, explaining not just *what* each
function does but *how* it does it — step by step, like reading over someone's shoulder.

---

## 1. The data "shapes" (not functions, but you need these to understand everything else)

- **`PType`** — a label for what kind of piece something is: EMPTY, PAWN, KNIGHT, BISHOP, ROOK,
  QUEEN, KING.
- **`Color`** — NONE, WHITE, or BLACK.
- **`Piece`** — a tiny bundle of `{type, color}`. Every one of the 64 squares holds one of these.
- **`Pos`** — just `{row, col}`, a coordinate.
- **`Move`** — a full description of one move: where it starts (`fromR/fromC`), where it ends
  (`toR/toC`), whether it's a promotion, castle, en passant, or a pawn's first two-square jump,
  and what piece/color moved and what (if anything) got captured. This struct is the "currency"
  the whole program passes around — move generation produces these, the board-updater consumes
  them, and the notation writer reads them to print things like "Nf3".
- **`GameState`** — the entire board (an 8×8 array of `Piece`) plus everything needed to enforce
  the trickier chess rules: whose turn it is, whether each king/rook has ever moved (needed for
  castling legality), the en passant target square, and move-counters for the 50-move draw rule.
- **`Stats`** — just running totals for the "Show Statistics" screen (captures, checks, castles,
  promotions, etc.) plus a timestamp of when the game started.

---

## 2. Small helper / "translator" functions

### `inBounds(r, c)`
```cpp
return r >= 0 && r < 8 && c >= 0 && c < 8;
```
Just checks a row and column are both between 0 and 7. The board is stored as an 8×8 array
indexed 0–7, so this is the guard used *everywhere* before touching `board[r][c]`, to avoid
accidentally reading outside the array (which in C++ can crash the program or read garbage
memory).

### `pieceLetterStr(t, out)`
Given a piece type, writes a single capital letter into `out`: N for knight, B for bishop, R
for rook, Q for queen, K for king. Pawns and empty squares get an empty string. This is the
"plain letters" style used in move notation (like "Nf3") — pawns traditionally have no letter
in chess notation, only the destination square.

### `pieceGlyph(piece, symbols, out)`
This decides what to actually *print* for one square of the board:
- If the square is empty, prints a period `.`.
- If `symbols` is false (ASCII mode), it prints a lowercase letter for black pieces (p, n, b, r,
  q, k) and uppercase for white (P, N, B, R, Q, K) — using `toupper()` to flip case for white.
- If `symbols` is true (Unicode mode), it looks up the actual chess figurine character (♙♘♗♖♕♔ for
  white, ♟♞♝♜♛♚ for black) written as `\u2659`-style escape codes, and copies that string into
  `out`.
This is called once per square, every time the board is redrawn.

### `pieceNotationGlyph(t, color, symbols, out)`
Very similar to `pieceGlyph`, but specifically for writing move history (like "Nf3" or "♘f3").
The key difference: pawns get *nothing* here (empty string), because standard chess notation
never prefixes a pawn move with a letter — "e4" not "Pe4".

### `squareNameStr(r, c, out)`
Converts internal row/column numbers into the algebraic square name humans read, like "e4".
Column 0 becomes `'a'`, row 0 becomes `'1'`, etc. — it's just `'a' + c` and `'1' + r`.

### `charToPromo(c)` / `promoToChar(t)`
A matched pair that convert between a promotion piece and the single letter typed for it. If
you type "e7e8q", `charToPromo('q')` turns that into `PType::QUEEN`. `promoToChar` does the
reverse — turning `PType::QUEEN` back into `'q'` when writing a save file or notation string.

### `opponent(color)`
One-liner: if you pass WHITE it returns BLACK, and vice versa. Used constantly whenever the code
needs to reason about "the other side."

---

## 3. Startup

### `ChessGame()` (the constructor)
Runs automatically the moment a `ChessGame` object is created in `main()`. All it does is record
the current wall-clock time into `stats.startTime`, so later the "Elapsed session time" stat can
be calculated.

### `setupUnicodeConsole()`
This is the fix added for the garbled-character problem. On Windows, it calls
`SetConsoleOutputCP(CP_UTF8)` and `SetConsoleCP(CP_UTF8)`, which tell the Windows console to
interpret bytes as UTF-8 text (the encoding the Unicode chess symbols are written in) instead of
its old default code page. Wrapped in `#ifdef _WIN32` so on Linux/macOS this function body is
empty and does nothing (those terminals are UTF-8 by default already).

### `run()`
The heartbeat of the whole program. It prints the title banner once, then loops forever:
- If no game is active (`gameActive == false`), it calls `mainMenuStep()`.
- If a game *is* active, it calls `gameMenuStep()`.
Both of those functions return `true` to keep looping or `false` to stop. The `while (running)`
loop ends the moment either menu function returns false (i.e., you chose "Exit"), and then it
prints "Goodbye!".

---

## 4. Menus — reading what you type and deciding what to do

### `mainMenuStep()`
Prints the "1. Start a new game / 2. Load / 0. Exit" menu, reads one line of input via
`readLine()`, and compares it against `"1"`, `"2"`, `"0"` using `strcmp` (C-string comparison —
returns 0 when the strings are identical). Depending on which matches, it calls `newGame()`,
`loadGameMenu()`, or returns `false` to end the program. Anything else prints "Invalid choice."

### `gameMenuStep()`
Same pattern, but for the much bigger in-game menu (display board, move, undo, history, stats,
legal-move hints, save, resign/draw, back to main menu, toggle symbols, exit). Each numbered
choice is checked with `strcmp` and dispatched to the matching function. Option "10" is handled
directly inline here: it just flips the `useSymbols` boolean and redraws the board so you can see
the effect immediately.

### `statusTextInto(out)`
Builds a short status string like `"White to move"` or `"White to move (IN CHECK)"` directly
into the buffer you pass it. It first checks if the game is already over (prints "Game over" and
stops), otherwise picks "White" or "Black" based on whose turn it is, then calls `inCheck()` to
decide whether to append the "(IN CHECK)" suffix. This is built with `strcpy`/`strcat` rather
than `snprintf` (part of the `<cstdio>` removal).

### `statusText()`
A tiny convenience wrapper: it has a `static char buf[64]` (a buffer that persists between calls
instead of being recreated each time), calls `statusTextInto(buf)` to fill it, and returns a
pointer to it. This lets other functions just write `cout << statusText()` without managing a
buffer themselves.

---

## 5. Setting up and drawing the board

### `initBoard(st)`
Resets a `GameState`'s board to the standard chess starting position:
1. First loop clears all 64 squares to `{EMPTY, NONE}`.
2. `backRank[8]` is an array listing the back-row piece order: Rook, Knight, Bishop, Queen, King,
   Bishop, Knight, Rook.
3. A second loop places White's back rank on row 0, White pawns on row 1, Black pawns on row 6,
   and Black's back rank on row 7 (mirrored, since arrays read left-to-right same as the columns).
4. Resets all the bookkeeping flags: turn back to White, no king/rook has moved yet, no en passant
   target, halfmove clock at 0, move number at 1.

### `newGame()`
Calls `initBoard()` on the live game state, resets the undo stack and move counters, creates a
fresh `Stats` object (wiping old statistics), stamps a new start time, marks the game as active
and not over, prints a message, then calls `printBoard()` to show the fresh position.

### `printBoard()`
Draws the board top-to-bottom. Chess rows are numbered 1–8 from White's side, but the array is
indexed 0–7, so the loop runs `for (int r = 7; r >= 0; r--)` — starting at array index 7 (which
represents row "8") and counting down to 0 (row "1"), so it prints in the visual order a human
expects. For each row, it prints the row number, then loops across all 8 columns calling
`pieceGlyph()` for each square to get the right symbol, prints a bottom border and the column
letters (a–h), and finally calls `statusText()` to show whose turn it is.

---

## 6. The "rules brain" — deciding what's attacked and what's legal

### `isAttacked(st, r, c, by)`
This answers: "if `by` is trying to attack square (r, c), could they actually capture something
sitting there?" It checks every way a piece could threaten that square, one piece type at a time:
- **Pawns**: pawns attack diagonally forward. If `by` is White, a white pawn diagonally
  *behind* (r-1) the square (from White's perspective moving up) would attack it — the code
  checks both diagonal squares. If `by` is Black, it checks the mirrored diagonals in the other
  direction.
- **Knights**: `kn[8][2]` lists all 8 possible knight-jump offsets (like {1,2}, {2,1}, etc.).
  It checks each one — if a knight of the attacking color sits on any of those 8 offset squares,
  it's an attacker.
- **Kings**: loops over all 8 neighboring squares (the double `for (dr...) for (dc...)` skipping
  {0,0}) checking for an enemy king right next to the square — this matters because two kings
  can never be adjacent, so this check helps enforce that indirectly elsewhere.
- **Rooks/Queens (straight lines)**: `straight[4][2]` lists the 4 compass directions (up, down,
  left, right). For each direction, it walks outward one square at a time (`while (inBounds...)`)
  until it either falls off the board or hits a piece. If the first piece it hits belongs to `by`
  and is a rook or queen, that's an attack. If it hits *any* piece (friend or foe, wrong type or
  not), it stops looking further in that direction — because pieces block line-of-sight.
- **Bishops/Queens (diagonals)**: identical idea to the rook check, but using the 4 diagonal
  directions instead.
If none of these checks find anything, it returns `false` — the square is safe.

### `findKing(st, color)`
A brute-force scan of all 64 squares looking for the king of the given color, returning its
`Pos`. Used whenever the program needs to know exactly where a king is (mainly for check
detection).

### `inCheck(st, color)`
Combines the two functions above: find where `color`'s king is, then ask `isAttacked()` whether
the *opponent* could capture that square right now. If yes, that king is in check.

---

## 7. Generating moves

### `addSliding(st, out, count, r, c, color, dirs, numDirs)`
A shared helper used by bishops, rooks, and queens, since they all move the same way — sliding in
straight lines until blocked. For each direction in `dirs`, it steps outward one square at a
time:
- If the square is empty, it's a valid destination (add it as a move) and the piece keeps sliding
  further in that direction.
- If the square has a piece: if it's an enemy piece, add one final move (a capture) — but either
  way, **stop** sliding in that direction, because you can't jump over pieces.
This single function handles bishop, rook, and queen movement just by being handed different
sets of directions (diagonal-only, straight-only, or all 8, respectively).

### `genPseudoMoves(st, color, out)`
The biggest function in the file. It loops over every square on the board; for any square holding
a piece of the current player's color, it works out that piece's normal moves based on its type:
- **Pawn**: handles four separate cases —
  1. *Single step forward* if the square ahead is empty.
  2. *Promotion* if that forward step lands on the last row — instead of one move, it generates
     **four** moves (one per promotion choice: Queen, Rook, Bishop, Knight), since the player
     gets to choose.
  3. *Double step* from the pawn's starting row, if both the one-ahead and two-ahead squares are
     empty (marked with `doubleStep = true`, which matters later for enabling en passant).
  4. *Diagonal captures* — checks both forward-diagonal squares; if an enemy piece sits there, it's
     a capture (again generating 4 promotion variants if landing on the last row); if the square
     is empty but matches the game's stored en-passant target square, it generates a special
     en-passant capture move instead.
- **Knight**: checks all 8 L-shaped jump offsets; any square that's empty or has an enemy piece is
  a valid destination.
- **Bishop / Rook / Queen**: just call `addSliding()` with the appropriate direction list.
- **King**: checks all 8 adjacent squares like a knight check does for its jumps, then separately
  handles **castling**: it checks the king hasn't moved and isn't currently in check, then for
  each side (kingside/queenside) checks the matching rook hasn't moved, the squares between king
  and rook are empty, and — critically — that the squares the king would *pass through* aren't
  under attack (you can't castle through or into check). If all conditions pass, it adds a special
  castling move.
Every generated move is a "pseudo-legal" move — legal by piece-movement rules, but not yet checked
against "does this leave my own king in check."

### `genLegalMoves(st, color, out)`
Takes the pseudo-legal moves from `genPseudoMoves()` and filters them down to truly legal ones.
For each candidate move, it makes a full *copy* of the game state (`GameState copy = st;`),
applies the move to that copy using `applyMoveInPlace()`, and then checks `inCheck(copy, color)`.
If making the move would leave your own king in check, it's discarded; otherwise it's kept. This
copy-and-test approach is simple but effective — it reuses the same check-detection logic instead
of writing separate "does this move expose my king" logic.

---

## 8. Actually applying a move to the board

### `applyMoveInPlace(st, m)`
Given a `Move`, this actually mutates the board:
1. Reads whatever piece is on the "from" square into `moving`.
2. If it's an en passant capture, it removes the captured pawn — which, importantly, is *not* on
   the destination square but on the square directly behind it (`st.board[m.fromR][m.toC]`),
   since en passant captures a pawn that just passed by, not one sitting where you're moving to.
3. Figures out what piece to actually place on the destination square — normally the same piece
   that moved, but if it was a promotion, the type is swapped to the promotion piece
   (`placed.type = m.promotion`).
4. Places that piece on the destination square and clears the origin square.
5. If it was a castle move, it also manually moves the rook (kingside moves the rook from column
   7 to column 5; queenside moves it from column 0 to column 3), since castling moves *two*
   pieces in one turn.
6. Updates the "has moved" flags: if a king or rook moved, records that (needed so castling can
   never be attempted again with that piece); it also checks whether a rook was *captured* on its
   home square, since a captured rook obviously can't be used for castling either even if it never
   technically "moved."

(Note: turn-switching, the en passant target square, and halfmove clock updates happen a little
further down in this same function, right after the piece is placed — the excerpt above covers
the core piece-movement logic.)

---

## 9. Writing human-readable move notation

### `moveToNotation(m, isCheck, isMate, out)`
Converts a `Move` struct into the text form you'd see in a real chess game:
- Castling gets special-cased immediately: "O-O" (kingside) or "O-O-O" (queenside), with a "+" or
  "#" suffix appended if it delivers check or checkmate.
- Otherwise, it builds the string piece by piece: the piece's notation glyph (blank for pawns),
  the origin square, a separator character (`x` for captures, `-` for quiet moves), the
  destination square, then optionally "=Q" (or whichever letter) for promotions, " ep" for en
  passant, and finally "+" or "#" for check/checkmate.
- All of these pieces are concatenated with `strcat` into the output buffer (rather than
  `snprintf`, following the `<cstdio>` removal), building the final string step by step.

---

## 10. Handling a move you type in

### `makeMoveMenu()`
The front door for making a move: checks the game isn't already over, prompts for input like
"e2e4", reads it, allows typing "cancel" to back out, then hands the string off to
`tryMakeMove()`. If it succeeds, redraws the board; if not, prints the error message
`tryMakeMove()` filled in.

### `tryMakeMove(input, err)`
The real workhorse behind making a move:
1. Bails out early with an error message if there's no active game or the game already ended.
2. Copies your input into a safe local buffer and strips out any whitespace characters, so
   "e2 e4" and "e2e4" behave the same.
3. Requires at least 4 characters (minimum needed for "e2e4" format); if there's a 5th character,
   it's read as the promotion letter via `charToPromo()`.
4. Converts the four characters into row/column numbers and checks they're on the board.
5. Calls `genLegalMoves()` for the current position and searches for a move whose from/to squares
   match what you typed. If it was a promotion move and you didn't specify a letter, it defaults
   to Queen (the overwhelmingly common choice).
6. If no matching legal move is found, returns an error ("not a legal move").
7. If found: saves a full snapshot of the current state onto `undoStack` (this is what makes
   "undo" possible later — it's literally a stack of complete board copies), then calls
   `applyMoveInPlace()` to actually make the move, and records it into `moveList`.
8. Updates statistics: increments total move count, capture counts (split by color), en passant
   count, castle count, and promotion count, based on flags on the applied move.
9. Checks whether the *opponent* is now in check (`inCheck`), and generates *their* legal replies
   to determine checkmate (in check with zero legal moves) or stalemate (not in check but zero
   legal moves).
10. Builds the notation string via `moveToNotation()`, stores it in `notationHistory`, increments
    `moveCount`, and prints "Move played: ...".
11. Prints checkmate/stalemate/check messages as appropriate, and separately checks the 50-move
    rule (`halfmoveClock >= 100`, since it counts half-moves, not full moves) to declare a draw if
    reached.

---

## 11. Undo, history, and statistics

### `undoMove()`
If there's nothing saved on `undoStack`, prints "Nothing to undo." Otherwise pops the last saved
snapshot off the stack (`undoStack[--undoTop]`) and assigns it directly over the live `state` —
instantly reverting the entire board to how it was before the last move. Also decrements
`moveCount` and clears the `gameOver` flag in case the undone move had ended the game, then
redraws the board.

### `showHistory()`
Loops through `notationHistory` and prints moves in pairs, numbering them like a real scoresheet:
"1. e4 e5  2. Nf3 Nc6  ...". The numbering logic (`i % 2 == 0` prints the move number before
White's move, `i % 2 == 1` adds a line break after Black's move) mimics standard chess notation
layout.

### `showStatistics()`
Prints all the running counters from the `Stats` struct, then does a fresh calculation of
material: it loops over every square, adds up the point value of every White piece (via
`pieceValue()`) and every Black piece separately, prints both totals and the difference
("Material balance"), and finally calculates and prints elapsed session time using
`difftime(now, stats.startTime)`.

### `pieceValue(t)`
A simple lookup: pawn = 1, knight = 3, bishop = 3, rook = 5, queen = 9, anything else (including
king, which is priceless/never captured) = 0. Standard chess point values used purely for the
material-balance statistic.

---

## 12. Extra features

### `showLegalMovesMenu()`
Asks you for a square (like "e2"), validates it's on the board, occupied, and belongs to the
current player. Then generates all legal moves for the whole position and filters down to just
the ones starting from that square, printing each destination (with a "(Q)" style suffix if it's
a promotion option). This is essentially a "give me a hint" feature.

### `resignOrDraw()`
Asks you to type "resign", "draw", or anything else to cancel. Uses `toLowerInPlace()` first so
it's not case-sensitive. Resigning declares the *other* side the winner; declaring a draw just
ends the game as a tie. Either way it sets `gameOver = true`.

---

## 13. Save and load (the part that used to rely on `<cstdio>`)

### `saveGameMenu()` / `loadGameMenu()`
Thin wrappers that just prompt for a filename and hand it off to the real `saveGame()` /
`loadGame()` functions. Typing nothing cancels.

### `saveGame(filename)`
Opens the file for writing using an `ofstream` (C++'s file-output stream, replacing the old
`fopen`/`fprintf`). Writes a header line `"CHESSSAVE1"` so the loader can recognize valid save
files, then loops through every move ever played (`moveList`) and writes it in short form — the
origin square, destination square, and (if it was a promotion) the promotion letter — one move
per line, e.g. "e2e4" or "e7e8q". This is intentionally *not* a full board dump — it's a replay
script, so loading a game means replaying every move from the start.

### `loadGame(filename)`
Opens the file for reading using an `ifstream`. Reads the first line and checks it matches the
expected header, bailing out with an error if the file is empty, unreadable, or has the wrong
format. If valid, it fully resets the game (fresh board, cleared undo stack, cleared stats/move
count, marks the game active). Then it reads the file line by line: trims whitespace off each
line, skips blank lines, and calls `tryMakeMove()` on each one — exactly as if you'd typed that
move yourself. If any move in the file turns out to be illegal (e.g. a corrupted or hand-edited
save file), it stops replaying immediately and reports how far it got, rather than silently
producing a broken board. Finally it reports how many moves were successfully replayed and shows
the resulting board.

---

## 14. Small input-handling helpers used throughout

### `trimInPlace(s)`
Removes leading and trailing whitespace from a C-string, in place (modifying the original buffer
rather than creating a new one). It finds the first non-space character and the last non-space
character, then uses `memmove` to shift that middle portion to the start of the buffer and cuts
the string short there.

### `readLine(buf, size)`
A safe wrapper around reading one line of console input. Uses `cin.getline()`, which reads up to
`size` characters (preventing buffer overflows even if you type something enormous). If the read
fails for any reason, it clears the error state on `cin` and just gives back an empty string
instead of leaving things in a broken state. Always calls `trimInPlace()` afterward so trailing
newlines/spaces never sneak into comparisons like `strcmp(choice, "1")`.

### `toLowerInPlace(s)`
Walks through a string character by character, converting each to lowercase using `tolower()`.
Used so commands like "Resign", "RESIGN", and "resign" all get treated the same.

---

## 15. The entry point

### `main()`
The very first code that runs when you launch the program:
1. Calls `setupUnicodeConsole()` — the fix for garbled chess symbols on Windows.
2. Creates a `ChessGame` object (which triggers its constructor, recording the start time).
3. Calls `game.run()` — this is where control stays for the entire time you're playing, since
   `run()` contains the main menu loop.
4. Once `run()` finally returns (you chose Exit), `main()` returns `0`, telling the operating
   system the program finished successfully.

---

## The big picture, one more time

```
main()
  └─ run()                              ← the loop that keeps the program alive
       ├─ mainMenuStep()  → newGame() / loadGame()
       └─ gameMenuStep()  → printBoard(), makeMoveMenu(), undoMove(), showHistory(),
                             showStatistics(), showLegalMovesMenu(), saveGame(),
                             resignOrDraw()

makeMoveMenu() → tryMakeMove()
                    ├─ genLegalMoves()  → genPseudoMoves() + inCheck() filtering
                    ├─ applyMoveInPlace()
                    ├─ moveToNotation()
                    └─ inCheck() / genLegalMoves() again  → detect checkmate/stalemate
```

Everything else (the helper/translator functions in section 2, and the input helpers in section
14) exists to support these core paths — turning raw text into board coordinates, and board data
back into readable text.
