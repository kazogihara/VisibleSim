#include "coatingBlockCode.hpp"

#include <unistd.h>

#include "coatingUtils.hpp"
#include "coatingMessages.hpp"

using namespace Catoms3D;

CoatingBlockCode::CoatingBlockCode(Catoms3DBlock *host) : Catoms3DBlockCode(host) {
    // @warning Do not remove block below, as a blockcode with a nullptr host might be created
    //  for command line parsing
    if (not host) return;

    // Registers a callback (handleSampleMessage) to the message of type SAMPLE_MSG_ID
    // addMessageEventFunc2(SAMPLE_MSG_ID,
    //                      std::bind(&CoatingBlockCode::handleSampleMessage, this,
    //                                std::placeholders::_1, std::placeholders::_2));

    world = BaseSimulator::getWorld();

    // set the module pointer
    catom = static_cast<Catoms3DBlock*>(hostBlock);

    if (COATING_MODE) isInG = CoatingBlockCode::isInCoating;
    else isInG = CoatingBlockCode::isInCSG;

    if (not neighborhood) neighborhood = new Neighborhood(isInG);
    if (not border) border = new Border(isInG, neighborhood);
    if (not seeding) seeding = new Seeding(isInG, neighborhood, border);
    if (not scaffold) scaffold = new ScaffoldManager(scaffoldSeed, CoatingBlockCode::isInCSG);
}

CoatingBlockCode::~CoatingBlockCode() {
    if (neighborhood) {
        delete neighborhood;
        neighborhood = nullptr;
    }

    if (border) {
        delete border;
        border = nullptr;
    }

    if (seeding) {
        delete seeding;
        seeding = nullptr;
    }

    if (scaffold) {
        delete scaffold;
        scaffold = nullptr;
    }
};

void CoatingBlockCode::startup() {
    if (COATING_MODE and not sandboxInitialized) initializeSandbox();

    if (catom->blockId == 1) {
        G_SEED_POS = catom->position; // FIXME:

        if (not isInG(G_SEED_POS)) {
            stringstream ss;
            ss << "Seed module at "
               << TermColor::BWhite << G_SEED_POS << TermColor::Reset
               << " must be in the CSG target" << endl;

            if (COATING_MODE)
                lattice->highlightAllCellsThatVerify([this](const Cell3DPosition& p) {
                    return isInCoatingLayer(p, HIGHLIGHT_COATING_LAYER); }, RED);
            else
                target->highlight();

            awaitKeyPressed();

            throw CoatingException(ss.str());
        }
    }

    static bool delayInit = false;
    if (not delayInit) {
        if (scheduler->getMode() == SCHEDULER_MODE_FASTEST)
            ATTRACT_DELAY = 0;
        delayInit = true;
    }

    if (HIGHLIGHT_COATING or HIGHLIGHT_CSG or HIGHLIGHT_SEEDS) {
        highlight();
        HIGHLIGHT_COATING = false;
        HIGHLIGHT_CSG = false;
        HIGHLIGHT_SEEDS = false;
    }

    if (isSupportPosition(catom->position)) {
        // For each free neighbor position
        for (const Cell3DPosition& p : lattice->getFreeNeighborCells(catom->position)) {
            // If it is a blocked coating position
            if (p[2] == catom->position[2]
                and coatingCellWillBeBlockedBy(p, catom->position)) {
                // Ensure that it has no other support position in its neighborhood
                //  or that this support is already in place, and then:
                for (const Cell3DPosition& pn : lattice->getFreeNeighborCells(p)){
                    if (isSupportPosition(pn))
                        goto nextNeighbor;
                }

                // Attract neighbors to that position until the next corner along the border
                expectedSegments.insert(p);
                attractedBySupport.insert(p);
                sendAttractSignalTo(p);
            }

        nextNeighbor: ;
        }
    }

    if (not isInG(catom->position)) {
        assembleInternalScaffoldNeighbors();
        return;
    }

    int layer = getGLayer(catom->position);

    if (attractedBySupport.count(catom->position)) {
        catom->setColor(MAGENTA);
        ++planeAttracted[layer];
        attractNextModuleAlongSegment();
        return;
    }

    if (catom->position == G_SEED_POS) {
        initializePlaneSeeds();

        if (COATING_MODE) attractStructuralSupports(0); // first layer supports only
    }

    if (++planeAttracted[layer] == planeRequires[layer]) {
        catom->setColor(CYAN);

        // Start next layer if not top plane
        // cout << "nPlanes: " << nPlanes << endl;
        // cout << "layer: " << layer << endl;
        if (layer < nPlanes - 1) {
            if (COATING_MODE) {
                // Attract structural supports
                attractStructuralSupports(layer + 1);
            }

            attractPlane(layer + 1);
        }
    }

    // Simulate authorizations
    if (watchlist.find(catom->position) != watchlist.end()) {
        auto const& it = watchlist.find(catom->position);

        stringstream info;
        info << " provides attract authorization to " << it->first;
        scheduler->trace(info.str(), catom->blockId, AUTH_DEBUG_COLOR);

        const auto& callback = it->second;
        callback();
        watchlist.erase(it);
    }


    if (isInG(catom->position)) {
        attract();
    }
}

