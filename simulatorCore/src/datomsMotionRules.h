#ifndef DATOMSMOTIONRULES_H
#define DATOMSMOTIONRULES_H

#include "lattice.h"
#include "datomsBlock.h"

//!< \namespace Datoms
namespace Datoms {
enum MotionRuleLinkType { hexaFace, octaFace};
class DatomsMotionRulesLink;

class DatomsMotionRulesConnector {
public :
    int ID;
    vector <DatomsMotionRulesLink*> tabLinks;

    DatomsMotionRulesConnector(int n):ID(n) {};
    void addLink(DatomsMotionRulesLink *lnk);
};

class DatomsMotionRulesLink {

    DatomsMotionRulesConnector *conFrom; //!< origin connector
    DatomsMotionRulesConnector *conTo; //!< destination connector
    Vector3D axis1; //!< first rotation axis
    Vector3D axis2; //!< second rotation axis
    vector <int> tabBlockingIDs; //!< array of blocking ID
    MotionRuleLinkType MRLT;
	uint8_t modelId;
public :
    DatomsMotionRulesLink(MotionRuleLinkType m,DatomsMotionRulesConnector *from,DatomsMotionRulesConnector *to,const Vector3D& ax1,const Vector3D& ax2,uint8_t id):
        MRLT(m),conFrom(from),conTo(to),axis1(ax1),axis2(ax2),modelId(id) {};
/**
   \brief Get connector ID of destination of the motion
   \return destination connector ID
**/
    inline int getConToID() { return conTo->ID; };
    inline int isOctaFace() { return MRLT==octaFace; };
/**
   \brief Get string like "X->Y" where X is a caractere representing origin ID ('0..9,A,B') and Y is a caractere representing destination ID
**/
    string getID();
/**
   \brief Add a blocking connector ID to the list
   \param id of the connector that must be free to allow the motion
**/
    void addBlockingConnector(int id);
/**
   \brief Add a blocking connectors into the list
   \param Comma separated string of ids of the connectors that must be free to allow the motion
**/
    void addBlockingConnectorsString(const string &str);
/**
   \brief Return if the rule is valid for a Catoms 3D
   \param The evaluated 3D catom
   \return boolean result
**/
    bool isValid(const DatomsBlock *c3d);
/**
   \brief Get the list of cells that must be free to apply the rule
   \param The evaluated 3D catom, get position and orientation for the rule
   \return vector of Cell3DPosition containing blocking positions in the grid
**/
    vector<Cell3DPosition> getBlockingCellsList(const DatomsBlock *c3d);
/**
   \brief Get the final position of the catom c3D after applying the rule
   \param The evaluated 3D catom
   \return Position in the grid
**/
    Cell3DPosition getFinalPosition(DatomsBlock *c3d);
/**
   \brief Send a rotation event associated to the rule
   \param mobile : mobile catom that will turn
   \param fixed : pivot catom of rotation
   \param t : time of start of rotation
**/
    void sendRotationEvent(DatomsBlock *mobile,DatomsBlock *fixed,double t);
};

/*! \class DatomsMotionRules
    \brief Define the graph of possible motions for a 3D Catom
*/
class DatomsMotionRules {
    DatomsMotionRulesConnector *tabConnectors[12]; //!< array of connector rules
    public:
        DatomsMotionRules();
        virtual ~DatomsMotionRules();
/**
   \brief Returns if c3d catom is able to turn from the orientation fromId to the toId one
   \param c3d: the catom
   \param fromId : initial connector
   \param toId : final connector
   \return true if c3d catom is able to turn from the orientation fromId to the toId one */
        bool isValide(const DatomsBlock& c3d,int fromId, int toId);
/**
   \brief Get the list of valid motion rules from a connector for c3D catom
   \param c3d: the catom
   \param fromId : initial connector
   \param vec : vector of valid motion rules
   \return return if c3d catom is able to turn from the orientation fromId to the toId one */
        bool getValidMotionList(const DatomsBlock* c3d,int from,vector<DatomsMotionRulesLink*>&vec);
        bool getValidMotionListFromPivot(const DatomsBlock* pivot,int from,vector<DatomsMotionRulesLink*>&vec,const FCCLattice2 *lattice,const Target *target);

    protected:
    private:
        void addLinks4(int id1, int id2, int id3, int id4,const Vector3D &left,const Vector3D &lup,const Vector3D &rup,uint8_t modelId);
        void addLink(MotionRuleLinkType mrlt,int id1, int id2,const Vector3D &axis1,const Vector3D &axis2,int n,int *tabBC,uint8_t modelId);
};

} // Datoms namespace
#endif // DATOMSMOTIONRULES_H