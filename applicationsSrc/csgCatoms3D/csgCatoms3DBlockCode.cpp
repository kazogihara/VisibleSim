/*
 * csgCatoms3DBlockCode.cpp
 *
 *  Created on: 05 August 2015
 *  Author: Thadeu
 */

#include <iostream>
#include <sstream>
#include "csgCatoms3DBlockCode.h"
#include "catoms3DBlock.h"
#include "scheduler.h"
#include "events.h"
#include <chrono>
#include <fstream>
#define MAX 1
#define INF 99999

using namespace std;
using namespace Catoms3D;

int CsgCatoms3DStats::difference_bitmap = 0;
int CsgCatoms3DStats::difference_mesh = 0;
int CsgCatoms3DStats::difference_stoy = 0;
int CsgCatoms3DStats::total_csg = 0;
double CsgCatoms3DStats::bitmap_time_elapsed = 0;
double CsgCatoms3DStats::csg_time_elapsed = 0;
double CsgCatoms3DStats::stoy_time_elapsed = 0;
double CsgCatoms3DStats::mesh_time_elapsed = 0;
bool CsgCatoms3DBlockCode::bitmap[27000] = {0};
int CsgCatoms3DBlockCode::side_size = 0;
CSGNode* CsgCatoms3DBlockCode::csgRoot = NULL;

CsgCatoms3DBlockCode::CsgCatoms3DBlockCode(Catoms3DBlock *host):Catoms3DBlockCode(host) {
	cout << "CsgCatoms3DBlockCode constructor" << endl;
    scheduler = getScheduler();
	catom = (Catoms3DBlock*)hostBlock;
}

CsgCatoms3DBlockCode::~CsgCatoms3DBlockCode() {
	cout << "CsgCatoms3DBlockCode destructor" << endl;
}

void CsgCatoms3DBlockCode::generateBitmap(int side_size) {
    int x = catom->position.pt[0];
    int y = catom->position.pt[1];
    int z = catom->position.pt[2];
    int pos = x + y*side_size + z*side_size*side_size;
    Color c;
    bitmap[pos] = csgRoot->isInside(myPosition, c);
}

void CsgCatoms3DBlockCode::startup() {
	stringstream info;
	info << "Starting  ";
	scheduler->trace(info.str(),hostBlock->blockId);

    hasPosition = false;
	if (catom->blockId==1) {
        csgRoot = csgUtils.readFile("data/mug-rotate.bc");
        csgRoot->toString();
        BoundingBox bb;
        csgRoot->boundingBox(bb);
        cout << bb.P0 << ' ' << bb.P1 << endl;

        stoyUtils.readFile("data/mug-high.stoy");
        meshUtils.readFile("data/mug-highresolution.obj");
        bitmapUtils.readFile("data/mug.bmp");
        myPosition = bb.P0;


        Color color;
        if (csgRoot->isInside(myPosition, color)) {
            catom->setColor(color);
        }
        else {
            catom->setVisible(false);
        }

        hasPosition = true;
        sendCSGMessage();
	}
}


void CsgCatoms3DBlockCode::processLocalEvent(EventPtr pev) {
	MessagePtr message;
	stringstream info;

	switch (pev->eventType) {
    case EVENT_NI_RECEIVE: {
      message = (std::static_pointer_cast<NetworkInterfaceReceiveEvent>(pev))->message;
        switch(message->id) {
            case CSG_MSG_ID:
            {
                if (!hasPosition) {
                    catom->setColor(PINK);
                    CSG_message_ptr recv_message = std::static_pointer_cast<CSG_message>(message);

                    // When the scene is transfered to each catom.
                    /*
                    char *csgBuffer = recv_message->getCsgBuffer();
                    int csgBufferSize = recv_message->getCsgBufferSize();
                    csgRoot = csgUtils.readCSGBuffer(csgBuffer, csgBufferSize);
                    stoyUtils.setBricks(recv_message->getBricks());
                    bitmapUtils.setBitmap(recv_message->getBitmap());
                    */

                    myPosition = recv_message->getPosition();

                    Color color;
                    if (csgRoot->isInside(myPosition, color)) {
                        catom->setColor(color);
                    }
                    else {
                        catom->setVisible(false);
                    }

                    hasPosition = true;
                    sendCSGMessage();
                }
                break;
            }
          }
      }
      break;
	}
}
/*
void CsgCatoms3DBlockCode::methodsDifference() {
    if (csgUtils.isInside(myPosition) != bitmapUtils.isInside(catom->position, side_size))
        difference_bitmap++;
    if (csgUtils.isInside(myPosition) != stoyUtils.isInside(myPosition))
        difference_stoy++;
    if (csgUtils.isInside(myPosition) != meshUtils.isInside(myPosition))
        difference_mesh++;
    if (csgUtils.isInside(myPosition))
        total_csg++;
}
*/