void CoatingBlockCode::handleSampleMessage(MessagePtr msgPtr, P2PNetworkInterface* sender) {
    // MessageOf<int>* msg = static_cast<MessageOf<int>*>(msgPtr.get());
}

void CoatingBlockCode::onMotionEnd() {
    console << " has reached " << catom->position << "\n";

    // do stuff
    // ...
}

void CoatingBlockCode::processLocalEvent(EventPtr pev) {
    MessagePtr message;
    stringstream info;

    // Do not remove line below
    BlockCode::processLocalEvent(pev);

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
            }
        } break;

        case EVENT_ADD_NEIGHBOR: {
            // Do something when a neighbor is added to an interface of the catom
            uint64_t face = Catoms3DWorld::getWorld()->lattice->
                getOppositeDirection((std::static_pointer_cast<AddNeighborEvent>(pev))
                                     ->face);
            const Cell3DPosition& pos = catom->getNeighborBlock(face)->position;

            if (pos == completionNeighborPos) {
                // Send Completion message to attracted neighbor
                expectingCompletionNeighbor = false;
                P2PNetworkInterface* nextItf = catom->getInterface(pos);
                sendMessage(new BorderCompletionMessage, nextItf, MSG_DELAY, 0);
            }

            break;
        }

        case EVENT_REMOVE_NEIGHBOR: {
            // Do something when a neighbor is removed from an interface of the catom
            break;
        }
    }
}

/// ADVANCED BLOCKCODE FUNCTIONS BELOW

void CoatingBlockCode::onBlockSelected() {
    // Debug stuff:
    cout << endl << "--- PRINT CATOM " << *catom << "---" << endl;

    // cout << "isNorthSeed(" << catom->position << "): "
    //      << seeding->isNorthSeed(catom->position) << endl;

    // cout << "isInG(" << catom->position << "): "
    //      << isInG(catom->position) << endl;

    // cout << "couldBeSeed(" << neighborhood->cellInDirection(catom->position, East) << "): "
    //      << seeding->couldBeSeed(neighborhood->cellInDirection(catom->position, East)) << endl;

    // cout << "couldBeSeed(" << neighborhood->cellInDirection(catom->position, South) << "): "
    //      << seeding->couldBeSeed(neighborhood->cellInDirection(catom->position, South)) <<endl;

    // cout << "isSeedBorderOnNextPlane(" << catom->position + seeding->backwardSeed << "): "
    //      << seeding->isSeedBorderOnNextPlane(catom->position + seeding->backwardSeed) << endl;

    // cout << "isSeedBorderOnCurrentPlane(" << catom->position << "): "
    //      << seeding->isSeedBorderOnCurrentPlane(catom->position) << endl;

    // cout << "isOnBorder(" << catom->position << "): "
    //      << border->isOnBorder(catom->position) << endl;

    // cout << "isLowestOfBorderOnCurrentPlane(" << catom->position << "): "
    //      << seeding->isLowestOfBorderOnCurrentPlane(catom->position) << endl;

    // cout << "isLowestOfBorderOnNextPlane(" << catom->position + seeding->backwardSeed << "): "
    //      <<seeding->isLowestOfBorderOnNextPlane(catom->position + seeding->backwardSeed)<<endl;

    // cout << endl << "Plane Requires: " << endl;
    // for (int i = 0; i < nPlanes; i++) {
    //     cout << i << "\t" << planeRequires[i] << endl;
    // }

    // cout << endl << "Plane Attracted: " << endl;
    // for (int i = 0; i < nPlanes; i++) {
    //     cout << i << "\t" << planeAttracted[i] << endl;
    // }

    // cout << endl << "Plane Seed: " << endl;
    // for (int i = 0; i < nPlanes; i++) {
    //     cout << i;
    //     for (const Cell3DPosition& seed : planeSeed[i]) {
    //         cout << "\t" << seed;
    //     }
    //     cout << endl;
    // }

    cout << endl << "Plane Structural Supports: " << endl;
    cout << catom->position[2] << "\t[";
    for(const Cell3DPosition&pos:scaffold->getAllSupportPositionsForPlane(catom->position[2])){
        cout << pos << ", ";
    }
    cout << "]" << endl;
}

