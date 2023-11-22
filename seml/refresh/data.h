#pragma once

#include "types.h"

struct TestInfo {
    friend struct TestInfos;

    std::unordered_map<ZombieTypes, std::pair<float, int>, ZombieTypesHash> merged_accident_rates;

private:
    void update(const Test& test)
    {
        for (const auto& [zombie_types, accident_rates] : test.accident_rates) {
            auto& merged_accident_rate = merged_accident_rates[zombie_types];
            for (const auto& accident_rate : accident_rates) {
                merged_accident_rate.first += accident_rate;
                merged_accident_rate.second++;
            }
        }
    }

    void merge(const TestInfo& other)
    {
        for (const auto& [zombie_types, accident_rates] : other.merged_accident_rates) {
            auto& merged_accident_rate = merged_accident_rates[zombie_types];
            merged_accident_rate.first += accident_rates.first;
            merged_accident_rate.second += accident_rates.second;
        }
    }
};

struct Table {
    struct Col {
        double average_accident_rate;
        std::vector<std::pair<ZombieTypes, float>> rows;
    };

    std::vector<Col> cols;
    size_t max_row_count;
};

struct TestInfos {
    std::vector<TestInfo> test_infos;

    void update(const std::vector<Test>& tests)
    {
        if (test_infos.empty()) {
            test_infos.resize(tests.size());
        }

        for (size_t i = 0; i < tests.size(); i++) {
            test_infos[i].update(tests.at(i));
        }
    }

    void merge(const TestInfos& other)
    {
        if (test_infos.empty()) {
            test_infos = other.test_infos;
        } else {
            for (size_t i = 0; i < test_infos.size(); i++) {
                test_infos[i].merge(other.test_infos.at(i));
            }
        }
    }

    Table make_table() const
    {
        Table table = {};

        for (const auto& test_info : test_infos) {
            Table::Col col;
            double total_sum = 0;
            int total_count = 0;

            for (const auto& [zombie_types, accident_rate] : test_info.merged_accident_rates) {
                const auto& [sum, count] = accident_rate;
                col.rows.push_back({zombie_types, sum / static_cast<float>(count)});
                total_sum += sum;
                total_count += count;
            }

            std::sort(col.rows.begin(), col.rows.end(),
                [](const std::pair<ZombieTypes, float>& a, const std::pair<ZombieTypes, float>& b) {
                    return a.second > b.second;
                });

            col.average_accident_rate = total_sum / total_count;
            table.cols.push_back(col);
            table.max_row_count = std::max(table.max_row_count, col.rows.size());
        }

        return table;
    }
};