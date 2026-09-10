#pragma once

#include "LicensePlate.h"
#include "PlateDetectionStrategy.h"
#include "ofMain.h"

// Detection strategy for EU-standard license plates.
// EU plates carry a narrow blue strip (with the EU stars / country code)
// along the left edge of the plate, in contrast to the Indian HSRP white-plate approach.
class EUDetectionStrategy : public PlateDetectionStrategy {
public:
	EUDetectionStrategy() = default;
	~EUDetectionStrategy() override = default;

	// main method from the interface
	LicensePlate detect(const ofPixels & input) override;

	// --- DEBUG: temporarily made public so ofApp can visualize the intermediate mask ---
	// support method for phase 1 of the pipeline: locate the blue Euro-strip
	ofPixels filterBlueStrip(const ofPixels & input);

private:
	// support method for phase 2 of the pipeline: derive full plate bounding box
	// from the blue strip's position (the strip only covers the left portion of the plate).
	// outStripWidth receives the strip's own detected width (px), so a caller can trim
	// it out of the resulting crop before running external OCR on the plate.
	ofRectangle findPlateBoundingBox(const ofPixels & thresholdedImage, int & outStripWidth);

	// support method for phase 3 of the pipeline: binarize the cropped ROI for OCR
	ofPixels binarizeROI(const ofPixels & croppedROI);
};
