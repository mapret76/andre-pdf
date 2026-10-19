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

#include "pdflongtermvalidation.h"
#include "pdfdocumentsigner.h"
#include "pdfdocumentbuilder.h"
#include "pdfdocumentreader.h"
#include "pdfdocumentwriter.h"
#include "pdfstreamfilters.h"
#include "pdfform.h"

#include <QBuffer>
#include <QDateTime>
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QDeadlineTimer>
#include <QNetworkProxyFactory>
#include <QTimeZone>
#include <QUrl>

#include <openssl/cms.h>
#include <openssl/ocsp.h>
#include <openssl/pem.h>
#include <openssl/pkcs7.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include <ctime>
#include <memory>

#include "pdfdbgheap.h"

namespace pdf
{

class PDFLongTermValidationHelper
{
public:
    PDFLongTermValidationHelper() = delete;

    /// Certificates owned by the pool, unique by their DER encoding
    struct CertificatePool
    {
        ~CertificatePool()
        {
            for (X509* certificate : certificates)
            {
                X509_free(certificate);
            }
        }

        /// Adds a certificate (a reference is taken), returns true if it was new
        bool add(X509* certificate)
        {
            const QByteArray data = encode(certificate);
            if (data.isEmpty() || encoded.contains(data))
            {
                return false;
            }
            X509_up_ref(certificate);
            certificates.push_back(certificate);
            encoded.push_back(data);
            return true;
        }

        X509* findIssuer(X509* certificate) const
        {
            for (X509* candidate : certificates)
            {
                if (candidate != certificate && X509_check_issued(candidate, certificate) == X509_V_OK &&
                    X509_verify(certificate, X509_get0_pubkey(candidate)) == 1)
                {
                    return candidate;
                }
            }
            return nullptr;
        }

        std::vector<X509*> certificates;
        QByteArrayList encoded;
    };

    static QByteArray encode(X509* certificate)
    {
        unsigned char* buffer = nullptr;
        const int length = i2d_X509(certificate, &buffer);
        QByteArray result = length > 0 ? QByteArray(reinterpret_cast<const char*>(buffer), length) : QByteArray();
        OPENSSL_free(buffer);
        return result;
    }

    static QString getSubject(X509* certificate)
    {
        char* text = X509_NAME_oneline(X509_get_subject_name(certificate), nullptr, 0);
        const QString result = QString::fromUtf8(text);
        OPENSSL_free(text);
        return result;
    }

    static bool isSelfSigned(X509* certificate)
    {
        return (X509_get_extension_flags(certificate) & EXFLAG_SS) != 0;
    }

    /// Adds the certificates of a CMS signed data object, and of the timestamp
    /// tokens in the unsigned attributes of its signers
    static void addCertificates(const QByteArray& data, CertificatePool& pool, int depth = 0)
    {
        const unsigned char* pointer = reinterpret_cast<const unsigned char*>(data.constData());
        CMS_ContentInfo* cms = d2i_CMS_ContentInfo(nullptr, &pointer, data.size());
        if (!cms)
        {
            return;
        }

        if (STACK_OF(X509)* certificates = CMS_get1_certs(cms))
        {
            for (int i = 0; i < sk_X509_num(certificates); ++i)
            {
                pool.add(sk_X509_value(certificates, i));
            }
            sk_X509_pop_free(certificates, X509_free);
        }

        STACK_OF(CMS_SignerInfo)* signerInfos = CMS_get0_SignerInfos(cms);
        for (int i = 0; depth == 0 && i < sk_CMS_SignerInfo_num(signerInfos); ++i)
        {
            CMS_SignerInfo* signerInfo = sk_CMS_SignerInfo_value(signerInfos, i);
            int index = -1;
            while ((index = CMS_unsigned_get_attr_by_NID(signerInfo, NID_id_smime_aa_timeStampToken, index)) >= 0)
            {
                X509_ATTRIBUTE* attribute = CMS_unsigned_get_attr(signerInfo, index);
                ASN1_TYPE* value = attribute ? X509_ATTRIBUTE_get0_type(attribute, 0) : nullptr;
                if (value && value->type == V_ASN1_SEQUENCE)
                {
                    addCertificates(QByteArray(reinterpret_cast<const char*>(value->value.sequence->data), value->value.sequence->length), pool, depth + 1);
                }
            }
        }

        CMS_ContentInfo_free(cms);
    }

