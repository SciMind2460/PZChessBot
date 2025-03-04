#pragma once

#include "includes.hpp"

// Selects the occupancy array by xoring 6 with side (white: false = 0 ^ 6 = 6, black: true = 1 ^ 6 = 7)
#define OCC(side) (6 ^ (side))
#define OPPOCC(side) (7 ^ (side))

constexpr Bitboard FileABits = 0x0101010101010101ULL;
constexpr Bitboard FileBBits = FileABits << 1;
constexpr Bitboard FileCBits = FileABits << 2;
constexpr Bitboard FileDBits = FileABits << 3;
constexpr Bitboard FileEBits = FileABits << 4;
constexpr Bitboard FileFBits = FileABits << 5;
constexpr Bitboard FileGBits = FileABits << 6;
constexpr Bitboard FileHBits = FileABits << 7;

constexpr Bitboard Rank1Bits = 0xff;
constexpr Bitboard Rank2Bits = Rank1Bits << (8 * 1);
constexpr Bitboard Rank3Bits = Rank1Bits << (8 * 2);
constexpr Bitboard Rank4Bits = Rank1Bits << (8 * 3);
constexpr Bitboard Rank5Bits = Rank1Bits << (8 * 4);
constexpr Bitboard Rank6Bits = Rank1Bits << (8 * 5);
constexpr Bitboard Rank7Bits = Rank1Bits << (8 * 6);
constexpr Bitboard Rank8Bits = Rank1Bits << (8 * 7);

extern Bitboard pseudoAttacks[6][64];

// clang-format off
constexpr Bitboard square_bits(Square x) { return 1ULL << x; }
constexpr Bitboard square_bits(Rank r, File f) { return 1ULL << (r * 8 + f); }
// clang-format on

void print_bitboard(Bitboard);

// A move needs 16 bits to be stored
// bits 0-5: Destination square (from 0 to 63)
// bits 6-11: Origin (from 0 to 63)
// bits 12-13: promotion piece type - 1 (from KNIGHT-1 to QUEEN-1)
// bits 14-15: special move flag: promotion (1), en passant (2), castling (3)
struct Move {
	uint16_t data;
	constexpr explicit Move(uint16_t d) : data(d) {}
	constexpr Move(int from, int to) : data((from << 6) | to) {}
	template <MoveType T> static constexpr Move make(int from, int to, PieceType pt = KNIGHT) {
		return Move(T | ((pt - KNIGHT) << 12) | (from << 6) | to);
	}
	constexpr Square src() const {
		return (Square)(data >> 6 & 0x3f);
	};
	constexpr Square dst() const {
		return (Square)(data & 0x3f);
	};
	constexpr PieceType promotion() const {
		return (PieceType)((data >> 12) & 0b11);
	};
	constexpr MoveType type() const {
		return (MoveType)(data & 0xc000);
	};
	constexpr bool operator==(const Move &m) const {
		return data == m.data;
	};
	constexpr bool operator!=(const Move &m) const {
		return data != m.data;
	};
	std::string to_string() const;
};

static constexpr Move NullMove = Move(0);

// A history entry stores the move and other information needed to unmake it
// bits 0-15: move
// bits 16-19: captured piece
// bits 20-23: previous castling rights
// bits 24-29: previous en passant square
// bits 29-38: previous halfmove clock
struct HistoryEntry {
	uint32_t data;
	constexpr explicit HistoryEntry(uint32_t d) : data(d) {}
	constexpr HistoryEntry(Move m, Piece prev_piece, uint8_t prev_castling, Square prev_ep, uint16_t prev_halfmove_clock)
		: data(m.data | ((uint32_t)prev_piece << 16) | ((uint32_t)prev_castling << 20) | ((uint32_t)prev_ep << 24) | ((uint32_t)prev_halfmove_clock << 28) {}
	constexpr Move move() {
		return Move(data & 0xffff);
	}
	constexpr Piece prev_piece() {
		return Piece((data >> 15) & 0b1111);
	}
	constexpr uint8_t prev_castling() {
		return (data >> 19) & 0b1111;
	}
	constexpr Square prev_ep() {
		return Square((data >> 23) & 0b111111);
	}
        constexpr uint16_t prev_halfmove() {
		return (data >> 28) & 0b1111111111;
	}
};

struct Board {
	// pawns, knights, bishops, rooks, queens, kings, w_occupancy, b_occupancy
	Bitboard piece_boards[8] = {0};
	// w_control, b_control
	Bitboard control[2] = {0};
	bool side = WHITE;
	uint8_t castling = 0xf;
	Square ep_square = SQ_NONE;
        uint16_t halfmove_clock = 0;

	// Mailbox representation of the board for faster queries of certain data
	Piece mailbox[8 * 8] = {WHITE_ROOK, WHITE_KNIGHT, WHITE_BISHOP, WHITE_QUEEN, WHITE_KING, WHITE_BISHOP, WHITE_KNIGHT, WHITE_ROOK,
							WHITE_PAWN, WHITE_PAWN,	  WHITE_PAWN,	WHITE_PAWN,	 WHITE_PAWN, WHITE_PAWN,   WHITE_PAWN,	 WHITE_PAWN,
							NO_PIECE,	NO_PIECE,	  NO_PIECE,		NO_PIECE,	 NO_PIECE,	 NO_PIECE,	   NO_PIECE,	 NO_PIECE,
							NO_PIECE,	NO_PIECE,	  NO_PIECE,		NO_PIECE,	 NO_PIECE,	 NO_PIECE,	   NO_PIECE,	 NO_PIECE,
							NO_PIECE,	NO_PIECE,	  NO_PIECE,		NO_PIECE,	 NO_PIECE,	 NO_PIECE,	   NO_PIECE,	 NO_PIECE,
							NO_PIECE,	NO_PIECE,	  NO_PIECE,		NO_PIECE,	 NO_PIECE,	 NO_PIECE,	   NO_PIECE,	 NO_PIECE,
							BLACK_PAWN, BLACK_PAWN,	  BLACK_PAWN,	BLACK_PAWN,	 BLACK_PAWN, BLACK_PAWN,   BLACK_PAWN,	 BLACK_PAWN,
							BLACK_ROOK, BLACK_KNIGHT, BLACK_BISHOP, BLACK_QUEEN, BLACK_KING, BLACK_BISHOP, BLACK_KNIGHT, BLACK_ROOK};

	// Moves with extra information (taken piece etc..)
	// better documentation will be included later
	std::stack<HistoryEntry> move_hist;

	Board() {
		// Load starting position
		piece_boards[0] = Rank2Bits | Rank7Bits;
		piece_boards[1] = square_bits(SQ_B1) | square_bits(SQ_G1) | square_bits(SQ_B8) | square_bits(SQ_G8);
		piece_boards[2] = square_bits(SQ_C1) | square_bits(SQ_F1) | square_bits(SQ_C8) | square_bits(SQ_F8);
		piece_boards[3] = square_bits(SQ_A1) | square_bits(SQ_H1) | square_bits(SQ_A8) | square_bits(SQ_H8);
		piece_boards[4] = square_bits(SQ_D1) | square_bits(SQ_D8);
		piece_boards[5] = square_bits(SQ_E1) | square_bits(SQ_E8);
		piece_boards[6] = Rank1Bits | Rank2Bits;
		piece_boards[7] = Rank7Bits | Rank8Bits;
	}
	Board(std::string fen) {
		load_fen(fen);
	};

	void load_fen(std::string);
	void print_board();

	void make_move(Move);
	void unmake_move();

	void legal_moves(std::vector<Move> &) const;

	Bitboard hash() const;
};
