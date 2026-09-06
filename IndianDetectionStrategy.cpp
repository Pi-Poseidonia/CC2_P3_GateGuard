#include "IndianDetectionStrategy.h"

// support method for phase 2 of the pipeline: color filtering to detect white HSRP plates
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
