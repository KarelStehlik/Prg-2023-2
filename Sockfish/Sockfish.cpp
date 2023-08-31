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
#include "Docs.h"

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

// Abilities dictate how a piece moves. They are stored in a tree.
// Alternative abilities (eg. rook +(1,0), +(0,1), are stored in continuous memory, terminating in one with the LAST_OPTION flag.)
// abilities which may be played if the previous one didn't capture anything (eg. bishop +(1,1) -> +(2,2), are stored in next)
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
			return -1;
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
					std::cout << "Error: move description ending with '=' :" << input << "\n";
					return -1;
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

	int countOptions() {
		int count = 0;
		for (Ability* checking = this;; checking++) {
			count++;
			if (checking->hasProperty(LAST_OPTION)) {
				return count;
			}
		}
	}

	// invalidates this pointer
	int moveInMemory(Ability* newAdress) {
		Ability* addr = newAdress;
		int moved = 0;
		for (Ability* moving = this;; moving++) {
			*addr = *moving;
			if (addr->next != NULL) {
				addr->next->prev = addr;
			}
			addr++;
			moved++;
			if (moving->hasProperty(LAST_OPTION)) {
				//newAdress->fillPreviousness(NULL);
				delete[] this;
				return moved;
			}
		}
	}

	// invalidates pi=ointers in input vector
	static Ability* combine(const std::vector<Ability*>& in) {
		int totalSize = 0;
		for (Ability* a : in) {
			totalSize += a->countOptions();
		}
		Ability* result = new Ability[totalSize];
		int offset = 0;
		for (Ability* a : in) {
			offset += a->moveInMemory(result + offset);
			result[offset - 1].properties &= ~LAST_OPTION;
		}
		result[totalSize - 1].properties |= LAST_OPTION;
		result->fillPreviousness(NULL);
		return result;
	}

	// frees all memory in the subtree
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
		int options = og->countOptions();
		Ability* a = new Ability[options];
		for (int i = 0; i < options; i++) {
			a[i] = og[i];
			if (og[i].next != NULL) {
				a[i].next = copy(og[i].next);
			}
		}
		a->fillPreviousness(NULL);
		return a;
	}

	static Ability* parseString(std::string in);

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

	void init(char id, Ability* ability, bool isKing, float value) {
		this->id = id;
		this->ability = ability;
		this->invertedAbility = Ability::copy(ability);
		invertedAbility->invertY();
		this->isKing = isKing;
		estimatedValue = value;
	}

	PieceType(char id, Ability* ability, bool isKing, float value) {
		init(id, ability, isKing, value);
	}
};

std::vector<PieceType*> pieceTypes;

