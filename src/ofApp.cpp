#include "ofApp.h"

// Crops an image down to the bounding box of its actual dark ("ink") pixels, with a
// small padding margin kept around the edges.
static ofPixels tightCropToInkWithPadding(const ofPixels & input, int padding) {
	int width = input.getWidth();
	int height = input.getHeight();
	int numChannels = input.getNumChannels();

	int minX = width, minY = height, maxX = 0, maxY = 0;
	bool foundAny = false;

	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			int index = (y * width + x) * numChannels;
			if (input[index] < 128) {
				if (x < minX) minX = x;
				if (x > maxX) maxX = x;
				if (y < minY) minY = y;
				if (y > maxY) maxY = y;
				foundAny = true;
			}
		}
	}

	if (!foundAny) {
		return input; // nothing to trim
	}

	int cropX = std::max(0, minX - padding);
	int cropY = std::max(0, minY - padding);
	int cropMaxX = std::min(width - 1, maxX + padding);
	int cropMaxY = std::min(height - 1, maxY + padding);

	ofPixels tight;
	input.cropTo(tight, cropX, cropY, cropMaxX - cropX + 1, cropMaxY - cropY + 1);
	return tight;
}

// A pure ink bounding box (above) can't distinguish "real text" from "an isolated
// stray mark" - both are ink, so both get included, expanding the box to cover the
// stray mark rather than excluding it. This is the actual fix: find each separate
// contiguous horizontal ink blob (via column-gap detection, the same technique
// PlateDetector uses to segment characters), and drop the trailing blob if it's both
// isolated by a gap AND much narrower than its neighbor - a real character is roughly
// as wide as the ones next to it, while a border/frame fragment caught by the plate
// box's safety margin tends to be a thin sliver.
static ofPixels trimStrayEdgeBlob(const ofPixels & input) {
	int width = input.getWidth();
	int height = input.getHeight();
	int numChannels = input.getNumChannels();

	std::vector<bool> columnHasInk(width, false);
	for (int x = 0; x < width; x++) {
		for (int y = 0; y < height; y++) {
			int index = (y * width + x) * numChannels;
			if (input[index] < 128) {
				columnHasInk[x] = true;
				break;
			}
		}
	}

	struct Blob {
		int startX;
		int width;
	};
	std::vector<Blob> blobs;
	int blobStart = -1;
	for (int x = 0; x < width; x++) {
		if (columnHasInk[x] && blobStart == -1) {
			blobStart = x;
		} else if (!columnHasInk[x] && blobStart != -1) {
			blobs.push_back({ blobStart, x - blobStart });
			blobStart = -1;
		}
	}
	if (blobStart != -1) {
		blobs.push_back({ blobStart, width - blobStart });
	}

	if (blobs.size() < 2) {
		return input; // nothing to compare against - leave as-is
	}

	const Blob & last = blobs.back();
	const Blob & secondLast = blobs[blobs.size() - 2];

	// Drop the trailing blob only if it's noticeably narrower than its neighbor -
	// real characters are roughly consistent in width; a border fragment isn't.
	if (last.width < secondLast.width * 0.5f) {
		int cutX = std::max(0, last.startX - 3); // small margin before the dropped blob
		ofPixels trimmed;
		input.cropTo(trimmed, 0, 0, cutX, height);
		return trimmed;
	}

	return input;
}

