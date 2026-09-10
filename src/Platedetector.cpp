#include "PlateDetector.h"
#include <algorithm>
#include <cmath>

PlateDetector::PlateDetector(std::shared_ptr<PlateDetectionStrategy> strategy)
	: activeStrategy(strategy) {
}

void PlateDetector::setStrategy(std::shared_ptr<PlateDetectionStrategy> strategy) {
	activeStrategy = strategy;
}

// Loads every alphabet/number template image (0-9, A-Z) from bin/data/<templateFolder>.
// Filenames are expected to match the character exactly, e.g. "0.png", "B.png"
// (matches the alphabet/ folder contents already in the project). Each template is
// tight-cropped to its own ink bounding box, then letterboxed into a fixed
// kCanvasSize x kCanvasSize canvas, and its zone feature vector is precomputed once
// here - so matching only has to compute the incoming character's features per
// comparison, not re-derive every template's.
//
// NOTE: a font-rendered alternative (generating high-resolution templates in code
// instead of loading these ~20x35px PNGs) was tested and found to perform no better -
// which ruled out template resolution as the accuracy bottleneck. This reverts to the
// originally provided template images.
void PlateDetector::loadTemplates(const std::string & templateFolder) {
	templateFeatures.clear();

	std::string digits = "0123456789";
	std::string letters = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	std::string allChars = digits + letters;

	for (char c : allChars) {
		std::string label(1, c);
		std::string path = templateFolder + label + ".png";

		ofImage img;
		if (img.load(path)) {
			img.setImageType(OF_IMAGE_GRAYSCALE);

			ofPixels tight = tightCropToInk(img.getPixels());
			ofPixels normalized = letterbox(tight, kCanvasSize);

			templateFeatures[label] = computeZoneFeatures(normalized, kCanvasSize);
		} else {
			ofLogWarning("PlateDetector") << "Could not load template: " << path;
		}
	}

	ofLogNotice("PlateDetector") << "Loaded " << templateFeatures.size() << " character templates.";
}

// Full pipeline: delegate detection to whichever strategy is active, then OCR
// the result if a valid plate was found.
LicensePlate PlateDetector::process(const ofPixels & input) {
	LicensePlate result;

	if (!activeStrategy) {
		ofLogError("PlateDetector") << "No active PlateDetectionStrategy set.";
		result.isValid = false;
		return result;
	}

	result = activeStrategy->detect(input);

	if (result.isValid) {
		result.plateText = runOCR(result.croppedPlate);
	}

	return result;
}

// Scans the binarized plate image column by column, looking for contiguous runs
// of columns containing dark (character) pixels. Each contiguous run becomes one
// character's bounding box - a simple but effective vertical-projection segmentation.
std::vector<ofRectangle> PlateDetector::segmentCharacterBlobs(const ofPixels & binarizedPlate) {
	std::vector<ofRectangle> blobs;

	int width = binarizedPlate.getWidth();
	int height = binarizedPlate.getHeight();
	int numChannels = binarizedPlate.getNumChannels();

	if (width == 0 || height == 0) {
		return blobs;
	}

	// 1. Build a per-column "has dark pixel" projection.
	// Restricted to a central vertical band (excludes top/bottom ~18% of rows) so the
	// plate's outer border line - which runs continuously across the full width - doesn't
	// bridge every column into one giant blob.
	int topMargin = static_cast<int>(height * 0.18f);
	int bottomMargin = height - topMargin;

	std::vector<bool> columnHasInk(width, false);
	for (int x = 0; x < width; x++) {
		for (int y = topMargin; y < bottomMargin; y++) {
			int index = (y * width + x) * numChannels;
			unsigned char value = binarizedPlate[index];
			if (value < 128) {
				columnHasInk[x] = true;
				break;
			}
		}
	}

	// 2. Walk the projection, grouping contiguous ink columns into blobs.
	int blobStartX = -1;
	// Scaled to the plate's own width rather than a fixed pixel count, so this adapts
	// to photos taken at very different distances/resolutions. A fixed 20px threshold
	// (tuned against a high-res, close-up test photo) consumed almost an entire
	// character's width on a lower-resolution real-world photo where the whole plate
	// crop was only ~275px wide - causing characters to wrongly merge or fragment.
	// ~2.5% of plate width approximates the real minimum character width across the
	// test images gathered so far; never go below a small absolute floor either, to
	// avoid treating single-pixel noise as valid on a very narrow crop.
	const int minCharWidth = std::max(6, static_cast<int>(width * 0.025f));

	for (int x = 0; x < width; x++) {
		if (columnHasInk[x] && blobStartX == -1) {
			blobStartX = x;
		} else if (!columnHasInk[x] && blobStartX != -1) {
			int blobWidth = x - blobStartX;
			if (blobWidth >= minCharWidth) {
				int clampedX = static_cast<int>(ofClamp(blobStartX, 0, width - 1));
				int clampedWidth = static_cast<int>(ofClamp(blobWidth, 0, width - clampedX));
				blobs.push_back(ofRectangle(clampedX, 0, clampedWidth, height));
			}
			blobStartX = -1;
		}
	}
	if (blobStartX != -1) {
		int blobWidth = width - blobStartX;
		if (blobWidth >= minCharWidth) {
			int clampedX = static_cast<int>(ofClamp(blobStartX, 0, width - 1));
			int clampedWidth = static_cast<int>(ofClamp(blobWidth, 0, width - clampedX));
			blobs.push_back(ofRectangle(clampedX, 0, clampedWidth, height));
		}
	}

	return blobs;
}

