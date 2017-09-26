#include "../morph_constants.h"
#include "XYZPlaneOverlay.h"
#include "../../../mec_log.h"

namespace mec {
namespace morph {

XYZPlaneOverlay::XYZPlaneOverlay(const std::string name, ISurfaceCallback &surfaceCallback, ICallback &callback)
        : OverlayFunction(name, surfaceCallback, callback) {}

bool XYZPlaneOverlay::init(const Preferences &preferences) {
    if (preferences.exists("semitones")) {
        semitones_ = preferences.getDouble("semitones");
    } else {
        semitones_ = 12;
        LOG_1("morph::OverlayFunction::init - property semitones not defined, using default 12");
    }

    if (preferences.exists("baseNote")) {
        baseNote_ = preferences.getDouble("baseNote");
    } else {
        baseNote_ = 32;
        LOG_1("morph::OverlayFunction::init - property baseNote not defined, using default 32");
    }
    return true;
}

bool XYZPlaneOverlay::interpretTouches(const Touches &touches) {
    const std::vector<std::shared_ptr<TouchWithDeltas>> &newTouches = touches.getNewTouches();
    for (auto touchIter = newTouches.begin(); touchIter != newTouches.end(); ++touchIter) {
        surfaceCallback_.touchOn(**touchIter);
        // remove as soon as surface support is fully implemented
        callback_.touchOn((*touchIter)->id_, xPosToNote((*touchIter)->x_), normalizeXPos((*touchIter)->x_),
                          normalizeYPos((*touchIter)->y_), normalizeZPos((*touchIter)->z_));
    }
    const std::vector<std::shared_ptr<TouchWithDeltas>> &continuedTouches = touches.getContinuedTouches();
    for (auto touchIter = continuedTouches.begin(); touchIter != continuedTouches.end(); ++touchIter) {
        surfaceCallback_.touchContinue(**touchIter);
        // remove as soon as surface support is fully implemented
        callback_.touchContinue((*touchIter)->id_, xPosToNote((*touchIter)->x_), normalizeXPos((*touchIter)->x_),
                                normalizeYPos((*touchIter)->y_), normalizeZPos((*touchIter)->z_));
    }
    const std::vector<std::shared_ptr<TouchWithDeltas>> &endedTouches = touches.getEndedTouches();
    for (auto touchIter = endedTouches.begin(); touchIter != endedTouches.end(); ++touchIter) {
        surfaceCallback_.touchOff(**touchIter);
        // remove as soon as surface support is fully implemented
        callback_.touchOff((*touchIter)->id_, xPosToNote((*touchIter)->x_), normalizeXPos((*touchIter)->x_),
                           normalizeYPos((*touchIter)->y_), normalizeZPos((*touchIter)->z_));
    }
    return true;
}

float XYZPlaneOverlay::xPosToNote(float xPos) {
    return (xPos / MEC_MORPH_PANEL_WIDTH * 12 + baseNote_); //TODO: semitones: get panel width for composite panels
}

float XYZPlaneOverlay::normalizeXPos(float xPos) {
    return (xPos / MEC_MORPH_PANEL_WIDTH * 12);
}

float XYZPlaneOverlay::normalizeYPos(float yPos) {
    return 1.0 - yPos / MEC_MORPH_PANEL_HEIGHT;
}

float XYZPlaneOverlay::normalizeZPos(float zPos) {
    float normalizedPressure = zPos / MEC_MORPH_MAX_Z_PRESSURE;
    if (normalizedPressure < 0.01) {
        normalizedPressure = 0.01;
    }
    return normalizedPressure;
}

}
}