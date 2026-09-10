#pragma once

#include "ofMain.h"
#include <string>

// Thin wrapper around Tesseract's C++ API, scoped specifically to reading a single
// line of plate-style text (uppercase letters + digits only) from an already-detected,
// already-cropped/binarized plate image. This is meant to sit alongside - not replace -
// EUDetectionStrategy's detection work; only the character-recognition step changes.
class TesseractPlateReader {
public:
	TesseractPlateReader();
	~TesseractPlateReader();

	// tessdataPath is the folder that directly CONTAINS the .traineddata file (e.g. if
	// eng.traineddata lives at bin/tessdata/eng.traineddata, pass "tessdata" when
	// running from bin/). This build of Tesseract does NOT auto-append a "tessdata/"
	// subfolder - the path given must point at the file's actual containing folder.
	bool init(const std::string & tessdataPath = "tessdata", const std::string & language = "eng");

	// Runs OCR on an already-cropped, already-binarized plate image (grayscale ofPixels,
	// black text on white background - the same format EUDetectionStrategy produces).
	// Restricted to uppercase letters + digits, matching real plate character sets.
	std::string recognize(const ofPixels & binarizedPlate);

private:
	void * tessApi; // opaque tesseract::TessBaseAPI*, kept as void* so this header
	// doesn't require every includer to have Tesseract's headers on
	// their include path.
	bool initialized;
};
