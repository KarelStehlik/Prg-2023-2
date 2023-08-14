// Sockfish.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <stack>
#include<istream>
#include <sstream>
#include<vector>
#include "StringStuff.h"
#include "ColorOut.h"
#include "libs/color-console-master/include/color.hpp"

using std::string;

enum Side { black, white };
enum playability { playable, unplayable, capture };

class Position {
public:
	int x, y;
	Position operator+(Position& p) {
		return  { x + p.x, y + p.y };
	}

	Position operator-(Position& p) {
		return  { x - p.x, y - p.y };
	}

	bool operator==(Position& p) {
		return x == p.x && y == p.y;
	}
	bool isValidPosition() {
		return 0 <= x && 0 <= y && 8 > x && 8 > y;
	}
	bool isValidMove() {
		return -7 <= x && -7 <= y && 7 >= x && 7 >= y;
	}

	int toArrayIndex() {
		return x + 8 * y;
	}
	static Position fromArrayIndex(int i) {
		return { i % 8, i / 8 };
	}
};

Position INVALID_POSITION = { -8888, 69420 };

class Ability {
public:
	constexpr static int CONDITION_CAPTURE_ONLY = 1;
	constexpr static int CONDITION_NO_CAPTURE = 1 << 1;
	constexpr static int ONLY_FIRST_MOVE = 1 << 2;
	constexpr static int DIRECTLY_PLAYABLE = 1 << 3;
	constexpr static int LAST_OPTION = 1 << 4;

	Ability* next, * prev;
	Position totalMovement;

private:
	int16_t properties;
	int readProperties(string input) {
		int posLen = 2;
		if (input[0] == '-') {
			totalMovement.x = -(input[1] - '0');
			posLen++;
		}
		else {
			totalMovement.x = input[0] - '0';
		}
		if (input[posLen - 1] == '-') {
			totalMovement.y = -(input[posLen] - '0');
			posLen++;
		}
		else {
			totalMovement.y = input[posLen - 1] - '0';
		}
		if (!totalMovement.isValidMove()) {
			std::cout << "Error: invalid move location '" << input[0] << input[1] << "'.";
		}

		int read = posLen;
		for (; read < input.size(); read++) {
			char c = input[read];
			if (c == 'c') {
				properties |= CONDITION_CAPTURE_ONLY;
			}
			else if (c == 'n') {
				properties |= CONDITION_NO_CAPTURE;
			}
			else if (c == 'f') {
				properties |= ONLY_FIRST_MOVE;
			}
			else if (c == '>') {
				break;
			}
		}
		return read;
	}
public:
	Ability() {
		totalMovement = { 0,0 };
		properties = DIRECTLY_PLAYABLE;
		next = NULL;
		prev = NULL;
	}

	void fillPreviousness(Ability* prev) {
		this->prev = prev;
		if (next != NULL) {
			next->fillPreviousness(this);
		}
		if (!hasProperty(LAST_OPTION)) {
			(this + 1)->fillPreviousness(prev);
		}
	}

	static Ability* parseString(string input) {
		string in = delChars(input, ' ');
		if (in[0] == '(') {
			in = in.substr(1, in.size() - 2);
		}
		std::vector<string> parts = splitRespectBrackets(in, ',');
		const int options = parts.size();
		Ability* a = new Ability[options];
		for (int i = 0; i < options; i++) {
			int read = a[i].readProperties(parts[i]);
			if (parts[i].size() > read + 2) {
				a[i].next = parseString(parts[i].substr(read + 1));
			}
		}
		a[options - 1].properties |= LAST_OPTION;
		a->fillPreviousness(NULL);
		return a;
	}

	void del() {
		for (Ability* ptr = this;; ptr++) {
			if (ptr->next != NULL) {
				ptr->next->del();
			}
			if (ptr->hasProperty(LAST_OPTION)) {
				break;
			}
		}
		delete[] this;
	}

	static Ability* copy(Ability* og) {
		int options = 1;
		for (int i = 0; !og[i].hasProperty(LAST_OPTION); i++) {
			options++;
		}
		Ability* a = new Ability[options];
		for (int i = 0; i < options; i++) {
			a[i].totalMovement = og[i].totalMovement;
			a[i].properties = og[i].properties;
			if (og[i].next != NULL) {
				a[i].next = copy(og[i].next);
			}
		}
		a->fillPreviousness(NULL);
		return a;
	}

	void invertXY() {
		totalMovement.y = -totalMovement.y;
		if (next != NULL) {
			next->invertXY();
		}
		if (!hasProperty(LAST_OPTION)) {
			(this + 1)->invertXY();
		}
	}

