
/**
 * @file bordersCode.cpp
 * Generated by VisibleSim BlockCode Generator
 * https://services-stgi.pu-pm.univ-fcomte.fr/visiblesim/generator.php#
 * @author yourName
 * @date 2022-10-13                                                                     
 **/

#include "bordersCode.hpp"
#include <array>

static const SCLattice::Direction N = SCLattice::North;
static const SCLattice::Direction E = SCLattice::East;
static const SCLattice::Direction S = SCLattice::South;
static const SCLattice::Direction W = SCLattice::West;
static array<SCLattice::Direction, 4> neighborDir = {S, E, N, W};
static const array<uint8_t[3],36> tabBorders = { {
        {11,W,N}, {15,W,N}, {22,S,W}, {23,S,W}, {31,S,N},
        {43,W,N}, {47,W,N}, {63,S,N}, {104,N,E}, {105,N,E},
        {107,W,E},{111,W,E},{127,S,E},{150,S,W},{151,S,W},{159,S,N}, {191,S,N},
        {208,E,S},{212,E,S},{214,E,W},{215,E,W}, {223,E,N}, {232,N,E},
        {233,N,E},{235,W,E}, {239,W,E},{240,E,S},{244,E,S},{246,E,W},{247,E,W},
        {248,N,S},{249,N,S}, {251,W,S},{252,N,S},{253,N,S},
        {254,N,W}}};


BordersCode::BordersCode(BlinkyBlocksBlock *host) : BlinkyBlocksBlockCode(host), module(host) {
    // @warning Do not remove block below, as a blockcode with a NULL host might be created
    //  for command line parsing
    if (not host) return;

    // Registers a callback (myPositionFunc) to the message of type O
    addMessageEventFunc2(POSITION_MSG_ID,
                         std::bind(&BordersCode::myPositionFunc, this,
                                   std::placeholders::_1, std::placeholders::_2));

    // Registers a callback (myLeaderFunc) to the message of type A
    addMessageEventFunc2(LEADER_MSG_ID,
                         std::bind(&BordersCode::myLeaderFunc,this,
                                   std::placeholders::_1, std::placeholders::_2));
    // Registers a callback (myDistanceFunc) to the message of type A
    addMessageEventFunc2(DISTANCE_MSG_ID,
                         std::bind(&BordersCode::myDistanceFunc,this,
                                   std::placeholders::_1, std::placeholders::_2));
    order=numeric_limits<uint16_t>::max();
    isLeader=false;
}

void BordersCode::startup() {
    console << "start " << getId() << "\n";

    for (int i = 0; i < SCLattice::Direction::MAX_NB_NEIGHBORS; i++) {
        neighborhood.setNeighbor(SCLattice::Direction(i), module->getInterface(i)->isConnected());
//        console << "add(" << i << "," << int(module->getInterface(i)->isConnected()) << ")" << int(neighborhood.getState()) <<"\n";
    }
    neighborhood.complete();
    console << "Nh_1=" << int(neighborhood.getState()) << "," << int(neighborhood.getValidity()) << "\n";
    for (auto dir: neighborDir) {
        auto p2p = module->getInterface(dir);
        if (p2p->isConnected()) {
            uint8_t code = neighborhood.getNeighborCode(dir);
            sendMessage(new MessageOf<uint8_t>(POSITION_MSG_ID, code), p2p, 1000 + 100 * dir, 0);
        }
    }
    setColor(RED);


}

