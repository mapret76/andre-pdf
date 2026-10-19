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

#include "pdfsignaturelibrary.h"

#include <QDir>
#include <QBuffer>
#include <QFileInfo>
#include <QSaveFile>
#include <QDataStream>
#include <QStandardPaths>

#ifdef Q_OS_WIN
#include <Windows.h>
#include <dpapi.h>
#if defined(PDF4QT_USE_PRAGMA_LIB)
#pragma comment(lib, "crypt32.lib")
#endif
#endif

#include "pdfdbgheap.h"

namespace pdf
{

static constexpr const char* SIGNATURE_LIBRARY_MAGIC = "ANDREPDF-SIGNATURES";
static constexpr quint32 SIGNATURE_LIBRARY_VERSION = 1;
static constexpr const char* PROTECTED_DPAPI_MARK = "DPAPI1";
static constexpr const char* PROTECTED_PLAIN_MARK = "PLAIN1";

// Width of the band around the threshold, in which the ink fades out. It keeps the
// anti-aliased edges of the strokes smooth.
static constexpr int EXTRACT_EDGE_BAND = 24;
static constexpr int EXTRACT_MARGIN = 4;
static constexpr int EXTRACT_MAXIMAL_WIDTH = 1200;

QString PDFSignatureLibrary::getDisplayName(int index) const
{
    const Slot& slot = getSlot(index);
    return slot.name.isEmpty() ? tr("Signature %1").arg(index + 1) : slot.name;
}

QByteArray PDFSignatureLibrary::serialize() const
{
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_0);
    stream << QByteArray(SIGNATURE_LIBRARY_MAGIC) << SIGNATURE_LIBRARY_VERSION << quint32(m_slots.size());
    for (const Slot& slot : m_slots)
    {
        stream << slot.name << slot.png;
    }
    return data;
}

bool PDFSignatureLibrary::deserialize(const QByteArray& data)
{
    QDataStream stream(data);
    stream.setVersion(QDataStream::Qt_6_0);

    QByteArray magic;
    quint32 version = 0;
    quint32 count = 0;
    stream >> magic >> version >> count;
    if (stream.status() != QDataStream::Ok || magic != SIGNATURE_LIBRARY_MAGIC || version != SIGNATURE_LIBRARY_VERSION || count > 64)
    {
        return false;
    }

    std::array<Slot, SLOT_COUNT> newSlots;
    for (quint32 i = 0; i < count; ++i)
    {
        Slot slot;
        stream >> slot.name >> slot.png;
        if (i < newSlots.size())
        {
            newSlots[i] = std::move(slot);
        }
    }

    if (stream.status() != QDataStream::Ok)
    {
        return false;
    }

    m_slots = std::move(newSlots);
    return true;
}

QString PDFSignatureLibrary::getDefaultFileName()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/signature-library.dat");
}

bool PDFSignatureLibrary::load(const QString& fileName, QString* errorMessage)
{
    QFile file(fileName);
    if (!file.exists())
    {
        m_slots = { };
        return true;
    }

    if (!file.open(QFile::ReadOnly))
    {
        if (errorMessage)
        {
            *errorMessage = tr("The signature library %1 cannot be opened: %2").arg(QDir::toNativeSeparators(fileName), file.errorString());
        }
        return false;
    }

    const std::optional<QByteArray> data = unprotect(file.readAll());
    file.close();

    if (!data || !deserialize(*data))
    {
        if (errorMessage)
        {
            *errorMessage = tr("The signature library %1 cannot be read. It was saved by another user account or is damaged.").arg(QDir::toNativeSeparators(fileName));
        }
        return false;
    }

    return true;
}

bool PDFSignatureLibrary::save(const QString& fileName, QString* errorMessage) const
{
    QDir().mkpath(QFileInfo(fileName).absolutePath());

    // On Windows only the encrypted form can be read back, so nothing else is written
    const QByteArray data = protect(serialize());
    if (data.isEmpty())
    {
        if (errorMessage)
        {
            *errorMessage = tr("The signature library %1 cannot be encrypted for your Windows account, it was not saved.").arg(QDir::toNativeSeparators(fileName));
        }
        return false;
    }

    QSaveFile file(fileName);
    if (file.open(QFile::WriteOnly))
    {
        file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
        file.write(data);
        if (file.commit())
        {
            return true;
        }
    }

    if (errorMessage)
    {
        *errorMessage = tr("The signature library %1 cannot be saved: %2").arg(QDir::toNativeSeparators(fileName), file.errorString());
    }
    return false;
}

QByteArray PDFSignatureLibrary::protect(const QByteArray& data)
{
#ifdef Q_OS_WIN
    DATA_BLOB input;
    input.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(data.constData()));
    input.cbData = DWORD(data.size());
    DATA_BLOB output = { };
    if (CryptProtectData(&input, L"Andre PDF signature library", nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output))
    {
        QByteArray result = QByteArray(PROTECTED_DPAPI_MARK) + QByteArray(reinterpret_cast<const char*>(output.pbData), int(output.cbData));
        LocalFree(output.pbData);
        return result;
    }
    return QByteArray();
#else
    // ponytail: outside Windows the library relies on the file permissions of the
    // user profile; use the system keyring (libsecret, Android Keystore) if it has to
    // withstand a copied file.
    return QByteArray(PROTECTED_PLAIN_MARK) + data;
#endif
}

