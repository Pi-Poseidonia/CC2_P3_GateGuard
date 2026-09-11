#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup() {

ofSetWindowTitle("PlateDetector - Multi-Region Testing (EU & India)");

	// --- 1. Initialize Smart Pointers & Strategy ---
	euStrategy = std::make_shared<EUDetectionStrategy>();
	indianStrategy = std::make_shared<IndianDetectionStrategy>();

	// Set default active strategy and load OCR templates
	detector.setStrategy(indianStrategy);
	detector.loadTemplates("alphabet/");

	// --- 2. Resize result vectors ---
	euImages.resize(testImageFilenames.size());
	euResults.resize(testImageFilenames.size());
	indianResults.resize(testImageFilenames.size());

	// --- 3. Load and process every test image for both regions ---
	for (size_t i = 0; i < testImageFilenames.size(); i++) {
		ofImage & img = euImages[i];

		if (img.load(testImageFilenames[i])) {
			img.setImageType(OF_IMAGE_COLOR);

			// Run EU detection & OCR
			detector.setStrategy(euStrategy);
			euResults[i] = detector.process(img.getPixels());

			// Run Indian detection & OCR
			detector.setStrategy(indianStrategy);
			indianResults[i] = detector.process(img.getPixels());

			// Logging
			ofLogNotice("Detection") << "[" << testImageFilenames[i] << "]"
									 << " EU Text: \"" << euResults[i].plateText << "\""
									 << " | Indian Text: \"" << indianResults[i].plateText << "\"";
		} else {
			ofLogError("ofApp") << "Could not load test image: " << testImageFilenames[i];
		}
	}

	// --- 4. Set detector back to starting mode ---
	if (currentMode == MODE_EU) {
		detector.setStrategy(euStrategy);
	} else {
		detector.setStrategy(indianStrategy);
	}

	ofLogNotice("ofApp") << "Loaded " << testImageFilenames.size() << " test image(s). "
						 << "Use LEFT/RIGHT arrow keys to switch between them.";

	//load user data base
	accessManager.loadDatabase("users.csv");
}


//--------------------------------------------------------------
void ofApp::update() {
}

//--------------------------------------------------------------
void ofApp::draw() {
	ofBackground(30);
	ofSetColor(255);

	if (euImages.empty() || currentIndex >= euImages.size()) {
		ofDrawBitmapStringHighlight("No test images loaded.", 20, 30);
		return;
	}

	float margin = 15;
	float availableWidth = ofGetWidth() - margin * 2;
	float yCursor = 50;

	// --- 1. Top Status Banner ---
	std::string modeStr = (currentMode == MODE_EU) ? "EU" : (currentMode == MODE_INDIAN ? "INDIAN" : "BOTH");
	std::string header = "Keys: [1] EU | [2] Indian | [3] Both  -->  Mode: " + modeStr
		+ " | Image " + ofToString(currentIndex + 1) + "/" + ofToString(euImages.size())
		+ " (" + testImageFilenames[currentIndex] + ") [<- / ->]";
	ofDrawBitmapStringHighlight(header, margin, 25, ofColor::black, ofColor::yellow);

	// Extract filename string for database lookup test
	std::string currentFilename = testImageFilenames[currentIndex];

	// Get image and detection results (using indianResult single instance from ofApp.h)
	ofImage & img = euImages[currentIndex];
	LicensePlate & euRes = euResults[currentIndex];
	LicensePlate & inRes = indianResult;

	if (!img.isAllocated()) {
		ofSetColor(255, 0, 0);
		ofDrawBitmapString("Failed to load image: " + currentFilename, margin, yCursor + 20);
		return;
	}

	// --- 2. Draw Original Image & Bounding Boxes ---
	float scale = availableWidth / img.getWidth();
	float h = img.getHeight() * scale;
	img.draw(margin, yCursor, availableWidth, h);

	ofNoFill();
	ofSetLineWidth(3);

	// EU Bounding Box (Blue)
	if ((currentMode == MODE_EU || currentMode == MODE_BOTH) && euRes.isValid) {
		ofSetColor(0, 100, 255);
		ofDrawRectangle(
			margin + euRes.boundingBox.x * scale,
			yCursor + euRes.boundingBox.y * scale,
			euRes.boundingBox.width * scale,
			euRes.boundingBox.height * scale);
		ofDrawBitmapString("EU Plate", margin + euRes.boundingBox.x * scale, yCursor + euRes.boundingBox.y * scale - 5);
	}

	// Indian Bounding Box (Green)
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

	// --- 3. Draw Binarized Crop & Access Control ---
	ofSetLineWidth(1);

	// Indian Crop & Access Control Check
	if ((currentMode == MODE_INDIAN || currentMode == MODE_BOTH) && inRes.isValid) {
		ofSetColor(255);
		if (inRes.croppedPlate.isAllocated()) {
			float cropScale = availableWidth / inRes.croppedPlate.getWidth();
			float cropH = inRes.croppedPlate.getHeight() * cropScale;
			inRes.croppedPlate.draw(margin, yCursor, availableWidth, cropH);
			yCursor += cropH + 5;
		}

		bool isAuthorized = accessManager.isAuthorized(currentFilename);
		std::string statusText = isAuthorized ? "ACCESS GRANTED" : "ACCESS DENIED";
		ofColor statusBgColor = isAuthorized ? ofColor::green : ofColor::red;

		ofDrawBitmapStringHighlight("Indian Security Check: " + statusText, margin, yCursor + 15, statusBgColor, ofColor::white);
		yCursor += 35;
	}

	// EU Crop & Access Control Check
	if ((currentMode == MODE_EU || currentMode == MODE_BOTH) && euRes.isValid) {
		ofSetColor(255);
		if (euRes.croppedPlate.isAllocated()) {
			float cropScale = availableWidth / euRes.croppedPlate.getWidth();
			float cropH = euRes.croppedPlate.getHeight() * cropScale;
			euRes.croppedPlate.draw(margin, yCursor, availableWidth, cropH);
			yCursor += cropH + 5;
		}

		bool isAuthorized = accessManager.isAuthorized(currentFilename);
		std::string statusText = isAuthorized ? "ACCESS GRANTED" : "ACCESS DENIED";
		ofColor statusBgColor = isAuthorized ? ofColor::green : ofColor::red;

		ofDrawBitmapStringHighlight("EU Security Check: " + statusText, margin, yCursor + 15, statusBgColor, ofColor::white);
		yCursor += 35;
	}

	// Display alert if no plate detected in active mode
	bool noPlateFound = (currentMode == MODE_EU && !euRes.isValid) || (currentMode == MODE_INDIAN && !inRes.isValid) || (currentMode == MODE_BOTH && !euRes.isValid && !inRes.isValid);

	if (noPlateFound) {
		ofSetColor(255, 0, 0);
		ofDrawBitmapStringHighlight("No plate detected for current active mode.", margin, yCursor + 15, ofColor::red, ofColor::white);
	}
}
//--------------------------------------------------------------
void ofApp::keyPressed(int key) {
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
