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

#ifndef PDFPAGEINITIALS_H
#define PDFPAGEINITIALS_H

#include "pdfglobal.h"
#include "pdfdocument.h"

#include <QImage>
#include <QRectF>
#include <QTransform>
#include <QCoreApplication>

#include <set>

namespace pdf
{
class PDFPage;

/// Initials on every page of a contract: an image (usually from the signature
/// library) placed at the same corner of each page as the reader sees it, so the
/// page rotation (/Rotate) is honoured, and drawn into the page content.
class PDF4QTLIBCORESHARED_EXPORT PDFPageInitials
{
    Q_DECLARE_TR_FUNCTIONS(pdf::PDFPageInitials)

public:
    PDFPageInitials() = delete;

    enum class Position
    {
        BottomRight,
        BottomCenter,
        BottomLeft,
        TopRight,
        TopLeft
    };

    struct Settings
    {
        Position position = Position::BottomRight;
        PDFReal width = 56.0;       ///< Width of the initials in points (about 20 mm), the height follows the image
        PDFReal margin = 28.0;      ///< Distance from the edges of the visible page in points (about 10 mm)
        std::set<PDFInteger> skippedPages; ///< Indices of the pages left without initials
    };

    /// Returns the transformation from the page as displayed (origin at its bottom
    /// left corner, y up, rotation applied) to the user space of the page
    static QTransform getDisplayToPageTransform(const PDFPage* page);

    /// Returns the rectangle of the initials in the displayed page coordinates
    /// \param page Page
    /// \param imageSize Size of the image (only its aspect ratio is used)
    /// \param settings Settings
    static QRectF getDisplayRectangle(const PDFPage* page, QSizeF imageSize, const Settings& settings);

    /// Draws the initials onto the pages of the document. Returns the modified
    /// document, or nothing when no page was changed.
    /// \param document Document
    /// \param image Image of the initials, transparent background
    /// \param settings Settings
    static PDFDocumentPointer apply(const PDFDocument* document, const QImage& image, const Settings& settings);
};

}   // namespace pdf

#endif // PDFPAGEINITIALS_H
