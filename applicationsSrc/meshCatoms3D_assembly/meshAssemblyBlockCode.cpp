/**
 * @file   meshAssemblyBlockCode.cpp
 * @author pthalamy <pthalamy@p3520-pthalamy-linux>
 * @date   Mon Oct  1 10:42:29 2018
 *
 * @brief
 *
 *
 */

#include <iostream>
#include <set>

#include "catoms3DWorld.h"
#include "scheduler.h"
#include "events.h"
#include "trace.h"
#include "tDefs.h"

#include "teleportationEvents.h"
#include "rotation3DEvents.h"
#include "catoms3DMotionEngine.h"

#include "meshAssemblyBlockCode.hpp"

using namespace Catoms3D;
using namespace MeshCoating;

Time MeshAssemblyBlockCode::t0 = 0;
int MeshAssemblyBlockCode::nbCatomsInPlace = 0;
int MeshAssemblyBlockCode::nbMessages = 0;
bool MeshAssemblyBlockCode::sandboxInitialized = false;
uint MeshAssemblyBlockCode::X_MAX;
uint MeshAssemblyBlockCode::Y_MAX;
uint MeshAssemblyBlockCode::Z_MAX;
constexpr std::array<Cell3DPosition, 6> MeshAssemblyBlockCode::incidentTipRelativePos;
constexpr std::array<Cell3DPosition, 12> MeshAssemblyBlockCode::entryPointRelativePos;
constexpr Cell3DPosition MeshAssemblyBlockCode::meshSeedPosition;

MeshAssemblyBlockCode::MeshAssemblyBlockCode(Catoms3DBlock *host):
    Catoms3DBlockCode(host) {
    scheduler = getScheduler();
    world = BaseSimulator::getWorld();
    lattice = world->lattice;
    catom = host;

    const Cell3DPosition& ub = lattice->getGridUpperBounds();
    // Round down mesh dimensions to previous multiple of B
    // TODO: Adapt to CSG
    X_MAX = ub[0] - (B - ub[0] % B);
    Y_MAX = ub[1] - (B - ub[1] % B);
    Z_MAX = ub[2] - (B - ub[2] % B);
    ruleMatcher = new MeshRuleMatcher(X_MAX, Y_MAX, Z_MAX, B);
}


MeshAssemblyBlockCode::~MeshAssemblyBlockCode() {
    if (ruleMatcher->isInMesh(norm(catom->position))) {
        OUTPUT << "bitrate:\t" << catom->blockId << "\t"
               << maxBitrate.first << "\t"
               << (maxBitrate.second.empty() ?
                   ruleMatcher->roleToString(role) : maxBitrate.second) << endl;
    }
}

void MeshAssemblyBlockCode::onBlockSelected() {
    // Debug:
    // (1) Print details of branch growth plan and previous round
    if (role == Coordinator) {
        cout << "Growth Plan: [ ";
        for (int i = 0; i < 6; i++)
            cout << catomsReqByBranch[i] << ", ";
        cout << " ]" << endl;

        cout << "Construction Queue: [ " << endl;
        cout << "|   Component   |   EPL  |" << endl << endl;
        for (const auto& pair : constructionQueue) {
            cout << "\t{ " << ruleMatcher->component_to_string(pair.first) << ", "
                 << ruleMatcher->component_to_string(pair.second) << " }" << endl;
        }
        cout << "]" << endl;
    }

    cout << "branch: " << branch << endl;
    cout << "coordinatorPos: " << coordinatorPos << endl;
    cout << "nearestCoordinatorPos: " << denorm(ruleMatcher->getNearestTileRootPosition(norm(catom->position))) << endl;
    cout << "role: " << MeshRuleMatcher::roleToString(role) << endl;
    cout << "localNeighborhood: " << catom->getLocalNeighborhoodState() << endl;    
    Cell3DPosition nextHop;
    matchLocalRules(catom->getLocalNeighborhoodState(), catom->position,
                    targetPosition, coordinatorPos, step, nextHop);
    cout << "nextHop: " << getTileRelativePosition() << " -> " << nextHop << endl;
    cout << "isInMesh: " << ruleMatcher->isInMesh(norm(catom->position)) << endl;
    cout << "isInMeshOrSandbox: "<<ruleMatcher->isInMeshOrSandbox(norm(catom->position)) <<endl;
    cout << "targetPos: " << targetPosition << endl;
    cout << "matchingLocalRule: " << matchingLocalRule << endl;
    cout << "greenLightIsOn: " << greenLightIsOn << endl;
    cout << "pivotPosition: " << pivotPosition << endl;
}

