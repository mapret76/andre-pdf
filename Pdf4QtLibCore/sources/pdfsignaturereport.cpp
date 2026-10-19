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

#include "pdfsignaturereport.h"
#include "pdfcertificatestore.h"
#include "pdfdocumentreader.h"
#include "pdfdocumentsigner.h"
#include "pdfform.h"
#include "pdfocrapplyprocessor.h"
#include "pdfsecurityhandler.h"
#include "pdfsignaturehandler.h"
#include "pdfsignaturestamp.h"

#include <QCryptographicHash>
#include <QFileInfo>

#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/cms.h>
#include <openssl/ocsp.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include "pdfdbgheap.h"

namespace pdf
{

class PDFSignatureReportHelper
{
public:
    PDFSignatureReportHelper() = delete;

    /// Owns the certificates found in the signatures and in the security store
    struct Pool
    {
        ~Pool()
        {
            for (X509* certificate : certificates)
            {
                X509_free(certificate);
            }
        }

        void add(X509* certificate)
        {
            for (X509* existing : certificates)
            {
                if (X509_cmp(existing, certificate) == 0)
                {
                    return;
                }
            }
            X509_up_ref(certificate);
            certificates.push_back(certificate);
        }

        void add(const QByteArray& der)
        {
            const unsigned char* pointer = reinterpret_cast<const unsigned char*>(der.constData());
            if (X509* certificate = d2i_X509(nullptr, &pointer, der.size()))
            {
                add(certificate);
                X509_free(certificate);
            }
        }

        X509* findIssuer(X509* certificate) const
        {
            for (X509* candidate : certificates)
            {
                if (X509_cmp(candidate, certificate) != 0 && X509_check_issued(candidate, certificate) == X509_V_OK &&
                    X509_verify(certificate, X509_get0_pubkey(candidate)) == 1)
                {
                    return candidate;
                }
            }
            return nullptr;
        }

        std::vector<X509*> certificates;
    };

    static CMS_ContentInfo* decode(const QByteArray& data)
    {
        const unsigned char* pointer = reinterpret_cast<const unsigned char*>(data.constData());
        return d2i_CMS_ContentInfo(nullptr, &pointer, data.size());
    }

    /// Adds the certificates of the CMS object; returns its first signer's certificate (not owned)
    static X509* addCertificates(CMS_ContentInfo* cms, Pool& pool)
    {
        X509* signer = nullptr;
        STACK_OF(X509)* certificates = CMS_get1_certs(cms);
        STACK_OF(CMS_SignerInfo)* signerInfos = CMS_get0_SignerInfos(cms);
        CMS_SignerInfo* signerInfo = sk_CMS_SignerInfo_num(signerInfos) > 0 ? sk_CMS_SignerInfo_value(signerInfos, 0) : nullptr;
        for (int i = 0; i < sk_X509_num(certificates); ++i)
        {
            X509* certificate = sk_X509_value(certificates, i);
            pool.add(certificate);
            if (!signer && signerInfo && CMS_SignerInfo_cert_cmp(signerInfo, certificate) == 0)
            {
                for (X509* pooled : pool.certificates)
                {
                    if (X509_cmp(pooled, certificate) == 0)
                    {
                        signer = pooled;
                    }
                }
            }
        }
        sk_X509_pop_free(certificates, X509_free);
        return signer;
    }

    static QByteArray getTimestampToken(CMS_ContentInfo* cms)
    {
        STACK_OF(CMS_SignerInfo)* signerInfos = CMS_get0_SignerInfos(cms);
        CMS_SignerInfo* signerInfo = sk_CMS_SignerInfo_num(signerInfos) > 0 ? sk_CMS_SignerInfo_value(signerInfos, 0) : nullptr;
        const int index = signerInfo ? CMS_unsigned_get_attr_by_NID(signerInfo, NID_id_smime_aa_timeStampToken, -1) : -1;
        X509_ATTRIBUTE* attribute = index >= 0 ? CMS_unsigned_get_attr(signerInfo, index) : nullptr;
        ASN1_TYPE* value = attribute ? X509_ATTRIBUTE_get0_type(attribute, 0) : nullptr;
        if (value && value->type == V_ASN1_SEQUENCE)
        {
            return QByteArray(reinterpret_cast<const char*>(value->value.sequence->data), value->value.sequence->length);
        }
        return QByteArray();
    }

