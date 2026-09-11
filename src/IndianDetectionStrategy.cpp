#include "IndianDetectionStrategy.h"
#include <algorithm>
#include <cmath>
#include <vector>

// Executes the three-stage computer vision pipeline for Indian HSRP license plates.
LicensePlate IndianDetectionStrategy::detect(const ofPixels & input) {
	LicensePlate result;

	// Safety check: verify memory allocation of incoming frame
	if (!input.isAllocated()) {
		result.isValid = false;
		return result;
	}

	// Stage 1: Segment high-contrast white plate areas and blue IND markers
	ofPixels colorFiltered = filterPlateColor(input);

	// Stage 2: Derive full plate bounding box using connected-component analysis
	ofRectangle plateBox = findPlateBoundingBox(colorFiltered);

	// Stage 3: Extract Region of Interest (ROI) and binarize for OCR processing
	if (plateBox.width > 0 && plateBox.height > 0) {
		ofPixels croppedROI;

		// Memory-efficient crop directly from input frame buffer
		input.cropTo(croppedROI, plateBox.x, plateBox.y, plateBox.width, plateBox.height);

		// High-contrast binarization for character segmentation
		ofPixels binarizedPlate = binarizeROI(croppedROI);

		// Populate return struct aligned with the standard detection interface
		result.croppedPlate.setFromPixels(binarizedPlate);
		result.boundingBox = plateBox;
		result.stripWidthPx = 0; // Indian HSRP plates lack a separate full-height EU strip
		result.isValid = true;
	} else {
		result.isValid = false;
	}

	return result;
}

// Stage 1: Color & Luminance filtering for Indian HSRP plates
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

			// 1. Target white HSRP plate base (high luminance, low color saturation)
			bool isHighBrightness = (r > 140 && g > 140 && b > 140);
			bool isLowSaturation = (std::abs(r - g) < 20 && std::abs(r - b) < 20 && std::abs(g - b) < 20);

			// 2. Optional target for blue IND emblem on the left edge
			bool isIndBlue = (b > 90 && b > r + 15 && b > g + 10);

			if ((isHighBrightness && isLowSaturation) || isIndBlue) {
				output[y * width + x] = 255; // Valid plate candidate pixel: white
			} else {
				output[y * width + x] = 0; // Background pixel: black
			}
		}
	}

	return output;
}

// Stage 2: Geometric analysis using Flood-Fill (BFS Stack)
//
// NOTE ON ARCHITECTURAL CHANGE (vs. Previous Implementation):
// Earlier versions relied on a 1D horizontal edge-transition heuristic (counting black-to-white
// pixel switches per row) to separate plates from flat car bodies. While fast, that approach was
// sensitive to high-contrast grill lines and bumper contours.
//
// The updated approach below uses connected-component analysis (Flood-Fill via BFS stack), matching
// the structural design of the EU strategy. Instead of scanning line-by-line, it identifies fully
// contiguous 2D shape clusters and retains only the largest valid blob. This dramatically increases
// robustness against complex backgrounds and glare while maintaining full architectural alignment
// across all detection strategies.

ofRectangle IndianDetectionStrategy::findPlateBoundingBox(const ofPixels & thresholdedImage) {
	int width = thresholdedImage.getWidth();
	int height = thresholdedImage.getHeight();

	std::vector<bool> visited(width * height, false);

	int bestMinX = 0, bestMinY = 0, bestMaxX = 0, bestMaxY = 0;
	int bestPixelCount = 0;
	bool foundAny = false;

	// Flood-fill (4-connectivity) to identify and measure each distinct contiguous region
	for (int startY = 0; startY < height; startY++) {
		for (int startX = 0; startX < width; startX++) {
			int startIndex = startY * width + startX;

			if (visited[startIndex] || thresholdedImage[startIndex] != 255) {
				continue;
			}

			// BFS traversal stack to eliminate deep recursive calls on larger blobs
			std::vector<ofPoint> stack;
			stack.push_back(ofPoint(startX, startY));
			visited[startIndex] = true;

			int minX = startX, minY = startY, maxX = startX, maxY = startY;
			int pixelCount = 0;

			while (!stack.empty()) {
				ofPoint p = stack.back();
				stack.pop_back();

				int px = static_cast<int>(p.x);
				int py = static_cast<int>(p.y);
				pixelCount++;

				if (px < minX) minX = px;
				if (px > maxX) maxX = px;
				if (py < minY) minY = py;
				if (py > maxY) maxY = py;

				// 4-connected direction vectors
				const int dx[4] = { -1, 1, 0, 0 };
				const int dy[4] = { 0, 0, -1, 1 };

				for (int dir = 0; dir < 4; dir++) {
					int nx = px + dx[dir];
					int ny = py + dy[dir];

					if (nx < 0 || nx >= width || ny < 0 || ny >= height) {
						continue;
					}

					int nIndex = ny * width + nx;
					if (!visited[nIndex] && thresholdedImage[nIndex] == 255) {
						visited[nIndex] = true;
						stack.push_back(ofPoint(nx, ny));
					}
				}
			}

			// Retain only the largest contiguous candidate blob (filters out background glare noise)
			if (pixelCount > bestPixelCount) {
				bestPixelCount = pixelCount;
				bestMinX = minX;
				bestMinY = minY;
				bestMaxX = maxX;
				bestMaxY = maxY;
				foundAny = true;
			}
		}
	}

	if (!foundAny) {
		return ofRectangle(0, 0, 0, 0);
	}

	int boxWidth = bestMaxX - bestMinX;
	int boxHeight = bestMaxY - bestMinY;

	// 1. Minimum Size Check: Filter out small noise artifacts
	if (boxWidth < 40 || boxHeight < 12) {
		return ofRectangle(0, 0, 0, 0);
	}

	// 2. Aspect Ratio Validation: Standard Indian HSRP ratio (~4.0:1)
	float aspect = static_cast<float>(boxWidth) / static_cast<float>(boxHeight);
	if (aspect < 2.2f || aspect > 5.5f) {
		return ofRectangle(0, 0, 0, 0);
	}

	ofLogNotice("IndianDetection") << "Detected HSRP Plate - X: " << bestMinX
								   << " Y: " << bestMinY
								   << " W: " << boxWidth
								   << " H: " << boxHeight
								   << " Aspect: " << aspect;

	return ofRectangle(bestMinX, bestMinY, boxWidth, boxHeight);
}

// Stage 3: Binarize cropped ROI for OCR character extraction
ofPixels IndianDetectionStrategy::binarizeROI(const ofPixels & croppedROI) {
	ofPixels binarized;
	binarized.allocate(croppedROI.getWidth(), croppedROI.getHeight(), OF_IMAGE_GRAYSCALE);

	int width = croppedROI.getWidth();
	int height = croppedROI.getHeight();
	int numChannels = croppedROI.getNumChannels();

	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			int index = (y * width + x) * numChannels;

			// Convert RGB to single luminance channel
			unsigned char gray = (croppedROI[index] + croppedROI[index + 1] + croppedROI[index + 2]) / 3;

			// Static threshold tuned for dark characters on light background
			binarized[y * width + x] = (gray < 115) ? 0 : 255;
		}
	}

	return binarized;
}
