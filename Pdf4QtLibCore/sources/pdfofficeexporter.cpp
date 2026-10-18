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

#include "pdfofficeexporter.h"
#include "pdfdocument.h"
#include "pdftextlayout.h"
#include "pdftextlayoutgenerator.h"
#include "pdfcms.h"
#include "pdffont.h"
#include "pdfcolorspaces.h"
#include "pdfconstants.h"
#include "pdfoptionalcontent.h"

#include <QFile>
#include <QSaveFile>
#include <QDateTime>
#include <QRegularExpression>

#include <zlib.h>

#include <algorithm>

#include "pdfdbgheap.h"

namespace pdf
{

class PDFOfficeExporterHelper
{
public:
    PDFOfficeExporterHelper() = delete;

    static QString escape(const QString& text)
    {
        QString result = text.toHtmlEscaped();
        // XML 1.0 has no control characters except tab, new line and carriage return
        result.removeIf([](QChar c) { return c.unicode() < 0x20 && c != '\t' && c != '\n' && c != '\r'; });
        return result;
    }

    static void appendUInt16(QByteArray& data, quint16 value)
    {
        data.append(char(value & 0xFF));
        data.append(char((value >> 8) & 0xFF));
    }

    static void appendUInt32(QByteArray& data, quint32 value)
    {
        appendUInt16(data, quint16(value & 0xFFFF));
        appendUInt16(data, quint16(value >> 16));
    }

    /// Column name of Excel: 0 -> A, 25 -> Z, 26 -> AA
    static QString getColumnName(int column)
    {
        QString name;
        for (++column; column > 0; column = (column - 1) / 26)
        {
            name.prepend(QChar('A' + (column - 1) % 26));
        }
        return name;
    }

