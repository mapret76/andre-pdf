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

#include "pdfcertificatemanager.h"
#include "pdfcertificatestore.h"
#include "pdfdocument.h"
#include "pdfdocumentbuilder.h"
#include "pdfdocumentreader.h"
#include "pdfdocumentsigner.h"
#include "pdfform.h"
#include "pdflongtermvalidation.h"
#include "pdfsignaturereport.h"
#include "pdfsignaturehandler.h"

#include <QtTest>
#include <QTcpServer>
#include <QTcpSocket>

#include <openssl/cms.h>
#include <openssl/evp.h>
#include <openssl/ocsp.h>
#include <openssl/ess.h>
#include <openssl/pkcs12.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include <memory>

using namespace pdf;

namespace longtermvalidationtest
{

QByteArray toDer(X509* certificate)
{
    unsigned char* buffer = nullptr;
    const int size = i2d_X509(certificate, &buffer);
    QByteArray result = size > 0 ? QByteArray(reinterpret_cast<const char*>(buffer), size) : QByteArray();
    OPENSSL_free(buffer);
    return result;
}

/// Test PKI on the loopback interface: a root CA, its OCSP responder, its CRL and
/// its certificate (CA issuers), served over HTTP
class TestCertificationAuthority
{
public:
    enum class OcspBehaviour
    {
        Answer,         ///< Signed by the CA
        Unavailable,    ///< HTTP 500
        ForeignSigner   ///< Signed by a key the CA never authorised
    };

    ~TestCertificationAuthority()
    {
        for (X509* certificate : m_issued)
        {
            X509_free(certificate);
        }
        X509_free(m_root);
        EVP_PKEY_free(m_rootKey);
        EVP_PKEY_free(m_foreignKey);
        EVP_PKEY_free(m_userKey);
    }

    bool start()
    {
        if (!m_server.listen(QHostAddress::LocalHost))
        {
            return false;
        }
        QObject::connect(&m_server, &QTcpServer::newConnection, &m_server, [this]() { onConnection(); });

        m_rootKey = EVP_RSA_gen(2048);
        m_foreignKey = EVP_RSA_gen(2048);
        m_userKey = EVP_RSA_gen(2048);
        m_root = createCertificate("Andre PDF test root CA", 1, m_rootKey, nullptr, true, true, true, true);
        return m_rootKey && m_foreignKey && m_userKey && m_root;
    }

    QString url(const char* path) const { return QString("http://127.0.0.1:%1/%2").arg(m_server.serverPort()).arg(path); }

    /// Issues a signing certificate for the user key
    X509* issue(const char* commonName, long serial, bool withOcsp, bool withCrl)
    {
        X509* certificate = createCertificate(commonName, serial, m_userKey, m_root, false, withOcsp, withCrl, true);
        m_issued.push_back(certificate);
        return certificate;
    }

    /// PKCS#12 of the user key and the certificate, with or without the root
    QByteArray createPkcs12(X509* certificate, bool withRoot) const
    {
        STACK_OF(X509)* chain = sk_X509_new_null();
        if (withRoot)
        {
            sk_X509_push(chain, m_root);
        }
        PKCS12* pkcs12 = PKCS12_create("test", "test", m_userKey, certificate, chain, 0, 0, 0, 0, 0);
        sk_X509_free(chain);

        unsigned char* buffer = nullptr;
        const int size = pkcs12 ? i2d_PKCS12(pkcs12, &buffer) : 0;
        QByteArray result = size > 0 ? QByteArray(reinterpret_cast<const char*>(buffer), size) : QByteArray();
        OPENSSL_free(buffer);
        PKCS12_free(pkcs12);
        return result;
    }

    QByteArray getRoot() const { return toDer(m_root); }

    /// CAdES signature whose signing-certificate-v2 attribute names the whole chain
    /// (signer and root, RFC 5035)
    QByteArray signWithChainAttribute(X509* certificate, const QByteArray& data) const
    {
        BIO* input = BIO_new_mem_buf(data.constData(), int(data.size()));
        CMS_ContentInfo* cms = CMS_sign(nullptr, nullptr, nullptr, input, CMS_DETACHED | CMS_BINARY | CMS_PARTIAL);
        CMS_SignerInfo* signerInfo = CMS_add1_signer(cms, certificate, m_userKey, EVP_sha256(), CMS_BINARY | CMS_PARTIAL | CMS_NOSMIMECAP);
        STACK_OF(X509)* chain = sk_X509_new_null();
        sk_X509_push(chain, m_root);
        ESS_SIGNING_CERT_V2* attribute = OSSL_ESS_signing_cert_v2_new_init(EVP_sha256(), certificate, chain, 1);
        unsigned char* der = nullptr;
        const int derLength = i2d_ESS_SIGNING_CERT_V2(attribute, &der);
        CMS_signed_add1_attr_by_NID(signerInfo, NID_id_smime_aa_signingCertificateV2, V_ASN1_SEQUENCE, der, derLength);
        OPENSSL_free(der);
        ESS_SIGNING_CERT_V2_free(attribute);
        sk_X509_free(chain);
        CMS_add1_cert(cms, m_root);
        CMS_final(cms, input, nullptr, CMS_DETACHED | CMS_BINARY);
        BIO_free(input);

        unsigned char* buffer = nullptr;
        const int size = i2d_CMS_ContentInfo(cms, &buffer);
        QByteArray result(reinterpret_cast<const char*>(buffer), size);
        OPENSSL_free(buffer);
        CMS_ContentInfo_free(cms);
        return result;
    }

