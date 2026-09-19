#include "StartScreen.h"

//--------------------------------------------------------------
void StartScreen::draw() {
	if (!active) return;

	// Dark semi-transparent overlay background
	ofSetColor(20, 20, 30, 240);
	ofDrawRectangle(0, 0, ofGetWidth(), ofGetHeight());

	// Main Card Frame
	int cardWidth = 680;
	int cardHeight = 340;
	int cardX = (ofGetWidth() - cardWidth) / 2;
	int cardY = (ofGetHeight() - cardHeight) / 2;

	ofSetColor(40, 45, 60);
	ofDrawRectRounded(cardX, cardY, cardWidth, cardHeight, 10);

	// Header Accent Line
	ofSetColor(0, 200, 255);
	ofDrawRectangle(cardX, cardY, cardWidth, 6);

	// Title Text - Scaled up for a larger, bold appearance
	ofSetColor(255);
	ofPushMatrix();
	float scaleFactor = 1.6f;
	ofScale(scaleFactor, scaleFactor);
	ofDrawBitmapString("GATEGUARD ANPR SYSTEM", (cardX + 30) / scaleFactor, (cardY + 50) / scaleFactor);
	ofPopMatrix();

	ofSetColor(200, 220, 255);
	ofDrawBitmapString("Welcome! Please select an image to begin license plate analysis.", cardX + 30, cardY + 95);

	// --- Central Action Button: Browse & Import ---
	btnImport.set(cardX + 40, cardY + 140, cardWidth - 80, 60);
	ofSetColor(0, 180, 120);
	ofDrawRectRounded(btnImport, 8);
	ofSetColor(255);
	ofDrawBitmapStringHighlight("BROWSE & IMPORT IMAGE FILE", btnImport.x + 180, btnImport.y + 35);

	// Footer Hint
	ofSetColor(160, 170, 190);
	ofDrawBitmapString("Tip: Alternatively, drag & drop an image file directly into this window.", cardX + 30, cardY + 280);
}

//--------------------------------------------------------------
void StartScreen::mousePressed(int x, int y, int button) {
	if (!active) return;

	// Check if the user clicked inside the import action button
	if (btnImport.inside(x, y)) {
		importTriggered = true;
	}
}