PieceType* getPieceType(char c) {
	c = tolower(c);
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
	PieceType* pieceType;

	Piece(PieceType* t, Position pos, bool black) {
		ability = NULL;
		position = pos;
		typeId = ' ';
		hasMoved = false;
		isKing = t->isKing;
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

class Game {
public:
	struct MoveRecord {
		Position from, to;
		Piece* captured;
		bool hasMovedBefore;
		int positionalEvalBefore;
		PieceType* promotedFrom;
		int halfMoves;
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
	int piecesOnBoard = 0;
	int halfMovesWithoutCapture = 0;
	bool whiteIsAi = false;
	bool blackIsAi = true;
	bool isDraw = false;
	int maxTableSize = 500000;

	static inline bool isMate(int eval) {
		return (eval > 1000 || eval < -1000) && eval!=INFINITY && eval!=-INFINITY;
	}

	struct evaluation {
		float value;
		Piece* bestMoveFrom;
		Ability* bestMove;
	};

	struct Transposition {
		int depth;
		int move;
		int pieces;
		int timesUsed;
		evaluation eval;
		float lowerBound;
		float upperBound;
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
		result += blackToPlay ? " b" : " w";
		result += " - - "; // no castling or en passant yet
		result += std::to_string(halfMovesWithoutCapture) + " ";
		result += std::to_string((int)move / 2); //full moves
		return result;
	}

	Game() {
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

		moveHistory.push({ from, to, target, moving->hasMoved , materialWhite - materialBlack, moving->pieceType, halfMovesWithoutCapture });
		halfMovesWithoutCapture++;

		if ((to.y == 7 || to.y == 0) && ability->promotionType!=NULL) {
			moving->updateType(ability->promotionType);
		}
		moving->move(to);
		moving->hasMoved = true;

		if (target!= NULL) {
			halfMovesWithoutCapture = 0;
			piecesOnBoard--;
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
		
		squares[tai] = moving;
		squares[fai] = NULL;

		hash ^= moving->hash();
	}

	void unmakeMove() {
		move--;
		blackToPlay = !blackToPlay;
		hash ^= getPlayerHash();
		MoveRecord lastMove = moveHistory.top();
		halfMovesWithoutCapture = lastMove.halfMoves;
		moveHistory.pop();
		int fai = lastMove.from.toArrayIndex(), tai = lastMove.to.toArrayIndex();
		Piece* moving = squares[tai];

		hash ^= moving->hash();

		moving->move(lastMove.from);
		squares[fai] = moving;
		moving->hasMoved = lastMove.hasMovedBefore;

		if (moving->pieceType != lastMove.promotedFrom) {
			moving->updateType(lastMove.promotedFrom);
		}
		squares[tai] = lastMove.captured;
		positionalEval = lastMove.positionalEvalBefore;
		if (lastMove.captured != NULL) {
			piecesOnBoard++;
			Piece* p = lastMove.captured;
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
		piecesOnBoard++;
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
		bool error = false;
		
		// validate the piece composition
		for (string row : rows) {
			int rowSize = row.length();
			for (char c : row) {
				if ('0' < c && c <= '9') {
					rowSize += c - '1';
				}
				else if (getPieceType(tolower(c))==NULL) {
					std::cout << "Error: no piece found with identifier " << c<<'\n';
					error = true;
				}
			}
			if (rowSize != 8) {
				std::cout << "Error: row should contain 8 pieces or spaces, contains "<<rowSize<<" instead: "<< row << '\n';
				error = true;
			}
		}

		// validate field to move
		if (fields[1] != "b" && fields[1] != "w") {
			std::cout << "Error: expected 'b' or 'w' in second field of FEN, got '"<<fields[1]<<"'\n";
			error = true;
		}


		//validate half move clock
		try {
			const int i{ std::stoi(fields[4])};
			if (i < 0 || i>99) {
				std::cout << "Error: half move clock is out of range 1-99, is '" << fields[4] << "'\n";
				error = true;
			}
		}
		catch (std::invalid_argument const& ex)
		{
			std::cout << "Error: invalid argument for half move clock: '" << fields[4] << "'\n";
			error = true;
		}
		catch (std::out_of_range const& ex)
		{
			std::cout << "Error: invalid argument for half move clock: '" << fields[4] << "'\n";
			error = true;
		}
		
		// validate total moves
		try {
			const int i{ std::stoi(fields[5]) };
			if (i < 0) {
				std::cout << "Error: moves played cannot be less than 0, is '" << fields[4] << "'\n";
				error = true;
			}
		}
		catch (std::invalid_argument const& ex)
		{
			std::cout << "Error: invalid argument for moves played: '" << fields[4] << "'\n";
			error = true;
		}
		catch (std::out_of_range const& ex)
		{
			std::cout << "Error: invalid argument for moves played: '" << fields[4] << "'\n";
			error = true;
		}

		if (error) {
			return;
		}

		// todo: validate fields castling, enpassant
		
		piecesWhite.clear();
		piecesBlack.clear();

		halfMovesWithoutCapture= std::stoi(fields[4]);
		move= std::stoi(fields[5]);
		blackToPlay = fields[1] == "b";

		for (int i = 0; i < 64; i++) {
			squares[i] = NULL;
		}

		for (Piece* p : piecesBlack) {
			delete p;
		}
		for (Piece* p : piecesWhite) {
			delete p;
		}

		piecesBlack.clear();
		piecesWhite.clear();
		piecesOnBoard = 0;
		
		while (!moveHistory.empty()) {
			moveHistory.pop();
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
				if (!moveHistory.empty() && (moveHistory.top().from == Position{j, i} || moveHistory.top().to == Position{j, i})) {
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
		if (p == NULL || blackToPlay != p->isBlack) {
			return NULL;
		}
		return searchPlayability(from, p->ability, to - from);
	}

	bool isPlayableIncludingPrevious(Ability* a, Position startingSquare) {
		if (!isPlayable(a, startingSquare)) {
			return false;
		}
		while (a->prev != NULL) {
			a = a->prev;
			if (!isPlayable(a, startingSquare)) {
				return false;
			}
		}
		return true;
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
				std::cout << "error loading fen";
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
			std::cout << "that's not a valid move\n";
			return false;
		}
		makeMove({ x1,y1 }, p);
		return true;
	}

	bool shouldDeleteTransposition(Transposition& t, int usedThreshold) {
		if (t.pieces < piecesOnBoard) {
			return true;
		}
		if (t.timesUsed < usedThreshold) {
			return true;
		}
		return false;
	}

	void cleanTranspositionTable() {
		int threshold = 1;
		while (transpositionTable.size() > maxTableSize) {
			auto itr = transpositionTable.begin();
			while (itr != transpositionTable.end()) {
				if (shouldDeleteTransposition(itr->second, threshold)) {
					itr = transpositionTable.erase(itr);
				}
				else {
					++itr;
				}
			}
			threshold *= 2;
		}
	}


	bool isCheck(bool testingWhiteInCheck) {
		std::vector<Piece*>& pieces = testingWhiteInCheck ? piecesBlack:piecesWhite;
		for (Piece* piece : pieces) {
			if (piece->alive) {
				Ability::MoveIterator iter = Ability::MoveIterator(piece->ability);
				Ability* current;
				while ((current = iter.get()) != NULL) {
					playability play = isPlayable(current, piece->position);
					if (play != unplayable && current->hasProperty(current->DIRECTLY_PLAYABLE)) {
						Position from = piece->position, to = piece->position + current->totalMovement;

						if (squares[to.toArrayIndex()] != NULL && squares[to.toArrayIndex()]->isKing) {
							return true;
						}
					}
					iter.next(play);
				}
			}
		}
		return false;
	}
	
	int maxMoveReached = 0;

	float delayMate(float value, int moveInserted) {
		if (!isMate(value)) {
			return value;
		}
		if (value < 0) {
			return value + move-moveInserted;
		}
		return value -move+moveInserted;
	}

	void storeTransposition(Transposition trans) {
		transpositionTable[hash] = trans;
	}

	Transposition* getTransposition() {
		auto found = transpositionTable.find(hash);
		if (found == transpositionTable.end()) {
			return NULL;
		}
		Transposition* t = &found->second;

		t->eval.value = delayMate(t->eval.value, t->move);
		t->lowerBound = delayMate(t->lowerBound, t->move);
		t->upperBound = delayMate(t->upperBound, t->move);
		t->move = move;

		return t;
	}

	int stored = 0, loaded = 0;

	// fen Q7/2k5/2p2p1R/8/7B/6PP/5PK1/8 w - - 10 39

	evaluation simpleEval() {
		float material = 0;//materialWhite - materialBlack;

		float movesWhite = 0;
		for (Piece* piece : piecesWhite) {
			if (piece->alive) {
				Ability::MoveIterator iter = Ability::MoveIterator(piece->ability);
				Ability* current;
				int thisPieceMoves = 0;
				while ((current = iter.get()) != NULL) {
					playability play = isPlayable(current, piece->position);
					iter.next(play);
					if (play != unplayable) {
						thisPieceMoves++;
					}
				}
				movesWhite += sqrt(thisPieceMoves);
			}
		}
		float movesBlack = 0;
		for (Piece* piece : piecesBlack) {
			if (piece->alive) {
				Ability::MoveIterator iter = Ability::MoveIterator(piece->ability);
				Ability* current;
				int thisPieceMoves = 0;
				while ((current = iter.get()) != NULL) {
					playability play = isPlayable(current, piece->position);
					iter.next(play);
					if (play != unplayable) {
						thisPieceMoves++;
					}
				}
				movesBlack += sqrt(thisPieceMoves);
			}
		}
		return { material + (movesWhite - movesBlack), NULL, NULL };
	}

	void evaluateMove(Piece* piece, Ability* current, int depth, float& alpha, float& beta, evaluation& best, bool& couldBeStalemate) {
		makeMove(piece->position, current);
		float value = alphaBeta(depth - 1, alpha, beta).value;
		unmakeMove();

		if (!blackToPlay) {
			couldBeStalemate &= value == -MATE +move+2;
			if (value > best.value) {
				best = { value, piece, current };
			}
			alpha = max(alpha, value);
		}
		else {
			couldBeStalemate &= value == MATE -move-2;
			if (value < best.value) {
				best = { value, piece, current };
			}
			beta = min(beta, value);
		}
	}

	evaluation alphaBeta(int depth, float alpha, float beta) {
		if (blackWon || whiteWon) {
			return { (blackWon ? -MATE+move : MATE-move) , NULL, NULL };
		}

		evaluation best = { blackToPlay ? INFINITY : -INFINITY , NULL,NULL };

		Transposition* done = getTransposition();

		if (done != NULL) {
			if (done->depth >= depth) {
				done->timesUsed++;
				loaded++;

				alpha = max(alpha, done->lowerBound);
				beta = min(beta, done->upperBound);

				if (alpha >= beta) {
					return done->eval;
				}
			}
		}

		if (depth == 0) {
			return simpleEval();
		}

		float ogAlpha = alpha, ogBeta = beta;

		bool couldBeStalemate = true;

		if (done!=NULL && done->eval.bestMove != NULL && done->depth<depth) {
			Ability* move = done->eval.bestMove;
			Piece* piece = done->eval.bestMoveFrom;
			if (piece->alive && piece->isBlack == blackToPlay && isPlayableIncludingPrevious(move, piece->position)) {
				evaluateMove(piece, move, depth, alpha, beta, best, couldBeStalemate);
			}
		}
		std::vector<Piece*>& pieces = blackToPlay ? piecesBlack : piecesWhite;

		for (Piece* piece : pieces) {
			if (!piece->alive) {
				continue;
			}
			Ability::MoveIterator iter = Ability::MoveIterator(piece->ability);
			Ability* current;
			while ((current = iter.get()) != NULL) {
				playability play = isPlayable(current, piece->position);
				iter.next(play);
				if (alpha >= beta) {
					couldBeStalemate = false;
					break;
				}
				if (play != unplayable && current->hasProperty(current->DIRECTLY_PLAYABLE)) {
					evaluateMove(piece, current, depth, alpha, beta, best, couldBeStalemate);
				}
			}

		}
		if (couldBeStalemate) {
			if (!isCheck(!blackToPlay)) {
				best = { 0.f, NULL, NULL };
				storeTransposition({ 1000, move, piecesOnBoard, 0, best, best.value, best.value });
				stored++;
				return best;
			}
		}

		if (done == NULL || done->depth<depth) {
			storeTransposition({ depth, move, piecesOnBoard, 0, best, -INFINITY, INFINITY });
			done = getTransposition();
		}
		if (best.value <= ogAlpha) {
			done->upperBound = best.value;
		}
		if(best.value > ogAlpha && best.value < ogBeta) {
			done->lowerBound = best.value;
			done->upperBound = best.value;
		}
		if(best.value >= ogBeta) {
			done->lowerBound = best.value;
		}
		done->depth = depth;

		stored++;
		return best;
	}

	void resetSearchData() {
		cleanTranspositionTable();
	}

	evaluation RunAi(int maxDepth) {
		stored = 0;
		loaded = 0;
		double elapsedUs = 0;
		auto start = std::chrono::system_clock::now();
		evaluation currentEval;
		int i = 0;
		for (; i <= maxDepth && elapsedUs<300000; i++) {
			resetSearchData();
			currentEval=alphaBeta(i, -INFINITY, INFINITY);
			elapsedUs = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - start).count();
		}
		std::cout << "searched to depth " << i << " in " << elapsedUs/1000000 << " s\n";
		std::cout << stored << " transpositions saved, " << transpositionTable.size() << " kept, " << loaded << " loaded\n";
		std::cout << isCheck(!blackToPlay) << "\n";
		return currentEval;
	}
};

Ability* Ability::parseString(string input) {
	if (!verifyBracketing(input)) {
		std::cout << "input \"" << input << "\" is not correctly bracketed";
		return NULL;
	}
	string in = delChars(input, ' ');
	if (in[0] == '(') {
		in = in.substr(1, in.size() - 2);
	}

	std::vector<string> parts = splitRespectBrackets(in, ',');
	const int options = parts.size();
	std::vector<Ability*> partAbilities;
	bool failed = false;
	for (int i = 0; i < options; i++) {
		Ability* a = new Ability[1];
		a->properties |= LAST_OPTION;
		int read = a->readProperties(parts[i]);
		if (read == -1) {
			failed = true;
		}
		if (parts[i].size() > read + 2) {
			a->next = parseString(parts[i].substr(read + 1));
		}
		partAbilities.push_back(a);
	}

	if (failed) {
		for (Ability* a : partAbilities) {
			a->del();
		}
		return NULL;
	}

	return combine(partAbilities);
}


Ability* makePieceType(std::string in) {
	// this does not really need to be efficient
	auto words = splitOffFirstWord(in, ' ');
	if (words[0] == "mirror") {
		if (words.size() != 2) {
			std::cout << "expected 'X', 'Y' or 'XY' after 'mirror', got nothing\n";
			return NULL;
		}
		auto props = splitOffFirstWord(words[1], ' ');
		if (props.size() != 2 || props[0].size() > 2) {
			std::cout << "expected 'X', 'Y' or 'XY' after 'mirror', got '" << props[0]<<"'\n";
			return NULL;
		}
		Ability* a = Ability::parseString(props[1]);
		if (a == NULL) {
			std::cout << "error parsing moves description '" << props[1] << "'\n";
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
			std::cout << "expected moves description after 'symmetric', got nothing\n";
			return NULL;
		}
		Ability* a = Ability::parseString(words[1]);
		if (a == NULL) {
			std::cout << "error parsing moves description '" << words[1] << "'\n";
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
		return result;
	}
	if (words[0] == "combine") {
		if (words.size() != 2) {
			std::cout << "expected list of piece identifiers after 'combine', got nothing\n";
			return NULL;
		}
		std::vector<std::string> pieces = split(delChars(words[1], ' '), ',');
		std::vector<Ability*> abilities;
		for (std::string s : pieces) {
			if (s.length() != 1) {
				std::cout << "'" << s << "'is not a valid piece identifier\n";
				return NULL;
			}
			PieceType* pieceType = getPieceType(s[0]);
			if (pieceType == NULL) {
				std::cout << "no piece with identifier '" << s << "'\n";
				return NULL;
			}
			abilities.push_back(pieceType->ability);
		}
		for (int i = 0; i < abilities.size(); i++) {
			abilities[i] = Ability::copy(abilities[i]);
		}
		return Ability::combine(abilities);
	}
	Ability* a = Ability::parseString(in);
	if (a == NULL) {
		std::cout << "error parsing moves description '" << in << "'\n";
	}
	return a;
}

void userInput(std::string in, Game& b) {
	std::vector<std::string> words = splitOffFirstWord(in, ' ');
	std::string action = words[0];

	if (words.size() == 1) {
		if (action == "fen") {
			std::cout << b.fen() << "\n";
			return;
		}
		if (action == "reset") {
			b.loadFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w - - 0 0");
			return;
		}
		if (action == "toggleBlackBot") {
			b.blackIsAi = !b.blackIsAi;
			return;
		}
		if (action == "toggleWhiteBot") {
			b.whiteIsAi = !b.whiteIsAi;
			return;
		}
		if (action == "help") {
			printDocs("");
			return;
		}
		b.parseMove(action);
		return;
	}
	std::string data = words[1];

	if (action == "piece") {
		std::vector<std::string> idAndAbility = splitOffFirstWord(data, ' ');
		if (idAndAbility.size() != 2) {
			std::cout << "Error: expecting a piece type and description, example 'piece n symmetric 12,21'\n";
			return;
		}
		if (idAndAbility[0].size() != 1) {
			std::cout << "Error: '" << idAndAbility[0] << "' is not a valid piece identifier\n";
			return;
		}
		const char id = tolower(idAndAbility[0][0]);
		Ability* ability = makePieceType(idAndAbility[1]);
		if (id > 'z' || id < 'a') {
			std::cout << "Error: '"<<id<<"' is not a valid piece identifier\n";
			return;
		}
		if (ability == NULL) {
			return;
		}
		PieceType* t = getPieceType(id);
		if (t != NULL) {
			std::cout << "'" << id << "' is an existing piece type. Type 'yes' to overwrite it, anything else to cancel.\n";
			std::string prompt;
			std::getline(std::cin, prompt);
			if (prompt != "yes") {
				std::cout << "action cancelled.\n";
				ability->del();
				return;
			}
			t->init(id, ability, false, 3);
		}
		pieceTypes.push_back(new PieceType(id, ability, false, 3));
		return;
	}
	if (action == "fen") {
		b.loadFen(data);
		return;
	}
	if (action == "help") {
		printDocs(data);
		return;
	}
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

	std::cout << "\n\n";


	Game b;
	//b.loadFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w - - 0 0"); // default
	//tests:
	//b.loadFen("rnbqkbnr/8/8/8/8/8/8/RNBQKBNR - - - - -");
	b.loadFen("R1K5/8/8/8/8/5k2/8/8 w - - 0 0"); // rook mate
	//b.loadFen("1K6/8/8/7k/8/8/8/6R1 b - - 0 0"); // y u repeat
	//b.loadFen("NBK5/8/8/8/8/5k2/8/8 - - - - -"); // bishop knight mate
	std::cout << b.fen()<<"\n";

	constexpr int depth = 100;
	while (true) {
		b.print();
		if (b.blackWon) {
			std::cout << "Black wins!\n";
			while (b.blackWon) {
				string s;
				std::getline(std::cin, s);
				userInput(s, b);
			}
		}
		else if (b.whiteWon) {
			std::cout << "White wins!\n";
			while (b.whiteWon) {
				string s;
				std::getline(std::cin, s);
				userInput(s, b);
			}
		}
		if ((b.blackToPlay && b.blackIsAi) || (!b.blackToPlay && b.whiteIsAi)) {
			std::cout << "bot is thinking...\n";
			Game::evaluation eval = b.RunAi(depth);
			std::cout << "eval: " << eval.value << " " << '\n';
			b.makeMove(eval.bestMoveFrom->position, eval.bestMove);
		}
		else {
			string s;
			std::getline(std::cin, s);
			userInput(s, b);
		}
	}
}