void CoatingBlockCode::onAssertTriggered() {
    console << " has triggered an assert" << "\n";

    // Print debugging some info if needed below
    // ...
}

bool CoatingBlockCode::parseUserCommandLineArgument(int &argc, char **argv[]) {
    /* Reading the command line */
    if ((argc > 0) && ((*argv)[0][0] == '-')) {
        switch((*argv)[0][1]) {

            // Single character example: -u
            case 'u':   {
                COATING_MODE = true;
                return true;
            } break;

                // Composite argument example: --foo 13
            case '-': {
                string varg = string((*argv)[0] + 2); // argv[0] without "--"
                if (varg == string("coating")) { //
                    HIGHLIGHT_COATING = true;

                    if ((*argv)[1] and (*argv)[1][0] != '-') {
                        try {
                            HIGHLIGHT_COATING_LAYER = stoi((*argv)[1]);
                            argc--;
                            (*argv)++;
                        } catch(std::logic_error&) {
                            stringstream err;
                            err << "Found invalid parameter after option --coating: "
                                << (*argv)[1] << endl;
                            throw CLIParsingError(err.str());
                        }
                    }

                    cout << "--coating option provided with value: "
                         << HIGHLIGHT_COATING_LAYER << endl;
                } else if (varg == string("csg")) {
                    HIGHLIGHT_CSG = true;

                    cout << "--csg option provided" << endl;
                } else if (varg == string("seeds")) {
                    HIGHLIGHT_SEEDS = true;

                    cout << "--seeds option provided" << endl;
                } else if (varg == string("supports")) {
                    HIGHLIGHT_SUPPORTS = true;

                    cout << "--supports option provided" << endl;
                } else if (varg == string("scaffold")) {
                    HIGHLIGHT_SCAFFOLD = true;

                    cout << "--scaffold option provided" << endl;
                } else if (varg == string("delay")) { //
                    try {
                        ATTRACT_DELAY = stoi((*argv)[1]);
                        argc--;
                        (*argv)++;
                    } catch(std::logic_error&) {
                        stringstream err;
                        err << "Found invalid parameter after option --delay: '"
                            << (*argv)[1] << "' (expected integer value in ms)" << endl;
                        throw CLIParsingError(err.str());
                    }

                    cout << "--delay option provided with value: "
                         << ATTRACT_DELAY << " ms" << endl;
                } else {
                    return false;
                }

                return true;
            }
        }
    }

    return false;
}

void CoatingBlockCode::highlight() const {
    if (HIGHLIGHT_CSG) target->highlight();

    if (HIGHLIGHT_COATING) {
        lattice->highlightAllCellsThatVerify([this](const Cell3DPosition& p) {
            return isInCoatingLayer(p, HIGHLIGHT_COATING_LAYER); }, WHITE);

        // Scaffold:
        // lattice->highlightAllCellsThatVerify([this](const Cell3DPosition& p) {
        //     return scaffold->isWithinCSGMinus2(p)
        //         and scaffold->isInScaffold(p - scaffoldSeed); }, YELLOW);
    }

    if (HIGHLIGHT_SEEDS) {
        // lattice->highlightAllCellsThatVerify(
        //     [this](const Cell3DPosition& p) { return seeding->isNorthSeed(p); }, GREEN);
        // lattice->highlightAllCellsThatVerify(
        //     [this](const Cell3DPosition& p) { return neighborhood->isSouthSeed(p); }, ORANGE);
        // lattice->highlightAllCellsThatVerify( [this](const Cell3DPosition& p) {
        //     return neighborhood->isNorthLineOnMerge(p); }, RED);
        // lattice->highlightAllCellsThatVerify([this](const Cell3DPosition& p) {
        //     return neighborhood->isSouthLineOnMerge(p); }, BLUE);
        // lattice->highlightAllCellsThatVerify([this](const Cell3DPosition& p) {
        //     return isInG(p) and border->isOnInternalHole(p); }, MAGENTA);
        lattice->highlightAllCellsThatVerify([this](const Cell3DPosition& p) {
            return isInG(p) and seeding->isPlaneSeed(p); }, MAGENTA);
    }

    if (HIGHLIGHT_SCAFFOLD) {
        // Scaffold
        lattice->highlightAllCellsThatVerify(
            [this](const Cell3DPosition& p){
                return scaffold->isInsideFn(p)
                    and scaffold->isInScaffold(scaffold->normalize(p));
            }, GREEN);
    }

    if (HIGHLIGHT_SUPPORTS) {
        int zmax = lattice->getGridUpperBounds()[2];
        for (int z = 0; z <= zmax; z++) {
            for (const Cell3DPosition& pos : scaffold->getAllSupportPositionsForPlane(z)) {
                lattice->highlightCell(pos, SupportColor);
            }
        }
    }
}

