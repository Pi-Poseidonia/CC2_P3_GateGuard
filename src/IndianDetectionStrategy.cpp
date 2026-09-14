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

			// Simple horizontal gradient check.
			// License plate characters create strong intensity changes.
			int gray = (r + g + b) / 3;

			int gradient = 0;

			if (x < width - 1) {

				int nextIndex = (y * width + (x + 1)) * numChannels;

				int nextGray = (input[nextIndex]
								   + input[nextIndex + 1]
								   + input[nextIndex + 2])
					/ 3;

				gradient = std::abs(nextGray - gray);
			}

			bool hasStrongEdge = (gradient > 15);

			// Optional target for blue IND emblem on the left edge
			bool isIndBlue = (b > 90 && b > r + 15 && b > g + 10);

			if ((isHighBrightness && isLowSaturation && hasStrongEdge)
				|| isIndBlue) {

				output[y * width + x] = 255;
			} else {
				output[y * width + x] = 0;
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

	float bestScore = -1.0f;

	bool foundAny = false;

	// Flood-fill (4-connectivity) to identify and measure each distinct contiguous region

	for (int startY = 0; startY < height; startY++) {

		for (int startX = 0; startX < width; startX++) {

			int startIndex = startY * width + startX;

			if (visited[startIndex] || thresholdedImage[startIndex] != 255) {

				continue;
			}

			// BFS traversal stack

			std::vector<ofPoint> stack;

			stack.push_back(ofPoint(startX, startY));

			visited[startIndex] = true;

			int minX = startX, minY = startY, maxX = startX, maxY = startX;

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

			int boxWidth = maxX - minX;

			int boxHeight = maxY - minY;

			// 1. Minimum Size Check: Filter out small noise artifacts

			if (boxWidth < 40 || boxHeight < 12) {

				continue;
			}

			// 2. Aspect Ratio Validation: Standard Indian HSRP ratio (~4.0:1)

			float aspect = static_cast<float>(boxWidth) / static_cast<float>(boxHeight);

			if (aspect < 2.2f || aspect > 5.5f) {

				continue;
			}

			// 3. Density / Text Check: Count black pixels (0) inside the bounding box.

			// A real license plate contains text and borders. A car hood reflection is solid white (0% black pixels).

			int interiorPixels = 0;

			int blackPixelCount = 0;

			for (int y = minY; y <= maxY; y++) {

				for (int x = minX; x <= maxX; x++) {

					interiorPixels++;

					if (thresholdedImage[y * width + x] == 0) {

						blackPixelCount++;
					}
				}
			}

			float blackDensity = static_cast<float>(blackPixelCount) / static_cast<float>(interiorPixels);

			// Reject if there is virtually no text/border inside (like smooth car body paint)

			if (blackDensity < 0.04f || blackDensity > 0.50f) {

				continue;
			}

			// --- SCORING SYSTEM ---

			float idealAspect = 4.1f;

			float aspectDeviation = std::abs(aspect - idealAspect);

			float score = 1.0f / (1.0f + aspectDeviation);

			// Keep the candidate with the highest shape accuracy score

			if (score > bestScore) {

				bestScore = score;

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

	int finalWidth = bestMaxX - bestMinX;

	int finalHeight = bestMaxY - bestMinY;

	float finalAspect = static_cast<float>(finalWidth) / static_cast<float>(finalHeight);

	ofLogNotice("IndianDetection") << "Detected HSRP Plate - X: " << bestMinX

								   << " Y: " << bestMinY

								   << " W: " << finalWidth

								   << " H: " << finalHeight

								   << " Aspect: " << finalAspect

								   << " Score: " << bestScore;

	ofRectangle detectedRect(bestMinX, bestMinY, finalWidth, finalHeight);

	// Safety check against out-of-bounds cropping

	if (detectedRect.x < 0 || detectedRect.y < 0 ||

		detectedRect.x + detectedRect.width > width ||

		detectedRect.y + detectedRect.height > height) {

		return ofRectangle(0, 0, 0, 0);
	}

	return detectedRect;
}

/**
 * @brief Binarizes the extracted License Plate Region of Interest (ROI) for OCR processing.
 * @param croppedROI The cropped color or grayscale image of the license plate candidate.
 * @return ofPixels A high-contrast grayscale binarized image optimized for character segmentation.
 */
ofPixels IndianDetectionStrategy::binarizeROI(const ofPixels & croppedROI) {
	int width = croppedROI.getWidth();
	int height = croppedROI.getHeight();
	int numChannels = croppedROI.getNumChannels();

	ofPixels grayscale;
	grayscale.allocate(width, height, OF_IMAGE_GRAYSCALE);

	// Convert cropped ROI to grayscale
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			int index = (y * width + x) * numChannels;
			unsigned char gray = 0;
			if (numChannels >= 3) {
				unsigned char r = croppedROI[index];
				unsigned char g = croppedROI[index + 1];
				unsigned char b = croppedROI[index + 2];
				gray = static_cast<unsigned char>(0.299f * r + 0.587f * g + 0.114f * b);
			} else {
				gray = croppedROI[index];
			}
			grayscale[y * width + x] = gray;
		}
	}

	// Dynamic thresholding based on average pixel intensity within the license plate ROI
	long long sum = 0;
	int totalPixels = width * height;
	if (totalPixels == 0) return grayscale;

	for (int i = 0; i < totalPixels; i++) {
		sum += grayscale[i];
	}
	unsigned char threshold = static_cast<unsigned char>(sum / totalPixels);

	ofPixels binarized;
	binarized.allocate(width, height, OF_IMAGE_GRAYSCALE);

	for (int i = 0; i < totalPixels; i++) {
		if (grayscale[i] < threshold) {
			binarized[i] = 0; // Characters / Black
		} else {
			binarized[i] = 255; // Background / White
		}
	}

	return binarized;
}
