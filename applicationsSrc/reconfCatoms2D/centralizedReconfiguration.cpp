#include <iostream>
#include "centralizedReconfiguration.h"
#include "bfs.h"
#include "catoms2DWorld.h"
#include "coordinate.h"
#include "catoms2DMove.h"
#include "map.h"

using namespace std;
using namespace Catoms2D;

#define COLOR_DEBUG
#define ROTATION_DIRECTION Catoms2DMove::ROTATE_CW
#define UNDEFINED_GRADIENT -1

enum state_t {IN_SHAPE = 0, OUT_SHAPE = 1, WELL_PLACED = 2};

static Coordinate getMapBottomRight() {
  Catoms2DWorld *world = Catoms2DWorld::getWorld();
  Catoms2DBlock  *c;
  Coordinate rb(INT_MIN,INT_MAX);

  map<int, BuildingBlock*>::iterator it;
  for (it=world->getMap().begin() ; it != world->getMap().end(); ++it) {
    c = (Catoms2DBlock*) it->second;
    Coordinate cc(c->position[0], c->position[2]);
    /*if (cc.x >= rb.x) {
      rb.x = cc.x;
      if (cc.y <= rb.y) {
	rb.y = cc.y;
      }
      }*/
    if (cc.y <= rb.y) {
      rb.y = cc.y;
      if (cc.x >= rb.x) {
	rb.x = cc.x;
      }
    }
  }
  return rb;
}

static Coordinate getTargetBottomLeft() {
  Coordinate p(INT_MIN,INT_MIN);
  Catoms2DWorld *world = Catoms2DWorld::getWorld();
  int *gridSize = world->getGridSize();
  for (int iy = 0; iy < gridSize[2]; iy++) {
    for (int ix = 0; ix < gridSize[0]; ix++) {
      if (world->getTargetGrid(ix,0,iy) == fullCell ) {
	Coordinate cc(ix, iy);
	if (cc.y <= p.y) {
	  p.y = cc.y;
	  if (cc.x <= p.x) {
	    p.x = cc.x;
	  }
	}
      }
    }
  }
  return p;
}

static bool isOver() {
  Catoms2DWorld *world = Catoms2DWorld::getWorld();
  int *gridSize = world->getGridSize();
  for (int iy = 0; iy < gridSize[2]; iy++) {
    for (int ix = 0; ix < gridSize[0]; ix++) {
      if (world->getTargetGrid(ix,0,iy) == fullCell ) {
        if (!world->getGridPtr(ix,0,iy)) {
	  return false;
	}
      }
    }
  }
  return true;
}

static P2PNetworkInterface* 
nextInterface(Catoms2DBlock *c, Catoms2DMove::direction_t d, P2PNetworkInterface *p2p) {
  int p2pDirection = c->getDirection(p2p);
  cout << "next: ";
  cout << "i: " << p2pDirection;
  if (d == Catoms2DMove::ROTATE_CCW) {
    if (p2pDirection == NeighborDirection::BottomRight) {
      p2pDirection = NeighborDirection::Right;
    } else {
      p2pDirection++;
    }
  } else if (d == Catoms2DMove::ROTATE_CW) {
    if (p2pDirection == NeighborDirection::Right) {
      p2pDirection = NeighborDirection::BottomRight;
    } else {
      p2pDirection--;
    }
  }
  cout << "n: " << p2pDirection << endl;
  return c->getInterface((NeighborDirection::Direction)p2pDirection);
}