void MeshAssemblyBlockCode::startup() {
    stringstream info;
    info << "Starting ";
    initialized = true;    
    startTime = scheduler->now();

    if (not sandboxInitialized)
        initializeSandbox();

    coordinatorPos =
        denorm(ruleMatcher->getNearestTileRootPosition(norm(catom->position)));

    // Do stuff
    if (catom->position == (getEntryPointForMeshComponent(MeshComponent::R)
                            // CONTINUOUS FEEDING
                            + Cell3DPosition(0, 1, 0))
        and coordinatorPos[2] == meshSeedPosition[2]
        and lattice->isFree(coordinatorPos)) {
        // Catom is one of the future ground tile roots waiting on RZ_EPL
        role = FreeAgent;

        if (coordinatorPos == meshSeedPosition) {
            targetPosition = coordinatorPos;

            // Delay the algorithm start
            getScheduler()->schedule(
                new InterruptionEvent(getScheduler()->now() +
                                      (getRoundDuration()),
                                      catom, IT_MODE_ALGORITHM_START));
        }

        // others are waiting for horizontal branches leading to their tile to be completed
        return;

    } else if (ruleMatcher->isVerticalBranchTip(norm(catom->position))) {
        // Catom is one of the sandbox catoms here for decoration but also for actuating
        //  for incoming modules
        role = ActiveBeamTip; // nothing to be done, wait for tPos requests

        // Add z = B to ensure that level -1 catoms are handled properly
        short bi = ruleMatcher->determineBranchForPosition(
            norm(catom->position[2] < meshSeedPosition[2] ?
                 catom->position + Cell3DPosition(0,0,B) : catom->position));
        VS_ASSERT_MSG(bi >= 0 and bi < N_BRANCHES, "cannot determine branch.");
        branch = static_cast<BranchIndex>(bi);
    } else if (meshSeedPosition[2] - catom->position[2] > 1) {
        role = PassiveBeam; // nothing to be done here for visual decoration only

        // Add z = B to ensure that level -1 catoms are handled properly
        short bi = ruleMatcher->getBranchIndexForNonRootPosition(norm(catom->position)
                                                                 + Cell3DPosition(0, 0, B));
        VS_ASSERT_MSG(bi >= 0 and bi < N_BRANCHES, "cannot determine branch.");
        branch = static_cast<BranchIndex>(bi);
    } else {
        // Catom is active module summoned from the sandbox and that will be used for scaffolding
        role = FreeAgent;
        requestTargetCellFromTileRoot();
        // if (not requestTargetCellFromTileRoot() )
        // VS_ASSERT_MSG(false, "meshAssembly: spawned module cannot be without a delegate coordinator in its vicinity.");
    }
}

const Cell3DPosition
MeshAssemblyBlockCode::norm(const Cell3DPosition& pos) {
    return pos - meshSeedPosition;
}

const Cell3DPosition
MeshAssemblyBlockCode::sbnorm(const Cell3DPosition& pos) {
    Cell3DPosition meshPos = norm(pos) + Cell3DPosition(0,0,B);
    Cell3DPosition fixPos;
    fixPos.pt[0] = (meshPos[0] >= (int)X_MAX ? meshPos[0] - B : meshPos[0]);
    fixPos.pt[1] = (meshPos[1] >= (int)Y_MAX ? meshPos[1] - B : meshPos[1]);
    fixPos.pt[2] = (meshPos[2] >= (int)Z_MAX ? meshPos[2] - B : meshPos[2]);
    
    return fixPos;
}

const Cell3DPosition
MeshAssemblyBlockCode::relatify(const Cell3DPosition& pos) {
    return pos - coordinatorPos;
}

const Cell3DPosition
MeshAssemblyBlockCode::denorm(const Cell3DPosition& pos) {
    return pos + meshSeedPosition;
}

const Cell3DPosition
MeshAssemblyBlockCode::derelatify(const Cell3DPosition& pos) {
    return pos + coordinatorPos;
}

void MeshAssemblyBlockCode::processReceivedMessage(MessagePtr msg,
                                                   P2PNetworkInterface *sender) {
    stringstream info;

    switch (msg->type) {
        // ALL MOVED TO HANDLEABLE MESSAGES
        default:
            cout << "Unknown Generic Message Type" << endl;
            assert(false);
            break;
    }

}

