/**
 * @file   meshAssemblyLocalRules.hpp
 * @author pthalamy <pthalamy@p3520-pthalamy-linux>
 * @date   Thu Oct 18 22:49:43 2018
 * 
 * @brief  
 * 
 * 
 */


#ifndef MESHASSEMBLYLOCALRULES_HPP__
#define MESHASSEMBLYLOCALRULES_HPP__

#include "cell3DPosition.h"

#include <bitset>
#include <utility>

using namespace BaseSimulator;
using namespace std;
    
template<size_t sz> struct localrule_comparer {
    bool operator() (const std::pair<const std::bitset<sz>, const Cell3DPosition> &p1,
                     const std::pair<const std::bitset<sz>, const Cell3DPosition> &p2) const {
        if (p1.first.to_ulong() < p2.first.to_ulong()) return true;
        else if (p1.first.to_ulong() > p2.first.to_ulong()) return false;
        else return p1.second < p2.second;
    }
};

static const std::map <const std::pair<const std::bitset<12>, const Cell3DPosition>,
                       const Cell3DPosition, localrule_comparer<12>>
localMotionRules = 
{
    { { std::bitset<12>(0x840), Cell3DPosition(2, 1, -1) }, Cell3DPosition(1, 1, 0) },
    { { std::bitset<12>(0x100), Cell3DPosition(1, 1, 0) }, Cell3DPosition(0, 0, 0) }
};


/** 
 * Search for the next action among the local rules library 
 * @param localNeighborhood a bitset representing the local neighborhood 
 *  of module at position pos
 * @param pos position of the module awaiting action
 * @return the matched next position if there is one, pos otherwise (meaning no movement)
 */
inline static const Cell3DPosition matchLocalRules(const std::bitset<12>& localNeighborhood,
                                                    const Cell3DPosition& pos,
                                                    const Cell3DPosition& tileRootPos) {
    auto match = localMotionRules.find(make_pair(localNeighborhood, pos));

    return match != localMotionRules.end() ? match->second + tileRootPos : pos + tileRootPos;
}

#endif /* MESHASSEMBLYLOCALRULES_HPP__ */
    
