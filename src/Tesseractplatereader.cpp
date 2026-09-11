#include "TesseractPlateReader.h"

#include <tesseract/baseapi.h>

TesseractPlateReader::TesseractPlateReader()
	: tessApi(nullptr)
	, initialized(false) {
}

TesseractPlateReader::~TesseractPlateReader() {
	if (tessApi) {
		tesseract::TessBaseAPI * api = static_cast<tesseract::TessBaseAPI *>(tessApi);
		api->End();
		delete api;
	}
}

bool TesseractPlateReader::init(const std::string & tessdataPath, const std::string & language) {
	tesseract::TessBaseAPI * api = new tesseract::TessBaseAPI();

	// This build of Tesseract expects datapath to point directly at the folder
	// containing <language>.traineddata - it does not append a "tessdata/" subfolder.
	int result = api->Init(tessdataPath.c_str(), language.c_str());

	if (result != 0) {
		ofLogError("TesseractPlateReader") << "Failed to initialize Tesseract with datapath='"
										   << tessdataPath << "' language='" << language
										   << "' - check that " << tessdataPath << "/" << language
										   << ".traineddata exists.";
		delete api;
		tessApi = nullptr;
		initialized = false;
		return false;
	}

	// Restrict recognition to a single line of text - plates are one line of characters,
	// not a paragraph, and this page segmentation mode is far more reliable for that.
	api->SetPageSegMode(tesseract::PSM_SINGLE_LINE);

	// Real plates only ever contain uppercase letters and digits - constraining
	// Tesseract's character set eliminates a large class of confusions (e.g. it will
	// never consider lowercase letters or punctuation as candidates).
	api->SetVariable("tessedit_char_whitelist", "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ");

	tessApi = api;
	initialized = true;

	ofLogNotice("TesseractPlateReader") << "Tesseract initialized successfully (datapath='"
										<< tessdataPath << "', language='" << language << "').";

	return true;
}

std::string TesseractPlateReader::recognize(const ofPixels & binarizedPlate) {
	if (!initialized || !tessApi) {
		ofLogWarning("TesseractPlateReader") << "recognize() called before successful init().";
		return "";
	}

	tesseract::TessBaseAPI * api = static_cast<tesseract::TessBaseAPI *>(tessApi);

	int width = binarizedPlate.getWidth();
	int height = binarizedPlate.getHeight();
	int numChannels = binarizedPlate.getNumChannels();

	if (width == 0 || height == 0) {
		return "";
	}

	// SetImage's raw-buffer overload builds Tesseract's internal image representation
	// directly from pixel bytes, without us needing to construct a Leptonica PIX
	// ourselves. bytesPerPixel=1 for grayscale; bytesPerLine=width assumes tightly
	// packed rows, which is how ofPixels stores a single-channel image.
	api->SetImage(binarizedPlate.getData(), width, height, numChannels, width * numChannels);

	char * outText = api->GetUTF8Text();
	std::string result = outText ? std::string(outText) : "";
	if (outText) {
		delete[] outText;
	}

	// GetUTF8Text() includes a trailing newline - strip any whitespace/newlines so the
	// result is directly comparable to our own PlateDetector's plateText output.
	while (!result.empty() && std::isspace(static_cast<unsigned char>(result.back()))) {
		result.pop_back();
	}

	api->Clear(); // release internal image data before the next call

	return result;
}
