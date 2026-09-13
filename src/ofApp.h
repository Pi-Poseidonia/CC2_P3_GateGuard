	#pragma once

	#include "StartScreen.h"
	#include "AccessDecision.h"
	#include "AccessLog.h"
	#include "EUDetectionStrategy.h"
	#include "IndianDetectionStrategy.h"
	#include "GateAccessController.h"
	#include "LicensePlate.h"
	#include "PlateDetector.h"
	#include "TesseractPlateReader.h"
	#include "AccessLogger.h"
	#include "ConsoleAccessLogger.h"
	#include "EmailSecurityAlert.h"
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
		StartScreen startScreen;

		// support method for processing and adding a single image
		void processSingleImage(const std::string & filePath);

		// --- Dynamic Test Mode ---
		DetectionMode currentMode = MODE_INDIAN; // default to Indian mode; can be changed via key press

	// --- Detector & Strategies with smart pointers ---
		std::shared_ptr<EUDetectionStrategy> euStrategy;
		std::shared_ptr<IndianDetectionStrategy> indianStrategy;

		// Main detector (strategy is set dynamically via setStrategy)
		PlateDetector detector;

		// --- Single Image Test Data ---
		ofImage testImage;
		LicensePlate result; // EU detection result
		LicensePlate indianResult; // Indian detection result
		ofImage debugMask;

		// --- EU plate testing: multiple images, processed once at startup ---
		// NOTE: Indian testing removed for now - IndianDetectionStrategy.h/.cpp aren't yet
		// present in this branch's project (they live on feature/indian-strategy).
		std::vector<std::string> testImageFilenames = {
			"images/EU_DE1.jpg",
			"images/EU-DE2.jpg",
			"images/EU-DE3.jpg",
			"images/I_HR26.jpg",
			"images/I_RJ14.jpg",
			"images/I_RJ19.jpg",
			"images/I_TN87.jpg"
		};

		std::vector<ofImage> euImages;
		std::vector<LicensePlate> euResults; // Vector for EU detection results
		std::vector<LicensePlate> indianResults; // Vector for Indian detection results

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
