#pragma once

#include "AccessDecision.h"
#include "AccessLog.h"
#include "EUDetectionStrategy.h"
#include "GateAccessController.h"
#include "LicensePlate.h"
#include "PlateDetector.h"
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
	// NOTE: Indian testing removed for now - IndianDetectionStrategy.h/.cpp aren't yet
	// present in this branch's project (they live on feature/indian-strategy).
	std::vector<std::string> testImageFilenames = {
		"images/EU_DE1.jpg",
		"images/EU-DE2.jpg",
		"images/EU-DE3.jpg"
	};

	std::vector<ofImage> euImages;
	std::vector<LicensePlate> euResults;

	// --- Two OCR engines run side by side on the same detected plate crop, for
	// direct accuracy comparison: our custom zoning matcher (inside PlateDetector)
	// vs Tesseract. Detection (EUDetectionStrategy) is shared/unchanged either way. ---
	PlateDetector euDetector { std::make_shared<EUDetectionStrategy>() };
	TesseractPlateReader tesseractReader;
	std::vector<std::string> tesseractResults;

	// --- Phase 3: fuzzy-match each Tesseract result against the authorized plate
	// list (bin/data/users.csv), and log every decision (granted or denied). ---
	GateAccessController accessController;
	AccessLog accessLog;
	std::vector<AccessDecision> accessDecisions;

	// Which test image is currently shown - cycle with LEFT/RIGHT arrow keys
	int currentIndex = 0;
};
