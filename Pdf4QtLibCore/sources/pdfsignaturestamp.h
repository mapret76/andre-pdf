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

#ifndef PDFSIGNATURESTAMP_H
#define PDFSIGNATURESTAMP_H

#include "pdfglobal.h"
#include "pdfdocumentsigner.h"

#include <QDateTime>
#include <QCoreApplication>
#include <QRectF>
#include <QStringList>
#include <QImage>

#include <optional>

#include <vector>

class QPainter;

namespace pdf
{
class PDFCertificateInfo;

/// Verification stamp drawn into the visible appearance of a signature: a QR code
/// and a few lines of text saying who signed, when, how the document is locked,
/// and the fingerprint of the file as it was before the signature. The stamp
/// itself proves nothing, the proof is the digital signature; the stamp lets a
/// reader check that what he sees matches what was signed.
class PDF4QTLIBCORESHARED_EXPORT PDFSignatureStamp
{
    Q_DECLARE_TR_FUNCTIONS(pdf::PDFSignatureStamp)

public:
    PDFSignatureStamp() = delete;

    struct Data
    {
        QString signer;
        QDateTime signingTime;
        PDFDocumentSigner::Certification certification = PDFDocumentSigner::Certification::None;
        QByteArray certificateSha256;   ///< SHA-256 of the DER encoded signing certificate
        QByteArray documentSha256;      ///< SHA-256 of the file before the signature, empty for a new file
    };

    /// Collects the data of the stamp
    /// \param certificate Signing certificate
    /// \param signingTime Signing time
    /// \param certification Certification of the signature
    /// \param originalDocumentData File the signature is added to, can be empty
    static Data create(const PDFCertificateInfo& certificate,
                       const QDateTime& signingTime,
                       PDFDocumentSigner::Certification certification,
                       const QByteArray& originalDocumentData);

    /// Returns the text encoded in the QR code, readable by any QR scanner
    static QString getPayload(const Data& data);

    /// Returns the lines of text printed next to the QR code
    static QStringList getTextLines(const Data& data);

    /// Returns a description of the certification, empty for an approval signature
    static QString getCertificationText(PDFDocumentSigner::Certification certification);

    /// Returns the modules of the QR code of the payload, row by row, true is dark
    static std::vector<std::vector<bool>> getQrModules(const QString& payload);

    /// Returns the size of the stamp in points
    static QSizeF getSize(const Data& data);

    /// Draws the stamp. The painter works in PDF coordinates (y goes up).
    /// \param painter Painter
    /// \param rect Rectangle of the stamp, of the size from getSize
    /// \param data Data of the stamp
    static void draw(QPainter* painter, const QRectF& rect, const Data& data);

    /// Returns a function, which creates a visible signature field for the signer:
    /// the image fitted into the rectangle and the stamp under it (over it, when the
    /// page has no room under it). Used, where no interactive scene draws the
    /// appearance, for example when a contract pack is signed in a batch.
    /// \param document Document being signed
    /// \param fieldName Name of the signature field
    /// \param pageIndex Page of the field
    /// \param imageRectangle Rectangle of the image, page coordinates (y up)
    /// \param image Signature image, can be null (stamp only)
    /// \param stamp Verification stamp, none for no stamp
    static PDFDocumentSigner::CreateSignatureFieldFunction createVisibleSignatureField(const PDFDocument* document,
                                                                                       QString fieldName,
                                                                                       PDFInteger pageIndex,
                                                                                       QRectF imageRectangle,
                                                                                       QImage image,
                                                                                       std::optional<Data> stamp);

    /// Formats the fingerprint as groups of four hexadecimal digits
    /// \param hash Hash
    /// \param bytes Number of bytes shown, all when negative
    static QString formatFingerprint(const QByteArray& hash, int bytes = -1);
};

}   // namespace pdf

#endif // PDFSIGNATURESTAMP_H
