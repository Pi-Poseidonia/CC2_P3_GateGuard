#pragma once
#include "ofMain.h"

/**
 * UI Component that displays an introduction/welcome screen
 * with GUI buttons for mode selection and file imports.
 */
class StartScreen {
public:
	void draw();
	void mousePressed(int x, int y, int button);

	bool isActive() const { return active; }
	void setActive(bool state) { active = state; }

	// Flags to communicate button clicks to ofApp
	bool loadDefaultTriggered = false;
	bool importCustomTriggered = false;

private:
	bool active = true;

	// GUI Button bounds
	ofRectangle btnDefault;
	ofRectangle btnImport;
};
