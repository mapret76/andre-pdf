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

#include "pdffontidentifier.h"
#include "pdfsignaturelibrary.h"

#include <QFont>
#include <QPainter>
#include <QFontMetricsF>
#include <QFontDatabase>
#include <QRegularExpression>

#include <algorithm>
#include <cmath>

#include "pdfdbgheap.h"

namespace pdf
{

// Size of the normalized ink masks compared by the matching
static constexpr int MATCH_HEIGHT = 40;

class PDFFontIdentifierHelper
{
public:
    PDFFontIdentifierHelper() = delete;

    /// Ink mask (true = ink) of a grayscale image cropped to the ink and scaled to
    /// the given height; the aspect ratio of the ink is returned
    static std::vector<bool> getMask(const QImage& image, int threshold, int& width, double& aspectRatio)
    {
        const QImage gray = image.convertToFormat(QImage::Format_Grayscale8);
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
            width = 0;
            aspectRatio = 0.0;
            return { };
        }

        const QImage ink = gray.copy(QRect(QPoint(left, top), QPoint(right, bottom)));
        aspectRatio = double(ink.width()) / double(ink.height());
        width = qMax(1, int(std::lround(MATCH_HEIGHT * aspectRatio)));
        const QImage scaled = ink.scaled(width, MATCH_HEIGHT, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

        std::vector<bool> mask(size_t(width) * MATCH_HEIGHT, false);
        for (int y = 0; y < MATCH_HEIGHT; ++y)
        {
            const uchar* line = scaled.constScanLine(y);
            for (int x = 0; x < width; ++x)
            {
                mask[size_t(y) * width + x] = line[x] < threshold;
            }
        }
        return mask;
    }

    /// Mask resampled to another width (nearest neighbour)
    static std::vector<bool> resample(const std::vector<bool>& mask, int width, int newWidth)
    {
        std::vector<bool> result(size_t(newWidth) * MATCH_HEIGHT, false);
        for (int y = 0; y < MATCH_HEIGHT; ++y)
        {
            for (int x = 0; x < newWidth; ++x)
            {
                result[size_t(y) * newWidth + x] = mask[size_t(y) * width + qMin(width - 1, x * width / newWidth)];
            }
        }
        return result;
    }

    /// Likeness of two masks of the same size: the ink of each found within one pixel
    /// of the ink of the other (tolerant to the blur and the noise of a scan)
    static double compare(const std::vector<bool>& a, const std::vector<bool>& b, int width)
    {
        auto isNear = [width](const std::vector<bool>& mask, int x, int y)
        {
            for (int dy = -1; dy <= 1; ++dy)
            {
                for (int dx = -1; dx <= 1; ++dx)
                {
                    const int nx = x + dx;
                    const int ny = y + dy;
                    if (nx >= 0 && ny >= 0 && nx < width && ny < MATCH_HEIGHT && mask[size_t(ny) * width + nx])
                    {
                        return true;
                    }
                }
            }
            return false;
        };

        int inkA = 0;
        int inkB = 0;
        int foundA = 0;
        int foundB = 0;
        for (int y = 0; y < MATCH_HEIGHT; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                const size_t index = size_t(y) * width + x;
                if (a[index])
                {
                    ++inkA;
                    foundA += isNear(b, x, y) ? 1 : 0;
                }
                if (b[index])
                {
                    ++inkB;
                    foundB += isNear(a, x, y) ? 1 : 0;
                }
            }
        }

        if (inkA == 0 || inkB == 0)
        {
            return 0.0;
        }

        // Harmonic mean of the two directions, and the amount of ink must agree
        // (a bold font covers the thin strokes of a regular one)
        const double precision = double(foundA) / inkA;
        const double recall = double(foundB) / inkB;
        const double f = (precision + recall) > 0 ? 2.0 * precision * recall / (precision + recall) : 0.0;
        const double inkRatio = double(qMin(inkA, inkB)) / double(qMax(inkA, inkB));
        return f * std::sqrt(inkRatio);
    }
};

