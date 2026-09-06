#pragma once

#include "LicensePlate.h"
#include "PlateDetectionStrategy.h"
#include "ofMain.h"

class IndianDetectionStrategy : public PlateDetectionStrategy {
public:
	IndianDetectionStrategy() = default;
	~IndianDetectionStrategy() override = default;

	// main method from the interface
	LicensePlate detect(const ofPixels & input) override;

private:
	// support method for phase 1 of the pipeline
	ofPixels filterPlateColor(const ofPixels & input);
	ofRectangle findPlateBoundingBox(const ofPixels & thresholdedImage);
	ofPixels binarizeROI(const ofPixels & croppedROI);
};
