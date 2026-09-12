#include "GarageUI.h"

void GarageUI::drawGateStatus(bool isOpen, float x, float y, float width, float height) {
	ofFill();
	if (isOpen) {
		ofSetColor(0, 200, 0);
	} else {
		ofSetColor(200, 0, 0);
	}
	ofDrawRectangle(x, y, width, height);

	ofSetColor(255);
	std::string label = isOpen ? "GATE OPEN" : "GATE CLOSED";
	// Roughly center the label in the panel - ofBitmapFont characters are ~8px wide.
	float textX = x + (width - label.size() * 8.0f) / 2.0f;
	float textY = y + height / 2.0f + 4.0f;
	ofDrawBitmapString(label, textX, textY);
}

void GarageUI::drawStrategyToggle(const std::vector<std::string> & strategyNames, float x, float y) {
	if (strategyNames.empty()) {
		return;
	}

	if (selectedStrategy.empty()) {
		selectedStrategy = strategyNames[0]; // default to the first option
	}

	buttons.clear();

	const float buttonWidth = 100;
	const float buttonHeight = 32;
	const float gap = 10;

	float cursorX = x;

	for (const std::string & name : strategyNames) {
		ofRectangle bounds(cursorX, y, buttonWidth, buttonHeight);
		buttons.push_back({ name, bounds });

		bool isSelected = (name == selectedStrategy);

		ofFill();
		ofSetColor(isSelected ? ofColor(0, 120, 255) : ofColor(60, 60, 60));
		ofDrawRectangle(bounds);

		ofNoFill();
		ofSetColor(255);
		ofDrawRectangle(bounds); // border, so unselected buttons are still visible as clickable
		ofFill();

		ofSetColor(255);
		float textX = bounds.x + (buttonWidth - name.size() * 8.0f) / 2.0f;
		float textY = bounds.y + buttonHeight / 2.0f + 4.0f;
		ofDrawBitmapString(name, textX, textY);

		cursorX += buttonWidth + gap;
	}
}

void GarageUI::handleMousePressed(int mx, int my) {
	for (const Button & button : buttons) {
		if (button.bounds.inside(mx, my)) {
			selectedStrategy = button.name;
			ofLogNotice("GarageUI") << "Strategy toggle switched to: " << selectedStrategy;
			return;
		}
	}
}

std::string GarageUI::getSelectedStrategy() const {
	return selectedStrategy;
}
