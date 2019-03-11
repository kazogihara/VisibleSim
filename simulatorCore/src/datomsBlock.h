/*!
 * \file datomsBlock.h
 * \brief deformable atoms Block
 * \date 28/01/2018
 * \author Benoît Piranda
 */


#ifndef DATOMSBLOCK_H_
#define DATOMSBLOCK_H_

#include <stdexcept>
#include <bitset>

#include "buildingBlock.h"
#include "datomsBlockCode.h"
#include "datomsGlBlock.h"
#include "cell3DPosition.h"
//#include "datomsCapabilities.h"
#include "utils.h"

using namespace BaseSimulator::utils;

//! @namespace Datoms
namespace Datoms {

/**
   @brief list of rotations around x,y,z axis applied to the initial catom to obtain orientation of indice orientationCode

   tabOrientationAngles[oc] : rotations done to transform initial catom to oc oriented catom

*/
const float tabOrientationAngles[12][3] = { {0,0,0}, {180.0f,0.0f,-90.0f}, {-90.0f,45.0f,-45.0f},
                                            {90.0f,45.0f,-135.0f}, {-90.0f,45.0f,135.0f}, {90.0f,45.0f,45.0f},
                                            {0,0,180.0f}, {180.0f,0,90.0f}, {90.0f,-45.0f,135.0f},
                                            {-90.0f,-45.0f,45.0f}, {90.0f,-45.0f,-45.0f}, {-90.0f,-45.0f,-135.0f} };

/**
   @brief list of connector positions (x,y,z) in catom local coordinates

   tabConnectorPositions[i] : coordinates of connector #i

*/
const float tabConnectorPositions[12][3] = { {1,0,0}, {0,1,0}, {0.5,0.5,M_SQRT2_2},
                                             {-0.5,0.5,M_SQRT2_2},{-0.5,-0.5,M_SQRT2_2},{0.5,-0.5,M_SQRT2_2},
                                             {-1,0,0}, {0,-1,0}, {-0.5,-0.5,-M_SQRT2_2},
                                             {0.5,-0.5,-M_SQRT2_2},{0.5,0.5,-M_SQRT2_2},{-0.5,0.5,-M_SQRT2_2}};

class DatomsBlockCode;

enum DeformationLinkType { HexaFace, OctaFace, Any, None }; //!< Kind of face to be used for a rotation. Any refers to no preference and None is used as a return statement to indicate an absence of possible link
enum PistonId { AllPistonsOff=1, Piston012A, Piston136B, Piston4678, Piston0579, Piston2345, Piston89AB }; //!< ID of piston for deformation

/*! @class DatomsBlock
  @brief Special treatement and data for 3D quasi-spherical robot
*/
class DatomsBlock : public BaseSimulator::BuildingBlock {
public :
    short orientationCode; //!< number of the connector that is along the x axis.
    short compressedPiston; // -1 for none
public:
    /**
       @brief Constructor
       @param bId: id of the block
       @param bcd : code block function
    */
    DatomsBlock(int bId, BlockCodeBuilder bcb);
    ~DatomsBlock();

    inline virtual DatomsGlBlock* getGlBlock() const { 
		return static_cast<DatomsGlBlock*>(ptrGlBlock); 
	};
	
    inline void setGlBlock(DatomsGlBlock*ptr) { ptrGlBlock=ptr;};
    /**
       @brief Show/Hide a catom in the interface
       @param visible: new state of the catom. */
    void setVisible(bool visible);
    /**
       @brief Get the interface from the neighbor position in the grid
       @param pos: position of the cell (if in the grid)
       @return return interface if it exists one connected, NULL otherwise */
    P2PNetworkInterface *getInterface(const Cell3DPosition &pos) const;
    /**
       @brief Get the interface from the interface id
       @param id: interface number
       @return return interface if it exists one connected, NULL otherwise */
    inline P2PNetworkInterface *getInterface(int id) const { return P2PNetworkInterfaces[id]; };
    /**
       @brief Get the position of the gridcell in the direction of the given connector
       @param connectorId: id of connector (0..11)
       @param pos: position of the cell (if in the grid)
       @return return true if the cell is in the grid, false otherwise. */
    bool getNeighborPos(short connectorId,Cell3DPosition &pos) const;

     /** 
     * Gets a pointer to direct neighbor on connector conId
     * @param connectorId connector identifier
     * @return a pointer to neighbor on connector conId of datom, or NULL if there is none
     */
    DatomsBlock *getNeighborBlock(short conId) const;

    /** 
     * Gets a pointer to neighbor (direct neighborhood or distant) at cell nPos
     * @param nPos neighbor cell
     * @attention this function does not check whether nPos is in the local neighborhood of the catom. This is a feature since a neighbor in the second- or nth-neighborhood of the datom could be asked. 
	 * Prefer getNeighborBlock(short conId) for local neighborhood.
     * @return a pointer to neighbor at ABSOLUTE position nPos
     */
    DatomsBlock *getNeighborBlock(const Cell3DPosition& nPos) const;
    
