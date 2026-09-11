#pragma once
#include "LicensePlate.h"
#include "PlateDetectionStrategy.h"
#include "ofMain.h"

// Detection strategy for Indian HSRP (High Security Registration Plates) standard plates.
// Unlike EU plates which rely on a distinct blue strip, Indian HSRP plates feature a high-contrast
// white background with black characters and an optional subtle IND blue emblem on the left.
class IndianDetectionStrategy : public PlateDetectionStrategy {
public:
	IndianDetectionStrategy() = default;
	~IndianDetectionStrategy() override = default;

	// Main execution entry point inherited from PlateDetectionStrategy
	LicensePlate detect(const ofPixels & input) override;

private:
	// Stage 1: Color/luminance segmentation targeting white HSRP background and IND emblem
	ofPixels filterPlateColor(const ofPixels & input);

	// Stage 2: Connected-component analysis (Flood-Fill BFS) to locate the largest contiguous plate candidate
	ofRectangle findPlateBoundingBox(const ofPixels & thresholdedImage);

	// Stage 3: Dynamic binarization for clean character extraction prior to OCR
	ofPixels binarizeROI(const ofPixels & croppedROI);
};