void MeshAssemblyBlockCode::processLocalEvent(EventPtr pev) {
    MessagePtr message;
    stringstream info;

    switch (pev->eventType) {
        case EVENT_RECEIVE_MESSAGE: {
            message =
                (std::static_pointer_cast<NetworkInterfaceReceiveEvent>(pev))->message;

            if (message->isMessageHandleable()) {
                std::shared_ptr<HandleableMessage> hMsg =
                    (std::static_pointer_cast<HandleableMessage>(message));

                console << " received " << hMsg->getName() << " from "
                        << message->sourceInterface->hostBlock->blockId
                        << " at " << getScheduler()->now() << "\n";
                hMsg->handle(this);
            } else {
                P2PNetworkInterface * recv_interface = message->destinationInterface;

                // Handover to global message handler
                processReceivedMessage(message, recv_interface);
            }
        } break;

        case EVENT_ADD_NEIGHBOR: {            
            if (role != FreeAgent) {                
                if (not rotating) {
                    uint64_t face = Catoms3DWorld::getWorld()->lattice->getOppositeDirection((std::static_pointer_cast<AddNeighborEvent>(pev))->face);
                    const Cell3DPosition& pos = catom->getNeighborBlock(face)->position;

                    if (not ruleMatcher->isInMeshOrSandbox(norm(pos))) {
                        // Neighbor is module in terminal position 
                        greenLightIsOn = false;
                        catom->setColor(RED);
                    } else {
                        // Module has taken position and is now pivot
                        setGreenLightAndResumeFlow();
                    }
                }
                
            } else if (role == FreeAgent and matchingLocalRule) {
                matchRulesAndProbeGreenLight();
            } 
            break;
        }

        case EVENT_REMOVE_NEIGHBOR: {
            if (not rotating) {
                if (role != FreeAgent) {
                    uint64_t face = Catoms3DWorld::getWorld()->lattice->getOppositeDirection((std::static_pointer_cast<RemoveNeighborEvent>(pev))->face);

                    Cell3DPosition pos;
                    if (catom->getNeighborPos(face, pos)
                        // catom not actuating
                        and (catom->getState() == BuildingBlock::State::ALIVE)) { 
                        setGreenLightAndResumeFlow();
                    }
                } else if (matchingLocalRule) {
                    matchRulesAndProbeGreenLight();
                }
            }
            
            break;
        }
        case EVENT_PIVOT_ACTUATION_START: {
            std::shared_ptr<PivotActuationStartEvent> pase = std::static_pointer_cast
                <PivotActuationStartEvent>(pev);

            // A free agent module should never be used as pivot (for now)
            VS_ASSERT(role != FreeAgent);
            
            if (greenLightIsOn) {
                // cout << *catom << endl;
                // VS_ASSERT(false); // FIXME: should never happen, light should be red already
                greenLightIsOn = false;
                catom->setColor(RED);
            }

            catom->getNeighborPos(pase->toConP, actuationTargetPos);

            // console << " started actuating for module #" << pase->mobile->blockId << "\n";
        } break;

        case EVENT_PIVOT_ACTUATION_END: {
            // std::shared_ptr<PivotActuationEndEvent> paee = std::static_pointer_cast
            //     <PivotActuationEndEvent>(pev);

            // console << " finished actuating for module #" << paee->mobile->blockId << "\n";
        } break;

        case EVENT_ROTATION3D_START:
            VS_ASSERT(catom->pivot);
            pivotPosition = catom->pivot->position;
            break;
        case EVENT_ROTATION3D_END: {
            // console << "Rotation to " << catom->position << " over" << "\n";
            rotating = false;
            step++;
            if (catom->position == targetPosition and not isOnEntryPoint(catom->position)) {
                role = ruleMatcher->getRoleForPosition(norm(catom->position));
                catom->setColor(ruleMatcher->getColorForPosition(norm(catom->position)));

                greenLightIsOn = true;
                catom->setColor(GREEN);
                moduleAwaitingGo = false;

                // Inform pivot that motion sequence is over and that it can turn green
                P2PNetworkInterface* pivotItf = catom->getInterface(pivotPosition);
                cout << *catom << " pp " << pivotPosition << endl;
                VS_ASSERT(pivotItf and pivotItf->isConnected());
                sendMessage(new FinalTargetReachedMessage(catom->position),
                            pivotItf, MSG_DELAY_MC, 0);

                // STAT EXPORT
                OUTPUT << "nbCatomsInPlace:\t" << (int)round(scheduler->now() / getRoundDuration()) << "\t" << ++nbCatomsInPlace << endl;

                if (ruleMatcher->isVerticalBranchTip(norm(catom->position))) {
                    coordinatorPos =
                        denorm(ruleMatcher->getNearestTileRootPosition(norm(catom->position)));
                    return;
                }

                if (ruleMatcher->isTileRoot(norm(catom->position)))
                    initializeTileRoot();
                else if (ruleMatcher->isTileSupport(norm(catom->position)))
                    initializeSupportModule();
                else {
                    BranchIndex bi =
                        ruleMatcher->getBranchIndexForNonRootPosition(norm(targetPosition));
                    const Cell3DPosition& nextPosAlongBranch =
                        catom->position + ruleMatcher->getBranchUnitOffset(bi);

                    // Coordinate to let the last arrived branch continue the construction
                    if (ruleMatcher->isTileRoot(norm(nextPosAlongBranch))
                        and incidentBranchesToRootAreComplete(nextPosAlongBranch)) {
                        lattice->highlightCell(nextPosAlongBranch, BLUE);
                        cout << "Some branches are missing around "
                             << nextPosAlongBranch << endl;

                        // Notify future tile root as position should be filled
                        P2PNetworkInterface* zBranchTipItf = NULL;
                        for (const auto& pos : lattice->
                                 getActiveNeighborCells(catom->position)) {
                            if (ruleMatcher->isOnZBranch(norm(pos))) {
                                zBranchTipItf = catom->getInterface(pos);
                                break;
                            }
                        }

                        VS_ASSERT(zBranchTipItf);
                        sendMessage(new TileInsertionReadyMessage(),
                                    zBranchTipItf, MSG_DELAY_MC, 0);
                        log_send_message();
                    }
                }
            } else {
                if (catom->position == targetPosition and isOnEntryPoint(catom->position)) {
                    // Update tile belonging
                    coordinatorPos = denorm(ruleMatcher->
                                            getNearestTileRootPosition(norm(catom->position)));

                    // Reset step counter for matching a new ruleset
                    step = 1;

                    // Check that that new tile root is in place and if absent,
                    //  wait for above layer XY modules to notify completion of previous tiles
                    if (lattice->isFree(coordinatorPos)) {
                        if (ruleMatcher->isOnXPyramidBorder(norm(coordinatorPos))
                            and ruleMatcher->isOnYPyramidBorder(norm(coordinatorPos)))
                            targetPosition = coordinatorPos;
                        else {
                            catom->setColor(WHITE);
                            lattice->highlightCell(coordinatorPos, WHITE);
                            return;
                        }
                    } else {
                        // Otherwise ask it for a new targetPosition
                        catom->setColor(BLACK);
                        if (not requestTargetCellFromTileRoot()) {
                            catom->setColor(BLACK);
                            VS_ASSERT_MSG(false, "arriving module cannot be without delegate coordinator in its vicinity.");
                        }

                        return;
                    }
                }

                matchRulesAndProbeGreenLight();
            }
        } break;

        case EVENT_TAP: {
        } break;

        case EVENT_INTERRUPTION: {
            std::shared_ptr<InterruptionEvent> itev =
                std::static_pointer_cast<InterruptionEvent>(pev);

            switch(itev->mode) {

                case IT_MODE_TILEROOT_ACTIVATION: {
                    // Only introduce catoms if on the lower tile level
                    if (catom->position[2] == meshSeedPosition[2]) {
                        feedBranches();

                        getScheduler()->schedule(
                            new InterruptionEvent(getScheduler()->now() +
                                                  (getRoundDuration()),
                                                  catom, IT_MODE_TILEROOT_ACTIVATION));
                    }
                } break;

                case IT_MODE_ALGORITHM_START:
                    matchRulesAndProbeGreenLight(); // the seed starts the algorithm
                    break;
            }
        }
    }
}

