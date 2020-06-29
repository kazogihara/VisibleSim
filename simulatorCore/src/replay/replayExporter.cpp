/**
 * @file   replayExporter.cpp
 * @author Pierre Thalamy <pthalamy@pierre-ZenBook-UX433FA>
 * @date   Tue Jun  9 11:14:00 2020
 *
 * @brief Simulation data exporter to be used for simulation replay
 *
 *
 */

#include <algorithm>
#include "replayExporter.h"
#include "replayTags.h"

#include "../utils/utils.h"
#include "../base/simulator.h"

using namespace BaseSimulator;
using namespace ReplayTags;

ReplayExporter::ReplayExporter() {
    const string& fnbin = buildExportFilename();
    nbEventsBeforeKeyframe=NumberOfEventsBetweenKeyFrames;

    cout << TermColor::BWhite
         << "(replay) exporting simulation data to file: " << TermColor::Reset
         << fnbin << endl;

    exportFile = new ofstream(fnbin, ios::out | ios::binary);

    if (debug) {
        const string& fndebug = debugFilenameFromExportFilename(fnbin);

        cout << TermColor::BWhite
             << "(replay) exporting replay debug to file: " << TermColor::Reset
             << fndebug << endl;

        debugFile = new ofstream(fndebug, ios::out);
    }
}

string ReplayExporter::buildExportFilename() const {
    const string& cliFilename = Simulator::getSimulator()->getCmdLine().getReplayFilename();
    if (not cliFilename.empty())
        return cliFilename;

    auto& cmdLine = Simulator::getSimulator()->getCmdLine();

    string appName = cmdLine.getApplicationName();
    // if appname starts with "./" strip it
    if (appName[0] == '.' and appName[1] == '/')
        appName = appName.substr(2, appName.size());

    string configName = cmdLine.getConfigFile();

    // assumes config name format *.xml
    configName = configName.substr(0, configName.size() - 4);

    stringstream prefix;
    prefix << "replay_" << appName;
    if (configName != "config")
        prefix << "_" << configName;

    return utils::generateTimestampedFilename(prefix.str(), extension, true);
}

string ReplayExporter::debugFilenameFromExportFilename(const string& exportFn) const {
    string str = string(exportFn);

    std::size_t ext = str.find_last_of(".");

//    return str.replace(str.begin() + ext, str.end(), ".txt");
    return "debug.txt";
}


void ReplayExporter::endExport() {
    writeKeyFramesIndex();
    writeSimulationEndTime();

    if (exportFile) {
        exportFile->close();
        delete exportFile;
        exportFile = nullptr;
    }

    if (debugFile) {
        debugFile->close();
        delete debugFile;
        debugFile = nullptr;
    }
}

bool ReplayExporter::isReplayEnabled() {
    return (Simulator::getSimulator()->getCmdLine().isReplayEnabled() && enabled);
}

void ReplayExporter::writeHeader() {
    exportFile->write((char*)&VS_MAGIC, sizeof(u4));

    const u1 moduleType = BaseSimulator::getWorld()->getBlockType();
    const Cell3DPosition& gridSize = BaseSimulator::getWorld()->lattice->gridSize;

    exportFile->write((char*)&moduleType, sizeof(u1));
    exportFile->write((char*)&gridSize, 3*sizeof(u2)); // xyz

    keyFramesIndexPos = exportFile->tellp();
    exportFile->write((char*)&keyFramesIndexPos, sizeof(u8)); // we temporaly write keyFramesIndexPos, will be updated by keyframe table address

    cout << "debug:" << debug << endl;
    if (debug) {
        debugFile->write((char*)&VS_MAGIC, sizeof(u4)); *debugFile << endl;
        *debugFile << (int)moduleType << endl;
        *debugFile << gridSize[0] << " " << gridSize[1] << " " << gridSize[2] << endl; // xyz
        *debugFile << "TABLE INDEX: " << keyFramesIndexPos << endl;
        // keyFramesIndexPosDebug = debugFile->tellp();
        // cout << "keyFramesIndexPosDebug: " << keyFramesIndexPosDebug << endl;
    }
}

void ReplayExporter::writeKeyFramesIndex() {
    // Write number of index entries
    // Move write head to previously saved index location
    auto currentPos = exportFile->tellp(); // current position
    exportFile->seekp(keyFramesIndexPos); // begin of the file
    exportFile->write((char*)&currentPos, sizeof(u8));
    exportFile->seekp(currentPos); // back at the end of the file

    //if (debug) debugFile->seekp(keyFramesIndexPosDebug);
    u8 nEntries = keyFramesIndex.size();
    exportFile->write((char*)&nEntries, sizeof(u8));

    if (debug) {
        *debugFile << "-- BEGIN KEY FRAME INDEX --" << endl;
        *debugFile << nEntries << endl;
    }

    //  and write all key frame pairs (kfp) to file
    for (const auto& kfp : keyFramesIndex) {
        s8 pos = (s8)kfp.second;

        exportFile->write((char*)&kfp.first, sizeof(u8));
        exportFile->write((char*)&pos, sizeof(s8));

        if (debug) *debugFile << kfp.first << " " << pos << endl;
    }

    if (debug) *debugFile << "-- END KEY FRAME INDEX --" << endl;
}

void ReplayExporter::writeSimulationEndTime() {
    Time endDate = getScheduler()->now();

    exportFile->write((char*)&endDate, sizeof(u8));
    if (debug) *debugFile << endDate << endl;
}

void ReplayExporter::writeKeyFrameIfNeeded(Time date) {
    //if (date > (lastKeyFrameExportDate + keyFrameSaveFrequency)) {
    if (nbEventsBeforeKeyframe<=0) {
        writeKeyFrame(date);
        //lastKeyFrameExportDate = date;
    }
}

