#pragma once
#include "ofMain.h"
#include <string>

// Data structure representing a detected license plate
struct LicensePlate {
	ofImage croppedPlate; // Cropped image region of the license plate
	ofRectangle boundingBox; // Position and dimensions within the original image (X, Y, W, H)
	std::string plateText; // Recognized text (e.g., "B-MW2026")
	bool isValid = false; // Status flag indicating whether detection was successful
	int stripWidthPx = 0; // Width of the EU color strip in pixels (0 if not applicable)

	// Default constructor
	LicensePlate() = default;

	// Copy constructor ensuring a deep copy of the pixel buffer
	LicensePlate(const LicensePlate & other) {
		isValid = other.isValid;
		boundingBox = other.boundingBox;
		plateText = other.plateText;
		stripWidthPx = other.stripWidthPx;
		if (other.croppedPlate.isAllocated()) {
			croppedPlate.setFromPixels(other.croppedPlate.getPixels());
		}
	}

	// Assignment operator ensuring a deep copy of the pixel buffer
	LicensePlate & operator=(const LicensePlate & other) {
		if (this != &other) {
			isValid = other.isValid;
			boundingBox = other.boundingBox;
			plateText = other.plateText;
			stripWidthPx = other.stripWidthPx;
			if (other.croppedPlate.isAllocated()) {
				croppedPlate.setFromPixels(other.croppedPlate.getPixels());
			} else {
				croppedPlate.clear();
			}
		}
		return *this;
	}
};
