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

#ifndef PDFFONTIDENTIFIER_H
#define PDFFONTIDENTIFIER_H

#include "pdfglobal.h"

#include <QImage>
#include <QString>
#include <QStringList>
#include <QCoreApplication>

#include <vector>

namespace pdf
{

/// Identifies the font of text, so that an edit is made in the same font: from the
/// name of a PDF font (born-digital text) or from the shapes of the letters of a scan
/// (OCR'd text). Everything runs locally, no image leaves the computer.
class PDF4QTLIBCORESHARED_EXPORT PDFFontIdentifier
{
    Q_DECLARE_TR_FUNCTIONS(pdf::PDFFontIdentifier)

public:
    PDFFontIdentifier() = delete;

    struct FontName
    {
        QString family;     ///< Family name as fonts are installed ("Times New Roman")
        bool bold = false;
        bool italic = false;
    };

    /// Reads the family and the style from the /BaseFont of a PDF font
    /// ("ABCDEF+TimesNewRomanPS-BoldMT" -> Times New Roman, bold)
    static FontName parsePdfFontName(const QByteArray& baseFont);

    /// Returns true, if a font of the family is installed (or loaded by the application)
    static bool isInstalled(const QString& family);

    /// Returns free fonts with the same metrics as a commercial font (Arial -> Liberation
    /// Sans), so the text keeps its width; empty when none is known
    static QStringList getMetricCompatibleSubstitutes(const QString& family);

    struct Match
    {
        QString family;
        bool bold = false;
        bool italic = false;
        double score = 0.0;         ///< 0 (no likeness) to 1 (identical shapes)
        double pixelSize = 0.0;     ///< Pixel size of the font matching the height of the scan
    };

    /// Ranks the fonts by the likeness of the text rendered in them to a scan of it.
    /// \param scan Image of the text (the word or the line, dark ink on light paper)
    /// \param text Text in the image (from OCR)
    /// \param families Families to try (installed or loaded)
    /// \param maximalResults Number of the best matches returned
    static std::vector<Match> matchScan(const QImage& scan, const QString& text, const QStringList& families, int maximalResults = 5);

    /// Returns the installed families usable for Latin text, without symbol fonts
    static QStringList getLatinFamilies();

    /// Returns the colours of the paper and of the ink of a scan (medians)
    static void getScanColors(const QImage& scan, QColor& paper, QColor& ink);
};

}   // namespace pdf

#endif // PDFFONTIDENTIFIER_H
