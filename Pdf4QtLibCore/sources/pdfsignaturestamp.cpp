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

#include "pdfsignaturestamp.h"
#include "pdfcertificatestore.h"
#include "qrcodegen/qrcodegen.hpp"

#include <QFont>
#include <QPainter>
#include <QPainterPath>
#include <QCryptographicHash>

#include "pdfdbgheap.h"

namespace pdf
{

// Layout of the stamp in points. A module of the QR code of one point (0.35 mm)
// is still read by a phone from a printed page.
static constexpr qreal STAMP_QR_MODULE_SIZE = 1.0;
static constexpr int STAMP_QR_QUIET_ZONE = 2;
static constexpr qreal STAMP_FONT_SIZE = 7.0;
static constexpr qreal STAMP_LINE_HEIGHT = 9.0;
static constexpr qreal STAMP_GAP = 4.0;
static constexpr qreal STAMP_TEXT_WIDTH = 170.0;

PDFSignatureStamp::Data PDFSignatureStamp::create(const PDFCertificateInfo& certificate,
                                                  const QDateTime& signingTime,
                                                  PDFDocumentSigner::Certification certification,
                                                  const QByteArray& originalDocumentData)
{
    Data data;
    data.signer = certificate.getName(PDFCertificateInfo::CommonName);
    data.signingTime = signingTime;
    data.certification = certification;
    data.certificateSha256 = QCryptographicHash::hash(certificate.getCertificateData(), QCryptographicHash::Sha256);
    if (!originalDocumentData.isEmpty())
    {
        data.documentSha256 = QCryptographicHash::hash(originalDocumentData, QCryptographicHash::Sha256);
    }
    return data;
}

QString PDFSignatureStamp::getPayload(const Data& data)
{
    // Plain text, so that a reader sees it in any QR scanner app. The keys are
    // not translated, the payload is read by programs, too.
    QStringList lines;
    lines << QStringLiteral("Andre PDF electronic signature");
    lines << QStringLiteral("Signer: %1").arg(data.signer);
    lines << QStringLiteral("Signed: %1").arg(data.signingTime.toUTC().toString(Qt::ISODate));
    switch (data.certification)
    {
        case PDFDocumentSigner::Certification::NoChanges:
            lines << QStringLiteral("Lock: no changes");
            break;
        case PDFDocumentSigner::Certification::FormFilling:
            lines << QStringLiteral("Lock: form filling and signing");
            break;
        case PDFDocumentSigner::Certification::FormFillingAndAnnotations:
            lines << QStringLiteral("Lock: form filling, signing and comments");
            break;
        case PDFDocumentSigner::Certification::None:
            lines << QStringLiteral("Lock: none");
            break;
    }
    lines << QStringLiteral("Certificate SHA-256: %1").arg(QString::fromLatin1(data.certificateSha256.toHex().toUpper()));
    if (!data.documentSha256.isEmpty())
    {
        lines << QStringLiteral("File before signing SHA-256: %1").arg(QString::fromLatin1(data.documentSha256.toHex().toUpper()));
    }
    return lines.join('\n');
}

QString PDFSignatureStamp::getCertificationText(PDFDocumentSigner::Certification certification)
{
    switch (certification)
    {
        case PDFDocumentSigner::Certification::NoChanges:
            return tr("Certified: no changes allowed");
        case PDFDocumentSigner::Certification::FormFilling:
            return tr("Certified: form filling and signing allowed");
        case PDFDocumentSigner::Certification::FormFillingAndAnnotations:
            return tr("Certified: form filling, signing and comments allowed");
        case PDFDocumentSigner::Certification::None:
            break;
    }
    return QString();
}

QStringList PDFSignatureStamp::getTextLines(const Data& data)
{
    QStringList lines;
    lines << tr("Electronically signed by %1").arg(data.signer);
    lines << data.signingTime.toString("yyyy-MM-dd HH:mm:ss t");
    const QString certificationText = getCertificationText(data.certification);
    if (!certificationText.isEmpty())
    {
        lines << certificationText;
    }
    lines << tr("Certificate %1").arg(formatFingerprint(data.certificateSha256, 8));
    if (!data.documentSha256.isEmpty())
    {
        lines << tr("File before signing %1").arg(formatFingerprint(data.documentSha256, 8));
    }
    lines << tr("Verify: signature panel of a PDF reader");
    return lines;
}

QString PDFSignatureStamp::formatFingerprint(const QByteArray& hash, int bytes)
{
    const QByteArray shown = bytes < 0 ? hash : hash.left(bytes);
    const QString hex = QString::fromLatin1(shown.toHex().toUpper());
    QStringList groups;
    for (int i = 0; i < hex.size(); i += 4)
    {
        groups << hex.mid(i, 4);
    }
    return groups.join(' ');
}

std::vector<std::vector<bool>> PDFSignatureStamp::getQrModules(const QString& payload)
{
    const QByteArray utf8 = payload.toUtf8();
    const qrcodegen::QrCode qrCode = qrcodegen::QrCode::encodeText(utf8.constData(), qrcodegen::QrCode::Ecc::LOW);

    const int size = qrCode.getSize();
    std::vector<std::vector<bool>> modules(size, std::vector<bool>(size, false));
    for (int y = 0; y < size; ++y)
    {
        for (int x = 0; x < size; ++x)
        {
            modules[y][x] = qrCode.getModule(x, y);
        }
    }
    return modules;
}

QSizeF PDFSignatureStamp::getSize(const Data& data)
{
    const qreal qrSize = (getQrModules(getPayload(data)).size() + 2 * STAMP_QR_QUIET_ZONE) * STAMP_QR_MODULE_SIZE;
    const qreal textHeight = getTextLines(data).size() * STAMP_LINE_HEIGHT + 2.0;
    return QSizeF(qrSize + STAMP_GAP + STAMP_TEXT_WIDTH, qMax(qrSize, textHeight));
}

void PDFSignatureStamp::draw(QPainter* painter, const QRectF& rect, const Data& data)
{
    const std::vector<std::vector<bool>> modules = getQrModules(getPayload(data));
    const int moduleCount = int(modules.size());
    const qreal qrSize = (moduleCount + 2 * STAMP_QR_QUIET_ZONE) * STAMP_QR_MODULE_SIZE;

    painter->save();

    // QR code at the top left corner of the stamp (the top has the larger y),
    // on a white background with its quiet zone
    const QRectF qrRect(rect.left(), rect.bottom() - qrSize, qrSize, qrSize);
    painter->setPen(Qt::NoPen);
    painter->setBrush(Qt::white);
    painter->drawRect(qrRect);

    QPainterPath path;
    for (int row = 0; row < moduleCount; ++row)
    {
        for (int column = 0; column < moduleCount; ++column)
        {
            if (modules[row][column])
            {
                // Row 0 is the top row, so it has the largest y
                const qreal x = qrRect.left() + (column + STAMP_QR_QUIET_ZONE) * STAMP_QR_MODULE_SIZE;
                const qreal y = qrRect.bottom() - (row + 1 + STAMP_QR_QUIET_ZONE) * STAMP_QR_MODULE_SIZE;
                path.addRect(QRectF(x, y, STAMP_QR_MODULE_SIZE, STAMP_QR_MODULE_SIZE));
            }
        }
    }
    path.setFillRule(Qt::WindingFill);
    painter->setBrush(Qt::black);
    painter->drawPath(path);

    // Text to the right of the QR code, drawn with the y axis flipped back
    const QRectF textRect(qrRect.right() + STAMP_GAP, rect.top(), rect.right() - qrRect.right() - STAMP_GAP, rect.height());
    QFont font("Arial");
    font.setPixelSize(qRound(STAMP_FONT_SIZE));
    painter->setFont(font);
    painter->setPen(QColor(0, 0, 128));
    painter->translate(textRect.center());
    painter->scale(1.0, -1.0);
    painter->drawText(QRectF(-textRect.width() * 0.5, -textRect.height() * 0.5, textRect.width(), textRect.height()),
                      Qt::AlignLeft | Qt::AlignTop, getTextLines(data).join('\n'));

    painter->restore();
}

}   // namespace pdf