std::optional<QByteArray> PDFSignatureLibrary::unprotect(const QByteArray& data)
{
#ifndef Q_OS_WIN
    // On Windows only the DPAPI form is written, a plain file planted in the profile
    // is not accepted as the user's signatures
    if (data.startsWith(PROTECTED_PLAIN_MARK))
    {
        return data.mid(int(qstrlen(PROTECTED_PLAIN_MARK)));
    }
#endif

#ifdef Q_OS_WIN
    if (data.startsWith(PROTECTED_DPAPI_MARK))
    {
        const QByteArray encrypted = data.mid(int(qstrlen(PROTECTED_DPAPI_MARK)));
        DATA_BLOB input;
        input.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(encrypted.constData()));
        input.cbData = DWORD(encrypted.size());
        DATA_BLOB output = { };
        if (CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output))
        {
            QByteArray result(reinterpret_cast<const char*>(output.pbData), int(output.cbData));
            SecureZeroMemory(output.pbData, output.cbData);
            LocalFree(output.pbData);
            return result;
        }
    }
#endif

    return std::nullopt;
}

int PDFSignatureLibrary::getAutomaticThreshold(const QImage& grayscale)
{
    std::array<qint64, 256> histogram = { };
    for (int y = 0; y < grayscale.height(); ++y)
    {
        const uchar* line = grayscale.constScanLine(y);
        for (int x = 0; x < grayscale.width(); ++x)
        {
            ++histogram[line[x]];
        }
    }

    // Otsu's method: the level maximizing the variance between the two classes
    const qint64 total = qint64(grayscale.width()) * grayscale.height();
    double sum = 0.0;
    for (int i = 0; i < 256; ++i)
    {
        sum += double(i) * histogram[i];
    }

    double sumBackground = 0.0;
    qint64 weightBackground = 0;
    // A clean scan has empty levels between ink and paper, all of them maximize the
    // variance equally; the middle of them separates the two classes best.
    double maximalVariance = -1.0;
    int firstThreshold = 128;
    int lastThreshold = 128;
    for (int i = 0; i < 256; ++i)
    {
        weightBackground += histogram[i];
        if (weightBackground == 0)
        {
            continue;
        }

        const qint64 weightForeground = total - weightBackground;
        if (weightForeground == 0)
        {
            break;
        }

        sumBackground += double(i) * histogram[i];
        const double meanBackground = sumBackground / weightBackground;
        const double meanForeground = (sum - sumBackground) / weightForeground;
        const double variance = double(weightBackground) * double(weightForeground) * (meanBackground - meanForeground) * (meanBackground - meanForeground);
        if (variance > maximalVariance * (1.0 + 1e-12))
        {
            maximalVariance = variance;
            firstThreshold = i;
            lastThreshold = i;
        }
        else if (variance >= maximalVariance * (1.0 - 1e-12))
        {
            lastThreshold = i;
        }
    }

    return (firstThreshold + lastThreshold) / 2;
}

QImage PDFSignatureLibrary::extractSignature(const QImage& scan, int threshold, const QColor& inkColor)
{
    if (scan.isNull())
    {
        return QImage();
    }

    QImage source = scan.convertToFormat(QImage::Format_ARGB32);
    if (source.width() > 4 * EXTRACT_MAXIMAL_WIDTH)
    {
        // Photos from a phone are large, the signature does not need such a resolution
        source = source.scaledToWidth(4 * EXTRACT_MAXIMAL_WIDTH, Qt::SmoothTransformation);
    }

    const QImage grayscale = source.convertToFormat(QImage::Format_Grayscale8);
    if (threshold < 0)
    {
        threshold = getAutomaticThreshold(grayscale);
    }

    QImage result(source.size(), QImage::Format_ARGB32);
    int left = result.width();
    int top = result.height();
    int right = -1;
    int bottom = -1;

    for (int y = 0; y < source.height(); ++y)
    {
        const QRgb* sourceLine = reinterpret_cast<const QRgb*>(source.constScanLine(y));
        const uchar* grayLine = grayscale.constScanLine(y);
        QRgb* resultLine = reinterpret_cast<QRgb*>(result.scanLine(y));

        for (int x = 0; x < source.width(); ++x)
        {
            const int gray = grayLine[x];
            int alpha = 0;
            if (gray <= threshold - EXTRACT_EDGE_BAND)
            {
                alpha = 255;
            }
            else if (gray < threshold + EXTRACT_EDGE_BAND)
            {
                alpha = (threshold + EXTRACT_EDGE_BAND - gray) * 255 / (2 * EXTRACT_EDGE_BAND);
            }

            const QRgb color = inkColor.isValid() ? inkColor.rgb() : sourceLine[x];
            resultLine[x] = qRgba(qRed(color), qGreen(color), qBlue(color), alpha);

            if (alpha >= 128)
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
        // No ink at all
        return QImage();
    }

    const QRect inkRect = QRect(QPoint(left, top), QPoint(right, bottom)).adjusted(-EXTRACT_MARGIN, -EXTRACT_MARGIN, EXTRACT_MARGIN, EXTRACT_MARGIN).intersected(result.rect());
    result = result.copy(inkRect);
    if (result.width() > EXTRACT_MAXIMAL_WIDTH)
    {
        result = result.scaledToWidth(EXTRACT_MAXIMAL_WIDTH, Qt::SmoothTransformation);
    }
    return result;
}

QByteArray PDFSignatureLibrary::toPng(const QImage& image)
{
    QByteArray data;
    QBuffer buffer(&data);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return data;
}

}   // namespace pdf
