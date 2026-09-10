#pragma once

#include "LicensePlate.h"
#include "PlateDetectionStrategy.h"
#include "ofMain.h"
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// Orchestrates the full plate-reading pipeline: delegates plate detection to
// whichever PlateDetectionStrategy is currently active (EU or Indian), then
// runs template-matching OCR on the resulting cropped/binarized plate to
// resolve a plateText string.
//
// OCR technique: zoning-based feature comparison. Each normalized glyph (template
// or segmented character) is divided into a grid of zones; each zone's ink density
// becomes one entry in a compact feature vector. Two glyphs are compared by how
// similar their feature vectors are, rather than by raw pixel-for-pixel overlap.
// This is a standard classic-OCR technique, chosen here because it tolerates font
// differences, stroke-width variance, and small misalignment better than direct
// pixel/IoU comparison - which is what earlier iterations of this class used.
class PlateDetector {
public:
	PlateDetector() = default;
	explicit PlateDetector(std::shared_ptr<PlateDetectionStrategy> strategy);

	// Swap strategies at runtime (e.g. via a UI toggle button in Phase 3).
	void setStrategy(std::shared_ptr<PlateDetectionStrategy> strategy);

	// Loads all character templates ("0".."9", "A".."Z") from bin/data/<templateFolder>.
	// Each template is tight-cropped to its ink, letterboxed into a fixed canvas, and its
	// zone feature vector is precomputed once here. (A font-rendered alternative was
	// tested and found to perform no better - see project notes/report - so this reverts
	// to the originally provided template images.)
	void loadTemplates(const std::string & templateFolder = "alphabet/");

	// Full pipeline: runs the active strategy's detect(), then - if a plate was
	// found - segments and OCRs its characters, filling in result.plateText.
	LicensePlate process(const ofPixels & input);

private:
	std::shared_ptr<PlateDetectionStrategy> activeStrategy;

	// Keyed by character label. templateFeatures holds each template's precomputed
	// zone feature vector (grid density fingerprint), so matching never has to
	// recompute it - only the incoming character needs computing per comparison.
	std::map<std::string, std::vector<float>> templateFeatures;

	// Fixed square canvas size (px) every glyph gets normalized into before zoning.
	static constexpr int kCanvasSize = 64;

	// Zoning grid dimensions. 8 rows x 6 columns = 48 zones is a reasonable balance:
	// fine enough to capture shape structure (e.g. B's open gap vs 8's closed loop),
	// coarse enough to stay tolerant of font/position differences.
	static constexpr int kZoneRows = 8;
	static constexpr int kZoneCols = 6;

	// support method: scans the binarized plate's vertical column projection to
	// find contiguous black-pixel blobs - one blob per character.
	std::vector<ofRectangle> segmentCharacterBlobs(const ofPixels & binarizedPlate);

	// support method: crops away any blank margin around a character's ink, so both
	// sides of a comparison are normalized to their actual glyph shape.
	ofPixels tightCropToInk(const ofPixels & input);

	// support method: uniformly scales a tight-cropped glyph to fit inside a
	// kCanvasSize x kCanvasSize white canvas, centered, preserving aspect ratio, then
	// re-binarizes the result (resize interpolation blurs edges into gray, which would
	// otherwise distort zone density counts).
	ofPixels letterbox(const ofPixels & tightInput, int canvasSize);

	// support method: divides a normalized (letterboxed) glyph into a kZoneRows x
	// kZoneCols grid and returns the fraction of dark pixels in each zone as a
	// flattened feature vector of length kZoneRows * kZoneCols.
	std::vector<float> computeZoneFeatures(const ofPixels & normalized, int canvasSize);

	// support method: compares two zone feature vectors via cosine similarity combined
	// with a magnitude-ratio penalty, converted into a similarity score in [0, 1]
	// (1 = identical shape and ink amount).
	float featureSimilarity(const std::vector<float> & a, const std::vector<float> & b);

	// support method: normalizes one segmented character crop and compares its zone
	// features against every loaded template, returning the best-matching label and
	// its confidence, so low-confidence matches (non-character artwork) can be
	// filtered out by the caller.
	std::pair<std::string, float> matchCharacter(const ofPixels & charPixels);

	// support method: runs segmentation + matching across the whole plate image
	// and concatenates the results into the final plate text string.
	std::string runOCR(const ofImage & binarizedPlate);
};