    /// CAdES signature of the data whose certificate set holds a decoy first: a copy of the
    /// signer's certificate (same issuer, serial and key) with another name, signed by a
    /// foreign key. With \p withSigner false the real certificate is left out.
    QByteArray signWithDecoy(X509* certificate, const QByteArray& data, bool withSigner) const
    {
        X509* decoy = X509_dup(certificate);
        X509_NAME* name = X509_NAME_new();
        X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, reinterpret_cast<const unsigned char*>("Decoy Person"), -1, -1, 0);
        X509_set_subject_name(decoy, name);
        X509_NAME_free(name);
        X509_sign(decoy, m_foreignKey, EVP_sha256());

        BIO* input = BIO_new_mem_buf(data.constData(), int(data.size()));
        CMS_ContentInfo* cms = CMS_sign(certificate, m_userKey, nullptr, input, CMS_DETACHED | CMS_BINARY | CMS_NOSMIMECAP | CMS_NOCERTS | CMS_CADES);
        BIO_free(input);
        CMS_add1_cert(cms, decoy);
        if (withSigner)
        {
            CMS_add1_cert(cms, certificate);
        }
        unsigned char* buffer = nullptr;
        const int size = i2d_CMS_ContentInfo(cms, &buffer);
        QByteArray result(reinterpret_cast<const char*>(buffer), size);
        OPENSSL_free(buffer);
        CMS_ContentInfo_free(cms);
        X509_free(decoy);
        return result;
    }

    OcspBehaviour ocspBehaviour = OcspBehaviour::Answer;
    bool isCrlAvailable = true;
    bool isStale = false;   ///< OCSP answers 30 days old without a next update, CRLs expired 30 days ago
    long revokedSerial = -1;
    int requestCount = 0;

private:
    X509* createCertificate(const char* commonName, long serial, EVP_PKEY* key, X509* issuer, bool isCA, bool withOcsp, bool withCrl, bool withIssuers)
    {
        X509* certificate = X509_new();
        X509_set_version(certificate, 2);
        ASN1_INTEGER_set(X509_get_serialNumber(certificate), serial);
        X509_gmtime_adj(X509_getm_notBefore(certificate), -3600);
        X509_gmtime_adj(X509_getm_notAfter(certificate), 365 * 24 * 3600);
        X509_set_pubkey(certificate, key);
        X509_NAME* name = X509_get_subject_name(certificate);
        X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, reinterpret_cast<const unsigned char*>(commonName), -1, -1, 0);
        X509_set_issuer_name(certificate, issuer ? X509_get_subject_name(issuer) : name);

        X509V3_CTX context;
        X509V3_set_ctx_nodb(&context);
        X509V3_set_ctx(&context, issuer ? issuer : certificate, certificate, nullptr, nullptr, 0);

        std::vector<std::pair<int, QByteArray>> extensions;
        extensions.emplace_back(NID_basic_constraints, isCA ? "critical,CA:TRUE" : "critical,CA:FALSE");
        extensions.emplace_back(NID_key_usage, isCA ? "critical,keyCertSign,cRLSign,digitalSignature" : "critical,digitalSignature,nonRepudiation");
        if (!isCA)
        {
            QByteArray access;
            if (withOcsp)
            {
                access += "OCSP;URI:" + url("ocsp").toLatin1();
            }
            if (withIssuers)
            {
                access += (access.isEmpty() ? "" : ",") + QByteArray("caIssuers;URI:") + url("ca.cer").toLatin1();
            }
            if (!access.isEmpty())
            {
                extensions.emplace_back(NID_info_access, access);
            }
            if (withCrl)
            {
                extensions.emplace_back(NID_crl_distribution_points, "URI:" + url("ca.crl").toLatin1());
            }
        }

        for (const auto& extension : extensions)
        {
            X509_EXTENSION* object = X509V3_EXT_conf_nid(nullptr, &context, extension.first, extension.second.constData());
            X509_add_ext(certificate, object, -1);
            X509_EXTENSION_free(object);
        }

