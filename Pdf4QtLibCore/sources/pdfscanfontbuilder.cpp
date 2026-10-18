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

#include "pdfscanfontbuilder.h"
#include "pdfscantexteditor.h"
#include "pdfsignaturelibrary.h"
#include "pdftextlayout.h"
#include "pdftextlayoutgenerator.h"
#include "pdfcms.h"
#include "pdffont.h"
#include "pdfcolorspaces.h"
#include "pdfconstants.h"
#include "pdfoptionalcontent.h"

#include <QMap>
#include <QtEndian>

#include <map>
#include <cmath>
#include <algorithm>

#include "pdfdbgheap.h"

namespace pdf
{

// Units per em of the written font
static constexpr int SCAN_FONT_UNITS_PER_EM = 1000;
// Usual ratio of the x-height to the em of a text font
static constexpr double SCAN_FONT_XHEIGHT_RATIO = 0.48;

class PDFScanFontBuilderHelper
{
public:
    PDFScanFontBuilderHelper() = delete;

    struct Component
    {
        QRect box;
        std::vector<QPoint> pixels;
    };

    /// Ink shapes (8-connected) of a binary image inside the rectangle
    static std::vector<Component> getComponents(const std::vector<uchar>& ink, int width, int height, const QRect& rectangle)
    {
        std::vector<Component> components;
        std::vector<uchar> visited(ink.size(), 0);
        const QRect area = rectangle.intersected(QRect(0, 0, width, height));
        std::vector<QPoint> stack;
        for (int y = area.top(); y <= area.bottom(); ++y)
        {
            for (int x = area.left(); x <= area.right(); ++x)
            {
                const size_t index = size_t(y) * width + x;
                if (!ink[index] || visited[index])
                {
                    continue;
                }

                Component component;
                visited[index] = 1;
                stack.push_back(QPoint(x, y));
                while (!stack.empty())
                {
                    const QPoint point = stack.back();
                    stack.pop_back();
                    component.pixels.push_back(point);
                    for (int dy = -1; dy <= 1; ++dy)
                    {
                        for (int dx = -1; dx <= 1; ++dx)
                        {
                            const int nx = point.x() + dx;
                            const int ny = point.y() + dy;
                            if (area.contains(nx, ny))
                            {
                                const size_t neighbour = size_t(ny) * width + nx;
                                if (ink[neighbour] && !visited[neighbour])
                                {
                                    visited[neighbour] = 1;
                                    stack.push_back(QPoint(nx, ny));
                                }
                            }
                        }
                    }
                }

                int left = width, right = -1, top = height, bottom = -1;
                for (const QPoint& point : component.pixels)
                {
                    left = qMin(left, point.x());
                    right = qMax(right, point.x());
                    top = qMin(top, point.y());
                    bottom = qMax(bottom, point.y());
                }
                component.box = QRect(QPoint(left, top), QPoint(right, bottom));
                components.push_back(std::move(component));
            }
        }
        return components;
    }

    static double median(std::vector<double> values)
    {
        if (values.empty())
        {
            return 0.0;
        }
        std::nth_element(values.begin(), values.begin() + values.size() / 2, values.end());
        return values[values.size() / 2];
    }

    /// Perpendicular distance of a point from a line through two points
    static double distance(const QPointF& point, const QPointF& a, const QPointF& b)
    {
        const QPointF d = b - a;
        const double length = std::hypot(d.x(), d.y());
        if (length < 1e-9)
        {
            return std::hypot(point.x() - a.x(), point.y() - a.y());
        }
        return std::abs(d.y() * point.x() - d.x() * point.y() + b.x() * a.y() - b.y() * a.x()) / length;
    }

    static void simplify(const std::vector<QPointF>& points, size_t first, size_t last, double tolerance, std::vector<bool>& keep)
    {
        double maximal = 0.0;
        size_t index = first;
        for (size_t i = first + 1; i < last; ++i)
        {
            const double d = distance(points[i], points[first], points[last]);
            if (d > maximal)
            {
                maximal = d;
                index = i;
            }
        }
        if (maximal > tolerance)
        {
            keep[index] = true;
            simplify(points, first, index, tolerance, keep);
            simplify(points, index, last, tolerance, keep);
        }
    }

