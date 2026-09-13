#pragma once

#include "AccessDecision.h"
#include "AccessLog.h"
#include "AccessLogger.h"
#include "ConsoleAccessLogger.h"
#include "EUDetectionStrategy.h"
#include "EmailSecurityAlert.h"
#include "GarageUI.h"
#include "GateAccessController.h"
#include "IndianDetectionStrategy.h"
#include "LicensePlate.h"
#include "PlateDetector.h"
#include "PlateDisplayer.h"
#include "StartScreen.h"
#include "TesseractPlateReader.h"
#include "ofMain.h"
#include <algorithm>
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

	// --- Polymorphic Logging ---
	std::vector<std::shared_ptr<AccessLogger>> loggers;

private:
	// --- Screens & UI Elements ---
	StartScreen startScreen;
	GarageUI garageUI;
	PlateDisplayer plateDisplayer;

	// --- Image Loading & Processing ---
	void loadDefaultPipelineImages();
	void loadSingleImageFromPath(const std::string & path);

	// support method for processing and adding a single image
	void processSingleImage(const std::string & filePath);
	void processOneImage(const std::string & filename, PlateDetector & detector,
		ofImage & outImage, LicensePlate & outResult,
		std::string & outTesseractText, AccessDecision & outDecision, bool accessListReady);

	// --- Dynamic Test Mode ---
	DetectionMode currentMode = MODE_INDIAN; // default to Indian mode; can be changed via key press

	// --- Detector & Strategies with smart pointers ---
	std::shared_ptr<EUDetectionStrategy> euStrategy;
	std::shared_ptr<IndianDetectionStrategy> indianStrategy;

	// Main detector (strategy is set dynamically via setStrategy)
	PlateDetector detector;
	PlateDetector euDetector { std::make_shared<EUDetectionStrategy>() };

	// --- Single Image Test Data ---
	ofImage testImage;
	LicensePlate result; // EU detection result
	LicensePlate indianResult; // Indian detection result
	ofImage debugMask;

	// --- Multi-Image Datasets & Results ---
	std::vector<std::string> testImageFilenames;
	std::vector<ofImage> euImages;
	std::vector<LicensePlate> euResults; // Vector for EU detection results
	std::vector<LicensePlate> indianResults; // Vector for Indian detection results

	// --- OCR Engines ---
	TesseractPlateReader tesseractReader;
	std::vector<std::string> tesseractResults;

	// --- Phase 3: Access Control & Logging ---
	GateAccessController accessController;
	AccessLog accessLog;
	std::vector<AccessDecision> accessDecisions;

	// Which test image is currently shown - cycle with LEFT/RIGHT arrow keys
	int currentIndex = 0;
};