PDFFontIdentifier::FontName PDFFontIdentifier::parsePdfFontName(const QByteArray& baseFont)
{
    QString name = QString::fromLatin1(baseFont);

    // Subset prefix of six capital letters
    static const QRegularExpression subset(QStringLiteral("^[A-Z]{6}\\+"));
    name.remove(subset);

    FontName result;
    QString style;
    const qsizetype separator = name.indexOf(QRegularExpression(QStringLiteral("[-,]")));
    if (separator >= 0)
    {
        style = name.mid(separator + 1);
        name = name.left(separator);
    }

    // Style words may be glued to the name ("ArialBold", "TimesNewRomanPSMT")
    static const QRegularExpression styleWords(QStringLiteral("(Bold|Black|Heavy|Semibold|SemiBold|Demi|Italic|Oblique|Inclined)"));
    for (QString* part : { &name, &style })
    {
        QRegularExpressionMatchIterator it = styleWords.globalMatch(*part);
        while (it.hasNext())
        {
            const QString word = it.next().captured(1);
            result.bold = result.bold || word.contains(QStringLiteral("Bold"), Qt::CaseInsensitive) || word == QStringLiteral("Black") || word == QStringLiteral("Heavy") || word == QStringLiteral("Demi");
            result.italic = result.italic || word == QStringLiteral("Italic") || word == QStringLiteral("Oblique") || word == QStringLiteral("Inclined");
        }
    }
    name.remove(styleWords);
    name.remove(QRegularExpression(QStringLiteral("(PSMT|PS|MT|Regular|Roman|Std|Pro|LT)$")));

    // Known PostScript names of the standard and the common fonts
    static const std::pair<const char*, const char*> aliases[] = {
        { "Helvetica", "Helvetica" }, { "Times", "Times New Roman" }, { "TimesNewRoman", "Times New Roman" },
        { "Courier", "Courier New" }, { "CourierNew", "Courier New" }, { "Arial", "Arial" }, { "ArialNarrow", "Arial Narrow" },
        { "Calibri", "Calibri" }, { "Cambria", "Cambria" }, { "CenturyGothic", "Century Gothic" }, { "Tahoma", "Tahoma" },
        { "Verdana", "Verdana" }, { "Garamond", "Garamond" }, { "BookAntiqua", "Book Antiqua" }, { "SegoeUI", "Segoe UI" },
        { "TrebuchetMS", "Trebuchet MS" }, { "Georgia", "Georgia" }, { "Symbol", "Symbol" }, { "ZapfDingbats", "Zapf Dingbats" }
    };
    for (const auto& [postScriptName, family] : aliases)
    {
        if (name.compare(QLatin1String(postScriptName), Qt::CaseInsensitive) == 0)
        {
            result.family = QLatin1String(family);
            return result;
        }
    }

    // CamelCase to words ("OpenSans" -> "Open Sans")
    static const QRegularExpression camelCase(QStringLiteral("(?<=[a-z])(?=[A-Z])|(?<=[A-Z])(?=[A-Z][a-z])"));
    result.family = name.split(camelCase, Qt::SkipEmptyParts).join(QChar(' ')).simplified();
    return result;
}

bool PDFFontIdentifier::isInstalled(const QString& family)
{
    // The font database lists a family once per foundry, "Nimbus Sans [urw]"
    static const QRegularExpression foundry(QStringLiteral("\\s*\\[[^\\]]*\\]$"));
    const QStringList families = QFontDatabase::families();
    return std::any_of(families.cbegin(), families.cend(), [&family](const QString& installed) { return QString(installed).remove(foundry).compare(family, Qt::CaseInsensitive) == 0; });
}

QStringList PDFFontIdentifier::getMetricCompatibleSubstitutes(const QString& family)
{
    static const std::pair<const char*, const char*> substitutes[] = {
        { "Arial", "Liberation Sans;Arimo" }, { "Helvetica", "Liberation Sans;Arimo;TeX Gyre Heros" },
        { "Times New Roman", "Liberation Serif;Tinos" }, { "Times", "Liberation Serif;Tinos;TeX Gyre Termes" },
        { "Courier New", "Liberation Mono;Cousine" }, { "Courier", "Liberation Mono;Cousine;TeX Gyre Cursor" },
        { "Calibri", "Carlito" }, { "Cambria", "Caladea" }, { "Arial Narrow", "Liberation Sans Narrow" },
        { "Georgia", "Gelasio" }, { "Century Gothic", "TeX Gyre Adventor" }, { "Book Antiqua", "TeX Gyre Pagella" },
        { "Palatino", "TeX Gyre Pagella" }, { "Garamond", "EB Garamond" }
    };
    for (const auto& [commercial, free] : substitutes)
    {
        if (family.compare(QLatin1String(commercial), Qt::CaseInsensitive) == 0)
        {
            return QString::fromLatin1(free).split(QChar(';'));
        }
    }
    return QStringList();
}

QStringList PDFFontIdentifier::getLatinFamilies()
{
    static const QRegularExpression foundry(QStringLiteral("\\s*\\[[^\\]]*\\]$"));
    QStringList result;
    for (const QString& family : QFontDatabase::families(QFontDatabase::Latin))
    {
        if (!QFontDatabase::isPrivateFamily(family) && !QFontDatabase::writingSystems(family).contains(QFontDatabase::Symbol))
        {
            result << QString(family).remove(foundry);
        }
    }
    result.removeDuplicates();
    return result;
}

