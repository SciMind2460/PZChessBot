#include "bitboard.hpp"
// #include "moves.hpp"

void print_bitboard(Bitboard board) {
	for (int i = 7; i >= 0; i--) {
		for (int j = 0; j < 8; j++) {
			std::cout << (((board >> (i * 8 + j)) & 1) ? "X " : ". ");
		}
		std::cout << '\n';
	}
}

std::string Move::to_string() const {
	if (data == 0)
		return "0000";
	std::string str = "";
	str += (char)('a' + (src() & 0b111));
	str += (char)('1' + (src() >> 3));
	str += (char)('a' + (dst() & 0b111));
	str += (char)('1' + (dst() >> 3));
	if ((data & 0xc000) == PROMOTION) {
		str += piecetype_letter[((data >> 12) & 0b11) + KNIGHT];
	}
	return str;
}

void Board::load_fen(std::string fen) {
	memset(piece_boards, 0, sizeof(piece_boards));
	memset(control, 0, sizeof(control));
	memset(mailbox, NO_PIECE, sizeof(mailbox));
	castling = NO_CASTLE;
	uint16_t rank = RANK_8;
	uint16_t file = FILE_A;
	int inputIdx = 0;
	// Load piece data
	while (rank <= RANK_8) { // Will catch wrap-around subtractions
		char cur = fen[inputIdx++];
		if (cur == '/') // Ignore slashes
			continue;
		if (cur <= '9') { // Character is a number indicating blank squares
			// Skip the required amount of squares
			file += cur - '0';
		} else { // Must be a piece
			// We ignore the first first part of the ASCII representation
			PieceType piece = letter_piece[(cur & 0x1f) - 2];
			piece_boards[piece] |= square_bits((Rank)rank, (File)file);
			// Uppercase in ASCII is 0b010xxxxx, lowercase is 0b011xxxxx
			// Chooses either [6] or [7] (white or black occupancy)
			piece_boards[(6 ^ 0b10) ^ (cur >> 5)] |= square_bits((Rank)rank, (File)file);
			mailbox[rank * 8 + file] = Piece(piece + ((cur >> 2) & 0b1000));
			file++;
		}
		// Fix overflow
		rank -= file >> 3;
		file &= 7;
	}
	// Set side according to FEN data ('w' (odd) for white, 'b' (even) for black)
	side = (~fen[inputIdx + 1]) & 1;
	inputIdx += 3;

	// Load castling rights
	if (fen[inputIdx] != '-') {
		if (fen[inputIdx] == 'K') {
			castling |= WHITE_OO;
			inputIdx++;
		}
		if (fen[inputIdx] == 'Q') {
			castling |= WHITE_OOO;
			inputIdx++;
		}
		if (fen[inputIdx] == 'k') {
			castling |= BLACK_OO;
			inputIdx++;
		}
		if (fen[inputIdx] == 'q') {
			castling |= BLACK_OOO;
			inputIdx++;
		}
		inputIdx++;
	} else {
		inputIdx += 2;
	}

	// Load EP square
	if (fen[inputIdx] != '-') {
		ep_square = Square((fen[inputIdx] - 'a') + 8 * (fen[inputIdx + 1] - '1'));
		inputIdx += 3;
	} else {
		inputIdx += 2;
	}

	// Load halfmove clock
	if (fen[inputIdx + 1] > '0') {
		halfmove_clock = (fen[inputIdx] - '0') * 10 + (fen[inputIdx + 1] - '0');
		inputIdx += 3;
	} else {
		halfmove_clock = (fen[inputIdx] - '0');
		inputIdx += 2;
	}
	// Ignore the rest (who cares anyways)
}

