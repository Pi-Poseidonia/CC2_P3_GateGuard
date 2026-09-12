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

	ofSetWindowTitle("GateGuard - EU Plate Testing");

	// --- Load character templates once, shared across all test images ---
	euDetector.loadTemplates("alphabet/");

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

	// --- EU images ---
	euImages.resize(euImageFilenames.size());
	euResults.resize(euImageFilenames.size());
	euTesseractResults.resize(euImageFilenames.size());
	euAccessDecisions.resize(euImageFilenames.size());

	for (size_t i = 0; i < euImageFilenames.size(); i++) {
		if (tesseractReady) {
			processOneImage(euImageFilenames[i], euDetector, euImages[i], euResults[i],
				euTesseractResults[i], euAccessDecisions[i], accessListReady);
		}
	}

	ofLogNotice("ofApp") << "Loaded " << euImageFilenames.size() << " test image(s). "
						 << "Use LEFT/RIGHT arrow keys to switch between them.";
}

//--------------------------------------------------------------
void ofApp::update() {
}

//--------------------------------------------------------------
void ofApp::draw() {
	ofBackground(30);
	ofSetColor(255);

	if (euImages.empty()) {
		ofDrawBitmapStringHighlight("No test images loaded.", 20, 30);
		return;
	}
	if (currentIndex >= (int)euImages.size()) {
		currentIndex = 0;
	}

	float margin = 15;
	float availableWidth = ofGetWidth() - margin * 2;
	float yCursor = margin;

	// --- Header: which image, navigation hint ---
	std::string header = "Image " + ofToString(currentIndex + 1) + " / " + ofToString(euImages.size())
		+ "   (" + euImageFilenames[currentIndex] + ")   [<- / -> to switch]";
	ofDrawBitmapStringHighlight(header, margin, yCursor + 10);
	yCursor += 25;

	// --- Phase 4 dashboard: gate status + strategy toggle, side by side ---
	bool gateIsOpen = (currentIndex < (int)euAccessDecisions.size()) && euAccessDecisions[currentIndex].granted;
	garageUI.drawGateStatus(gateIsOpen, margin, yCursor, 200, 40);

	// NOTE: only "EU" is functionally wired up - IndianDetectionStrategy isn't part
	// of this branch (see header comment). The "Indian" button is shown/clickable
	// so the toggle UI is ready, but selecting it currently has no effect.
	garageUI.drawStrategyToggle({ "EU", "Indian" }, margin + 220, yCursor);
	yCursor += 55;

	// --- Per-plate visual pipeline, delegated to PlateDisplayer ---
	ofImage & img = euImages[currentIndex];
	LicensePlate & result = euResults[currentIndex];

	if (!img.isAllocated()) {
		ofSetColor(255, 0, 0);
		ofDrawBitmapString("Failed to load this image - check the filename/path.", margin, yCursor + 20);
		return;
	}

	std::vector<DisplayLine> lines;

	if (result.isValid) {
		lines.push_back({ "Custom matcher: " + result.plateText, ofColor::green });

		std::string tessText = (currentIndex < (int)euTesseractResults.size()) ? euTesseractResults[currentIndex] : "";
		lines.push_back({ "Tesseract:      " + tessText, ofColor(255, 200, 0) });

		if (currentIndex < (int)euAccessDecisions.size()) {
			const AccessDecision & decision = euAccessDecisions[currentIndex];
			if (decision.granted) {
				lines.push_back({ "ACCESS GRANTED  -  Welcome, " + decision.ownerName
						+ "  (plate \"" + decision.matchedPlate
						+ "\", edit distance " + ofToString(decision.editDistance) + ")",
					ofColor::green });
			} else {
				std::string reason = decision.matchedPlate.empty()
					? "no close match found"
					: "closest was \"" + decision.matchedPlate + "\" (edit distance " + ofToString(decision.editDistance) + ")";
				lines.push_back({ "ACCESS DENIED  -  " + reason, ofColor(255, 60, 60) });
			}
		}
	} else {
		lines.push_back({ "No plate detected in this image", ofColor(255, 0, 0) });
	}

	plateDisplayer.draw(img, result, lines, margin, yCursor, availableWidth, ofGetHeight() - yCursor - margin);
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key) {
	if (euImages.empty()) {
		return;
	}

	if (key == OF_KEY_RIGHT) {
		currentIndex = (currentIndex + 1) % euImages.size();
	} else if (key == OF_KEY_LEFT) {
		currentIndex = (currentIndex - 1 + euImages.size()) % euImages.size();
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
void ofApp::dragEvent(ofDragInfo dragInfo) {
}