int CoatingBlockCode::getGLayer(const Cell3DPosition& pos) {
    return pos[2] - G_SEED_POS[2];
}


bool CoatingBlockCode::isOnCSGBorder(const Cell3DPosition& pos) {
    if (target->isInTarget(pos)) {
        for (const Cell3DPosition& neighbor :
                 BaseSimulator::getWorld()->lattice->getNeighborhood(pos)) {
            if (neighbor[2] >= G_SEED_POS[2]
                and not target->isInTarget(neighbor)) return true;
        }
    } else {
        // Try to include outer corners but exclude inner corners
        bool hasOrthoNeighborsInCSG = hasOrthogonalNeighborsInCSG(pos);

        if (not hasOrthoNeighborsInCSG) return false;

        for (const Cell3DPosition& pRel : diagNeighbors) {
            if (target->isInTarget(pRel + pos)) return true;
        }
    }

    return false;
}

bool CoatingBlockCode::isInCoating(const Cell3DPosition& pos) {
    return BaseSimulator::getWorld()->lattice->isInGrid(pos)
        and pos[2] >= G_SEED_POS[2] and isInCoatingLayer(pos, getGLayer(pos));
}

bool CoatingBlockCode::isInCoatingLayer(const Cell3DPosition& pos, int layer) {
    int pLayer = getGLayer(pos);

    if (// not isInCSG(pos) or
        (layer != -1 and pLayer != layer)) return false;

    return isOnCSGBorder(pos);

    // return (layer == -1 or pLayer == layer)
    //     // and hasNeighborInCSG(pos);  // Coating at distance 1
    //     and not hasHorizontalNeighborInCSG(pos) and has2ndOrderNeighborInCSG(pos); // Coating distance 2
}

bool CoatingBlockCode::hasHorizontalNeighborInCSG(const Cell3DPosition& pos) {
    for (const Cell3DPosition& p : BaseSimulator::getWorld()->lattice->getNeighborhood(pos)) {
        if (p[2] < pos[2]) continue;

        if (isInCSG(p)) return true;
    }

    // for (const Cell3DPosition& pRel : diagNeighbors) {
    //     if (isInCSG(pRel + pos)) return true;
    // }

    return false;
}

bool CoatingBlockCode::has2ndOrderNeighborInCSG(const Cell3DPosition& pos) {
    for (const Cell3DPosition& pRel : _2ndOrderNeighbors) {
        if (isInCSG(pRel + pos)) return true;
    }

    return false;
}

void CoatingBlockCode::attract() {
    stringstream info;

    // North attraction
    if (seeding->isNorthSeed(catom->position)
        and not neighborhood->hasNeighborInDirection(catom->position,
                                                     SkewFCCLattice::Direction::C1North)) {
        sendAttractSignalTo(neighborhood->cellInDirection(catom->position, North));
    }

    // South attraction
    if (seeding->isSouthSeed(catom->position)
        and not neighborhood->hasNeighborInDirection(catom->position,
                                                     SkewFCCLattice::Direction::C7South)) {
        sendAttractSignalTo(neighborhood->cellInDirection(catom->position, South));
    }

    // West attraction
    if (neighborhood->directionIsInCSG(catom->position, West)
        and not neighborhood->hasNeighborInDirection(catom->position,
                                                     SkewFCCLattice::Direction::C6West)) {
        const Cell3DPosition& wPos = neighborhood->cellInDirection(catom->position, West);
        if (neighborhood->hasNoHorizontalNeighbor(catom->position))
            sendAttractSignalTo(wPos);
        else if (neighborhood->directionIsInCSG(catom->position, SouthWest)
            and neighborhood->hasNeighborInDirection(catom->position,
                                                     SkewFCCLattice::Direction::C7South)) {
            getAuthorizationToAttract(neighborhood->
                                      cellInDirection(catom->position, South), West);
        } else if (not neighborhood->hasNeighborInDirection(catom->position,
                                                            SkewFCCLattice::Direction::C6West)
                   and not neighborhood->directionIsInCSG(catom->position, South)
                   and neighborhood->directionIsInCSG(catom->position, West)
                   and neighborhood->directionIsInCSG(catom->position, SouthWest)
                   and border->isOnInternalHole(catom->position)
                   and not borderHasWaitingModule(East)) {
            info << " sends a WEST border following request for " << wPos;
            scheduler->trace(info.str(),catom->blockId, ATTRACT_DEBUG_COLOR);

            borderFollowingAttractRequest(neighborhood->cellInDirection(wPos, South), West);
        } else {
            sendAttractSignalTo(wPos);
        }
    }

    // East attraction
    if (neighborhood->directionIsInCSG(catom->position, East)
        and not neighborhood->hasNeighborInDirection(catom->position,
                                                     SkewFCCLattice::Direction::C0East)) {
        const Cell3DPosition& ePos = neighborhood->cellInDirection(catom->position, East);
        if (neighborhood->hasNoHorizontalNeighbor(catom->position))
            sendAttractSignalTo(ePos);
        else if (neighborhood->directionIsInCSG(catom->position, NorthEast)
            and neighborhood->hasNeighborInDirection(catom->position,
                                                     SkewFCCLattice::Direction::C1North)) {
            getAuthorizationToAttract(neighborhood->
                                      cellInDirection(catom->position, North),East);
        } else if (not neighborhood->hasNeighborInDirection(catom->position,
                                                            SkewFCCLattice::Direction::C0East)
                   and not neighborhood->directionIsInCSG(catom->position, North)
                   and neighborhood->directionIsInCSG(catom->position, East)
                   and neighborhood->directionIsInCSG(catom->position, NorthEast)
                   and border->isOnInternalHole(catom->position)
                   and not borderHasWaitingModule(West)) {
            info.str("");
            info << " sends a EAST border following request for " << ePos;
            scheduler->trace(info.str(),catom->blockId, ATTRACT_DEBUG_COLOR);

            borderFollowingAttractRequest(neighborhood->cellInDirection(ePos, North), East);
        } else {
            sendAttractSignalTo(ePos);
        }
    }
}

