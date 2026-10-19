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

#ifndef PDFLONGTERMVALIDATION_H
#define PDFLONGTERMVALIDATION_H

#include "pdfglobal.h"
#include "pdfcertificatemanager.h"
#include "pdfdocumentsigner.h"

#include <QByteArray>
#include <QString>
#include <QStringList>

namespace pdf
{

/// Long-term validation of signed documents (PAdES baseline B-LT and B-LTA, ETSI
/// EN 319 142-1): the certificates of all signatures and timestamps, and their
/// revocation data (OCSP responses, CRLs), are stored in the document security
/// store (/DSS) of an incremental update, so a third party can still check the
/// signatures when the certificates have expired or the services are gone. A
/// document timestamp over it (B-LTA) protects the stored data.
class PDF4QTLIBCORESHARED_EXPORT PDFLongTermValidation
{
    Q_DECLARE_TR_FUNCTIONS(pdf::PDFLongTermValidation)

public:
    PDFLongTermValidation() = delete;

    struct Result
    {
        QByteArray document;            ///< Extended document, empty when it could not be written
        int certificateCount = 0;       ///< Certificates added to the store
        int ocspCount = 0;              ///< OCSP responses added to the store
        int crlCount = 0;               ///< CRLs added to the store
        QStringList revokedCertificates;///< Subjects of certificates reported as revoked
        QStringList warnings;           ///< Certificates whose revocation data was not obtained
        QString error;                  ///< Why the document was not written

        bool isOK() const { return !document.isEmpty(); }
    };

    /// Adds the validation data of every signature of the document. Revocation
    /// data is fetched from the addresses in the certificates (OCSP first, then the
    /// CRL), the issuer of a certificate missing in the signature is fetched from its
    /// "CA issuers" address. Every response is checked against the issuer before it
    /// is stored. Self-signed (root) certificates need no revocation data.
    /// \param signedDocument Bytes of the signed document
    /// \param password Password opening the document, when it is encrypted
    /// \param timeoutMilliseconds Limit of one network request
    static Result addValidationData(const QByteArray& signedDocument, const QString& password, int timeoutMilliseconds = 15000);

    /// Adds a document timestamp (ETSI.RFC3161) as an incremental update. Returns
    /// false and sets \p errorMessage, when it was not added.
    /// \param document Bytes of the document
    /// \param password Password opening the document, when it is encrypted
    /// \param settings Timestamp authority
    /// \param result Bytes of the timestamped document
    /// \param errorMessage Reason of a failure
    static bool addDocumentTimestamp(const QByteArray& document,
                                     const QString& password,
                                     const PDFSignatureFactory::TimestampSettings& settings,
                                     QByteArray& result,
                                     QString& errorMessage,
                                     QString* fieldName = nullptr);

    /// Extension of a signed document to PAdES B-LTA
    struct Extension
    {
        QByteArray document;            ///< The extended document; the signed document itself when nothing could be added
        Result validation;              ///< Validation data added (B-LT)
        QString timestampFieldName;     ///< Field of the document timestamp, empty when none was added
        QString timestampError;         ///< Why the document timestamp was not added

        /// Lines describing the result for the user
        QStringList getReport() const;
    };

    /// Adds the validation data, then the document timestamp over it. Each step
    /// that fails is skipped and reported; the signed bytes are never changed.
    static Extension extend(const QByteArray& signedDocument, const QString& password, const PDFSignatureFactory::TimestampSettings& settings);

    /// Checks a saved file, which must be the signed document followed by its
    /// extension: the signature is checked in the signed document, the saved file
    /// must begin with exactly those bytes and its document timestamp must be valid.
    static PDFDocumentSigner::SignedFileCheck checkExtendedFile(const QByteArray& file,
                                                                const QByteArray& signedDocument,
                                                                const QString& signatureFieldName,
                                                                const QString& timestampFieldName,
                                                                const QString& password);

    /// Largest accepted OCSP response, CA certificate and CRL
    static constexpr qint64 MAXIMAL_OCSP_SIZE = 1024 * 1024;
    static constexpr qint64 MAXIMAL_CERTIFICATE_SIZE = 1024 * 1024;
    static constexpr qint64 MAXIMAL_CRL_SIZE = 10 * 1024 * 1024;
};

}   // namespace pdf

#endif // PDFLONGTERMVALIDATION_H
