// MIT License
//
// Copyright (c) 2018-2026 Jakub Melka and Contributors
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef PDFSCANFONTBUILDER_H
#define PDFSCANFONTBUILDER_H

#include "pdfglobal.h"
#include "pdfdocument.h"

#include <QImage>
#include <QPolygonF>
#include <QCoreApplication>

#include <vector>

namespace pdf
{

/// Builds a font from the letters of a scan, when no installed or free font is
/// like the font of the document: every word of the OCR text is split into its ink
/// shapes, the shapes are matched to the recognized letters, the samples of each
/// letter are averaged (which removes the noise of the scan), traced to outlines
/// and written as a TrueType font. Edits then use the very letters of the document.
/// The font has only the letters found in the scan; other letters fall back to the
/// best matching font.
class PDF4QTLIBCORESHARED_EXPORT PDFScanFontBuilder
{
    Q_DECLARE_TR_FUNCTIONS(pdf::PDFScanFontBuilder)

public:
    PDFScanFontBuilder() = delete;

    struct Glyph
    {
        QChar character;
        QImage bitmap;          ///< Grayscale, ink is black (0), averaged from the samples
        int baselineOffset = 0; ///< Pixels of the bitmap below the baseline
        int samples = 0;        ///< Number of the samples averaged
    };

    struct Metrics
    {
        double xHeight = 0.0;       ///< Height of the lower case letters, pixels
        double sideBearing = 0.0;   ///< Half of the gap between the letters of a word, pixels
        double wordSpace = 0.0;     ///< Gap between the words, pixels
    };

    /// Collects the letters of the pages, rendered at the resolution
    static std::vector<Glyph> harvest(const PDFDocument* document, const std::vector<PDFInteger>& pageIndices, double dpi, Metrics* metrics);

    /// Traces the outlines of the ink of a bitmap (black is ink): closed polygons on the
    /// pixel corners, simplified within the tolerance (pixels); outer outlines go
    /// clockwise on the screen (y down), holes the other way
    static std::vector<QPolygonF> traceContours(const QImage& bitmap, double tolerance = 0.7);

    /// Writes a TrueType font of the glyphs
    /// \param glyphs Glyphs
    /// \param metrics Metrics of the scan
    /// \param family Family name of the font
    static QByteArray createTrueTypeFont(const std::vector<Glyph>& glyphs, const Metrics& metrics, const QString& family);

    /// Harvests the pages and writes the font; empty when too few letters were found
    static QByteArray build(const PDFDocument* document, const std::vector<PDFInteger>& pageIndices, const QString& family, QString* characters = nullptr);
};

}   // namespace pdf

#endif // PDFSCANFONTBUILDER_H