void CoatingBlockCode::attractStructuralSupports(int layer) {
    int z = G_SEED_POS[2] + layer;

    if (planeSupports.size() <= (unsigned int)layer) {
        planeSupports.push_back(set<Cell3DPosition>());
        planeSupportsReady.push_back(0);
        VS_ASSERT(planeSupports.size() > (unsigned int)layer);
    }

    for (const Cell3DPosition& pos : scaffold->getAllSupportPositionsForPlane(z)) {
        stringstream info;
        info << " attracts layer " << layer << " structural support to " << pos;
        scheduler->trace(info.str(), catom->blockId, ATTRACT_DEBUG_COLOR);

        planeSupports[layer].insert(pos);

        Color color = SupportColor;
        if (lattice->cellIsBlocked(pos)) {
            lattice->highlightCell(pos, RED);
            color = InvalidColor;
        }

        try {
            world->addBlock(0, buildNewBlockCode, pos, color);
        } catch (DoubleInsertionException const& e) {
            cerr << e.what() << endl;
            lattice->highlightCell(pos, color);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(ATTRACT_DELAY * 4));
    }
}

void CoatingBlockCode::sendAttractSignalTo(const Cell3DPosition& pos) {
    stringstream info;
    info << " attracts to " << planarDirectionPositionToString(pos - catom->position)
         << " position " << pos;
    scheduler->trace(info.str(), catom->blockId, ATTRACT_DEBUG_COLOR);

    if (catom->color != InvalidColor // Preserve err trace
        and not isSupportPosition(catom->position)
        and not attractedBySupport.count(catom->position))
        catom->setColor(AttractedColor);

    std::this_thread::sleep_for(std::chrono::milliseconds(ATTRACT_DELAY));

    if (waitingModules.find(pos) != waitingModules.end())
        waitingModules.erase(catom->position);

    Color color = DefaultColor;
    if (lattice->cellIsBlocked(pos)) {
        lattice->highlightCell(pos, RED);
        color = InvalidColor;
    }

    try {
        world->addBlock(0, buildNewBlockCode, pos, color);
    } catch (DoubleInsertionException const& e) {
        cerr << e.what() << endl;
        catom->setColor(InvalidColor);
        info.str("");
        info << " DoubleInsertion when attracting module to " << pos;
        scheduler->trace(info.str(), catom->blockId, InvalidColor);
    }
}

bool CoatingBlockCode::getAuthorizationToAttract(const Cell3DPosition& requestee,
                                                 PlanarDir d) {
    stringstream info;
    const Cell3DPosition& pos = catom->position + planarPos[d];
    info << " requests authorization to attract " << pos << "("
         << planarDirectionIndexToString(d) << ") from " << requestee;
    scheduler->trace(info.str(), catom->blockId, AUTH_DEBUG_COLOR);

    // stringstream info2;
    const Cell3DPosition& checkPos = requestee + planarPos[d];
    // info2 << " checkPos: " << checkPos;
    // scheduler->trace(info2.str(), catom->blockId, AUTH_DEBUG_COLOR);

    if (lattice->getBlock(checkPos)) {
        stringstream info;
        info << checkPos << " module in place, self-granting authorization from " << requestee;
        scheduler->trace(info.str(), catom->blockId, AUTH_DEBUG_COLOR);

        sendAttractSignalTo(pos);

        return true;
    }

    // Else. Add to watchlist and get notified when module is added
    VS_ASSERT(watchlist.find(checkPos) == watchlist.end());
    watchlist.emplace(checkPos, std::bind(&CoatingBlockCode::sendAttractSignalTo, this, pos));
    catom->setColor(WaitingColor);

    return false;
}