// void MeshAssemblyBlockCode::discardNextTargetForComponent(MeshComponent comp) {
//     short idx = getEntryPointLocationForCell(getEntryPointForMeshComponent(comp)) - RevZ_EPL;
//     if (not targetQueueForEPL[idx].empty())
//         targetQueueForEPL[idx].pop();
// }

// const Cell3DPosition MeshAssemblyBlockCode::getNextTargetForEPL(MeshComponent epl) {
//     short idx = epl - RevZ_EPL;
//     if (not targetQueueForEPL[idx].empty()) {
//         const Cell3DPosition& tPos = coordinatorPos +
//             ruleMatcher->getComponentPosition(targetQueueForEPL[idx].front());
//         targetQueueForEPL[idx].pop();
//         return tPos;
//     }

//     switch (epl) {
//         case RevZ_EPL: return ruleMatcher->getComponentPosition(Z_EPL);
//         case RevZ_R_EPL: return ruleMatcher->getComponentPosition(RevZ_R_EPL);
//         case RZ_L_EPL: return ruleMatcher->getComponentPosition(RZ_L_EPL);
//         case RZ_EPL: return ruleMatcher->getComponentPosition(LZ_EPL);
//         case RZ_R_EPL: return ruleMatcher->getComponentPosition(RZ_R_EPL);
//         case Z_R_EPL: return ruleMatcher->getComponentPosition(Z_R_EPL);
//         case Z_EPL: return ruleMatcher->getComponentPosition(RevZ_EPL);
//         case Z_L_EPL: return ruleMatcher->getComponentPosition(Z_L_EPL);
//         case LZ_R_EPL: return ruleMatcher->getComponentPosition(RZ_EPL);
//         case LZ_EPL: return ruleMatcher->getComponentPosition(RZ_EPL);
//         case LZ_L_EPL: return ruleMatcher->getComponentPosition(LZ_R_EPL);
//         case RevZ_L_EPL: return ruleMatcher->getComponentPosition(RevZ_L_EPL);
//         default:
//             cerr << "getNextTargetForEPL(" << epl << ")" << endl;
//             VS_ASSERT_MSG(false, "getNextTargetForEPL: input is not an EPL");
//     }