void Board::print_board() {
#ifndef NODEBUG
	char print[64];
	// Start at -1 because we increment before processing to guarantee it happens in every case
	int printIdx = -1;
	// Occupancy
	Bitboard occ = piece_boards[6] | piece_boards[7];
	// Used for sanity checks during debugging (piece set but no occupancy)
	Bitboard sanity = (piece_boards[0] | piece_boards[1] | piece_boards[2] | piece_boards[3] | piece_boards[4] | piece_boards[5]) ^ occ;
	for (int rank = RANK_8; rank >= 0; rank--) {
		for (int file = FILE_A; file <= FILE_H; file++) {
			Bitboard bits = square_bits((Rank)rank, (File)file);
			printIdx++;
			if (mailbox[rank * 8 + file] != NO_PIECE && !(occ & square_bits((Rank)rank, (File)file))) { // Occupancy and mailbox differ
				print[printIdx] = '$';
				continue;
			}
			if (sanity & bits) { // Occupancy and collective piece boards differ
				if (occ & bits) // Occupied but no piece specified
					print[printIdx] = '?';
				else // Piece specified but no occupancy
					print[printIdx] = '!';
				continue;
			}
			if (!(occ & bits)) {
				print[printIdx] = '.';
				continue;
			}

			bool put = false;
			// Scan through piece types trying to find one that matches
			for (int type = PAWN; type < NO_PIECETYPE; type++) {
				if (!(piece_boards[type] & bits))
					continue;
				if (put) { // More than two pieces on one square
					print[printIdx] = '&';
					break;
				}
				put = true;
				if (mailbox[rank * 8 + file] != type + (!!(piece_boards[7] & bits) << 3)) { // Bitboard and mailbox representations differ
					print[printIdx] = '$';
					break;
				}
				print[printIdx] = piecetype_letter[type] + (!!(piece_boards[7] & bits) << 5);
			}
		}
	}
	// Print the board
	for (int i = 0; i < 64; i++) {
		std::cout << print[i];
		if (i % 8 == 7)
			std::cout << '\n';
		else
			std::cout << ' ';
	}
#else
	for (int rank = RANK_8; rank >= 0; rank--) {
		for (int file = FILE_A; file <= FILE_H; file++) {
			std::cout << piece_letter[mailbox[rank * 8 + file]];
			if (file == FILE_H)
				std::cout << '\n';
			else
				std::cout << ' ';
		}
	}
#endif
}

