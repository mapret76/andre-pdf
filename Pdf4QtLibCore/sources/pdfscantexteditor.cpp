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

#include "pdfscantexteditor.h"
#include "pdfsignaturelibrary.h"
#include "pdfdocumentbuilder.h"
#include "pdftextlayout.h"
#include "pdftextlayoutgenerator.h"
#include "pdfrenderer.h"
#include "pdfcms.h"
#include "pdffont.h"
#include "pdfcolorspaces.h"
#include "pdfconstants.h"
#include "pdfoptionalcontent.h"
#include "pdfocrtextlayerwriter.h"
#include "pdfpagecontenteditorprocessor.h"
#include "pdfpagecontenteditorcontentstreambuilder.h"
#include "pdfstreamfilters.h"
#include "pdfobjectutils.h"

#include <QPainter>
#include <QFontMetricsF>

#include "pdfdbgheap.h"

namespace pdf
{

QImage PDFScanTextEditor::renderArea(const PDFDocument* document, PDFInteger pageIndex, const QRectF& area, double dpi)
{
    const double scale = dpi / 72.0;
    const QSize size(qMax(1, qCeil(area.width() * scale)), qMax(1, qCeil(area.height() * scale)));
    QImage image(size, QImage::Format_RGB32);
    image.fill(Qt::white);

    PDFCMSGeneric cms;
    PDFFontCache fontCache(DEFAULT_FONT_CACHE_LIMIT, DEFAULT_REALIZED_FONT_CACHE_LIMIT);
    PDFICCProfileCache iccProfileCache;
    PDFOptionalContentActivity activity(document, OCUsage::View, nullptr);
    fontCache.setDocument(PDFModifiedDocument(const_cast<PDFDocument*>(document), &activity));
    iccProfileCache.setDocument(PDFModifiedDocument(const_cast<PDFDocument*>(document), &activity));
    PDFRenderer renderer(document, &fontCache, &iccProfileCache, &cms, &activity, PDFRenderer::getDefaultFeatures(), PDFMeshQualitySettings());

    // Page space (y up) to the pixels of the area (y down)
    // ponytail: the page is taken unrotated; an area on a page turned by /Rotate is
    // rendered turned with it, the font matching then sees sideways text.
    const QTransform matrix(scale, 0, 0, -scale, -area.left() * scale, area.bottom() * scale);
    QPainter painter(&image);
    renderer.render(&painter, matrix, size_t(pageIndex));
    painter.end();
    return image;
}

QString PDFScanTextEditor::getText(const PDFDocument* document, PDFInteger pageIndex, const QRectF& area)
{
    const PDFPage* page = document->getCatalog()->getPage(pageIndex);
    if (!page)
    {
        return QString();
    }

    PDFCMSGeneric cms;
    PDFFontCache fontCache(DEFAULT_FONT_CACHE_LIMIT, DEFAULT_REALIZED_FONT_CACHE_LIMIT);
    PDFICCProfileCache iccProfileCache;
    PDFMeshQualitySettings meshQuality;
    PDFOptionalContentActivity activity(document, OCUsage::View, nullptr);
    fontCache.setDocument(PDFModifiedDocument(const_cast<PDFDocument*>(document), &activity));
    iccProfileCache.setDocument(PDFModifiedDocument(const_cast<PDFDocument*>(document), &activity));

    PDFTextLayoutGenerator generator(PDFRenderer::IgnoreOptionalContent, page, document, &fontCache, &iccProfileCache, &cms, &activity, QTransform(), meshQuality);
    generator.processContents();
    const PDFTextLayout layout = generator.createTextLayout();

    QStringList lines;
    for (const PDFTextFlow& flow : PDFTextFlow::createTextFlows(layout, PDFTextFlow::SeparateBlocks, pageIndex))
    {
        const QString text = flow.getText();
        const std::vector<QRectF> boxes = flow.getBoundingBoxes();
        QString selected;
        for (qsizetype i = 0; i < text.size() && size_t(i) < boxes.size(); ++i)
        {
            // Characters whose centre lies in the area; the spaces between them stay
            if (area.contains(boxes[i].center()) || (text[i].isSpace() && !selected.isEmpty()))
            {
                selected += text[i];
            }
        }
        if (!selected.trimmed().isEmpty())
        {
            lines << selected.simplified();
        }
    }
    return lines.join(QChar(' '));
}

QRectF PDFScanTextEditor::getInkRectangle(const QImage& image, const QRectF& area)
{
    const QImage gray = image.convertToFormat(QImage::Format_Grayscale8);
    const int threshold = PDFSignatureLibrary::getAutomaticThreshold(gray);
    int left = gray.width();
    int right = -1;
    int top = gray.height();
    int bottom = -1;
    for (int y = 0; y < gray.height(); ++y)
    {
        const uchar* line = gray.constScanLine(y);
        for (int x = 0; x < gray.width(); ++x)
        {
            if (line[x] < threshold)
            {
                left = qMin(left, x);
                right = qMax(right, x);
                top = qMin(top, y);
                bottom = qMax(bottom, y);
            }
        }
    }
    if (right < 0)
    {
        return area;
    }

    // Pixels (y down) back to the page (y up)
    const double sx = area.width() / image.width();
    const double sy = area.height() / image.height();
    return QRectF(area.left() + left * sx, area.bottom() - (bottom + 1) * sy, (right - left + 1) * sx, (bottom - top + 1) * sy);
}

bool PDFScanTextEditor::hasOwnOcrLayer(const PDFDocument* document, PDFInteger pageIndex)
{
    return PDFOCRTextLayerWriter::getOwnLayerContentReference(document, pageIndex).isValid();
}

PDFDocumentPointer PDFScanTextEditor::removeInvisibleText(const PDFDocument* document, PDFInteger pageIndex, const QRectF& area)
{
    const PDFPage* page = document->getCatalog()->getPage(pageIndex);
    if (!page)
    {
        return nullptr;
    }

    PDFCMSGeneric cms;
    PDFFontCache fontCache(DEFAULT_FONT_CACHE_LIMIT, DEFAULT_REALIZED_FONT_CACHE_LIMIT);
    PDFICCProfileCache iccProfileCache;
    PDFOptionalContentActivity activity(document, OCUsage::View, nullptr);
    fontCache.setDocument(PDFModifiedDocument(const_cast<PDFDocument*>(document), &activity));
    iccProfileCache.setDocument(PDFModifiedDocument(const_cast<PDFDocument*>(document), &activity));
    PDFPageContentEditorProcessor processor(page, document, &fontCache, &iccProfileCache, &cms, &activity, QTransform(), PDFMeshQualitySettings());
    processor.processContents();
    PDFEditedPageContent content = processor.takeEditedPageContent();

    // Only a scan (images and invisible text) is written again; anything else could
    // change its look by the rewrite
    std::vector<const PDFEditedPageContentElement*> kept;
    bool isRemoved = false;
    for (size_t i = 0; i < content.getElementCount(); ++i)
    {
        const PDFEditedPageContentElement* element = content.getElement(i);
        if (element->getType() == PDFEditedPageContentElement::Type::Image)
        {
            kept.push_back(element);
            continue;
        }
        const PDFEditedPageContentElementText* text = element->asText();
        if (!text)
        {
            return nullptr;
        }

        // The state of a text item is the one set by the updates of the graphic state
        // before it (render mode, font, text matrix). Invisible text draws nothing, so it
        // has no outline: its place is estimated from the text matrix and the font size.
        // ponytail: half an em per character for the width; enough for OCR words and lines
        PDFPageContentProcessorState current = text->getState();
        QRectF box = text->getBoundingBox();
        for (const PDFEditedPageContentElementText::Item& item : text->getItems())
        {
            if (item.isUpdateGraphicState)
            {
                current = item.state;
                continue;
            }
            if (!item.isText)
            {
                continue;
            }
            if (current.getTextRenderingMode() != TextRenderingMode::Invisible)
            {
                return nullptr;     // visible text: a born-digital page
            }
            const QTransform matrix = current.getTextMatrix() * current.getCurrentTransformationMatrix();
            const qreal size = current.getTextFontSize();
            const qreal width = 0.5 * size * qMax<qsizetype>(1, qsizetype(item.textSequence.items.size())) * current.getTextHorizontalScaling() / 100.0;
            box = box.united(matrix.mapRect(QRectF(0, -0.2 * size, width, size)));
        }
        if (box.intersects(area))
        {
            isRemoved = true;
        }
        else
        {
            kept.push_back(element);
        }
    }
    if (!isRemoved)
    {
        return nullptr;
    }

    PDFDocument writable = *document;
    PDFPageContentEditorContentStreamBuilder contentBuilder(&writable);
    contentBuilder.setFontDictionary(content.getFontDictionary());
    contentBuilder.setXObjectDictionary(content.getXObjectDictionary());
    contentBuilder.setGraphicStateDictionary(content.getGraphicStateDictionary());
    contentBuilder.setShadingDictionary(content.getShadingDictionary());
    for (const PDFEditedPageContentElement* element : kept)
    {
        contentBuilder.writeEditedElement(element);
    }
    if (!contentBuilder.getErrors().isEmpty())
    {
        return nullptr;
    }

    PDFDocumentModifier modifier(document);
    PDFDocumentBuilder* builder = modifier.getBuilder();
    PDFDictionaryBuilder fontDictionary = contentBuilder.getFontDictionary();
    PDFDictionaryBuilder xobjectDictionary = contentBuilder.getXObjectDictionary();
    PDFDictionaryBuilder graphicStateDictionary = contentBuilder.getGraphicStateDictionary();
    PDFDictionaryBuilder shadingDictionary = contentBuilder.getShadingDictionary();
    builder->replaceObjectsByReferences(fontDictionary);
    builder->replaceObjectsByReferences(xobjectDictionary);
    builder->replaceObjectsByReferences(graphicStateDictionary);
    builder->replaceObjectsByReferences(shadingDictionary);

    // As the page content editor does: the regenerated categories of the resources are
    // replaced, the others kept
    PDFDictionaryBuilder resources;
    if (const PDFDictionary* current = document->getDictionaryFromObject(page->getResources()))
    {
        resources = PDFDictionaryBuilder(*current);
    }
    auto setResources = [&resources](const char* key, const PDFDictionaryBuilder& dictionary)
    {
        if (!dictionary.isEmpty())
        {
            resources.setEntry(PDFInplaceOrMemoryString(key), PDFObject::createDictionary(dictionary));
        }
    };
    setResources("Font", fontDictionary);
    setResources("XObject", xobjectDictionary);
    setResources("ExtGState", graphicStateDictionary);
    setResources("Shading", shadingDictionary);

    PDFArrayBuilder filters;
    filters.appendItem(PDFObject::createName("FlateDecode"));
    QByteArray compressed = PDFFlateDecodeFilter::compress(contentBuilder.getOutputContent());
    PDFDictionaryBuilder streamDictionary;
    streamDictionary.setEntry(PDFInplaceOrMemoryString("Length"), PDFObject::createInteger(compressed.size()));
    streamDictionary.setEntry(PDFInplaceOrMemoryString("Filter"), PDFObject::createArray(std::move(filters)));
    const PDFObject contentObject = PDFObject::createStream(PDFStream(std::move(streamDictionary), std::move(compressed)));

    PDFObjectFactory factory;
    factory.beginDictionary();
    factory.beginDictionaryItem("Resources");
    factory << resources;
    factory.endDictionaryItem();
    factory.beginDictionaryItem("Contents");
    factory << builder->addObject(contentObject);
    factory.endDictionaryItem();
    factory.endDictionary();
    PDFObject pageObject = builder->getObjectByReference(page->getPageReference());
    pageObject = PDFObjectManipulator::merge(pageObject, factory.takeObject(), PDFObjectManipulator::RemoveNullObjects);
    builder->setObject(page->getPageReference(), std::move(pageObject));

    modifier.markPageContentsChanged();
    if (!modifier.finalize())
    {
        return nullptr;
    }
    return modifier.getDocument();
}

PDFDocumentPointer PDFScanTextEditor::apply(const PDFDocument* document, const Edit& edit, bool* isOcrLayerRemoved, HiddenText* hiddenText)
{
    const PDFPage* page = document->getCatalog()->getPage(edit.pageIndex);
    if (!page || edit.area.isEmpty())
    {
        return nullptr;
    }

    // The old words must not stay in the hidden text: the own OCR layer is removed (OCR
    // makes it again), the invisible text of another program in the area too
    HiddenText hidden = getText(document, edit.pageIndex, edit.area).isEmpty() ? HiddenText::None : HiddenText::Kept;
    PDFDocumentPointer cleaned;
    const bool hasOwnLayer = hasOwnOcrLayer(document, edit.pageIndex);
    if (!hasOwnLayer && hidden == HiddenText::Kept)
    {
        cleaned = removeInvisibleText(document, edit.pageIndex, edit.area);
        if (cleaned)
        {
            hidden = HiddenText::ForeignRemoved;
            document = cleaned.data();
            page = document->getCatalog()->getPage(edit.pageIndex);
        }
    }

    PDFDocumentModifier modifier(document);
    const bool removed = hasOwnLayer && PDFOCRTextLayerWriter::removeLayer(modifier.getBuilder(), document, edit.pageIndex);
    if (removed)
    {
        hidden = HiddenText::OwnLayerRemoved;
    }
    if (isOcrLayerRemoved)
    {
        *isOcrLayerRemoved = removed;
    }
    if (hiddenText)
    {
        *hiddenText = hidden;
    }
    PDFPageContentStreamBuilder builder(modifier.getBuilder(), PDFContentStreamBuilder::CoordinateSystem::PDF, PDFPageContentStreamBuilder::Mode::PlaceAfter);
    QPainter* painter = builder.begin(page->getPageReference());

    // The old text disappears under the paper
    painter->fillRect(edit.area, edit.paper);

    if (!edit.text.isEmpty())
    {
        // The tight box of the new text is centred on the ink of the old text, left
        // aligned with it; the painter is flipped back for the text (y down)
        const QFontMetricsF metrics(edit.font, painter->device());
        const QRectF textBox = metrics.tightBoundingRect(edit.text);
        const QRectF ink = edit.inkRectangle.isValid() ? edit.inkRectangle : edit.area;
        painter->save();
        painter->translate(ink.left(), ink.center().y());
        painter->scale(1.0, -1.0);
        painter->setFont(edit.font);
        painter->setPen(edit.ink);
        painter->drawText(QPointF(-textBox.left(), -textBox.center().y()), edit.text);
        painter->restore();
    }

    builder.end(painter);
    modifier.markPageContentsChanged();
    if (!modifier.finalize())
    {
        return nullptr;
    }
    return modifier.getDocument();
}

}   // namespace pdf
