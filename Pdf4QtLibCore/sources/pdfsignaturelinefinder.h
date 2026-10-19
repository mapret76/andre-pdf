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

#ifndef PDFSIGNATURELINEFINDER_H
#define PDFSIGNATURELINEFINDER_H

#include "pdfglobal.h"
#include "pdfdocument.h"

#include <QRectF>
#include <QString>
#include <QCoreApplication>

#include <vector>

namespace pdf
{
class PDFDocument;

/// Finds the places of a contract, where it is to be signed: unsigned signature
/// fields, and lines of underscores or dots labelled as a signature (in English and
/// Afrikaans), or a "Signature:" label ending a line.
class PDF4QTLIBCORESHARED_EXPORT PDFSignatureLineFinder
{
    Q_DECLARE_TR_FUNCTIONS(pdf::PDFSignatureLineFinder)

public:
    PDFSignatureLineFinder() = delete;

    enum class Source
    {
        SignatureField,     ///< Unsigned signature field of the form
        Line,               ///< Underscores or dots with a signature label
        Label               ///< A signature label ending a line, nothing to sign on
    };

    struct SignatureLine
    {
        PDFInteger pageIndex = -1;
        QRectF rectangle;       ///< Where the signature goes, page coordinates (y up)
        Source source = Source::Line;
        QString label;          ///< Text near the line, or the name of the field
        QString fieldName;      ///< Fully qualified name of the signature field
    };

    /// Finds the signature lines of the document, ordered by page and from the
    /// top of the page down
    static std::vector<SignatureLine> find(const PDFDocument* document);

    /// Adds an empty (unsigned) signature field on each line, for the lines left to
    /// another party: a reader offers the field to sign, also in a document certified
    /// "form filling and signing" or encrypted. Lines which are fields already are
    /// skipped. Returns the modified document, nothing when no field was added.
    static PDFDocumentPointer addSignatureFields(const PDFDocument* document, const std::vector<SignatureLine>& lines, const QString& namePrefix);

    /// Returns true, if the text labels a signature (and not a name or date line)
    static bool isSignatureLabel(const QString& text);

    /// Height of the signature area above a line, in points
    static constexpr PDFReal SIGNATURE_HEIGHT = 36.0;
};

}   // namespace pdf

#endif // PDFSIGNATURELINEFINDER_H
