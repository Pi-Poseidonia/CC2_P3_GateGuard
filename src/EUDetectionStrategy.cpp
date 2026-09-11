#include "EUDetectionStrategy.h"
#include <vector>

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
	int stripWidth = 0;
	ofRectangle plateBox = findPlateBoundingBox(colorFiltered, stripWidth);

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
		result.stripWidthPx = stripWidth;
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
// NOTE: earlier versions of this method restricted the search to the left ~25% of the
// image width. That assumption only held when the whole input photo was already just
// the plate - it broke on full-scene photos (e.g. a car's rear view) where the plate
// itself sits somewhere within a much wider frame, not necessarily near the image's own
// left edge; restricting the search there would miss the plate entirely. Non-strip blue
// false-positives are instead rejected downstream via connected-component analysis in
// findPlateBoundingBox (keeping only the largest contiguous blob) plus its aspect-ratio
// and minimum-size checks, none of which depend on the plate's position in the frame.

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

	// 1. Nested For-loop: Iterate over each pixel in the input image
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {

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
//
// Uses connected-component analysis (flood fill) rather than a single global min/max scan
// across the whole mask. A naive global scan treats every blue-flagged pixel in the entire
// frame as part of one shape, so even a single stray pixel far from the real strip (e.g.
// chrome/embossing glare elsewhere on the plate) balloons the bounding box and fails the
// aspect-ratio check below. Finding each contiguous blob separately and keeping only the
// largest one (by pixel count) is robust to scattered noise regardless of where it or the
// real strip happen to sit in the frame - unlike a fixed-position search cutoff, which only
// works when the plate is known to be at a specific spot in the image.

ofRectangle EUDetectionStrategy::findPlateBoundingBox(const ofPixels & thresholdedImage, int & outStripWidth) {
	outStripWidth = 0; // safe default - only overwritten once a valid strip is confirmed below

	int width = thresholdedImage.getWidth();
	int height = thresholdedImage.getHeight();

	std::vector<bool> visited(width * height, false);

	int bestMinX = 0, bestMinY = 0, bestMaxX = 0, bestMaxY = 0;
	int bestPixelCount = 0;
	bool foundAny = false;

	// Flood-fill (4-connectivity) every unvisited "on" pixel to find each contiguous blob.
	for (int startY = 0; startY < height; startY++) {
		for (int startX = 0; startX < width; startX++) {
			int startIndex = startY * width + startX;

			if (visited[startIndex] || thresholdedImage[startIndex] != 255) {
				continue;
			}

			// BFS this blob using an explicit stack (avoids recursion depth issues on large blobs)
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

				// 4-connected neighbors
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

			// Keep only the largest blob found so far - the real strip is a solid
			// contiguous region, while noise is scattered into small, separate blobs.
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

	// Return empty rectangle if no candidate blob was found
	if (!foundAny) {
		return ofRectangle(0, 0, 0, 0);
	}

	int stripWidth = bestMaxX - bestMinX;
	int stripHeight = bestMaxY - bestMinY;
	int minX = bestMinX;
	int minY = bestMinY;

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

	// Report the strip's own width (relative to the final crop's left edge, which is
	// exactly minX) so a caller can trim it out of the crop before running external OCR.
	outStripWidth = stripWidth;

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