//--------------------------------------------------------------
void ofApp::processOneImage(const std::string & filename, PlateDetector & detector,
	ofImage & outImage, LicensePlate & outResult,
	std::string & outTesseractText, AccessDecision & outDecision, bool accessListReady) {

	if (!outImage.load(filename)) {
		ofLogError("ofApp") << "Could not load test image: " << filename;
		return;
	}
	outImage.setImageType(OF_IMAGE_COLOR);

	outResult = detector.process(outImage.getPixels());

	if (!outResult.isValid) {
		ofLogNotice("Detection") << "[" << filename << "] No plate detected.";
		return;
	}

	ofLogNotice("Detection") << "[" << filename << "] Plate found at ("
							 << outResult.boundingBox.x << ", " << outResult.boundingBox.y << ") size "
							 << outResult.boundingBox.width << "x" << outResult.boundingBox.height
							 << " customText=\"" << outResult.plateText << "\"";

	// Run Tesseract on the detected plate crop, with any color strip trimmed off the
	// left edge first (stripWidthPx is 0 for strategies with no such strip, e.g.
	// Indian plates - trimAmount then naturally becomes 0, a no-op).
	ofPixels platePixels = outResult.croppedPlate.getPixels();
	int trimAmount = static_cast<int>(outResult.stripWidthPx * 1.15f);
	trimAmount = std::min(trimAmount, static_cast<int>(platePixels.getWidth()) - 1);

	ofPixels forOcr;
	if (trimAmount > 0) {
		platePixels.cropTo(forOcr, trimAmount, 0,
			platePixels.getWidth() - trimAmount, platePixels.getHeight());
	} else {
		forOcr = platePixels;
	}

	forOcr = trimStrayEdgeBlob(forOcr);
	forOcr = tightCropToInkWithPadding(forOcr, 5);

	outTesseractText = tesseractReader.recognize(forOcr);
	ofLogNotice("Tesseract") << "[" << filename << "] (trimmed " << trimAmount
							 << "px strip) tesseractText=\"" << outTesseractText << "\"";

	if (accessListReady) {
		outDecision = accessController.evaluate(outTesseractText);
		accessLog.record(outDecision);
	}
}

//--------------------------------------------------------------