    static bool hasSigningCertificateV2(CMS_ContentInfo* cms)
    {
        STACK_OF(CMS_SignerInfo)* signerInfos = CMS_get0_SignerInfos(cms);
        CMS_SignerInfo* signerInfo = sk_CMS_SignerInfo_num(signerInfos) > 0 ? sk_CMS_SignerInfo_value(signerInfos, 0) : nullptr;
        return signerInfo && CMS_signed_get_attr_by_NID(signerInfo, NID_id_smime_aa_signingCertificateV2, -1) >= 0;
    }

    static QString printName(X509_NAME* name)
    {
        BIO* bio = BIO_new(BIO_s_mem());
        X509_NAME_print_ex(bio, name, 0, XN_FLAG_RFC2253 & ~ASN1_STRFLGS_ESC_MSB);
        BUF_MEM* memory = nullptr;
        BIO_get_mem_ptr(bio, &memory);
        const QString result = QString::fromUtf8(memory->data, int(memory->length));
        BIO_free(bio);
        return result;
    }

    static QString getEntry(X509_NAME* name, int nid)
    {
        const int index = X509_NAME_get_index_by_NID(name, nid, -1);
        X509_NAME_ENTRY* entry = index >= 0 ? X509_NAME_get_entry(name, index) : nullptr;
        unsigned char* text = nullptr;
        const int length = entry ? ASN1_STRING_to_UTF8(&text, X509_NAME_ENTRY_get_data(entry)) : -1;
        const QString result = length >= 0 ? QString::fromUtf8(reinterpret_cast<const char*>(text), length) : QString();
        OPENSSL_free(text);
        return result;
    }

    static QDateTime toDateTime(const ASN1_TIME* time)
    {
        struct tm value = {};
        if (!time || ASN1_TIME_to_tm(time, &value) != 1)
        {
            return QDateTime();
        }
        return QDateTime(QDate(value.tm_year + 1900, value.tm_mon + 1, value.tm_mday), QTime(value.tm_hour, value.tm_min, value.tm_sec), QTimeZone::UTC);
    }

    static QString getFingerprint(X509* certificate, const EVP_MD* digest)
    {
        unsigned char buffer[EVP_MAX_MD_SIZE];
        unsigned int length = 0;
        X509_digest(certificate, digest, buffer, &length);
        return QString::fromLatin1(QByteArray(reinterpret_cast<const char*>(buffer), int(length)).toHex(':').toUpper());
    }