void CsgCatoms3DBlockCode::benchmark() {
    calcBitmap();
    calcCSG();
    calcStoy();
    calcMesh();
}

void CsgCatoms3DBlockCode::calcBitmap() {
    auto begin = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < MAX; i++) {
        bitmapUtils.isInside(catom->position, side_size);
    }
    auto end = std::chrono::high_resolution_clock::now();
    CsgCatoms3DStats::bitmap_time_elapsed += (std::chrono::duration_cast<std::chrono::nanoseconds>(end-begin).count())/MAX;

    cout << std::chrono::duration_cast<std::chrono::nanoseconds>(end-begin).count()/MAX << ";";
}

void CsgCatoms3DBlockCode::calcCSG() {
    auto begin = std::chrono::high_resolution_clock::now();
    Color c;
    for (int i = 0; i < MAX; i++) {
        csgRoot->isInside(myPosition, c);
    }
    auto end = std::chrono::high_resolution_clock::now();
    CsgCatoms3DStats::csg_time_elapsed += (std::chrono::duration_cast<std::chrono::nanoseconds>(end-begin).count())/MAX;

    cout << std::chrono::duration_cast<std::chrono::nanoseconds>(end-begin).count()/MAX << ";";
}

void CsgCatoms3DBlockCode::calcStoy() {
    auto begin = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < MAX; i++) {
        stoyUtils.isInside(myPosition);
    }
    auto end = std::chrono::high_resolution_clock::now();
    CsgCatoms3DStats::stoy_time_elapsed += (std::chrono::duration_cast<std::chrono::nanoseconds>(end-begin).count())/MAX;

    cout << std::chrono::duration_cast<std::chrono::nanoseconds>(end-begin).count()/MAX << ";";
}

void CsgCatoms3DBlockCode::calcMesh() {
    auto begin = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < MAX; i++) {
        meshUtils.isInside(myPosition);
    }
    auto end = std::chrono::high_resolution_clock::now();
    CsgCatoms3DStats::mesh_time_elapsed += (std::chrono::duration_cast<std::chrono::nanoseconds>(end-begin).count())/MAX;

    cout << std::chrono::duration_cast<std::chrono::nanoseconds>(end-begin).count()/MAX << endl;
}

void CsgCatoms3DBlockCode::sendCSGMessage() {
    for (int i = 0; i < 12; i++) {
        if (catom->getInterface(i)->connectedInterface != NULL) {
            Vector3D pos(
                myPosition.pt[0] + Catoms3D::tabConnectorPositions[i][0], 
                myPosition.pt[1] + Catoms3D::tabConnectorPositions[i][1],
                myPosition.pt[2] + Catoms3D::tabConnectorPositions[i][2],
                1);
            CSG_message *message = new CSG_message(csgUtils.getCSGBuffer(), csgUtils.getCSGBufferSize(), stoyUtils.getBricks(), bitmapUtils.getBitmap(), pos);
            scheduler->schedule(new NetworkInterfaceEnqueueOutgoingEvent(scheduler->now() + 100, message, catom->getInterface(i)));
        }
    }
}

CSG_message::CSG_message(char *_csgBuffer, int _csgBufferSize, vector<Brick> _bricks, string _bitmap, Vector3D pos) {
	id = CSG_MSG_ID;

//    csgBuffer = new char[_csgBufferSize];
//    memcpy(csgBuffer, _csgBuffer, _csgBufferSize);

//    csgBufferSize = _csgBufferSize;

    //bricks = _bricks;
    //bitmap = _bitmap;
    position = pos;
}

CSG_message::~CSG_message() {
}

BlockCode* CsgCatoms3DBlockCode::buildNewBlockCode(BuildingBlock *host) {
    return (new CsgCatoms3DBlockCode((Catoms3DBlock*)host));
}