//     return Cell3DPosition(); // unreachable
// }

short MeshAssemblyBlockCode::getEntryPointLocationForCell(const Cell3DPosition& pos) {
    for (int i = 0; i < 12; i++)
        if (pos == catom->position + entryPointRelativePos[i]) return i + RevZ_EPL;

    return -1;
}

const Cell3DPosition MeshAssemblyBlockCode::getEntryPointForMeshComponent(MeshComponent mc) {
    switch(mc) {
        case R: return getEntryPointPosition(Z_R_EPL);
        case S_Z: return getEntryPointPosition(LZ_EPL);
        case S_RevZ: return getEntryPointPosition(RZ_EPL);
        case S_LZ: return getEntryPointPosition(LZ_R_EPL);
        case S_RZ: return getEntryPointPosition(RZ_R_EPL);

        case X_1: return getEntryPointPosition(Z_R_EPL);
        case X_2: case X_3: case X_4: case X_5: return getEntryPointPosition(RZ_EPL);

        case Y_1: return getEntryPointPosition(Z_L_EPL);
        case Y_2: case Y_3: case Y_4: case Y_5: return getEntryPointPosition(LZ_EPL);

        case Z_1: case Z_2: case Z_3: case Z_4: case Z_5:
            return getEntryPointPosition(Z_EPL);

        case RevZ_1: case RevZ_2: case RevZ_3: case RevZ_4: case RevZ_5:
            return getEntryPointPosition(RevZ_EPL);

        case LZ_1: case LZ_2: case LZ_3: case LZ_4: case LZ_5:
            return getEntryPointPosition(LZ_EPL);

        case RZ_1: case RZ_2: case RZ_3: case RZ_4: case RZ_5:
            return getEntryPointPosition(RZ_EPL);

            // case EPLs
        case RevZ_EPL: return getEntryPointPosition(Z_EPL);
        case RevZ_R_EPL: return getEntryPointPosition(Z_EPL);
        case RZ_L_EPL: return getEntryPointPosition(LZ_EPL);
        case RZ_EPL: return getEntryPointPosition(LZ_EPL);
        case RZ_R_EPL: return getEntryPointPosition(LZ_EPL);
        case Z_R_EPL: return getEntryPointPosition(RevZ_EPL);
        case Z_EPL: return getEntryPointPosition(RevZ_EPL);
        case Z_L_EPL: return getEntryPointPosition(RevZ_EPL);
        case LZ_R_EPL: return getEntryPointPosition(RZ_EPL);
        case LZ_EPL: return getEntryPointPosition(RZ_EPL);
        case LZ_L_EPL: return getEntryPointPosition(RZ_EPL);
        case RevZ_L_EPL: return getEntryPointPosition(Z_EPL);
        default: throw NotImplementedException("Entry point for EPL mesh component");
    }

    return Cell3DPosition(); // unreachable
}

void MeshAssemblyBlockCode::handleMeshComponentInsertion(MeshComponent mc) {
    // Introduce new catoms
    // cout << "[t-" << scheduler->now() << "] catom introduced" << endl;
    world->addBlock(0, buildNewBlockCode,
                    getEntryPointForMeshComponent(mc), ORANGE);
}

bool MeshAssemblyBlockCode::handleModuleInsertionToBranch(BranchIndex bid) {
    // Introduce new catoms
    // cout << "[t-" << scheduler->now() << "] catom introduced" << endl;
    const Cell3DPosition& entryPoint = getEntryPointForModuleOnBranch(bid);
    if (lattice->isFree(entryPoint)) {
        world->addBlock(0, buildNewBlockCode, entryPoint, ORANGE);
        catomsSpawnedToVBranch[bid]++;
        return true;
    } else {
        VS_ASSERT(false);
    }

    return false;
}

const Cell3DPosition MeshAssemblyBlockCode::getEntryPointForModuleOnBranch(BranchIndex bid) {
    MeshComponent epl;
    switch(bid) {
        case RevZBranch: epl = Z_EPL; break;
        case ZBranch: epl = RevZ_EPL; break;
            // if (catomsSpawnedToVBranch[ZBranch] == 0) return getEntryPointPosition(Z_L_EPL);
            // else if (catomsSpawnedToVBranch[ZBranch] == 1)return getEntryPointPosition(Z_R_EPL);
            // else return getEntryPointPosition(Z_EPL);
        case LZBranch: epl = RZ_EPL; break;
            // if (catomsSpawnedToVBranch[LZBranch] == 0) return getEntryPointPosition(LZ_R_EPL);
            // else return getEntryPointPosition(LZ_EPL);
        case RZBranch: epl = LZ_EPL; break;
            // if (catomsSpawnedToVBranch[RZBranch] == 0) return getEntryPointPosition(RZ_R_EPL                                                                                 );
            // else return getEntryPointPosition(RZ_EPL);
        default: throw NotImplementedException("getEntryPointForModuleOnBranch: invalid bid");
    }

    return getEntryPointPosition(epl);
}