    /// Synchronous HTTP request (POST when \p body is not empty) with a size limit
    static QByteArray request(const QString& address, const QByteArray& body, const QByteArray& contentType, qint64 limit, int timeout, QString& errorMessage)
    {
        const QUrl url(address);
        if (!url.isValid() || (url.scheme() != QLatin1String("http") && url.scheme() != QLatin1String("https")) || url.host().isEmpty())
        {
            errorMessage = PDFLongTermValidation::tr("address %1 is not a web address").arg(address);
            return QByteArray();
        }

        QNetworkProxyFactory::setUseSystemConfiguration(true);
        QNetworkAccessManager manager;
        QNetworkRequest networkRequest(url);
        networkRequest.setTransferTimeout(timeout);
        networkRequest.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::SameOriginRedirectPolicy);
        networkRequest.setMaximumRedirectsAllowed(3);
        if (!contentType.isEmpty())
        {
            networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, contentType);
        }
        QNetworkReply* reply = body.isEmpty() ? manager.get(networkRequest) : manager.post(networkRequest, body);

        QByteArray data;
        bool isTooLarge = false;
        QObject::connect(reply, &QNetworkReply::readyRead, reply, [&]()
        {
            data.append(reply->readAll());
            if (data.size() > limit)
            {
                isTooLarge = true;
                reply->abort();
            }
        });

        QEventLoop loop;
        QTimer deadline;
        deadline.setSingleShot(true);
        QObject::connect(&deadline, &QTimer::timeout, reply, &QNetworkReply::abort);
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        deadline.start(timeout);
        if (!reply->isFinished())
        {
            loop.exec();
        }

        if (!isTooLarge)
        {
            data.append(reply->readAll());
        }

