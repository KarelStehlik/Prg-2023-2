// Sockfish.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <stack>
#include<istream>
#include <sstream>
#include<vector>
#include<unordered_map>
#include<chrono>
#include "StringStuff.h"
#include "ColorOut.h"
#include "libs/color-console-master/include/color.hpp"
#include "Zobrist.h"

using std::string;

enum playability { playable, unplayable, capture };

class PieceType;

PieceType* getPieceType(char c);

class Position {
public:
	int x, y;
	Position operator+(const Position& p) {
		return  { x + p.x, y + p.y };
	}

	Position operator-(const Position& p) {
		return  { x - p.x, y - p.y };
	}

	bool operator==(const Position& p) {
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
	PieceType* promotionType = NULL;


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
			std::cout << "Error: invalid move location '" << input[0] << input[1] << "'.\n";
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
			else if (c == '=') {
				if (read == input.size()) {
					std::cout << "Error: move description ending with '=' (" << input << ")\n";
				}
				read++;
				promotionType = getPieceType(input[read]);
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
		promotionType = NULL;
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

	static Ability* combine(const std::vector<Ability*>& in) {
		int totalSize = in.size();
		for (Ability* a : in) {
			while (!a->hasProperty(LAST_OPTION)) {
				totalSize++;
				a++;
			}
		}
		Ability* result = new Ability[totalSize];
		int i = 0;
		for (Ability* a : in) {
			while (!a->hasProperty(LAST_OPTION)) {
				result[i] = (*a);
				i++;
				a++;
			}
			result[i] = (*a);
			result[i].properties ^= LAST_OPTION;
			i++;
		}
		result[totalSize - 1].properties ^= LAST_OPTION;
		result->fillPreviousness(NULL);
		return result;
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
			a[i].promotionType = og[i].promotionType;
			if (og[i].next != NULL) {
				a[i].next = copy(og[i].next);
			}
		}
		a->fillPreviousness(NULL);
		return a;
	}

	void invertY() {
		totalMovement.y = -totalMovement.y;
		if (next != NULL) {
			next->invertY();
		}
		if (!hasProperty(LAST_OPTION)) {
			(this + 1)->invertY();
		}
	}

	void invertX() {
		totalMovement.x = -totalMovement.x;
		if (next != NULL) {
			next->invertX();
		}
		if (!hasProperty(LAST_OPTION)) {
			(this + 1)->invertX();
		}
	}

	void rotate() {
		int x = totalMovement.x;
		totalMovement.x = -totalMovement.y;
		totalMovement.y = x;
		if (next != NULL) {
			next->rotate();
		}
		if (!hasProperty(LAST_OPTION)) {
			(this + 1)->rotate();
		}
	}

	inline bool hasProperty(int propertyConstant) {
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

	bool eq(const Ability* other) {
		if (!(totalMovement == other->totalMovement) || properties != other->properties) {
			return false;
		}
		if (next != NULL) {
			if (other->next == NULL) {
				return false;
			}
			if (!next->eq(other->next)) {
				return false;
			}
		}
		if (hasProperty(LAST_OPTION)) {
			return true;
		}
		return (this + 1)->eq(other + 1);
	}

	void check() {
		print();
		std::cout << "\n";
		MoveIterator iter = MoveIterator(this);
		while (iter.get() != NULL) {
			std::cout << "(" << iter.get()->totalMovement.x << "," << iter.get()->totalMovement.y << ") ";
			iter.next(playable);
		}
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
		invertedAbility->invertY();
		this->isKing = isKing;
		estimatedValue = value;
	}
};

std::vector<PieceType*> pieceTypes;

PieceType* getPieceType(char c) {
	for (int i = 0; i < pieceTypes.size();i++) {
		if (pieceTypes[i]->id == c) {
			return pieceTypes[i];
		}
	}
	return NULL;
}

class Piece {
public:
	char typeId;
	bool isBlack;
	Ability* ability;
	Position position;
	bool hasMoved;
	bool isKing;
	float materialValue;
	bool alive = true;
	int search_valueIfMoves;
	PieceType* pieceType;

	Piece(PieceType* t, Position pos, bool black) {
		ability = NULL;
		position = pos;
		typeId = ' ';
		hasMoved = false;
		isKing = t->isKing;
		search_valueIfMoves = 0;
		materialValue = 0;

		isBlack = black;
		updateType(t);
	}

	void updateType(PieceType *t) {
		ability = isBlack ? t->invertedAbility: t->ability;
		typeId = t->id;
		materialValue = t->estimatedValue;
		pieceType = t;
	}

	static Piece* create(char typeId, bool black) {
		PieceType* t = getPieceType(typeId);
		if (t != NULL) {
			return new Piece(t, { 0,0 }, black);
		}
		std::cout << "Error: no piece found with identifier '" << typeId << "'\n";
		return NULL;
	}

	void move(Position pos) {
		position = pos;
	}

	uint64_t hash() {
		return getHash(typeId, position.toArrayIndex(), isBlack, hasMoved);
	}
};

class Board {
public:
	struct MoveRecord {
		Position from, to;
		Piece* captured;
		bool hasMovedBefore;
		int positionalEvalBefore;
		PieceType* promotedFrom;
	};

	Piece* squares[64];
	std::stack<MoveRecord> moveHistory;
	std::vector<Piece*> piecesWhite;
	std::vector<Piece*> piecesBlack;
	int materialWhite=0, materialBlack=0;
	bool whiteWon = false, blackWon = false;
	int positionalEval = 0;
	uint64_t hash = 0;
	int move = 0;
	bool blackToPlay = false;
	static constexpr float MATE = 10000;
	static inline bool isMate(int eval) {
		return eval > 1000 || eval < -1000;
	}

	struct evaluation {
		float value;
		Piece* bestMoveFrom;
		Ability* bestMove;
	};

	struct Transposition {
		int depth;
		int move;
		evaluation eval;
		//string fen;
	};

	std::unordered_map<uint64_t, Transposition> transpositionTable;

	string fen() {
		string result = "";
		for (int i = 0; i < 8; i++) {
			int empty = 0;
			for (int j = 0; j < 8; j++) {
				Piece* sq = squares[Position{ j, 7 - i }.toArrayIndex()];
				if (sq == NULL) {
					empty++;
				}
				else {
					if (empty != 0) {
						result.append(std::to_string(empty));
						empty = 0;
					}
					result += sq->isBlack ?  sq->typeId : std::toupper(sq->typeId);
				}
			}
			if (empty != 0) {
				result.append(std::to_string(empty));
				empty = 0;
			}
			if (i < 7) {
				result += '/';
			}
		}
		return result;
	}

	Board() {
		for (int i = 0; i < 64; i++) {
			squares[i] = NULL;
		}
	}

	void makeMove(Position from, Ability *ability) {
		move++;
		blackToPlay = !blackToPlay;
		hash ^= getPlayerHash();
		Position to = from + ability->totalMovement;
		int fai = from.toArrayIndex(), tai = to.toArrayIndex();
		Piece* target = squares[tai], *moving=squares[fai];

		hash ^= moving->hash();

		moveHistory.push({ from, to, target, moving->hasMoved , materialWhite - materialBlack, moving->pieceType });

		if ((to.y == 7 || to.y == 0) && ability->promotionType!=NULL) {
			moving->updateType(ability->promotionType);
		}
		moving->move(to);
		moving->hasMoved = true;

		if (target!= NULL) {
			hash ^= target->hash();
			if (target->isBlack) {
				materialBlack -= target->materialValue;
				whiteWon = target->isKing;
			}
			else {
				materialWhite -= target->materialValue;
				blackWon = target->isKing;
			}
			target->alive = false;
		}

		//positionalEval += 100 * (whiteWon - blackWon);
		
		squares[tai] = moving;
		squares[fai] = NULL;

		hash ^= moving->hash();
	}

	void unmakeMove() {
		move--;
		blackToPlay = !blackToPlay;
		hash ^= getPlayerHash();
		MoveRecord move = moveHistory.top();
		moveHistory.pop();
		int fai = move.from.toArrayIndex(), tai = move.to.toArrayIndex();
		Piece* moving = squares[tai];

		hash ^= moving->hash();

		moving->move(move.from);
		squares[fai] = moving;
		moving->hasMoved = move.hasMovedBefore;

		if (moving->pieceType != move.promotedFrom) {
			moving->updateType(move.promotedFrom);
		}
		squares[tai] = move.captured;
		positionalEval = move.positionalEvalBefore;
		if (move.captured != NULL) {
			Piece* p = move.captured;
			hash ^= p->hash();
			if (p->isBlack) {
				materialBlack += p->materialValue;
				whiteWon = false;
			}
			else {
				materialWhite += p->materialValue;
				blackWon = false;
			}
			p->alive = true;
		}
		hash ^= moving->hash();
	}


	void addPiece(Position pos, char id, bool isBlack) {
		Piece* p = Piece::create(id, isBlack);
		if (p == NULL)return;
		p->move(pos);
		squares[pos.toArrayIndex()] = p;
		if (isBlack) {
			piecesBlack.push_back(p);
			materialBlack += p->materialValue;
		}
		else {
			piecesWhite.push_back(p);
			materialWhite += p->materialValue;
		}
		hash ^= p->hash();
	}

	void loadFen(string fen) {
		whiteWon = false;
		blackWon = false;
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
				else if (getPieceType(tolower(c))==NULL) {
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
					bool black=true;
					char lowercase = c;
					if (isupper(c)) {
						lowercase = tolower(c);
						black=false;
					}
					addPiece(placing, lowercase, black);
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
				auto lastMove = moveHistory.top();
				if (lastMove.from == Position{j, i} || lastMove.to == Position{j, i}) {
					std::cout << hue::on_light_yellow;
				}else if ((i + j) % 2 == 0) {
					std::cout << hue::on_grey;
				}
				else {
					std::cout << hue::on_white;
				}
				if (squares[pieceIndex] == NULL) {
					std::cout << "   ";
				}
				else {
					if (squares[pieceIndex]->isBlack) {
						std::cout << hue::light_green << " " << (char)(squares[pieceIndex]->typeId) << " ";
					}
					else {
						std::cout << hue::red << " " << (char)(squares[pieceIndex]->typeId + 'A' - 'a') << " ";
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
		bool blackMove = actor->isBlack;
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

		if (target->isBlack == blackMove) {
			return unplayable;
		}

		if (a->hasProperty(a->CONDITION_NO_CAPTURE)) {
			return unplayable;
		}
		return capture;
	}

	Ability* searchPlayability(Position from, Ability* a, Position displacement) {

		Ability::MoveIterator iter = Ability::MoveIterator(a);
		while (iter.get() != NULL) {
			Ability* current = iter.get();

			playability check = isPlayable(current, from);
			if (check != unplayable && current->totalMovement == displacement) {
				return current;
			}
			iter.next(check);
		}
		return NULL;
	}

	Ability* isPlayable(Position from, Position to) {
		if (!from.isValidPosition() || !to.isValidPosition()) {
			return NULL;
		}
		Piece* p = squares[from.toArrayIndex()];
		if (p == NULL) {
			return NULL;
		}
		return searchPlayability(from, p->ability, to - from);
	}

	bool parseMove(string in) {
		if (in == "takeback") {
			unmakeMove();
			unmakeMove();
			return false;
		}
		std::vector<std::string> words = splitOffFirstWord(in, ' ');
		if (words[0] == "fen") {
			if (words.size() != 2) {
				std::cout << "error";
				return false;
			}
			loadFen(words[1]);
			return true;
		}

		int x1, y1, x2, y2;
		x1 = toupper( in[0] ) - 'A';
		y1 = in[1] - '0'-1;
		x2 = toupper(in[2]) - 'A';
		y2 = in[3] - '0'-1;
		Ability* p = isPlayable({ x1,y1 }, { x2,y2 });
		if (p==NULL) {
			std::cout << "that's not even a move, bozo\n";
			return false;
		}
		makeMove({ x1,y1 }, p);
		return true;
	}

	void storeTransposition(Transposition trans) {
		transpositionTable[hash] = trans;
	}

	int maxMoveReached = 0;

	Transposition* getTransposition() {
		auto found = transpositionTable.find(hash);
		if (found == transpositionTable.end()) {
			return NULL;
		}

		float& eval = found->second.eval.value;
		int &m = found->second.move;
		if (isMate(eval) && m!=move) {
			if (eval < 0) {
				eval += move-m;
			}
			else {
				eval -= move-m;
			}
			m = move;
		}
		return &found->second;
	}

	//testing fen 2b4r/p4pkp/6p1/3p2P1/7N/2BP4/PPP3rK/RN5R - - - - -
	evaluation alphaBeta(int depth, float alpha, float beta) {
		if (blackWon || whiteWon) {
			return { (blackWon ? -MATE+move : MATE-move) , NULL, NULL};
		}

		Transposition* done = getTransposition();
		//if (done!=NULL && done->fen != fen()) {
		//	std::cout << "hash collision\n";
		//}
		if (done!=NULL && done->depth >= depth) {
			return done->eval;
		}

		if (depth == 0) {
			// todo: some basic eval function
			float material = materialWhite - materialBlack;// +positionalEval;

			float movesWhite = 0;
			for (Piece* piece : piecesWhite) {
				if (piece->alive) {
					Ability::MoveIterator iter = Ability::MoveIterator(piece->ability);
					Ability* current;
					while ((current = iter.get()) != NULL) {
						playability play = isPlayable(current, piece->position);
						iter.next(play);
						movesWhite++;
					}
				}
			}
			float movesBlack = 0;
			for (Piece* piece : piecesBlack) {
				if (piece->alive) {
					Ability::MoveIterator iter = Ability::MoveIterator(piece->ability);
					Ability* current;
					while ((current=iter.get()) != NULL) {
						playability play = isPlayable(current, piece->position);
						iter.next(play);
						movesBlack++;
					}
				}
			}
			return { material +(movesWhite-movesBlack)/10, NULL, NULL };
		}

		evaluation best = { blackToPlay ? INFINITY : -INFINITY , NULL,NULL };

		std::vector<Piece*>& pieces = blackToPlay ? piecesBlack : piecesWhite;
		bool couldBeStalemate=true;

		for (Piece* piece : pieces) {
			if (piece->alive) {
				Ability::MoveIterator iter = Ability::MoveIterator(piece->ability);
				Ability* current;
				while ((current = iter.get()) != NULL) {
					playability play = isPlayable(current, piece->position);
					if (play != unplayable && current->hasProperty(current->DIRECTLY_PLAYABLE)) {
						Position from = piece->position, to = piece->position + current->totalMovement;
						makeMove(from, current);

						// todo: special actions (castling, leaving en passant)

						if (blackToPlay) {
							float value = alphaBeta(depth - 1, alpha, beta).value;
							if (value != -MATE+move+2) {
								couldBeStalemate = false;
							}
							if (value > best.value) {
								best = { value, piece, current };
								piece->search_valueIfMoves++;
							}
							if (value >= beta) {
								unmakeMove();
								return best;
							}
							if (value > alpha) {
								alpha = value;
							}
						}
						else {
							float value = alphaBeta(depth - 1, alpha, beta).value;
							if (value != MATE - move - 2) {
								couldBeStalemate = false;
							}
							if (value < best.value) {
								best = { value, piece, current };
								piece->search_valueIfMoves++;
							}
							if (value <= alpha) {
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
		if (couldBeStalemate) {
			hash ^= getPlayerHash();
			blackToPlay = !blackToPlay;
			if(!isMate(alphaBeta(1,-INFINITY,INFINITY).value)){
				best = { 0.f, NULL, NULL };
			}
			hash ^= getPlayerHash();
			blackToPlay = !blackToPlay;
		}
		//storeTransposition( { depth, move, best, fen() });
		storeTransposition({ depth, move, best });
		return best;
	}

	void resetSearchData() {
		for (Piece* p : piecesBlack) {
			p->search_valueIfMoves = 0;
		}
		for (Piece* p : piecesWhite) {
			p->search_valueIfMoves = 0;
		}
		//transpositionTable.clear();
	}

	evaluation RunAi(int maxDepth) {
		double elapsedUs = 0;
		auto start = std::chrono::system_clock::now();
		evaluation currentEval;
		int i = 0;
		for (; i <= maxDepth && elapsedUs<1000000; i++) {
			resetSearchData();
			currentEval=alphaBeta(i, -INFINITY, INFINITY);
			//std::sort(piecesBlack.begin(), piecesBlack.end(), [](const Piece* a, const Piece* b) {return a->search_valueIfMoves > b->search_valueIfMoves; });
			//std::sort(piecesWhite.begin(), piecesWhite.end(), [](const Piece* a, const Piece* b) {return a->search_valueIfMoves > b->search_valueIfMoves; });
			elapsedUs = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - start).count();
		}
		std::cout << "searched to depth " << i << " in " << elapsedUs/1000000 << " s\n";
		return currentEval;
	}
};

Ability* makePieceType(std::string in) {
	// this does not really need to be efficient
	auto words = splitOffFirstWord(in, ' ');
	if (words[0] == "mirror") {
		if (words.size() != 2) {
			std::cout << "error parsing piece with properties '" << in << "'";
			return NULL;
		}
		auto props = splitOffFirstWord(words[1], ' ');
		if (props.size() != 2 || props[0].size() > 2) {
			std::cout << "error parsing piece with properties '" << in << "'";
			return NULL;
		}
		Ability* a = Ability::parseString(props[1]);
		if (a == NULL) {
			std::cout << "error parsing piece with properties '" << in << "'";
			return NULL;
		}
		std::vector<Ability*> parts;
		parts.push_back(a);
		if (sharesChars(props[0], "Xx")) {
			Ability* b = Ability::copy(a);
			b->invertX();
			parts.push_back(b);
		}
		if (sharesChars(props[0], "Yy")) {
			Ability* c = Ability::copy(a);
			c->invertY();
			parts.push_back(c);

			if (sharesChars(props[0], "Xx")) {
				Ability* d = Ability::copy(c);
				d->invertX();
				parts.push_back(d);
			}
		}

		Ability* result = Ability::combine(parts);
		for (Ability* part : parts) {
			delete[] part;
		}
		return result;
	}
	if (words[0] == "symmetric") {
		if (words.size() != 2) {
			std::cout << "error parsing piece with properties '" << in << "'";
			return NULL;
		}
		Ability* a = Ability::parseString(words[1]);
		if (a == NULL) {
			std::cout << "error parsing piece with properties '" << in << "'";
			return NULL;
		}
		std::vector<Ability*> parts;
		parts.push_back(a);
		for (int i = 0; i < 3; i++) {
			Ability* b = Ability::copy(a);
			a->rotate();
			parts.push_back(b);
		}
		Ability* result = Ability::combine(parts);
		for (Ability* part : parts) {
			delete[] part;
		}
		return result;
	}
	Ability* a = Ability::parseString(in);
	return a;
}

int main()
{
	zobristInit();

	Ability* bishop = makePieceType("symmetric 11>22>33>44>55>66>77");
	pieceTypes.push_back(new PieceType('b', bishop, false, 3));

	Ability* king = makePieceType("symmetric 11,10");
	pieceTypes.push_back(new PieceType('k', king, true, 100));

	Ability* horse = makePieceType("symmetric 12,21");
	pieceTypes.push_back(new PieceType('n', horse, false, 3));

	Ability* queen = makePieceType("symmetric 11>22>33>44>55>66>77,  01>02>03>04>05>06>07");
	pieceTypes.push_back(new PieceType('q', queen, false, 9));

	Ability* rook = makePieceType("symmetric 10>20>30>40>50>60>70");
	pieceTypes.push_back(new PieceType('r', rook, false, 5));

	Ability* pawn = makePieceType("01n=q>02nf,11c=q,-11c=q");
	pieceTypes.push_back(new PieceType('p', pawn, false, 1));

	//bishop->check();
	//king->check();
	//horse->check();
	//queen->check();
	std::cout << "\n\n";


	Board b;
	//b.loadFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR - - - - -"); // default
	//b.loadFen("rnbqkbnr/8/8/8/8/8/8/RNBQKBNR - - - - -");
	//b.loadFen("R1K5/8/8/8/8/5k2/8/8 - - - - -"); // rook mate
	b.loadFen("NBK5/8/8/8/8/5k2/8/8 - - - - -"); // bishop knight mate
	std::cout << b.fen()<<"\n";

	constexpr int depth = 100;
	while (1 == 1) {
		std::cout << "zero-depth eval: " << b.alphaBeta(0, -INFINITY, INFINITY).value << "\n";


		//Board::evaluation eval = b.alphaBeta(depth, white, -INFINITY, INFINITY);
		Board::evaluation eval = b.RunAi(depth);
		std::cout << "alphabeta eval: " << eval.value << " " << '\n';
		std::cout << b.hash << '\n';


		b.makeMove(eval.bestMoveFrom->position, eval.bestMove);
		b.print();
		string s; 
		std::getline(std::cin, s);
		while (!b.parseMove(s)) {
			std::getline(std::cin, s);
		};
	}
}