// Scans an image for its dark-pixel (ink) bounding box and crops away everything
// outside it.
ofPixels PlateDetector::tightCropToInk(const ofPixels & input) {
	int width = input.getWidth();
	int height = input.getHeight();
	int numChannels = input.getNumChannels();

	int minX = width, minY = height, maxX = 0, maxY = 0;
	bool foundAny = false;

	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			int index = (y * width + x) * numChannels;
			if (input[index] < 128) {
				if (x < minX) minX = x;
				if (x > maxX) maxX = x;
				if (y < minY) minY = y;
				if (y > maxY) maxY = y;
				foundAny = true;
			}
		}
	}

	if (!foundAny) {
		return input;
	}

	ofPixels tight;
	int cropX = static_cast<int>(ofClamp(minX, 0, width - 1));
	int cropY = static_cast<int>(ofClamp(minY, 0, height - 1));
	int cropWidth = static_cast<int>(ofClamp(maxX - minX + 1, 1, width - cropX));
	int cropHeight = static_cast<int>(ofClamp(maxY - minY + 1, 1, height - cropY));

	input.cropTo(tight, cropX, cropY, cropWidth, cropHeight);
	return tight;
}

// Uniformly scales a tight-cropped glyph to fit inside a canvasSize x canvasSize white
// square, centered, preserving aspect ratio, then re-binarizes the result so resize
// interpolation blur never distorts the zone density counts computed afterward.
ofPixels PlateDetector::letterbox(const ofPixels & tightInput, int canvasSize) {
	int srcWidth = tightInput.getWidth();
	int srcHeight = tightInput.getHeight();

	ofPixels canvas;
	canvas.allocate(canvasSize, canvasSize, OF_IMAGE_GRAYSCALE);
	for (int i = 0; i < canvasSize * canvasSize; i++) {
		canvas[i] = 255;
	}

	if (srcWidth == 0 || srcHeight == 0) {
		return canvas;
	}

	const int margin = 4;
	int targetMax = canvasSize - margin * 2;

	float scale = std::min(
		static_cast<float>(targetMax) / static_cast<float>(srcWidth),
		static_cast<float>(targetMax) / static_cast<float>(srcHeight));

	int scaledWidth = std::max(1, static_cast<int>(srcWidth * scale));
	int scaledHeight = std::max(1, static_cast<int>(srcHeight * scale));

	ofImage tempImage;
	tempImage.setFromPixels(tightInput);
	tempImage.setImageType(OF_IMAGE_GRAYSCALE);
	tempImage.resize(scaledWidth, scaledHeight);

	ofPixels & scaledPixels = tempImage.getPixels();
	int scaledChannels = scaledPixels.getNumChannels();

	int offsetX = (canvasSize - scaledWidth) / 2;
	int offsetY = (canvasSize - scaledHeight) / 2;

	for (int y = 0; y < scaledHeight; y++) {
		for (int x = 0; x < scaledWidth; x++) {
			int srcIndex = (y * scaledWidth + x) * scaledChannels;
			unsigned char value = scaledPixels[srcIndex];

			int destX = offsetX + x;
			int destY = offsetY + y;

			if (destX >= 0 && destX < canvasSize && destY >= 0 && destY < canvasSize) {
				canvas[destY * canvasSize + destX] = value;
			}
		}
	}

	// Re-binarize: resize interpolation introduces gray edges, which would otherwise
	// leak partial/fractional values into the zone density counts below.
	for (int i = 0; i < canvasSize * canvasSize; i++) {
		canvas[i] = (canvas[i] < 128) ? 0 : 255;
	}

	return canvas;
}

// Divides the normalized glyph into a kZoneRows x kZoneCols grid and returns the
// fraction of dark pixels within each zone as a flattened feature vector.
std::vector<float> PlateDetector::computeZoneFeatures(const ofPixels & normalized, int canvasSize) {
	std::vector<float> features(kZoneRows * kZoneCols, 0.0f);

	float zoneWidth = static_cast<float>(canvasSize) / kZoneCols;
	float zoneHeight = static_cast<float>(canvasSize) / kZoneRows;

	for (int zy = 0; zy < kZoneRows; zy++) {
		for (int zx = 0; zx < kZoneCols; zx++) {
			int startX = static_cast<int>(zx * zoneWidth);
			int endX = static_cast<int>((zx + 1) * zoneWidth);
			int startY = static_cast<int>(zy * zoneHeight);
			int endY = static_cast<int>((zy + 1) * zoneHeight);

			int darkCount = 0;
			int totalCount = 0;

			for (int y = startY; y < endY && y < canvasSize; y++) {
				for (int x = startX; x < endX && x < canvasSize; x++) {
					totalCount++;
					if (normalized[y * canvasSize + x] < 128) {
						darkCount++;
					}
				}
			}

			int zoneIndex = zy * kZoneCols + zx;
			features[zoneIndex] = (totalCount > 0)
				? static_cast<float>(darkCount) / static_cast<float>(totalCount)
				: 0.0f;
		}
	}

	return features;
}

