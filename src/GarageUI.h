#pragma once

#include "ofMain.h"
#include <string>
#include <vector>

// Renders the gate status indicator (green = OPEN, red = CLOSED) and a row of
// strategy-toggle buttons (e.g. "EU", "Indian"). Deliberately decoupled from any
// actual PlateDetectionStrategy object - it only tracks and reports a selected name
// as a plain string, so it compiles and works regardless of which concrete strategy
// classes happen to be present in the project at any given time (e.g. if
// IndianDetectionStrategy isn't yet merged into this branch). ofApp is responsible
// for deciding what to actually do with the selected name.
class GarageUI {
public:
	// Draws the gate status panel: a filled green "GATE OPEN" or red "GATE CLOSED"
	// indicator, sized to the given rectangle.
	void drawGateStatus(bool isOpen, float x, float y, float width, float height = 50);

	// Draws one button per name in strategyNames, highlighting whichever is currently
	// selected. Must be called every frame - it's what records each button's current
	// screen position, which handleMousePressed() then hit-tests against. The first
	// call also picks strategyNames[0] as the initial selection if none is set yet.
	void drawStrategyToggle(const std::vector<std::string> & strategyNames, float x, float y);

	// Call from ofApp::mousePressed() with the click coordinates - updates the
	// selected strategy if the click landed on a button drawn by the most recent
	// drawStrategyToggle() call.
	void handleMousePressed(int mx, int my);

	std::string getSelectedStrategy() const;

private:
	struct Button {
		std::string name;
		ofRectangle bounds;
	};

	std::vector<Button> buttons;
	std::string selectedStrategy;
};
