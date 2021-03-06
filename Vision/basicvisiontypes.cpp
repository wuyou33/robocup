#include "basicvisiontypes.h"
#include <map>

using namespace std;

namespace Vision {

    //private statically defined maps
    namespace {
        class DebugMap : public map<DEBUG_ID, pair<int, string> > {
        public:
            DebugMap() {
                (*this)[DBID_IMAGE]                 = pair<int, string>(0, "Image");
                (*this)[DBID_CLASSED_IMAGE]         = pair<int, string>(1, "Classified Image");
                (*this)[DBID_H_SCANS]               = pair<int, string>(2, "Horizontal Scans");
                (*this)[DBID_V_SCANS]               = pair<int, string>(3, "Vertical Scans");
                (*this)[DBID_SEGMENTS]              = pair<int, string>(4, "Segments");
                (*this)[DBID_FILTERED_SEGMENTS]     = pair<int, string>(5, "Filtered Segments");
                (*this)[DBID_MATCHED_SEGMENTS]      = pair<int, string>(6, "Transitions (matched segments)");
                (*this)[DBID_HORIZON]               = pair<int, string>(7, "Kinematics Horizon");
                (*this)[DBID_GREENHORIZON_SCANS]    = pair<int, string>(8, "Green Horizon Scans");
                (*this)[DBID_GREENHORIZON_THROWN]   = pair<int, string>(9, "GH thrown points");
                (*this)[DBID_GREENHORIZON_FINAL]    = pair<int, string>(10, "Green Horizon");
                (*this)[DBID_OBSTACLE_POINTS]       = pair<int, string>(11, "Obstacle Points");
                (*this)[DBID_GOALS]                 = pair<int, string>(12, "Goals");
                (*this)[DBID_BALLS]                 = pair<int, string>(13, "Balls");
                (*this)[DBID_OBSTACLES]             = pair<int, string>(14, "Obstacles");
                (*this)[DBID_LINES]                 = pair<int, string>(15, "Lines");
                (*this)[DBID_CENTRE_CIRCLES]        = pair<int, string>(16, "Centre Circles");
                (*this)[DBID_CORNERS]               = pair<int, string>(17, "Corners");
                (*this)[DBID_GOAL_LINES_START]      = pair<int, string>(18, "Goal Lines (start)");
                (*this)[DBID_GOAL_LINES_END]        = pair<int, string>(19, "Goal Lines (end)");
                (*this)[DBID_GOALS_HIST]            = pair<int, string>(20, "Goals (histogram)");
                (*this)[DBID_GOALS_RANSAC_EDGES]    = pair<int, string>(21, "Goals (RANSAC edges)");
            }
        };

        DebugMap debugmap;

        class VFOMap : public map<VFO_ID, pair<int, string> > {
        public:
            VFOMap() {
                (*this)[BALL]           = pair<int, string>(0, "BALL");
                (*this)[FIELDLINE]      = pair<int, string>(1, "FIELDLINE");
                (*this)[CORNER]         = pair<int, string>(2, "CORNER");
                (*this)[CENTRE_CIRCLE]  = pair<int, string>(3, "CENTRE_CIRCLE");
                (*this)[OBSTACLE]       = pair<int, string>(4, "OBSTACLE");
                (*this)[GOAL_L]         = pair<int, string>(5, "GOAL_L");
                (*this)[GOAL_R]         = pair<int, string>(6, "GOAL_R");
                (*this)[GOAL_U]         = pair<int, string>(7, "GOAL_U");
                //        GOAL_Y_L=1,
                //        GOAL_Y_R=2,
                //        GOAL_Y_U=3,
                //        GOAL_B_L=4,
                //        GOAL_B_R=5,
                //        GOAL_B_U=6,
                //        BEACON_Y=7,
                //        BEACON_B=8,
                //        BEACON_U=9,
            }
        };

        VFOMap vfomap;
    }

    std::string debugIDName(DEBUG_ID id) {
        map<DEBUG_ID, pair<int, string> >::const_iterator it = debugmap.find(id);

        if(it == debugmap.end()) {
            throw "Invalid DEBUG_ID";
        }
        else {
            return it->second.second;
        }
    }

    DEBUG_ID debugIDFromInt(int id) {
        map<DEBUG_ID, pair<int, string> >::const_iterator it = debugmap.begin();

        while(it != debugmap.end() && it->second.first != id)
            it++;

        if(it == debugmap.end()) {
            throw "Invalid DEBUG_ID number";
        }
        else {
            return it->first;
        }
    }

