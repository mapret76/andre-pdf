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

#include "pdfversionhistory.h"

#include <QFile>
#include <QRegularExpression>

#include <algorithm>

#include "pdfdbgheap.h"

namespace pdf
{

bool PDFVersionHistory::isValidVersion(const QString& version)
{
    static const QRegularExpression expression(QRegularExpression::anchoredPattern(QStringLiteral("\\d{1,5}(\\.\\d{1,5}){0,3}")));
    return expression.match(version).hasMatch();
}

int PDFVersionHistory::compare(const QString& a, const QString& b)
{
    const QStringList partsA = a.split(QChar('.'));
    const QStringList partsB = b.split(QChar('.'));
    for (qsizetype i = 0; i < qMax(partsA.size(), partsB.size()); ++i)
    {
        const int numberA = partsA.value(i, QStringLiteral("0")).toInt();
        const int numberB = partsB.value(i, QStringLiteral("0")).toInt();
        if (numberA != numberB)
        {
            return numberA < numberB ? -1 : 1;
        }
    }
    return 0;
}

PDFVersionHistory::Releases PDFVersionHistory::parse(const QString& markdown)
{
    static const QRegularExpression heading(QStringLiteral("^##\\s+(\\d[\\d.]*)\\s*(?:-\\s*(\\d{4}-\\d{2}-\\d{2}))?\\s*$"));
    static const QRegularExpression summary(QStringLiteral("^Summary:\\s*(.+)$"));
    static const QRegularExpression item(QStringLiteral("^[-*]\\s+(.+)$"));

    Releases releases;
    bool isInValidRelease = false;  // lines under a heading of an invalid version belong to no release
    for (const QString& rawLine : markdown.split(QChar('\n')))
    {
        const QString line = rawLine.trimmed();
        if (const QRegularExpressionMatch match = heading.match(line); match.hasMatch())
        {
            Release release;
            release.version = match.captured(1);
            release.date = QDate::fromString(match.captured(2), Qt::ISODate);
            isInValidRelease = isValidVersion(release.version);
            if (isInValidRelease)
            {
                releases.push_back(release);
            }
            continue;
        }
        if (releases.empty() || !isInValidRelease)
        {
            continue;
        }
        if (const QRegularExpressionMatch match = summary.match(line); match.hasMatch() && releases.back().summary.isEmpty())
        {
            releases.back().summary = match.captured(1).trimmed();
        }
        else if (const QRegularExpressionMatch match = item.match(line); match.hasMatch())
        {
            releases.back().changes << match.captured(1).trimmed();
        }
        else if (!line.isEmpty() && !releases.back().changes.isEmpty() && rawLine.startsWith(QStringLiteral("  ")))
        {
            // A change continued on the next line
            releases.back().changes.last() += QChar(' ') + line;
        }
    }

    std::stable_sort(releases.begin(), releases.end(), [](const Release& x, const Release& y) { return compare(x.version, y.version) > 0; });
    return releases;
}

PDFVersionHistory::Releases PDFVersionHistory::getNewer(const Releases& releases, const QString& version)
{
    Releases result;
    std::copy_if(releases.cbegin(), releases.cend(), std::back_inserter(result), [&version](const Release& release) { return compare(release.version, version) > 0; });
    return result;
}

QString PDFVersionHistory::getEmbeddedHistory()
{
    QFile file(QStringLiteral(":/andre-pdf/CHANGELOG.md"));
    if (!file.open(QFile::ReadOnly))
    {
        return QString();
    }
    return QString::fromUtf8(file.readAll());
}

QString PDFVersionHistory::escapeMarkdown(const QString& text)
{
    // Text of the changelog or of the manifest is shown as written: every ASCII
    // punctuation character is escaped (CommonMark), line breaks become spaces
    QString result;
    result.reserve(text.size() * 2);
    for (const QChar character : text)
    {
        if (character == QChar('\n') || character == QChar('\r'))
        {
            result += QChar(' ');
            continue;
        }
        const char16_t code = character.unicode();
        if (code < 128 && (character.isPunct() || character.isSymbol()))
        {
            result += QChar('\\');
        }
        result += character;
    }
    return result;
}

QString PDFVersionHistory::toMarkdown(const Releases& releases)
{
    QString markdown;
    for (const Release& release : releases)
    {
        markdown += QStringLiteral("## %1%2\n\n").arg(release.version, release.date.isValid() ? QStringLiteral(" - ") + release.date.toString(Qt::ISODate) : QString());
        if (!release.summary.isEmpty())
        {
            markdown += QStringLiteral("**%1**\n\n").arg(escapeMarkdown(release.summary));
        }
        for (const QString& change : release.changes)
        {
            markdown += QStringLiteral("- %1\n").arg(escapeMarkdown(change));
        }
        markdown += QStringLiteral("\n");
    }
    return markdown;
}

}   // namespace pdf