        X509_sign(certificate, issuer ? m_rootKey : key, EVP_sha256());
        return certificate;
    }

    QByteArray createCrl() const
    {
        X509_CRL* crl = X509_CRL_new();
        X509_CRL_set_version(crl, 1);
        X509_CRL_set_issuer_name(crl, X509_get_subject_name(m_root));
        ASN1_TIME* now = X509_gmtime_adj(nullptr, isStale ? -60 * 24 * 3600 : 0);
        ASN1_TIME* next = X509_gmtime_adj(nullptr, isStale ? -30 * 24 * 3600 : 7 * 24 * 3600);
        X509_CRL_set1_lastUpdate(crl, now);
        X509_CRL_set1_nextUpdate(crl, next);
        if (revokedSerial > 0)
        {
            X509_REVOKED* revoked = X509_REVOKED_new();
            ASN1_INTEGER* serial = ASN1_INTEGER_new();
            ASN1_INTEGER_set(serial, revokedSerial);
            X509_REVOKED_set_serialNumber(revoked, serial);
            X509_REVOKED_set_revocationDate(revoked, now);
            X509_CRL_add0_revoked(crl, revoked);
            ASN1_INTEGER_free(serial);
        }
        X509_CRL_sort(crl);
        X509_CRL_sign(crl, m_rootKey, EVP_sha256());
        ASN1_TIME_free(now);
        ASN1_TIME_free(next);

        unsigned char* buffer = nullptr;
        const int size = i2d_X509_CRL(crl, &buffer);
        QByteArray result(reinterpret_cast<const char*>(buffer), size);
        OPENSSL_free(buffer);
        X509_CRL_free(crl);
        return result;
    }

    QByteArray createOcspResponse(const QByteArray& requestData) const
    {
        const unsigned char* pointer = reinterpret_cast<const unsigned char*>(requestData.constData());
        OCSP_REQUEST* request = d2i_OCSP_REQUEST(nullptr, &pointer, requestData.size());
        if (!request)
        {
            return QByteArray();
        }

        OCSP_BASICRESP* basic = OCSP_BASICRESP_new();
        ASN1_TIME* now = X509_gmtime_adj(nullptr, isStale ? -30 * 24 * 3600 : 0);
        ASN1_TIME* next = isStale ? nullptr : X509_gmtime_adj(nullptr, 24 * 3600);
        for (int i = 0; i < OCSP_request_onereq_count(request); ++i)
        {
            OCSP_CERTID* id = OCSP_onereq_get0_id(OCSP_request_onereq_get0(request, i));
            ASN1_INTEGER* serial = nullptr;
            OCSP_id_get0_info(nullptr, nullptr, nullptr, &serial, id);
            const bool isRevoked = serial && ASN1_INTEGER_get(serial) == revokedSerial;
            OCSP_basic_add1_status(basic, id, isRevoked ? V_OCSP_CERTSTATUS_REVOKED : V_OCSP_CERTSTATUS_GOOD,
                                   isRevoked ? OCSP_REVOKED_STATUS_KEYCOMPROMISE : -1, isRevoked ? now : nullptr, now, next);
        }

        const bool isForeign = ocspBehaviour == OcspBehaviour::ForeignSigner;
        X509* signer = m_root;
        X509* foreign = nullptr;
        if (isForeign)
        {
            // Same name as the root, other key: only the signature tells them apart
            foreign = X509_dup(m_root);
            X509_set_pubkey(foreign, m_foreignKey);
            X509_sign(foreign, m_foreignKey, EVP_sha256());
            signer = foreign;
        }
        OCSP_basic_sign(basic, signer, isForeign ? m_foreignKey : m_rootKey, EVP_sha256(), nullptr, 0);
        OCSP_RESPONSE* response = OCSP_response_create(OCSP_RESPONSE_STATUS_SUCCESSFUL, basic);

        unsigned char* buffer = nullptr;
        const int size = i2d_OCSP_RESPONSE(response, &buffer);
        QByteArray result(reinterpret_cast<const char*>(buffer), size);
        OPENSSL_free(buffer);
        OCSP_RESPONSE_free(response);
        OCSP_BASICRESP_free(basic);
        OCSP_REQUEST_free(request);
        X509_free(foreign);
        ASN1_TIME_free(now);
        ASN1_TIME_free(next);
        return result;
    }

    void onConnection()
    {
        while (QTcpSocket* socket = m_server.nextPendingConnection())
        {
            auto buffer = std::make_shared<QByteArray>();
            QObject::connect(socket, &QTcpSocket::readyRead, socket, [this, socket, buffer]()
            {
                buffer->append(socket->readAll());
                const qsizetype headerEnd = buffer->indexOf("\r\n\r\n");
                if (headerEnd == -1)
                {
                    return;
                }
                const QByteArray header = buffer->left(headerEnd).toLower();
                qsizetype contentLength = 0;
                const qsizetype lengthIndex = header.indexOf("content-length:");
                if (lengthIndex != -1)
                {
                    qsizetype endIndex = header.indexOf('\r', lengthIndex);
                    contentLength = header.mid(lengthIndex + 15, (endIndex == -1 ? header.size() : endIndex) - lengthIndex - 15).trimmed().toLongLong();
                }
                if (buffer->size() < headerEnd + 4 + contentLength)
                {
                    return;
                }
                const QByteArray path = header.split(' ').value(1);
                answer(socket, path, buffer->mid(headerEnd + 4, contentLength));
                buffer->clear();
            });
            QObject::connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        }
    }

    void answer(QTcpSocket* socket, const QByteArray& path, const QByteArray& body)
    {
        ++requestCount;
        QByteArray data;
        QByteArray status = "200 OK";
        if (path == "/ocsp" && ocspBehaviour != OcspBehaviour::Unavailable)
        {
            data = createOcspResponse(body);
        }
        else if (path == "/ca.crl" && isCrlAvailable)
        {
            data = createCrl();
        }
        else if (path == "/ca.cer")
        {
            data = toDer(m_root);
        }
        else
        {
            status = "500 Internal Server Error";
            data = "unavailable";
        }

        socket->write("HTTP/1.1 " + status + "\r\nContent-Type: application/octet-stream\r\nContent-Length: " +
                      QByteArray::number(data.size()) + "\r\nConnection: close\r\n\r\n" + data);
        socket->flush();
        socket->disconnectFromHost();
    }

    QTcpServer m_server;
    EVP_PKEY* m_rootKey = nullptr;
    EVP_PKEY* m_foreignKey = nullptr;
    EVP_PKEY* m_userKey = nullptr;
    X509* m_root = nullptr;
    std::vector<X509*> m_issued;
};

}   // namespace longtermvalidationtest