bool CoatingBlockCode::borderFollowingAttractRequest(const Cell3DPosition& requestee,
                                                     PlanarDir d) {
    stringstream info;
    const Cell3DPosition& pos = catom->position + planarPos[d];
    info << " requests BORDER authorization to attract " << pos << "("
         << planarDirectionIndexToString(d) << ") from " << requestee;
    scheduler->trace(info.str(), catom->blockId, AUTH_DEBUG_COLOR);

    if (lattice->getBlock(requestee)) {
        stringstream info;
        info << target << " module in place, self-granting BORDER authorization";
        scheduler->trace(info.str(), catom->blockId, AUTH_DEBUG_COLOR);

        sendAttractSignalTo(pos);

        return true;
    }

    // Else. Add to watchlist and get notified when module is added
    VS_ASSERT(watchlist.find(requestee) == watchlist.end());
    watchlist.emplace(requestee, std::bind(&CoatingBlockCode::sendAttractSignalTo, this, pos));

    catom->setColor(WaitingColor);
    waitingModules.insert(catom->position);

    return false;
}

void CoatingBlockCode::initializePlaneSeeds() {

    int maxPlane = -1;
    Cell3DPosition pos;

    for (short iz = G_SEED_POS[2]; iz <= lattice->getGridUpperBounds()[2]; iz++) {
        const Cell3DPosition& glb = lattice->getGridLowerBounds(iz);
        const Cell3DPosition& gub = lattice->getGridUpperBounds(iz);
        int idx = iz - G_SEED_POS[2];

        for (short iy = glb[1]; iy <= gub[1]; iy++) {
            for (short ix = glb[0]; ix <= gub[0]; ix++) {
                pos.set(ix,iy,iz);

                if (isInG(pos)) {
                    if (idx > maxPlane) {
                        maxPlane = idx;
                        planeRequires.push_back(0);
                        planeAttracted.push_back(0);
                        planeSeed.push_back(list<Cell3DPosition>());
                    }

                    if (seeding->isPlaneSeed(pos)) // Super costly
                        planeSeed[idx].push_back(pos);

                    planeRequires[idx]++;
                }
            }
        }
    }

    nPlanes = maxPlane + 1;
}

void CoatingBlockCode::initializeSandbox() {
    sandboxInitialized = true;

    static const int B = 6;
    const Cell3DPosition& gub = lattice->getGridUpperBounds(scaffoldSeed[2]);
    for (int x = scaffoldSeed[0]; x <= gub[0]; x += B) {
        for (int y = scaffoldSeed[1]; y <= gub[1]; y += B) {
            const Cell3DPosition& trPos = Cell3DPosition(x, y, scaffoldSeed[2]);

            for (int i = 0; i < XBranch; i++) {
                Cell3DPosition pos = trPos;
                for (int j = 0; j < 3; j++) {
                    pos += scaffold->getIncidentTipRelativePos((BranchIndex)i);

                    if (lattice->isInGrid(pos) and not lattice->getBlock(pos)) {
                        try {
                            world->addBlock(0, buildNewBlockCode, pos, GREY);
                        } catch (DoubleInsertionException const& e) {
                            cerr << e.what() << endl;
                        }
                        // nbSandboxCatoms++;
                    }
                }
            }
        }
    }
}

bool CoatingBlockCode::borderHasWaitingModule(int startIdx) const {
    // From Thadeu's Sync/sync.cpp
    int nTurns = 0;
    int idx = startIdx;
    if (idx == -1) return false;

    if (not border->isOnBorder(catom->position))
        return false;

    // lattice->highlightCell(catom->position, BLACK);

    Cell3DPosition currentPos = catom->position;
    nTurns += border->getNextBorderNeighborInPlace(idx, currentPos);

    // lattice->highlightCell(currentPos,YELLOW);

    while(currentPos != catom->position) {
        // lattice->unhighlightCell(currentPos);

        if (waitingModules.find(currentPos) != waitingModules.end()) return true;

        nTurns += border->getNextBorderNeighborInPlace(idx, currentPos);

        // lattice->highlightCell(currentPos,YELLOW);
        // usleep(200000);
    }

    return false;
}