    static void writeUInt16(QByteArray& data, quint16 value) { data.append(char(value >> 8)); data.append(char(value & 0xFF)); }
    static void writeInt16(QByteArray& data, qint16 value) { writeUInt16(data, quint16(value)); }
    static void writeUInt32(QByteArray& data, quint32 value) { writeUInt16(data, quint16(value >> 16)); writeUInt16(data, quint16(value & 0xFFFF)); }

    static quint32 checksum(const QByteArray& data)
    {
        quint32 sum = 0;
        QByteArray padded = data;
        while (padded.size() % 4)
        {
            padded.append('\0');
        }
        for (qsizetype i = 0; i < padded.size(); i += 4)
        {
            sum += qFromBigEndian<quint32>(reinterpret_cast<const uchar*>(padded.constData() + i));
        }
        return sum;
    }
};

std::vector<PDFScanFontBuilder::Glyph> PDFScanFontBuilder::harvest(const PDFDocument* document, const std::vector<PDFInteger>& pageIndices, double dpi, Metrics* metrics)
{
    using Helper = PDFScanFontBuilderHelper;
    static const QString descenders = QStringLiteral("gjpqy,;()[]{}|/Q@$");
    static const QString xHeightLetters = QStringLiteral("acemnorsuvwxz");

    struct Sample
    {
        QImage bitmap;
        int baselineOffset = 0;
    };
    std::map<QChar, std::vector<Sample>> samples;
    std::vector<double> xHeights;
    std::vector<double> letterGaps;
    std::vector<double> wordGaps;

    PDFCMSGeneric cms;
    PDFFontCache fontCache(DEFAULT_FONT_CACHE_LIMIT, DEFAULT_REALIZED_FONT_CACHE_LIMIT);
    PDFICCProfileCache iccProfileCache;
    PDFMeshQualitySettings meshQuality;
    PDFOptionalContentActivity activity(document, OCUsage::View, nullptr);
    fontCache.setDocument(PDFModifiedDocument(const_cast<PDFDocument*>(document), &activity));
    iccProfileCache.setDocument(PDFModifiedDocument(const_cast<PDFDocument*>(document), &activity));

    for (PDFInteger pageIndex : pageIndices)
    {
        const PDFPage* page = document->getCatalog()->getPage(pageIndex);
        if (!page)
        {
            continue;
        }

        // ponytail: the page is taken unrotated (as the scan text editor does)
        const QRectF area = page->getMediaBox();
        const QImage image = PDFScanTextEditor::renderArea(document, pageIndex, area, dpi).convertToFormat(QImage::Format_Grayscale8);
        const int threshold = PDFSignatureLibrary::getAutomaticThreshold(image);
        const int width = image.width();
        const int height = image.height();
        std::vector<uchar> ink(size_t(width) * height, 0);
        for (int y = 0; y < height; ++y)
        {
            const uchar* line = image.constScanLine(y);
            for (int x = 0; x < width; ++x)
            {
                ink[size_t(y) * width + x] = line[x] < threshold ? 1 : 0;
            }
        }
        const double scale = dpi / 72.0;
        auto toPixels = [&](const QRectF& box)
        {
            return QRectF((box.left() - area.left()) * scale, (area.bottom() - box.bottom()) * scale, box.width() * scale, box.height() * scale);
        };

        // Words of the text (the OCR layer of a scan) with their boxes
        PDFTextLayoutGenerator generator(PDFRenderer::IgnoreOptionalContent, page, document, &fontCache, &iccProfileCache, &cms, &activity, QTransform(), meshQuality);
        generator.processContents();
        const PDFTextLayout layout = generator.createTextLayout();
        for (const PDFTextFlow& flow : PDFTextFlow::createTextFlows(layout, PDFTextFlow::SeparateBlocks, pageIndex))
        {
            const QString text = flow.getText();
            const std::vector<QRectF> boxes = flow.getBoundingBoxes();
            QString word;
            QRectF wordBox;
            QRectF previousWordBox;
            auto processWord = [&]()
            {
                if (word.isEmpty())
                {
                    return;
                }
                if (previousWordBox.isValid() && qAbs(previousWordBox.center().y() - wordBox.center().y()) < wordBox.height() * 0.5)
                {
                    wordGaps.push_back((wordBox.left() - previousWordBox.right()) * scale);
                }
                previousWordBox = wordBox;

                // The OCR box may be tight, a margin keeps the whole letters
                QRectF pixelBox = toPixels(wordBox);
                pixelBox.adjust(-2, -pixelBox.height() * 0.2, 2, pixelBox.height() * 0.2);
                std::vector<Helper::Component> components = Helper::getComponents(ink, width, height, pixelBox.toAlignedRect());
                components.erase(std::remove_if(components.begin(), components.end(), [](const Helper::Component& c) { return c.pixels.size() < 3; }), components.end());
                std::sort(components.begin(), components.end(), [](const Helper::Component& a, const Helper::Component& b) { return a.box.left() < b.box.left(); });

                // Dots and accents stand over their letters
                for (size_t i = 0; i + 1 < components.size();)
                {
                    const int overlap = qMin(components[i].box.right(), components[i + 1].box.right()) - qMax(components[i].box.left(), components[i + 1].box.left());
                    if (overlap > 0.5 * qMin(components[i].box.width(), components[i + 1].box.width()))
                    {
                        components[i].pixels.insert(components[i].pixels.end(), components[i + 1].pixels.begin(), components[i + 1].pixels.end());
                        components[i].box = components[i].box.united(components[i + 1].box);
                        components.erase(components.begin() + i + 1);
                    }
                    else
                    {
                        ++i;
                    }
                }

                if (components.size() != size_t(word.size()))
                {
                    word.clear();
                    return;   // touching or broken letters, the word is not used
                }

                std::vector<double> bottoms;
                for (size_t i = 0; i < components.size(); ++i)
                {
                    if (!descenders.contains(word[i]) && word[i].isLetterOrNumber())
                    {
                        bottoms.push_back(components[i].box.bottom());
                    }
                }
                if (bottoms.empty())
                {
                    word.clear();
                    return;
                }
                const int baseline = qRound(Helper::median(bottoms));

                for (size_t i = 0; i < components.size(); ++i)
                {
                    const Helper::Component& component = components[i];
                    QImage bitmap(component.box.size(), QImage::Format_Grayscale8);
                    bitmap.fill(255);
                    for (const QPoint& point : component.pixels)
                    {
                        bitmap.setPixel(point - component.box.topLeft(), 0);
                    }
                    samples[word[i]].push_back({ bitmap, component.box.bottom() - baseline });
                    if (xHeightLetters.contains(word[i]))
                    {
                        xHeights.push_back(component.box.height());
                    }
                    if (i > 0)
                    {
                        letterGaps.push_back(component.box.left() - components[i - 1].box.right() - 1);
                    }
                }
                word.clear();
            };

            for (qsizetype i = 0; i < text.size() && size_t(i) < boxes.size(); ++i)
            {
                if (text[i].isSpace())
                {
                    processWord();
                    wordBox = QRectF();
                    continue;
                }
                if (word.isEmpty())
                {
                    wordBox = boxes[i];
                }
                word += text[i];
                wordBox = wordBox.united(boxes[i]);
            }
            processWord();
        }
    }

    // The samples of a letter are scaled to their median size and averaged
    std::vector<Glyph> glyphs;
    for (const auto& [character, letterSamples] : samples)
    {
        std::vector<double> widths;
        std::vector<double> heights;
        std::vector<double> offsets;
        for (const Sample& sample : letterSamples)
        {
            widths.push_back(sample.bitmap.width());
            heights.push_back(sample.bitmap.height());
            offsets.push_back(sample.baselineOffset);
        }
        const QSize size(qMax(1, qRound(Helper::median(widths))), qMax(1, qRound(Helper::median(heights))));
        std::vector<double> sum(size_t(size.width()) * size.height(), 0.0);
        for (const Sample& sample : letterSamples)
        {
            const QImage scaled = sample.bitmap.scaled(size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            for (int y = 0; y < size.height(); ++y)
            {
                const uchar* line = scaled.constScanLine(y);
                for (int x = 0; x < size.width(); ++x)
                {
                    sum[size_t(y) * size.width() + x] += (255 - line[x]) / 255.0;
                }
            }
        }

        Glyph glyph;
        glyph.character = character;
        glyph.samples = int(letterSamples.size());
        glyph.baselineOffset = qRound(Helper::median(offsets));
        glyph.bitmap = QImage(size, QImage::Format_Grayscale8);
        for (int y = 0; y < size.height(); ++y)
        {
            uchar* line = glyph.bitmap.scanLine(y);
            for (int x = 0; x < size.width(); ++x)
            {
                line[x] = sum[size_t(y) * size.width() + x] >= 0.5 * letterSamples.size() ? 0 : 255;
            }
        }
        glyphs.push_back(std::move(glyph));
    }

    if (metrics)
    {
        double allHeights = 0.0;
        for (const Glyph& glyph : glyphs)
        {
            allHeights += glyph.bitmap.height();
        }
        metrics->xHeight = !xHeights.empty() ? Helper::median(xHeights) : (glyphs.empty() ? 0.0 : 0.6 * allHeights / glyphs.size());
        metrics->sideBearing = qMax(0.5, 0.5 * Helper::median(letterGaps));
        metrics->wordSpace = !wordGaps.empty() ? Helper::median(wordGaps) : metrics->xHeight * 0.6;
    }
    return glyphs;
}

std::vector<QPolygonF> PDFScanFontBuilder::traceContours(const QImage& bitmap, double tolerance)
{
    const QImage gray = bitmap.convertToFormat(QImage::Format_Grayscale8);
    const int width = gray.width();
    const int height = gray.height();
    auto isInk = [&](int x, int y) { return x >= 0 && y >= 0 && x < width && y < height && gray.constScanLine(y)[x] < 128; };

    // Directed edges of the pixel boundaries, the ink on the right (clockwise on the screen)
    std::multimap<std::pair<int, int>, std::pair<int, int>> edges;
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            if (!isInk(x, y))
            {
                continue;
            }
            if (!isInk(x, y - 1)) { edges.emplace(std::make_pair(x, y), std::make_pair(x + 1, y)); }
            if (!isInk(x + 1, y)) { edges.emplace(std::make_pair(x + 1, y), std::make_pair(x + 1, y + 1)); }
            if (!isInk(x, y + 1)) { edges.emplace(std::make_pair(x + 1, y + 1), std::make_pair(x, y + 1)); }
            if (!isInk(x - 1, y)) { edges.emplace(std::make_pair(x, y + 1), std::make_pair(x, y)); }
        }
    }

