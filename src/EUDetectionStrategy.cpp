#include "EUDetectionStrategy.h"

//Executes the three-stage computer vision detection pipeline for EU-standard license plates.
//The raw input frame referenced directly via ofPixels.
//LicensePlate Struct containing the bounding box, processed ROI, and validity flag.

LicensePlate EUDetectionStrategy::detect(const ofPixels & input) {
	LicensePlate result;

	// Safety check: verify allocation of the incoming frame buffer
	if (!input.isAllocated()) {
		result.isValid = false;
		return result;
	}

	// Stage 1: Color segmentation targeting the blue Euro-strip on the left edge of the plate
	ofPixels colorFiltered = filterBlueStrip(input);

	// Stage 2: Geometric analysis - derive the full plate bounding box from the strip's position
	ofRectangle plateBox = findPlateBoundingBox(colorFiltered);

	// Stage 3: Region of Interest (ROI) extraction and binarization
	if (plateBox.width > 0 && plateBox.height > 0) {
		ofPixels croppedROI;

		// Memory-efficient crop directly from the input buffer
		input.cropTo(croppedROI, plateBox.x, plateBox.y, plateBox.width, plateBox.height);

		// Enhance contrast for downstream Optical Character Recognition (OCR)
		ofPixels binarizedPlate = binarizeROI(croppedROI);

		// Populate return structure
		result.croppedPlate.setFromPixels(binarizedPlate);
		result.boundingBox = plateBox;
		result.isValid = true;
	} else {
		result.isValid = false;
	}

	return result;
}

// support method for phase 1 of the pipeline: color filtering to detect the blue Euro-strip
//Filters blue pixels characteristic of the EU-strip (stars + country code) on the plate's left edge.
//Applies a hue/dominance threshold: blue channel must clearly dominate red and green.
//
// NOTE: the search is restricted to the left ~25% of the image width. An EU strip is always
// positioned at the plate's left edge, so this is a structural assumption (not just noise
// suppression) that also rejects stray false-positive pixels elsewhere in the frame - e.g.
// JPEG compression artifacts or antialiasing around dark text on a real photo, which can
// otherwise register as "blue-ish" and wreck the bounding box in phase 2.

ofPixels EUDetectionStrategy::filterBlueStrip(const ofPixels & input) {
	ofPixels output;
	// create greyscale-pixel-object of same size
	output.allocate(input.getWidth(), input.getHeight(), OF_IMAGE_GRAYSCALE);

	int width = input.getWidth();
	int height = input.getHeight();
	int numChannels = input.getNumChannels(); // usually 3 (RGB) or 4 (RGBA)

	// Fallback if image has fewer than 3 channels
	if (numChannels < 3) {
		return input;
	}

	// The EU strip never extends past roughly a quarter of the plate's width.
	int maxSearchX = static_cast<int>(width * 0.25f);

	// 1. Nested For-loop: Iterate over each pixel in the input image
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {

			// Skip pixels outside the strip's expected region entirely
			if (x > maxSearchX) {
				output[y * width + x] = 0;
				continue;
			}

			// Calculate memory index for RGB/RGBA buffer
			int index = (y * width + x) * numChannels;

			int r = input[index];
			int g = input[index + 1];
			int b = input[index + 2];

			// 2. main if-condition for the EU blue strip
			// Relaxed dominance thresholds to tolerate glare/washed-out real-world photos.
			bool isBlueDominant = (b > 60 && b > r + 15 && b > g + 10);
			bool isNotTooDark = (b > 40); // reject near-black background/shadow noise

			// 3. Write result into binary mask
			if (isBlueDominant && isNotTooDark) {
				output[y * width + x] = 255; // Euro-strip pixel: white
			} else {
				output[y * width + x] = 0; // Background pixel: black
			}
		}
	}

	return output;
}