bool CoatingBlockCode::hasOrthogonalNeighborsInCSG(const Cell3DPosition& pos) {
    SkewFCCLattice::Direction dirs[] = {
        SkewFCCLattice::Direction::C0East, SkewFCCLattice::Direction::C1North,
        SkewFCCLattice::Direction::C6West, SkewFCCLattice::Direction::C7South
    };

    Lattice *lattice = BaseSimulator::getWorld()->lattice;

    for (const SkewFCCLattice::Direction& d : dirs) {

        const pair<short, short> pair = Neighborhood::getOrthogonalDirections(d);
        const Cell3DPosition& pRef = lattice->getCellInDirection(pos, d);
        const Cell3DPosition& p1 = lattice->getCellInDirection(pos, pair.first);
        const Cell3DPosition& p2 = lattice->getCellInDirection(pos, pair.second);

        if ((target->isInTarget(pRef) and target->isInTarget(p1))
            or (target->isInTarget(pRef) and target->isInTarget(p2)))
            return true;
    }

    return false;
}

bool CoatingBlockCode::isSupportPosition(const Cell3DPosition& pos) {
    unsigned int layer = getGLayer(pos);

    if (planeSupports.size() <= layer) return false;

    return planeSupports[layer].count(pos);
}

bool CoatingBlockCode::isOnSupportSegment(const Cell3DPosition& pos) const {
    unsigned int layer = getGLayer(pos);

    if (planeSupports.size() <= layer) return false;

    // Find coating neighbor
    Cell3DPosition cn = Cell3DPosition(-1,-1,-1);
    for (const Cell3DPosition& p : lattice->getNeighborhood(pos)) {
        if (isInG(p)) {
            cn = p;
            break;
        }
    }

    VS_ASSERT(cn != Cell3DPosition(-1,-1,-1));

    // Find opposite opposition
    const Cell3DPosition& copp = lattice->getOppositeCell(pos, cn);

    // Search for support neighbor
    bool hasSupportNeighbor = false;
    Cell3DPosition suppN;
    for (const Cell3DPosition& p : lattice->getNeighborhood(pos)) {
        if (isSupportPosition(p)) {
            hasSupportNeighbor = true;
            suppN = p;
            break;
        }
    }

    if ( (hasSupportNeighbor and not isInG(lattice->getOppositeCell(pos, suppN)))
         or not isInG(copp) ) // module is on a corner
        return false;

    // Otherwise, check cn and copp directions and see if the first
    //  out of coating position is a support in one of these directions

    short d = lattice->getDirection(pos, cn);
    short dOpp = lattice->getDirection(pos, copp);

    Cell3DPosition cur;

    reachEndOfSegmentInDirection(d, cur, pos);
    if (isSupportPosition(cur)) return true;

    reachEndOfSegmentInDirection(dOpp, cur, pos);
    if (isSupportPosition(cur)) return true;

    return false;
}

void CoatingBlockCode::reachEndOfSegmentInDirection(short dir, Cell3DPosition& cur,
                                                    const Cell3DPosition& from) const {
    cur = from;

    do {
        // lattice->highlightCell(cur, BLUE);
        cur = lattice->getCellInDirection(cur, dir);
        // usleep(50000);
        // lattice->unhighlightCell(cur);
    } while (isInG(cur));
}


bool CoatingBlockCode::coatingCellWillBeBlockedBy(const Cell3DPosition& a,
                                                  const Cell3DPosition& b) const {
    // Topology:
    // b - a - c
    // a is only blocked by b is c is part of the coating
    return isInG(a) and isInG(lattice->getOppositeCell(a, b));
}

void CoatingBlockCode::attractNextModuleAlongSegment() {
    // Find the attracter of this module
    //  either a support for first module of segment, or a neighbor coating position
    Cell3DPosition attracter = Cell3DPosition(-1,-1,-1);
    short int numSupportNeighbors = 0;
    for (const Cell3DPosition& p : lattice->getActiveNeighborCells(catom->position)) {
        if (p[2] == catom->position[2]
            and (isInG(p) or isSupportPosition(p))) {
            attracter = p;
            if (isSupportPosition(p)) ++numSupportNeighbors;
            // break;
        }
    }

    VS_ASSERT(attracter != Cell3DPosition(-1,-1,-1));

    // Deduce its direction relative to self
    short dirAtt = lattice->getDirection(catom->position, attracter);

    // Infer direction and position of next module along segment
    short dirNext = lattice->getOppositeDirection(dirAtt);
    Cell3DPosition next = lattice->getCellInDirection(catom->position, dirNext);

    // // FIXME: Ensure that module is not a plane seed
    // bool nextIsSeed = false;
    // unsigned int layer = getGLayer(next);
    // if (layer > 0) {
    //     for (const Cell3DPosition& seed : planeSeed[layer - 1]) {
    //         const Cell3DPosition& aPos = isInG(seed + seeding->forwardSeed) ?
    //             seeding->forwardSeed : seeding->backwardSeed;

    //         if (aPos == next) {
    //             nextIsSeed = true;
    //             break;
    //         }
    //     }
    // }

    // Check if next module is a corner --> Not a corner if cell will be blocked by self
    // End segment if corner, or attract the next module otherwise
    if (coatingCellWillBeBlockedBy(next, catom->position)) {
        attractedBySupport.insert(next);
        sendAttractSignalTo(next);
    } else {
        // segment is complete, notify parent support

        // corner modules might have more than one support neighbors if the segment length
        //  is 1. However, only one of them has attracted it, thus, notify both as it is
        //  unknown which one is the actual parent
        if (numSupportNeighbors == 2) {
            for (const Cell3DPosition& p : lattice->getActiveNeighborCells(catom->position)){
                if (p[2] == catom->position[2] and isSupportPosition(p)) {
                    P2PNetworkInterface* supportItf = catom->getInterface(p);
                    sendMessage(new SupportSegmentCompleteMessage(), supportItf, MSG_DELAY, 0);
                }
            }
        } else {
            P2PNetworkInterface* prevItf = catom->getInterface(dirAtt);
            sendMessage(new SupportSegmentCompleteMessage(), prevItf, MSG_DELAY, 0);
        }
    }
}

