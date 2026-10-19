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

#ifndef PDFVERSIONHISTORY_H
#define PDFVERSIONHISTORY_H

#include "pdfglobal.h"

#include <QDate>
#include <QString>
#include <QStringList>

#include <vector>

namespace pdf
{

/// Version history of Andre PDF, read from CHANGELOG.md: one section per version,
/// "## 1.2.0 - 2026-11-01", a line "Summary: ..." and the changes as a list
class PDF4QTLIBCORESHARED_EXPORT PDFVersionHistory
{
public:
    PDFVersionHistory() = delete;

    struct Release
    {
        QString version;
        QDate date;
        QString summary;
        QStringList changes;
    };
    using Releases = std::vector<Release>;

    /// Parses the history (newest first, as written)
    static Releases parse(const QString& markdown);

    /// Compares two versions numerically ("1.10.0" is newer than "1.9.2"); returns a
    /// negative number when a is older, zero when equal, positive when a is newer
    static int compare(const QString& a, const QString& b);

    /// Escapes the text for Markdown, so it is shown as written (no markup, no HTML)
    static QString escapeMarkdown(const QString& text);

    /// Returns true, if the text is a version of numbers separated by dots
    static bool isValidVersion(const QString& version);

    /// Returns the releases newer than the version, newest first
    static Releases getNewer(const Releases& releases, const QString& version);

    /// Returns the history embedded in the program (the CHANGELOG.md of its sources)
    static QString getEmbeddedHistory();

    /// Formats releases as Markdown for display
    static QString toMarkdown(const Releases& releases);
};

}   // namespace pdf

#endif // PDFVERSIONHISTORY_H
