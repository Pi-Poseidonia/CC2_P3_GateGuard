#include "StartScreen.h"

void StartScreen::draw() {
	if (!active) return;

	// Dark semi-transparent overlay background
	ofSetColor(20, 20, 30, 235);
	ofDrawRectangle(0, 0, ofGetWidth(), ofGetHeight());

	// Main Card Frame
	int cardWidth = 600;
	int cardHeight = 360;
	int cardX = (ofGetWidth() - cardWidth) / 2;
	int cardY = (ofGetHeight() - cardHeight) / 2;

	ofSetColor(40, 45, 60);
	ofDrawRectRounded(cardX, cardY, cardWidth, cardHeight, 10);

	// Header Accent Line
	ofSetColor(0, 200, 255);
	ofDrawRectangle(cardX, cardY, cardWidth, 6);

	// Title Text
	ofSetColor(255);
	ofDrawBitmapStringHighlight("=== ANPR AUTOMATED GATE ACCESS CONTROL ===", cardX + 30, cardY + 45);

	ofSetColor(200, 220, 255);
	ofDrawBitmapString("Select how you want to proceed:", cardX + 30, cardY + 85);

	// --- GUI Button 1: Load Default Pipeline ---
	btnDefault.set(cardX + 40, cardY + 120, 520, 50);
	ofSetColor(0, 160, 220);
	ofDrawRectRounded(btnDefault, 6);
	ofSetColor(255);
	ofDrawBitmapStringHighlight("1. LOAD DEFAULT TEST PIPELINE", btnDefault.x + 20, btnDefault.y + 30);

	// --- GUI Button 2: Import Custom File ---
	btnImport.set(cardX + 40, cardY + 190, 520, 50);
	ofSetColor(0, 180, 120);
	ofDrawRectRounded(btnImport, 6);
	ofSetColor(255);
	ofDrawBitmapStringHighlight("2. BROWSE & IMPORT CUSTOM IMAGE", btnImport.x + 20, btnImport.y + 30);

	// Footer Hint
	ofSetColor(160, 170, 190);
	ofDrawBitmapString("Tip: You can also drag & drop an image file directly into the window.", cardX + 30, cardY + 290);
}

void StartScreen::mousePressed(int x, int y, int button) {
	if (!active) return;

	// Check if the user clicked inside Button 1 (Default Pipeline)
	if (btnDefault.inside(x, y)) {
		loadDefaultTriggered = true;
		active = false; // Dismiss StartScreen
	}
	// Check if the user clicked inside Button 2 (Custom Image Import)
	else if (btnImport.inside(x, y)) {
		importCustomTriggered = true;
		active = false; // Dismiss StartScreen
	}
}