static Catoms2DMove* nextMove(Catoms2DBlock  *c) {
  P2PNetworkInterface *p1 = NULL, *p2 = NULL;
  Catoms2DBlock  *pivot;
  int i = 0;
  
  // pick-up a neighbor of c
  for (i = 0; i < 6; i++) {
    p1 = c->getInterface((NeighborDirection::Direction)i);
    if (p1->connectedInterface) {
      break;
    }
  }
  
  if ((i == 5) && !p1->connectedInterface) {
    return NULL;
  }

  p2 = p1;
  cout << "p1 dir: " << c->getDirection(p2) << endl;
  while (true) {
    if (ROTATION_DIRECTION == Catoms2DMove::ROTATE_CCW) {
      p2 = nextInterface(c, Catoms2DMove::ROTATE_CW, p2);
    } else if (ROTATION_DIRECTION == Catoms2DMove::ROTATE_CW) {
      p2 = nextInterface(c, Catoms2DMove::ROTATE_CCW, p2);
    }
    
    cout << "p2 dir: " << c->getDirection(p2) << endl;
    if (p2->connectedInterface) {
      cout << "connected p2 dir: " << c->getDirection(p2) << endl;
      pivot = (Catoms2DBlock*)p2->connectedInterface->hostBlock;
      Catoms2DMove m(pivot,ROTATION_DIRECTION);
      if (c->canMove(m)) {
	cout << c->blockId << " can move arround " << pivot->blockId << endl;
	return new Catoms2DMove(m);
      }
      cout << "can't move" << endl;
    }
    if (p1 == p2) {
      return NULL;
    }
  }
}

static Coordinate getPosition(Catoms2DBlock* c, Catoms2DMove &m) {
  P2PNetworkInterface *p2p = m.getPivot()->getP2PNetworkInterfaceByBlockRef(c);
  Coordinate position(m.getPivot()->position[0], m.getPivot()->position[2]);
  p2p = nextInterface(m.getPivot(),m.getDirection(),p2p);
  return Map::getPosition(m.getPivot(),position,p2p);
}

static bool isInTarget(Coordinate &p) { 
  Catoms2DWorld *world = Catoms2DWorld::getWorld();
  return (world->getTargetGrid(p.x,0,p.y) == fullCell);
}

static bool canMove(Catoms2DBlock *c, int gradient[]) {
  // can algorithmically move (gradient higher or equal than all neighbors)
  int id1 = c->blockId; 

  if (gradient[id1] ==  UNDEFINED_GRADIENT) {
    return false;
  }

  for (int i = 0; i < 6; i++) {
    P2PNetworkInterface *p2p = c->getInterface((NeighborDirection::Direction)i);
    if (p2p->connectedInterface) {
      int id2 = p2p->connectedInterface->hostBlock->blockId;  

      if (gradient[id2] ==  UNDEFINED_GRADIENT) {
	return false;
      }

      if (gradient[id1] < gradient[id2]) {
	return false;
      }
    }
  }
  return true;
}

static void move(Catoms2DBlock* c, Catoms2DMove &m) {
  if (!c->canMove(m)) {
    cerr << "error illegal move" << endl;
    return;
  } 
  Catoms2DWorld *world = Catoms2DWorld::getWorld();
  // final position
  Coordinate p = getPosition(c,m);
  // disconnect
  world->disconnectBlock(c);
  // rotate
  c->angle += 60*m.getDirection();
  // set grid and update gl world
  world->setGridPtr(p.x,0,p.y,c);
  c->setPosition(Vecteur(p.x,0,p.y)); 
  world->updateGlData(c,world->gridToWorldPosition(c->position),c->angle);
  // connect
  world->connectBlock(c);
}

static void updateGradient(Catoms2DBlock *c, int gradient[]) {
// can algorithmically move (gradient higher or equal than all neighbors)
  int id1 = c->blockId;
  int minGradient = INT_MAX;
    
  for (int i = 0; i < 6; i++) {
    P2PNetworkInterface *p2p = c->getInterface((NeighborDirection::Direction)i);
    if (p2p->connectedInterface) {
      int id2 = p2p->connectedInterface->hostBlock->blockId;
      if (gradient[id2] == UNDEFINED_GRADIENT) {
	cerr << "error: update gradient" << endl;
      }
      minGradient = min(minGradient,gradient[id2]);
    }
  }
  gradient[id1] = minGradient + 1;
}

