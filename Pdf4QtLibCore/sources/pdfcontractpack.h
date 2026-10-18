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

#ifndef PDFCONTRACTPACK_H
#define PDFCONTRACTPACK_H

#include "pdfglobal.h"
#include "pdfdocument.h"

#include <QString>
#include <QCoreApplication>

#include <vector>

namespace pdf
{

/// A pack of contracts sent as one PDF, split into its contracts so that each is
/// signed on its own. The parts are suggested from the top level bookmarks or from
/// the page numbering restarting at "Page 1 of N", or entered as page ranges.
class PDF4QTLIBCORESHARED_EXPORT PDFContractPack
{
    Q_DECLARE_TR_FUNCTIONS(pdf::PDFContractPack)

public:
    PDFContractPack() = delete;

    struct Part
    {
        QString name;
        PDFInteger firstPage = 0;   ///< Index of the first page
        PDFInteger lastPage = 0;    ///< Index of the last page (inclusive)

        bool operator==(const Part&) const = default;
    };
    using Parts = std::vector<Part>;

    /// Parts starting at the pages of the top level bookmarks
    static Parts suggestFromOutline(const PDFDocument* document);

    /// Parts starting at the pages whose text has "Page 1 of N" (or "1 of N", "1/N",
    /// "Bladsy 1 van N")
    static Parts suggestFromPageNumbers(const PDFDocument* document);

    /// Parts from the start pages (indices, sorted or not), the last part ends with the document
    static Parts fromStartPages(std::vector<PDFInteger> startPages, PDFInteger pageCount, const QStringList& names = QStringList());

    /// Parses page ranges "1-5, 6-9, 10-12" (pages numbered from 1). The ranges must
    /// lie in the document and not overlap; an empty error message means success.
    static Parts parse(const QString& text, PDFInteger pageCount, QString* errorMessage);

    /// Returns the ranges of the parts as text, as parse() reads them
    static QString toText(const Parts& parts);

    /// Creates a document of the pages of a part (with its own copy of the objects)
    static PDFDocument extract(const PDFDocument* document, const Part& part);

    /// Returns a file name for a part: the name made safe for a file system
    static QString getFileName(const QString& baseName, const Part& part, size_t index);
};

}   // namespace pdf

#endif // PDFCONTRACTPACK_H
