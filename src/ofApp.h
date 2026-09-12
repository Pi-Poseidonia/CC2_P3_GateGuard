#pragma once

#include "AccessDecision.h"
#include "AccessLog.h"
#include "EUDetectionStrategy.h"
#include "GarageUI.h"
#include "GateAccessController.h"
#include "LicensePlate.h"
#include "PlateDetector.h"
#include "PlateDisplayer.h"
#include "TesseractPlateReader.h"
#include "ofMain.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

class ofApp : public ofBaseApp {

public:
	void setup();
	void update();
	void draw();

	void keyPressed(int key);
	void keyReleased(int key);
	void mouseMoved(int x, int y);
	void mouseDragged(int x, int y, int button);
	void mousePressed(int x, int y, int button);
	void mouseReleased(int x, int y, int button);
	void mouseEntered(int x, int y);
	void mouseExited(int x, int y);
	void windowResized(int w, int h);
	void dragEvent(ofDragInfo dragInfo);
	void gotMessage(ofMessage msg);

	// --- EU plate testing: multiple images, processed once at startup ---
	// NOTE: Indian testing was tried locally (temporarily copying IndianDetectionStrategy
	// from feature/indian-strategy) but removed again before check-in, since that
	// strategy's files belong to a teammate's branch, not this one. Once
	// feature/indian-strategy is actually merged, re-add an indianDetector +
	// indianImageFilenames set the same way, and switch based on
	// garageUI.getSelectedStrategy() in draw()/keyPressed() (see git history for the
	// dual-strategy version if useful as a reference).
	std::vector<std::string> euImageFilenames = {
		"images/EU_DE1.jpg",
		"images/EU-DE2.jpg",
		"images/EU-DE3.jpg"
	};
	std::vector<ofImage> euImages;
	std::vector<LicensePlate> euResults;
	std::vector<std::string> euTesseractResults;
	std::vector<AccessDecision> euAccessDecisions;
	PlateDetector euDetector { std::make_shared<EUDetectionStrategy>() };

	TesseractPlateReader tesseractReader;

	// --- Phase 3: fuzzy-match each Tesseract result against the authorized plate
	// list (bin/data/users.csv), and log every decision (granted or denied). ---
	GateAccessController accessController;
	AccessLog accessLog;

	// --- Phase 4: UI dashboard - gate status graphic, strategy toggle, and the
	// per-plate visual pipeline (image + red box + crop thumbnail + text lines) ---
	PlateDisplayer plateDisplayer;
	GarageUI garageUI; // "Indian" button still shown/clickable but has no effect yet - see note above

	// Which test image is currently shown - cycle with LEFT/RIGHT arrow keys
	int currentIndex = 0;

	// Runs the shared detect -> Tesseract -> access-control pipeline for one image.
	void processOneImage(const std::string & filename, PlateDetector & detector,
		ofImage & outImage, LicensePlate & outResult,
		std::string & outTesseractText, AccessDecision & outDecision, bool accessListReady);
};
