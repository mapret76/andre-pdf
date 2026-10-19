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

#include "pdfsignaturelinefinder.h"
#include "pdfdocument.h"
#include "pdfform.h"
#include "pdfdocumentbuilder.h"
#include "pdfsignaturehandler.h"
#include "pdftextlayout.h"
#include "pdftextlayoutgenerator.h"
#include "pdfcms.h"
#include "pdffont.h"
#include "pdfcolorspaces.h"
#include "pdfoptionalcontent.h"
#include "pdfconstants.h"
#include "pdfpageinitials.h"
#include "pdfpage.h"

#include <QRegularExpression>

#include <algorithm>

#include "pdfdbgheap.h"

namespace pdf
{

// Width of the signature area right of a label without a line, in points
static constexpr PDFReal LABEL_SIGNATURE_WIDTH = 160.0;

bool PDFSignatureLineFinder::isSignatureLabel(const QString& text)
{
    constexpr QRegularExpression::PatternOptions options = QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption;

    // The attestation clause "Signed at ___ on this ___ day of ___" has lines for the
    // place and the date, not for a signature
    static const QRegularExpression attestation(QStringLiteral("\\b(sign(ed)?|geteken|onderteken)\\s+(at|on|this|te|op)\\b|\\bday\\s+of\\b|\\bdag\\s+van\\b|\\bon\\s+this\\b"), options);
    // These name the signature itself, also next to a date ("Signature & Date")
    static const QRegularExpression explicitSignature(QStringLiteral("\\b(signature|handtekening|initials?|paraaf|parafeer)\\b"), options);
    // Lines for the name, the date or the place of the signing are not signed on,
    // even when the label mentions the signatory ("Name of signatory")
    static const QRegularExpression other(QStringLiteral("\\b(name|naam|date|datum|place|plek|capacity|hoedanigheid|id|identity|identiteit|address|adres|print|full names|volle name)\\b"), options);
    static const QRegularExpression signature(QStringLiteral("(sign|geteken|witness|getuie|onderteken)|\\bon\\s+behalf\\s+of\\b|\\bnamens\\b|"
                                                             "\\bfor\\s+the\\s+(purchaser|seller|buyer|lessor|lessee|landlord|tenant|company|employer|employee|contractor|client|owner|borrower|lender|surety)\\b|"
                                                             "\\bvir\\s+die\\s+(koper|verkoper|verhuurder|huurder|maatskappy|werkgewer|werknemer|eienaar)\\b"), options);

    if (attestation.match(text).hasMatch())
    {
        return false;
    }
    if (explicitSignature.match(text).hasMatch())
    {
        return true;
    }
    return !other.match(text).hasMatch() && signature.match(text).hasMatch();
}

PDFDocumentPointer PDFSignatureLineFinder::addSignatureFields(const PDFDocument* document, const std::vector<SignatureLine>& lines, const QString& namePrefix)
{
    PDFDocumentModifier modifier(document);
    PDFDocumentBuilder* builder = modifier.getBuilder();
    int count = 0;
    for (const SignatureLine& line : lines)
    {
        const PDFPage* page = document->getCatalog()->getPage(line.pageIndex);
        if (!page || line.source == Source::SignatureField || line.rectangle.isEmpty())
        {
            continue;
        }

        ++count;
        const PDFObjectReference field = builder->createSignatureField(QString("%1%2").arg(namePrefix).arg(count), PDFObjectReference(), page->getPageReference());
        PDFObjectFactory factory;
        factory.beginDictionary();
        factory.beginDictionaryItem("Rect");
        factory << line.rectangle;
        factory.endDictionaryItem();
        factory.endDictionary();
        builder->mergeTo(field, factory.takeObject());
    }

    if (count == 0)
    {
        return nullptr;
    }
    modifier.markFormFieldChanged();
    modifier.markAnnotationsChanged();
    if (!modifier.finalize())
    {
        return nullptr;
    }
    return modifier.getDocument();
}

std::vector<PDFSignatureLineFinder::SignatureLine> PDFSignatureLineFinder::find(const PDFDocument* document)
{
    std::vector<SignatureLine> lines;
    if (!document)
    {
        return lines;
    }

    const PDFCatalog* catalog = document->getCatalog();
    PDFDocumentDataLoaderDecorator loader(document);

    // Unsigned signature fields
    const PDFForm form = PDFForm::parse(document, catalog->getFormObject());
    form.apply([&](const PDFFormField* field)
    {
        const PDFFormFieldSignature* signatureField = dynamic_cast<const PDFFormFieldSignature*>(field);
        if (!signatureField || !signatureField->getSignature().getContents().isEmpty())
        {
            return;
        }

        for (const PDFFormWidget& widget : field->getWidgets())
        {
            const PDFDictionary* widgetDictionary = document->getDictionaryFromObject(document->getObjectByReference(widget.getWidget()));
            size_t pageIndex = catalog->getPageIndexFromPageReference(widget.getPage());
            for (size_t i = 0; pageIndex == PDFCatalog::INVALID_PAGE_INDEX && i < catalog->getPageCount(); ++i)
            {
                // The page entry /P of a widget is optional, the page lists it in /Annots
                const std::vector<PDFObjectReference>& annotations = catalog->getPage(i)->getAnnotations();
                if (std::find(annotations.cbegin(), annotations.cend(), widget.getWidget()) != annotations.cend())
                {
                    pageIndex = i;
                }
            }
            if (!widgetDictionary || pageIndex == PDFCatalog::INVALID_PAGE_INDEX)
            {
                continue;
            }

            SignatureLine line;
            line.pageIndex = PDFInteger(pageIndex);
            line.rectangle = loader.readRectangle(widgetDictionary->get("Rect"), QRectF()).normalized();
            line.source = Source::SignatureField;
            line.fieldName = field->getName(PDFFormField::FullyQualified);
            line.label = line.fieldName;
            if (line.rectangle.isValid())
            {
                lines.push_back(line);
            }
        }
    });
    const size_t fieldCount = lines.size();

    // Lines and labels in the text
    PDFFontCache fontCache(DEFAULT_FONT_CACHE_LIMIT, DEFAULT_REALIZED_FONT_CACHE_LIMIT);
    PDFICCProfileCache iccProfileCache;
    PDFCMSGeneric cms;
    PDFMeshQualitySettings meshQuality;
    PDFOptionalContentActivity optionalContentActivity(document, OCUsage::View, nullptr);
    PDFModifiedDocument modifiedDocument(const_cast<PDFDocument*>(document), &optionalContentActivity);
    fontCache.setDocument(modifiedDocument);
    iccProfileCache.setDocument(modifiedDocument);

    static const QRegularExpression lineExpression(QStringLiteral("(_{6,}|\\.{10,}|…{4,})"));
    static const QRegularExpression labelOnlyExpression(QStringLiteral("(signature|signed|handtekening|geteken)\\s*:?\\s*$"), QRegularExpression::CaseInsensitiveOption);

    for (size_t pageIndex = 0; pageIndex < catalog->getPageCount(); ++pageIndex)
    {
        const PDFPage* page = catalog->getPage(pageIndex);
        PDFTextLayoutGenerator generator(PDFRenderer::IgnoreOptionalContent, page, document, &fontCache, &iccProfileCache, &cms, &optionalContentActivity, QTransform(), meshQuality);
        generator.processContents();
        const PDFTextLayout layout = generator.createTextLayout();

        // The lines are found in the page as displayed (a page turned by /Rotate has its
        // lines across the user space); the rectangles go back to the page space
        const QTransform displayToPage = PDFPageInitials::getDisplayToPageTransform(page);
        const QTransform pageToDisplay = displayToPage.inverted();

        // All text lines of the page, a label can be in another text block than its line
        struct TextLine
        {
            QString text;
            std::vector<QRectF> boxes;
            QRectF box;
        };
        std::vector<TextLine> textLines;
        for (const PDFTextFlow& flow : PDFTextFlow::createTextFlows(layout, PDFTextFlow::SeparateBlocks, PDFInteger(pageIndex)))
        {
            const QString text = flow.getText();
            const std::vector<QRectF> boxes = flow.getBoundingBoxes();
            if (boxes.size() < size_t(text.size()))
            {
                continue;
            }

            qsizetype lineStart = 0;
            for (const QString& lineText : text.split(QChar('\n')))
            {
                TextLine textLine;
                textLine.text = lineText;
                textLine.boxes.reserve(lineText.size());
                for (qsizetype i = 0; i < lineText.size(); ++i)
                {
                    const QRectF box = pageToDisplay.mapRect(boxes[lineStart + i]);
                    textLine.boxes.push_back(box);
                    textLine.box = textLine.box.united(box);
                }
                lineStart += lineText.size() + 1;
                if (!lineText.trimmed().isEmpty())
                {
                    textLines.push_back(std::move(textLine));
                }
            }
        }

        auto getBox = [](const TextLine& textLine, qsizetype begin, qsizetype end)
        {
            QRectF box;
            for (qsizetype i = begin; i < end; ++i)
            {
                box = box.united(textLine.boxes[i]);
            }
            return box;
        };
        auto getLabel = [](const QString& text) { return QString(text).replace(lineExpression, QStringLiteral(" ")).simplified(); };

        // The nearest line under (below = true) or over the given one, which overlaps
        // it horizontally and is at most two and a half line heights away
        auto getNeighbour = [&textLines](const TextLine& textLine, bool below) -> const TextLine*
        {
            const TextLine* result = nullptr;
            qreal resultDistance = 2.5 * qMax(textLine.box.height(), 8.0);
            for (const TextLine& other : textLines)
            {
                if (&other == &textLine || other.box.right() < textLine.box.left() || other.box.left() > textLine.box.right())
                {
                    continue;
                }
                const qreal distance = below ? textLine.box.center().y() - other.box.center().y() : other.box.center().y() - textLine.box.center().y();
                if (distance > 0.0 && distance < resultDistance)
                {
                    result = &other;
                    resultDistance = distance;
                }
            }
            return result;
        };

        for (const TextLine& textLine : textLines)
        {
            bool hasRun = false;

            QRegularExpressionMatchIterator it = lineExpression.globalMatch(textLine.text);
            while (it.hasNext())
            {
                const QRegularExpressionMatch match = it.next();
                hasRun = true;

                // The label on the same line decides; a line standing alone is
                // labelled by the line under it ("________" over "Signature"), or
                // by the line over it.
                QString label = getLabel(textLine.text);
                for (bool below : { true, false })
                {
                    if (label.isEmpty())
                    {
                        if (const TextLine* neighbour = getNeighbour(textLine, below))
                        {
                            label = getLabel(neighbour->text);
                        }
                    }
                }
                if (!isSignatureLabel(label))
                {
                    continue;
                }

                const QRectF runBox = getBox(textLine, match.capturedStart(), match.capturedEnd());
                SignatureLine line;
                line.pageIndex = PDFInteger(pageIndex);
                line.rectangle = displayToPage.mapRect(QRectF(runBox.left(), runBox.top(), runBox.width(), SIGNATURE_HEIGHT));
                line.source = Source::Line;
                line.label = label;
                lines.push_back(line);
            }

            if (!hasRun && textLine.text.size() < 60 && labelOnlyExpression.match(textLine.text).hasMatch() && isSignatureLabel(textLine.text))
            {
                SignatureLine line;
                line.pageIndex = PDFInteger(pageIndex);
                line.rectangle = displayToPage.mapRect(QRectF(textLine.box.right() + 6.0, textLine.box.top(), LABEL_SIGNATURE_WIDTH, SIGNATURE_HEIGHT));
                line.source = Source::Label;
                line.label = textLine.text.simplified();
                lines.push_back(line);
            }
        }
    }

    // A line under a signature field is the field itself
    auto isCoveredByField = [&](const SignatureLine& line)
    {
        for (size_t i = 0; i < fieldCount; ++i)
        {
            const QRectF overlap = lines[i].rectangle.intersected(line.rectangle);
            const qreal smaller = qMin(lines[i].rectangle.width() * lines[i].rectangle.height(), line.rectangle.width() * line.rectangle.height());
            if (lines[i].pageIndex == line.pageIndex && !overlap.isEmpty() && overlap.width() * overlap.height() > 0.3 * smaller)
            {
                return true;
            }
        }
        return false;
    };
    std::vector<SignatureLine> result(lines.begin(), lines.begin() + fieldCount);
    std::copy_if(lines.begin() + fieldCount, lines.end(), std::back_inserter(result), [&](const SignatureLine& line) { return !isCoveredByField(line); });

    // Top of the displayed page first (y goes up), then from the left
    std::stable_sort(result.begin(), result.end(), [catalog](const SignatureLine& a, const SignatureLine& b)
    {
        if (a.pageIndex != b.pageIndex)
        {
            return a.pageIndex < b.pageIndex;
        }
        const QTransform pageToDisplay = PDFPageInitials::getDisplayToPageTransform(catalog->getPage(a.pageIndex)).inverted();
        const QRectF displayA = pageToDisplay.mapRect(a.rectangle);
        const QRectF displayB = pageToDisplay.mapRect(b.rectangle);
        if (!qFuzzyCompare(displayA.top(), displayB.top()))
        {
            return displayA.top() > displayB.top();
        }
        return displayA.left() < displayB.left();
    });
    return result;
}

}   // namespace pdf