   /** 
     * @brief Given a neighbor at position nPos and a lattice absolute direction (connector Id if neighbor had con0 aligned with x-axis) from that position, deduce what connector of the current module is adjacent to cell on direction nDirection of nPos.
     * @param nPos 
     * @param nDirection 
     * @param includeSelf if true, returns connector if cell on nDirection of nPos is own cell
     * @return connector of the current module is adjacent to cell on direction nDirection of nPos, or -1 if that cell and the current module are not adjacent */
    short projectAbsoluteNeighborDirection(const Cell3DPosition& nPos, short nDirection) const;

    /**
       @brief Get the connector of the catom that is next to the position in argument
       @param pos: position of the cell (must be in the grid)
       @return returns the id of the connector next to this pos or -1 if invalid. */
    short getConnectorId(const Cell3DPosition& pos) const;

    /** 
     * @attention SHOULD BE getConnector(p2p...)!
     */
    int getDirection(P2PNetworkInterface*) const;

    /** 
     * @brief For a given connector, returns the direction corresponding to this connector 
     *  relative to a catom whose 0 connector is aligned with the x-axis
     * @param connector the connector for which the absolute direction wants to be known
     * @return a short int in {0..11}, the direction of the connector, or -1 if connector is invalid or adjacent to a cell outside the lattice */
    short getAbsoluteDirection(short connector) const;
    short getAbsoluteDirection(const Cell3DPosition& pos) const;

    /**
     * @brief Sets the grid position of the catom, and updates its position matrix
     *
     * @param p :  the grid position (x,y,z) of the block as a Cell3DPosition
     */
    void setPosition(const Cell3DPosition &p);

    /**
       @brief Get the orientation code from the transformation matrix of the catom
       @param mat: homogeneous transformation matrix
       @return return orientation code. */
    static short getOrientationFromMatrix(const Matrix &mat);
    /**
       @brief Get the transformation matrix of the catom from its position in the grid and its orientation code
       @param pos: position of the cell constaining the catom
       @param code: orientation code (number of the connector aligned with x axis)
       @return return homogeneous transformation matrix. */
    static Matrix getMatrixFromPositionAndOrientation(const Cell3DPosition &pos,short code);
    /**
       @brief Set the catom in the grid according to a cell position and an orientation code
       @param pos: position of the cell constaining the catom
       @param code: orientation code (number of the connector aligned with x axis)*/
    void setPositionAndOrientation(const Cell3DPosition &pos,short code);

    /** 
     * @param otherOriCode Some other catom's orientation code
     * @return true if both catoms' orientation are inverted relative to each other
     */

	bool areOrientationsInverted(short otherOriCode) const;
	/** 
     * @brief If position in argument is adjacent to current module, returns a pointer to the neighbor in that cell
     * @param pos position of the neighbor to retrieve
     * @return a pointer to the neighbor on cell pos or NULL if pos is not adjacent to the module or out of lattice
    **/
    DatomsBlock* getNeighborOnCell(const Cell3DPosition &pos) const;
    
	/** 
     * Indicates whether the module is can reach position pos in a single rotation,
     *  given its current local neighborhood
     * @param pos target position
     * @param faceReq face requirement, if indicated the function will return true only if the 
     *  motion is possible using the type of face passed as argument. Any by default.
     * @attention this does not guarantee an absence of collision, as might occur if 
     *  a blocking modules exists in the 2nd-order neighborhood of the current block
     * @return true if motion is possible, false otherwise
     */
    bool canRotateToPosition(const Cell3DPosition &pos,
                             DeformationLinkType  faceReq = DeformationLinkType ::Any) const;

    /** 
     * Queries each of the module interface to determine the state of the local neighborhood.
     *  i.e., for each connector, if one module is connected or not
     * @return a 12-bit bitset where the n-th bit is set to true if interface in direction n 
     *  is connected to another module, 0 otherwise
     * @attention a P2PNetworkInterface at DIRECTION d = m corresponds to the interface that 
     *  is located at the same location as interface #m when block is at orientation 0
     */
    std::bitset<12> getLocalNeighborhoodState() const;
    
	
    // MeldInterpreter
    /**
     * @copydoc BuildingBlock::addNeighbor
     */
    virtual void addNeighbor(P2PNetworkInterface *ni, BuildingBlock* target);
    /**
     * @copydoc BuildingBlock::removeNeighbor
     */
    virtual void removeNeighbor(P2PNetworkInterface *ni);

};

std::ostream& operator<<(std::ostream &stream, DatomsBlock const& bb);

}

#endif /* DATOMSBLOCK_H_ */
