#pragma once

#include "types.h"

pvz_emulator::object::plant::explode_info operator+(
    const pvz_emulator::object::plant::explode_info& lhs,
    const pvz_emulator::object::plant::explode_info& rhs)
{
    return {lhs.from_upper + rhs.from_upper, lhs.from_same + rhs.from_same,
        lhs.from_lower + rhs.from_lower};
}

pvz_emulator::object::plant::explode_info& operator+=(
    pvz_emulator::object::plant::explode_info& lhs,
    const pvz_emulator::object::plant::explode_info& rhs)
{
    lhs.from_upper += rhs.from_upper;
    lhs.from_same += rhs.from_same;
    lhs.from_lower += rhs.from_lower;
    return lhs;
}

struct TestInfo {
    friend struct Table;

    int start_tick;
    std::vector<LossInfo> merged_loss_info;

private:
    void update(const Test& test)
    {
        if (merged_loss_info.empty()) {
            merged_loss_info.resize(test.loss_infos.size());
            start_tick = test.start_tick;
        }
        assert(merged_loss_info.size() == test.loss_infos.size());
        assert(start_tick == test.start_tick);

        for (size_t tick = 0; tick < test.loss_infos.size(); tick++) {
            const auto& loss_info = test.loss_infos.at(tick);

            for (const auto& plant : test.protect_plants) {
                merged_loss_info[tick].explode += loss_info[plant->row].explode;
                merged_loss_info[tick].hp_loss += loss_info[plant->row].hp_loss;
            }
        }
    }
};

struct Table {
    std::vector<TestInfo> test_infos;
    int repeat = 0;

    void update(const std::vector<Test>& tests)
    {
        if (test_infos.empty()) {
            test_infos.resize(tests.size());
        }
        assert(test_infos.size() == tests.size());

        for (size_t i = 0; i < tests.size(); i++) {
            test_infos[i].update(tests.at(i));
        }

        repeat++;
    }

    void merge(const Table& other)
    {
        if (test_infos.empty()) {
            test_infos = other.test_infos;
            repeat = other.repeat;
            return;
        }

        assert(test_infos.size() == other.test_infos.size());

        for (size_t i = 0; i < other.test_infos.size(); i++) {
            const auto& other_test_info = other.test_infos.at(i);
            auto& test_info = test_infos.at(i);

            assert(other_test_info.merged_loss_info.size() == test_info.merged_loss_info.size());
            assert(other_test_info.start_tick == test_info.start_tick);

            for (size_t tick = 0; tick < other_test_info.merged_loss_info.size(); tick++) {
                test_info.merged_loss_info[tick].explode
                    += other_test_info.merged_loss_info[tick].explode;
                test_info.merged_loss_info[tick].hp_loss
                    += other_test_info.merged_loss_info[tick].hp_loss;
            }
        }

        repeat += other.repeat;
    }
};