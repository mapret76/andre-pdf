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

#include "pdfcontractpack.h"
#include "pdfaction.h"
#include "pdfoutline.h"
#include "pdfdocumentmanipulator.h"
#include "pdftextlayout.h"
#include "pdftextlayoutgenerator.h"
#include "pdfcms.h"
#include "pdffont.h"
#include "pdfcolorspaces.h"
#include "pdfconstants.h"
#include "pdfoptionalcontent.h"

#include <QRegularExpression>

#include <algorithm>

#include "pdfdbgheap.h"

namespace pdf
{

PDFContractPack::Parts PDFContractPack::fromStartPages(std::vector<PDFInteger> startPages, PDFInteger pageCount, const QStringList& names)
{
    Parts parts;
    if (pageCount <= 0)
    {
        return parts;
    }

    // Keep the names with their pages while sorting and removing duplicates
    std::vector<std::pair<PDFInteger, QString>> starts;
    for (size_t i = 0; i < startPages.size(); ++i)
    {
        if (startPages[i] >= 0 && startPages[i] < pageCount)
        {
            starts.emplace_back(startPages[i], i < size_t(names.size()) ? names[i] : QString());
        }
    }
    std::stable_sort(starts.begin(), starts.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    starts.erase(std::unique(starts.begin(), starts.end(), [](const auto& a, const auto& b) { return a.first == b.first; }), starts.end());
    if (starts.empty() || starts.front().first != 0)
    {
        starts.insert(starts.begin(), { 0, QString() });
    }

    for (size_t i = 0; i < starts.size(); ++i)
    {
        Part part;
        part.firstPage = starts[i].first;
        part.lastPage = i + 1 < starts.size() ? starts[i + 1].first - 1 : pageCount - 1;
        part.name = starts[i].second.isEmpty() ? tr("Part %1").arg(i + 1) : starts[i].second.simplified();
        parts.push_back(part);
    }
    return parts;
}

PDFContractPack::Parts PDFContractPack::suggestFromOutline(const PDFDocument* document)
{
    const PDFCatalog* catalog = document->getCatalog();
    const QSharedPointer<PDFOutlineItem> root = catalog->getOutlineRootPtr();
    if (!root)
    {
        return Parts();
    }

    std::vector<PDFInteger> startPages;
    QStringList names;
    for (size_t i = 0; i < root->getChildCount(); ++i)
    {
        const PDFOutlineItem* item = root->getChild(i);
        const PDFActionGoTo* goTo = dynamic_cast<const PDFActionGoTo*>(item->getAction());
        if (!goTo)
        {
            continue;
        }

        const PDFDestination* destination = &goTo->getDestination();
        if (destination->getDestinationType() == DestinationType::Named)
        {
            destination = catalog->getNamedDestination(destination->getName());
        }
        if (!destination)
        {
            continue;
        }

        PDFInteger pageIndex = -1;
        if (destination->getPageReference().isValid())
        {
            const size_t index = catalog->getPageIndexFromPageReference(destination->getPageReference());
            pageIndex = index == PDFCatalog::INVALID_PAGE_INDEX ? -1 : PDFInteger(index);
        }
        else
        {
            pageIndex = destination->getPageIndex();
        }

        if (pageIndex >= 0)
        {
            startPages.push_back(pageIndex);
            names << item->getTitle();
        }
    }

    if (startPages.size() < 2)
    {
        return Parts();
    }
    return fromStartPages(startPages, PDFInteger(catalog->getPageCount()), names);
}

PDFContractPack::Parts PDFContractPack::suggestFromPageNumbers(const PDFDocument* document)
{
    const PDFCatalog* catalog = document->getCatalog();

    PDFFontCache fontCache(DEFAULT_FONT_CACHE_LIMIT, DEFAULT_REALIZED_FONT_CACHE_LIMIT);
    PDFICCProfileCache iccProfileCache;
    PDFCMSGeneric cms;
    PDFMeshQualitySettings meshQuality;
    PDFOptionalContentActivity optionalContentActivity(document, OCUsage::View, nullptr);
    PDFModifiedDocument modifiedDocument(const_cast<PDFDocument*>(document), &optionalContentActivity);
    fontCache.setDocument(modifiedDocument);
    iccProfileCache.setDocument(modifiedDocument);

    static const QRegularExpression firstPage(QStringLiteral("(\\b(page|bladsy|p\\.)\\s*1\\s*(of|van|/)\\s*\\d+\\b)|(^\\s*1\\s*(of|van|/)\\s*\\d+\\s*$)"),
                                              QRegularExpression::CaseInsensitiveOption | QRegularExpression::MultilineOption);

    std::vector<PDFInteger> startPages;
    for (size_t pageIndex = 0; pageIndex < catalog->getPageCount(); ++pageIndex)
    {
        PDFTextLayoutGenerator generator(PDFRenderer::IgnoreOptionalContent, catalog->getPage(pageIndex), document, &fontCache, &iccProfileCache, &cms, &optionalContentActivity, QTransform(), meshQuality);
        generator.processContents();
        const PDFTextLayout layout = generator.createTextLayout();
        for (const PDFTextFlow& flow : PDFTextFlow::createTextFlows(layout, PDFTextFlow::SeparateBlocks, PDFInteger(pageIndex)))
        {
            if (firstPage.match(flow.getText()).hasMatch())
            {
                startPages.push_back(PDFInteger(pageIndex));
                break;
            }
        }
    }

    if (startPages.size() < 2)
    {
        return Parts();
    }
    return fromStartPages(startPages, PDFInteger(catalog->getPageCount()));
}

PDFContractPack::Parts PDFContractPack::parse(const QString& text, PDFInteger pageCount, QString* errorMessage)
{
    Parts parts;
    errorMessage->clear();

    static const QRegularExpression rangeExpression(QStringLiteral("^\\s*(\\d+)\\s*(-\\s*(\\d+))?\\s*$"));
    const QStringList items = text.split(QChar(','), Qt::SkipEmptyParts);
    for (const QString& item : items)
    {
        const QRegularExpressionMatch match = rangeExpression.match(item);
        if (!match.hasMatch())
        {
            *errorMessage = tr("'%1' is not a page range.").arg(item.trimmed());
            return Parts();
        }

        Part part;
        part.firstPage = match.captured(1).toLongLong() - 1;
        part.lastPage = match.captured(3).isEmpty() ? part.firstPage : match.captured(3).toLongLong() - 1;
        if (part.firstPage < 0 || part.lastPage >= pageCount || part.firstPage > part.lastPage)
        {
            *errorMessage = tr("The range '%1' is not in the document of %2 pages.").arg(item.trimmed()).arg(pageCount);
            return Parts();
        }
        part.name = tr("Part %1").arg(parts.size() + 1);
        parts.push_back(part);
    }

    for (size_t i = 0; i < parts.size(); ++i)
    {
        for (size_t j = i + 1; j < parts.size(); ++j)
        {
            if (parts[i].firstPage <= parts[j].lastPage && parts[j].firstPage <= parts[i].lastPage)
            {
                *errorMessage = tr("The ranges of %1 and %2 overlap.").arg(parts[i].name, parts[j].name);
                return Parts();
            }
        }
    }

    return parts;
}

QString PDFContractPack::toText(const Parts& parts)
{
    QStringList ranges;
    for (const Part& part : parts)
    {
        ranges << (part.firstPage == part.lastPage ? QString::number(part.firstPage + 1) : QString("%1-%2").arg(part.firstPage + 1).arg(part.lastPage + 1));
    }
    return ranges.join(", ");
}

PDFDocument PDFContractPack::extract(const PDFDocument* document, const Part& part)
{
    PDFDocumentManipulator manipulator;
    manipulator.addDocument(0, document);
    manipulator.setOutlineMode(PDFDocumentManipulator::OutlineMode::NoOutline);

    PDFDocumentManipulator::AssembledPages pages;
    for (PDFInteger pageIndex = part.firstPage; pageIndex <= part.lastPage; ++pageIndex)
    {
        const PDFPage* page = document->getCatalog()->getPage(pageIndex);
        PDFDocumentManipulator::AssembledPage assembledPage;
        assembledPage.documentIndex = 0;
        assembledPage.pageIndex = pageIndex;
        assembledPage.pageSize = page->getMediaBox().size();
        assembledPage.sourcePageRotation = page->getPageRotation();
        assembledPage.pageRotation = page->getPageRotation();
        pages.push_back(assembledPage);
    }

    if (!manipulator.assemble(pages))
    {
        return PDFDocument();
    }
    return manipulator.getAssembledDocument();
}

QString PDFContractPack::getFileName(const QString& baseName, const Part& part, size_t index)
{
    static const QRegularExpression unsafe(QStringLiteral("[\\\\/:*?\"<>|\\x00-\\x1F]"));
    QString name = QString(part.name).replace(unsafe, QStringLiteral(" ")).simplified();
    if (name.isEmpty())
    {
        name = tr("Part %1").arg(index + 1);
    }
    return QString("%1 - %2 %3.pdf").arg(baseName).arg(index + 1, 2, 10, QChar('0')).arg(name.left(80));
}

}   // namespace pdf
