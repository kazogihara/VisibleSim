#include "datomsMotionRules.h"
#include "deformationEvents.h"

using namespace std;
using namespace BaseSimulator::utils;

//! \namespace Datoms
namespace Datoms {

const int tabConnectors4[6][4] = { {0,1,2,10},{1,3,6,11},{4,6,7,8},{0,5,7,9},{8,9,10,11},{2,3,4,5}};

DatomsMotionRules::DatomsMotionRules() {

// allocation of connectors
    for (int i=0; i<12; i++) {
        tabConnectors[i] = new DatomsMotionRulesConnector(i);
    }

    Vector3D up(0,+1,0),upleft(+1,+1,+M_SQRT2),upright(-1,+1,-M_SQRT2),
             down(0,-1,0),downright(+1,-1,-M_SQRT2),downleft(-1,-1,+M_SQRT2),left(0,0,1),right(0,0,-1),
             lup(-1,+1,0),rup(+1,+1,0);
    addLinks4(0,2,1,10,left,lup,rup,2);
    addLinks4(6,4,7,8,left,lup,rup,4);
    addLinks4(1,3,6,11,right,-rup,-lup,3);
    addLinks4(7,5,0,9,right,-rup,-lup,5);
    addLinks4(2,5,4,3,left,lup,rup,6);
    addLinks4(8,9,10,11,right,-rup,-lup,7);
}

DatomsMotionRules::~DatomsMotionRules() {
    for (int i=0; i<12; i++) {
        delete tabConnectors[i];
    }
}

string DatomsMotionRulesLink::getID() {
    string c="X->X";
    uint8_t id1 = conFrom->ID;
    uint8_t id2 = conTo->ID;
    char ch = (id1<10?'0'+id1:'A'+id1-10);
    c[0]=ch;
    ch = (id2<10?'0'+id2:'A'+id2-10);
    c[3]=ch;
    return c;
}

const int *findTab4(int id1,int id2) {
    int i=0,j;
    bool id1found=false,id2found=false;
    while (i<6 && !(id1found && id2found)) {
        id1found=false;
        id2found=false;
        for (j=0; j<4; j++) {
            if (id1==tabConnectors4[i][j]) id1found=true;
            if (id2==tabConnectors4[i][j]) id2found=true;
        }
        i++;
    }
    assert(id1found && id2found);
    return tabConnectors4[i-1];
}

void DatomsMotionRules::addLinks4(int id1, int id2, int id3, int id4, const Vector3D &left,const Vector3D &lup,const Vector3D &rup,uint8_t modelId) {
    int tabBC1[4],tabBC2[4],tabBC3[4],tabBC4[4];

	tabBC1[0]=id2;	tabBC1[1]=id3; 	tabBC1[2]=id4; tabBC1[3] = (id1+6)%12;
	tabBC2[0]=id1;	tabBC2[1]=id3; 	tabBC2[2]=id4; tabBC2[3] = (id2+6)%12;
	tabBC3[0]=id2;	tabBC3[1]=id1; 	tabBC3[2]=id4; tabBC3[3] = (id3+6)%12;
	tabBC4[0]=id2;	tabBC4[1]=id3; 	tabBC4[2]=id1; tabBC4[3] = (id4+6)%12;

/* 1->2 & 2->1*/
    addLink(octaFace,id1,id2,-left,rup,4,tabBC1,modelId);
    addLink(octaFace,id2,id1,left,lup,4,tabBC2,modelId);
/* 1->3 & 3->1*/
    addLink(octaFace,id1,id3,-left,-left,4,tabBC1,modelId);
    addLink(octaFace,id3,id1,-left,-left,4,tabBC3,modelId);
/* 1->4 & 4->1*/
    addLink(octaFace,id1,id4,-left,-rup,4,tabBC1,modelId);
    addLink(octaFace,id4,id1,left,-lup,4,tabBC4,modelId);
/* 2->3 & 3->2*/
    addLink(octaFace,id2,id3,left,-lup,4,tabBC2,modelId);
    addLink(octaFace,id3,id2,-left,-rup,4,tabBC3,modelId);
/* 2->4 & 4->2*/
    addLink(octaFace,id2,id4,left,left,4,tabBC2,modelId);
    addLink(octaFace,id4,id2,left,left,4,tabBC4,modelId);
/* 3->4 & 4->3*/
    addLink(octaFace,id3,id4,-left,rup,4,tabBC3,modelId);
    addLink(octaFace,id4,id3,left,lup,4,tabBC4,modelId);
}


void DatomsMotionRules::addLink(MotionRuleLinkType mrlt,int id1, int id2,const Vector3D &axis1,const Vector3D &axis2,int n, int *tabBC,uint8_t modelId) {
    Matrix M = DatomsBlock::getMatrixFromPositionAndOrientation(Cell3DPosition(0,0,0),id1);
    Matrix M_1;
    M.inverse(M_1);
    Vector3D ax1 = M_1*axis1;
    Vector3D ax2 = M_1*axis2;
    ax1.normer_interne();
    ax2.normer_interne();

    DatomsMotionRulesLink *lnk = new DatomsMotionRulesLink(mrlt,tabConnectors[id1],tabConnectors[id2],ax1,ax2,modelId);
    tabConnectors[id1]->addLink(lnk);
    //OUTPUT << id1 << " -> " << id2 << endl;
    for (int i=0; i<n; i++) {
        if (tabBC[i]!=id2) {
            lnk->addBlockingConnector(tabBC[i]);
            //OUTPUT << tabBC[i] << " ";
        }
    }
    //OUTPUT << endl;
}

bool DatomsMotionRules::getValidMotionList(const DatomsBlock* c3d,int from,vector<DatomsMotionRulesLink*>&vec) {
    DatomsMotionRulesConnector *conn = tabConnectors[from];
    bool notEmpty=false;

	vector <DatomsMotionRulesLink*>::const_iterator ci=conn->tabLinks.begin();
    while (ci!=conn->tabLinks.end()) {
        OUTPUT << from << " -> " << (*ci)->getConToID() << ", " << endl;
        if ((*ci)->isValid(c3d)) {
            vec.push_back(*ci);
            notEmpty=true;
        }
        ci++;
    }
    return notEmpty;
}

bool DatomsMotionRules::getValidMotionListFromPivot(const DatomsBlock* pivot,int from,vector<DatomsMotionRulesLink*>&vec,const FCCLattice2 *lattice,const Target *target) {
    DatomsMotionRulesConnector *conn = tabConnectors[from];
    bool notEmpty=false;
    bool isOk;
    Cell3DPosition pos,pos2;

    // source must be free or is not in goal
    pivot->getNeighborPos(from,pos);
//OUTPUT << "FROM pos=" << pos << endl;
    if (lattice->cellHasBlock(pos) && target->isInTarget(pos)) return false;
    pos2 = 2*pos + pos - pivot->position;
//OUTPUT << "OPP FROM pos=" << pos2 << endl;
    if (lattice->cellHasBlock(pos2)) return false;

    vector <DatomsMotionRulesLink*>::const_iterator ci=conn->tabLinks.begin();
    while (ci!=conn->tabLinks.end()) {
        int to = (*ci)->getConToID();
        //OUTPUT << from << " -> " << to << ", ";
        // destination must be in lattice and free
        pivot->getNeighborPos(to,pos);
//OUTPUT << "TO pos=" << pos << endl;
        isOk = lattice->isInGrid(pos) && !lattice->cellHasBlock(pos);
        // opposite cell must be free
        pos2 = 2*pos - pivot->position;
//OUTPUT << "OPP TO pos=" << pos2 << endl;
        isOk = isOk && !lattice->cellHasBlock(pos2);

        // list of cells that must be free
        if ((*ci)->isOctaFace()) {
            const int *ptr = findTab4(from,to);
            int i=0;
            while (isOk && i<4) {
                if (ptr[i]!=from && ptr[i]!=to) {
                    pivot->getNeighborPos(ptr[i],pos);
                    isOk = !lattice->cellHasBlock(pos);
                }
                i++;
            }
        } 
        if (isOk) {
            vec.push_back(*ci);
            //OUTPUT << "ADD " << from << " -> " << to << endl;
            notEmpty=true;
        }
        ci++;
    }
    return notEmpty;
}


void DatomsMotionRulesConnector::addLink(DatomsMotionRulesLink *lnk) {
    tabLinks.push_back(lnk);
}

void DatomsMotionRulesLink::addBlockingConnectorsString(const string &str) {
    int n;
    std::size_t found=str.find_first_of(","),prev=0;
    while (found!=std::string::npos) {
        n = stoi(str.substr(prev,found-prev).c_str());
        tabBlockingIDs.push_back(n);
        prev = found+1;
        found=str.find_first_of(",",prev);
    }
    n = stoi(str.substr(prev).c_str());
    tabBlockingIDs.push_back(n);

    // add dest
    tabBlockingIDs.push_back(conTo->ID);
}

void DatomsMotionRulesLink::addBlockingConnector(int n) {
    tabBlockingIDs.push_back(n);
}

bool DatomsMotionRulesLink::isValid(const DatomsBlock *c3d) {
    vector<int>::const_iterator ci = tabBlockingIDs.begin();
// final position (connector) must be free
    if (c3d->getInterface(conTo->ID)->connectedInterface!=NULL) return false;
// all blocking connectors must be free
    while (ci!=tabBlockingIDs.end() && c3d->getInterface(*ci)->connectedInterface==NULL) {
        ci++;
    }
    if (ci!=tabBlockingIDs.end()) OUTPUT << "blocking: "<< *ci << endl;
    return (ci==tabBlockingIDs.end());
}

Cell3DPosition DatomsMotionRulesLink::getFinalPosition(DatomsBlock *c3d) {
    Cell3DPosition finalPosition;
    short orient;
    DatomsBlock *neighbor = (DatomsBlock *)c3d->getInterface(conFrom->ID)->connectedInterface->hostBlock;
    Deformation deformation(c3d,neighbor,axis1,axis2,modelId);
    deformation.init(((DatomsGlBlock*)c3d->ptrGlBlock)->mat);
    deformation.getFinalPositionAndOrientation(finalPosition,orient);
//    OUTPUT << "deformation " << c3d->blockId << ":" << conFrom->ID << " et " << neighbor->blockId << endl;
    return finalPosition;
}

void DatomsMotionRulesLink::sendRotationEvent(DatomsBlock*mobile,DatomsBlock*fixed,double t) {
    Deformation deformation(mobile,fixed,axis1,axis2,modelId);
    getScheduler()->schedule(new DeformationStartEvent(t,mobile,deformation));
}

vector<Cell3DPosition> DatomsMotionRulesLink::getBlockingCellsList(const DatomsBlock *c3d) {
    vector<int>::const_iterator ci = tabBlockingIDs.begin();
    vector<Cell3DPosition> tabPos;
    Cell3DPosition pos;

    //OUTPUT << "getBlockingCellsList ";
    while (ci!=tabBlockingIDs.end()) {
        if (c3d->getNeighborPos(*ci,pos)) {
            //OUTPUT << *ci << ":" << pos << ", ";
            tabPos.push_back(pos);
        }
        ci++;
    }
    //OUTPUT << endl;
    return tabPos;
}

} // Datoms namespace