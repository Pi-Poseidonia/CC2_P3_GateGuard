#include "PlateDisplayer.h"

float PlateDisplayer::draw(const ofImage & originalImage, const LicensePlate & result,
	const std::vector<DisplayLine> & textLines, float x, float y, float width, float maxTotalHeight) {

	float yCursor = y;

	if (!originalImage.isAllocated()) {
		ofSetColor(255, 0, 0);
		ofDrawBitmapString("No image to display.", x, yCursor + 15);
		return 20;
	}

	bool hasCrop = result.isValid && result.croppedPlate.isAllocated();

	// --- Compute natural (width-fit) heights for the image and crop, then check
	// whether they'd fit within the height budget alongside the text lines. If not,
	// shrink both the image and crop by the SAME factor (preserving their own aspect
	// ratios) rather than independently squashing height, which would distort them. ---
	float naturalImageHeight = originalImage.getHeight() * (width / originalImage.getWidth());
	float naturalCropHeight = hasCrop
		? result.croppedPlate.getHeight() * (width / result.croppedPlate.getWidth())
		: 0.0f;

	float textBlockHeight = textLines.size() * 22.0f;
	float gaps = 10.0f + (hasCrop ? 10.0f : 0.0f);

	float scaleFactor = 1.0f;
	if (maxTotalHeight > 0.0f) {
		float imagesNeeded = naturalImageHeight + naturalCropHeight;
		float imagesBudget = maxTotalHeight - textBlockHeight - gaps;
		if (imagesBudget > 0.0f && imagesNeeded > imagesBudget) {
			scaleFactor = imagesBudget / imagesNeeded;
		}
	}

	float drawWidth = width * scaleFactor;

	// --- Original image, scaled to fit the (possibly shrunk) draw width ---
	ofSetColor(255);
	float scale = drawWidth / originalImage.getWidth();
	float imageHeight = originalImage.getHeight() * scale;
	originalImage.draw(x, yCursor, drawWidth, imageHeight);

	// --- Red bounding-box overlay around the detected plate ---
	if (result.isValid) {
		ofNoFill();
		ofSetLineWidth(3);
		ofSetColor(255, 0, 0); // red, per the plate-detection overlay spec
		ofDrawRectangle(
			x + result.boundingBox.x * scale,
			yCursor + result.boundingBox.y * scale,
			result.boundingBox.width * scale,
			result.boundingBox.height * scale);
		ofFill();
	}
	yCursor += imageHeight + 10;

	// --- Intermediate CV step thumbnail: the binarized/cropped plate ---
	if (hasCrop) {
		ofSetColor(255);
		float cropScale = drawWidth / result.croppedPlate.getWidth();
		float cropHeight = result.croppedPlate.getHeight() * cropScale;
		result.croppedPlate.draw(x, yCursor, drawWidth, cropHeight);
		yCursor += cropHeight + 10;
	} else {
		ofSetColor(255, 0, 0);
		ofDrawBitmapString("No plate detected - nothing to crop.", x, yCursor + 15);
		yCursor += 25;
	}

	// --- Extracted text strings (OCR results, access decisions, etc.) - drawn at
	// full width regardless of scaleFactor, since text doesn't need shrinking to fit ---
	for (const DisplayLine & line : textLines) {
		ofSetColor(line.color);
		ofDrawBitmapStringHighlight(line.text, x, yCursor + 12);
		yCursor += 22;
	}

	return yCursor - y;
}
