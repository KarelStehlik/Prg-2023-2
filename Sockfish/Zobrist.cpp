#include <cstdlib>
#include <ctime>
#include <iostream>
#include<random>
#include "Zobrist.h"

static constexpr int possiblePieces = 'z' - 'a' + 1;
static constexpr int positions = 64;
static constexpr int randomsNeeded = possiblePieces * positions * 2 * 2;

uint64_t numbers[randomsNeeded];
uint64_t whiteToPlay;

void zobristInit() {
	std::mt19937_64 rng(0);
	for (int i = 0; i < randomsNeeded; i++) {
		numbers[i] = rng();
	}
	whiteToPlay = rng();
}

int64_t getHash(char pieceId, int position, bool isBlack, bool hasMoved) {
	int piece = pieceId - 'a';
	int index = piece * positions * 2 * 2 + position * 2 * 2 + isBlack * 2 + hasMoved;
	return numbers[index];
}

int64_t getPlayerHash() {
	return whiteToPlay;
}