void ofApp::setup() {

	ofSetWindowTitle("PlateDetector - Multi-Region Testing (EU & India)");

	// --- On-Demand Image Loading Architecture ---
	// Startup image loading is intentionally bypassed to maintain a non-blocking initial state.
	// Images are loaded dynamically via system file picker or drag-and-drop upon user interaction.

	// --- Polymorphic Logging Initialization ---
	// Enable openFrameworks log output
	ofSetLogLevel(OF_LOG_NOTICE);

	ofLogNotice("ofApp")
		<< "Logging initialized";

	loggers.push_back(std::make_shared<ConsoleAccessLogger>());
	loggers.push_back(std::make_shared<EmailSecurityAlert>());

	// --- Initialize Smart Pointers & Strategy ---
	euStrategy = std::make_shared<EUDetectionStrategy>();
	indianStrategy = std::make_shared<IndianDetectionStrategy>();

	// --- Initialize Tesseract. Build the tessdata path from the exe's actual directory
	// (ofFilePath::getCurrentExeDir()) rather than a bare relative "tessdata" string -
	// a relative path depends on the working directory VS's debugger happens to be
	// using, which doesn't always match bin/ as expected. This resolves correctly
	// regardless of that setting. ---
	std::string tessdataPath = ofFilePath::getCurrentExeDir() + "tessdata";
	ofLogNotice("ofApp") << "Looking for tessdata at: " << tessdataPath;
	bool tesseractReady = tesseractReader.init(tessdataPath, "eng");
	if (!tesseractReady) {
		ofLogWarning("ofApp") << "Tesseract failed to initialize - Tesseract results will be empty. "
							  << "Check that bin/tessdata/eng.traineddata exists.";
	}

	// --- Load the authorized plate list for Phase 3 access control ---
	bool accessListReady = accessController.loadAuthorizedPlates("users.csv");
	if (!accessListReady) {
		ofLogWarning("ofApp") << "Authorized plate list failed to load - "
							  << "check that bin/data/users.csv exists and has plates in its first column.";
	}

	// --- Set detector back to starting mode ---
	if (currentMode == MODE_EU) {
		detector.setStrategy(euStrategy);
	} else {
		detector.setStrategy(indianStrategy);
	}

	// Activate start screen gatekeeper overlay
	startScreen.setActive(true);
}
//--------------------------------------------------------------
// Handles state updates and responds to active StartScreen action triggers
void ofApp::update() {
}
//--------------------------------------------------------------
void ofApp::draw() {
	ofBackground(30);
	ofSetColor(255);

	// 1. If the start screen is active, draw it exclusively and return early
	if (startScreen.isActive()) {
		startScreen.draw();
		return;
	}

	// Safety check: ensure images are loaded properly
	if (euImages.empty() || currentIndex >= (int)euImages.size()) {
		ofDrawBitmapStringHighlight("No test images loaded.", 20, 30);
		return;
	}

	if (currentIndex < 0) {
		currentIndex = 0;
	}

	float margin = 15;
	float availableWidth = ofGetWidth() - margin * 2;
	float yCursor = 50;

	// --- Top Status Banner ---
	std::string modeStr = (currentMode == MODE_EU) ? "EU" : (currentMode == MODE_INDIAN ? "INDIAN" : "BOTH");
	std::string header = "Keys: [1] EU | [2] Indian | [3] Both  -->  Mode: " + modeStr
		+ " | Image " + ofToString(currentIndex + 1) + "/" + ofToString(euImages.size())
		+ " (" + testImageFilenames[currentIndex] + ") [<- / ->]";
	ofDrawBitmapStringHighlight(header, margin, 25, ofColor::black, ofColor::yellow);

	// --- GarageUI Dashboard Integration ---
	// Check if access was granted for the currently displayed image
	bool gateIsOpen = (currentIndex < (int)accessDecisions.size()) && accessDecisions[currentIndex].granted;

	// Render the visual gate status indicator panel (GATE OPEN / GATE CLOSED)
	garageUI.drawGateStatus(gateIsOpen, margin, yCursor, 200, 40);

	// Render the interactive strategy toggle UI buttons
	garageUI.drawStrategyToggle({ "EU", "Indian" }, margin + 220, yCursor);
	yCursor += 55;

	// --- Image Rendering & Bounding Boxes ---
	std::string currentFilename = testImageFilenames[currentIndex];
	ofImage & img = euImages[currentIndex];
	LicensePlate & euRes = euResults[currentIndex];
	LicensePlate & inRes = indianResults[currentIndex];

	if (!img.isAllocated()) {
		ofSetColor(255, 0, 0);
		ofDrawBitmapString("Failed to load image: " + currentFilename, margin, yCursor + 20);
		return;
	}

	// --- Height-budget-aware scaling: shrink the image (preserving aspect ratio) if
	// its natural width-fit height would overflow the remaining window space. Fixes a
	// regression where a tall source photo could push the crop thumbnail and text
	// lines off-screen entirely (seen earlier in testing on a full car-rear photo). ---
	float naturalImageHeight = img.getHeight() * (availableWidth / img.getWidth());
	float estimatedTextHeight = 90.0f; // ~3 text lines + the crop thumbnail below
	float imageBudget = (ofGetHeight() - yCursor - margin) - estimatedTextHeight;
	float imageScaleFactor = 1.0f;
	if (imageBudget > 0 && naturalImageHeight > imageBudget) {
		imageScaleFactor = imageBudget / naturalImageHeight;
	}
	float drawWidth = availableWidth * imageScaleFactor;

	// Scale image proportionally to fit within window margins
	float scale = drawWidth / img.getWidth();
	float h = img.getHeight() * scale;
	img.draw(margin, yCursor, drawWidth, h);

	ofNoFill();
	ofSetLineWidth(3);

	// Render EU Bounding Box (Blue)
	if ((currentMode == MODE_EU || currentMode == MODE_BOTH) && euRes.isValid) {
		ofSetColor(0, 100, 255);
		ofDrawRectangle(
			margin + euRes.boundingBox.x * scale,
			yCursor + euRes.boundingBox.y * scale,
			euRes.boundingBox.width * scale,
			euRes.boundingBox.height * scale);
		ofDrawBitmapString("EU Plate", margin + euRes.boundingBox.x * scale, yCursor + euRes.boundingBox.y * scale - 5);
	}

	// Render Indian Bounding Box (Green)
	if ((currentMode == MODE_INDIAN || currentMode == MODE_BOTH) && inRes.isValid) {
		ofSetColor(0, 255, 0);
		ofDrawRectangle(
			margin + inRes.boundingBox.x * scale,
			yCursor + inRes.boundingBox.y * scale,
			inRes.boundingBox.width * scale,
			inRes.boundingBox.height * scale);
		ofDrawBitmapString("Indian HSRP", margin + inRes.boundingBox.x * scale, yCursor + inRes.boundingBox.y * scale - 5);
	}

	yCursor += h + margin;
	ofSetLineWidth(1);

	// --- Detection & Access Decision Results ---
	LicensePlate & activePlate = inRes.isValid ? inRes : euRes;

	// --- Restored: intermediate CV step thumbnail (binarized/cropped plate) - was
	// present in the EU-branch PlateDisplayer version but missing from this rewrite;
	// this is an explicit Phase 3 deliverable ("display intermediate CV step
	// thumbnails"), so it's added back here rather than left out for the demo. ---
	if (activePlate.isValid && activePlate.croppedPlate.isAllocated()) {
		ofSetColor(255);
		float cropScale = drawWidth / activePlate.croppedPlate.getWidth();
		float cropHeight = activePlate.croppedPlate.getHeight() * cropScale;
		activePlate.croppedPlate.draw(margin, yCursor, drawWidth, cropHeight);
		yCursor += cropHeight + margin;
	}

	if (activePlate.isValid) {
		ofSetColor(0, 255, 0);
		ofDrawBitmapStringHighlight("Custom matcher: " + activePlate.plateText, margin, yCursor + 10);
		yCursor += 25;

		ofSetColor(255, 200, 0);
		std::string tessText = (currentIndex < (int)tesseractResults.size()) ? tesseractResults[currentIndex] : "";
		ofDrawBitmapStringHighlight("Tesseract:      " + tessText, margin, yCursor + 10);
		yCursor += 25;

		if (currentIndex < (int)accessDecisions.size()) {
			const AccessDecision & decision = accessDecisions[currentIndex];
			if (decision.granted) {
				ofDrawBitmapStringHighlight("ACCESS GRANTED - Welcome, " + decision.ownerName
						+ " (plate \"" + decision.matchedPlate + "\")",
					margin, yCursor + 10, ofColor::green, ofColor::black);
			} else {
				std::string reason = decision.matchedPlate.empty()
					? "no close match found"
					: "closest was \"" + decision.matchedPlate + "\" (edit distance " + ofToString(decision.editDistance) + ")";
				ofDrawBitmapStringHighlight("ACCESS DENIED - " + reason, margin, yCursor + 10, ofColor::red, ofColor::white);
			}
		}
	} else {
		ofDrawBitmapStringHighlight("No plate detected for current active mode.", margin, yCursor + 15, ofColor::red, ofColor::white);
	}
}
//--------------------------------------------------------------
void ofApp::keyPressed(int key) {
	std::cout << "KEYPRESSED" << std::endl;

	// 1. Mode switching
	if (key == '1') {
		currentMode = MODE_EU;
		if (euStrategy) detector.setStrategy(euStrategy);
		ofLogNotice("ofApp") << "Switched to MODE_EU";
	} else if (key == '2') {
		currentMode = MODE_INDIAN;
		if (indianStrategy) detector.setStrategy(indianStrategy);
		ofLogNotice("ofApp") << "Switched to MODE_INDIAN";
	} else if (key == '3') {
		currentMode = MODE_BOTH;
		ofLogNotice("ofApp") << "Switched to MODE_BOTH";
	}

	// 2. Image navigation
	if (euImages.empty()) {
		return;
	}

	if (key == OF_KEY_RIGHT) {
		currentIndex = (currentIndex + 1) % euImages.size();
	} else if (key == OF_KEY_LEFT) {
		currentIndex = (currentIndex - 1 + euImages.size()) % euImages.size();
	}

	if (key == OF_KEY_RIGHT || key == OF_KEY_LEFT) {

		if (currentIndex >= 0 && currentIndex < (int)accessDecisions.size()) {

			std::string currentPlate = (currentIndex < (int)tesseractResults.size())
				? tesseractResults[currentIndex]
				: "UNKNOWN";

			for (auto & logger : loggers) {
				logger->logAccess(
					currentPlate,
					accessDecisions[currentIndex].granted);
			}
		}
	}
}

