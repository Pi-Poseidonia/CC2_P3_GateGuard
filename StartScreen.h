#pragma once
#include "ofMain.h"

/**
 * UI Component that displays an introduction/welcome screen
 * prompting the user to import a license plate image.
 */
class StartScreen {
public:
	void draw();
	void mousePressed(int x, int y, int button);

	bool isActive() const { return active; }
	void setActive(bool state) { active = state; }

	// Flag to communicate custom file selection trigger to ofApp
	bool importTriggered = false;
	bool isImportTriggered() const { return importTriggered; }

	void resetTrigger() {
		importTriggered = false;
	}

private:
	bool active = true;

	// Single central action button bounds
	ofRectangle btnImport;
};