using namespace longtermvalidationtest;

class LongTermValidationTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();

    void cadesSignatureAttributes();
    void validationDataFromOcsp();
    void validationDataFallsBackToCrl();
    void issuerIsFetched();
    void revokedCertificateIsReported();
    void foreignOcspAnswerIsRejected();
    void staleRevocationDataIsRefused();
    void existingStoreIsKept();
    void encryptedDocument();
    void extensionWithoutTimestampAuthority();
    void verificationReport();
    void verificationReportOfChangedFile();
    void verificationReportIgnoresDecoyCertificate();
    void verificationReportGapInSignedRange();
    void verificationReportChainInSigningCertificate();
    void onlyTheNamedSignatureIsWalked();

private:
    QByteArray sign(X509* certificate, bool withRoot, const std::optional<QString>& encryptionPassword = std::nullopt);
    static PDFDocument read(const QByteArray& data, const QString& password = QString());
    static std::vector<PDFSignatureVerificationResult> verify(const QByteArray& data, const QString& password = QString());

    TestCertificationAuthority m_authority;
    X509* m_good = nullptr;
    X509* m_crlOnly = nullptr;
    X509* m_revoked = nullptr;
    X509* m_hostileName = nullptr;
};

void LongTermValidationTest::initTestCase()
{
    QVERIFY(m_authority.start());
    m_good = m_authority.issue("Andre PDF test signer", 10, true, true);
    m_crlOnly = m_authority.issue("Andre PDF test signer (CRL only)", 11, false, true);
    m_revoked = m_authority.issue("Andre PDF test signer (revoked)", 12, true, true);
    m_hostileName = m_authority.issue("<b>Mallory</b> & \"Co\"", 13, true, true);
    QVERIFY(m_good && m_crlOnly && m_revoked);
}

void LongTermValidationTest::init()
{
    m_authority.ocspBehaviour = TestCertificationAuthority::OcspBehaviour::Answer;
    m_authority.isCrlAvailable = true;
    m_authority.revokedSerial = -1;
    m_authority.isStale = false;
}

QByteArray LongTermValidationTest::sign(X509* certificate, bool withRoot, const std::optional<QString>& encryptionPassword)
{
    PDFDocumentBuilder builder;

    const PDFObjectReference page = builder.appendPage(QRectF(0, 0, 595, 842));
    const PDFDocument document = builder.build();

    PDFCertificateEntry entry;
    entry.pkcs12 = m_authority.createPkcs12(certificate, withRoot);

    PDFDocumentSigner::Parameters parameters;
    parameters.document = &document;
    parameters.encryptionUserPassword = encryptionPassword;
    parameters.certification = PDFDocumentSigner::Certification::FormFilling;
    parameters.signFunction = [entry](const QByteArray& data, QByteArray& result) { return PDFSignatureFactory::sign(entry, "test", data, result); };
    parameters.createSignatureFieldFunction = [page](PDFDocumentBuilder& fieldBuilder, PDFObjectReference signatureDictionary)
    {
        return fieldBuilder.createSignatureField("Signature", signatureDictionary, page);
    };

    QByteArray signedDocument;
    const PDFDocumentSigner::Result result = PDFDocumentSigner::sign(parameters, signedDocument);
    return result == PDFDocumentSigner::Result::OK ? signedDocument : QByteArray();
}

PDFDocument LongTermValidationTest::read(const QByteArray& data, const QString& password)
{
    bool offered = false;
    PDFDocumentReader reader(nullptr, [&](bool* ok) { *ok = !offered; offered = true; return password; }, false, false);
    return reader.readFromBuffer(data);
}

std::vector<PDFSignatureVerificationResult> LongTermValidationTest::verify(const QByteArray& data, const QString& password)
{
    const PDFDocument document = read(data, password);
    PDFCertificateStore store;
    PDFSignatureHandler::Parameters parameters;
    parameters.store = &store;
    parameters.useSystemCertificateStore = false;
    parameters.dss = &document.getCatalog()->getDocumentSecurityStore();
    const PDFForm form = PDFForm::parse(&document, document.getCatalog()->getFormObject());
    return PDFSignatureHandler::verifySignatures(form, data, parameters);
}