        const bool isOK = reply->error() == QNetworkReply::NoError && !isTooLarge && data.size() <= limit;
        if (!isOK)
        {
            errorMessage = isTooLarge ? PDFLongTermValidation::tr("the answer of %1 is too large").arg(address)
                                      : PDFLongTermValidation::tr("%1 cannot be reached (%2)").arg(address, reply->errorString());
            data.clear();
        }
        delete reply;
        return data;
    }

    /// Returns the URIs of the authority information access of the given method
    static QStringList getAccessUris(X509* certificate, int method)
    {
        QStringList result;
        AUTHORITY_INFO_ACCESS* access = static_cast<AUTHORITY_INFO_ACCESS*>(X509_get_ext_d2i(certificate, NID_info_access, nullptr, nullptr));
        for (int i = 0; i < sk_ACCESS_DESCRIPTION_num(access); ++i)
        {
            ACCESS_DESCRIPTION* description = sk_ACCESS_DESCRIPTION_value(access, i);
            if (OBJ_obj2nid(description->method) == method && description->location->type == GEN_URI)
            {
                const ASN1_IA5STRING* uri = description->location->d.uniformResourceIdentifier;
                result << QString::fromLatin1(reinterpret_cast<const char*>(ASN1_STRING_get0_data(uri)), ASN1_STRING_length(uri));
            }
        }
        AUTHORITY_INFO_ACCESS_free(access);
        return result;
    }

    static QStringList getCrlUris(X509* certificate)
    {
        QStringList result;
        CRL_DIST_POINTS* points = static_cast<CRL_DIST_POINTS*>(X509_get_ext_d2i(certificate, NID_crl_distribution_points, nullptr, nullptr));
        for (int i = 0; i < sk_DIST_POINT_num(points); ++i)
        {
            DIST_POINT* point = sk_DIST_POINT_value(points, i);
            if (!point->distpoint || point->distpoint->type != 0)
            {
                continue;
            }
            GENERAL_NAMES* names = point->distpoint->name.fullname;
            for (int j = 0; j < sk_GENERAL_NAME_num(names); ++j)
            {
                GENERAL_NAME* name = sk_GENERAL_NAME_value(names, j);
                if (name->type == GEN_URI)
                {
                    const ASN1_IA5STRING* uri = name->d.uniformResourceIdentifier;
                    result << QString::fromLatin1(reinterpret_cast<const char*>(ASN1_STRING_get0_data(uri)), ASN1_STRING_length(uri));
                }
            }
        }
        CRL_DIST_POINTS_free(points);
        return result;
    }

    /// Time left for one request: the limit of a request, within the overall budget
    static int getRemaining(const QDeadlineTimer& budget, int timeout)
    {
        return int(qMax<qint64>(1, qMin<qint64>(timeout, budget.remainingTime())));
    }

    /// Fetches the issuer of a certificate from its "CA issuers" address
    static X509* fetchIssuer(X509* certificate, const QDeadlineTimer& budget, int timeout)
    {
        for (const QString& uri : getAccessUris(certificate, NID_ad_ca_issuers))
        {
            QString errorMessage;
            const QByteArray data = request(uri, QByteArray(), QByteArray(), PDFLongTermValidation::MAXIMAL_CERTIFICATE_SIZE, getRemaining(budget, timeout), errorMessage);
            const unsigned char* pointer = reinterpret_cast<const unsigned char*>(data.constData());
            X509* issuer = d2i_X509(nullptr, &pointer, data.size());
            if (!issuer)
            {
                BIO* bio = BIO_new_mem_buf(data.constData(), int(data.size()));
                issuer = PEM_read_bio_X509(bio, nullptr, nullptr, nullptr);
                BIO_free(bio);
            }
            if (!issuer)
            {
                // A PKCS#7 "certs-only" answer (.p7c)
                const unsigned char* p7Pointer = reinterpret_cast<const unsigned char*>(data.constData());
                if (PKCS7* p7 = d2i_PKCS7(nullptr, &p7Pointer, data.size()))
                {
                    STACK_OF(X509)* certificates = PKCS7_type_is_signed(p7) ? p7->d.sign->cert : nullptr;
                    for (int i = 0; !issuer && i < sk_X509_num(certificates); ++i)
                    {
                        X509* candidate = sk_X509_value(certificates, i);
                        if (X509_check_issued(candidate, certificate) == X509_V_OK && X509_verify(certificate, X509_get0_pubkey(candidate)) == 1)
                        {
                            X509_up_ref(candidate);
                            issuer = candidate;
                        }
                    }
                    PKCS7_free(p7);
                }
            }
            if (issuer && X509_check_issued(issuer, certificate) == X509_V_OK && X509_verify(certificate, X509_get0_pubkey(issuer)) == 1)
            {
                return issuer;
            }
            X509_free(issuer);
        }
        return nullptr;
    }

    /// Revocation data older than this is not stored, seconds
    static constexpr long MAXIMAL_REVOCATION_AGE = 7 * 24 * 3600;

    static QDateTime toDateTime(const ASN1_TIME* time)
    {
        struct tm value = {};
        if (!time || ASN1_TIME_to_tm(time, &value) != 1)
        {
            return QDateTime();
        }
        return QDateTime(QDate(value.tm_year + 1900, value.tm_mon + 1, value.tm_mday), QTime(value.tm_hour, value.tm_min, value.tm_sec), QTimeZone::UTC);
    }

    enum class Status
    {
        Unknown,    ///< No usable answer
        Good,
        Revoked
    };

    /// Asks the OCSP responders of the certificate. The response must answer for
    /// this certificate, be current and signed by the issuer or by a responder the
    /// issuer authorised.
    static Status getOcspResponse(X509* certificate, X509* issuer, const QDeadlineTimer& budget, int timeout, QByteArray& response, CertificatePool& responders, QString& errorMessage)
    {
        for (const QString& uri : getAccessUris(certificate, NID_ad_OCSP))
        {
            OCSP_CERTID* id = OCSP_cert_to_id(EVP_sha1(), certificate, issuer);
            OCSP_REQUEST* ocspRequest = OCSP_REQUEST_new();
            if (!id || !ocspRequest || !OCSP_request_add0_id(ocspRequest, OCSP_CERTID_dup(id)))
            {
                OCSP_CERTID_free(id);
                OCSP_REQUEST_free(ocspRequest);
                return Status::Unknown;
            }

            unsigned char* buffer = nullptr;
            const int length = i2d_OCSP_REQUEST(ocspRequest, &buffer);
            const QByteArray requestData = length > 0 ? QByteArray(reinterpret_cast<const char*>(buffer), length) : QByteArray();
            OPENSSL_free(buffer);
            OCSP_REQUEST_free(ocspRequest);

            const QByteArray data = request(uri, requestData, "application/ocsp-request", PDFLongTermValidation::MAXIMAL_OCSP_SIZE, getRemaining(budget, timeout), errorMessage);
            const unsigned char* pointer = reinterpret_cast<const unsigned char*>(data.constData());
            OCSP_RESPONSE* ocspResponse = data.isEmpty() ? nullptr : d2i_OCSP_RESPONSE(nullptr, &pointer, data.size());
            OCSP_BASICRESP* basic = (ocspResponse && OCSP_response_status(ocspResponse) == OCSP_RESPONSE_STATUS_SUCCESSFUL) ? OCSP_response_get1_basic(ocspResponse) : nullptr;

            Status status = Status::Unknown;
            int certificateStatus = -1;
            int reason = -1;
            ASN1_GENERALIZEDTIME* revocationTime = nullptr;
            ASN1_GENERALIZEDTIME* thisUpdate = nullptr;
            ASN1_GENERALIZEDTIME* nextUpdate = nullptr;
            if (basic && OCSP_resp_find_status(basic, id, &certificateStatus, &reason, &revocationTime, &thisUpdate, &nextUpdate) == 1 &&
                OCSP_check_validity(thisUpdate, nextUpdate, 300, nextUpdate ? -1 : MAXIMAL_REVOCATION_AGE) == 1)
            {
                X509_STORE* store = X509_STORE_new();
                STACK_OF(X509)* issuers = sk_X509_new_null();
                X509_STORE_add_cert(store, issuer);
                X509_STORE_set_flags(store, X509_V_FLAG_PARTIAL_CHAIN);
                sk_X509_push(issuers, issuer);
                if (OCSP_basic_verify(basic, issuers, store, 0) > 0)
                {
                    if (certificateStatus == V_OCSP_CERTSTATUS_GOOD)
                    {
                        status = Status::Good;
                    }
                    else if (certificateStatus == V_OCSP_CERTSTATUS_REVOKED)
                    {
                        status = Status::Revoked;
                    }

                    const STACK_OF(X509)* responderCertificates = OCSP_resp_get0_certs(basic);
                    for (int i = 0; i < sk_X509_num(responderCertificates); ++i)
                    {
                        responders.add(sk_X509_value(responderCertificates, i));
                    }
                }
                else
                {
                    errorMessage = PDFLongTermValidation::tr("the OCSP answer of %1 is not signed by the issuer").arg(uri);
                }
                sk_X509_free(issuers);
                X509_STORE_free(store);
            }
            else if (errorMessage.isEmpty())
            {
                errorMessage = PDFLongTermValidation::tr("%1 gave no current OCSP answer").arg(uri);
            }

            OCSP_BASICRESP_free(basic);
            OCSP_RESPONSE_free(ocspResponse);
            OCSP_CERTID_free(id);

            if (status != Status::Unknown)
            {
                response = data;
                return status;
            }
        }
        return Status::Unknown;
    }

    /// Downloads the CRL of the certificate; it must be signed by the issuer
    static Status getCrl(X509* certificate, X509* issuer, const QDeadlineTimer& budget, int timeout, QByteArray& crlData, QString& errorMessage)
    {
        for (const QString& uri : getCrlUris(certificate))
        {
            const QByteArray data = request(uri, QByteArray(), QByteArray(), PDFLongTermValidation::MAXIMAL_CRL_SIZE, getRemaining(budget, timeout), errorMessage);
            const unsigned char* pointer = reinterpret_cast<const unsigned char*>(data.constData());
            X509_CRL* crl = data.isEmpty() ? nullptr : d2i_X509_CRL(nullptr, &pointer, data.size());
            QByteArray der = data;
            if (!crl && !data.isEmpty())
            {
                BIO* bio = BIO_new_mem_buf(data.constData(), int(data.size()));
                crl = PEM_read_bio_X509_CRL(bio, nullptr, nullptr, nullptr);
                BIO_free(bio);
                unsigned char* buffer = nullptr;
                const int length = crl ? i2d_X509_CRL(crl, &buffer) : 0;
                der = length > 0 ? QByteArray(reinterpret_cast<const char*>(buffer), length) : QByteArray();
                OPENSSL_free(buffer);
            }

            Status status = Status::Unknown;
            // A CRL must be current: issued in the past, its next update still to come (RFC 5280)
            const ASN1_TIME* nextUpdate = crl ? X509_CRL_get0_nextUpdate(crl) : nullptr;
            const ASN1_TIME* lastUpdate = crl ? X509_CRL_get0_lastUpdate(crl) : nullptr;
            // five minutes of clock difference allowed, as for OCSP answers
            time_t earlier = time(nullptr) - 300;
            time_t later = time(nullptr) + 300;
            const bool isCurrent = nextUpdate && lastUpdate && X509_cmp_time(nextUpdate, &earlier) > 0 && X509_cmp_time(lastUpdate, &later) < 0;
            if (crl && !isCurrent)
            {
                errorMessage = PDFLongTermValidation::tr("the CRL of %1 is out of date").arg(uri);
            }
            else if (crl && X509_NAME_cmp(X509_CRL_get_issuer(crl), X509_get_subject_name(issuer)) == 0 &&
                     X509_CRL_verify(crl, X509_get0_pubkey(issuer)) == 1)
            {
                X509_REVOKED* revoked = nullptr;
                status = X509_CRL_get0_by_cert(crl, &revoked, certificate) == 1 ? Status::Revoked : Status::Good;
            }
            else if (crl)
            {
                errorMessage = PDFLongTermValidation::tr("the CRL of %1 is not signed by the issuer").arg(uri);
            }
            X509_CRL_free(crl);

            if (status != Status::Unknown && !der.isEmpty())
            {
                crlData = der;
                return status;
            }
        }
        return Status::Unknown;
    }

    static PDFDocument read(const QByteArray& data, const QString& password, bool& isOK)
    {
        bool passwordOffered = false;
        auto getPassword = [&password, &passwordOffered](bool* ok)
        {
            *ok = !passwordOffered;
            passwordOffered = true;
            return password;
        };
        PDFDocumentReader reader(nullptr, getPassword, false, false);
        PDFDocument document = reader.readFromBuffer(data);
        isOK = reader.getReadingResult() == PDFDocumentReader::Result::OK;
        return document;
    }
};

