#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup() {

	ofSetWindowTitle("PlateDetector - EU Testing (multiple images)");

	// --- Load character templates once, shared across all test images ---
	euDetector.loadTemplates("alphabet/");

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

	if (result.isValid) {
		ofNoFill();
		ofSetLineWidth(3);
		ofSetColor(0, 100, 255);
		ofDrawRectangle(
			margin + result.boundingBox.x * scale,
			yCursor + result.boundingBox.y * scale,
			result.boundingBox.width * scale,
			result.boundingBox.height * scale);
	}
	yCursor += h + margin;

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
