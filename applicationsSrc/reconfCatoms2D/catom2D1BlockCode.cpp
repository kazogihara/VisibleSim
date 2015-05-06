/*
 * catom2D1BlockCode.cpp
 *
 *  Created on: 12 avril 2013
 *      Author: ben
 */

#include <iostream>
#include <sstream>
#include "catom2D1BlockCode.h"
#include "scheduler.h"
#include "events.h"
#include "catoms2DEvents.h"
//MODIF NICO
#include <boost/shared_ptr.hpp>

#include "reconfCatoms2DMessages.h"
#include "reconfCatoms2DEvents.h"
#include "angle.h"
#include <float.h>

using namespace std;
using namespace Catoms2D;

//#define MAP_DEBUG
#define GEO_ROUTING_DEBUG
//#define ANGLE_DEBUG
//#define TUPLE_DEBUG

Coordinate Catoms2D1BlockCode::ccth;

Catoms2D1BlockCode::Catoms2D1BlockCode(Catoms2DBlock *host):Catoms2DBlockCode(host) {
  scheduler = Catoms2D::getScheduler();
  catom2D = (Catoms2DBlock*)hostBlock;
  connectedToHost = false;
  waiting = 0;
  toHost = NULL;
  positionKnown = false;
}

Catoms2D1BlockCode::~Catoms2D1BlockCode() {
}

void Catoms2D1BlockCode::updateBorder() {
  for (int i = 0; i < MAX_NB_NEIGHBORS; i++) {
    if (catom2D->getInterface((NeighborDirection::Direction)i)->connectedInterface == NULL) {
      catom2D->setColor(RED);
      return;
    }
  }
  catom2D->setColor(GREEN);
}

bool Catoms2D1BlockCode::canMove() {
  int nbNeighbors = catom2D->nbNeighbors();
  int nbConsecutiveNeighbors = catom2D->nbConsecutiveNeighbors();
  return ((nbNeighbors == 1) || ((nbNeighbors == 2) && (nbConsecutiveNeighbors == 2)));
}

void Catoms2D1BlockCode::startup() {
  stringstream info;
  info << "Starting ";
  scheduler->trace(info.str(),hostBlock->blockId);
  
  //updateBorder();
  /*if (canMove()) {
    catom2D->setColor(RED);
    } else {
    catom2D->setColor(GREEN);
    }*/
  //catom2D->setColor(DARKGREY);
	
  /*if (catom2D->blockId == 10) {
    cout << "@" << catom2D->blockId << " " << catom2D->position << endl;
    startMotion(ROTATE_LEFT,world->getBlockById(8));
    }
	
    if (catom2D->blockId == 8) {
    cout << "@" << catom2D->blockId << " " << catom2D->position << endl;
    startMotion(ROTATE_LEFT,world->getBlockById(7));
    }*/
	
  /*if (catom2D->blockId == 4) {
    cout << "@" << catom2D->blockId << " " << catom2D->position << endl;
    startMotion(ROTATE_LEFT,world->getBlockById(3));
    }*/
		
  if (catom2D->blockId == 1) {
    connectedToHost = true;
    toHost = NULL;
  }
  
  if(connectedToHost) {
    Coordinate c = Coordinate(0,0);
    setPosition(c);
    ccth.x = catom2D->position[0];
    ccth.y = catom2D->position[2];
    buildMap();
  }
}

