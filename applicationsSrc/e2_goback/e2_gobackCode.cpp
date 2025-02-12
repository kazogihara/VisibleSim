/**
 * @file e2_GoBackCode.cpp
 * Generated by VisibleSim BlockCode Generator
 * https://services-stgi.pu-pm.univ-fcomte.fr/visiblesim/generator.php#
 * @author yourName
 * @date 2022-09-20
 **/
#include "e2_gobackCode.hpp"

E2_GoBackCode::E2_GoBackCode(BlinkyBlocksBlock *host):BlinkyBlocksBlockCode(host),module(host) {
    // @warning Do not remove block below, as a blockcode with a NULL host might be created
    //  for command line parsing
    if (not host) return;

    // Registers a callback (myBroadcastFunc) to the message of type R
    addMessageEventFunc2(GO_MSG_ID,
                       std::bind(&E2_GoBackCode::myGoFunc,this,
                       std::placeholders::_1, std::placeholders::_2));

    // Registers a callback (myForecastFunc) to the message of type R
    addMessageEventFunc2(BACK_MSG_ID,
                       std::bind(&E2_GoBackCode::myBackFunc,this,
                       std::placeholders::_1, std::placeholders::_2));

}

void E2_GoBackCode::startup() {
    console << "start " << getId() << "\n";
    myDistance=10000;
    myParent=nullptr;
    if (isLeader) {
        setColor(RED);
        myNbWaitedAnswer=sendMessageToAllNeighbors("go",
                                  new MessageOf<int>(GO_MSG_ID,1), 100, 1000, 0);
    }
}

void E2_GoBackCode::myGoFunc(std::shared_ptr<Message>_msg, P2PNetworkInterface*sender) {
    MessageOf<int>* msg = static_cast<MessageOf<int>*>(_msg.get());
    int msgData = *msg->getData();

    console << "rcv " << msgData << " from " << sender->getConnectedBlockBId() << "\n";
    if (myParent==nullptr) {
        myDistance = msgData;
        myParent=sender;
        setColor(msgData);
        myNbWaitedAnswer=sendMessageToAllNeighbors("go",
                                  new MessageOf<int>(GO_MSG_ID,msgData + 1), 100, 1000, 1, sender);
        if (myNbWaitedAnswer==0) {
            sendMessage("back",new Message(BACK_MSG_ID),myParent,100,1000);
        }
    } else {
        sendMessage("back",new Message(BACK_MSG_ID),sender,100,1000);
    }
}

void E2_GoBackCode::myBackFunc(std::shared_ptr<Message>_msg, P2PNetworkInterface*sender) {
    console << "rcv ACK  from " << sender->getConnectedBlockBId() << "\n";
    myNbWaitedAnswer--;
    if (myNbWaitedAnswer==0) {
        if (myParent) {
            sendMessage("back", new Message(BACK_MSG_ID), myParent, 100, 1000);
        } else {
            setColor(BLACK);
        }
    }
}

void E2_GoBackCode::parseUserBlockElements(TiXmlElement *config) {
    const char *attr = config->Attribute("leader");
    if (attr!=nullptr) {
        std::cout << getId() << " is leader!" << std::endl; // complete with your code
        isLeader=true;
    }
}
