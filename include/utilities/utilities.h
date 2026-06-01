#ifndef UTILITIES_H
#define UTILITIES_H

#include <cstring>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <stdlib.h>
#include <queue>
#include <algorithm>
#include <cctype>


template<typename... Args> 
char* concatStringsModern(Args... args){
    std::string combined = (std::string(args) + ...);  // Fold expression (C++17)
    char* result = new char[combined.size() + 1];
    std::strcpy(result, combined.c_str());
    return result;
}

std::vector<std::string> split(const std::string &str, const std::string &delimiter);

std::string HexToStr(const long long value, int width);

std::string int_to_hex(const int value);

uint64_t string_to_uint64(std::string addr);

void printProgressBar(int progress, int total, int barWidth);

// std::string runPythonScript(const std::string& cmd) {
//     std::array<char, 128> buffer;
//     std::string result;
//     std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
//     if (!pipe) throw std::runtime_error("popen() failed!");
//     while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
//         result += buffer.data();
//     }
//     return result;
// }

int to_int(const std::string& s);

#endif