    int intFromDebugID(DEBUG_ID id) {
        map<DEBUG_ID, pair<int, string> >::const_iterator it = debugmap.find(id);

        if(it == debugmap.end()) {
            throw "Invalid DEBUG_ID";
        }
        else {
            return it->second.first;
        }
    }

    int numDebugIDs()
    {
        return debugmap.size();
    }

    std::string VFOName(VFO_ID id)
    {
        map<VFO_ID, pair<int, string> >::const_iterator it = vfomap.find(id);

        if(it == vfomap.end()) {
            throw "Invalid VFO_ID passed to VFOName()";
        }
        else {
            return it->second.second;
        }
    }

    VFO_ID VFOFromName(const std::string &name)
    {
        map<VFO_ID, pair<int, string> >::const_iterator it = vfomap.begin();

        while(it != vfomap.end() && it->second.second.compare(name) != 0)
            it++;

        if(it == vfomap.end()) {
            throw "Invalid VFO_ID name passed to VFOFromName()";
        }
        else {
            return it->first;
        }
    }

    VFO_ID VFOFromInt(int n) {
        map<VFO_ID, pair<int, string> >::const_iterator it = vfomap.begin();

        while(it != vfomap.end() && it->second.first != n)
            it++;

        if(it == vfomap.end()) {
            throw "Invalid VFO_ID number passed to VFOFromInt()";
        }
        else {
            return it->first;
        }
    }

    int intFromVFO(VFO_ID id) {
        map<VFO_ID, pair<int, string> >::const_iterator it = vfomap.find(id);

        if(it == vfomap.end()) {
            throw "Invalid VFO_ID passed to intFromVFO()";
        }
        else {
            return it->second.first;
        }
    }

    int numVFOIDs()
    {
        return vfomap.size();
    }

    std::string getColourClassName(COLOUR_CLASS id)
    {
        switch(id) {
        case BALL_COLOUR:          return "BALL_COLOUR";
        case GOAL_COLOUR:          return "GOAL_COLOUR";
    //    case GOAL_Y_COLOUR:        return "GOAL_Y_COLOUR";
    //    case GOAL_B_COLOUR:        return "GOAL_B_COLOUR";
        case LINE_COLOUR:          return "LINE_COLOUR";
        default:                   return "UNKNOWN_COLOUR";
        }
    }

    COLOUR_CLASS getColourClassFromName(const std::string& name)
    {
        if(name.compare("BALL_COLOUR") == 0)
            return BALL_COLOUR;
        else if(name.compare("GOAL_COLOUR") == 0)
            return GOAL_COLOUR;
    //    else if(name.compare("GOAL_Y_COLOUR") == 0)
    //        return GOAL_Y_COLOUR;
    //    else if(name.compare("GOAL_B_COLOUR") == 0)
    //        return GOAL_B_COLOUR;
        else if(name.compare("LINE_COLOUR") == 0)
            return LINE_COLOUR;
        else
            return UNKNOWN_COLOUR;
    }

    DistanceMethod getDistanceMethodFromName(std::string name)
    {
        if(name.compare("WIDTH") == 0)
            return Width;
        else if(name.compare("D2P") == 0)
            return D2P;
        else if(name.compare("LEAST") == 0)
            return Least;
        else if(name.compare("AVERAGE") == 0)
            return Average;

        //no match - return default
        #ifdef DEBUG_VISION_VERBOSITY_ON
            debug << "getDistanceMethodFromName - unmatched method name: " << name << " used D2P instead" << std::endl;
        #endif
        return D2P; //default
    }

    std::string getDistanceMethodName(DistanceMethod method)
    {
        switch(method) {
        case Width:     return "WIDTH";
        case D2P:       return "D2P";
        case Average:   return "AVERAGE";
        case Least:     return "LEAST";
        default:        return "UNKOWN";
        }
    }

    LineDetectionMethod getLineMethodFromName(std::string name)
    {
        if(name.compare("SAM") == 0)
            return SAM;
        else if(name.compare("RANSAC") == 0)
            return RANSAC;

        //no match - return default
        #ifdef DEBUG_VISION_VERBOSITY_ON
            debug << "VisionConstants::getLineMethodFromName - unmatched method name: " << name << " used RANSAC instead" << std::endl;
        #endif
        return RANSAC; //default
    }

    std::string getLineMethodName(LineDetectionMethod method)
    {
        switch(method) {
        case SAM:       return "SAM";
        case RANSAC:    return "RANSAC";
        default:        return "INVALID";
        }
    }
}