bool MeshAssemblyBlockCode::
incidentBranchesToRootAreComplete(const Cell3DPosition& pos) {
    VS_ASSERT(ruleMatcher->isInMesh(norm(pos))
              and ruleMatcher->isTileRoot(norm(pos)));

    for (int i = 0; i < N_BRANCHES; i++) {
        if (!isIncidentBranchTipInPlace(pos, static_cast<BranchIndex>(i))) return false;
    }

    return true;
}

bool MeshAssemblyBlockCode::
isIncidentBranchTipInPlace(const Cell3DPosition& trp, BranchIndex bi) {
    const Cell3DPosition& tipp = trp + incidentTipRelativePos[bi];
    return (not ruleMatcher->isInPyramid(ruleMatcher->
                                         getTileRootPositionForMeshPosition(norm(tipp))))
        or lattice->cellHasBlock(tipp);
}

void MeshAssemblyBlockCode::scheduleRotationTo(const Cell3DPosition& pos,
                                               Catoms3DBlock* pivot = NULL) {
    try {
        if (not pivot) pivot = customFindMotionPivot(catom, pos);
        
        OUTPUT << "mvmt: " << round((scheduler->now()) / getRoundDuration()) << "\t" << endl;
        // cout << "[t-" << scheduler->now() << "] rotation scheduled" << endl;
        scheduler->schedule(new Rotation3DStartEvent(getScheduler()->now(),
                                                     catom, pivot, pos,
                                                     RotationLinkType::OctaFace, false));
#ifdef INTERACTIVE_MODE
        awaitKeyPressed();
#endif
    } catch (const NoAvailableRotationPivotException& e_piv) {
        cerr << e_piv.what();
        cerr << "target position: " << pos << endl;
        catom->setColor(MAGENTA);
        VS_ASSERT(false);
    } catch (std::exception const& e) {
        cerr << "exception: " << e.what() << endl;
        VS_ASSERT(false);
    }
}

void MeshAssemblyBlockCode::initializeTileRoot() {
    // Switch role
    role = Coordinator;
    coordinatorPos = catom->position;

    if (norm(catom->position) == Cell3DPosition(0,0,0)) t0 = scheduler->now();
    OUTPUT << "root: " << (int)(round((scheduler->now() - t0) / getRoundDuration())) << "\t" << norm(catom->position) << endl;

    // Determine how many branches need to grow from here
    // and initialize growth data structures
    for (short bi = 0; bi < N_BRANCHES; bi++) {
        catomsReqByBranch[bi] = ruleMatcher->
            shouldGrowPyramidBranch(norm(catom->position), (BranchIndex)bi) ? B - 1 : -1;
    }

    // Inspect each incoming vertical branch to see where catoms are ready to take part in
    //  the construction of the tile
    for (const Cell3DPosition& nPos : lattice->getActiveNeighborCells(catom->position)) {
        if (ruleMatcher->isVerticalBranchTip(norm(nPos))) {
            P2PNetworkInterface* nItf = catom->getInterface(nPos);
            VS_ASSERT(nItf and nItf->isConnected());
            sendMessage(new CoordinatorReadyMessage(), nItf, MSG_DELAY_MC, 0);
        }
    }

    // Populate EPLPivots to be used by ground modules
    EPLPivotBC[0] = static_cast<MeshAssemblyBlockCode*>(lattice->getBlock(catom->position + Cell3DPosition(0, 0, -2))->blockCode); // RevZBranch
    EPLPivotBC[1] = static_cast<MeshAssemblyBlockCode*>(lattice->getBlock(catom->position + Cell3DPosition(2, 2, -2))->blockCode); // ZBranch
    EPLPivotBC[2] = static_cast<MeshAssemblyBlockCode*>(lattice->getBlock(catom->position + Cell3DPosition(2, 0, -2))->blockCode); // RZBranch
    EPLPivotBC[3] = static_cast<MeshAssemblyBlockCode*>(lattice->getBlock(catom->position + Cell3DPosition(0, 2, -2))->blockCode); // LZBranch
    for (short bi = 0; bi < XBranch; bi++) VS_ASSERT(EPLPivotBC[bi]);

    
    // Schedule next growth iteration (at t + MOVEMENT_DURATION (?) )
    getScheduler()->schedule(
        new InterruptionEvent(getScheduler()->now(),
                              catom, IT_MODE_TILEROOT_ACTIVATION));
}