//--------------------------------------------------------------
void ofApp::keyReleased(int key) {
}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y) {
}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button) {
}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button) {
	if (startScreen.isActive()) {
		startScreen.mousePressed(x, y, button);

		// Uses the new single import trigger instead of the old default/custom options
		if (startScreen.isImportTriggered()) {
			startScreen.resetTrigger();

			ofFileDialogResult result = ofSystemLoadDialog("Select License Plate Image", false);
			if (result.bSuccess) {
				startScreen.setActive(false);
				loadSingleImageFromPath(result.getPath());
			}
		}
		return;
	}

	garageUI.handleMousePressed(x, y);
}
//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button) {
}

//--------------------------------------------------------------
void ofApp::mouseEntered(int x, int y) {
}

//--------------------------------------------------------------
void ofApp::mouseExited(int x, int y) {
}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h) {
}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg) {
}

//--------------------------------------------------------------
// Handles drag-and-drop events for importing image files directly into the application
void ofApp::dragEvent(ofDragInfo dragInfo) {
	if (dragInfo.files.size() > 0) {
		// Dismiss the start screen gatekeeper if it's still active
		if (startScreen.isActive()) {
			startScreen.setActive(false);
		}
		// Load and process the dropped image instantly (works anytime in dashboard too)
		loadSingleImageFromPath(dragInfo.files[0].string());
	}
}

