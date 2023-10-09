#pragma once

#include "operation.h"

[[nodiscard]] std::string dump(const std::vector<Op>& ops)
{
    std::stringstream ss;
    for (const auto& op : ops) {
        ss << op.tick << " ";
    }
    ss << "\n";
    return ss.str();
}

[[nodiscard]] std::string dump(const std::vector<_SmashInternal::OpInfo>& op_infos)
{
    std::stringstream ss;
    for (const auto& op_info : op_infos) {
        ss << "w" << op_info.wave << " t" << op_info.tick << " " << op_info.desc;
        ss << " uuid:[";
        for (const auto& plant : op_info.plants) {
            ss << plant.uuid << ",";
        }
        ss << "]\n";
    }
    return ss.str();
}

[[nodiscard]] std::string dump(const std::vector<_SmashInternal::GargInfo>& garg_infos)
{
    std::stringstream ss;
    for (const auto& garg_info : garg_infos) {
        ss << "t" << garg_info.spawn_tick << "+" << garg_info.alive_time << " "
           << garg_info.zombie.uuid << " cob:[";
        for (const auto& h : garg_info.hit_by_cob) {
            ss << h << ",";
        }
        ss << "] attempted_smashes:[";
        for (const auto& s : garg_info.attempted_smashes) {
            ss << s << ",";
        }
        ss << "] ignored_smashes:[";
        for (const auto& s : garg_info.ignored_smashes) {
            ss << s << ",";
        }
        ss << "]\n";
    }
    return ss.str();
}

void dump_all(const std::vector<Op>& ops, const Info& info)
{
    std::cout << "---------ops----------\n";
    std::cout << dump(ops);
    std::cout << "---------op_infos----------\n";
    std::cout << dump(info.op_infos);
    std::cout << "---------garg_infos----------\n";
    std::cout << dump(info.garg_infos);
    std::cout << "-------------------\n";
}
