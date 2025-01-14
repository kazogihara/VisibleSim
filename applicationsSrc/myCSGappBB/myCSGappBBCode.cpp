
/**
 * @file myCSGappCode.cpp
 * Generated by VisibleSim BlockCode Generator
 * https://services-stgi.pu-pm.univ-fcomte.fr/visiblesim/generator.php#
 * @author yourName
 * @date 2023-05-11                                                                     
 **/
 
#include "myCSGappBBCode.hpp"

MyCSGappBBCode::MyCSGappBBCode(BlinkyBlocksBlock *host):BlinkyBlocksBlockCode(host),module(host) {
    // @warning Do not remove block below, as a blockcode with a NULL host might be created
    //  for command line parsing
    if (not host) return;

    // Registers a callback (myBroadcastFunc) to the message of type R
    addMessageEventFunc2(BROADCAST_MSG_ID,
                         std::bind(&MyCSGappBBCode::myBroadcastFunc,this,
                                   std::placeholders::_1, std::placeholders::_2));

    // Registers a callback (myBackFunc) to the message of type C
    addMessageEventFunc2(BACK_MSG_ID,
                         std::bind(&MyCSGappBBCode::myBackFunc,this,
                                   std::placeholders::_1, std::placeholders::_2));

}

void MyCSGappBBCode::startup() {
    if (getId()==476) {
        setColor(RED);
        nbWaitedAnswers=sendMessageToAllNeighbors(new MessageOf<pair<uint16_t,uint16_t>>(BROADCAST_MSG_ID,pair(0,1)),100,0,0);
    }
}

void MyCSGappBBCode::myBroadcastFunc(std::shared_ptr<Message>_msg, P2PNetworkInterface*sender) {
    MessageOf<pair<uint16_t,uint16_t>>*msg = static_cast<MessageOf<pair<uint16_t,uint16_t>>*>(_msg.get());
    pair<uint16_t,uint16_t> msgData = *msg->getData();
    uint16_t msgDist=msgData.first;
    uint16_t msgRound=msgData.second;

//    console << "rec. Flood (" << msgDist << "," << msgRound << ") from " << sender->getConnectedBlockId() << "\n";
    if (parent==nullptr || msgDist<distance) {
        distance=msgDist;
        maxDistance=distance;
        setColor(distance);
        currentRound=msgRound;
        parent=sender;
        nbWaitedAnswers=sendMessageToAllNeighbors(new MessageOf<pair<int,int>>(BROADCAST_MSG_ID,make_pair(distance+1,currentRound)),1000,100,1,sender);
        if (nbWaitedAnswers==0) {
            sendMessage(new MessageOf<uint16_t>(BACK_MSG_ID,distance),parent,10,0);
        }
    } else {
        sendMessage(new MessageOf<uint16_t>(BACK_MSG_ID,0),sender,10,0);
    }
}

void MyCSGappBBCode::myBackFunc(std::shared_ptr<Message>_msg, P2PNetworkInterface*sender) {
    MessageOf<uint16_t>* msg = static_cast<MessageOf<uint16_t>*>(_msg.get());
    uint16_t msgData = *msg->getData();

    nbWaitedAnswers--;
    if (msgData>maxDistance) maxDistance=msgData;
    console << "rec. Ack(" << int(msgData) << ") from " << sender->getConnectedBlockId() << "\n";
    if (nbWaitedAnswers==0) {
        if (parent==nullptr) {
            setColor(WHITE);
            cout << "#------------------------------------#\nExcentricity="<< maxDistance << endl;
        } else {
            sendMessage(new MessageOf<uint16_t>(BACK_MSG_ID,maxDistance),parent,1000,100);
        }
    }
}