    static PDFSignatureReport::Certificate describe(X509* certificate)
    {
        PDFSignatureReport::Certificate result;
        X509_NAME* subject = X509_get_subject_name(certificate);
        result.subject = printName(subject);
        result.commonName = getEntry(subject, NID_commonName);
        result.issuer = printName(X509_get_issuer_name(certificate));

        STACK_OF(OPENSSL_STRING)* emails = X509_get1_email(certificate);
        if (sk_OPENSSL_STRING_num(emails) > 0)
        {
            result.email = QString::fromUtf8(sk_OPENSSL_STRING_value(emails, 0));
        }
        X509_email_free(emails);

        BIGNUM* serial = ASN1_INTEGER_to_BN(X509_get0_serialNumber(certificate), nullptr);
        char* serialText = serial ? BN_bn2hex(serial) : nullptr;
        result.serialNumber = QString::fromLatin1(serialText);
        OPENSSL_free(serialText);
        BN_free(serial);

        result.sha256 = getFingerprint(certificate, EVP_sha256());
        result.sha1 = getFingerprint(certificate, EVP_sha1());
        result.notBefore = toDateTime(X509_get0_notBefore(certificate));
        result.notAfter = toDateTime(X509_get0_notAfter(certificate));
        result.isSelfSigned = (X509_get_extension_flags(certificate) & EXFLAG_SS) != 0;
        result.isQualified = X509_get_ext_by_NID(certificate, NID_qcStatements, -1) >= 0;

        CERTIFICATEPOLICIES* policies = static_cast<CERTIFICATEPOLICIES*>(X509_get_ext_d2i(certificate, NID_certificate_policies, nullptr, nullptr));
        for (int i = 0; i < sk_POLICYINFO_num(policies); ++i)
        {
            char oid[128] = {};
            OBJ_obj2txt(oid, sizeof(oid), sk_POLICYINFO_value(policies, i)->policyid, 1);
            result.policies << QString::fromLatin1(oid);
        }
        CERTIFICATEPOLICIES_free(policies);
        return result;
    }

    /// Revocation status of the certificate found in the stored OCSP answers and CRLs;
    /// an answer counts only when the issuer (or its authorised responder) signed it
    static PDFSignatureReport::Revocation getRevocation(X509* certificate, X509* issuer, const PDFDocumentSecurityStore::SecurityStoreItem* store, QString& source, QDateTime& time)
    {
        if (!issuer || !store)
        {
            return PDFSignatureReport::Revocation::NotChecked;
        }

        for (const QByteArray& data : store->OCSP)
        {
            const unsigned char* pointer = reinterpret_cast<const unsigned char*>(data.constData());
            OCSP_RESPONSE* response = d2i_OCSP_RESPONSE(nullptr, &pointer, data.size());
            OCSP_BASICRESP* basic = (response && OCSP_response_status(response) == OCSP_RESPONSE_STATUS_SUCCESSFUL) ? OCSP_response_get1_basic(response) : nullptr;
            PDFSignatureReport::Revocation result = PDFSignatureReport::Revocation::NotChecked;
            for (const EVP_MD* digest : { EVP_sha1(), EVP_sha256() })
            {
                OCSP_CERTID* id = basic ? OCSP_cert_to_id(digest, certificate, issuer) : nullptr;
                int status = -1;
                int reason = -1;
                ASN1_GENERALIZEDTIME* revoked = nullptr;
                ASN1_GENERALIZEDTIME* thisUpdate = nullptr;
                ASN1_GENERALIZEDTIME* nextUpdate = nullptr;
                if (id && OCSP_resp_find_status(basic, id, &status, &reason, &revoked, &thisUpdate, &nextUpdate) == 1)
                {
                    X509_STORE* trust = X509_STORE_new();
                    STACK_OF(X509)* issuers = sk_X509_new_null();
                    X509_STORE_add_cert(trust, issuer);
                    X509_STORE_set_flags(trust, X509_V_FLAG_PARTIAL_CHAIN | X509_V_FLAG_NO_CHECK_TIME);
                    sk_X509_push(issuers, issuer);
                    if (OCSP_basic_verify(basic, issuers, trust, 0) > 0 && status != V_OCSP_CERTSTATUS_UNKNOWN)
                    {
                        result = status == V_OCSP_CERTSTATUS_REVOKED ? PDFSignatureReport::Revocation::Revoked : PDFSignatureReport::Revocation::Good;
                        source = QStringLiteral("OCSP");
                        time = toDateTime(OCSP_resp_get0_produced_at(basic));
                    }
                    sk_X509_free(issuers);
                    X509_STORE_free(trust);
                }
                OCSP_CERTID_free(id);
                if (result != PDFSignatureReport::Revocation::NotChecked)
                {
                    break;
                }
            }
            OCSP_BASICRESP_free(basic);
            OCSP_RESPONSE_free(response);
            if (result != PDFSignatureReport::Revocation::NotChecked)
            {
                return result;
            }
        }

        for (const QByteArray& data : store->CRL)
        {
            const unsigned char* pointer = reinterpret_cast<const unsigned char*>(data.constData());
            X509_CRL* crl = d2i_X509_CRL(nullptr, &pointer, data.size());
            PDFSignatureReport::Revocation result = PDFSignatureReport::Revocation::NotChecked;
            if (crl && X509_NAME_cmp(X509_CRL_get_issuer(crl), X509_get_subject_name(issuer)) == 0 && X509_CRL_verify(crl, X509_get0_pubkey(issuer)) == 1)
            {
                X509_REVOKED* revoked = nullptr;
                result = X509_CRL_get0_by_cert(crl, &revoked, certificate) == 1 ? PDFSignatureReport::Revocation::Revoked : PDFSignatureReport::Revocation::Good;
                source = QStringLiteral("CRL");
                time = toDateTime(X509_CRL_get0_lastUpdate(crl));
            }
            X509_CRL_free(crl);
            if (result != PDFSignatureReport::Revocation::NotChecked)
            {
                return result;
            }
        }
        return PDFSignatureReport::Revocation::NotChecked;
    }

