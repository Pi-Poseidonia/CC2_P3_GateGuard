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

	//enables external code to check if a button was clicked
	bool isLoadDefaultTriggered() const { return loadDefaultTriggered; }
	bool isImportCustomTriggered() const { return importCustomTriggered; }

	void resetTriggers() {
		loadDefaultTriggered = false;
		importCustomTriggered = false;
	}

private:
	bool active = true;

	// GUI Button bounds
	ofRectangle btnDefault;
	ofRectangle btnImport;
};
