#pragma once

#include "EUDetectionStrategy.h"
#include "IndianDetectionStrategy.h"
#include "LicensePlate.h"
#include "PlateDetector.h"
#include "ofMain.h"
#include <memory>
#include <string>
#include <vector>

// Enum to switch between different detection modes (EU, Indian, or both)
enum DetectionMode {
	MODE_EU,
	MODE_INDIAN,
	MODE_BOTH
};

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

	// --- Dynamic Test Mode ---
	DetectionMode currentMode = MODE_INDIAN; // default to Indian mode; can be changed via key press

	// --- EU plate testing ---
	ofImage testImage;
	EUDetectionStrategy euStrategy;
	LicensePlate result;

	// --- DEBUG: intermediate blue-strip mask, for visual tuning of thresholds ---
	ofImage debugMask;

	// --- Indian plate testing ---
	IndianDetectionStrategy indianStrategy;
	LicensePlate indianResult;

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

	PlateDetector euDetector { std::make_shared<EUDetectionStrategy>() };

	// Which test image is currently shown - cycle with LEFT/RIGHT arrow keys
	int currentIndex = 0;
};