PDFLongTermValidation::Result PDFLongTermValidation::addValidationData(const QByteArray& signedDocument, const QString& password, const QString& signatureFieldName, int timeoutMilliseconds)
{
    Result result;

    bool isRead = false;
    const PDFDocument document = PDFLongTermValidationHelper::read(signedDocument, password, isRead);
    if (!isRead)
    {
        result.error = tr("The signed document cannot be read.");
        return result;
    }

    // Only the newest signature (the one just made) and its timestamp are walked: their
    // certificates are the signer's own choice. The addresses in the certificates of
    // other parties' earlier signatures are never contacted.
    PDFLongTermValidationHelper::CertificatePool pool;
    const PDFForm form = PDFForm::parse(&document, document.getCatalog()->getFormObject());
    const PDFFormFieldSignature* newest = nullptr;
    form.apply([&newest, &signatureFieldName](const PDFFormField* field)
    {
        const PDFFormFieldSignature* signatureField = dynamic_cast<const PDFFormFieldSignature*>(field);
        if (signatureField && field->getName(PDFFormField::NameType::FullyQualified) == signatureFieldName)
        {
            newest = signatureField;
        }
    });
    if (newest)
    {
        PDFLongTermValidationHelper::addCertificates(newest->getSignature().getContents(), pool);
    }

    if (pool.certificates.empty())
    {
        result.error = tr("The document has no signature with certificates.");
        return result;
    }

    // What the store already holds is kept and not added again
    const PDFDocumentSecurityStore::SecurityStoreItem* existing = document.getCatalog()->getDocumentSecurityStore().getMasterItem();
    QByteArrayList newCertificates;
    QByteArrayList newOcsps;
    QByteArrayList newCrls;

    // Certificates already stored may complete the chain; the responders' certificates
    // are stored, not walked
    PDFLongTermValidationHelper::CertificatePool stored;
    for (const QByteArray& data : existing->Cert)
    {
        const unsigned char* pointer = reinterpret_cast<const unsigned char*>(data.constData());
        if (X509* certificate = d2i_X509(nullptr, &pointer, data.size()))
        {
            stored.add(certificate);
            X509_free(certificate);
        }
    }
    PDFLongTermValidationHelper::CertificatePool responders;

    // All requests together get one minute, at most 32 certificates are walked
    QDeadlineTimer budget(60000);

    // The pool grows while it is walked (issuers), so it is walked by index
    for (size_t i = 0; i < pool.certificates.size() && i < 32; ++i)
    {
        X509* certificate = pool.certificates[i];
        if (PDFLongTermValidationHelper::isSelfSigned(certificate))
        {
            continue;
        }
        if (budget.hasExpired())
        {
            result.warnings << tr("%1: not checked, the time for the revocation services is over.").arg(PDFLongTermValidationHelper::getSubject(certificate));
            continue;
        }

        X509* issuer = pool.findIssuer(certificate);
        if (!issuer)
        {
            if (X509* storedIssuer = stored.findIssuer(certificate))
            {
                pool.add(storedIssuer);
                issuer = pool.findIssuer(certificate);
            }
        }
        if (!issuer)
        {
            if (X509* fetchedIssuer = PDFLongTermValidationHelper::fetchIssuer(certificate, budget, timeoutMilliseconds))
            {
                pool.add(fetchedIssuer);
                X509_free(fetchedIssuer);
                issuer = pool.findIssuer(certificate);
            }
        }

        const QString subject = PDFLongTermValidationHelper::getSubject(certificate);
        if (!issuer)
        {
            result.warnings << tr("%1: the issuing certificate was not found.").arg(subject);
            continue;
        }

        // A responder certificate marked "no check" needs no revocation data (RFC 6960, 4.2.2.2.1)
        if (X509_get_ext_by_NID(certificate, NID_id_pkix_OCSP_noCheck, -1) >= 0)
        {
            continue;
        }

        QString errorMessage;
        QByteArray data;
        PDFLongTermValidationHelper::Status status = PDFLongTermValidationHelper::getOcspResponse(certificate, issuer, budget, timeoutMilliseconds, data, responders, errorMessage);
        if (status != PDFLongTermValidationHelper::Status::Unknown)
        {
            if (std::find(existing->OCSP.cbegin(), existing->OCSP.cend(), data) == existing->OCSP.cend() && !newOcsps.contains(data))
            {
                newOcsps << data;
            }
        }

        // The CRL as well when there is one: verifiers that check CRLs only (the
        // signature panel of this program) find the status, too
        QString crlErrorMessage;
        QByteArray crlData;
        const PDFLongTermValidationHelper::Status crlStatus = budget.hasExpired() ? PDFLongTermValidationHelper::Status::Unknown
                                                                                  : PDFLongTermValidationHelper::getCrl(certificate, issuer, budget, timeoutMilliseconds, crlData, crlErrorMessage);
        if (crlStatus != PDFLongTermValidationHelper::Status::Unknown &&
            std::find(existing->CRL.cbegin(), existing->CRL.cend(), crlData) == existing->CRL.cend() && !newCrls.contains(crlData))
        {
            newCrls << crlData;
        }
        if (status == PDFLongTermValidationHelper::Status::Unknown || crlStatus == PDFLongTermValidationHelper::Status::Revoked)
        {
            status = crlStatus;
            if (errorMessage.isEmpty())
            {
                errorMessage = crlErrorMessage;
            }
        }

        if (status == PDFLongTermValidationHelper::Status::Unknown)
        {
            result.warnings << (errorMessage.isEmpty() ? tr("%1: the certificate names no revocation service.").arg(subject)
                                                       : tr("%1: no revocation data, %2.").arg(subject, errorMessage));
        }
        else if (status == PDFLongTermValidationHelper::Status::Revoked)
        {
            result.revokedCertificates << subject;
        }
    }

    if (pool.certificates.size() > 32)
    {
        result.warnings << tr("%1 certificates were not checked (at most 32 are).").arg(pool.certificates.size() - 32);
    }

    QByteArrayList certificates = pool.encoded;
    certificates << responders.encoded;
    for (const QByteArray& certificate : certificates)
    {
        if (std::find(existing->Cert.cbegin(), existing->Cert.cend(), certificate) == existing->Cert.cend() && !newCertificates.contains(certificate))
        {
            newCertificates << certificate;
        }
    }

    result.certificateCount = int(newCertificates.size());
    result.ocspCount = int(newOcsps.size());
    result.crlCount = int(newCrls.size());

    // The new /DSS keeps the references of the old one and adds the new streams
    PDFDocumentBuilder builder(&document);
    PDFDocumentDataLoaderDecorator loader(&document);
    const PDFObject catalogObject = document.getObjectByReference(builder.getCatalogReference());
    const PDFDictionary* catalogDictionary = document.getDictionaryFromObject(catalogObject);
    const PDFDictionary* dssDictionary = catalogDictionary ? document.getDictionaryFromObject(catalogDictionary->get("DSS")) : nullptr;

    auto createArray = [&](const char* key, const QByteArrayList& items) -> PDFObject
    {
        PDFArrayBuilder array;
        if (dssDictionary)
        {
            for (const PDFObjectReference& reference : loader.readReferenceArray(dssDictionary->get(key)))
            {
                array.appendItem(PDFObject::createReference(reference));
            }
        }
        for (const QByteArray& item : items)
        {
            QByteArray compressed = PDFFlateDecodeFilter::compress(item);
            PDFDictionaryBuilder streamDictionary;
            streamDictionary.addEntry(PDFInplaceOrMemoryString("Length"), PDFObject::createInteger(compressed.size()));
            streamDictionary.addEntry(PDFInplaceOrMemoryString("Filter"), PDFObject::createName("FlateDecode"));
            array.appendItem(PDFObject::createReference(builder.addObject(PDFObject::createStream(PDFStream(std::move(streamDictionary), std::move(compressed))))));
        }
        return PDFObject::createArray(std::move(array));
    };

    PDFDictionaryBuilder dss;
    if (dssDictionary)
    {
        dss = PDFDictionaryBuilder(*dssDictionary);
    }
    dss.setEntry(PDFInplaceOrMemoryString("Type"), PDFObject::createName("DSS"));
    dss.setEntry(PDFInplaceOrMemoryString("Certs"), createArray("Certs", newCertificates));
    dss.setEntry(PDFInplaceOrMemoryString("OCSPs"), createArray("OCSPs", newOcsps));
    dss.setEntry(PDFInplaceOrMemoryString("CRLs"), createArray("CRLs", newCrls));
    const PDFObjectReference dssReference = builder.addObject(PDFObject::createDictionary(std::move(dss)));

    PDFObjectFactory catalogUpdate;
    catalogUpdate.beginDictionary();
    catalogUpdate.beginDictionaryItem("DSS");
    catalogUpdate << dssReference;
    catalogUpdate.endDictionaryItem();
    catalogUpdate.endDictionary();
    builder.mergeTo(builder.getCatalogReference(), catalogUpdate.takeObject());

    const PDFDocument extendedDocument = builder.build();
    QBuffer buffer;
    buffer.open(QBuffer::WriteOnly);
    PDFDocumentWriter writer(nullptr);
    if (!writer.writeIncrementalUpdate(&buffer, signedDocument, &extendedDocument, password))
    {
        result.error = tr("The validation data cannot be written.");
        return result;
    }

    // The update is appended to the signed bytes, which stay untouched
    if (!buffer.data().startsWith(signedDocument))
    {
        result.error = tr("The validation data cannot be appended to the signed document.");
        return result;
    }

    result.document = buffer.data();
    return result;
}

