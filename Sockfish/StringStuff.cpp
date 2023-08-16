#include <string>
#include<vector>
#include "StringStuff.h"
using std::string;

std::vector<string> split(string input, char delim) {
	std::vector<string> result;
	int begin = 0;
	for (int i = 0; i < input.size(); i++) {
		if (input[i] == delim) {
			result.push_back(input.substr(begin, i-begin));
			begin = i + 1;
		}
	}
	result.push_back(input.substr(begin));
	return result;
}

std::vector<string> splitOffFirstWord(string input, char delim) {
	std::vector<string> result;
	int begin = 0;
	bool done = false;
	for (int i = 0; i < input.size(); i++) {
		if (!done && input[i] == delim) {
			result.push_back(input.substr(begin, i - begin));
			begin = i + 1;
			done = true;
		}
	}
	result.push_back(input.substr(begin));
	return result;
}

// returns the number of chars between the opening bracket at param "index" and its closing bracket.
// -1 if there is no closing bracket, -2 if there is no opening bracket at the index.
int getBracketLength(string input, int index) {
	char opening = input[index];
	char closing;
	int layers = 1;
	if (opening == '(') { closing = ')'; }
	else if (opening == '[') { closing = ']'; }
	else if (opening == '{') { closing = '}'; }
	else return -2;
	for (int i = index + 1; i < input.size(); i++) {
		char current = input[i];
		if (current == opening) {
			layers++;
		}
		else if (current == closing) {
			layers--;
		}
		if (layers == 0) {
			return i - index -1;
		}
	}
	return -1;
}

std::vector<string> splitRespectBrackets(string input, char delim) {
	std::vector<string> result;
	int begin = 0;
	for (int i = 0; i < input.size(); i++) {
		int next = getBracketLength(input, i);
		if (next > 0) {
			i += next + 2;
		}
		if (input[i] == delim) {
			result.push_back(input.substr(begin, i - begin));
			begin = i + 1;
		}
	}
	result.push_back(input.substr(begin));
	return result;
}

string delChars(string input, char toDelete) {
	std::vector<string> parts = split(input, toDelete);
	string result = "";
	for (int i = 0; i < parts.size(); i++) {
		result.append(parts[i]);
	}
	return result;
}

bool sharesChars(string a, string b) {
	for (int x = 0; x < a.size(); x++) {
		for (int y = 0; y < b.size(); y++) {
			if (a[x] == b[y]) {
				return true;
			}
		}
	}
	return false;
}