    static bool write(const QString& fileName, const QByteArray& data, QString* errorMessage)
    {
        QSaveFile file(fileName);
        if (file.open(QFile::WriteOnly) && file.write(data) == data.size() && file.commit())
        {
            return true;
        }
        if (errorMessage)
        {
            *errorMessage = PDFOfficeExporter::tr("The file %1 cannot be written: %2").arg(fileName, file.errorString());
        }
        return false;
    }
};

PDFOfficeExporter::Pages PDFOfficeExporter::extract(const PDFDocument* document)
{
    Pages pages;
    const PDFCatalog* catalog = document->getCatalog();

    PDFFontCache fontCache(DEFAULT_FONT_CACHE_LIMIT, DEFAULT_REALIZED_FONT_CACHE_LIMIT);
    PDFICCProfileCache iccProfileCache;
    PDFCMSGeneric cms;
    PDFMeshQualitySettings meshQuality;
    PDFOptionalContentActivity optionalContentActivity(document, OCUsage::Export, nullptr);
    PDFModifiedDocument modifiedDocument(const_cast<PDFDocument*>(document), &optionalContentActivity);
    fontCache.setDocument(modifiedDocument);
    iccProfileCache.setDocument(modifiedDocument);

    for (size_t pageIndex = 0; pageIndex < catalog->getPageCount(); ++pageIndex)
    {
        PDFTextLayoutGenerator generator(PDFRenderer::IgnoreOptionalContent, catalog->getPage(pageIndex), document, &fontCache, &iccProfileCache, &cms, &optionalContentActivity, QTransform(), meshQuality);
        generator.processContents();
        const PDFTextLayout layout = generator.createTextLayout();

        // The characters of the whole page are regrouped into lines by their
        // vertical position: a table is laid out as blocks of columns, but its
        // rows are the lines of a spreadsheet
        struct Character
        {
            QChar character;
            QRectF box;
            bool isSpaceAfter = false;  ///< A space (or line end) follows it in the text
        };
        std::vector<Character> characters;
        for (const PDFTextFlow& flow : PDFTextFlow::createTextFlows(layout, PDFTextFlow::FlowFlags(PDFTextFlow::SeparateBlocks) | PDFTextFlow::RemoveSoftHyphen, PDFInteger(pageIndex)))
        {
            const QString text = flow.getText();
            const std::vector<QRectF> boxes = flow.getBoundingBoxes();
            for (qsizetype i = 0; i < text.size() && size_t(i) < boxes.size(); ++i)
            {
                if (!text[i].isSpace() && boxes[i].isValid())
                {
                    characters.push_back({ text[i], boxes[i], i + 1 >= text.size() || text[i + 1].isSpace() });
                }
            }
        }

        // Top of the page first (y goes up), then left to right
        std::stable_sort(characters.begin(), characters.end(), [](const Character& a, const Character& b) { return a.box.center().y() > b.box.center().y(); });

        // A character belongs to the line, when its centre is within the half height of
        // the line (punctuation like a comma is small and sits low, it must not start a line)
        std::vector<std::vector<Character>> lines;
        qreal lineCenter = 0.0;
        qreal lineHeight = 0.0;
        for (const Character& character : characters)
        {
            const qreal tolerance = 0.6 * std::max({ lineHeight, character.box.height(), qreal(2.0) });
            if (lines.empty() || qAbs(character.box.center().y() - lineCenter) > tolerance)
            {
                lines.emplace_back();
                lineCenter = character.box.center().y();
                lineHeight = 0.0;
            }
            if (character.box.height() > lineHeight)
            {
                lineHeight = character.box.height();
                lineCenter = character.box.center().y();
            }
            lines.back().push_back(character);
        }

        Page page;
        for (std::vector<Character>& line : lines)
        {
            std::stable_sort(line.begin(), line.end(), [](const Character& a, const Character& b) { return a.box.left() < b.box.left(); });

            // The word spaces come from the text; a gap of one and a half of the height
            // of the text (about its em) separates cells.
            // ponytail: one threshold per line; a column gap narrower than 1.5 em merges
            // two cells, then the user splits them in Excel.
            std::vector<qreal> heights;
            for (const Character& character : line)
            {
                heights.push_back(character.box.height());
            }
            std::nth_element(heights.begin(), heights.begin() + heights.size() / 2, heights.end());
            const qreal em = qMax(heights[heights.size() / 2], 1.0);

            TextLine textLine;
            QString cell;
            for (size_t i = 0; i < line.size(); ++i)
            {
                if (i > 0)
                {
                    const qreal gap = line[i].box.left() - line[i - 1].box.right();
                    if (gap > 1.5 * em)
                    {
                        textLine.cells << cell;
                        cell.clear();
                    }
                    else if (line[i - 1].isSpaceAfter)
                    {
                        cell += QChar(' ');
                    }
                }
                cell += line[i].character;
            }
            textLine.cells << cell;
            textLine.text = textLine.cells.join(QChar(' '));
            page.push_back(std::move(textLine));
        }
        pages.push_back(std::move(page));
    }

    return pages;
}

bool PDFOfficeExporter::parseNumber(const QString& text, double& value)
{
    QString number = text.trimmed();
    number.remove(QRegularExpression(QStringLiteral("^(R|ZAR|\\$|€|£)\\s*")));
    number.remove(QChar(' '));
    number.remove(QChar(0x00A0));
    static const QRegularExpression numeric(QStringLiteral("^-?[0-9][0-9.,]*$"));
    if (number.isEmpty() || !numeric.match(number).hasMatch())
    {
        return false;
    }

    // The last separator is the decimal one, when two or fewer digits follow it
    const qsizetype lastComma = number.lastIndexOf(QChar(','));
    const qsizetype lastPoint = number.lastIndexOf(QChar('.'));
    const qsizetype last = qMax(lastComma, lastPoint);
    QString normalized;
    if (last >= 0 && number.size() - last - 1 <= 2 && number.size() - last - 1 > 0)
    {
        normalized = number.left(last);
        normalized.remove(QChar(',')).remove(QChar('.'));
        normalized += QChar('.') + number.mid(last + 1);
    }
    else
    {
        normalized = number;
        normalized.remove(QChar(',')).remove(QChar('.'));
    }

    bool ok = false;
    value = normalized.toDouble(&ok);
    return ok;
}

QByteArray PDFOfficeExporter::createZip(const std::vector<ZipEntry>& entries)
{
    using Helper = PDFOfficeExporterHelper;

    // DOS time and date of now
    const QDateTime now = QDateTime::currentDateTime();
    const quint16 dosTime = quint16((now.time().hour() << 11) | (now.time().minute() << 5) | (now.time().second() / 2));
    const quint16 dosDate = quint16(((now.date().year() - 1980) << 9) | (now.date().month() << 5) | now.date().day());

    QByteArray archive;
    QByteArray directory;
    for (const ZipEntry& entry : entries)
    {
        const QByteArray name = entry.name.toUtf8();
        const quint32 crc = quint32(crc32(crc32(0L, Z_NULL, 0), reinterpret_cast<const Bytef*>(entry.data.constData()), uInt(entry.data.size())));
        const quint32 offset = quint32(archive.size());

        // Local file header, version 2.0, UTF-8 names (bit 11), stored
        Helper::appendUInt32(archive, 0x04034b50);
        Helper::appendUInt16(archive, 20);
        Helper::appendUInt16(archive, 0x0800);
        Helper::appendUInt16(archive, 0);
        Helper::appendUInt16(archive, dosTime);
        Helper::appendUInt16(archive, dosDate);
        Helper::appendUInt32(archive, crc);
        Helper::appendUInt32(archive, quint32(entry.data.size()));
        Helper::appendUInt32(archive, quint32(entry.data.size()));
        Helper::appendUInt16(archive, quint16(name.size()));
        Helper::appendUInt16(archive, 0);
        archive.append(name);
        archive.append(entry.data);

        // Central directory entry
        Helper::appendUInt32(directory, 0x02014b50);
        Helper::appendUInt16(directory, 20);
        Helper::appendUInt16(directory, 20);
        Helper::appendUInt16(directory, 0x0800);
        Helper::appendUInt16(directory, 0);
        Helper::appendUInt16(directory, dosTime);
        Helper::appendUInt16(directory, dosDate);
        Helper::appendUInt32(directory, crc);
        Helper::appendUInt32(directory, quint32(entry.data.size()));
        Helper::appendUInt32(directory, quint32(entry.data.size()));
        Helper::appendUInt16(directory, quint16(name.size()));
        Helper::appendUInt16(directory, 0);
        Helper::appendUInt16(directory, 0);
        Helper::appendUInt16(directory, 0);
        Helper::appendUInt16(directory, 0);
        Helper::appendUInt32(directory, 0);
        Helper::appendUInt32(directory, offset);
        directory.append(name);
    }

    const quint32 directoryOffset = quint32(archive.size());
    archive.append(directory);
    Helper::appendUInt32(archive, 0x06054b50);
    Helper::appendUInt16(archive, 0);
    Helper::appendUInt16(archive, 0);
    Helper::appendUInt16(archive, quint16(entries.size()));
    Helper::appendUInt16(archive, quint16(entries.size()));
    Helper::appendUInt32(archive, quint32(directory.size()));
    Helper::appendUInt32(archive, directoryOffset);
    Helper::appendUInt16(archive, 0);
    return archive;
}

bool PDFOfficeExporter::writeDocx(const QString& fileName, const Pages& pages, QString* errorMessage)
{
    using Helper = PDFOfficeExporterHelper;

    QString body;
    for (size_t pageIndex = 0; pageIndex < pages.size(); ++pageIndex)
    {
        for (const TextLine& line : pages[pageIndex])
        {
            // Cells of a table row are kept apart by tabs
            body += QStringLiteral("<w:p><w:r><w:t xml:space=\"preserve\">%1</w:t></w:r></w:p>").arg(Helper::escape(line.cells.join(QChar('\t')))).replace(QStringLiteral("\t"), QStringLiteral("</w:t><w:tab/><w:t xml:space=\"preserve\">"));
        }
        if (pageIndex + 1 < pages.size())
        {
            body += QStringLiteral("<w:p><w:r><w:br w:type=\"page\"/></w:r></w:p>");
        }
    }

    std::vector<ZipEntry> entries;
    entries.push_back({ QStringLiteral("[Content_Types].xml"), QByteArray(
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
        "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
        "<Default Extension=\"xml\" ContentType=\"application/xml\"/>"
        "<Override PartName=\"/word/document.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml\"/>"
        "</Types>") });
    entries.push_back({ QStringLiteral("_rels/.rels"), QByteArray(
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"word/document.xml\"/>"
        "</Relationships>") });
    entries.push_back({ QStringLiteral("word/document.xml"), QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\"><w:body>%1"
        "<w:sectPr><w:pgSz w:w=\"11906\" w:h=\"16838\"/><w:pgMar w:top=\"1134\" w:right=\"1134\" w:bottom=\"1134\" w:left=\"1134\" w:header=\"709\" w:footer=\"709\" w:gutter=\"0\"/></w:sectPr>"
        "</w:body></w:document>").arg(body).toUtf8() });

