#pragma once
#include "ofMain.h"
#include <string>

// datastructure to hold the information of a detected license plate
struct LicensePlate {
	ofImage croppedPlate; // Cropped image of the license plate
	ofRectangle boundingBox; // Position and size in the original image (X, Y, Width, Height)
	std::string plateText; // Recognized text (e.g., "RJ1934KE4598")
	bool isValid = false; // Status indicating whether the recognition was successful
};
