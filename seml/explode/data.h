#pragma once

#include "types.h"

pvz_emulator::object::plant::explode_info operator+(
    const pvz_emulator::object::plant::explode_info& lhs,
    const pvz_emulator::object::plant::explode_info& rhs)
{
    return {lhs.from_upper + rhs.from_upper, lhs.from_same + rhs.from_same,
        lhs.from_lower + rhs.from_lower};
}

int sum(const pvz_emulator::object::plant::explode_info& explode)
{
    return explode.from_upper + explode.from_same + explode.from_lower;
}

struct Table {
    std::vector<std::vector<LossInfo>> data;
    int repeat = 0;
};

void update_table(Table& table, const std::vector<Test>& tests)
{
    if (table.data.empty()) {
        table.data.resize(tests.size());
    }
    assert(table.data.size() == tests.size());

    for (int test_num = 0; test_num < tests.size(); test_num++) {
        const auto& test = tests.at(test_num);
        if (table.data[test_num].empty()) {
            table.data[test_num].resize(test.loss_infos.size());
        }
        assert(table.data[test_num].size() == test.loss_infos.size());

        for (int tick = 0; tick < test.loss_infos.size(); tick++) {
            const auto& loss_info = test.loss_infos.at(tick);

            pvz_emulator::object::plant::explode_info explode = {0, 0, 0};
            int hp_loss = 0;
            for (const auto& plant : test.plants) {
                explode = explode + loss_info[plant->row].explode;
                hp_loss += loss_info[plant->row].hp_loss;
            }

            table.data[test_num][tick].explode = table.data[test_num][tick].explode + explode;
            table.data[test_num][tick].hp_loss += hp_loss;
        }
    }

    table.repeat++;
}

void merge_table(const Table& src, Table& dst)
{
    if (dst.data.empty()) {
        dst.data.resize(src.data.size());
    }
    assert(src.data.size() == dst.data.size());

    for (int test_num = 0; test_num < src.data.size(); test_num++) {
        if (dst.data[test_num].empty()) {
            dst.data[test_num].resize(src.data[test_num].size());
        }
        assert(src.data[test_num].size() == dst.data[test_num].size());

        for (int tick = 0; tick < src.data[test_num].size(); tick++) {
            dst.data[test_num][tick].explode
                = dst.data[test_num][tick].explode + src.data[test_num][tick].explode;
            dst.data[test_num][tick].hp_loss += src.data[test_num][tick].hp_loss;
        }
    }

    dst.repeat += src.repeat;
}