//--------------------------------------------------------------
// Imports a custom image file and runs the dual EU/Indian detection pipeline + OCR
void ofApp::loadSingleImageFromPath(const std::string & path) {
	ofImage newImg;
	if (!newImg.load(path)) {
		ofLogError("ofApp") << "Failed to load image from path: " << path;
		return;
	}

	testImageFilenames.push_back(path);
	euImages.push_back(newImg);

	// Dynamic sizing for results vectors
	euResults.resize(testImageFilenames.size());
	indianResults.resize(testImageFilenames.size());
	tesseractResults.resize(testImageFilenames.size());
	accessDecisions.resize(testImageFilenames.size());

	size_t newIdx = testImageFilenames.size() - 1;

	// Run pipeline for newly added image
	if (euStrategy) {
		detector.setStrategy(euStrategy);
		euResults[newIdx] = detector.process(newImg.getPixels());
	}

	if (indianStrategy) {
		detector.setStrategy(indianStrategy);
		indianResults[newIdx] = detector.process(newImg.getPixels());
	}

	// --- OCR & Access Control Integration ---
	LicensePlate & activePlate = indianResults[newIdx].isValid ? indianResults[newIdx] : euResults[newIdx];

	if (activePlate.isValid && activePlate.croppedPlate.isAllocated() && activePlate.croppedPlate.getWidth() > 0) {
		ofPixels platePixels = activePlate.croppedPlate.getPixels();

		int imgWidth = platePixels.getWidth();
		int imgHeight = platePixels.getHeight();

		int trimAmount = std::max(0, static_cast<int>(activePlate.stripWidthPx * 1.15f));
		trimAmount = std::min(trimAmount, imgWidth - 1);

		ofPixels forOcr;
		if (trimAmount > 0 && (imgWidth - trimAmount) > 0) {
			platePixels.cropTo(forOcr, trimAmount, 0, imgWidth - trimAmount, imgHeight);
		} else {
			forOcr = platePixels;
		}

		if (forOcr.isAllocated() && forOcr.getWidth() > 0) {
			forOcr = trimStrayEdgeBlob(forOcr);
			forOcr = tightCropToInkWithPadding(forOcr, 5);

			tesseractResults[newIdx] = tesseractReader.recognize(forOcr);
			accessDecisions[newIdx] = accessController.evaluate(tesseractResults[newIdx]);
			accessLog.record(accessDecisions[newIdx]);

			for (auto & logger : loggers) {
				logger->logAccess(tesseractResults[newIdx], accessDecisions[newIdx].granted);
			}
		}
	}

	// Switch view to the newly imported image
	currentIndex = (int)newIdx;
	ofLogNotice("ofApp") << "Loaded, processed and evaluated image: " << path;
}