void ReplayExporter::writeKeyFrame(Time date) {
    keyFramesIndex.insert(make_pair(date, exportFile->tellp()));

    u4 nbModules = BaseSimulator::getWorld()->lattice->nbModules;
    exportFile->write((char*)&nbModules, sizeof(u4));
    if (debug) {
        *debugFile << "-- BEGIN KEY FRAME #" << keyFramesIndex.size()
                   << " (t = " << date << ") --"  << endl;
        *debugFile << nbModules << endl;
    }

    for (const pair<bID, BuildingBlock*>& pair:BaseSimulator::getWorld()->buildingBlocksMap) {
        pair.second->serialize(*exportFile);
        if (debug) pair.second->serialize_cleartext(*debugFile);
    }

    if (debug) {
        *debugFile << "-- END KEY FRAME #" << keyFramesIndex.size() << endl;
    }
    nbEventsBeforeKeyframe=NumberOfEventsBetweenKeyFrames;
}

void ReplayExporter::writeColorUpdate(Time date, bID bid, const Color& color) {
    exportFile->write((char*)&date, sizeof(Time));
    exportFile->write((char*)&EVENT_COLOR_UPDATE, sizeof(u1));
    exportFile->write((char*)&bid, sizeof(bID));
    u1 u1color[3];
    for (std::size_t i;i<3; i++) {
        u1color[i] = static_cast<int>(color.rgba[i] * 255.0);
        if (u1color[i]<0) u1color[i]=0;
        else if (u1color[i]>255) u1color[i]=255;
    }
    exportFile->write((char*)&u1color,3*sizeof(u1));

    if (debug) {
        *debugFile << "Color:" << date << " " << (int)EVENT_COLOR_UPDATE << " " << bid << " " << (int)u1color[0] << " " << (int)u1color[1] << " " << (int)u1color[2] << endl;
    }
    nbEventsBeforeKeyframe--;
}

void ReplayExporter::writeDisplayUpdate(Time date, bID bid, uint16_t value) {
    exportFile->write((char*)&date, sizeof(Time));
    exportFile->write((char*)&EVENT_DISPLAY_UPDATE, sizeof(u1));
    exportFile->write((char*)&bid, sizeof(bID));
    exportFile->write((char*)&value, sizeof(u2));

    if (debug) {
        *debugFile << "Dpy:" << date << " " << (int)EVENT_DISPLAY_UPDATE << " " << bid << " " << value << endl;
    }
    nbEventsBeforeKeyframe--;
}

void ReplayExporter::writePositionUpdate(Time date, bID bid, const Cell3DPosition& pos, uint8_t orientation) {
    exportFile->write((char*)&date, sizeof(Time));
    exportFile->write((char*)&EVENT_POSITION_UPDATE, sizeof(u1));
    exportFile->write((char*)&bid, sizeof(bID));
    exportFile->write((char*)&pos.pt,3*sizeof(u2));
    exportFile->write((char*)&orientation,sizeof(u1));
    if (debug) {
        *debugFile << "Pos:" << date << " " << (int)EVENT_POSITION_UPDATE << " " << bid << " " << pos[0] << " " << pos[1] << " " << pos[2] << " " << (int)orientation << endl;
    }
    nbEventsBeforeKeyframe--;
}

void ReplayExporter::writeAddModule(Time date, bID bid) {
    exportFile->write((char*)&date, sizeof(Time));
    exportFile->write((char*)&EVENT_ADD_MODULE, sizeof(u1));
    exportFile->write((char*)&bid, sizeof(u2));

    if (debug) {
        *debugFile << "Add:" << date << " " << (int)EVENT_ADD_MODULE << " " << bid << endl;
    }
    nbEventsBeforeKeyframe--;
}

void ReplayExporter::writeRemoveModule(Time date, bID bid) {
    exportFile->write((char*)&date, sizeof(Time));
    exportFile->write((char*)&EVENT_REMOVE_MODULE, sizeof(u1));
    exportFile->write((char*)&bid, sizeof(bID));

    if (debug) {
        *debugFile << "Rmv:" << date << " " << (int)EVENT_REMOVE_MODULE << " " << bid << endl;
    }
    nbEventsBeforeKeyframe--;
}

void ReplayExporter::writeMotion(Time date, bID bid, Time duration_us,
                                 const Cell3DPosition& destination) {
    exportFile->write((char*)&date, sizeof(Time));
    exportFile->write((char*)&EVENT_MOTION, sizeof(u1));
    exportFile->write((char*)&bid, sizeof(bID));
    exportFile->write((char*)&duration_us, sizeof(u8));
    exportFile->write((char*)&destination.pt,3*sizeof(u2));

    if (debug) {
        *debugFile << "Motion:" << date << " " << (int)EVENT_MOTION << " " << bid
            << " " << (int)duration_us << " " << " " << destination[0] << " " << destination[1] << " " << destination[2] << endl;
    }
    nbEventsBeforeKeyframe--;
}

void ReplayExporter::writeConsoleTrace(Time date, bID bid, const string& trace) {
    /*exportFile->write((char*)&date, sizeof(Time));
    exportFile->write((char*)&EVENT_CONSOLE_TRACE, sizeof(u1));
    exportFile->write((char*)&bid, sizeof(bID));
    exportFile->write((char*)&trace.cstr(), trace.length()*sizeof(u1));


    if (debug) {
        *debugFile << "INFO:" << date << " " <<(int) EVENT_MOTION << " " << bid << " " << trace << endl;
    }
    nbEventsBeforeKeyframe--;
*/
}
