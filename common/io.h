#pragma once

#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

// format: 2009.12.25_21.41.37
[[nodiscard]] std::string get_timestamp()
{
    std::time_t t = std::time(nullptr);
    std::tm* tm = std::localtime(&t);

    std::stringstream ss;
    ss << std::put_time(tm, "%Y.%m.%d_%H.%M.%S");

    return ss.str();
}

[[nodiscard]] std::ofstream open_csv(const std::string& filename)
{
    ::system("chcp 65001 > nul");

    const auto& full_filename = filename + " (" + get_timestamp() + ") .csv";
    std::ofstream file(full_filename, std::ios::binary);

    if (!file) {
        std::cerr << "打开文件失败: " << full_filename << std::endl;
        throw std::runtime_error("打开文件失败");
    }

    file << "\xEF\xBB\xBF"; // UTF-8 BOM
    return file;
}