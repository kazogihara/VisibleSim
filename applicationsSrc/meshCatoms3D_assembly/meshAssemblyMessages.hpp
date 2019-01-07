/**
 * @file   messages.hpp
 * @author pthalamy <pthalamy@p3520-pthalamy-linux>
 * @date   Tue Jul 10 13:47:20 2018
 * 
 * @brief  
 * 
 * 
 */


#ifndef MC3D_MESSAGES_H_
#define MC3D_MESSAGES_H_

#include "network.h"

#include "meshRuleMatcher.hpp"

static const uint MSG_DELAY = 0;

using namespace MeshCoating;

class RequestTargetCellMessage : public HandleableMessage {
    const Cell3DPosition srcPos;
public:
    RequestTargetCellMessage(const Cell3DPosition& _srcPos)
        : HandleableMessage(), srcPos(_srcPos) {};
    virtual ~RequestTargetCellMessage() {};

    virtual void handle(BaseSimulator::BlockCode*);
    virtual Message* clone() const { return new RequestTargetCellMessage(*this); }
    virtual string getName() const { return "RequestTargetCell"; }
};

class ProvideTargetCellMessage : public HandleableMessage {
    const Cell3DPosition tPos;
    const Cell3DPosition dstPos;
public:
    ProvideTargetCellMessage(const Cell3DPosition& _tPos, const Cell3DPosition& _dstPos)
        : HandleableMessage(), tPos(_tPos), dstPos(_dstPos) {};
    virtual ~ProvideTargetCellMessage() {};

    virtual void handle(BaseSimulator::BlockCode*);
    virtual Message* clone() const { return new ProvideTargetCellMessage(*this); }
    virtual string getName() const { return "ProvideTargetCell"; }
};

class TileNotReadyMessage : public HandleableMessage {
    const Cell3DPosition dstPos;
public:
    TileNotReadyMessage(const Cell3DPosition& _dstPos)
        : HandleableMessage(), dstPos(_dstPos) {};
    virtual ~TileNotReadyMessage() {};

    virtual void handle(BaseSimulator::BlockCode*);
    virtual Message* clone() const { return new TileNotReadyMessage(*this); }
    virtual string getName() const { return "TileNotReady"; }
};

class TileInsertionReadyMessage : public HandleableMessage {    
public:
    TileInsertionReadyMessage() : HandleableMessage() {};
    virtual ~TileInsertionReadyMessage() {};

    virtual void handle(BaseSimulator::BlockCode*);
    virtual Message* clone() const { return new TileInsertionReadyMessage(*this); }
    virtual string getName() const { return "TileInsertionReady"; }
};

/////////////////////////////////////////////////////////////////
///////////////////// MOTION COORDINATION ///////////////////////
/////////////////////////////////////////////////////////////////

/**
 * This message should be routed through the line until it reaches the dstPos, which as a pivot
 *  module must check whether its light is on and either give a go, or wait until its status 
 *  clears and send a go at that time.
 */
class ProbePivotLightStateMessage : public HandleableMessage {
    const Cell3DPosition srcPos;
    const Cell3DPosition targetPos;
public:
    ProbePivotLightStateMessage(const Cell3DPosition& _srcPos,
                                const Cell3DPosition& _targetPos)
        : HandleableMessage(), srcPos(_srcPos), targetPos(_targetPos) {};
    virtual ~ProbePivotLightStateMessage() {};

    virtual void handle(BaseSimulator::BlockCode*);
    virtual Message* clone() const { return new ProbePivotLightStateMessage(*this); }
    virtual string getName() const { return "ProbePivotLightState{" + srcPos.to_string()
            + ", " + targetPos.to_string() + "}";
    }
};


/**
 * This message is routed back from the pivot module to the module awaiting motion
 *  in order to notify it that it can be safely performed.
 */
class GreenLightIsOnMessage : public HandleableMessage {
    const Cell3DPosition srcPos;
    const Cell3DPosition dstPos;
public:
    GreenLightIsOnMessage(const Cell3DPosition& _srcPos,
                          const Cell3DPosition& _dstPos)
        : HandleableMessage(), srcPos(_srcPos), dstPos(_dstPos) {};
    virtual ~GreenLightIsOnMessage() {};

    virtual void handle(BaseSimulator::BlockCode*);
    virtual Message* clone() const { return new GreenLightIsOnMessage(*this); }
    virtual string getName() const { return "GreenLightIsOn{" + srcPos.to_string()
            + ", " + dstPos.to_string() + "}";
    }
};

#endif /* MC3D_MESSAGES_H_ */