// Compares two zone feature vectors via a combination of cosine similarity (the angle
// between them, capturing shape/pattern) and a magnitude-ratio penalty (how similar
// their overall ink amount is). Cosine similarity alone is fooled by sparse vectors:
// a thin glyph like "I" (ink concentrated in a few central zones, zero elsewhere) ends
// up cosine-similar to almost any denser character that also has central ink, simply
// because it has little pattern to disagree with. Penalizing large magnitude
// differences suppresses that false attraction while leaving genuine matches - which
// share both shape AND overall ink amount - unaffected.
float PlateDetector::featureSimilarity(const std::vector<float> & a, const std::vector<float> & b) {
	if (a.size() != b.size() || a.empty()) {
		return 0.0f;
	}

	float dotProduct = 0.0f;
	float normA = 0.0f;
	float normB = 0.0f;

	for (size_t i = 0; i < a.size(); i++) {
		dotProduct += a[i] * b[i];
		normA += a[i] * a[i];
		normB += b[i] * b[i];
	}

	normA = std::sqrt(normA);
	normB = std::sqrt(normB);

	if (normA < 1e-6f || normB < 1e-6f) {
		return 0.0f;
	}

	float cosineSimilarity = ofClamp(dotProduct / (normA * normB), 0.0f, 1.0f);

	// 1.0 when magnitudes match exactly; shrinks toward 0 as they diverge.
	float magnitudeRatio = std::min(normA, normB) / std::max(normA, normB);

	float combinedScore = cosineSimilarity * magnitudeRatio;
	return combinedScore;
}

// Normalizes one segmented character crop the same way templates were normalized at
// load time, computes its zone features, and compares against every template's
// precomputed feature vector to find the closest match.
std::pair<std::string, float> PlateDetector::matchCharacter(const ofPixels & charPixels) {
	std::string bestMatch = "?";
	float bestScore = -1.0f;

	ofPixels tight = tightCropToInk(charPixels);
	ofPixels normalized = letterbox(tight, kCanvasSize);
	std::vector<float> charFeatures = computeZoneFeatures(normalized, kCanvasSize);

	for (auto & entry : templateFeatures) {
		const std::string & label = entry.first;
		const std::vector<float> & templateFeat = entry.second;

		float score = featureSimilarity(charFeatures, templateFeat);

		if (score > bestScore) {
			bestScore = score;
			bestMatch = label;
		}
	}

	return { bestMatch, bestScore };
}

// Runs segmentation across the full binarized plate, then matches each segmented
// character blob against the template set, concatenating the results left-to-right.
std::string PlateDetector::runOCR(const ofImage & binarizedPlate) {
	if (templateFeatures.empty()) {
		ofLogWarning("PlateDetector") << "runOCR called with no templates loaded - call loadTemplates() first.";
		return "";
	}

	const ofPixels & platePixels = binarizedPlate.getPixels();
	std::vector<ofRectangle> blobs = segmentCharacterBlobs(platePixels);

	ofLogNotice("PlateDetector") << "segmentCharacterBlobs found " << blobs.size() << " blob(s):";
	for (size_t i = 0; i < blobs.size(); i++) {
		ofLogNotice("PlateDetector") << "  blob[" << i << "] x=" << blobs[i].x
									 << " width=" << blobs[i].width << " height=" << blobs[i].height;
	}

	std::string plateText;
	// Combined score (cosine x magnitude-ratio) runs lower than cosine alone since it's
	// a product of two [0,1] terms - starting threshold, tune based on real results.
	const float minMatchConfidence = 0.45f;

	for (size_t i = 0; i < blobs.size(); i++) {
		const ofRectangle & blob = blobs[i];

		// The EU strip is always segmented as the first blob, starting at x=0 - this
		// follows directly from how EUDetectionStrategy builds the plate crop.
		if (i == 0 && blob.x <= 2) {
			continue;
		}

		ofPixels charCrop;

		int cropX = static_cast<int>(ofClamp(blob.x, 0, platePixels.getWidth() - 1));
		int cropY = static_cast<int>(ofClamp(blob.y, 0, platePixels.getHeight() - 1));
		int cropWidth = static_cast<int>(ofClamp(blob.width, 1, platePixels.getWidth() - cropX));
		int cropHeight = static_cast<int>(ofClamp(blob.height, 1, platePixels.getHeight() - cropY));

		platePixels.cropTo(charCrop, cropX, cropY, cropWidth, cropHeight);

		std::pair<std::string, float> match = matchCharacter(charCrop);
		ofLogNotice("PlateDetector") << "  match[" << i << "] label=" << match.first
									 << " confidence=" << match.second;

		if (match.second >= minMatchConfidence) {
			plateText += match.first;
		}
	}

	return plateText;
}