void BordersCode::myPositionFunc(std::shared_ptr<Message> _msg, P2PNetworkInterface *sender) {
    MessageOf<uint8_t> *msg = static_cast<MessageOf<uint8_t> *>(_msg.get());
    uint8_t msgData = *msg->getData();

    neighborhood.merge(SCLattice::Direction(module->getInterfaceBId(sender)), msgData);
    console << "Nh_2=" << int(neighborhood.getState()) << "," << int(neighborhood.getValidity()) << "\n";
    setColor(neighborhood.isDefined() ? WHITE : ORANGE);

    if (neighborhood.isDefined()) {
        // search code in list
        auto it = tabBorders.begin();
        auto v = neighborhood.getState();
        while (it != tabBorders.end() && (*it)[0] != v) {
            it++;
        }
        if (it != tabBorders.end()) {
            neighborhood.setFromTo(SCLattice::Direction((*it)[1]), SCLattice::Direction((*it)[2]));
            setColor(GREEN);
            order=0;
            sendMessage(new MessageOf<pair<uint16_t,int8_t>>(LEADER_MSG_ID, make_pair(getId(),neighborhood.getTurning())), module->getInterface((*it)[2]), 1000, 0);
        }
    }
}

void BordersCode::myLeaderFunc(std::shared_ptr<Message>_msg, P2PNetworkInterface*sender) {
    MessageOf<pair<uint16_t,int8_t>>* msg = static_cast<MessageOf<pair<uint16_t,int8_t>>*>(_msg.get());
    pair<uint16_t,int8_t> msgData = *msg->getData();

    console << "Rec.L " << int(msgData.first) << "," << int(msgData.second) << "\n";
    auto myId = getId();
    if (msgData.first==myId) {
        isLeader=true;
        order=0;
        neighborhood.setExternalBorder(msgData.second>0);
        setColor(msgData.second>0);
        sendMessage(new MessageOf<pair<uint16_t,uint8_t>>(DISTANCE_MSG_ID, make_pair(1,neighborhood.isExternalBorder())),
                    module->getInterface(neighborhood.getTo()), 1000, 0);
    } else {
        if (msgData.first < myId) {
            setColor(BLUE);
            sendMessage(new MessageOf<pair<uint16_t,int8_t>>(LEADER_MSG_ID, make_pair(msgData.first,neighborhood.getTurning()+msgData.second)),
                        module->getInterface(neighborhood.getTo()), 1000, 0);
        } else setColor(CYAN);
    }
}

void BordersCode::myDistanceFunc(std::shared_ptr<Message>_msg, P2PNetworkInterface*sender) {
    MessageOf<pair<uint16_t,uint8_t>>* msg = static_cast<MessageOf<pair<uint16_t,uint8_t>>*>(_msg.get());
    pair<uint16_t,uint8_t> msgData = *msg->getData();

    console << "Rec.D " << int(msgData.first) << "," << int(msgData.second) << "\n";
    if (isLeader) {
        setColor(neighborhood.isExternalBorder()?RED:YELLOW);
    } else {
        order = msgData.first;
        neighborhood.setExternalBorder(msgData.second);
        setColor(msgData.second);
        sendMessage(new MessageOf<pair<uint16_t,uint8_t>>(DISTANCE_MSG_ID, make_pair(order+1,msgData.second)),
                        module->getInterface(neighborhood.getTo()), 1000, 0);
    }
}

void Neighborhood::setNeighbor(SCLattice::Direction dir, bool value) {
    switch (dir) {
        case SCLattice::West:
            validity |= 0b00000010;
            if (value) state |= 0b00000010; else state &= 0b11111101;
            break;
        case SCLattice::South: // 0
            validity |= 0b00010000;
            if (value) state |= 0b00010000; else state &= 0b11101111;
            break;
        case SCLattice::East: // 1
            validity |= 0b01000000;
            if (value) state |= 0b01000000; else state &= 0b10111111;
            break;
        case SCLattice::North:
            validity |= 0b00001000;
            if (value) state |= 0b00001000; else state &= 0b11110111;
            break;
    }
}

void Neighborhood::setNeighbor(short dx, short dy, bool value) {
    uint8_t code = 0;
    if (dy == -1) {
        code = 1 >> (dx);
    } else if (dy == 0) {
        code = (dx == -1 ? 8 : 16);
    } else if (dy == 1) {
        code = 0b00100000 >> (dx);
    }
    validity |= code;
    if (value) state |= code; else state &= (255 - code);
}