    std::vector<QPolygonF> contours;
    while (!edges.empty())
    {
        auto it = edges.begin();
        const std::pair<int, int> start = it->first;
        std::vector<QPointF> points;
        std::pair<int, int> current = start;
        std::pair<int, int> direction(0, 0);
        while (true)
        {
            auto range = edges.equal_range(current);
            if (range.first == range.second)
            {
                break;
            }

            // At a corner touched by two pixels, the right turn keeps the outline tight
            auto chosen = range.first;
            if (std::distance(range.first, range.second) > 1)
            {
                for (auto candidate = range.first; candidate != range.second; ++candidate)
                {
                    const std::pair<int, int> d(candidate->second.first - current.first, candidate->second.second - current.second);
                    if (d.first == -direction.second && d.second == direction.first)
                    {
                        chosen = candidate;
                    }
                }
            }

            const std::pair<int, int> next = chosen->second;
            direction = std::make_pair(next.first - current.first, next.second - current.second);
            points.push_back(QPointF(current.first, current.second));
            edges.erase(chosen);
            current = next;
            if (current == start)
            {
                break;
            }
        }

        if (points.size() < 3)
        {
            continue;
        }

        // Douglas-Peucker on the closed outline, split at its farthest point
        size_t farthest = 0;
        double farthestDistance = 0.0;
        for (size_t i = 1; i < points.size(); ++i)
        {
            const double d = std::hypot(points[i].x() - points[0].x(), points[i].y() - points[0].y());
            if (d > farthestDistance)
            {
                farthestDistance = d;
                farthest = i;
            }
        }
        std::vector<QPointF> closed = points;
        closed.push_back(points.front());
        std::vector<bool> keep(closed.size(), false);
        keep[0] = true;
        keep[farthest] = true;
        keep.back() = true;
        PDFScanFontBuilderHelper::simplify(closed, 0, farthest, tolerance, keep);
        PDFScanFontBuilderHelper::simplify(closed, farthest, closed.size() - 1, tolerance, keep);

        QPolygonF polygon;
        for (size_t i = 0; i + 1 < closed.size(); ++i)
        {
            if (keep[i])
            {
                polygon << closed[i];
            }
        }
        if (polygon.size() >= 3)
        {
            contours.push_back(polygon);
        }
    }
    return contours;
}