void PDFFontIdentifier::getScanColors(const QImage& scan, QColor& paper, QColor& ink)
{
    const QImage gray = scan.convertToFormat(QImage::Format_Grayscale8);
    const QImage rgb = scan.convertToFormat(QImage::Format_RGB32);
    const int threshold = PDFSignatureLibrary::getAutomaticThreshold(gray);

    std::vector<QRgb> paperPixels;
    std::vector<QRgb> inkPixels;
    for (int y = 0; y < gray.height(); ++y)
    {
        const uchar* grayLine = gray.constScanLine(y);
        const QRgb* rgbLine = reinterpret_cast<const QRgb*>(rgb.constScanLine(y));
        for (int x = 0; x < gray.width(); ++x)
        {
            (grayLine[x] < threshold ? inkPixels : paperPixels).push_back(rgbLine[x]);
        }
    }

    auto median = [](std::vector<QRgb>& pixels, QColor fallback)
    {
        if (pixels.empty())
        {
            return fallback;
        }
        auto channel = [&pixels](auto get)
        {
            std::vector<int> values;
            values.reserve(pixels.size());
            for (QRgb pixel : pixels)
            {
                values.push_back(get(pixel));
            }
            std::nth_element(values.begin(), values.begin() + values.size() / 2, values.end());
            return values[values.size() / 2];
        };
        return QColor(channel([](QRgb p) { return qRed(p); }), channel([](QRgb p) { return qGreen(p); }), channel([](QRgb p) { return qBlue(p); }));
    };
    paper = median(paperPixels, Qt::white);
    ink = median(inkPixels, Qt::black);
}

std::vector<PDFFontIdentifier::Match> PDFFontIdentifier::matchScan(const QImage& scan, const QString& text, const QStringList& families, int maximalResults)
{
    std::vector<Match> matches;
    const QString trimmedText = text.simplified();
    if (scan.isNull() || trimmedText.isEmpty())
    {
        return matches;
    }

    const QImage scanGray = scan.convertToFormat(QImage::Format_Grayscale8);
    const int scanThreshold = PDFSignatureLibrary::getAutomaticThreshold(scanGray);
    int scanWidth = 0;
    double scanAspect = 0.0;
    const std::vector<bool> scanMask = PDFFontIdentifierHelper::getMask(scanGray, scanThreshold, scanWidth, scanAspect);
    if (scanMask.empty())
    {
        return matches;
    }

    // Height of the ink of the scan, used to size the font in the edit
    int scanInkHeight = 0;
    {
        int top = scanGray.height();
        int bottom = -1;
        for (int y = 0; y < scanGray.height(); ++y)
        {
            const uchar* line = scanGray.constScanLine(y);
            for (int x = 0; x < scanGray.width(); ++x)
            {
                if (line[x] < scanThreshold)
                {
                    top = qMin(top, y);
                    bottom = qMax(bottom, y);
                    break;
                }
            }
        }
        scanInkHeight = qMax(1, bottom - top + 1);
    }

    constexpr int renderPixelSize = 64;
    for (const QString& family : families)
    {
        for (const bool bold : { false, true })
        {
            for (const bool italic : { false, true })
            {
                QFont font(family);
                font.setPixelSize(renderPixelSize);
                font.setBold(bold);
                font.setItalic(italic);
                font.setStyleStrategy(QFont::NoFontMerging);

                const QFontMetricsF metrics(font);
                const QRectF bounds = metrics.boundingRect(trimmedText);
                if (bounds.isEmpty())
                {
                    continue;
                }

                QImage rendered(qCeil(bounds.width()) + 8, qCeil(bounds.height()) + 8, QImage::Format_Grayscale8);
                rendered.fill(255);
                {
                    QPainter painter(&rendered);
                    painter.setRenderHint(QPainter::TextAntialiasing);
                    painter.setFont(font);
                    painter.setPen(Qt::black);
                    painter.drawText(QPointF(4 - bounds.left(), 4 - bounds.top()), trimmedText);
                }

                int renderedWidth = 0;
                double renderedAspect = 0.0;
                const std::vector<bool> renderedMask = PDFFontIdentifierHelper::getMask(rendered, 128, renderedWidth, renderedAspect);
                if (renderedMask.empty())
                {
                    continue;
                }

                // Compared at the width of the scan; a different width of the whole
                // text is itself a difference of the fonts
                const double shape = PDFFontIdentifierHelper::compare(scanMask, PDFFontIdentifierHelper::resample(renderedMask, renderedWidth, scanWidth), scanWidth);
                const double widthLikeness = std::exp(-2.5 * std::abs(std::log(renderedAspect / scanAspect)));

                Match match;
                match.family = family;
                match.bold = bold;
                match.italic = italic;
                match.score = shape * widthLikeness;
                match.pixelSize = renderPixelSize * double(scanInkHeight) / double(qMax(1.0, std::ceil(bounds.height())));
                matches.push_back(match);
            }
        }
    }

    std::sort(matches.begin(), matches.end(), [](const Match& a, const Match& b) { return a.score > b.score; });

    // One entry per family, the best of its styles
    std::vector<Match> result;
    for (const Match& match : matches)
    {
        if (std::none_of(result.cbegin(), result.cend(), [&match](const Match& other) { return other.family == match.family; }))
        {
            result.push_back(match);
            if (int(result.size()) >= maximalResults)
            {
                break;
            }
        }
    }
    return result;
}

}   // namespace pdf
