#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup() {

	ofSetWindowTitle("PlateDetector - EU Testing (multiple images)");

	// --- Load character templates once, shared across all test images ---
	euDetector.loadTemplates("alphabet/");

	if (testImage.isAllocated()) {

	// --- DEBUG: generate the intermediate blue-strip mask for visualization ---
	ofPixels mask = euStrategy.filterBlueStrip(testImage.getPixels());
	debugMask.setFromPixels(mask);

	// --- Load and process every test image up front ---
	euImages.resize(testImageFilenames.size());
	euResults.resize(testImageFilenames.size());

	for (size_t i = 0; i < testImageFilenames.size(); i++) {
		ofImage & img = euImages[i];

		if (img.load(testImageFilenames[i])) {
			img.setImageType(OF_IMAGE_COLOR);

			euResults[i] = euDetector.process(img.getPixels());

			if (euResults[i].isValid) {
				ofLogNotice("EUDetection") << "[" << testImageFilenames[i] << "] Plate found at ("
										   << euResults[i].boundingBox.x << ", " << euResults[i].boundingBox.y << ") size "
										   << euResults[i].boundingBox.width << "x" << euResults[i].boundingBox.height
										   << " text=\"" << euResults[i].plateText << "\"";
			} else {
				ofLogNotice("EUDetection") << "[" << testImageFilenames[i] << "] No plate detected.";
			}
		} else {
			ofLogError("ofApp") << "Could not load test image: " << testImageFilenames[i];
		}
	}

// --- Indian Plate Testing ---
		indianResult = indianStrategy.detect(testImage.getPixels());

		if (indianResult.isValid) {
			ofLogNotice("IndianDetection") << "Indian HSRP Plate found at ("
										   << indianResult.boundingBox.x << ", " << indianResult.boundingBox.y << ") size "
										   << indianResult.boundingBox.width << "x" << indianResult.boundingBox.height;
		} else {
			ofLogNotice("IndianDetection") << "No Indian plate detected.";
		}

	} else {
		ofLogError("ofApp") << "Could not load test image from bin/data/images/";
	}

	ofLogNotice("ofApp") << "Loaded " << testImageFilenames.size() << " test image(s). "
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

	float margin = 15;
	float availableWidth = ofGetWidth() - margin * 2;
	float yCursor = margin;


	// original image & layout variables
	float scale1 = availableWidth / testImage.getWidth();
	float h1 = testImage.getHeight() * scale1;
	testImage.draw(margin, margin, availableWidth, h1);

	std::string header = "Image " + ofToString(currentIndex + 1) + " / " + ofToString(euImages.size())
		+ "   (" + testImageFilenames[currentIndex] + ")   [<- / -> to switch]";
	ofDrawBitmapStringHighlight(header, margin, yCursor + 10);
	yCursor += 25;

	ofImage & img = euImages[currentIndex];
	LicensePlate & result = euResults[currentIndex];

	if (!img.isAllocated()) {
		ofSetColor(255, 0, 0);
		ofDrawBitmapString("Failed to load this image - check the filename/path.", margin, yCursor + 20);
		return;
	}

	ofSetColor(255);
	float scale = availableWidth / img.getWidth();
	float h = img.getHeight() * scale;
	img.draw(margin, yCursor, availableWidth, h);

	// --- 1. EU Bounding Box (for EU mode and both modes) ---
	if ((currentMode == MODE_EU || currentMode == MODE_BOTH) && result.isValid) {
		ofNoFill();
		ofSetLineWidth(3);

		ofSetColor(0, 100, 255); // blue EU
		ofDrawRectangle(
			margin + result.boundingBox.x * scale1,
			margin + result.boundingBox.y * scale1,
			result.boundingBox.width * scale1,
			result.boundingBox.height * scale1);
		ofDrawBitmapString("EU Plate", margin + result.boundingBox.x * scale1, margin + result.boundingBox.y * scale1 - 5);

		ofSetColor(0, 100, 255);
		ofDrawRectangle(
			margin + result.boundingBox.x * scale,
			yCursor + result.boundingBox.y * scale,
			result.boundingBox.width * scale,
			result.boundingBox.height * scale);

	}
	yCursor += h + margin;


	// --- 2. Indian Bounding Box (for Indian mode and both modes) ---
	if ((currentMode == MODE_INDIAN || currentMode == MODE_BOTH) && indianResult.isValid) {
		ofNoFill();
		ofSetLineWidth(3);
		ofSetColor(0, 255, 0); // green for India
		ofDrawRectangle(
			margin + indianResult.boundingBox.x * scale1,
			margin + indianResult.boundingBox.y * scale1,
			indianResult.boundingBox.width * scale1,
			indianResult.boundingBox.height * scale1);
		ofDrawBitmapString("Indian HSRP", margin + indianResult.boundingBox.x * scale1, margin + indianResult.boundingBox.y * scale1 - 5);
	}


	float yCursor = margin + h1 + margin;

	// --- 3. Show binarized plate per active mode---
	if ((currentMode == MODE_INDIAN || currentMode == MODE_BOTH) && indianResult.isValid) {
		ofSetColor(255);
		float scale3 = availableWidth / indianResult.croppedPlate.getWidth();
		float h3 = indianResult.croppedPlate.getHeight() * scale3;
		indianResult.croppedPlate.draw(margin, yCursor, availableWidth, h3);
		ofDrawBitmapStringHighlight("[Indian Engine Crop]", margin + 5, yCursor + 15);
	} else if ((currentMode == MODE_EU || currentMode == MODE_BOTH) && result.isValid) {
		ofSetColor(255);
		float scale3 = availableWidth / result.croppedPlate.getWidth();
		float h3 = result.croppedPlate.getHeight() * scale3;
		result.croppedPlate.draw(margin, yCursor, availableWidth, h3);
		ofDrawBitmapStringHighlight("[EU Engine Crop]", margin + 5, yCursor + 15);
	} else {
		ofSetColor(255, 0, 0);
		ofDrawBitmapString("No plate detected for current mode", margin, yCursor + 20);

	if (result.isValid) {
		ofSetColor(255);
		float cropScale = availableWidth / result.croppedPlate.getWidth();
		float cropH = result.croppedPlate.getHeight() * cropScale;
		result.croppedPlate.draw(margin, yCursor, availableWidth, cropH);
		yCursor += cropH + margin;

		ofSetColor(0, 255, 0);
		ofDrawBitmapStringHighlight("Text: " + result.plateText, margin, yCursor + 10);
	} else {
		ofSetColor(255, 0, 0);
		ofDrawBitmapString("No plate detected in this image", margin, yCursor + 20);

	}

	// --- 4. Top Status Banner ---
	std::string modeStr = (currentMode == MODE_EU) ? "EU" : (currentMode == MODE_INDIAN ? "INDIAN" : "BOTH");
	ofDrawBitmapStringHighlight("Key 1: EU | Key 2: Indian | Key 3: Both  -->  Active Mode: " + modeStr, 20, 25);
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
