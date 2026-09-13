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

	// --- EU plate testing: multiple images, processed once at startup ---
	testImageFilenames = {
		"images/EU_DE1.jpg",
		"images/EU-DE2.jpg",
		"images/EU-DE3.jpg",
		"images/I_HR26.jpg",
		"images/I_RJ14.jpg",
		"images/I_RJ19.jpg",
		"images/I_TN87.jpg",
		"images/I_MH01.jpg",
		"images/I_MH20.jpg"
	};

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

	// --- Load and process every test image up front ---
	euImages.resize(testImageFilenames.size());
	euResults.resize(testImageFilenames.size());
	indianResults.resize(testImageFilenames.size());
	tesseractResults.resize(testImageFilenames.size());
	accessDecisions.resize(testImageFilenames.size());

	for (size_t i = 0; i < testImageFilenames.size(); i++) {
		ofImage & img = euImages[i];

		// Ensure the image loads properly and is allocated in memory
		if (img.load(testImageFilenames[i]) && img.isAllocated()) {
			img.setImageType(OF_IMAGE_COLOR);

			// 1. Run EU detection strategy
			detector.setStrategy(euStrategy);
			euResults[i] = detector.process(img.getPixels());

			// 2. Run Indian detection strategy
			detector.setStrategy(indianStrategy);
			indianResults[i] = detector.process(img.getPixels());

			// 3. Select active plate (Indian takes priority if valid, otherwise EU)
			LicensePlate & activePlate = indianResults[i].isValid ? indianResults[i] : euResults[i];

			if (activePlate.isValid) {
				ofLogNotice("Detection") << "[" << testImageFilenames[i] << "] Plate found at ("
										 << activePlate.boundingBox.x << ", " << activePlate.boundingBox.y << ") size "
										 << activePlate.boundingBox.width << "x" << activePlate.boundingBox.height
										 << " customText=\"" << activePlate.plateText << "\"";

				// Ensure Tesseract is ready and the cropped plate is valid
				if (tesseractReady && activePlate.croppedPlate.isAllocated() && activePlate.croppedPlate.getWidth() > 0) {
					// .getPixels() is required if croppedPlate is an ofImage
					ofPixels platePixels = activePlate.croppedPlate.getPixels();

					int imgWidth = platePixels.getWidth();
					int imgHeight = platePixels.getHeight();

					// Trim side strips safely (0 for Indian plates since stripWidthPx is 0)
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

						tesseractResults[i] = tesseractReader.recognize(forOcr);
						ofLogNotice("Tesseract") << "[" << testImageFilenames[i] << "] tesseractText=\"" << tesseractResults[i] << "\"";

						// Phase 3: Evaluate against authorized list
						if (accessListReady) {
							accessDecisions[i] = accessController.evaluate(tesseractResults[i]);
							accessLog.record(accessDecisions[i]);
						}
					}
				}
			} else {
				ofLogNotice("Detection") << "[" << testImageFilenames[i] << "] No plate detected.";
			}
		} else {
			ofLogError("ofApp") << "Could not load test image: " << testImageFilenames[i];
		}
	}

	// first image / log all decisions:
	for (size_t i = 0; i < accessDecisions.size(); ++i) {
		std::string plateText = (i < tesseractResults.size()) ? tesseractResults[i] : "UNKNOWN";

		for (auto & logger : loggers) {
			logger->logAccess(plateText, accessDecisions[i].granted);
		}
	}

	// --- Set detector back to starting mode ---
	if (currentMode == MODE_EU) {
		detector.setStrategy(euStrategy);
	} else {
		detector.setStrategy(indianStrategy);
	}

	ofLogNotice("ofApp") << "Loaded " << testImageFilenames.size() << " test image(s). "
						 << "Use LEFT/RIGHT arrow keys to switch between them.";

	startScreen.setActive(true);
}
//--------------------------------------------------------------
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

	// Scale image proportionally to fit within window margins
	float scale = availableWidth / img.getWidth();
	float h = img.getHeight() * scale;
	img.draw(margin, yCursor, availableWidth, h);

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
		// Forward click coordinates to the start screen UI
		startScreen.mousePressed(x, y, button);
	} else {
		// Call from ofApp::mousePressed() with the click coordinates - updates GarageUI button states
		garageUI.handleMousePressed(x, y);
	}
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