void Catoms2D1BlockCode::processLocalEvent(EventPtr pev) {
  MessagePtr message;
  stringstream info;
  
  switch (pev->eventType) {
  case EVENT_NI_RECEIVE: {
    message = (boost::static_pointer_cast<NetworkInterfaceReceiveEvent>(pev))->message;
    P2PNetworkInterface * recv_interface = message->destinationInterface;
    switch(message->type) {
    case GO_MAP_MSG: {
      GoMapMessage_ptr m = boost::static_pointer_cast<GoMapMessage>(message);
      if (!positionKnown) {
	toHost = recv_interface;
	Coordinate c = m->getPosition(); //getPosition(toHost, m->getLast());
	setPosition(c);
#ifdef MAP_DEBUG
	Coordinate real;
	real.x = catom2D->position[0];
	real.y = catom2D->position[2];
	real.x -= ccth.x;
	real.y -= ccth.y;
	cout << "@" << catom2D->blockId <<  " position " << position << " vs " << real << endl;
#endif
	waiting = 0;
	buildMap();
	if (waiting==0) {
	  mapBuilt(toHost);
	}
      } else {
	mapBuilt(recv_interface);
      }
    }
      break;
    case BACK_MAP_MSG: {
      BackMapMessage_ptr m = boost::static_pointer_cast<BackMapMessage>(message);
      waiting--;
#ifdef MAP_DEBUG
      //cout << "@" << catom2D->blockId <<  " back msg " << waiting << endl;
#endif
      if (!waiting) {
	if (!connectedToHost) {
	  mapBuilt(toHost);
	} else {
	  cout << "@" << catom2D->blockId << " is receiving the target map and disseminating it..." << endl;
	  // Link to PC host simulation:
	  out(new ContextTuple(Coordinate(2,2), string("target"))); 
	  //out(new ContextTuple(Coordinate(1,3), string("target")));
	  //out(new ContextTuple(Coordinate(1,2), string("target")));
	  /*Catoms2DWorld *world = Catoms2DWorld::getWorld();
	  int *gridSize = world->getGridSize();
	  for (int iy = 0; iy < gridSize[2]; iy++) {
	    for (int ix = 0; ix < gridSize[0]; ix++) {
	      if (world->getTargetGrid(ix,0,iy) == fullCell ) {
		Coordinate t(ix,iy);
		t.x -= ccth.x;
		t.y -= ccth.y;
		cout << "(" << t.x << " " << t.y << ")" << endl;
		out(new ContextTuple(Coordinate(t.x,t.y), string("target")));
		//localTuples.out(Tuple(string("target"), ix, iy));
		//tuples.out(new Tuple(string("aaa"), 5, 12.5));
		//Tuple query(string("aaa"), TYPE(int), 12.5);  
		//Tuple *res = tuples.inp(query);
	      }
	    }
	    }*/
	}
      }
    }
      break;
    case GEO_TUPLE_MSG: {
      GeoMessage_ptr m = boost::static_pointer_cast<GeoMessage>(message); 
#ifdef GEO_ROUTING_DEBUG
      // cout << "Geo message: " << m->getSource() << " -> ... ->  " << m->getLast() << " -> ... -> " << position << " -> ... -> " <<  m->getDestination() <<  endl;
      cout << "Geo message: s=" << m->getSource() << " d=" << m->getDestination() << " l=" << getPosition(recv_interface) << " p=" <<  position <<  endl;
#endif
      if (position == m->getDestination()) {
	// message well arrived
#ifdef GEO_ROUTING_DEBUG
	cout << "msg had a safe trip!" << endl;
#endif
      } else {
	// message is not arrived
	switch (m->getMode()) {
	case GeoMessage::mode_t::GREEDY:
	  {
	    P2PNetworkInterface *next = getClosestInterface(m->getDestination(), recv_interface);
	    if(next != NULL) {
#ifdef GEO_ROUTING_DEBUG
	      cout << "Greedy forward to " << getPosition(next) << endl;
#endif
	      forward(m,next);
	    } else {
	      // perimeter mode
	      m->setPerimeterMode(position);
	      // find interface
	      next = getNextCounterClockWiseInterface(m->getDestination());
	      forward(m,next);
 #ifdef GEO_ROUTING_DEBUG
	  cout << "Perimeter (new) forward to " << getPosition(next) << endl;
#endif
	    }
	  }
	  break;
	case GeoMessage::mode_t::PERIMETER: {
	  if (m->getPerimeterStart() == position) {
	    // packet has made a complete tour
	    cout << "packet has made a complete tour (" 
		 << m->getTuple() 
		 << ")"
		 << endl;
	  } else {
	    int d1 = distance(position, m->getDestination());
	    int d2 = distance(m->getPerimeterStart(),m->getDestination());
	      
	    if (d1 < d2) {
	      // leave PERIMETER mode
	      m->setGreedyMode();
	      P2PNetworkInterface *next = getClosestInterface(m->getDestination(), recv_interface);
	      if(next != NULL) {
#ifdef GEO_ROUTING_DEBUG
		cout << "Greedy forward to " << getPosition(next) << endl;
#endif
		forward(m,next);
	      }
	    } else {
	      // an incident edge hit/cut the segment 
	      // (destination;point enter in perimeter mode)
	      Segment s(m->getDestination(),m->getPerimeterStart()); 
	      P2PNetworkInterface *p2p = getIntersectInterface(s,NULL);
	      if ((p2p != NULL) && (getPosition(p2p) != m->getPerimeterStart())) {
		Coordinate p = getPosition(p2p);
		P2PNetworkInterface *next = getNextCounterClockWiseInterface(p);
		forward(m,next);
#ifdef GEO_ROUTING_DEBUG
		cout << "Perimeter (new face?) forward to " << getPosition(next) << endl;
#endif
	      } else {
		P2PNetworkInterface *next = getNextCounterClockWiseInterface(recv_interface);
		forward(m,next);
#ifdef GEO_ROUTING_DEBUG
		cout << "Perimeter (same face) forward to " << getPosition(next) << endl;
#endif
	      }
	    }
	  }
	}
	  break;
	default:
	  cerr << "unknown mode" << endl;
	}

	/*
	  }
	  else {
	  // should enter/stay in perimeter mode
	  if (m->isInPerimeterMode()) {
	  else {
	  next = getNextCounterClockWiseInterface(recv_interface);      
	  forward(m,next);
	  #ifdef GEO_ROUTING_DEBUG
	  cout << "Perimeter forward to " << getPosition(next) << endl;
	  #endif
	  }
	  } else {
	  m->setPerimeterMode(position);
	  next = getNextCounterClockWiseInterface(recv_interface);
	  forward(m,next);
	  #ifdef GEO_ROUTING_DEBUG
	  cout << "Perimeter (new) forward to " << getPosition(next) << endl;
	  #endif
	  }
	  }
	  }*/
	getchar();
      }
    }
      break;
    default:
      cerr << "unknown message type" << endl;
    }
  }
    break;
  case EVENT_MOTION_END: {
    cout << "motion end" << endl;
    cout << "@" << catom2D->blockId << " " << catom2D->position << endl;
    catom2D->setColor(DARKGREY);
    break;
  }
  }
}

