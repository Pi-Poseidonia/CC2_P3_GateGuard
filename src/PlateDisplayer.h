#pragma once

#include "LicensePlate.h"
#include "ofMain.h"
#include <string>
#include <vector>

// One line of text to draw under a plate detection's visuals, with its own color -
// lets a caller show color-coded results (e.g. green for a correct-looking OCR
// result, red for ACCESS DENIED) without PlateDisplayer needing to know what any
// line actually means.
struct DisplayLine {
	std::string text;
	ofColor color = ofColor::white;
};

// Renders the full visual pipeline for one plate detection: the original image with a
// red bounding-box overlay around the detected plate, a thumbnail of the intermediate
// binarized/cropped plate image below it, and a stack of caller-provided text lines
// (OCR results, access decisions, etc.) underneath that. Centralizing this here -
// rather than inline in ofApp::draw() - means any future screen/panel that needs to
// show a plate detection can reuse the same rendering logic instead of duplicating it.
class PlateDisplayer {
public:
	// Draws everything for one plate detection, stacked vertically starting at (x, y),
	// scaled to fit within the given width. If maxTotalHeight is set (> 0), the whole
	// block (image + crop thumbnail, not the text lines) is shrunk proportionally -
	// preserving aspect ratio - so it never overflows that height budget, regardless
	// of the source photo's own aspect ratio. Returns the total height consumed, so
	// callers can stack multiple detections vertically without overlapping them.
	float draw(const ofImage & originalImage, const LicensePlate & result,
		const std::vector<DisplayLine> & textLines, float x, float y, float width,
		float maxTotalHeight = 0.0f);
};