void centralized_reconfiguration() {
  cout << "centralized reconfiguration" << endl;
  Catoms2DWorld *world = Catoms2DWorld::getWorld();
  Catoms2DBlock *seed = NULL;
  int gradient[world->getSize()+1];
  //state_t states[world->getSize()+1];
  Tree *bfs = NULL;
  
  Coordinate mapBottomRight = getMapBottomRight();
  seed = world->getGridPtr(mapBottomRight.x,0,mapBottomRight.y);
#ifdef COLOR_DEBUG
  seed->setColor(RED);
#endif
  
  for (int i = 0; i < (world->getSize()+1); i++) {
    gradient[i] = UNDEFINED_GRADIENT;
    //if (
    //states[i] = 
  }

  bfs = Tree::bfs(seed->blockId,gradient,world->getSize());

  while (!isOver()) {
    // algorithm moving condition of catom c1:
    // FALSE ???:
    // c1 gradient will be lower in the destination cell.
    // none of the gradient of c1's neighbors will change (ie gradient is lower or equal to c1).
    
    // physical moving condition
    // move CW around i connector: i+1 and i+2 should be free
    // move CCW around i connector: i-1 and i-2 should be free
    Catoms2DWorld *world = Catoms2DWorld::getWorld();
    Catoms2DBlock  *c;

    map<int, BuildingBlock*>::iterator it;
    for (it=world->getMap().begin() ; it != world->getMap().end(); ++it) {

      c = (Catoms2DBlock*) it->second;
      if (c == seed) {
	continue;
      }

      if (canMove(c,gradient)) {
	//cout << "c satisfies gradient condition" << endl;
	//cout << "@" << c->blockId << " can physically move" << endl;
	Coordinate p1(c->position[0], c->position[2]);
	Catoms2DMove *mv = nextMove(c);
	if (mv != NULL) {
	  Coordinate p2 = getPosition(c,*mv);
	  if (!isInTarget(p1) || (isInTarget(p1) && isInTarget(p2))) {
	    cout << c->blockId << " is moving from " << p1 << " to " 
		 << p2 << " using " << mv->getPivot()->blockId << " in direction " << mv->getDirection() << "..."; 
	    move(c,*mv);
	    cout << c->blockId << " has " << c->nbNeighbors() << " neighbors" << endl; 
	    gradient[c->blockId] = UNDEFINED_GRADIENT;
	    updateGradient(c,gradient);
	    cout << " done" << endl;
	    getchar();
	  }
	  delete mv;
	} else { cout << " move == NULL" << endl;}
      }
    }
  }
  cout << "reconfiguration is over" << endl;
  /*Coordinate targetBottomLeft = getTargetBottomLeft();
  Coordinate 
  Coordinate trTargetBottomLeft = Map::real2Virtual(*/


  /* Current map:
     int *gridSize = world->getGridSize();
     for (int iy = 0; iy < gridSize[2]; iy++) {
     for (int ix = 0; ix < gridSize[0]; ix++) {
     Catoms2DBlock* c = world->getGridPtr(ix,0,iy);
     if (c != NULL) {
	
     }
     }
     }*/

  /* Target map:
  Coordinate pmin(INT_MAX,INT_MAX);
  Coordinate pmax(INT_MIN,INT_MIN);
  Catoms2DWorld *world = Catoms2DWorld::getWorld();
  int *gridSize = world->getGridSize();
  for (int iy = 0; iy < gridSize[2]; iy++) {
    for (int ix = 0; ix < gridSize[0]; ix++) {
      if (world->getTargetGrid(ix,0,iy) == fullCell ) {
	pmin.x = min(pmin.x, ix);
	pmin.y = min(pmin.y, iy);
	pmax.x = max(pmax.x, ix);
	pmax.y = max(pmax.y, iy);
	Coordinate real(ix,iy);
	Coordinate t =  map.real2Virtual(real);
	cout << "target: (" << t.x << " " << t.y << ")" << endl;
	ctuples.out(ContextTuple(string("target"),t));
      }
    }
  }
  Rectangle bounds(pmin,pmax);
  CTuple::setBounds(bounds);
  */
}
