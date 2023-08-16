#pragma once
#include <string>
#include<vector>
std::vector<std::string> split(std::string input, char delim);
int getBracketLength(std::string input, int index);
std::vector<std::string> splitRespectBrackets(std::string input, char delim);
std::string delChars(std::string input, char toDelete);
bool sharesChars(std::string a, std::string b);
std::vector<std::string> splitOffFirstWord(std::string input, char delim);