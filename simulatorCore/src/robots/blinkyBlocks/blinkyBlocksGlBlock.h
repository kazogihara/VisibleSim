/*
 * blinkyBlocksGlBlock.h
 *
 *  Created on: 05 juin 2013
 *      Author: ben
 */

#ifndef BLINKYBLOCKSGLBLOCK_H_
#define BLINKYBLOCKSGLBLOCK_H_
#include <string>
#include "../../gui/objLoader.h"
#include "../../base/glBlock.h"

namespace BlinkyBlocks {
class BlinkyBlocksGlBlock:public GlBlock {
protected :
public :
    uint8_t rotCoef;
    BlinkyBlocksGlBlock(bID id) : GlBlock(id) {};
    virtual ~BlinkyBlocksGlBlock() {};

    virtual void glDraw(ObjLoader::ObjLoader *ptrObj) override;
    void setRotation(short rotCode) { rotCoef=rotCode; };
    virtual string getInfo() override;
};
}
#endif /* BLINKYBLOCKSGLBLOCK_H_ */
