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

#include "pdftextredline.h"
#include "pdfofficeexporter.h"
#include "pdfdocumentbuilder.h"

#include <QImage>
#include <QPainter>
#include <QTextDocument>
#include <QAbstractTextDocumentLayout>

#include "pdfdbgheap.h"

namespace pdf
{

PDFTextRedline::Words PDFTextRedline::getWords(const PDFDocument* document)
{
    Words words;
    const PDFOfficeExporter::Pages pages = PDFOfficeExporter::extract(document);
    for (size_t pageIndex = 0; pageIndex < pages.size(); ++pageIndex)
    {
        for (const PDFOfficeExporter::TextLine& line : pages[pageIndex])
        {
            for (const QString& word : line.text.split(QChar(' '), Qt::SkipEmptyParts))
            {
                words.push_back({ word, PDFInteger(pageIndex) });
            }
        }
    }
    return words;
}

PDFTextRedline::Changes PDFTextRedline::compare(const Words& oldWords, const Words& newWords)
{
    // The common beginning and end are skipped, the diff runs on the rest
    const int oldSize = int(oldWords.size());
    const int newSize = int(newWords.size());
    int prefix = 0;
    while (prefix < oldSize && prefix < newSize && oldWords[prefix].text == newWords[prefix].text)
    {
        ++prefix;
    }
    int suffix = 0;
    while (suffix < oldSize - prefix && suffix < newSize - prefix && oldWords[oldSize - 1 - suffix].text == newWords[newSize - 1 - suffix].text)
    {
        ++suffix;
    }

    const int n = oldSize - prefix - suffix;
    const int m = newSize - prefix - suffix;
    auto equal = [&](int x, int y) { return oldWords[prefix + x].text == newWords[prefix + y].text; };

    // Myers' O(ND) diff, the furthest reaching paths of every distance are kept for
    // the backtracking. Edit script as (old index, new index) of the matched words.
    std::vector<std::pair<int, int>> matches;
    const int maximal = qMin(n + m, MAXIMAL_EDIT_DISTANCE);
    const int offset = maximal + 1;
    std::vector<int> v(2 * maximal + 3, 0);
    std::vector<std::vector<int>> trace;
    bool isFound = (n == 0 && m == 0);
    int distance = 0;
    for (int d = 0; d <= maximal && !isFound; ++d)
    {
        trace.push_back(v);
        for (int k = -d; k <= d; k += 2)
        {
            int x = (k == -d || (k != d && v[offset + k - 1] < v[offset + k + 1])) ? v[offset + k + 1] : v[offset + k - 1] + 1;
            int y = x - k;
            while (x < n && y < m && equal(x, y))
            {
                ++x;
                ++y;
            }
            v[offset + k] = x;
            if (x >= n && y >= m)
            {
                isFound = true;
                distance = d;
                break;
            }
        }
    }

    if (isFound)
    {
        int x = n;
        int y = m;
        for (int d = distance; d > 0; --d)
        {
            const std::vector<int>& previous = trace[d];
            const int k = x - y;
            const int previousK = (k == -d || (k != d && previous[offset + k - 1] < previous[offset + k + 1])) ? k + 1 : k - 1;
            const int previousX = previous[offset + previousK];
            const int previousY = previousX - previousK;
            while (x > previousX && y > previousY)
            {
                --x;
                --y;
                matches.emplace_back(x, y);
            }
            x = previousX;
            y = previousY;
        }
        while (x > 0 && y > 0)
        {
            --x;
            --y;
            matches.emplace_back(x, y);
        }
        std::reverse(matches.begin(), matches.end());
    }
    // ponytail: beyond MAXIMAL_EDIT_DISTANCE edits the middle is one replaced block;
    // a version with that many changes is a different document, not a redline.

    // Runs of words between the matched words are the changes
    auto join = [](const Words& words, int begin, int end)
    {
        QStringList list;
        for (int i = qMax(begin, 0); i < qMin(end, int(words.size())); ++i)
        {
            list << words[i].text;
        }
        return list.join(QChar(' '));
    };
    auto pageOf = [](const Words& words, int index) { return words.empty() ? PDFInteger(0) : words[qBound(0, index, int(words.size()) - 1)].pageIndex; };

    Changes changes;
    int oldIndex = 0;
    int newIndex = 0;
    matches.emplace_back(n, m); // end sentinel
    for (const auto& [matchedOld, matchedNew] : matches)
    {
        if (matchedOld > oldIndex || matchedNew > newIndex)
        {
            const int oldBegin = prefix + oldIndex;
            const int oldEnd = prefix + matchedOld;
            const int newBegin = prefix + newIndex;
            const int newEnd = prefix + matchedNew;

            Change change;
            change.oldText = join(oldWords, oldBegin, oldEnd);
            change.newText = join(newWords, newBegin, newEnd);
            change.type = change.oldText.isEmpty() ? ChangeType::Inserted : (change.newText.isEmpty() ? ChangeType::Deleted : ChangeType::Replaced);
            change.oldPage = pageOf(oldWords, oldBegin);
            change.newPage = pageOf(newWords, newBegin);
            change.before = join(newWords, newBegin - CONTEXT_WORDS, newBegin);
            change.after = join(newWords, newEnd, newEnd + CONTEXT_WORDS);
            changes.push_back(change);
        }
        oldIndex = matchedOld + 1;
        newIndex = matchedNew + 1;
    }

    return changes;
}

PDFDocument PDFTextRedline::createReport(const Changes& changes, const QString& oldName, const QString& newName)
{
    QString html = QStringLiteral("<h2>%1</h2><p>%2<br/>%3</p><p><b>%4</b></p>")
                       .arg(tr("Redline").toHtmlEscaped(),
                            tr("Old version: %1").arg(oldName).toHtmlEscaped(),
                            tr("New version: %1").arg(newName).toHtmlEscaped(),
                            (changes.empty() ? tr("The texts are the same.") : tr("%n change(s). Deleted text is struck through in red, inserted text underlined in blue.", nullptr, int(changes.size()))).toHtmlEscaped());
    for (size_t i = 0; i < changes.size(); ++i)
    {
        const Change& change = changes[i];
        const QString page = change.oldPage == change.newPage ? tr("Page %1").arg(change.newPage + 1) : tr("Page %1 (was page %2)").arg(change.newPage + 1).arg(change.oldPage + 1);
        QString text = change.before.toHtmlEscaped();
        if (!change.oldText.isEmpty())
        {
            text += QStringLiteral(" <span style=\"color:#c00000; text-decoration:line-through;\">%1</span>").arg(change.oldText.toHtmlEscaped());
        }
        if (!change.newText.isEmpty())
        {
            text += QStringLiteral(" <span style=\"color:#0050c8; text-decoration:underline;\">%1</span>").arg(change.newText.toHtmlEscaped());
        }
        text += QStringLiteral(" ") + change.after.toHtmlEscaped();
        html += QStringLiteral("<p><b>%1. %2</b><br/>%3</p>").arg(i + 1).arg(page.toHtmlEscaped(), text);
    }

    // A4 pages, the text flows over as many pages as it needs
    constexpr qreal pageWidth = 595.0;
    constexpr qreal pageHeight = 842.0;
    constexpr qreal margin = 56.0;
    const QSizeF textSize(pageWidth - 2 * margin, pageHeight - 2 * margin);

    // The text is laid out for 72 dpi, the resolution of the PDF writer of the content
    // builder; the device of the layout must live as long as the text document
    QImage layoutDevice(1, 1, QImage::Format_ARGB32);
    layoutDevice.setDotsPerMeterX(qRound(72.0 / 0.0254));
    layoutDevice.setDotsPerMeterY(qRound(72.0 / 0.0254));

    PDFDocumentBuilder builder;
    QTextDocument textDocument;
    textDocument.documentLayout()->setPaintDevice(&layoutDevice);
    textDocument.setDefaultFont(QFont(QStringLiteral("Arial"), 10));
    textDocument.setHtml(html);
    textDocument.setPageSize(textSize);
    const int pageCount = qMax(1, textDocument.pageCount());
    for (int pageIndex = 0; pageIndex < pageCount; ++pageIndex)
    {
        const PDFObjectReference page = builder.appendPage(QRectF(0, 0, pageWidth, pageHeight));
        PDFPageContentStreamBuilder content(&builder, PDFContentStreamBuilder::CoordinateSystem::Qt, PDFPageContentStreamBuilder::Mode::Replace);
        QPainter* painter = content.begin(page);
        painter->translate(margin, margin - pageIndex * textSize.height());
        const QRectF clip(0, pageIndex * textSize.height(), textSize.width(), textSize.height());
        textDocument.drawContents(painter, clip);
        content.end(painter);
    }

    return builder.build();
}

}   // namespace pdf