	bool hasProperty(int propertyConstant) {
		return properties & propertyConstant;
	}
	void print() {
		std::cout << totalMovement.x << totalMovement.y;
		if (next != NULL) {
			std::cout << ">";
			std::cout << '(';
			next->print();
			std::cout << ")";
		}
		if (!hasProperty(LAST_OPTION)) {
			std::cout << ", ";
			(this + 1)->print();
		}
	}


	class MoveIterator {
	private:
		Ability* current;

	public:
		MoveIterator(Ability* a) {
			current = a;
		}
		Ability* get() {
			return current;
		}
		Ability* next(playability currentPlayability) {
			if (current!=NULL && current->next != NULL && currentPlayability == playable) {
				current = current->next;
				return current;
			}
			while (current!=NULL && current->hasProperty(LAST_OPTION)) {
				current = current->prev;
			}
			if (current == NULL) {
				return NULL;
			}
			current++;
			return current;
		}
	};
	
	void testPrint() {
		MoveIterator iter = MoveIterator(this);
		
		do{
			Ability* a = iter.get();
			std::cout <<"(" << a->totalMovement.x << a->totalMovement.y << ")";
		} while (iter.next(playable) != NULL);
		std::cout << "\n";
	}
};

class PieceType {
public:
	char id;
	Ability* ability;
	Ability* invertedAbility;
	bool isKing;
	float estimatedValue;
	PieceType(char id, Ability* ability, bool isKing, float value) {
		this->id = id;
		this->ability = ability;
		this->invertedAbility = Ability::copy(ability);
		invertedAbility->invertXY();
		this->isKing = isKing;
		estimatedValue = value;
	}
};

std::vector<PieceType> pieceTypes;

bool isPiece(char c) {
	for (auto i = pieceTypes.begin(); i < pieceTypes.end(); i++) {
		if (i->id == c) {
			return true;
		}
	}
	return false;
}

class Piece {
public:
	char type;
	Side side;
	Ability* ability;
	Position position;
	bool hasMoved;
	bool isKing;
	float materialValue;
	bool alive = true;

	Piece(Position pos) {
		ability = NULL;
		position = pos;
		side = white;
		type = ' ';
		hasMoved = false;
		isKing = false;
	}

	Piece() {
		Piece({ 0,0 });
	}

	static Piece* create(char typeId, Side side) {
		PieceType* t = NULL;
		for (auto i = pieceTypes.begin(); i < pieceTypes.end(); i++) {
			if (i->id == typeId) {
				Piece* p = new Piece();
				p->side = side;
				p->ability = side == white ? i->ability : i->invertedAbility;
				p->type = typeId;
				p->materialValue = i->estimatedValue;
				return p;
			}
		}
		std::cout << "Error: no piece found with identifier '" << typeId << "'\n";
		return NULL;
	}

	void move(Position pos) {
		position = pos;
	}
};

class Board {
public:
	struct MoveRecord {
		Position from, to;
		Piece* captured;
		bool hasMovedBefore;
		int positionalEvalBefore;
	};

	Piece* squares[64];
	std::stack<MoveRecord> moveHistory;
	std::vector<Piece*> piecesWhite;
	std::vector<Piece*> piecesBlack;
	int materialWhite=0, materialBlack=0;
	bool whiteWon = false, blackWon = false;
	int positionalEval = 0;

	Board() {
		for (int i = 0; i < 64; i++) {
			squares[i] = NULL;
		}
	}

	void makeMove(Position from, Position to) {
		int fai = from.toArrayIndex(), tai = to.toArrayIndex();
		Piece* target = squares[tai];
		moveHistory.push({ from, to, target, squares[fai]->hasMoved , materialWhite - materialBlack });
		squares[fai]->move(to);
		squares[fai]->hasMoved = true;

		if (target!= NULL) {
			if (target->side == white) {
				materialWhite -= target->materialValue;
				blackWon = target->isKing;
			}
			else {
				materialBlack -= target->materialValue;
				whiteWon = target->isKing;
			}
			target->alive = false;
		}

		//positionalEval += 100 * (whiteWon - blackWon);
		
		squares[tai] = squares[fai];
		squares[fai] = NULL;
	}

	void unmakeMove() {
		MoveRecord move = moveHistory.top();
		moveHistory.pop();
		int fai = move.from.toArrayIndex(), tai = move.to.toArrayIndex();
		squares[tai]->move(move.from);
		squares[fai] = squares[tai];
		squares[fai]->hasMoved = move.hasMovedBefore;
		squares[tai] = move.captured;
		positionalEval = move.positionalEvalBefore;
		if (move.captured != NULL) {
			Piece* p = move.captured;
			if (p->side == white) {
				materialWhite += p->materialValue;
				blackWon = false;
			}
			else {
				materialBlack += p->materialValue;
				whiteWon = false;
			}
			p->alive = true;
		}
	}


