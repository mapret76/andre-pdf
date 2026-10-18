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

#ifndef PDFOFFICEEXPORTER_H
#define PDFOFFICEEXPORTER_H

#include "pdfglobal.h"

#include <QString>
#include <QStringList>
#include <QByteArray>
#include <QCoreApplication>

#include <vector>

namespace pdf
{
class PDFDocument;

/// Export of the text of a document to Microsoft Word (.docx) and Excel (.xlsx),
/// written by the program itself (Office Open XML in a ZIP container). Word keeps
/// the paragraphs and pages; Excel gets one sheet per page with the lines split
/// into cells at wide gaps, numbers as numbers. The layout of the page is not
/// reproduced - Microsoft Word opens a PDF itself, when the layout matters.
class PDF4QTLIBCORESHARED_EXPORT PDFOfficeExporter
{
    Q_DECLARE_TR_FUNCTIONS(pdf::PDFOfficeExporter)

public:
    PDFOfficeExporter() = delete;

    struct TextLine
    {
        QString text;
        QStringList cells;      ///< Text split at wide gaps (columns of a table)
    };
    using Page = std::vector<TextLine>;
    using Pages = std::vector<Page>;

    /// Reads the text lines of every page, top to bottom
    static Pages extract(const PDFDocument* document);

    /// Writes a Word document, returns false on failure
    static bool writeDocx(const QString& fileName, const Pages& pages, QString* errorMessage = nullptr);

    /// Writes an Excel workbook, one sheet per page, returns false on failure
    static bool writeXlsx(const QString& fileName, const Pages& pages, QString* errorMessage = nullptr);

    /// Returns the value of a cell as a number, when the cell holds one ("1 234,56",
    /// "1,234.56", "R 250.00", "-12"); returns false for text
    static bool parseNumber(const QString& text, double& value);

    /// One file of a ZIP archive
    struct ZipEntry
    {
        QString name;
        QByteArray data;
    };

    /// Creates a ZIP archive with the files stored (not compressed)
    static QByteArray createZip(const std::vector<ZipEntry>& entries);
};

}   // namespace pdf

#endif // PDFOFFICEEXPORTER_H