// TS
void Catoms2D1BlockCode::out(ContextTuple *t) {

#ifdef TUPLE_DEBUG
  cout << "insert tuple: " << *t << endl;
#endif

  P2PNetworkInterface *p2p = getClosestInterface(t->getLocation(),NULL);
  if (p2p == NULL) {
    // tuple should be stored locally
#ifdef TUPLE_DEBUG
    cout << "locally" << endl;
#endif
  } else {
    // tuple should be stored remotely
#ifdef TUPLE_DEBUG
    cout << "remotely" << endl;
#endif
    //(Coordinate s, Coordinate d, ContextTuple &t, data_mode_t dm
    GeoMessage * msg = new GeoMessage(position,t->getLocation(),*t,GeoMessage::STORE);
    scheduler->schedule(new NetworkInterfaceEnqueueOutgoingEvent(scheduler->now(), msg, p2p));
  }
}

// Geo-routing
P2PNetworkInterface* Catoms2D1BlockCode::getClosestInterface(Coordinate dest, P2PNetworkInterface *ignore) {
  P2PNetworkInterface *closest = NULL;
  int minDistance = distance(position,dest);
  for (int i = 0; i<6; i++) {
    P2PNetworkInterface *it = catom2D->getInterface((NeighborDirection::Direction)i);
    if((it == ignore) || !it->connectedInterface) {
      continue;
    }
    int d = distance(getPosition(it), dest);
    if (d < minDistance) {
      closest = it;
    }
  }
  return closest;
}

P2PNetworkInterface* Catoms2D1BlockCode::getIntersectInterface(Segment &s, P2PNetworkInterface *ignore) {
  for (int i = 0; i < 6; i++) {
    P2PNetworkInterface *it = catom2D->getInterface((NeighborDirection::Direction)i);
    Coordinate pdir = getPosition(it);
    if (ignore == it) {
      continue;
    }
    Segment seg = Segment(position,pdir);
    if(seg.intersect(s)) {
      return it;
    }
  }
  return NULL;
}

