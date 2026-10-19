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

#ifndef PDFSIGNATUREREPORT_H
#define PDFSIGNATUREREPORT_H

#include "pdfglobal.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QString>
#include <QStringList>

#include <vector>

namespace pdf
{

/// Verification report of the signatures of a file, for a third party who has to
/// confirm who signed it: the identity in each certificate (subject, issuer, serial
/// number, fingerprints), its chain, its revocation status embedded in the file,
/// the integrity of the signed bytes, the timestamps, the PAdES level, the lock
/// and the status under the South African Electronic Communications and
/// Transactions Act 25 of 2002.
class PDF4QTLIBCORESHARED_EXPORT PDFSignatureReport
{
    Q_DECLARE_TR_FUNCTIONS(pdf::PDFSignatureReport)

public:
    PDFSignatureReport() = delete;

    struct Certificate
    {
        QString subject;            ///< Distinguished name (RFC 2253)
        QString commonName;
        QString email;
        QString issuer;             ///< Distinguished name of the issuer (RFC 2253)
        QString serialNumber;       ///< Hexadecimal
        QString sha256;             ///< Fingerprint of the DER encoding, hexadecimal with colons
        QString sha1;
        QDateTime notBefore;
        QDateTime notAfter;
        QStringList policies;       ///< Certificate policy OIDs
        bool isSelfSigned = false;
        bool isQualified = false;   ///< Has the ETSI qualified certificate statement
    };

    enum class Revocation
    {
        NotChecked,     ///< No revocation data for the certificate in the file
        Good,
        Revoked
    };

    struct Signature
    {
        QString fieldName;
        bool isDocumentTimestamp = false;
        QByteArray subfilter;
        QString padesLevel;             ///< "B-B", "B-T", "B-LT", "B-LTA" or empty (not PAdES)
        QString name;                   ///< /Name of the signature dictionary
        QString reason;
        QString location;
        QString contactInfo;
        QDateTime claimedTime;          ///< /M, the time the signer's computer claimed
        QDateTime timestampTime;        ///< Time of the signature timestamp (trusted time)
        QString timestampAuthority;     ///< Subject of the timestamp authority
        bool isIntegrityValid = false;  ///< The signed bytes are unchanged and the signature is mathematically valid
        bool isCertificateTrusted = false; ///< The chain ends in a trusted root
        QString certificateStatus;
        qint64 signedBytesEnd = 0;      ///< End of the signed byte range
        bool isCoveringWholeFile = false;
        std::vector<Certificate> chain; ///< Signer first, then its issuers
        Revocation revocation = Revocation::NotChecked;
        QString revocationSource;       ///< "OCSP" or "CRL"
        QDateTime revocationTime;       ///< Production time of the OCSP answer or of the CRL
        QStringList errors;
        QStringList warnings;
    };

    struct Report
    {
        QString fileName;
        QString fileSha256;
        qint64 fileSize = 0;
        bool isOpened = false;
        bool isEncrypted = false;
        int certification = 0;          ///< DocMDP permissions, 0 = not certified
        int storedCertificates = 0;     ///< Document security store
        int storedOcsps = 0;
        int storedCrls = 0;
        std::vector<Signature> signatures;
        QDateTime created;
    };

    /// Verifies all signatures of the file and collects the report
    /// \param data Bytes of the file
    /// \param fileName Name of the file, only shown
    /// \param password Password opening the file, when it is encrypted
    /// \param useSystemCertificateStore Trust the roots of the system (Windows) store
    static Report create(const QByteArray& data, const QString& fileName, const QString& password, bool useSystemCertificateStore = true);

    /// Describes a DER encoded certificate (an empty description when it cannot be read)
    static Certificate describeCertificate(const QByteArray& certificateData);

    /// Report as a self-contained HTML document (every value is escaped)
    static QString toHtml(const Report& report);

};

}   // namespace pdf

#endif // PDFSIGNATUREREPORT_H
