// Sockfish.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <stack>
#include<istream>
#include <sstream>
#include<vector>

using std::string;

enum Side {black, white};

struct Position {
    int x, y;

    Position operator+(Position& p) {
        return  { x + p.x, y + p.y };
    }

    Position operator-(Position& p) {
        return  { x - p.x, y - p.y };
    }
};

constexpr Position INVALID_POSITION = { -8888, 69420 };

class Ability {
public:
    constexpr static int CONDITION_CAPTURE_ONLY = 1;
    constexpr static int CONDITION_NO_CAPTURE = 1 << 1;
    constexpr static int ONLY_FIRST_MOVE = 1 << 2;
    constexpr static int DIRECTLY_PLAYABLE = 1 << 3;
    constexpr static int LAST_OPTION = 1 << 4;

    Ability* next, *prev;
    Position relativePos;
    int16_t conditions;

    Ability() {
        relativePos = { 0,0 };
        conditions = 0;
        next = NULL;
        prev = NULL;
    }
    ~Ability() {
        for (Ability* ptr = this; ptr++;) {
            if (ptr->next != NULL) { delete ptr->next; }
            if (ptr->conditions && LAST_OPTION) { break; }
        }
        delete[] this;
    }
    Ability(Ability* copy) {
        prev = NULL;
        relativePos = copy->relativePos;
        conditions = copy->conditions;
        if (copy->next == NULL) {
            next = NULL;
        }
        else {
            next = new Ability(copy->next);
            next->prev = this;
        }
    }
};

struct PieceType {
    char id;
    Ability* ability;
};

std::vector<PieceType> pieceTypes;

class Piece {
public:
    char type;
    Side side;
    Ability* ability;
    Position position;
    bool hasMoved;
    Piece(Position pos) {
        ability = NULL;
        position = pos;
        side = white;
        type = ' ';
        hasMoved = false;
    }

    Piece() {
        Piece({ 0,0 });
    }

    static Piece* create(char typeId) {
        PieceType* t = NULL;
        for (auto i = pieceTypes.begin(); i < pieceTypes.end(); i++) {
            if (i->id == typeId) {
                Piece* p=new Piece();
                p->ability = i->ability;
                p->type = typeId;
                return p;
            }
        }
        std::cout << "Error: no piece found with identifier '"<<typeId<<"'\n";
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
    };

    Piece *squares[64];
    std::stack<MoveRecord> moveHistory;

    static int toArrayIndex(Position pos) {
        return pos.x + 8 * pos.y;
    }
    static Position fromArrayIndex(int i) {
        return { i % 8, i / 8 };
    }

    Board() {
        for (int i = 0; i < 64; i++) {
            squares[i] = NULL;
        }
    }

    void makeMove(Position from, Position to) {
        moveHistory.push({ from, to, squares[toArrayIndex(to)] });
        squares[toArrayIndex(from)] -> move(to);
        squares[toArrayIndex(to)] = squares[toArrayIndex(from)];
        squares[toArrayIndex(from)] = NULL;
    }

    void unmakeMove() {
        MoveRecord move = moveHistory.top();
        moveHistory.pop();
        squares[toArrayIndex(move.to)]->move(move.from);
        squares[toArrayIndex(move.from)] = squares[toArrayIndex(move.to)];
        squares[toArrayIndex(move.to)] = move.captured;
    }

    void print() {
        std::cout << '\n';
        for (int i = 0; i < 64; i++) {
            if (squares[i] == NULL) std::cout << "# ";
            else std::cout << (char)(squares[i]->type + (squares[i]->side == Side::white ? 0 : 'A' - 'a')) << ' ';
            if (i%8==7) std::cout << '\n';
        }
    }

    void parseMove(string in) {
        std::istringstream stream(in);
        int x1, y1, x2, y2;
        stream >> x1 >> y1 >> x2 >> y2;
        makeMove({ x1,y1 }, { x2,y2 });
        print();
    }

    void addPiece(Position pos, char id) {
        Piece* p = Piece::create(id);
        if (p == NULL)return;
        p->move(pos);
        p->type = 'o';
        p->side = pos.y<4? Side::black: Side::white;
        squares[toArrayIndex(pos)] = p;
    }

    float eval(int depth, Side toPlay) {
        if (depth == 0) {
            return 0; // todo: some basic eval function
        }
        float bestEval = toPlay == white? -INFINITY : INFINITY;

        for (int i = 0; i < 64; i++) {
            Piece* piece = squares[i];
            if (piece != NULL) {
                evalMoves(bestEval, piece, piece->ability, depth);
            }
        }
        return bestEval;
    }

    void evalMoves(float& bestEval, Piece* piece, Ability* ability, int depth) {
        Side toPlay = piece->side;
        makeMove(piece->position, piece->position + ability->relativePos);
        // todo: special actions (castling, leaving en passant)
        if (ability->conditions && ability->DIRECTLY_PLAYABLE) {
            float value = eval(depth-1, toPlay==white?black:white);
            if ((value < bestEval && toPlay == black) || (value > bestEval && toPlay == white)) {
                bestEval = value;
            }
        }
        if (ability->next != NULL) {
            evalMoves(bestEval, piece, ability->next, depth);
        }
        unmakeMove();
        if (!ability->conditions && ability->LAST_OPTION) {
            evalMoves(bestEval, piece, ability+1, depth);
        }
    }
};

int main()
{
    Ability* a = new Ability();
    pieceTypes.push_back({ 'o', a });
    Board b;
    b.addPiece({ 0,0 },'o');
    b.addPiece({ 0,1 }, 'o');
    b.addPiece({ 1,0 }, 'o');
    b.addPiece({ 1,1 }, 'o');
    b.addPiece({ 2,0 }, 'o');
    b.addPiece({ 2,1 }, 'o');
    b.addPiece({ 3,0 }, 'o');
    b.addPiece({ 3,1 }, 'o');
    b.addPiece({ 4,0 }, 'o');
    b.addPiece({ 4,1 }, 'o');
    b.addPiece({ 5,0 }, 'o');
    b.addPiece({ 5,1 }, 'o');
    b.addPiece({ 6,0 }, 'o');
    b.addPiece({ 6,1 }, 'o');
    b.addPiece({ 7,0 }, 'o');
    b.addPiece({ 7,1 }, 'o');
    while (1 == 1) {
        string s;
        std::getline(std::cin, s);
        b.parseMove(s);
    }
}