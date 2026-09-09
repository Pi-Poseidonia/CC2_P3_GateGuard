#include "IndianDetectionStrategy.h"

//Executes the three-stage computer vision detection pipeline for Indian HSRP license plates.
//The raw input frame referenced directly via ofPixels.
//LicensePlate Struct containing the bounding box, processed ROI, and validity flag.

LicensePlate IndianDetectionStrategy::detect(const ofPixels & input) {
	LicensePlate result;

	// Safety check: verify allocation of the incoming frame buffer
	if (!input.isAllocated()) {
		result.isValid = false;
		return result;
	}

	// Stage 1: Color segmentation targeting private HSRP white plate regions
	ofPixels colorFiltered = filterPlateColor(input);

	// Stage 2: Geometric analysis to compute bounding rectangle of active candidate pixels
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

// support method for phase 1 of the pipeline: color filtering to detect white HSRP plates
//Filters white pixels characteristic of private Indian High Security Registration Plates (HSRP).
//Applies a combined luminance and low - chrominance threshold on RGB channels.
										
ofPixels IndianDetectionStrategy::filterPlateColor(const ofPixels & input) {
	ofPixels output;
	// create greyscale-pixel-object of tsame seize
	output.allocate(input.getWidth(), input.getHeight(), OF_IMAGE_GRAYSCALE);

	int width = input.getWidth();
	int height = input.getHeight();
	int numChannels = input.getNumChannels(); // usuall 3 (RGB) or 4 (RGBA)

	// Fallback if image has fewer than 3 channels
	if (numChannels < 3) {
		return input;
	}

	// 1. Nested For-loop: Iterate over each pixel in the input image
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {

			// Calculate memory index for RGB/RGBA buffer
			int index = (y * width + x) * numChannels;

			int r = input[index];
			int g = input[index + 1];
			int b = input[index + 2];

			// 2. main if-condition for white HSRP plates
			bool isHighBrightness = (r > 160 && g > 160 && b > 160);
			bool isLowSaturation = (std::abs(r - g) < 25 && std::abs(r - b) < 25 && std::abs(g - b) < 25);

			// 3. Write result into binary mask
			if (isHighBrightness && isLowSaturation) {
				output[y * width + x] = 255; // White HSRP plate: white
			} else {
				output[y * width + x] = 0; // Background pixel: Black
			}
		}
	}

	return output;
}

// support method for phase 2 of the pipeline: find bounding box of candidate plate
//Binarizes the extracted Region of Interest (ROI) to maximize character edge separation.

ofRectangle IndianDetectionStrategy::findPlateBoundingBox(const ofPixels & thresholdedImage) {
	int width = thresholdedImage.getWidth();
	int height = thresholdedImage.getHeight();

	int minX = width;
	int minY = height;
	int maxX = 0;
	int maxY = 0;
	bool foundAny = false;

	// Scan binary mask to find extreme coordinates of active pixels (255 = White)
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

	int boxWidth = maxX - minX;
	int boxHeight = maxY - minY;

	// 1. Minimum Size Check: Reject tiny noise artifacts
	if (boxWidth < 40 || boxHeight < 10) {
		return ofRectangle(0, 0, 0, 0);
	}

	// 2. Aspect Ratio Check: Indian HSRP plates are wide rectangles (~3:1 to 5:1 ratio)
	float aspectRatio = static_cast<float>(boxWidth) / static_cast<float>(boxHeight);

	// Reject shapes that are square or vertically elongated (e.g., headlights or background noise)
	if (aspectRatio < 2.0f || aspectRatio > 6.0f) {
		return ofRectangle(0, 0, 0, 0);
	}

	// Return valid bounding box for the detected plate region
	return ofRectangle(minX, minY, boxWidth, boxHeight);
}

// support method for phase 3 of the pipeline: binarize the cropped ROI for OCR
ofPixels IndianDetectionStrategy::binarizeROI(const ofPixels & croppedROI) {
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