bool PDFLongTermValidation::addDocumentTimestamp(const QByteArray& document,
                                                 const QString& password,
                                                 const PDFSignatureFactory::TimestampSettings& settings,
                                                 QByteArray& result,
                                                 QString& errorMessage,
                                                 QString* fieldName)
{
    errorMessage.clear();

    bool isRead = false;
    const PDFDocument parsedDocument = PDFLongTermValidationHelper::read(document, password, isRead);
    if (!isRead || parsedDocument.getCatalog()->getPageCount() == 0)
    {
        errorMessage = tr("The document cannot be read.");
        return false;
    }

    const QString timestampFieldName = QString("pdf4qt_timestamp_%1").arg(QDateTime::currentMSecsSinceEpoch());
    const PDFObjectReference pageReference = parsedDocument.getCatalog()->getPage(0)->getPageReference();

    PDFDocumentSigner::Parameters parameters;
    parameters.document = &parsedDocument;
    parameters.originalDocumentData = document;
    parameters.openPassword = password;
    parameters.subfilter = "ETSI.RFC3161";
    parameters.signatureDictionaryType = "DocTimeStamp";
    parameters.signFunction = [&settings, &errorMessage](const QByteArray& data, QByteArray& token)
    {
        return PDFSignatureFactory::createTimestampToken(data, settings, token, errorMessage);
    };
    parameters.createSignatureFieldFunction = [&](PDFDocumentBuilder& builder, PDFObjectReference signatureDictionary)
    {
        return builder.createSignatureField(timestampFieldName, signatureDictionary, pageReference);
    };

    const PDFDocumentSigner::Result signingResult = PDFDocumentSigner::sign(parameters, result);
    if (signingResult != PDFDocumentSigner::Result::OK)
    {
        if (errorMessage.isEmpty())
        {
            errorMessage = PDFDocumentSigner::getResultMessage(signingResult);
        }
        return false;
    }
    if (fieldName)
    {
        *fieldName = timestampFieldName;
    }
    return true;
}

