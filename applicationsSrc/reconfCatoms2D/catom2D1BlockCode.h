/*
 * catom2D1BlockCode.h
 *
 *  Created on: 12 avril 2013
 *      Author: ben
 */

#ifndef CATOM2D1BLOCKCODE_H_
#define CATOM2D1BLOCKCODE_H_

#include "catoms2DBlockCode.h"
#include "catoms2DSimulator.h"
#include "catoms2DScheduler.h"
#include "catoms2DBlock.h"
#include "localTupleSpace.hpp"
#include "tuple.hpp"
#include "contextTuple.hpp"

class Catoms2D1BlockCode : public Catoms2D::Catoms2DBlockCode {
   
public:

	Catoms2D::Catoms2DScheduler *scheduler;
	Catoms2D::Catoms2DBlock *catom2D;

	Catoms2D1BlockCode (Catoms2D::Catoms2DBlock *host);
	~Catoms2D1BlockCode ();
	Coordinate position;
	LocalTupleSpace localTuples;

	void startup();
	void processLocalEvent(EventPtr pev);
	
	void startMotion(int direction, Catoms2D::Catoms2DBlock *pivot);
	
	void updateBorder();
	bool canMove();

	void out(ContextTuple *t);
	Coordinate getPosition(P2PNetworkInterface *recv_it, Coordinate c);

	static Catoms2D::Catoms2DBlockCode *buildNewBlockCode(Catoms2D::Catoms2DBlock *host);
};

#endif /* CATOM2DBLOCKCODE_H_ */
