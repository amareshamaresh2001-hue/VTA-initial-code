#include "utilities.h"

#include <algorithm>


std::vector<std::string> split(const std::string &str, const std::string &delimiter) {
    std::vector<std::string> tokens;
    size_t start = 0;
    size_t end = str.find(delimiter);

    while (end != std::string::npos) {
        tokens.push_back(str.substr(start, end - start));
        start = end + delimiter.length();
        end = str.find(delimiter, start);
    }
    tokens.push_back(str.substr(start)); // last part
    return tokens;
}

std::string HexToStr(const long long value, int width){
	std::string hexStr;
	std::stringstream num;
	num.width(width);
	num.fill('0');
	num << std::fixed << std::hex << value;
	hexStr = num.str();
	return hexStr;
}

std::string int_to_hex(const int value){
	std::string hexStr;
	std::stringstream num;
	// num.width(sizeof(value)*2);
    num << "0x";
	num.fill('0');
	num << std::fixed << std::hex << value;
	hexStr = num.str();
	return hexStr;
}

uint64_t string_to_uint64(std::string addr){
    std::istringstream converter(addr);
    unsigned int value;
    converter >> std::hex >> value;
    return value;
}

void printProgressBar(int progress, int total, int barWidth) {
    if (total <= 0) {
        std::cout << "[";
        for (int i = 0; i < barWidth; ++i) {
            std::cout << "=";
        }
        std::cout << "] 100 %\n";
        return;
    }

    const int clamped_progress = std::min(progress, total);
    const float ratio = static_cast<float>(clamped_progress) / static_cast<float>(total);
    const int pos = static_cast<int>(barWidth * ratio);

    std::cout << "[";
    for (int i = 0; i < barWidth; ++i) {
        if (i < pos) std::cout << "=";
        else if (i == pos) std::cout << ">";
        else std::cout << " ";
    }
    std::cout << "] " << int(ratio * 100.0) << " %\r";
    if (clamped_progress < total) {
        std::cout.flush();
    } else {
        std::cout << std::endl;
    }
}

int to_int(const std::string& s) {
    try {
        return s != "" && s != " " ? std::stoi(s) : 0;
    } catch (...) {
        return 0;
    }
}