void LongTermValidationTest::cadesSignatureAttributes()
{
    const QByteArray signedDocument = sign(m_good, true);
    QVERIFY(!signedDocument.isEmpty());
    QVERIFY(signedDocument.contains("/ETSI.CAdES.detached"));

    const PDFDocument document = read(signedDocument);
    QByteArray contents;
    PDFForm::parse(&document, document.getCatalog()->getFormObject()).apply([&contents](const PDFFormField* field)
    {
        if (const PDFFormFieldSignature* signatureField = dynamic_cast<const PDFFormFieldSignature*>(field))
        {
            contents = signatureField->getSignature().getContents();
        }
    });

    const unsigned char* pointer = reinterpret_cast<const unsigned char*>(contents.constData());
    CMS_ContentInfo* cms = d2i_CMS_ContentInfo(nullptr, &pointer, contents.size());
    QVERIFY(cms);
    CMS_SignerInfo* signerInfo = sk_CMS_SignerInfo_value(CMS_get0_SignerInfos(cms), 0);
    QVERIFY(signerInfo);
    QVERIFY(CMS_signed_get_attr_by_NID(signerInfo, NID_id_smime_aa_signingCertificateV2, -1) >= 0);
#ifdef CMS_NO_SIGNING_TIME
    QCOMPARE(CMS_signed_get_attr_by_NID(signerInfo, NID_pkcs9_signingTime, -1), -1);
#endif
    CMS_ContentInfo_free(cms);

    const auto results = verify(signedDocument);
    QCOMPARE(results.size(), size_t(1));
    QVERIFY(results.front().isSignatureValid());
}

void LongTermValidationTest::validationDataFromOcsp()
{
    const QByteArray signedDocument = sign(m_good, true);
    QVERIFY(!signedDocument.isEmpty());

    const PDFLongTermValidation::Result result = PDFLongTermValidation::addValidationData(signedDocument, QString(), "Signature", 5000);
    QVERIFY2(result.isOK(), qPrintable(result.error));
    QVERIFY2(result.warnings.isEmpty(), qPrintable(result.warnings.join('\n')));
    QVERIFY(result.revokedCertificates.isEmpty());
    QCOMPARE(result.ocspCount, 1);
    QCOMPARE(result.crlCount, 1);   // the CRL as well, for verifiers checking CRLs only
    QCOMPARE(result.certificateCount, 2);
    QVERIFY(result.document.startsWith(signedDocument));

    const PDFDocument document = read(result.document);
    const auto* store = document.getCatalog()->getDocumentSecurityStore().getMasterItem();
    QCOMPARE(store->OCSP.size(), size_t(1));
    QCOMPARE(store->Cert.size(), size_t(2));
    QVERIFY(std::find(store->Cert.cbegin(), store->Cert.cend(), m_authority.getRoot()) != store->Cert.cend());

    const auto results = verify(result.document);
    QCOMPARE(results.size(), size_t(1));
    QVERIFY(results.front().isSignatureValid());
}

void LongTermValidationTest::validationDataFallsBackToCrl()
{
    const QByteArray signedDocument = sign(m_crlOnly, true);
    QVERIFY(!signedDocument.isEmpty());
    const PDFLongTermValidation::Result result = PDFLongTermValidation::addValidationData(signedDocument, QString(), "Signature", 5000);
    QVERIFY2(result.isOK(), qPrintable(result.error));
    QCOMPARE(result.ocspCount, 0);
    QCOMPARE(result.crlCount, 1);
    QVERIFY(result.warnings.isEmpty());

    // The responder is down: the CRL is used instead
    m_authority.ocspBehaviour = TestCertificationAuthority::OcspBehaviour::Unavailable;
    const PDFLongTermValidation::Result fallback = PDFLongTermValidation::addValidationData(sign(m_good, true), QString(), "Signature", 5000);
    QVERIFY(fallback.isOK());
    QCOMPARE(fallback.ocspCount, 0);
    QCOMPARE(fallback.crlCount, 1);
}

void LongTermValidationTest::issuerIsFetched()
{
    const QByteArray signedDocument = sign(m_good, false);
    QVERIFY(!signedDocument.isEmpty());
    const PDFLongTermValidation::Result result = PDFLongTermValidation::addValidationData(signedDocument, QString(), "Signature", 5000);
    QVERIFY2(result.isOK(), qPrintable(result.error));
    QVERIFY2(result.warnings.isEmpty(), qPrintable(result.warnings.join('\n')));
    QCOMPARE(result.certificateCount, 2);
    QCOMPARE(result.ocspCount, 1);
}