void MeshAssemblyBlockCode::initializeSupportModule() {
    for (const Cell3DPosition& nPos : lattice->getActiveNeighborCells(catom->position)) {
        if (ruleMatcher->isVerticalBranchTip(norm(nPos))) {
            branchTipPos = nPos;
            short bi = ruleMatcher->determineBranchForPosition(
                norm(nPos[2] < meshSeedPosition[2] ?
                     nPos + Cell3DPosition(0,0,B) : catom->position));
            branch = static_cast<BranchIndex>(bi);
            return;
        }
    }

    VS_ASSERT_MSG(false, "cannot find branch tip among neighbor modules");
}

const Cell3DPosition
MeshAssemblyBlockCode::getEntryPointPosition(MeshComponent epl) const {
    return getEntryPointRelativePos(epl) + coordinatorPos;
}

bool MeshAssemblyBlockCode::isOnEntryPoint(const Cell3DPosition& pos) const {
    const Cell3DPosition& nearestTR = denorm(
        ruleMatcher->getNearestTileRootPosition(norm(pos)));

    for (const Cell3DPosition& ep : entryPointRelativePos) {
        if (pos == ep + nearestTR) return true;
    }

    return false;
}

bool MeshAssemblyBlockCode::requestTargetCellFromTileRoot() {
    for (const Cell3DPosition& nPos : lattice->getActiveNeighborCells(catom->position)) {
        if (ruleMatcher->isVerticalBranchTip(norm(nPos))
            or ruleMatcher->isNFromVerticalBranchTip(norm(nPos), 1)
            or ruleMatcher->isTileSupport(norm(nPos))) {
            // Module is delegate coordinator
            P2PNetworkInterface* nItf = catom->getInterface(nPos);
            VS_ASSERT(nItf);
            // cout << "[t-" << getScheduler()->now() << "] requesting target cell" << endl;
            sendMessage(new RequestTargetCellMessage(catom->position, catom->blockId), nItf,
                        MSG_DELAY_MC, 0);
            log_send_message();
            return true;
        } // else Support module not yet in place, wait for it to come online and notify us
    }

    return false;
}

void MeshAssemblyBlockCode::initializeSandbox() {
    for (const auto& pos : ruleMatcher->getAllGroundTileRootPositionsForMesh()) {
        const Cell3DPosition& denormPos = denorm(pos);
        // cout << pos << " " << denormPos << endl;

        for (int i = 0; i < XBranch; i++) {
            world->addBlock(0, buildNewBlockCode,
                            denormPos + incidentTipRelativePos[i], PINK);
            world->addBlock(0, buildNewBlockCode, denormPos +
                            incidentTipRelativePos[i] + incidentTipRelativePos[i], GREY);
            world->addBlock(0, buildNewBlockCode,
                            denormPos + incidentTipRelativePos[i]
                            + incidentTipRelativePos[i] + incidentTipRelativePos[i],
                            GREY);
        }

        // Add waiting tile EPL modules
        for (int i = 0; i < XBranch; i++) {
                MeshComponent epl =ruleMatcher->getDefaultEPLComponentForBranch((BranchIndex)i);
                if (denormPos != meshSeedPosition or i != ZBranch) {
                    world->addBlock(0, buildNewBlockCode,
                                    denormPos + getEntryPointRelativePos(epl), YELLOW);
                }
        }

    }

    sandboxInitialized = true;
    // cout << "round duration: " << getRoundDuration() << endl;
}


void MeshAssemblyBlockCode::matchRulesAndRotate() {
    Cell3DPosition nextPos;
    bool matched = matchLocalRules(catom->getLocalNeighborhoodState(),
                                   catom->position,
                                   targetPosition,
                                   coordinatorPos, step, nextPos);

    if (matched) {
        scheduleRotationTo(nextPos);
    } else {
        // Try matching rules again once neighborhood updates
        catom->setColor(GOLD);
        cout << *catom << " is local rule matching" << endl;
        matchingLocalRule = true;
    }
}

void MeshAssemblyBlockCode::feedBranches() {
    for (int bi = 0; bi < XBranch; bi++) {
        const Cell3DPosition& supportPos = catom->position + 
            ruleMatcher->getSupportPositionForPosition(sbnorm(EPLPivotBC[bi]->catom->position));
        Catoms3DBlock* support = static_cast<Catoms3DBlock*>(lattice->getBlock(supportPos));
        
        // Only insert if green light on EPL pivot
        if (EPLPivotBC[bi]->greenLightIsOn
            // and if support is ready to receive modules as well
            and (not support
                 or static_cast<MeshAssemblyBlockCode*>(support->blockCode)->greenLightIsOn))
            handleModuleInsertionToBranch(static_cast<BranchIndex>(bi));
    }
}

bool MeshAssemblyBlockCode::isAtGroundLevel() {
    return catom->position[2] == meshSeedPosition[2];
}

/************************************************************************
 ************************* MOTION COORDINATION **************************
 ***********************************************************************/