P2PNetworkInterface* Catoms2D1BlockCode::getNextCounterClockWiseInterface(Coordinate a) {
  double angles[6] = {0,0,0,0,0,0};
  int minI = 0;
  for (int i=0; i < 6; i++) {
    P2PNetworkInterface *p2p = catom2D->getInterface((NeighborDirection::Direction)i);
    if (p2p->connectedInterface == NULL) {
      angles[i] = DBL_MAX;
      continue;
    }
    Coordinate c = getPosition(p2p);
    angles[i] = ccwAngle(c,position,a);
#ifdef DEBUG_ANGLE
    cout << c << " " << angles[i] << endl;
#endif
    if (angles[i] < angles[minI]) {
      minI = i;
    }
  }
  return catom2D->getInterface((NeighborDirection::Direction)minI);
}

P2PNetworkInterface* Catoms2D1BlockCode::getNextCounterClockWiseInterface(P2PNetworkInterface *recv) {
  int d = catom2D->getDirection(recv);
  P2PNetworkInterface *next = NULL;
  do {
    d = (d+1)%6;
    next = catom2D->getInterface((NeighborDirection::Direction)d);
  } while (next->connectedInterface == NULL);
  return next;
}

P2PNetworkInterface* Catoms2D1BlockCode::getNextClockWiseInterface(P2PNetworkInterface *recv) {
  int d = catom2D->getDirection(recv);
  P2PNetworkInterface *next = NULL;
  cout << "arrival interface: " << d << endl;
  do {
    if (d == 0) { 
      d=5; 
    } else { 
      d--;
    }
    next = catom2D->getInterface((NeighborDirection::Direction)d);
  } while (next->connectedInterface == NULL);
  cout << "next interface: " << d << endl;
  return next;
}      

void Catoms2D1BlockCode::forward(GeoMessage_ptr m, P2PNetworkInterface *p2p) {
  GeoMessage * msg = new GeoMessage(m.get());
  scheduler->schedule(new NetworkInterfaceEnqueueOutgoingEvent(scheduler->now(), msg, p2p));
}

void Catoms2D1BlockCode::startMotion(int direction, Catoms2DBlock *pivot) {
  scheduler->schedule(new MotionStartEvent(scheduler->now(),catom2D,pivot,direction));
}

Catoms2D::Catoms2DBlockCode* Catoms2D1BlockCode::buildNewBlockCode(Catoms2DBlock *host) {
  return(new Catoms2D1BlockCode(host));
}

Coordinate Catoms2D1BlockCode::getPosition(P2PNetworkInterface *it) {
  Coordinate p = position;
  switch(catom2D->getDirection(it)) {
  case NeighborDirection::BottomLeft:
    if (p.y%2 == 0) {
      p.x--;
    }
    p.y--;
    break;
  case NeighborDirection::Left:
    p.x--;
    break;
  case NeighborDirection::TopLeft:    
    if (p.y%2 == 0) {
      p.x--;
    }
    p.y++;
    break;
  case NeighborDirection::TopRight:
    if (p.y%2 == 1) {
      p.x++;
    }
    p.y++;
    break;
  case NeighborDirection::Right:
    p.x++;
    break;
  case NeighborDirection::BottomRight: 
    if (p.y%2 == 1) {
      p.x++;
    }
    p.y--;
    break;
  }
  return p;
}


void Catoms2D1BlockCode::buildMap() {
  P2PNetworkInterface *p2p;
  for (int i=0; i<6; i++) {
    p2p = catom2D->getInterface((NeighborDirection::Direction)i);
    if( (p2p == toHost) || !p2p->connectedInterface) {
      continue;
    }
    GoMapMessage * msg = new GoMapMessage(getPosition(p2p));
    scheduler->schedule( new NetworkInterfaceEnqueueOutgoingEvent(scheduler->now(), msg, p2p));
    waiting++;
  }
}

void Catoms2D1BlockCode::mapBuilt(P2PNetworkInterface *d) {
  BackMapMessage * msg = new BackMapMessage();
  scheduler->schedule( new NetworkInterfaceEnqueueOutgoingEvent(scheduler->now(), msg, d));
}

void Catoms2D1BlockCode::setPosition(Coordinate p) {
  position = p;
  localTuples.out(new ContextTuple(position, string("map")));
  positionKnown = true;
}

int Catoms2D1BlockCode::distance(Coordinate p1, Coordinate p2) {
  return abs(p2.x - p1.x) +  abs(p2.y - p1.y); 
}

//bool legalMove(int sens, 