void LongTermValidationTest::revokedCertificateIsReported()
{
    m_authority.revokedSerial = 12;
    const QByteArray signedDocument = sign(m_revoked, true);
    QVERIFY(!signedDocument.isEmpty());
    PDFLongTermValidation::Result result = PDFLongTermValidation::addValidationData(signedDocument, QString(), "Signature", 5000);
    QVERIFY(result.isOK());
    QCOMPARE(result.revokedCertificates.size(), 1);

    m_authority.ocspBehaviour = TestCertificationAuthority::OcspBehaviour::Unavailable;
    result = PDFLongTermValidation::addValidationData(signedDocument, QString(), "Signature", 5000);
    QCOMPARE(result.revokedCertificates.size(), 1);
    QCOMPARE(result.crlCount, 1);
}

void LongTermValidationTest::foreignOcspAnswerIsRejected()
{
    m_authority.ocspBehaviour = TestCertificationAuthority::OcspBehaviour::ForeignSigner;
    m_authority.isCrlAvailable = false;
    const PDFLongTermValidation::Result result = PDFLongTermValidation::addValidationData(sign(m_good, true), QString(), "Signature", 5000);
    QVERIFY(result.isOK());
    QCOMPARE(result.ocspCount, 0);
    QCOMPARE(result.crlCount, 0);
    QCOMPARE(result.warnings.size(), 1);
}

void LongTermValidationTest::staleRevocationDataIsRefused()
{
    // Review R2-A M1: an OCSP answer a month old (no next update) and an expired CRL
    m_authority.isStale = true;
    const PDFLongTermValidation::Result result = PDFLongTermValidation::addValidationData(sign(m_good, true), QString(), "Signature", 5000);
    QVERIFY(result.isOK());
    QCOMPARE(result.ocspCount, 0);
    QCOMPARE(result.crlCount, 0);
    QCOMPARE(result.warnings.size(), 1);
}

void LongTermValidationTest::existingStoreIsKept()
{
    const PDFLongTermValidation::Result first = PDFLongTermValidation::addValidationData(sign(m_good, true), QString(), "Signature", 5000);
    QVERIFY(first.isOK());

    // The second time (no OCSP): the certificates and the CRL are there already
    m_authority.ocspBehaviour = TestCertificationAuthority::OcspBehaviour::Unavailable;
    const PDFLongTermValidation::Result second = PDFLongTermValidation::addValidationData(first.document, QString(), "Signature", 5000);
    QVERIFY(second.isOK());
    QCOMPARE(second.certificateCount, 0);
    QCOMPARE(second.crlCount, 0);
    QVERIFY(second.warnings.isEmpty());

    const PDFDocument document = read(second.document);
    const auto* store = document.getCatalog()->getDocumentSecurityStore().getMasterItem();
    QCOMPARE(store->Cert.size(), size_t(2));
    QCOMPARE(store->OCSP.size(), size_t(1));
    QCOMPARE(store->CRL.size(), size_t(1));
}

void LongTermValidationTest::encryptedDocument()
{
    const QByteArray signedDocument = sign(m_good, true, QString("secret"));
    QVERIFY(!signedDocument.isEmpty());

    QVERIFY(!PDFLongTermValidation::addValidationData(signedDocument, QString("wrong"), "Signature", 5000).isOK());

    const PDFLongTermValidation::Result result = PDFLongTermValidation::addValidationData(signedDocument, QString("secret"), "Signature", 5000);
    QVERIFY2(result.isOK(), qPrintable(result.error));
    const PDFDocument document = read(result.document, "secret");
    QCOMPARE(document.getCatalog()->getDocumentSecurityStore().getMasterItem()->OCSP.size(), size_t(1));

    const auto results = verify(result.document, "secret");
    QCOMPARE(results.size(), size_t(1));
    QVERIFY(results.front().isSignatureValid());
}

void LongTermValidationTest::extensionWithoutTimestampAuthority()
{
    const QByteArray signedDocument = sign(m_good, true);
    QVERIFY(!signedDocument.isEmpty());

    PDFSignatureFactory::TimestampSettings settings;
    settings.url = "http://127.0.0.1:1/";
    settings.timeoutMilliseconds = 2000;
    const PDFLongTermValidation::Extension extension = PDFLongTermValidation::extend(signedDocument, QString(), "Signature", settings);
    QVERIFY(extension.validation.isOK());
    QVERIFY(extension.timestampFieldName.isEmpty());
    QVERIFY(!extension.timestampError.isEmpty());
    QCOMPARE(extension.document, extension.validation.document);
    QVERIFY(!extension.getReport().isEmpty());

    QVERIFY(PDFLongTermValidation::checkExtendedFile(extension.document, signedDocument, "Signature", QString(), QString()).isOK());

    // A changed signed byte, or a file of other bytes, fails the check
    QByteArray changed = extension.document;
    changed[signedDocument.size() / 2] = changed[signedDocument.size() / 2] ^ 0x01;
    QVERIFY(!PDFLongTermValidation::checkExtendedFile(changed, signedDocument, "Signature", QString(), QString()).isOK());
    QVERIFY(!PDFLongTermValidation::checkExtendedFile(extension.document.mid(1), signedDocument, "Signature", QString(), QString()).isOK());
}