    static QString escape(const QString& text) { return text.toHtmlEscaped(); }

    static QString row(const QString& name, const QString& value)
    {
        return QString("<tr><th align=\"left\" valign=\"top\">%1</th><td>%2</td></tr>\n").arg(escape(name), escape(value));
    }

    static QString formatTime(const QDateTime& time)
    {
        return time.isValid() ? time.toUTC().toString("yyyy-MM-dd HH:mm:ss 'UTC'") : PDFSignatureReport::tr("not available");
    }
};

PDFSignatureReport::Report PDFSignatureReport::create(const QByteArray& data, const QString& fileName, const QString& password, bool useSystemCertificateStore)
{
    Report report;
    report.fileName = QFileInfo(fileName).fileName();
    report.fileSize = data.size();
    report.fileSha256 = QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
    report.created = QDateTime::currentDateTimeUtc();

    bool passwordOffered = false;
    auto getPassword = [&password, &passwordOffered](bool* ok)
    {
        *ok = !passwordOffered;
        passwordOffered = true;
        return password;
    };
    PDFDocumentReader reader(nullptr, getPassword, false, false);
    const PDFDocument document = reader.readFromBuffer(data);
    if (reader.getReadingResult() != PDFDocumentReader::Result::OK)
    {
        return report;
    }

    report.isOpened = true;
    const PDFSecurityHandler* securityHandler = document.getStorage().getSecurityHandler();
    report.isEncrypted = securityHandler && securityHandler->getMode() != EncryptionMode::None;
    report.certification = int(PDFOCRApplyProcessor::getCertificationPermissions(&document));

    const PDFDocumentSecurityStore& dss = document.getCatalog()->getDocumentSecurityStore();
    const PDFDocumentSecurityStore::SecurityStoreItem* store = dss.getMasterItem();
    report.storedCertificates = int(store->Cert.size());
    report.storedOcsps = int(store->OCSP.size());
    report.storedCrls = int(store->CRL.size());

    PDFSignatureReportHelper::Pool pool;
    for (const QByteArray& certificate : store->Cert)
    {
        pool.add(certificate);
    }

    PDFCertificateStore certificateStore;
    PDFSignatureHandler::Parameters parameters;
    parameters.store = &certificateStore;
    parameters.dss = &dss;
    parameters.enableVerification = true;
    parameters.useSystemCertificateStore = useSystemCertificateStore;

    const PDFForm form = PDFForm::parse(&document, document.getCatalog()->getFormObject());
    const std::vector<PDFSignatureVerificationResult> results = PDFSignatureHandler::verifySignatures(form, data, parameters);

    std::vector<const PDFFormFieldSignature*> fields;
    form.apply([&fields](const PDFFormField* field)
    {
        if (const PDFFormFieldSignature* signatureField = dynamic_cast<const PDFFormFieldSignature*>(field))
        {
            if (!signatureField->getSignature().getContents().isEmpty())
            {
                fields.push_back(signatureField);
            }
        }
    });

    // Certificates of all signatures first, so every chain can be completed
    std::vector<CMS_ContentInfo*> messages;
    std::vector<X509*> signers;
    for (const PDFFormFieldSignature* field : fields)
    {
        CMS_ContentInfo* cms = PDFSignatureReportHelper::decode(field->getSignature().getContents());
        X509* signer = cms ? PDFSignatureReportHelper::addCertificates(cms, pool) : nullptr;
        const QByteArray token = cms ? PDFSignatureReportHelper::getTimestampToken(cms) : QByteArray();
        if (CMS_ContentInfo* tokenCms = token.isEmpty() ? nullptr : PDFSignatureReportHelper::decode(token))
        {
            PDFSignatureReportHelper::addCertificates(tokenCms, pool);
            CMS_ContentInfo_free(tokenCms);
        }
        messages.push_back(cms);
        signers.push_back(signer);
    }

    for (size_t i = 0; i < fields.size(); ++i)
    {
        const PDFFormFieldSignature* field = fields[i];
        const PDFSignature& signature = field->getSignature();

        Signature item;
        item.fieldName = field->getName(PDFFormField::NameType::FullyQualified);
        item.subfilter = signature.getSubfilter();
        item.isDocumentTimestamp = signature.getType() == PDFSignature::Type::DocTimeStamp || item.subfilter == "ETSI.RFC3161";
        item.name = signature.getName();
        item.reason = signature.getReason();
        item.location = signature.getLocation();
        item.contactInfo = signature.getContactInfo();
        item.claimedTime = signature.getSigningDateTime();
        for (const PDFSignature::ByteRange& range : signature.getByteRanges())
        {
            item.signedBytesEnd = std::max<qint64>(item.signedBytesEnd, range.offset + range.size);
        }
        item.isCoveringWholeFile = item.signedBytesEnd == data.size();

        for (const PDFSignatureVerificationResult& result : results)
        {
            if (result.getSignatureFieldQualifiedName() == item.fieldName)
            {
                item.isIntegrityValid = result.isSignatureValid();
                item.isCertificateTrusted = result.isCertificateValid();
                item.certificateStatus = result.getCertificateStatusText();
                item.timestampTime = result.getTimestampDate();
                item.errors = result.getErrors();
                item.warnings = result.getWarnings();
            }
        }

        // Chain of the signer, as far as the certificates reach
        X509* certificate = signers[i];
        for (int depth = 0; certificate && depth < 10; ++depth)
        {
            item.chain.push_back(PDFSignatureReportHelper::describe(certificate));
            if (item.chain.back().isSelfSigned)
            {
                break;
            }
            certificate = pool.findIssuer(certificate);
        }

        if (signers[i])
        {
            item.revocation = PDFSignatureReportHelper::getRevocation(signers[i], pool.findIssuer(signers[i]), store, item.revocationSource, item.revocationTime);
        }

        const QByteArray token = messages[i] ? PDFSignatureReportHelper::getTimestampToken(messages[i]) : QByteArray();
        if (CMS_ContentInfo* tokenCms = token.isEmpty() ? nullptr : PDFSignatureReportHelper::decode(token))
        {
            PDFSignatureReportHelper::Pool tokenPool;
            if (X509* authority = PDFSignatureReportHelper::addCertificates(tokenCms, tokenPool))
            {
                item.timestampAuthority = PDFSignatureReportHelper::printName(X509_get_subject_name(authority));
            }
            CMS_ContentInfo_free(tokenCms);
        }
        else if (item.isDocumentTimestamp && signers[i])
        {
            item.timestampAuthority = PDFSignatureReportHelper::printName(X509_get_subject_name(signers[i]));
        }

        // PAdES baseline level (ETSI EN 319 142-1)
        if (!item.isDocumentTimestamp && item.subfilter == "ETSI.CAdES.detached" && messages[i] && PDFSignatureReportHelper::hasSigningCertificateV2(messages[i]))
        {
            item.padesLevel = QStringLiteral("B-B");
            if (!token.isEmpty())
            {
                item.padesLevel = QStringLiteral("B-T");
                if (item.revocation != Revocation::NotChecked)
                {
                    item.padesLevel = QStringLiteral("B-LT");
                }
            }
        }
        report.signatures.push_back(std::move(item));
    }

    // B-LTA: a valid document timestamp written after the B-LT signature
    for (Signature& item : report.signatures)
    {
        if (item.padesLevel != QLatin1String("B-LT"))
        {
            continue;
        }
        for (const Signature& other : report.signatures)
        {
            if (other.isDocumentTimestamp && other.isIntegrityValid && other.signedBytesEnd > item.signedBytesEnd)
            {
                item.padesLevel = QStringLiteral("B-LTA");
            }
        }
    }

    for (CMS_ContentInfo* cms : messages)
    {
        CMS_ContentInfo_free(cms);
    }
    return report;
}

QString PDFSignatureReport::toHtml(const Report& report)
{
    using Helper = PDFSignatureReportHelper;

    QString html;
    html += "<html><head><meta charset=\"utf-8\"><title>" + Helper::escape(tr("Signature verification report")) + "</title></head><body>\n";
    html += "<h1>" + Helper::escape(tr("Signature verification report")) + "</h1>\n";
    html += "<table>\n";
    html += Helper::row(tr("File"), report.fileName);
    html += Helper::row(tr("Size"), tr("%1 bytes").arg(report.fileSize));
    html += Helper::row(tr("SHA-256 of the file"), report.fileSha256);
    html += Helper::row(tr("Report created"), Helper::formatTime(report.created));
    if (!report.isOpened)
    {
        html += "</table><p><b>" + Helper::escape(tr("The file cannot be opened, nothing was verified.")) + "</b></p></body></html>";
        return html;
    }
    html += Helper::row(tr("Encrypted"), report.isEncrypted ? tr("yes") : tr("no"));
    const QString lock = PDFSignatureStamp::getCertificationText(static_cast<PDFDocumentSigner::Certification>(report.certification));
    html += Helper::row(tr("Lock"), lock.isEmpty() ? tr("not certified (no lock)") : lock);
    html += Helper::row(tr("Long-term validation data"), tr("%1 certificates, %2 OCSP responses, %3 CRLs").arg(report.storedCertificates).arg(report.storedOcsps).arg(report.storedCrls));
    html += Helper::row(tr("Signatures"), QString::number(report.signatures.size()));
    html += "</table>\n";

    if (report.signatures.empty())
    {
        html += "<p><b>" + Helper::escape(tr("The file contains no digital signature.")) + "</b></p>\n";
    }

    int index = 0;
    for (const Signature& signature : report.signatures)
    {
        ++index;
        const QString title = signature.isDocumentTimestamp ? tr("Document timestamp %1").arg(index) : tr("Signature %1").arg(index);
        html += "<h2>" + Helper::escape(title) + "</h2>\n<table>\n";
        html += Helper::row(tr("Field"), signature.fieldName);

        QString integrity = signature.isIntegrityValid ? tr("VALID: the signed bytes have not been changed since signing") : tr("INVALID: the document was changed after signing or the signature is damaged");
        if (signature.isIntegrityValid)
        {
            integrity += signature.isCoveringWholeFile ? tr("; the signature covers the whole file")
                                                       : tr("; %1 bytes were appended later (incremental updates: later signatures, timestamps or validation data, or other changes)").arg(report.fileSize - signature.signedBytesEnd);
        }
        html += Helper::row(tr("Integrity"), integrity);

        if (!signature.isDocumentTimestamp)
        {
            html += Helper::row(tr("Format"), signature.padesLevel.isEmpty() ? tr("%1 (not a PAdES baseline signature)").arg(QString::fromLatin1(signature.subfilter))
                                                                             : tr("PAdES baseline %1 (%2)").arg(signature.padesLevel, QString::fromLatin1(signature.subfilter)));
            html += Helper::row(tr("Time claimed by the signer"), Helper::formatTime(signature.claimedTime));
            html += Helper::row(tr("Trusted timestamp"), signature.timestampTime.isValid() ? tr("%1 by %2").arg(Helper::formatTime(signature.timestampTime), signature.timestampAuthority)
                                                                                           : tr("none (the time is only claimed by the signer's computer)"));
        }
        else
        {
            html += Helper::row(tr("Time"), Helper::formatTime(signature.timestampTime));
            html += Helper::row(tr("Timestamp authority"), signature.timestampAuthority);
        }

        if (!signature.reason.isEmpty())
        {
            html += Helper::row(tr("Reason"), signature.reason);
        }
        if (!signature.location.isEmpty())
        {
            html += Helper::row(tr("Location"), signature.location);
        }
        if (!signature.contactInfo.isEmpty())
        {
            html += Helper::row(tr("Contact"), signature.contactInfo);
        }

        html += Helper::row(tr("Certificate trust"), (signature.isCertificateTrusted ? tr("trusted: the chain ends in a trusted root") : tr("NOT trusted by this computer")) + QString(" (%1)").arg(signature.certificateStatus));

        QString revocation;
        switch (signature.revocation)
        {
            case Revocation::Good:
                revocation = tr("not revoked (%1 of %2, embedded in the file)").arg(signature.revocationSource, Helper::formatTime(signature.revocationTime));
                break;
            case Revocation::Revoked:
                revocation = tr("REVOKED (%1 of %2, embedded in the file)").arg(signature.revocationSource, Helper::formatTime(signature.revocationTime));
                break;
            case Revocation::NotChecked:
                revocation = tr("no revocation data embedded in the file");
                break;
        }
        html += Helper::row(tr("Revocation status"), revocation);
        for (const QString& error : signature.errors)
        {
            html += Helper::row(tr("Error"), error);
        }
        for (const QString& warning : signature.warnings)
        {
            html += Helper::row(tr("Warning"), warning);
        }
        html += "</table>\n";

        int level = 0;
        for (const Certificate& certificate : signature.chain)
        {
            const QString heading = level == 0 ? (signature.isDocumentTimestamp ? tr("Certificate of the timestamp authority") : tr("Certificate of the signer"))
                                               : tr("Issuer certificate %1").arg(level);
            html += "<h3>" + Helper::escape(heading) + "</h3>\n<table>\n";
            html += Helper::row(tr("Name"), certificate.commonName);
            if (!certificate.email.isEmpty())
            {
                html += Helper::row(tr("E-mail"), certificate.email);
            }
            html += Helper::row(tr("Subject"), certificate.subject);
            html += Helper::row(tr("Issuer"), certificate.issuer);
            html += Helper::row(tr("Serial number"), certificate.serialNumber);
            html += Helper::row(tr("SHA-256 fingerprint"), certificate.sha256);
            html += Helper::row(tr("SHA-1 fingerprint"), certificate.sha1);
            html += Helper::row(tr("Valid"), tr("%1 to %2").arg(Helper::formatTime(certificate.notBefore), Helper::formatTime(certificate.notAfter)));
            if (!certificate.policies.isEmpty())
            {
                html += Helper::row(tr("Certificate policies"), certificate.policies.join(", "));
            }
            if (certificate.isQualified)
            {
                html += Helper::row(tr("Qualified statement"), tr("present (the issuer declares a qualified certificate)"));
            }
            if (certificate.isSelfSigned)
            {
                html += Helper::row(tr("Self-signed"), level == 0 ? tr("yes: nobody but the signer vouches for this identity") : tr("yes (root)"));
            }
            html += "</table>\n";
            ++level;
        }
        if (signature.chain.empty())
        {
            html += "<p>" + Helper::escape(tr("The certificate of the signer is not in the signature.")) + "</p>\n";
        }
        else if (!signature.chain.back().isSelfSigned)
        {
            html += "<p>" + Helper::escape(tr("The chain is incomplete: the next issuer is not in the file.")) + "</p>\n";
        }

        if (!signature.isDocumentTimestamp && !signature.chain.empty())
        {
            const Certificate& signer = signature.chain.front();
            html += "<h3>" + Helper::escape(tr("Status under the ECT Act 25 of 2002 (South Africa)")) + "</h3>\n<p>";
            if (signer.isSelfSigned)
            {
                html += Helper::escape(tr("The certificate is self-signed. This is an electronic signature (section 13(2) and (3)); it is NOT an advanced electronic signature."));
            }
            else
            {
                html += Helper::escape(tr("This is an advanced electronic signature (section 13(1), presumed valid under section 13(4)) only if the certificate was issued under a process accredited by the South African Accreditation Authority (sections 37 and 38). "
                                          "The issuer is \"%1\": compare it with the Authority's register of accredited authentication service providers. Otherwise it is an electronic signature (section 13(2) and (3)).").arg(signer.issuer));
            }
            html += "</p>\n";
        }
    }

    html += "<h2>" + Helper::escape(tr("How a third party confirms the signer and the certificate")) + "</h2>\n<ol>\n";
    for (const QString& step : {
             tr("Integrity: \"VALID\" above means that the signed bytes are exactly as signed. Bytes appended later are listed; only signatures, timestamps and validation data should be among them."),
             tr("Identity: the SHA-256 fingerprint of the signer's certificate above must equal the fingerprint printed in the visible signature stamp and in its QR code."),
             tr("Confirm that fingerprint and the serial number with the signer through another channel (for example by telephone), or with the issuer named above."),
             tr("Trust: when the chain ends in a trusted root, the issuer vouches for the identity in the certificate; a self-signed certificate proves only that the same key signed."),
             tr("Time: a trusted timestamp proves when the document was signed; the embedded revocation data shows that the certificate was not revoked at that time."),
             tr("Independent check: open the file in Adobe Acrobat Reader (free), Signature panel, Certificate Details - it shows the same subject, serial number and fingerprints.") })
    {
        html += "<li>" + Helper::escape(step) + "</li>\n";
    }
    html += "</ol>\n";

    html += "<h2>" + Helper::escape(tr("South African law (ECT Act 25 of 2002)")) + "</h2>\n<ul>\n";
    for (const QString& point : {
             tr("Where a law requires a signature and the parties have not agreed on a type, only an advanced electronic signature meets the requirement (section 13(1)). It must result from a process accredited by the South African Accreditation Authority (sections 1, 37 and 38), such as an accredited provider's certificate."),
             tr("Where the parties require an electronic signature without agreeing on its type, a method that identifies the person, indicates the person's approval and is as reliable as appropriate is sufficient (section 13(3)). An electronic signature is not without legal force only because it is electronic (section 13(2))."),
             tr("The Act does not give validity to the transactions of Schedule 2 (section 4(4)), among them agreements for the alienation of immovable property, long leases of immovable property (over 20 years), wills and codicils, and bills of exchange: these still need a signature on paper unless another law provides otherwise."),
             tr("This report states technical facts. It is not legal advice.") })
    {
        html += "<li>" + Helper::escape(point) + "</li>\n";
    }
    html += "</ul>\n</body></html>\n";
    return html;
}

}   // namespace pdf
