#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup() {

	ofSetWindowTitle("EU Plate Detection - Debug View");

	// Point this at your EU plate test image in bin/data/images/
	testImage.load("images/EU_DE.jpg");
	testImage.setImageType(OF_IMAGE_COLOR); // force 3-channel RGB (no alpha)

	if (testImage.isAllocated()) {

	// --- DEBUG: generate the intermediate blue-strip mask for visualization ---
	ofPixels mask = euStrategy.filterBlueStrip(testImage.getPixels());
	debugMask.setFromPixels(mask);

	result = euStrategy.detect(testImage.getPixels());

	if (result.isValid) {
		ofLogNotice("EUDetection") << "Plate found at ("
								   << result.boundingBox.x << ", " << result.boundingBox.y << ") size "
								   << result.boundingBox.width << "x" << result.boundingBox.height;

		// --- DEBUG: confirm the actual pixel dimensions stored in the ofImage itself ---
		ofLogNotice("EUDetection") << "croppedPlate ofImage actual size: "
								   << result.croppedPlate.getWidth() << "x" << result.croppedPlate.getHeight();
	} else {
		ofLogNotice("EUDetection") << "No plate detected in test image.";
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
}

//--------------------------------------------------------------
void ofApp::update() {
}

//--------------------------------------------------------------
void ofApp::draw() {
	ofBackground(30);
	ofSetColor(255); // reset color state every frame - prevents red tint bleeding across draws

	// --- Layout: scale every panel to fit inside the window, stacked with labels ---
	float margin = 15;
	float availableWidth = ofGetWidth() - margin * 2;

	// original image & layout variables
	float scale1 = availableWidth / testImage.getWidth();
	float h1 = testImage.getHeight() * scale1;
	testImage.draw(margin, margin, availableWidth, h1);

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
	}

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
	}

	// --- 4. Top Status Banner ---
	std::string modeStr = (currentMode == MODE_EU) ? "EU" : (currentMode == MODE_INDIAN ? "INDIAN" : "BOTH");
	ofDrawBitmapStringHighlight("Key 1: EU | Key 2: Indian | Key 3: Both  -->  Active Mode: " + modeStr, 20, 25);
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key) {
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