void Board::make_move(Move move) {
	// Add move to move history
	move_hist.push(HistoryEntry(move, mailbox[move.dst()], castling, ep_square, halfmove_clock));
	Square tmp_ep_square = SQ_NONE;
	if (move.data == 0) {
		// Null move, do nothing, just change sides
	} else if (move.type() == PROMOTION) {
		// Handle captures
		if (piece_boards[OPPOCC(side)] & square_bits(move.dst())) { // If opposite occupancy bit set on destination (capture)
			// Remove whatever piece it was
			uint8_t piece = mailbox[move.dst()] & 0b111;
			piece_boards[piece] ^= square_bits(move.dst());
			piece_boards[OPPOCC(side)] ^= square_bits(move.dst());
			halfmove_clock = 0;
		} else {
			halfmove_clock += 1;
		}
		// Remove the pawn on the src and add the piece on the dst
		mailbox[move.src()] = NO_PIECE;
		mailbox[move.dst()] = Piece(move.promotion() + ((!!side) << 3) + KNIGHT);
		piece_boards[PAWN] ^= square_bits(move.src());
		piece_boards[OCC(side)] ^= square_bits(move.src()) | square_bits(move.dst());
		piece_boards[move.promotion() + KNIGHT] ^= square_bits(move.dst());
	} else if (move.type() == EN_PASSANT) {
		// Remove the pawn on the src and the taken pawn, then add the pawn on the dst
		mailbox[move.dst()] = mailbox[move.src()];
		mailbox[move.src()] = NO_PIECE;
		mailbox[(move.src() & 0b111000) | (move.dst() & 0b111)] = NO_PIECE;
		piece_boards[PAWN] ^= square_bits(move.src()) | square_bits(move.dst()) | square_bits(Rank(move.src() >> 3), File(move.dst() & 0b111));
		piece_boards[OCC(side)] ^= square_bits(move.src()) | square_bits(move.dst());
		piece_boards[OPPOCC(side)] ^= square_bits(Rank(move.src() >> 3), File(move.dst() & 0b111));
		halfmove_clock = 0;
	} else if (move.type() == castling) {
		// Calculate where the rook is
		Bitboard rook_mask;
		mailbox[move.dst()] = mailbox[move.src()];
		mailbox[move.src()] = NO_PIECE;
		if ((move.dst() & 0b111) == FILE_C) {
			rook_mask = square_bits(Rank(move.src() >> 3), FILE_A);
			rook_mask |= rook_mask << 3;
			mailbox[move.dst() + 1] = mailbox[move.dst() - 2];
			mailbox[move.dst() - 2] = NO_PIECE;
		} else {
			rook_mask = square_bits(Rank(move.src() >> 3), FILE_H);
			rook_mask |= rook_mask >> 2;
			mailbox[move.dst() - 1] = mailbox[move.dst() + 1];
			mailbox[move.dst() + 1] = NO_PIECE;
		}
		piece_boards[OCC(side)] ^= square_bits(move.src()) ^ square_bits(move.dst()) ^ rook_mask;
		piece_boards[KING] ^= square_bits(move.src()) | square_bits(move.dst());
		piece_boards[ROOK] ^= rook_mask;
		// Remove castling rights
		castling &= ~((WHITE_OO | WHITE_OOO) << (side << 1));
		halfmove_clock += 1;
	} else {
		// Handle captures
		if (piece_boards[OPPOCC(side)] & square_bits(move.dst())) { // If opposite occupancy bit set on destination (capture)
			// Remove whatever piece it was
			uint8_t piece = mailbox[move.dst()] & 0b111;
			piece_boards[piece] ^= square_bits(move.dst());
			piece_boards[OPPOCC(side)] ^= square_bits(move.dst());
			halfmove_clock = 0; 
		} else {
			halfmove_clock += 1;
		}
		// Get piece that is moving
		uint8_t piece = mailbox[move.src()] & 0b111;
		// Update mailbox repr first
		mailbox[move.dst()] = mailbox[move.src()];
		mailbox[move.src()] = NO_PIECE;
		// Update piece and occupancy bitboard
		piece_boards[piece] ^= square_bits(move.src()) | square_bits(move.dst());
		piece_boards[OCC(side)] ^= square_bits(move.src()) | square_bits(move.dst());
	
		// Handle castling rights
		if (piece == KING) 
			castling &= ~((WHITE_OO | WHITE_OOO) << (side << 1));
		} else if (piece == ROOK) {
			if (move.src() == SQ_A1 || move.dst() == SQ_A1)
				castling &= ~WHITE_OOO;
			else if (move.src() == SQ_H1 || move.dst() == SQ_H1)
				castling &= ~WHITE_OO;
			else if (move.src() == SQ_A8 || move.dst() == SQ_A8)
				castling &= ~BLACK_OOO;
			else if (move.src() == SQ_H8 || move.dst() == SQ_H8)
				castling &= ~BLACK_OO;
		} else {
			// Set EP square if applicable
			if (piece == PAWN && ((move.src() - move.dst()) & 0b1111) == 0)
				tmp_ep_square = Square((move.src() + move.dst()) >> 1);
			else
				tmp_ep_square = SQ_NONE;
		}
	}
	// Update control bitboards
	// if (move != NullMove)
	// 	update_control(move.src(), move.dst());
	// Update EP square
	ep_square = tmp_ep_square;
	// Switch sides
	side = !side;
}