void LongTermValidationTest::verificationReport()
{
    const PDFLongTermValidation::Result extended = PDFLongTermValidation::addValidationData(sign(m_good, true), QString(), "Signature", 5000);
    QVERIFY(extended.isOK());

    const PDFSignatureReport::Report report = PDFSignatureReport::create(extended.document, "contract.pdf", QString(), false);
    QVERIFY(report.isOpened);
    QCOMPARE(report.certification, int(PDFDocumentSigner::Certification::FormFilling));
    QCOMPARE(report.storedOcsps, 1);
    QCOMPARE(report.signatures.size(), size_t(1));

    const PDFSignatureReport::Signature& signature = report.signatures.front();
    QVERIFY(signature.isIntegrityValid);
    QVERIFY(!signature.isCoveringWholeFile);
    QVERIFY(!signature.isCertificateTrusted);
    QCOMPARE(signature.padesLevel, QString("B-B"));
    QCOMPARE(signature.revocation, PDFSignatureReport::Revocation::Good);
    QCOMPARE(signature.revocationSource, QString("OCSP"));
    QCOMPARE(signature.chain.size(), size_t(2));
    QCOMPARE(signature.chain[0].commonName, QString("Andre PDF test signer"));
    QCOMPARE(signature.chain[0].serialNumber, QString("0A"));
    QVERIFY(signature.chain[0].issuer.contains("Andre PDF test root CA"));
    QVERIFY(signature.chain[1].isSelfSigned);

    const QByteArray der = toDer(m_good);
    const QString sha256 = QString::fromLatin1(QCryptographicHash::hash(der, QCryptographicHash::Sha256).toHex(':').toUpper());
    QCOMPARE(signature.chain[0].sha256, sha256);

    const QString html = PDFSignatureReport::toHtml(report);
    QVERIFY(html.contains(sha256));
    QVERIFY(html.contains("ECT Act"));

    // A revoked signer and a name with markup: the status is reported, the markup escaped
    m_authority.revokedSerial = 12;
    const PDFLongTermValidation::Result revoked = PDFLongTermValidation::addValidationData(sign(m_revoked, true), QString(), "Signature", 5000);
    QCOMPARE(PDFSignatureReport::create(revoked.document, "r.pdf", QString(), false).signatures.front().revocation, PDFSignatureReport::Revocation::Revoked);

    const PDFSignatureReport::Report hostile = PDFSignatureReport::create(sign(m_hostileName, true), "<x>.pdf", QString(), false);
    const QString hostileHtml = PDFSignatureReport::toHtml(hostile);
    QVERIFY(!hostileHtml.contains("<b>Mallory"));
    QVERIFY(!hostileHtml.contains("<x>"));
    QVERIFY(hostileHtml.contains("&lt;b&gt;Mallory"));
    QCOMPARE(hostile.signatures.front().revocation, PDFSignatureReport::Revocation::NotChecked);
}

void LongTermValidationTest::verificationReportOfChangedFile()
{
    QByteArray signedDocument = sign(m_good, true);
    QVERIFY(!signedDocument.isEmpty());

    // One byte of the signed range changed (inside the page dictionary's MediaBox)
    const qsizetype index = signedDocument.indexOf("595");
    QVERIFY(index > 0);
    signedDocument[index] = '6';

    const PDFSignatureReport::Report report = PDFSignatureReport::create(signedDocument, "changed.pdf", QString(), false);
    QVERIFY(report.isOpened);
    QCOMPARE(report.signatures.size(), size_t(1));
    QVERIFY(!report.signatures.front().isIntegrityValid);
    QVERIFY(PDFSignatureReport::toHtml(report).contains("INVALID"));

    QVERIFY(!PDFSignatureReport::create("not a pdf", "x.pdf", QString(), false).isOpened);
}

void LongTermValidationTest::verificationReportIgnoresDecoyCertificate()
{
    for (const bool withSigner : { true, false })
    {
        PDFDocumentBuilder builder;
        const PDFObjectReference page = builder.appendPage(QRectF(0, 0, 595, 842));
        const PDFDocument document = builder.build();

        PDFDocumentSigner::Parameters parameters;
        parameters.document = &document;
        parameters.signFunction = [this, withSigner](const QByteArray& data, QByteArray& result) { result = m_authority.signWithDecoy(m_good, data, withSigner); return !result.isEmpty(); };
        parameters.createSignatureFieldFunction = [page](PDFDocumentBuilder& fieldBuilder, PDFObjectReference signatureDictionary)
        {
            return fieldBuilder.createSignatureField("Signature", signatureDictionary, page);
        };
        QByteArray signedDocument;
        PDFDocumentSigner::sign(parameters, signedDocument);
        if (signedDocument.isEmpty())
        {
            // Without the real certificate the signer's own check refuses it already
            QVERIFY(!withSigner);
            continue;
        }

        const PDFSignatureReport::Report report = PDFSignatureReport::create(signedDocument, "decoy.pdf", QString(), false);
        QCOMPARE(report.signatures.size(), size_t(1));
        const PDFSignatureReport::Signature& signature = report.signatures.front();
        if (withSigner)
        {
            QVERIFY(!signature.chain.empty());
            QCOMPARE(signature.chain.front().commonName, QString("Andre PDF test signer"));
        }
        else
        {
            QVERIFY(!signature.isIntegrityValid);
            QVERIFY(signature.chain.empty());
        }
        QVERIFY(!PDFSignatureReport::toHtml(report).contains("Decoy Person") || withSigner);
    }
}