	void addPiece(Position pos, char id, Side side) {
		Piece* p = Piece::create(id, side);
		if (p == NULL)return;
		p->move(pos);
		squares[pos.toArrayIndex()] = p;
		if (side == white) {
			piecesWhite.push_back(p);
			materialWhite += p->materialValue;
		}
		else {
			piecesBlack.push_back(p);
			materialBlack += p->materialValue;
		}
	}

	void loadFen(string fen) {
		std::vector<string> fields = split(fen, ' ');
		if (fields.size() != 6) {
			std::cout << "Error: fen should have 6 fields separated by spaces\n";
			return;
		}
		std::vector<string> rows = split(fields[0], '/');
		
		// validate the piece composition
		for (string row : rows) {
			int rowSize = row.length();
			for (char c : row) {
				if ('0' < c && c <= '9') {
					rowSize += c - '1';
				}
				else if (!isPiece(tolower(c))) {
					std::cout << "Error: no piece found with identifier " << c<<'\n';
				}
			}
			if (rowSize != 8) {
				std::cout << "Error: row should contain 8 pieces or spaces, contains "<<rowSize<<" instead: "<< row << '\n';
				return;
			}
		}

		// todo: validate fields tomove, castling, enpassant, hafmovereset, moves
		
		piecesWhite.clear();
		piecesBlack.clear();

		for (int i = 0; i < 64; i++) {
			// todo: delete pieces through piece list
			squares[i] = NULL;
		}

		Position placing = { 0,7 };
		for (string row : rows) {
			for (char c : row) {
				if ('0' < c && c <= '9') {
					placing.x += c-'0';
				}
				else {
					Side side = black;
					char lowercase = c;
					if (isupper(c)) {
						lowercase = tolower(c);
						side = white;
					}
					addPiece(placing, lowercase, side);
					placing.x++;
				}
			}
			placing.y--;
			placing.x = 0;
		}
	}

	void print() {
		std::cout << "\n "<<hue::aqua;
		for (int i = 0; i < 8; i++) {
			std::cout << (char)('A' + i) << "  ";
		}
		std::cout << "\n"<< hue::reset;
		for (int i = 7; i >=0 ; i--) {
			//std::cout << " -------------------------------   __\n";
			for (int j = 0; j < 8; j++) {
				int pieceIndex = Position{ j,i }.toArrayIndex();
				//std::cout << "|";
				if ((i + j) % 2 == 1) {
					std::cout << hue::on_grey;
				}
				else {
					std::cout << hue::on_white;
				}
				if (squares[pieceIndex] == NULL) {
					std::cout << "   ";
				}
				else {
					if (squares[pieceIndex]->side == white) {
						std::cout << hue::red << " " << (char)( squares[pieceIndex]->type + 'A' - 'a') << " ";
					}
					else {
						std::cout << hue::light_green <<" " << (char)(squares[pieceIndex]->type) << " ";
					}
				}
				std::cout << hue::reset;
			}
			std::cout << hue::aqua <<" " << i + 1 << hue::reset << "\n";
		}
		std::cout << "\n";
	}

	// checks if the target square is obstructed by something the ability can't move to
	playability isPlayable(Ability* a, Position startingSquare) {
		Piece* actor = squares[startingSquare.toArrayIndex()];
		if (actor == NULL || (actor->hasMoved && a->hasProperty(a->ONLY_FIRST_MOVE))) {
			return unplayable;
		}
		Side side = actor->side;
		Position to = startingSquare + a->totalMovement;
		int targetIndex = to.toArrayIndex();
		if (!to.isValidPosition()) {
			return unplayable;
		}

		Piece* target = squares[targetIndex];
		if (target == NULL) {
			if (a->hasProperty(a->CONDITION_CAPTURE_ONLY)) {
				return unplayable;
			}
			return playable;
		}

		if (target->side == side) {
			return unplayable;
		}

		if (a->hasProperty(a->CONDITION_NO_CAPTURE)) {
			return unplayable;
		}
		return capture;
	}

	bool searchPlayability(Position from, Ability* a, Position displacement) {

		Ability::MoveIterator iter = Ability::MoveIterator(a);
		while (iter.get() != NULL) {
			Ability* current = iter.get();

			playability check = isPlayable(current, from);
			if (check != unplayable && current->totalMovement == displacement) {
				return true;
			}
			iter.next(check);
		}
		return false;
	}

	bool isPlayable(Position from, Position to) {
		if (!from.isValidPosition() || !to.isValidPosition()) {
			return false;
		}
		Piece* p = squares[from.toArrayIndex()];
		if (p == NULL) {
			return false;
		}
		return searchPlayability(from, p->ability, to - from);
	}