void Board::unmake_move() {
	// Switch sides first
	side = !side;
	HistoryEntry prev = move_hist.top();
	move_hist.pop();
	Move move = prev.move();
	halfmove_clock = prev.halfmove_clock()
	if (move.data == 0) {
		// Null move, do nothing on the board, but recover metadata
	} else if (move.type() == PROMOTION) {
		// Remove the piece on the dst and add the pawn on the src
		mailbox[move.src()] = Piece(PAWN + ((!!side) << 3));
		mailbox[move.dst()] = prev.prev_piece();
		piece_boards[PAWN] ^= square_bits(move.src());
		piece_boards[OCC(side)] ^= square_bits(move.src()) | square_bits(move.dst());
		piece_boards[((move.data >> 12) & 0b11) + KNIGHT] ^= square_bits(move.dst());
	} else if (move.type() == EN_PASSANT) {
		// Remove the pawn on the dst and add the pawn on the src and the taken pawn
		mailbox[move.src()] = mailbox[move.dst()];
		mailbox[move.dst()] = NO_PIECE;
		mailbox[(move.src() & 0b111000) | (move.dst() & 0b111)] = Piece(WHITE_PAWN + ((!side) << 3));
		piece_boards[PAWN] ^= square_bits(move.src()) | square_bits(move.dst()) | square_bits(Rank(move.src() >> 3), File(move.dst() & 0b111));
		piece_boards[OCC(side)] ^= square_bits(move.src()) | square_bits(move.dst());
		piece_boards[OPPOCC(side)] ^= square_bits(Rank(move.src() >> 3), File(move.dst() & 0b111));
	} else if (move.type() == castling) {
		// Calculate where the rook was
		Bitboard rook_mask;
		mailbox[move.src()] = mailbox[move.dst()];
		mailbox[move.dst()] = NO_PIECE;
		if ((move.dst() & 0b111) == FILE_C) {
			rook_mask = square_bits(Rank(move.src() >> 3), FILE_A);
			rook_mask |= rook_mask << 3;
			mailbox[move.dst() - 2] = mailbox[move.dst() + 1];
			mailbox[move.dst() + 1] = NO_PIECE;
		} else {
			rook_mask = square_bits(Rank(move.src() >> 3), FILE_H);
			rook_mask |= rook_mask >> 2;
			mailbox[move.dst() + 1] = mailbox[move.dst() - 1];
			mailbox[move.dst() - 1] = NO_PIECE;
		}
		piece_boards[OCC(side)] ^= square_bits(move.src()) ^ square_bits(move.dst()) ^ rook_mask;
		piece_boards[KING] ^= square_bits(move.src()) | square_bits(move.dst());
		piece_boards[ROOK] ^= rook_mask;
	} else {
		// Get piece that is moving
		uint8_t piece = mailbox[move.dst()] & 0b111;
		// Update mailbox repr first
		mailbox[move.src()] = mailbox[move.dst()];
		mailbox[move.dst()] = prev.prev_piece();
		// Update piece and occupancy bitboard
		piece_boards[piece] ^= square_bits(move.src()) | square_bits(move.dst());
		piece_boards[OCC(side)] ^= square_bits(move.src()) | square_bits(move.dst());
		// Handle captures
		if (prev.prev_piece() != NO_PIECE) { // If there was a capture
			// Add whatever piece it was
			piece = prev.prev_piece() & 0b111;
			piece_boards[piece] ^= square_bits(move.dst());
			piece_boards[OPPOCC(side)] ^= square_bits(move.dst());
		}
	}
	// Update control bitboards
	// if (move != NullMove)
	// 	update_control(move.dst(), move.src());
	// Update EP square
	ep_square = prev.prev_ep();
	// Update castling rights
	castling = prev.prev_castling();
}

Bitboard Board::hash() const {
	Bitboard hash = 0;
	// for (int i = 0; i < 6; i++) {
	// 	hash ^= pieces[i];
	// }
	// hash ^= castling;
	// hash ^= ep_square;
	// hash ^= control[0];
	// hash ^= control[1];
	// hash ^= side << 62;
	return hash;
}
