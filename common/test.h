#pragma once

#include <algorithm>
#include <ctime>
#include <fstream>
#include <iomanip> // std::put_time
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

// format: 2009.12.25_21.41.37
[[nodiscard]] std::string get_timestamp()
{
    std::time_t t = std::time(nullptr);
    std::tm* tm = std::localtime(&t);

    std::ostringstream os;
    os << std::put_time(tm, "%Y.%m.%d_%H.%M.%S");

    return os.str();
}

[[nodiscard]] std::pair<std::ofstream, std::string> open_csv(const std::string& filename)
{
    auto full_filename = filename + " (" + get_timestamp() + ") .csv";
    std::ofstream file(full_filename, std::ios::binary);

    if (!file) {
        std::cerr << "打开文件失败: " << full_filename << std::endl;
        throw std::runtime_error("打开文件失败");
    }

    file << "\xEF\xBB\xBF"; // UTF-8 BOM
    return {std::move(file), full_filename};
}

[[nodiscard]] std::string get_cmd_arg(const std::vector<std::string>& args,
    const std::string& option, const std::optional<std::string>& default_value = std::nullopt)
{
    auto it = std::find(args.begin(), args.end(), "-" + option);
    if (it != args.end() && ++it != args.end()) {
        return *it;
    }
    if (default_value.has_value()) {
        return *default_value;
    } else {
        std::cerr << "请提供参数: " << option << std::endl;
        exit(1);
        return "";
    }
}

[[nodiscard]] bool get_cmd_flag(const std::vector<std::string>& args, const std::string& option)
{
    return std::find(args.begin(), args.end(), "-" + option) != args.end();
}

[[nodiscard]] std::vector<int> assign_repeat(int total_repeat_num, int thread_num)
{
    if (total_repeat_num < thread_num) {
        thread_num = total_repeat_num;
    }

    int base = total_repeat_num / thread_num;
    int extra = total_repeat_num % thread_num;

    std::vector<int> repeat_per_thread(thread_num, base);

    for (int i = 0; i < extra; ++i) {
        repeat_per_thread[i]++;
    }
    return repeat_per_thread;
}

[[nodiscard]] std::vector<std::string> split(const std::string& s, char delim)
{
    std::vector<std::string> tokens;
    std::istringstream iss(s);
    std::string token;

    while (std::getline(iss, token, delim)) {
        tokens.push_back(token);
    }
    return tokens;
}