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

#ifndef PDFSCANTEXTEDITOR_H
#define PDFSCANTEXTEDITOR_H

#include "pdfglobal.h"
#include "pdfdocument.h"

#include <QFont>
#include <QColor>
#include <QImage>
#include <QRectF>
#include <QCoreApplication>

namespace pdf
{

/// Editing of the text of a scanned page in its original font: the area of the
/// words is rendered, its text read from the OCR layer, the font identified from the
/// image (PDFFontIdentifier); the edit covers the area in the colour of the paper and
/// writes the new text in the identified font, size and ink colour. The font is
/// embedded in the document, so the edit looks the same on any computer.
class PDF4QTLIBCORESHARED_EXPORT PDFScanTextEditor
{
    Q_DECLARE_TR_FUNCTIONS(pdf::PDFScanTextEditor)

public:
    PDFScanTextEditor() = delete;

    /// Renders an area of a page (page coordinates, y up) at the resolution
    static QImage renderArea(const PDFDocument* document, PDFInteger pageIndex, const QRectF& area, double dpi);

    /// Returns the text of the area (the OCR layer of a scan, or the text of the page)
    static QString getText(const PDFDocument* document, PDFInteger pageIndex, const QRectF& area);

    /// Returns the rectangle of the ink in an image rendered from an area, in page coordinates
    static QRectF getInkRectangle(const QImage& image, const QRectF& area);

    struct Edit
    {
        PDFInteger pageIndex = 0;
        QRectF area;            ///< Area covered by the colour of the paper (page coordinates)
        QRectF inkRectangle;    ///< Where the old text was inked; the new text is centred on it
        QString text;           ///< New text
        QFont font;             ///< Font, its point size is the size on the page
        QColor ink = Qt::black;
        QColor paper = Qt::white;
    };

    /// Returns true, if the page has the OCR text layer of this program
    static bool hasOwnOcrLayer(const PDFDocument* document, PDFInteger pageIndex);

    /// Applies the edit, returns the modified document, nothing on failure. The own
    /// OCR text layer of the page is removed: it would still hold the old words (and
    /// the change of the page unbinds it anyway); OCR of the page makes it again.
    /// \param document Document
    /// \param edit Edit
    /// \param isOcrLayerRemoved Set to true, when the OCR layer of the page was removed
    static PDFDocumentPointer apply(const PDFDocument* document, const Edit& edit, bool* isOcrLayerRemoved = nullptr);
};

}   // namespace pdf

#endif // PDFSCANTEXTEDITOR_H
