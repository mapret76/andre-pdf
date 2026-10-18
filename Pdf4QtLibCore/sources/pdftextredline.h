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

#ifndef PDFTEXTREDLINE_H
#define PDFTEXTREDLINE_H

#include "pdfglobal.h"
#include "pdfdocument.h"

#include <QString>
#include <QCoreApplication>

#include <vector>

namespace pdf
{

/// Redline of two versions of a contract: the words deleted, inserted or replaced
/// between them, found by a word diff of the text (so that a reflowed paragraph or a
/// moved page boundary does not count as a change), and a PDF report listing every
/// change in context - deletions struck through in red, insertions underlined in blue.
class PDF4QTLIBCORESHARED_EXPORT PDFTextRedline
{
    Q_DECLARE_TR_FUNCTIONS(pdf::PDFTextRedline)

public:
    PDFTextRedline() = delete;

    struct Word
    {
        QString text;
        PDFInteger pageIndex = 0;
    };
    using Words = std::vector<Word>;

    enum class ChangeType
    {
        Deleted,
        Inserted,
        Replaced
    };

    struct Change
    {
        ChangeType type = ChangeType::Replaced;
        QString oldText;
        QString newText;
        PDFInteger oldPage = -1;    ///< Page of the old text (or of the place of an insertion)
        PDFInteger newPage = -1;    ///< Page of the new text (or of the place of a deletion)
        QString before;             ///< Words before the change
        QString after;              ///< Words after the change
    };
    using Changes = std::vector<Change>;

    /// Returns the words of the document in reading order
    static Words getWords(const PDFDocument* document);

    /// Compares two word sequences
    static Changes compare(const Words& oldWords, const Words& newWords);

    /// Creates the redline report
    /// \param changes Changes
    /// \param oldName Name of the old version
    /// \param newName Name of the new version
    static PDFDocument createReport(const Changes& changes, const QString& oldName, const QString& newName);

    /// Largest number of edits the diff searches for; beyond it the texts are reported
    /// as replaced as a whole (two unrelated documents)
    static constexpr int MAXIMAL_EDIT_DISTANCE = 6000;

    /// Number of words of context before and after a change
    static constexpr int CONTEXT_WORDS = 6;
};

}   // namespace pdf

#endif // PDFTEXTREDLINE_H
