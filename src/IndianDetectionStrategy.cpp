#include "IndianDetectionStrategy.h"
#include <algorithm>
#include <cmath>
#include <vector>

LicensePlate IndianDetectionStrategy::detect(const ofPixels & input) {
	LicensePlate result;

	if (!input.isAllocated()) {
		result.isValid = false;
		return result;
	}

	// Stage 1: Color segmentation targeting private HSRP white plate regions
	ofPixels colorFiltered = filterPlateColor(input);

	// Stage 2: High-contrast edge density analysis to isolate plate from white cars
	ofRectangle plateBox = findPlateBoundingBox(colorFiltered);

	// Stage 3: Crop and binarize Region of Interest (ROI)
	if (plateBox.width > 0 && plateBox.height > 0) {
		ofPixels croppedROI;
		input.cropTo(croppedROI, plateBox.x, plateBox.y, plateBox.width, plateBox.height);

		ofPixels binarizedPlate = binarizeROI(croppedROI);

		result.croppedPlate.setFromPixels(binarizedPlate);
		result.boundingBox = plateBox;
		result.isValid = true;
	} else {
		result.isValid = false;
	}

	return result;
}

ofPixels IndianDetectionStrategy::filterPlateColor(const ofPixels & input) {
	ofPixels output;
	output.allocate(input.getWidth(), input.getHeight(), OF_IMAGE_GRAYSCALE);

	int width = input.getWidth();
	int height = input.getHeight();
	int numChannels = input.getNumChannels();

	if (numChannels < 3) return input;

	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			int index = (y * width + x) * numChannels;

			int r = input[index];
			int g = input[index + 1];
			int b = input[index + 2];

			// 1. High brightness and low saturation mask for HSRP white background
			bool isHighBrightness = (r > 135 && g > 135 && b > 135);
			bool isLowSaturation = (std::abs(r - g) < 25 && std::abs(r - b) < 25 && std::abs(g - b) < 25);

			// 2. Blue channel boost detection for the mandatory left 'IND' strip
			bool isIndBlue = (b > 100 && b > r + 20 && b > g + 10);

			if ((isHighBrightness && isLowSaturation) || isIndBlue) {
				output[y * width + x] = 255; // White plate area or blue IND strip
			} else {
				output[y * width + x] = 0; // Background pixel
			}
		}
	}

	return output;
}

ofRectangle IndianDetectionStrategy::findPlateBoundingBox(const ofPixels & thresholdedImage) {
	int width = thresholdedImage.getWidth();
	int height = thresholdedImage.getHeight();

	// Calculate horizontal transitions (edges) to separate plates from flat white car bodies
	std::vector<int> edgeTransitionsPerRow(height, 0);

	for (int y = 0; y < height; y++) {
		for (int x = 1; x < width; x++) {
			unsigned char prevPixel = thresholdedImage[y * width + (x - 1)];
			unsigned char currPixel = thresholdedImage[y * width + x];

			// Count black-to-white or white-to-black state switches (character edges)
			if (prevPixel != currPixel) {
				edgeTransitionsPerRow[y]++;
			}
		}
	}

	// Sliding window search for the vertical band with highest edge frequency
	int bestY = 0;
	int maxEdges = 0;
	int windowHeight = 40; // Approximate minimum height of an HSRP plate

	for (int y = 0; y < height - windowHeight; y += 2) {
		int currentEdgeSum = 0;
		for (int h = 0; h < windowHeight; h++) {
			currentEdgeSum += edgeTransitionsPerRow[y + h];
		}

		if (currentEdgeSum > maxEdges) {
			maxEdges = currentEdgeSum;
			bestY = y;
		}
	}

	// Reject regions without significant internal contrast (e.g. smooth bumper)
	if (maxEdges < 50) {
		return ofRectangle(0, 0, 0, 0);
	}

	int minY = bestY;
	int maxY = std::min(height - 1, bestY + windowHeight + 20);

	// Scan X boundaries inside the high-contrast Y band
	int minX = width;
	int maxX = 0;
	bool foundAny = false;

	for (int y = minY; y <= maxY; y++) {
		for (int x = 0; x < width; x++) {
			if (thresholdedImage[y * width + x] == 255) {
				if (x < minX) minX = x;
				if (x > maxX) maxX = x;
				foundAny = true;
			}
		}
	}

	if (!foundAny) return ofRectangle(0, 0, 0, 0);

	int boxWidth = maxX - minX;
	int boxHeight = maxY - minY;
	float aspectRatio = static_cast<float>(boxWidth) / static_cast<float>(boxHeight);

	// Validate against standard HSRP aspect ratio constraints
	if (boxWidth < 60 || boxHeight < 15 || aspectRatio < 2.5f || aspectRatio > 6.0f) {
		return ofRectangle(0, 0, 0, 0);
	}

	return ofRectangle(minX, minY, boxWidth, boxHeight);
}

ofPixels IndianDetectionStrategy::binarizeROI(const ofPixels & croppedROI) {
	ofPixels binarized;
	binarized.allocate(croppedROI.getWidth(), croppedROI.getHeight(), OF_IMAGE_GRAYSCALE);

	int width = croppedROI.getWidth();
	int height = croppedROI.getHeight();
	int numChannels = croppedROI.getNumChannels();

	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			int index = (y * width + x) * numChannels;

			// Grayscale luminance conversion
			unsigned char gray = (croppedROI[index] + croppedROI[index + 1] + croppedROI[index + 2]) / 3;

			// Thresholding tuned for character segmentation
			binarized[y * width + x] = (gray < 115) ? 0 : 255;
		}
	}

	return binarized;
}
