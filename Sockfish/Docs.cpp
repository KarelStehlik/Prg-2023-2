#include <iostream>
#include <fstream>
#include <string>
#include "Docs.h"

void printDocs(std::string category) {
	std::ifstream myfile;
    std::string line;
	myfile.open("help.txt");
    if (myfile.is_open())
    {
        if (category == "") {
            while (myfile.good())
            {
                std::getline(myfile, line);
                std::cout << line << '\n';
            }
        }
        else {
            bool printing = false;
            std::string lookingFor = "[" + category + "]";

            while (myfile.good())
            {
                std::getline(myfile, line);
                if (line == lookingFor) {
                    printing = true;
                }
                if (printing) {
                    if (line != "" && line[0] == '[') {
                        return;
                    }
                    std::cout << line << '\n';
                }
            }
        }
        myfile.close();
    }
    else {
        std::cout << "Unable to open docs file";
        return;
    }

    std::cout << "Docs for " << category << " not found.\n";
}