QByteArray PDFScanFontBuilder::createTrueTypeFont(const std::vector<Glyph>& glyphs, const Metrics& metrics, const QString& family)
{
    using Helper = PDFScanFontBuilderHelper;

    const double pixelsPerEm = qMax(1.0, metrics.xHeight / SCAN_FONT_XHEIGHT_RATIO);
    const double scale = SCAN_FONT_UNITS_PER_EM / pixelsPerEm;
    const int sideBearing = qRound(metrics.sideBearing * scale);

    // Glyph 0 is .notdef, glyph 1 the space, then the letters ordered by the code
    struct OutGlyph
    {
        QChar character;
        QByteArray data;
        int advance = 0;
        int leftSideBearing = 0;
        QRect bounds;
        int points = 0;
        int contours = 0;
    };
    std::vector<OutGlyph> out(2);
    out[0].advance = qRound(SCAN_FONT_UNITS_PER_EM * 0.5);
    out[1].character = QChar(' ');
    out[1].advance = qRound(qMax(metrics.wordSpace, metrics.xHeight * 0.4) * scale);

    std::vector<Glyph> sorted = glyphs;
    std::sort(sorted.begin(), sorted.end(), [](const Glyph& a, const Glyph& b) { return a.character < b.character; });
    QRect fontBounds;
    for (const Glyph& glyph : sorted)
    {
        if (glyph.character == QChar(' ') || glyph.character.unicode() > 0xFFFE)
        {
            continue;
        }

        const int baselineRow = glyph.bitmap.height() - glyph.baselineOffset;
        OutGlyph outGlyph;
        outGlyph.character = glyph.character;
        outGlyph.advance = qRound(glyph.bitmap.width() * scale) + 2 * sideBearing;

        std::vector<QPolygon> polygons;
        for (const QPolygonF& contour : traceContours(glyph.bitmap))
        {
            QPolygon polygon;
            for (const QPointF& point : contour)
            {
                polygon << QPoint(qRound(point.x() * scale) + sideBearing, qRound((baselineRow - point.y()) * scale));
            }
            // The y axis is flipped, clockwise on the screen becomes counter-clockwise;
            // TrueType wants the outer outlines clockwise
            std::reverse(polygon.begin(), polygon.end());
            polygons.push_back(polygon);
            outGlyph.bounds = outGlyph.bounds.isNull() ? polygon.boundingRect() : outGlyph.bounds.united(polygon.boundingRect());
        }

        if (!polygons.empty())
        {
            QByteArray& data = outGlyph.data;
            Helper::writeInt16(data, qint16(polygons.size()));
            Helper::writeInt16(data, qint16(outGlyph.bounds.left()));
            Helper::writeInt16(data, qint16(outGlyph.bounds.top()));
            Helper::writeInt16(data, qint16(outGlyph.bounds.right()));
            Helper::writeInt16(data, qint16(outGlyph.bounds.bottom()));
            int endPoint = -1;
            for (const QPolygon& polygon : polygons)
            {
                endPoint += int(polygon.size());
                Helper::writeUInt16(data, quint16(endPoint));
            }
            Helper::writeUInt16(data, 0);  // no instructions
            for (int i = 0; i <= endPoint; ++i)
            {
                data.append(char(0x01));    // on the curve, 16 bit deltas
            }
            QPoint last(0, 0);
            QByteArray ys;
            for (const QPolygon& polygon : polygons)
            {
                for (const QPoint& point : polygon)
                {
                    Helper::writeInt16(data, qint16(point.x() - last.x()));
                    Helper::writeInt16(ys, qint16(point.y() - last.y()));
                    last = point;
                }
            }
            data.append(ys);
            while (data.size() % 4)
            {
                data.append('\0');
            }
            outGlyph.points = endPoint + 1;
            outGlyph.contours = int(polygons.size());
            outGlyph.leftSideBearing = outGlyph.bounds.left();
            fontBounds = fontBounds.isNull() ? outGlyph.bounds : fontBounds.united(outGlyph.bounds);
        }
        out.push_back(outGlyph);
    }

    if (fontBounds.isNull())
    {
        fontBounds = QRect(0, 0, SCAN_FONT_UNITS_PER_EM / 2, SCAN_FONT_UNITS_PER_EM / 2);
    }
    const int ascender = qMax(fontBounds.bottom(), qRound(SCAN_FONT_UNITS_PER_EM * 0.75));
    const int descender = qMin(fontBounds.top(), -qRound(SCAN_FONT_UNITS_PER_EM * 0.25));
    const quint16 glyphCount = quint16(out.size());
    int maxPoints = 0;
    int maxContours = 0;
    int maxAdvance = 0;
    for (const OutGlyph& glyph : out)
    {
        maxPoints = qMax(maxPoints, glyph.points);
        maxContours = qMax(maxContours, glyph.contours);
        maxAdvance = qMax(maxAdvance, glyph.advance);
    }

    // glyf, loca, hmtx
    QByteArray glyf;
    QByteArray loca;
    QByteArray hmtx;
    for (const OutGlyph& glyph : out)
    {
        Helper::writeUInt32(loca, quint32(glyf.size()));
        glyf.append(glyph.data);
        Helper::writeUInt16(hmtx, quint16(glyph.advance));
        Helper::writeInt16(hmtx, qint16(glyph.leftSideBearing));
    }
    Helper::writeUInt32(loca, quint32(glyf.size()));

    // cmap: format 4, a segment per character and the final one
    QByteArray cmapSubtable;
    std::vector<std::pair<quint16, quint16>> mapping; // code, glyph
    for (quint16 i = 1; i < glyphCount; ++i)
    {
        mapping.emplace_back(out[i].character.unicode(), i);
    }
    std::sort(mapping.begin(), mapping.end());
    const quint16 segments = quint16(mapping.size() + 1);
    {
        quint16 searchRange = 1;
        quint16 entrySelector = 0;
        while (searchRange * 2 <= segments)
        {
            searchRange *= 2;
            ++entrySelector;
        }
        searchRange *= 2;
        QByteArray body;
        Helper::writeUInt16(body, quint16(segments * 2));
        Helper::writeUInt16(body, searchRange);
        Helper::writeUInt16(body, entrySelector);
        Helper::writeUInt16(body, quint16(segments * 2 - searchRange));
        for (const auto& [code, glyph] : mapping) { Helper::writeUInt16(body, code); }
        Helper::writeUInt16(body, 0xFFFF);
        Helper::writeUInt16(body, 0);   // reserved pad
        for (const auto& [code, glyph] : mapping) { Helper::writeUInt16(body, code); }
        Helper::writeUInt16(body, 0xFFFF);
        for (const auto& [code, glyph] : mapping) { Helper::writeInt16(body, qint16(quint16(glyph - code))); }
        Helper::writeInt16(body, 1);
        for (size_t i = 0; i < segments; ++i) { Helper::writeUInt16(body, 0); }
        Helper::writeUInt16(cmapSubtable, 4);
        Helper::writeUInt16(cmapSubtable, quint16(body.size() + 6));
        Helper::writeUInt16(cmapSubtable, 0);
        cmapSubtable.append(body);
    }
    QByteArray cmap;
    Helper::writeUInt16(cmap, 0);
    Helper::writeUInt16(cmap, 1);
    Helper::writeUInt16(cmap, 3);
    Helper::writeUInt16(cmap, 1);
    Helper::writeUInt32(cmap, 12);
    cmap.append(cmapSubtable);

    QByteArray head;
    Helper::writeUInt32(head, 0x00010000);
    Helper::writeUInt32(head, 0x00010000);
    Helper::writeUInt32(head, 0);              // checkSumAdjustment, set at the end
    Helper::writeUInt32(head, 0x5F0F3CF5);
    Helper::writeUInt16(head, 0x000B);
    Helper::writeUInt16(head, SCAN_FONT_UNITS_PER_EM);
    Helper::writeUInt32(head, 0); Helper::writeUInt32(head, 0); // created
    Helper::writeUInt32(head, 0); Helper::writeUInt32(head, 0); // modified
    Helper::writeInt16(head, qint16(fontBounds.left()));
    Helper::writeInt16(head, qint16(fontBounds.top()));
    Helper::writeInt16(head, qint16(fontBounds.right()));
    Helper::writeInt16(head, qint16(fontBounds.bottom()));
    Helper::writeUInt16(head, 0);              // macStyle
    Helper::writeUInt16(head, 8);              // lowestRecPPEM
    Helper::writeInt16(head, 2);
    Helper::writeInt16(head, 1);               // long loca
    Helper::writeInt16(head, 0);

    QByteArray hhea;
    Helper::writeUInt32(hhea, 0x00010000);
    Helper::writeInt16(hhea, qint16(ascender));
    Helper::writeInt16(hhea, qint16(descender));
    Helper::writeInt16(hhea, 0);
    Helper::writeUInt16(hhea, quint16(maxAdvance));
    Helper::writeInt16(hhea, qint16(fontBounds.left()));
    Helper::writeInt16(hhea, 0);
    Helper::writeInt16(hhea, qint16(fontBounds.right()));
    Helper::writeInt16(hhea, 1);
    Helper::writeInt16(hhea, 0);
    Helper::writeInt16(hhea, 0);
    for (int i = 0; i < 4; ++i) { Helper::writeInt16(hhea, 0); }
    Helper::writeInt16(hhea, 0);
    Helper::writeUInt16(hhea, glyphCount);

    QByteArray maxp;
    Helper::writeUInt32(maxp, 0x00010000);
    Helper::writeUInt16(maxp, glyphCount);
    Helper::writeUInt16(maxp, quint16(maxPoints));
    Helper::writeUInt16(maxp, quint16(maxContours));
    Helper::writeUInt16(maxp, 0);
    Helper::writeUInt16(maxp, 0);
    Helper::writeUInt16(maxp, 2);
    for (int i = 0; i < 8; ++i) { Helper::writeUInt16(maxp, 0); }

    const int xHeightUnits = qRound(metrics.xHeight * scale);
    QByteArray os2;
    Helper::writeUInt16(os2, 4);
    Helper::writeInt16(os2, qint16(maxAdvance / 2));
    Helper::writeUInt16(os2, 400);
    Helper::writeUInt16(os2, 5);
    Helper::writeUInt16(os2, 0);               // fsType: installable, embedding allowed
    for (int i = 0; i < 10; ++i) { Helper::writeInt16(os2, qint16(i % 2 ? 300 : 650)); } // sub/superscript sizes and offsets
    Helper::writeInt16(os2, 50);               // strikeout size
    Helper::writeInt16(os2, qint16(xHeightUnits / 2));
    Helper::writeInt16(os2, 0);                // family class
    os2.append(QByteArray(10, '\0'));          // panose
    Helper::writeUInt32(os2, 0x00000003);      // Basic Latin, Latin-1 Supplement
    Helper::writeUInt32(os2, 0);
    Helper::writeUInt32(os2, 0);
    Helper::writeUInt32(os2, 0);
    os2.append("APDF");
    Helper::writeUInt16(os2, 0x0040);          // regular
    Helper::writeUInt16(os2, mapping.empty() ? 0x20 : mapping.front().first);
    Helper::writeUInt16(os2, mapping.empty() ? 0x20 : mapping.back().first);
    Helper::writeInt16(os2, qint16(ascender));
    Helper::writeInt16(os2, qint16(descender));
    Helper::writeInt16(os2, qint16(SCAN_FONT_UNITS_PER_EM / 5));
    Helper::writeUInt16(os2, quint16(ascender));
    Helper::writeUInt16(os2, quint16(-descender));
    Helper::writeUInt32(os2, 0x00000001);      // Latin 1
    Helper::writeUInt32(os2, 0);
    Helper::writeInt16(os2, qint16(xHeightUnits));
    Helper::writeInt16(os2, qint16(ascender));
    Helper::writeUInt16(os2, 0);
    Helper::writeUInt16(os2, 0x20);
    Helper::writeUInt16(os2, 1);

    QByteArray post;
    Helper::writeUInt32(post, 0x00030000);
    Helper::writeUInt32(post, 0);
    Helper::writeInt16(post, -100);
    Helper::writeInt16(post, 50);
    for (int i = 0; i < 5; ++i) { Helper::writeUInt32(post, 0); }

    // name: Windows platform, US English, UTF-16BE
    const QString postScriptName = QString(family).remove(QChar(' '));
    const std::vector<std::pair<quint16, QString>> names = {
        { 1, family }, { 2, QStringLiteral("Regular") }, { 3, family + QStringLiteral(" Regular") }, { 4, family },
        { 5, QStringLiteral("Version 1.0") }, { 6, postScriptName }
    };
    QByteArray name;
    QByteArray strings;
    Helper::writeUInt16(name, 0);
    Helper::writeUInt16(name, quint16(names.size()));
    Helper::writeUInt16(name, quint16(6 + 12 * names.size()));
    for (const auto& [id, text] : names)
    {
        QByteArray utf16;
        for (QChar c : text)
        {
            Helper::writeUInt16(utf16, c.unicode());
        }
        Helper::writeUInt16(name, 3);
        Helper::writeUInt16(name, 1);
        Helper::writeUInt16(name, 0x0409);
        Helper::writeUInt16(name, id);
        Helper::writeUInt16(name, quint16(utf16.size()));
        Helper::writeUInt16(name, quint16(strings.size()));
        strings.append(utf16);
    }
    name.append(strings);

    // Tables in the order of their tags, aligned to four bytes
    const std::vector<std::pair<QByteArray, QByteArray*>> tables = {
        { "OS/2", &os2 }, { "cmap", &cmap }, { "glyf", &glyf }, { "head", &head }, { "hhea", &hhea },
        { "hmtx", &hmtx }, { "loca", &loca }, { "maxp", &maxp }, { "name", &name }, { "post", &post }
    };
    QByteArray font;
    const quint16 tableCount = quint16(tables.size());
    quint16 searchRange = 1;
    quint16 entrySelector = 0;
    while (searchRange * 2 <= tableCount)
    {
        searchRange *= 2;
        ++entrySelector;
    }
    searchRange *= 16;
    Helper::writeUInt32(font, 0x00010000);
    Helper::writeUInt16(font, tableCount);
    Helper::writeUInt16(font, searchRange);
    Helper::writeUInt16(font, entrySelector);
    Helper::writeUInt16(font, quint16(tableCount * 16 - searchRange));

    quint32 offset = quint32(12 + 16 * tables.size());
    QByteArray body;
    qsizetype headOffset = 0;
    for (const auto& [tag, table] : tables)
    {
        font.append(tag);
        Helper::writeUInt32(font, Helper::checksum(*table));
        Helper::writeUInt32(font, offset);
        Helper::writeUInt32(font, quint32(table->size()));
        if (tag == "head")
        {
            headOffset = offset;
        }
        QByteArray padded = *table;
        while (padded.size() % 4)
        {
            padded.append('\0');
        }
        body.append(padded);
        offset += quint32(padded.size());
    }
    font.append(body);

    const quint32 adjustment = 0xB1B0AFBAu - Helper::checksum(font);
    qToBigEndian<quint32>(adjustment, reinterpret_cast<uchar*>(font.data() + headOffset + 8));
    return font;
}

QByteArray PDFScanFontBuilder::build(const PDFDocument* document, const std::vector<PDFInteger>& pageIndices, const QString& family, QString* characters)
{
    Metrics metrics;
    const std::vector<Glyph> glyphs = harvest(document, pageIndices, 300.0, &metrics);
    if (characters)
    {
        characters->clear();
        for (const Glyph& glyph : glyphs)
        {
            *characters += glyph.character;
        }
    }
    if (glyphs.size() < 5 || metrics.xHeight <= 0.0)
    {
        return QByteArray();
    }
    return createTrueTypeFont(glyphs, metrics, family);
}

}   // namespace pdf