void MeshAssemblyBlockCode::setGreenLightAndResumeFlow() {
    greenLightIsOn = true;
    catom->setColor(GREEN);

    if (moduleAwaitingGo) {
        bool nextToModule = isAdjacentToPosition(awaitingModulePos);

        Cell3DPosition pos;
        if (not nextToModule)
            pos = catom->position - ruleMatcher->getBranchUnitOffset(
                getBranchIndex(catom->position));
        
        P2PNetworkInterface* itf = nextToModule ?
            catom->getInterface(awaitingModulePos) :
            // Move the message up the branch 
            catom->getInterface(pos);

        VS_ASSERT(itf and itf->isConnected());
        sendMessage(new GreenLightIsOnMessage(catom->position, awaitingModulePos),
                    itf, MSG_DELAY_MC, 0);
        moduleAwaitingGo = false;
    }
}

bool MeshAssemblyBlockCode::isAdjacentToPosition(const Cell3DPosition& pos) const {
    return lattice->cellsAreAdjacent(catom->position, pos);
}

Catoms3DBlock* MeshAssemblyBlockCode::
findTargetLightAmongNeighbors(const Cell3DPosition& targetPos,
                              const Cell3DPosition& srcPos) const {
    for (const auto& cell : lattice->getActiveNeighborCells(catom->position)) {
        if (lattice->cellsAreAdjacent(cell, targetPos)
            and ruleMatcher->isInMesh(norm(cell))
            and cell != srcPos
            and Cell3DPosition::compare_ZYX(catom->position, cell)) // TOCHECK
            return static_cast<Catoms3DBlock*>(lattice->getBlock(cell));
    }

    return NULL;
}

void MeshAssemblyBlockCode::matchRulesAndProbeGreenLight() {
    Cell3DPosition nextPos;
    bool matched = matchLocalRules(catom->getLocalNeighborhoodState(),
                                   catom->position,
                                   targetPosition,
                                   coordinatorPos, step, nextPos);

    if (matched) {
        stepTargetPos = nextPos;
        Catoms3DBlock *pivot = customFindMotionPivot(catom, stepTargetPos);
        VS_ASSERT(pivot);
       
        matchingLocalRule = false;        
        
        sendMessage(new ProbePivotLightStateMessage(catom->position, stepTargetPos),
                    catom->getInterface(pivot->position), MSG_DELAY_MC, 0);
    } else {
        // Try matching rules again once neighborhood updates
        catom->setColor(BLUE);
        matchingLocalRule = true;
    }    
}

Catoms3DBlock* MeshAssemblyBlockCode::customFindMotionPivot(const Catoms3DBlock* m,
                                                            const Cell3DPosition& tPos,
                                                            RotationLinkType faceReq) {    
    const auto &allLinkPairs =
        Catoms3DMotionEngine::findPivotLinkPairsForTargetCell(m, tPos, faceReq);

    for (const auto& pair : allLinkPairs) {
        // Additional rule compared to Catoms3DMotionEngine::customFindMotionPivot:
        //  Make sure that pivot is not a FreeAgent (i.e., is part of scaffold)
        if (static_cast<MeshAssemblyBlockCode*>(pair.first->blockCode)->role == FreeAgent)
            continue;
            
        // cout << "{ " << *pair.first << ", " << *pair.second << " }" << endl;
        if (pair.second->getMRLT() == faceReq or faceReq == RotationLinkType::Any)
            return pair.first;
    }
    
    return NULL;
}

/************************************************************************
 ******************************** STATS *********************************
 ***********************************************************************/

void MeshAssemblyBlockCode::updateMsgRate() {
    Time t = (int)(round((scheduler->now() - t0) / getRoundDuration()));

    if (rate.first != t) {
        rate.first = t;
        rate.second = 1;
    } else {
        rate.second++;
    }

    if (rate.second > maxBitrate.first) {
        maxBitrate.first = rate.second;
        maxBitrate.second = ruleMatcher->roleToString(role);
    }
}

int MeshAssemblyBlockCode::sendMessage(HandleableMessage *msg,P2PNetworkInterface *dest,
                                       Time t0,Time dt) {
    // OUTPUT << "nbMessages:\t" << round(scheduler->now() / getRoundDuration()) << "\t" << ++nbMessages << endl   ;
    // updateMsgRate();
    return BlockCode::sendMessage(msg, dest, t0, dt);
}

int MeshAssemblyBlockCode::sendMessage(Message *msg,P2PNetworkInterface *dest,
                                       Time t0,Time dt) {
    // OUTPUT << "nbMessages:\t" << round(scheduler->now() / getRoundDuration()) << "\t" << ++nbMessages << endl;
    // updateMsgRate();
    return BlockCode::sendMessage(msg, dest, t0, dt);
}

void MeshAssemblyBlockCode::log_send_message() const {
    OUTPUT << "lfmsg: " << round((scheduler->now() - startTime) / getRoundDuration()) << "\t" << MeshRuleMatcher::roleToString(role) << endl;
}