void LongTermValidationTest::verificationReportGapInSignedRange()
{
    QByteArray signedDocument = sign(m_good, true);
    QVERIFY(!signedDocument.isEmpty());

    // Shift the second byte range one byte later: one signed byte is left out
    const QRegularExpression byteRange(QStringLiteral("/ByteRange\\s*\\[\\s*0\\s+(\\d+)\\s+(\\d+)\\s+(\\d+)\\s*\\]"));
    const QRegularExpressionMatch match = byteRange.match(QString::fromLatin1(signedDocument));
    QVERIFY(match.hasMatch());
    const QString original = match.captured(0);
    QString changed = QString("/ByteRange [0 %1 %2 %3]").arg(match.captured(1)).arg(match.captured(2).toLongLong() + 1).arg(match.captured(3).toLongLong() - 1);
    changed = changed.leftJustified(original.size(), QChar(' '));
    QCOMPARE(changed.size(), original.size());
    signedDocument.replace(original.toLatin1(), changed.toLatin1());

    const PDFSignatureReport::Report report = PDFSignatureReport::create(signedDocument, "gap.pdf", QString(), false);
    QCOMPARE(report.signatures.size(), size_t(1));
    QVERIFY(!report.signatures.front().isIntegrityValid);
    QVERIFY(!report.signatures.front().isCoveringWholeFile);
}

void LongTermValidationTest::verificationReportChainInSigningCertificate()
{
    // Review R3 N2: the attribute names the signer and its root
    PDFDocumentBuilder builder;
    const PDFObjectReference page = builder.appendPage(QRectF(0, 0, 595, 842));
    const PDFDocument document = builder.build();
    PDFDocumentSigner::Parameters parameters;
    parameters.document = &document;
    parameters.signFunction = [this](const QByteArray& data, QByteArray& result) { result = m_authority.signWithChainAttribute(m_good, data); return !result.isEmpty(); };
    parameters.createSignatureFieldFunction = [page](PDFDocumentBuilder& fieldBuilder, PDFObjectReference signatureDictionary)
    {
        return fieldBuilder.createSignatureField("Signature", signatureDictionary, page);
    };
    QByteArray signedDocument;
    QCOMPARE(PDFDocumentSigner::sign(parameters, signedDocument), PDFDocumentSigner::Result::OK);

    const PDFSignatureReport::Report report = PDFSignatureReport::create(signedDocument, "chain.pdf", QString(), false);
    QCOMPARE(report.signatures.size(), size_t(1));
    QVERIFY2(report.signatures.front().isIntegrityValid, qPrintable(report.signatures.front().errors.join('\n')));
    QVERIFY(!report.signatures.front().chain.empty());
    QCOMPARE(report.signatures.front().chain.front().commonName, QString("Andre PDF test signer"));
}

void LongTermValidationTest::onlyTheNamedSignatureIsWalked()
{
    // Review R3 N1: a revoked first signer, then a good countersigner; only the named
    // signature's chain is checked, whatever the byte ranges in the file say
    m_authority.revokedSerial = 12;
    const QByteArray first = sign(m_revoked, true);
    QVERIFY(!first.isEmpty());

    bool offered = false;
    PDFDocumentReader reader(nullptr, [&](bool* ok) { *ok = !offered; offered = true; return QString(); }, false, false);
    const PDFDocument document = reader.readFromBuffer(first);
    PDFCertificateEntry entry;
    entry.pkcs12 = m_authority.createPkcs12(m_good, true);
    PDFDocumentSigner::Parameters parameters;
    parameters.document = &document;
    parameters.originalDocumentData = first;
    parameters.signFunction = [entry](const QByteArray& data, QByteArray& result) { return PDFSignatureFactory::sign(entry, "test", data, result); };
    const PDFObjectReference page = document.getCatalog()->getPage(0)->getPageReference();
    parameters.createSignatureFieldFunction = [page](PDFDocumentBuilder& fieldBuilder, PDFObjectReference signatureDictionary)
    {
        return fieldBuilder.createSignatureField("Second", signatureDictionary, page);
    };
    QByteArray both;
    QCOMPARE(PDFDocumentSigner::sign(parameters, both), PDFDocumentSigner::Result::OK);

    const PDFLongTermValidation::Result second = PDFLongTermValidation::addValidationData(both, QString(), "Second", 5000);
    QVERIFY(second.isOK());
    QVERIFY(second.revokedCertificates.isEmpty());

    const PDFLongTermValidation::Result firstOnly = PDFLongTermValidation::addValidationData(both, QString(), "Signature", 5000);
    QCOMPARE(firstOnly.revokedCertificates.size(), 1);

    QVERIFY(!PDFLongTermValidation::addValidationData(both, QString(), "Missing", 5000).isOK());
}

QTEST_MAIN(LongTermValidationTest)

#include "tst_longtermvalidationtest.moc"