    return Helper::write(fileName, createZip(entries), errorMessage);
}

bool PDFOfficeExporter::writeXlsx(const QString& fileName, const Pages& pages, QString* errorMessage)
{
    using Helper = PDFOfficeExporterHelper;

    QString contentTypes = QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
        "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
        "<Default Extension=\"xml\" ContentType=\"application/xml\"/>"
        "<Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>");
    QString sheets;
    QString relationships;
    std::vector<ZipEntry> sheetEntries;

    const size_t sheetCount = qMax(pages.size(), size_t(1));
    for (size_t pageIndex = 0; pageIndex < sheetCount; ++pageIndex)
    {
        QString rows;
        if (pageIndex < pages.size())
        {
            for (size_t row = 0; row < pages[pageIndex].size(); ++row)
            {
                QString cells;
                const QStringList& lineCells = pages[pageIndex][row].cells;
                for (int column = 0; column < lineCells.size(); ++column)
                {
                    const QString reference = Helper::getColumnName(column) + QString::number(row + 1);
                    double number = 0.0;
                    if (parseNumber(lineCells[column], number))
                    {
                        cells += QStringLiteral("<c r=\"%1\"><v>%2</v></c>").arg(reference, QString::number(number, 'g', 15));
                    }
                    else
                    {
                        cells += QStringLiteral("<c r=\"%1\" t=\"inlineStr\"><is><t xml:space=\"preserve\">%2</t></is></c>").arg(reference, Helper::escape(lineCells[column]));
                    }
                }
                rows += QStringLiteral("<row r=\"%1\">%2</row>").arg(row + 1).arg(cells);
            }
        }

        const int number = int(pageIndex) + 1;
        sheetEntries.push_back({ QString("xl/worksheets/sheet%1.xml").arg(number), QStringLiteral(
            "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
            "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\"><sheetData>%1</sheetData></worksheet>").arg(rows).toUtf8() });
        contentTypes += QString("<Override PartName=\"/xl/worksheets/sheet%1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>").arg(number);
        sheets += QString("<sheet name=\"%1\" sheetId=\"%2\" r:id=\"rId%2\"/>").arg(Helper::escape(tr("Page %1").arg(number)), QString::number(number));
        relationships += QString("<Relationship Id=\"rId%1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet%1.xml\"/>").arg(number);
    }
    contentTypes += QStringLiteral("</Types>");

    std::vector<ZipEntry> entries;
    entries.push_back({ QStringLiteral("[Content_Types].xml"), contentTypes.toUtf8() });
    entries.push_back({ QStringLiteral("_rels/.rels"), QByteArray(
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/>"
        "</Relationships>") });
    entries.push_back({ QStringLiteral("xl/workbook.xml"), QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">"
        "<sheets>%1</sheets></workbook>").arg(sheets).toUtf8() });
    entries.push_back({ QStringLiteral("xl/_rels/workbook.xml.rels"), QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">%1</Relationships>").arg(relationships).toUtf8() });
    entries.insert(entries.end(), sheetEntries.begin(), sheetEntries.end());

    return Helper::write(fileName, createZip(entries), errorMessage);
}

}   // namespace pdf
