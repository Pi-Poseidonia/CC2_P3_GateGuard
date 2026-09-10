#pragma once
#include "ofMain.h"
#include <string>

// Datenstruktur für ein erkanntes Nummernschild
struct LicensePlate {
	ofImage croppedPlate; // Ausgeschnittener Bildbereich des Schildes
	ofRectangle boundingBox; // Position und Größe im Ursprungsbild (X, Y, B, H)
	std::string plateText; // Erkannter Text (z. B. "B-MW2026")
	bool isValid = false; // Status, ob die Erkennung erfolgreich war

	// Width, in pixels, of the EU color strip at the left edge of croppedPlate (0 if
	// not applicable/not detected, e.g. for a strategy with no such strip). Lets a
	// caller trim the strip out of croppedPlate before running external OCR (like
	// Tesseract) on it - the strip's stars/seal artwork can otherwise be misread as
	// stray characters, since external OCR doesn't know about the strip's existence.
	int stripWidthPx = 0;
};
