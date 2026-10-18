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

#include "pdfpageinitials.h"
#include "pdfdocumentbuilder.h"
#include "pdfpage.h"

#include <QPainter>

#include "pdfdbgheap.h"

namespace pdf
{

QTransform PDFPageInitials::getDisplayToPageTransform(const PDFPage* page)
{
    const QRectF box = page->getCropBox().isValid() ? page->getCropBox() : page->getMediaBox();
    const qreal x0 = box.left();
    const qreal y0 = box.top();     // smaller y, PDF space has y going up
    const qreal w = box.width();
    const qreal h = box.height();

    // /Rotate turns the page clockwise for display
    switch (page->getPageRotation())
    {
        case PageRotation::None:
            return QTransform(1, 0, 0, 1, x0, y0);
        case PageRotation::Rotate90:
            return QTransform(0, 1, -1, 0, x0 + w, y0);
        case PageRotation::Rotate180:
            return QTransform(-1, 0, 0, -1, x0 + w, y0 + h);
        case PageRotation::Rotate270:
            return QTransform(0, -1, 1, 0, x0, y0 + h);
    }

    return QTransform(1, 0, 0, 1, x0, y0);
}

QRectF PDFPageInitials::getDisplayRectangle(const PDFPage* page, QSizeF imageSize, const Settings& settings)
{
    const QRectF box = page->getCropBox().isValid() ? page->getCropBox() : page->getMediaBox();
    const bool isQuarterTurn = page->getPageRotation() == PageRotation::Rotate90 || page->getPageRotation() == PageRotation::Rotate270;
    const QSizeF displaySize = isQuarterTurn ? box.size().transposed() : box.size();

    const qreal width = qMin(settings.width, displaySize.width() - 2 * settings.margin);
    const qreal height = imageSize.width() > 0 ? width * imageSize.height() / imageSize.width() : width;

    qreal x = settings.margin;
    qreal y = settings.margin;
    switch (settings.position)
    {
        case Position::BottomRight:
            x = displaySize.width() - settings.margin - width;
            break;
        case Position::BottomCenter:
            x = (displaySize.width() - width) * 0.5;
            break;
        case Position::BottomLeft:
            break;
        case Position::TopRight:
            x = displaySize.width() - settings.margin - width;
            y = displaySize.height() - settings.margin - height;
            break;
        case Position::TopLeft:
            y = displaySize.height() - settings.margin - height;
            break;
    }

    return QRectF(x, y, width, height);
}

QRectF PDFPageInitials::getFittedDisplayRectangle(const PDFPage* page, const QRectF& area, QSizeF imageSize)
{
    const QRectF displayArea = getDisplayToPageTransform(page).inverted().mapRect(area);
    if (imageSize.isEmpty() || displayArea.isEmpty())
    {
        return displayArea;
    }

    const QSizeF size = imageSize.scaled(displayArea.size(), Qt::KeepAspectRatio);
    return QRectF(displayArea.topLeft(), size);   // y up: topLeft() is the bottom left corner
}

PDFDocumentPointer PDFPageInitials::drawImages(const PDFDocument* document, const QImage& image, const std::vector<Placement>& placements)
{
    if (!document || image.isNull() || placements.empty())
    {
        return nullptr;
    }

    PDFDocumentModifier modifier(document);
    const PDFCatalog* catalog = document->getCatalog();
    bool isChanged = false;

    for (const Placement& placement : placements)
    {
        const PDFPage* page = catalog->getPage(placement.pageIndex);
        if (!page)
        {
            continue;
        }

        const QRectF rectangle = getFittedDisplayRectangle(page, placement.area, image.size());
        PDFPageContentStreamBuilder builder(modifier.getBuilder(), PDFContentStreamBuilder::CoordinateSystem::PDF, PDFPageContentStreamBuilder::Mode::PlaceAfter);
        QPainter* painter = builder.begin(page->getPageReference());
        painter->setTransform(getDisplayToPageTransform(page), true);
        painter->translate(rectangle.left(), rectangle.bottom());
        painter->scale(1.0, -1.0);
        painter->drawImage(QRectF(0, 0, rectangle.width(), rectangle.height()), image);
        builder.end(painter);
        isChanged = true;
    }

    if (!isChanged)
    {
        return nullptr;
    }

    modifier.markPageContentsChanged();
    if (!modifier.finalize())
    {
        return nullptr;
    }
    return modifier.getDocument();
}

PDFDocumentPointer PDFPageInitials::apply(const PDFDocument* document, const QImage& image, const Settings& settings)
{
    if (!document || image.isNull())
    {
        return nullptr;
    }

    PDFDocumentModifier modifier(document);
    const PDFCatalog* catalog = document->getCatalog();
    bool isChanged = false;

    for (size_t pageIndex = 0; pageIndex < catalog->getPageCount(); ++pageIndex)
    {
        if (settings.skippedPages.count(PDFInteger(pageIndex)))
        {
            continue;
        }

        const PDFPage* page = catalog->getPage(pageIndex);
        const QRectF rectangle = getDisplayRectangle(page, image.size(), settings);

        PDFPageContentStreamBuilder builder(modifier.getBuilder(), PDFContentStreamBuilder::CoordinateSystem::PDF, PDFPageContentStreamBuilder::Mode::PlaceAfter);
        QPainter* painter = builder.begin(page->getPageReference());
        painter->setTransform(getDisplayToPageTransform(page), true);

        // The image rows go down, the y axis of the page goes up
        painter->translate(rectangle.left(), rectangle.bottom());
        painter->scale(1.0, -1.0);
        painter->drawImage(QRectF(0, 0, rectangle.width(), rectangle.height()), image);
        builder.end(painter);
        isChanged = true;
    }

    if (!isChanged)
    {
        return nullptr;
    }

    modifier.markPageContentsChanged();
    if (!modifier.finalize())
    {
        return nullptr;
    }
    return modifier.getDocument();
}

}   // namespace pdf
