Fork or clone your this chess project into a new GitHub repository.

Add support for FEN stringsLinks to an external site. to your game setup so that instead of the current way you are setting up your game board you are setting it up with a call similar to the following call.

FENtoBoard("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR");

Your routine should be able to take just the board position portion of a FEN string, or the entire FEN string like so:

FENtoBoard("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

(you can ignore the end for now)

This will allow you to quickly check that your castling, promotion and en passant code is working.


FentoBoard:
- convert Fen string to Board positions
- iterate thorugh all the letters in the fen string while keeping track of the current index position of the board
- if the letter is "/", continue
- if the letter is a digit (identified thorugh ascii values), then i update the current index index by adding however amount the digit is
- if the letter is an actual leter, I add the corresponding peice to the board
- anything else, return (any other letter would mean invalid, and when encountered a space it means that the board portion of the fen string is finished)

Bitboard system:
- Have an bitboard for every types of peice on the map (black pawns, white rooks, etc)
- for Knight and King, have a specific move lookup table that is initialized off start. The table is used for quick move validation during game
  - Move table is the valid moves that type of peice can go on each position of the board
- For Pawns, the move table is computed at the end of round for the other player, because it takes account in the current state of the board and whos currently playing
- Changed canBitMoveTo function to do move validation through the above methods on Knight, King, and Pawns
- Updated BitMovedFrom function so it updates the bitboard at end of turn, and calls the next pawn bitboard iteration
- Added a generate all moves functions, which generates all possible moves for the player at that instance
