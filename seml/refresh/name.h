#pragma once

#include <sstream>

#include "types.h"

namespace _refresh_internal {

using namespace pvz_emulator::object;

const std::unordered_map<zombie_type, std::string> ZOMBIE_NAMES = {
    {zombie_type::zombie, "普"},
    {zombie_type::conehead, "障"},
    {zombie_type::pole_vaulting, "杆"},
    {zombie_type::buckethead, "桶"},
    {zombie_type::newspaper, "报"},
    {zombie_type::screendoor, "门"},
    {zombie_type::football, "橄"},
    {zombie_type::dancing, "舞"},
    {zombie_type::snorkel, "潜"},
    {zombie_type::zomboni, "车"},
    {zombie_type::dolphin_rider, "豚"},
    {zombie_type::jack_in_the_box, "丑"},
    {zombie_type::balloon, "气"},
    {zombie_type::digger, "矿"},
    {zombie_type::pogo, "跳"},
    {zombie_type::bungee, "偷"},
    {zombie_type::ladder, "梯"},
    {zombie_type::catapult, "篮"},
    {zombie_type::gargantuar, "白"},
    {zombie_type::giga_gargantuar, "红"},
};

}

std::string zombie_type_to_name(pvz_emulator::object::zombie_type type)
{
    return _refresh_internal::ZOMBIE_NAMES.at(type);
}

std::string zombie_types_to_names(
    const ZombieTypes& zombie_types, const std::string& fallback = "　")
{
    if (zombie_types.empty()) {
        return "无";
    } else {
        std::ostringstream os;
        for (int i = 0; i < 33; i++) {
            auto type = static_cast<pvz_emulator::object::zombie_type>(i);
            if (_refresh_internal::ZOMBIE_NAMES.count(type)) {
                if (zombie_types.count(type)) {
                    os << zombie_type_to_name(type);
                } else {
                    os << fallback;
                }
            }
        }
        return os.str();
    }
}
