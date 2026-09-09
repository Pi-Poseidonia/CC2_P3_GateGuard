#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup() {

	ofSetWindowTitle("EU Plate Detection - Debug View");

	// Point this at your EU plate test image in bin/data/images/
	testImage.load("images/EU_DE.jpg");
	testImage.setImageType(OF_IMAGE_COLOR); // force 3-channel RGB (no alpha)

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

	// Panel 1: original image, scaled to fit window width
	float scale1 = availableWidth / testImage.getWidth();
	float h1 = testImage.getHeight() * scale1;
	testImage.draw(margin, margin, availableWidth, h1);

	if (result.isValid) {
		ofNoFill();
		ofSetColor(255, 0, 0);
		ofDrawRectangle(
			margin + result.boundingBox.x * scale1,
			margin + result.boundingBox.y * scale1,
			result.boundingBox.width * scale1,
			result.boundingBox.height * scale1);
	}

	float yCursor = margin + h1 + margin;

	// Panel 2: debug mask, scaled to fit window width
	ofSetColor(255);
	float scale2 = availableWidth / debugMask.getWidth();
	float h2 = debugMask.getHeight() * scale2;
	debugMask.draw(margin, yCursor, availableWidth, h2);
	yCursor += h2 + margin;

	// Panel 3: cropped/binarized plate, scaled to fit window width
	if (result.isValid) {
		ofSetColor(255);
		float scale3 = availableWidth / result.croppedPlate.getWidth();
		float h3 = result.croppedPlate.getHeight() * scale3;
		result.croppedPlate.draw(margin, yCursor, availableWidth, h3);
	} else {
		ofSetColor(255, 0, 0);
		ofDrawBitmapString("No plate detected", margin, yCursor + 20);
	}
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