// support method for phase 2 of the pipeline: derive full plate bounding box from the blue strip
//The blue strip only spans a narrow column at the plate's left edge (~1/12 of total plate width),
//but shares the same height as the full plate, so the strip's bounding box lets us extrapolate
//the complete plate rectangle using the standard EU plate aspect ratio (~4.7:1).

ofRectangle EUDetectionStrategy::findPlateBoundingBox(const ofPixels & thresholdedImage) {
	int width = thresholdedImage.getWidth();
	int height = thresholdedImage.getHeight();

	int minX = width;
	int minY = height;
	int maxX = 0;
	int maxY = 0;
	bool foundAny = false;

	// Scan binary mask to find extreme coordinates of active (blue-strip) pixels
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			if (thresholdedImage[y * width + x] == 255) {
				if (x < minX) minX = x;
				if (x > maxX) maxX = x;
				if (y < minY) minY = y;
				if (y > maxY) maxY = y;
				foundAny = true;
			}
		}
	}

	// Return empty rectangle if no candidate pixels were found
	if (!foundAny) {
		return ofRectangle(0, 0, 0, 0);
	}

	int stripWidth = maxX - minX;
	int stripHeight = maxY - minY;

	// 1. Minimum Size Check: Reject tiny noise artifacts
	if (stripWidth < 3 || stripHeight < 8) {
		return ofRectangle(0, 0, 0, 0);
	}

	// 2. Sanity Check: the strip itself should be tall and narrow (not wide),
	// since it's only a thin vertical band at the plate's edge.
	float stripAspect = static_cast<float>(stripWidth) / static_cast<float>(stripHeight);
	if (stripAspect > 1.5f) {
		return ofRectangle(0, 0, 0, 0);
	}

	// 3. Extrapolate the full plate rectangle from the strip's height.
	// Standard EU plate ratio is ~520mm x 110mm => aspect ratio ~4.7:1.
	// A small safety margin is added because antialiasing/border rounding on the strip's
	// edges tends to slightly underestimate its true height, which otherwise undershoots
	// the extrapolated width enough to clip the last characters of the plate text.
	const float plateAspectRatio = 4.7f;
	const float widthSafetyMargin = 1.15f;
	int plateHeight = stripHeight;
	int plateWidth = static_cast<int>(plateHeight * plateAspectRatio * widthSafetyMargin);

	// The strip sits at the plate's left edge, so the plate extends rightward from minX.
	int plateX = minX;
	int plateY = minY;

	// Clamp so the extrapolated box never exceeds the image bounds
	if (plateX + plateWidth > width) {
		plateWidth = width - plateX;
	}

	// --- DEBUG: confirm this code path is actually running with the expected numbers ---
	ofLogNotice("EUDetection") << "stripHeight=" << stripHeight
							   << " rawPlateWidth(no margin)=" << static_cast<int>(stripHeight * plateAspectRatio)
							   << " marginedPlateWidth=" << plateWidth
							   << " imageWidth=" << width;

	return ofRectangle(plateX, plateY, plateWidth, plateHeight);
}

// support method for phase 3 of the pipeline: binarize the cropped ROI for OCR
ofPixels EUDetectionStrategy::binarizeROI(const ofPixels & croppedROI) {
	ofPixels binarized;
	binarized.allocate(croppedROI.getWidth(), croppedROI.getHeight(), OF_IMAGE_GRAYSCALE);

	int width = croppedROI.getWidth();
	int height = croppedROI.getHeight();
	int numChannels = croppedROI.getNumChannels();

	// Convert ROI to grayscale and apply global intensity thresholding
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			int index = (y * width + x) * numChannels;

			// Arithmetic mean luminance calculation across channels
			unsigned char gray = (croppedROI[index] + croppedROI[index + 1] + croppedROI[index + 2]) / 3;

			// Static threshold at intensity 110
			binarized[y * width + x] = (gray < 110) ? 0 : 255;
		}
	}

	return binarized;
}