	bool parseMove(string in) {
		int x1, y1, x2, y2;
		x1 = toupper( in[0] ) - 'A';
		y1 = in[1] - '0'-1;
		x2 = toupper(in[2]) - 'A';
		y2 = in[3] - '0'-1;
		if (!isPlayable({ x1,y1 }, { x2,y2 })) {
			std::cout << "that's not even a move, bozo\n";
			return false;
		}
		makeMove({ x1,y1 }, { x2,y2 });
		return true;
	}

	struct evaluation {
		float value;
		Position bestMoveFrom, bestMoveTo;
	};

	evaluation alphaBeta(int depth, Side toPlay, float alpha, float beta) {
		if (depth == 0) {
			// todo: some basic eval function
			float material = materialWhite - materialBlack;// +positionalEval;
			return { material , {-1,-1}, {-1,-1} };
		}

		evaluation best = { toPlay == white ? -INFINITY : INFINITY , {-1,-1}, {-1,-1} };

		std::vector<Piece*>& pieces = toPlay == white ? piecesWhite : piecesBlack;

		for (Piece* piece : pieces) {
			if (piece->alive) {
				Ability::MoveIterator iter = Ability::MoveIterator(piece->ability);
				Ability* current;
				while ((current = iter.get()) != NULL) {
					playability play = isPlayable(current, piece->position);
					if (play != unplayable && current->hasProperty(current->DIRECTLY_PLAYABLE)) {
						Position from = piece->position, to = piece->position + current->totalMovement;
						makeMove(from, to);
						// todo: special actions (castling, leaving en passant)

						if (toPlay == white) {
							float value = alphaBeta(depth - 1, black, alpha, beta).value;
							if (value > best.value) {
								best = { value, from, to };
							}
							if (value > beta) {
								unmakeMove();
								return best;
							}
							if (value > alpha) {
								alpha = value;
							}
						}
						else {
							float value = alphaBeta(depth - 1, white, alpha, beta).value;
							if (value < best.value) {
								best = { value, from, to };
							}
							if (value < alpha) {
								unmakeMove();
								return best;
							}
							if (value < beta) {
								beta = value;
							}
						}


						unmakeMove();
					}
					iter.next(play);
				}
			}
		}
		return best;
	}
};

int main()
{
	Ability* pawn = Ability::parseString("01n>02nf,11c,-11c");

	Ability* bishop = Ability::parseString("11>22>33>44>55>66>77,  -1-1>-2-2>-3-3>-4-4>-5-5>-6-6>-7-7,  -11>-22>-33>-44>-55>-66>-77,  1-1>2-2>3-3>4-4>5-5>6-6>7-7");

	Ability* king = Ability::parseString("11,10,01,-11,0-1,-10,-1-1,1-1");

	Ability* horse = Ability::parseString("12,21,-12,-21,1-2,2-1,-1-2,-2-1");

	Ability* queen = Ability::parseString("11>22>33>44>55>66>77,  -1-1>-2-2>-3-3>-4-4>-5-5>-6-6>-7-7,  -11>-22>-33>-44>-55>-66>-77,  1-1>2-2>3-3>4-4>5-5>6-6>7-7,  10>20>30>40>50>60>70,  -10>-20>-30>-40>-50>-60>-70,  01>02>03>04>05>06>07,  0-1>0-2>0-3>0-4>0-5>0-6>0-7");

	Ability* rook = Ability::parseString("10>20>30>40>50>60>70,  -10>-20>-30>-40>-50>-60>-70,  01>02>03>04>05>06>07,  0-1>0-2>0-3>0-4>0-5>0-6>0-7");

	pieceTypes.push_back(PieceType('p', pawn, false, 1));
	pieceTypes.push_back(PieceType('b', bishop, false, 3));
	pieceTypes.push_back(PieceType('k', king, true, 100));
	pieceTypes.push_back(PieceType('q', queen, false, 9));
	pieceTypes.push_back(PieceType('n', horse, false, 3));
	pieceTypes.push_back(PieceType('r', rook, false, 5));

	Board b;
	b.loadFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR - - - - -");
	//b.loadFen("r1b1k2r/p1p2ppp/pbn5/3p4/RP2n2q/2p1P3/5PPP/1NBQKBNR - - - - -");
	constexpr int depth = 6;
	while (1 == 1) {
		std::cout << "zero-depth eval: " << b.alphaBeta(0, white, -INFINITY, INFINITY).value << "\n";


		Board::evaluation eval = b.alphaBeta(depth, white, -INFINITY, INFINITY);
		std::cout << "alphabeta eval: " << eval.value << " " << eval.bestMoveFrom.x << eval.bestMoveFrom.y << eval.bestMoveTo.x << eval.bestMoveTo.y << '\n';


		b.makeMove(eval.bestMoveFrom, eval.bestMoveTo);
		b.print();
		string s; 
		std::getline(std::cin, s);
		while (!b.parseMove(s)) {
			std::getline(std::cin, s);
		};
	}
}