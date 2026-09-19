#pragma once

#include "LicensePlate.h"
#include "ofMain.h"

//abstract basis class for detecting license plates in images.
class PlateDetectionStrategy {
public:
	virtual ~PlateDetectionStrategy() = default;

	//pure virtual method. all initialized strategies must implement this function.
	virtual LicensePlate detect(const ofPixels & input) = 0;
};