uint8_t Neighborhood::getNeighborCode(SCLattice::Direction dir) {
    uint8_t res = 0;
    switch (dir) {
        case SCLattice::Direction::South :
            res |= (state & 2) != 0;
            res |= ((state & 64) != 0) << 1;
            break;
        case SCLattice::Direction::East :
            res |= (state & 16) != 0;
            res |= ((state & 8) != 0) << 1;
            break;
        case SCLattice::Direction::North :
            res |= (state & 64) != 0;
            res |= ((state & 2) != 0) << 1;
            break;
        case SCLattice::Direction::West :
            res |= (state & 8) != 0;
            res |= ((state & 16) != 0) << 1;
            break;
    }
    return res;
}

bool Neighborhood::merge(SCLattice::Direction dir, uint8_t code) {
    uint8_t oldValidity = validity;
    switch (dir) {
        case SCLattice::Direction::North :
            validity |= 0b00100001; // 1 & 32
            state |= ((code & 1) != 0);
            state |= (((code & 2) != 0) << 5);
            break;
        case SCLattice::Direction::West :
            validity |= 0b00000101; // 4 & 1
            state |= ((code & 1) != 0) << 2;
            state |= (((code & 2) != 0));
            break;
        case SCLattice::Direction::South :
            validity |= 0b10000100; // 128 & 4
            state |= ((code & 1) != 0) << 7;
            state |= (((code & 2) != 0) << 2);
            break;
        case SCLattice::Direction::East :
            validity |= 0b10100000; // 32 & 128
            state |= ((code & 1) != 0) << 5;
            state |= (((code & 2) != 0)) << 7;
            break;
    }
    return validity != oldValidity;
}

void Neighborhood::complete() {
    // corner 1
    if ((state & 2) == 0 && (validity & 2) && (state & 8) == 0 && (validity & 8)) {
        state &= 0b11111110;
        validity |= 0b00000001;
    }
    // corner 4
    if ((state & 2) == 0 && (validity & 2) && (state & 16) == 0 && (validity & 16)) {
        state &= 0b11111011;
        validity |= 0b00000100;
    }
    // corner 128
    if ((state & 64) == 0 && (validity & 64) && (state & 16) == 0 && (validity & 16)) {
        state &= 0b01111111;
        validity |= 0b10000000;
    }
    // corner 32
    if ((state & 64) == 0 && (validity & 64) && (state & 8) == 0 && (validity & 8)) {
        state &= 0b11011111;
        validity |= 0b00100000;
    }
}

/* Example of onGlDraw Function
 / which draws a 3x3x10 box in each cell of the grid.
 * This function is executed once by the first or the selected module */
void BordersCode::onGlDraw() {
    static const float red[4]={1.2f,0.2f,0.2f,1.0f};
    static const float green[4]={0.2f,1.2f,0.2f,1.0f};

    auto bbs=BlinkyBlocksWorld::getWorld()->getMap();
    int ix,iy;
    for (auto &bb:bbs) {
        auto nh = static_cast<BordersCode*>(bb.second->blockCode)->getNeiborhood();

        if (nh.isBorder()) {
            auto glBc = static_cast<BlinkyBlocksGlBlock*>(bb.second->getGlBlock());

            glPushMatrix();
            glTranslatef(glBc->position[0]+20,glBc->position[1]+20,50);
            glRotatef(-90.0f*(float)(glBc->rotCoef),0,0,1.0f);

            nh.getFromPosition(ix,iy);
            glMaterialfv(GL_FRONT,GL_AMBIENT_AND_DIFFUSE,red);
            glLineWidth(10);
            glBegin(GL_LINES);
            glVertex3f(ix,iy,0.0);
            glVertex3f(0.0,0.0,0.0);
            glEnd();
            nh.getToPosition(ix,iy);
            glMaterialfv(GL_FRONT,GL_AMBIENT_AND_DIFFUSE,green);
            glBegin(GL_LINES);
            glVertex3f(ix,iy,0.0);
            glVertex3f(0.0,0.0,0.0);
            glEnd();
            glLineWidth(1);

            glPopMatrix();
        }
    }
}