void CoatingBlockCode::attractPlane(unsigned int layer) {
    // Attract first modules of next plane
    for (const Cell3DPosition& seed : planeSeed[layer - 1]) {
        const Cell3DPosition& aPos = isInG(seed + seeding->forwardSeed) ?
            seeding->forwardSeed : seeding->backwardSeed;

        const Cell3DPosition& firstPos = seed + aPos;

        VS_ASSERT_MSG(isInG(firstPos), "seed target is not in G!");

        // If first module of next plane has already been attracted by a support module
        //  Message it to start attracting remaining modules?

        if (m_mod(layer, 6) != 0) {
            sendAttractSignalTo(firstPos);
        } else {
            if (not isOnSupportSegment(firstPos)) {
                sendAttractSignalTo(firstPos);
            } else {
                // FIXME: THIS SHOULD BE DONE FROM THE SEED MODULE, MIGHT CAUSE ERRORS
                // check for neighbor segment positions
                bool hasSegmentNeighbor = false;
                for (const Cell3DPosition& p : lattice->getNeighborhood(seed)) {
                    if (p[2] == catom->position[2] and isOnSupportSegment(p)) {
                        hasSegmentNeighbor = true;
                        break;
                    }
                }

                // Completion is guaranteed
                if (hasSegmentNeighbor) {
                    startBorderCompletionAlgorithmFromSeed(seed);
                } else { // Check if completion is necessary before attracting First
                    // Find next coating position on own layer and send it a detection message
                    Cell3DPosition next =
                        findNextCoatingPositionOnLayer(Cell3DPosition(-1,-1,-1));
                    VS_ASSERT(next != Cell3DPosition(-1,-1,-1));

                    P2PNetworkInterface* nextItf = catom->getInterface(next);
                    sendMessage(new NextPlaneSegmentDetectionMessage(false),
                                nextItf, MSG_DELAY, 0);
                }
            }
        }
    }
}

void CoatingBlockCode::startBorderCompletionAlgorithmFromSeed(const Cell3DPosition& seed) {
    // FIXME: This should not be done by manually getting the module from Lattice!
    Catoms3DBlock *seedCatom = static_cast<Catoms3DBlock*>(lattice->getBlock(seed));
    VS_ASSERT(seedCatom != nullptr);

    const Cell3DPosition& aPos = isInG(seed + seeding->forwardSeed) ?
        seeding->forwardSeed : seeding->backwardSeed;

    const Cell3DPosition& firstPos = seed + aPos;

    P2PNetworkInterface* nextItf = seedCatom->getInterface(firstPos);
    sendMessage(new BorderCompletionMessage, nextItf, MSG_DELAY, 0);
}

Cell3DPosition CoatingBlockCode::
findNextCoatingPositionOnLayer(const Cell3DPosition& previous) const {
    for (const Cell3DPosition& p : lattice->getNeighborhood(catom->position)) {
        if (p[2] == catom->position[2] and isInG(p) and p != previous) {
            return p;
        }
    }

    return Cell3DPosition(-1,-1,-1);
}

void CoatingBlockCode::assembleInternalScaffoldNeighbors() {
    for (const Cell3DPosition& pos : lattice->getNeighborhood(catom->position)) {
        if (scaffold->isInScaffold(scaffold->normalize(pos)) and scaffold->isInsideFn(pos)
            and lattice->isFree(pos)) {
            try {
                world->addBlock(0, buildNewBlockCode, pos, GREY);
            } catch (DoubleInsertionException const& e) {
                cerr << e.what() << endl;
                lattice->highlightCell(pos, InvalidColor);
            }
        }
    }

    builtScaffold = true;
}