PDFLongTermValidation::Extension PDFLongTermValidation::extend(const QByteArray& signedDocument, const QString& password, const QString& signatureFieldName, const PDFSignatureFactory::TimestampSettings& settings)
{
    Extension extension;
    extension.validation = addValidationData(signedDocument, password, signatureFieldName, settings.timeoutMilliseconds);
    extension.document = extension.validation.isOK() ? extension.validation.document : signedDocument;

    QByteArray timestamped;
    if (addDocumentTimestamp(extension.document, password, settings, timestamped, extension.timestampError, &extension.timestampFieldName))
    {
        extension.document = timestamped;
    }
    return extension;
}

QStringList PDFLongTermValidation::Extension::getReport() const
{
    QStringList lines;
    if (validation.isOK())
    {
        lines << tr("- long-term validation data embedded: %1 certificates, %2 OCSP responses, %3 CRLs")
                     .arg(validation.certificateCount).arg(validation.ocspCount).arg(validation.crlCount);
    }
    else
    {
        lines << tr("- long-term validation data NOT embedded: %1").arg(validation.error);
    }
    for (const QString& subject : validation.revokedCertificates)
    {
        lines << tr("- WARNING: the certificate %1 is reported as REVOKED").arg(subject);
    }
    for (const QString& warning : validation.warnings)
    {
        lines << tr("- %1").arg(warning);
    }
    lines << (timestampFieldName.isEmpty() ? tr("- document timestamp NOT added: %1").arg(timestampError)
                                           : tr("- document timestamp added over the signature and its validation data"));
    return lines;
}

PDFDocumentSigner::SignedFileCheck PDFLongTermValidation::checkExtendedFile(const QByteArray& file,
                                                                            const QByteArray& signedDocument,
                                                                            const QString& signatureFieldName,
                                                                            const QString& timestampFieldName,
                                                                            const QString& password)
{
    PDFDocumentSigner::SignedFileCheck check = PDFDocumentSigner::checkSignedFile(signedDocument, signatureFieldName, password);
    if (!file.startsWith(signedDocument))
    {
        check.isCoveringWholeFile = false;
        check.errors << tr("The saved file does not begin with the signed document.");
        return check;
    }

    if (!timestampFieldName.isEmpty())
    {
        const PDFDocumentSigner::SignedFileCheck timestampCheck = PDFDocumentSigner::checkSignedFile(file, timestampFieldName, password);
        if (!timestampCheck.isOK())
        {
            check.isCoveringWholeFile = false;
            check.errors << tr("The document timestamp of the saved file is not valid.") << timestampCheck.errors;
        }
    }
    else
    {
        bool isRead = false;
        PDFLongTermValidationHelper::read(file, password, isRead);
        check.isOpened = check.isOpened && isRead;
    }
    return check;
}

}   // namespace pdf
