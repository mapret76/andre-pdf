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

#include "pdfdocumentbuilder.h"
#include "pdfcertificatemanager.h"
#include "pdfdocumentsigner.h"
#include "pdfsignaturehandler.h"
#include "pdfform.h"
#include "pdfdocumentreader.h"
#include "pdfdocumentwriter.h"
#include "pdfocrapplyprocessor.h"
#include "pdfsignaturestamp.h"
#include "pdfsignaturelibrary.h"
#include "pdfpageinitials.h"
#include "pdfsignaturelinefinder.h"
#include "pdfcontractpack.h"
#include "pdfsizereducer.h"
#include "pdfofficeexporter.h"
#include "pdftextredline.h"
#include "pdffontidentifier.h"
#include "pdffontinstaller.h"
#include "pdfscantexteditor.h"
#include "pdfscanfontbuilder.h"
#include "pdfversionhistory.h"
#include <QRawFont>
#include "pdfrenderer.h"
#include "pdfcms.h"
#include "pdffont.h"
#include "pdfcolorspaces.h"
#include "pdfoptionalcontent.h"
#include "pdfsecurityhandler.h"

#include <QtTest>
#include <QBuffer>
#include <QTemporaryDir>
#include <QPainter>
#include <QProcess>
#include <QElapsedTimer>
#include <QTcpServer>
#include <QTcpSocket>
#include <QFontDatabase>
#include <QRandomGenerator>
#include <QStandardPaths>
#include <QCryptographicHash>

using namespace pdf;

namespace signaturebuildertest
{
PDFObject dictionary(std::initializer_list<std::pair<const char*, PDFObject>> entries)
{
    PDFDictionaryBuilder result;
    for (const auto& entry : entries)
    {
        result.setEntry(PDFInplaceOrMemoryString(entry.first), PDFObject(entry.second));
    }
    return PDFObject::createDictionary(std::move(result));
}

PDFObjectReference appearance(PDFDocumentBuilder& builder, QRectF bbox)
{
    PDFObjectFactory factory;
    factory.beginDictionary();
    factory.beginDictionaryItem("Type");
    factory << WrapName("XObject");
    factory.endDictionaryItem();
    factory.beginDictionaryItem("Subtype");
    factory << WrapName("Form");
    factory.endDictionaryItem();
    factory.beginDictionaryItem("BBox");
    factory << bbox;
    factory.endDictionaryItem();
    factory.endDictionary();
    const PDFObject object = factory.takeObject();
    PDFDictionaryBuilder streamDictionary(*object.getDictionary());
    QByteArray data("1 0 0 rg 20 30 80 40 re f\n");
    streamDictionary.setEntry(PDFInplaceOrMemoryString("Length"), PDFObject::createInteger(data.size()));
    return builder.addObject(PDFObject::createStream(PDFStream(std::move(streamDictionary), std::move(data))));
}
}   // namespace signaturebuildertest

using namespace signaturebuildertest;

class SignatureBuilderTest : public QObject
{
    Q_OBJECT
private slots:
    void certificateSignatureUsage();
    void signedDocumentRoundTrip();
    void signatureSizeChanges_data();
    void signatureSizeChanges();
    void signingFailureIsReported_data();
    void signingFailureIsReported();
    void multipleSignatures();
    void signingOverInvalidSignature();
    void existingSignaturesNotPreserved();
    void preservesAcroForm_data();
    void preservesAcroForm();
    void widgetStructure_data();
    void widgetStructure();
    void certification_data();
    void certification();
    void certificationMustBeFirst();
    void verificationStamp();
    void encryptedCertification();
    void signatureLibrary();
    void signatureExtraction();
    void initialsOnEveryPage();
    void signatureLineLabels();
    void signatureLineFinder();
    void contractPack();
    void sizeReduction();
    void officeExport();
    void textRedline();
    void fontNames();
    void fontMatching();
    void fontInstaller();
    void scanTextEdit();
    void scanTextForeignOcr_data();
    void scanTextForeignOcr();
    void scanFont();
    void versionHistory();

private:
    static bool createTestCertificate(const QTemporaryDir& directory, PDFCertificateEntry& certificate, QString& password);
    static std::vector<PDFSignatureVerificationResult> verifySignedDocument(const QByteArray& signedDocument);
    static PDFDocument readDocument(const QByteArray& data);

    /// Signs the first page of the document by an invisible signature
    static PDFDocumentSigner::Result signDocument(const PDFDocument& document,
                                                  const QByteArray& originalData,
                                                  const PDFCertificateEntry& certificate,
                                                  const QString& password,
                                                  const QString& fieldName,
                                                  QByteArray& signedDocument,
                                                  PDFDocumentSigner::Certification certification = PDFDocumentSigner::Certification::None);
};

PDFDocument SignatureBuilderTest::readDocument(const QByteArray& data)
{
    PDFDocumentReader reader(nullptr, nullptr, false, false);
    return reader.readFromBuffer(data);
}

PDFDocumentSigner::Result SignatureBuilderTest::signDocument(const PDFDocument& document,
                                                             const QByteArray& originalData,
                                                             const PDFCertificateEntry& certificate,
                                                             const QString& password,
                                                             const QString& fieldName,
                                                             QByteArray& signedDocument,
                                                             PDFDocumentSigner::Certification certification)
{
    PDFDocumentSigner::Parameters parameters;
    parameters.document = &document;
    parameters.originalDocumentData = originalData;
    parameters.certification = certification;
    parameters.signFunction = [&](const QByteArray& data, QByteArray& signature)
    {
        return PDFSignatureFactory::sign(certificate, password, data, signature);
    };
    parameters.createSignatureFieldFunction = [&](PDFDocumentBuilder& builder, PDFObjectReference signatureDictionary)
    {
        return builder.createSignatureField(fieldName, signatureDictionary, document.getCatalog()->getPage(0)->getPageReference());
    };
    return PDFDocumentSigner::sign(parameters, signedDocument);
}

bool SignatureBuilderTest::createTestCertificate(const QTemporaryDir& directory, PDFCertificateEntry& certificate, QString& password)
{
    PDFCertificateManager::NewCertificateInfo info;
    info.fileName = directory.filePath("signature-test.p12");
    info.privateKeyPasword = "test-password";
    info.certCommonName = "PDF4QT signature regression test";
    info.rsaKeyLength = 2048;
    PDFCertificateManager().createCertificate(info);

    QFile file(info.fileName);
    if (!file.open(QIODevice::ReadOnly))
    {
        return false;
    }

    certificate = PDFCertificateEntry();
    certificate.pkcs12 = file.readAll();
    file.close();
    password = info.privateKeyPasword;

    return !certificate.pkcs12.isEmpty();
}

std::vector<PDFSignatureVerificationResult> SignatureBuilderTest::verifySignedDocument(const QByteArray& signedDocument)
{
    PDFDocumentReader reader(nullptr, nullptr, false, false);
    PDFDocument document = reader.readFromBuffer(signedDocument);

    if (reader.getReadingResult() != PDFDocumentReader::Result::OK)
    {
        return { };
    }

    const PDFForm form = PDFForm::parse(&document, document.getCatalog()->getFormObject());
    PDFSignatureHandler::Parameters parameters;
    parameters.useSystemCertificateStore = false;
    return PDFSignatureHandler::verifySignatures(form, signedDocument, parameters);
}

void SignatureBuilderTest::certificateSignatureUsage()
{
    // Only certificates, which declare the digital signature or the non
    // repudiation key usage, can be offered to the user for signing. The
    // personal certificate storage of the operating system contains also
    // certificates without the key usage extension, which are generated
    // by the system for its internal purposes.
    PDFCertificateInfo info;
    QVERIFY(!info.isUsableForDigitalSignature());

    info.setKeyUsage(PDFCertificateInfo::KeyUsageKeyEncipherment);
    QVERIFY(!info.isUsableForDigitalSignature());

    info.setKeyUsage(PDFCertificateInfo::KeyUsageDigitalSignature);
    QVERIFY(info.isUsableForDigitalSignature());

    info.setKeyUsage(PDFCertificateInfo::KeyUsageNonRepudiation);
    QVERIFY(info.isUsableForDigitalSignature());

    info.setKeyUsage(PDFCertificateInfo::KeyUsageFlags(PDFCertificateInfo::KeyUsageDigitalSignature | PDFCertificateInfo::KeyUsageAgreement));
    QVERIFY(info.isUsableForDigitalSignature());
}

void SignatureBuilderTest::signedDocumentRoundTrip()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    PDFCertificateEntry certificate;
    QString password;
    QVERIFY(createTestCertificate(directory, certificate, password));

    for (bool visible : {false, true})
    {
        PDFDocumentBuilder builder;
        const auto page = builder.appendPage(QRectF(100, 200, 300, 400));
        PDFDocument document = builder.build();

        QByteArray signedDocument;
        PDFDocumentSigner::Parameters parameters;
        parameters.document = &document;
        parameters.signFunction = [&](const QByteArray& data, QByteArray& signature)
        {
            return PDFSignatureFactory::sign(certificate, password, data, signature);
        };
        parameters.createSignatureFieldFunction = [&](PDFDocumentBuilder& signedBuilder, PDFObjectReference signatureDictionary)
        {
            const auto stream = visible ? appearance(signedBuilder, QRectF(20, 30, 80, 40)) : PDFObjectReference();
            return signedBuilder.createSignatureField("Signature", signatureDictionary, page, stream, QRectF(120, 230, 80, 40));
        };

        QCOMPARE(PDFDocumentSigner::sign(parameters, signedDocument), PDFDocumentSigner::Result::OK);
        QVERIFY(!signedDocument.isEmpty());

        const auto results = verifySignedDocument(signedDocument);
        QCOMPARE(results.size(), size_t(1));
        QVERIFY2(results.front().isSignatureValid(), qPrintable(results.front().getErrors().join('\n')));
        QVERIFY(!results.front().hasSignatureWarning());

        // A changed signedData byte must be detected independently of certificate trust.
        QByteArray changedBytes = signedDocument;
        changedBytes[10] = changedBytes[10] == 'X' ? 'Y' : 'X';
        const auto changedResults = verifySignedDocument(changedBytes);
        QCOMPARE(changedResults.size(), size_t(1));
        QVERIFY(!changedResults.front().isSignatureValid());
    }
}

void SignatureBuilderTest::signatureSizeChanges_data()
{
    QTest::addColumn<int>("extraBytes");

    // The size of a signature is not stable - the same data signedData twice can
    // produce results differing by a few bytes. The document must be signedData
    // correctly whatever the size of the final signature is, including the case
    // when it does not fit into the initially reserved space.
    QTest::newRow("same-size") << 0;
    QTest::newRow("longer-by-one") << 1;
    QTest::newRow("longer-by-eight") << 8;
    QTest::newRow("longer-than-reserved-space") << 2000;
    QTest::newRow("much-longer-than-reserved-space") << 40000;
}

void SignatureBuilderTest::signatureSizeChanges()
{
    QFETCH(int, extraBytes);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    PDFCertificateEntry certificate;
    QString password;
    QVERIFY(createTestCertificate(directory, certificate, password));

    PDFDocumentBuilder builder;
    const auto page = builder.appendPage(QRectF(0, 0, 300, 400));
    PDFDocument document = builder.build();

    // The trial signature keeps its size, the signature of the document is
    // enlarged - the padding is ignored by the DER decoder, so the signature
    // stays valid.
    QByteArray signedDocument;
    PDFDocumentSigner::Parameters parameters;
    parameters.document = &document;
    parameters.signFunction = [&](const QByteArray& data, QByteArray& signature)
    {
        if (!PDFSignatureFactory::sign(certificate, password, data, signature))
        {
            return false;
        }

        if (data.size() > 1000)
        {
            signature.append(QByteArray(extraBytes, char(0)));
        }

        return true;
    };
    parameters.createSignatureFieldFunction = [&](PDFDocumentBuilder& signedBuilder, PDFObjectReference signatureDictionary)
    {
        return signedBuilder.createSignatureField("Signature", signatureDictionary, page);
    };

    QCOMPARE(PDFDocumentSigner::sign(parameters, signedDocument), PDFDocumentSigner::Result::OK);

    const auto results = verifySignedDocument(signedDocument);
    QCOMPARE(results.size(), size_t(1));
    QVERIFY2(results.front().isSignatureValid(), qPrintable(results.front().getErrors().join('\n')));

    // Nothing outside of the signature string may be left out of the signature
    QVERIFY(!results.front().hasSignatureWarning());
}

void SignatureBuilderTest::signingFailureIsReported_data()
{
    QTest::addColumn<int>("failureMode");
    QTest::addColumn<int>("expectedResult");

    QTest::newRow("signing-failed") << 0 << int(PDFDocumentSigner::Result::SigningFailed);
    QTest::newRow("signature-never-fits") << 1 << int(PDFDocumentSigner::Result::SignatureTooLarge);
    QTest::newRow("damaged-signature") << 2 << int(PDFDocumentSigner::Result::VerificationFailed);
}

void SignatureBuilderTest::signingFailureIsReported()
{
    QFETCH(int, failureMode);
    QFETCH(int, expectedResult);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    PDFCertificateEntry certificate;
    QString password;
    QVERIFY(createTestCertificate(directory, certificate, password));

    PDFDocumentBuilder builder;
    const auto page = builder.appendPage(QRectF(0, 0, 300, 400));
    PDFDocument document = builder.build();

    QByteArray signedDocument;
    int signatureGrowth = 0;
    PDFDocumentSigner::Parameters parameters;
    parameters.document = &document;
    parameters.signFunction = [&](const QByteArray& data, QByteArray& signature)
    {
        const bool isDocumentSigned = data.size() > 1000;

        if (failureMode == 0 && isDocumentSigned)
        {
            return false;
        }

        if (!PDFSignatureFactory::sign(certificate, password, data, signature))
        {
            return false;
        }

        if (failureMode == 1 && isDocumentSigned)
        {
            // The signature quadruples on each attempt, which is faster than the
            // signer can enlarge the reserved space, so it never fits - the
            // signer must give up instead of writing a damaged document.
            signature.append(QByteArray((1 << 16) << (2 * signatureGrowth++), char(0)));
        }

        if (failureMode == 2 && isDocumentSigned)
        {
            // The signature does not belong to the signedData data
            PDFSignatureFactory::sign(certificate, password, QByteArray("something else"), signature);
        }

        return true;
    };
    parameters.createSignatureFieldFunction = [&](PDFDocumentBuilder& signedBuilder, PDFObjectReference signatureDictionary)
    {
        return signedBuilder.createSignatureField("Signature", signatureDictionary, page);
    };

    QCOMPARE(int(PDFDocumentSigner::sign(parameters, signedDocument)), expectedResult);

    // Nothing may be handed over to the caller, when the document was not signedData
    QVERIFY(signedDocument.isEmpty());
}

void SignatureBuilderTest::multipleSignatures()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    PDFCertificateEntry certificate;
    QString password;
    QVERIFY(createTestCertificate(directory, certificate, password));

    PDFDocumentBuilder builder;
    builder.appendPage(QRectF(0, 0, 300, 400));
    const PDFDocument document = builder.build();

    QByteArray firstSigned;
    QCOMPARE(signDocument(document, QByteArray(), certificate, password, "First", firstSigned), PDFDocumentSigner::Result::OK);

    // The second signature is added as an incremental update, so the bytes
    // covered by the first signature are not touched and it stays valid.
    QByteArray secondSigned;
    QCOMPARE(signDocument(readDocument(firstSigned), firstSigned, certificate, password, "Second", secondSigned), PDFDocumentSigner::Result::OK);
    QVERIFY(secondSigned.startsWith(firstSigned));

    QByteArray thirdSigned;
    QCOMPARE(signDocument(readDocument(secondSigned), secondSigned, certificate, password, "Third", thirdSigned), PDFDocumentSigner::Result::OK);
    QVERIFY(thirdSigned.startsWith(secondSigned));

    const auto results = verifySignedDocument(thirdSigned);
    QCOMPARE(results.size(), size_t(3));
    for (const PDFSignatureVerificationResult& result : results)
    {
        QVERIFY2(result.isSignatureValid(), qPrintable(result.getSignatureFieldQualifiedName() + ": " + result.getErrors().join('\n')));
    }

    // The last signature covers the whole document, the earlier ones only their part of it
    QVERIFY(results.front().hasSignatureWarning());
    QVERIFY(!results.back().hasSignatureWarning());
    QCOMPARE(results.back().getSignatureFieldQualifiedName(), QString("Third"));
}

void SignatureBuilderTest::signingOverInvalidSignature()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    PDFCertificateEntry certificate;
    QString password;
    QVERIFY(createTestCertificate(directory, certificate, password));

    PDFDocumentBuilder builder;
    builder.appendPage(QRectF(0, 0, 300, 400));
    const PDFDocument document = builder.build();

    QByteArray signedData;
    QCOMPARE(signDocument(document, QByteArray(), certificate, password, "First", signedData), PDFDocumentSigner::Result::OK);

    // Damage the first signature by changing the first byte of its value - the
    // document stays readable, but the signature cannot be decoded anymore
    const qsizetype contents = signedData.indexOf("/Contents <");
    QVERIFY(contents > 0);
    const qsizetype firstDigit = contents + qsizetype(std::strlen("/Contents <"));
    signedData[firstDigit] = signedData[firstDigit] == '3' ? '4' : '3';
    const auto damagedResults = verifySignedDocument(signedData);
    QCOMPARE(damagedResults.size(), size_t(1));
    QVERIFY(!damagedResults.front().isSignatureValid());

    // A document with an invalid signature can still be signedData, only the new
    // signature is verified
    QByteArray resigned;
    QCOMPARE(signDocument(readDocument(signedData), signedData, certificate, password, "Second", resigned), PDFDocumentSigner::Result::OK);

    const auto results = verifySignedDocument(resigned);
    QCOMPARE(results.size(), size_t(2));
    QCOMPARE(results.front().getSignatureFieldQualifiedName(), QString("First"));
    QVERIFY(!results.front().isSignatureValid());
    QCOMPARE(results.back().getSignatureFieldQualifiedName(), QString("Second"));
    QVERIFY2(results.back().isSignatureValid(), qPrintable(results.back().getErrors().join('\n')));
    QVERIFY(!results.back().hasSignatureWarning());
}

void SignatureBuilderTest::existingSignaturesNotPreserved()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    PDFCertificateEntry certificate;
    QString password;
    QVERIFY(createTestCertificate(directory, certificate, password));

    PDFDocumentBuilder builder;
    builder.appendPage(QRectF(0, 0, 300, 400));
    const PDFDocument document = builder.build();

    QByteArray signedData;
    QCOMPARE(signDocument(document, QByteArray(), certificate, password, "First", signedData), PDFDocumentSigner::Result::OK);
    const PDFDocument signedDocument = readDocument(signedData);

    // Without the original data, or with data which cannot be updated, the
    // existing signature would be damaged by writing the document, so the
    // signer must refuse instead
    QByteArray resigned;
    QCOMPARE(signDocument(signedDocument, QByteArray(), certificate, password, "Second", resigned), PDFDocumentSigner::Result::ExistingSignaturesNotPreserved);
    QVERIFY(resigned.isEmpty());
    QCOMPARE(signDocument(signedDocument, QByteArray("not a document"), certificate, password, "Second", resigned), PDFDocumentSigner::Result::ExistingSignaturesNotPreserved);
    QVERIFY(resigned.isEmpty());
}

void SignatureBuilderTest::preservesAcroForm_data()
{
    QTest::addColumn<bool>("indirectForm");
    QTest::addColumn<bool>("indirectFields");
    QTest::newRow("direct-form-direct-fields") << false << false;
    QTest::newRow("direct-form-indirect-fields") << false << true;
    QTest::newRow("indirect-form-direct-fields") << true << false;
    QTest::newRow("indirect-form-indirect-fields") << true << true;
}

void SignatureBuilderTest::preservesAcroForm()
{
    QFETCH(bool, indirectForm);
    QFETCH(bool, indirectFields);
    PDFDocumentBuilder original;
    const auto page = original.appendPage(QRectF(0, 0, 300, 400));
    const auto oldField = original.addObject(dictionary({{"FT", PDFObject::createName("Tx")},
                                                       {"T", PDFObjectFactory::createTextString("Existing")},
                                                       {"V", PDFObjectFactory::createTextString("Keep this value")}}));
    PDFObject fields = PDFObject::createArray(PDFArrayBuilder(PDFDocumentBuilder::createObjectsFromReferences({oldField})));
    if (indirectFields)
    {
        fields = PDFObject::createReference(original.addObject(fields));
    }
    const auto resources = original.addObject(dictionary({}));
    const auto flags = original.addObject(PDFObject::createInteger(8));
    const PDFObject form = dictionary({{"Fields", fields}, {"SigFlags", PDFObject::createReference(flags)},
                                      {"NeedAppearances", PDFObject::createBool(true)},
                                      {"DR", PDFObject::createReference(resources)},
                                      {"DA", PDFObjectFactory::createTextString("/Helv 12 Tf 0 g")},
                                      {"Q", PDFObject::createInteger(2)},
                                      {"XFA", PDFObjectFactory::createTextString("Preserve XFA")}});
    const PDFObjectReference formReference = indirectForm ? original.addObject(form) : PDFObjectReference();
    original.mergeTo(original.getCatalogReference(), dictionary({{"AcroForm", indirectForm ? PDFObject::createReference(formReference) : form}}));
    const PDFDocument source = original.build();
    PDFDocumentBuilder builder(&source);
    const auto signature = builder.createSignatureDictionary("Adobe.PPKLite", "adbe.pkcs7.detached", "test", QDateTime::currentDateTime(), 0);
    const auto field = builder.createSignatureField("Signature", signature, page);
    const PDFDocument document = builder.build();
    const PDFObject acroForm = document.getObjectByReference(builder.getCatalogReference()).getDictionary()->get("AcroForm");
    if (indirectForm)
    {
        QCOMPARE(acroForm.getReference(), formReference);
    }
    const PDFDictionary* result = document.getDictionaryFromObject(acroForm);
    QVERIFY(result);
    const PDFObject resultFields = document.getObject(result->get("Fields"));
    QVERIFY(resultFields.isArray());
    QCOMPARE(resultFields.getArray()->getCount(), size_t(2));
    QCOMPARE(resultFields.getArray()->getItem(0).getReference(), oldField);
    QCOMPARE(resultFields.getArray()->getItem(1).getReference(), field);
    QCOMPARE(document.getObject(result->get("SigFlags")).getInteger(), PDFInteger(11));
    for (const char* key : {"NeedAppearances", "DR", "DA", "Q", "XFA"})
    {
        QVERIFY(result->get(key) == form.getDictionary()->get(key));
    }
    QVERIFY(document.getObjectByReference(oldField) == source.getObjectByReference(oldField));
    const auto* sourceForm = source.getDictionaryFromObject(source.getObjectByReference(builder.getCatalogReference()).getDictionary()->get("AcroForm"));
    QCOMPARE(source.getObject(sourceForm->get("Fields")).getArray()->getCount(), size_t(1));
    QCOMPARE(source.getObject(sourceForm->get("SigFlags")).getInteger(), PDFInteger(8));
}

void SignatureBuilderTest::widgetStructure_data()
{
    QTest::addColumn<bool>("visible");
    QTest::addColumn<QPointF>("origin");
    QTest::addColumn<int>("rotation");
    for (bool visible : {false, true})
    {
        for (QPointF origin : {QPointF(), QPointF(100, 200), QPointF(-100, -200)})
        {
            for (int rotation : {0, 90, 180, 270})
            {
                const QByteArray name = QByteArray::number(visible) + "-" + QByteArray::number(origin.x()) + "-" + QByteArray::number(rotation);
                QTest::newRow(name.constData()) << visible << origin << rotation;
            }
        }
    }
}

void SignatureBuilderTest::widgetStructure()
{
    QFETCH(bool, visible);
    QFETCH(QPointF, origin);
    QFETCH(int, rotation);
    PDFDocumentBuilder builder;
    const auto firstPage = builder.appendPage(QRectF(0, 0, 100, 100));
    const auto page = builder.appendPage(QRectF(origin, QSizeF(300, 400)));
    builder.setPageRotation(page, static_cast<PageRotation>(rotation / 90));
    const auto annotation = builder.addObject(dictionary({{"Type", PDFObject::createName("Annot")},
                                                        {"Subtype", PDFObject::createName("Text")}}));
    builder.mergeTo(page, dictionary({{"Annots", PDFObject::createArray(PDFArrayBuilder(PDFDocumentBuilder::createObjectsFromReferences({annotation})))}}));
    // Existing /Annots may itself be indirect.
    const auto annots = builder.getStorage()->getObjectByReference(page).getDictionary()->get("Annots");
    const auto annotsReference = builder.addObject(annots);
    builder.mergeTo(page, dictionary({{"Annots", PDFObject::createReference(annotsReference)}}));
    const QRectF bbox(20, 30, 80, 40);
    const QRectF rect = bbox.translated(origin);
    const auto stream = visible ? appearance(builder, bbox) : PDFObjectReference();
    const auto signature = builder.createSignatureDictionary("Adobe.PPKLite", "adbe.pkcs7.detached", "test", QDateTime::currentDateTime(), 0);
    const auto field = builder.createSignatureField("Signature", signature, page, stream, visible ? rect : QRectF());
    PDFDocument document = builder.build();

    // Verify the serialized PDF too, including stream references and page annotations.
    QBuffer buffer;
    QVERIFY(buffer.open(QIODevice::ReadWrite));
    PDFDocumentWriter writer(nullptr);
    QVERIFY(writer.write(&buffer, &document));
    PDFDocumentReader reader(nullptr, nullptr, false, false);
    document = reader.readFromBuffer(buffer.data());
    QVERIFY2(reader.getReadingResult() == PDFDocumentReader::Result::OK, qPrintable(reader.getErrorMessage()));
    const PDFDictionary* widget = document.getObjectByReference(field).getDictionary();
    QVERIFY(widget);
    PDFDocumentDataLoaderDecorator loader(&document);
    QCOMPARE(loader.readRectangle(widget->get("Rect"), QRectF()), visible ? rect : QRectF());
    QCOMPARE(widget->get("F").getInteger(), PDFInteger(PDFAnnotation::Print));
    QCOMPARE(widget->get("P").getReference(), page);
    QCOMPARE(widget->get("V").getReference(), signature);
    QCOMPARE(widget->get("Subtype").getString(), QByteArray("Widget"));
    const auto* form = document.getDictionaryFromObject(document.getObjectByReference(builder.getCatalogReference()).getDictionary()->get("AcroForm"));
    QVERIFY(form);
    QCOMPARE(form->get("SigFlags").getInteger(), PDFInteger(3));
    QCOMPARE(document.getObject(form->get("Fields")).getArray()->getItem(0).getReference(), field);
    const PDFObject pageAnnots = document.getObject(document.getObjectByReference(page).getDictionary()->get("Annots"));
    QCOMPARE(pageAnnots.getArray()->getCount(), size_t(2));
    QCOMPARE(pageAnnots.getArray()->getItem(0).getReference(), annotation);
    QCOMPARE(pageAnnots.getArray()->getItem(1).getReference(), field);
    QVERIFY(document.getObjectByReference(firstPage).getDictionary()->get("Annots").isNull());
    if (visible)
    {
        const auto* ap = document.getDictionaryFromObject(widget->get("AP"));
        QVERIFY(ap);
        const PDFObject normal = document.getObject(ap->get("N"));
        QVERIFY(normal.isStream());
        QCOMPARE(loader.readRectangle(normal.getStream()->getDictionary()->get("BBox"), QRectF()), bbox);
    }
    else
    {
        QVERIFY(widget->get("AP").isNull());
    }
}

void SignatureBuilderTest::certification_data()
{
    QTest::addColumn<int>("certification");
    QTest::addColumn<int>("expectedPermissions");

    QTest::newRow("no-changes") << int(PDFDocumentSigner::Certification::NoChanges) << int(PDFOCRApplyProcessor::CertificationPermissions::NoChanges);
    QTest::newRow("form-filling") << int(PDFDocumentSigner::Certification::FormFilling) << int(PDFOCRApplyProcessor::CertificationPermissions::FormFilling);
    QTest::newRow("annotations") << int(PDFDocumentSigner::Certification::FormFillingAndAnnotations) << int(PDFOCRApplyProcessor::CertificationPermissions::FormFillingAndAnnotations);
}

void SignatureBuilderTest::certification()
{
    QFETCH(int, certification);
    QFETCH(int, expectedPermissions);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    PDFCertificateEntry certificate;
    QString password;
    QVERIFY(createTestCertificate(directory, certificate, password));

    PDFDocumentBuilder builder;
    builder.appendPage(QRectF(0, 0, 300, 400));
    const PDFDocument document = builder.build();

    QByteArray signedData;
    QCOMPARE(signDocument(document, QByteArray(), certificate, password, "Certification", signedData, PDFDocumentSigner::Certification(certification)), PDFDocumentSigner::Result::OK);

    const PDFDocument signedDocument = readDocument(signedData);
    QCOMPARE(int(PDFOCRApplyProcessor::getCertificationPermissions(&signedDocument)), expectedPermissions);

    // Catalog /Perms /DocMDP references the signature dictionary of the field,
    // whose only signature reference is the DocMDP transform (version 1.2)
    const PDFObject& permsObject = signedDocument.getObject(signedDocument.getCatalog()->getPerms());
    QVERIFY(permsObject.isDictionary());
    const PDFObject docMDP = permsObject.getDictionary()->get("DocMDP");
    QVERIFY(docMDP.isReference());

    const PDFForm form = PDFForm::parse(&signedDocument, signedDocument.getCatalog()->getFormObject());
    const PDFFormFieldSignature* field = nullptr;
    form.apply([&field](const PDFFormField* formField) { field = dynamic_cast<const PDFFormFieldSignature*>(formField); });
    QVERIFY(field);
    const PDFDictionary* fieldDictionary = signedDocument.getDictionaryFromObject(signedDocument.getObjectByReference(field->getSelfReference()));
    QVERIFY(fieldDictionary);
    QCOMPARE(fieldDictionary->get("V").getReference(), docMDP.getReference());

    const std::vector<PDFSignatureReference>& references = field->getSignature().getReferences();
    QCOMPARE(references.size(), size_t(1));
    QCOMPARE(references.front().getTransformMethod(), PDFSignatureReference::TransformMethod::DocMDP);
    const PDFDictionary* transformParams = signedDocument.getDictionaryFromObject(references.front().getTransformParams());
    QVERIFY(transformParams);
    QCOMPARE(transformParams->get("P").getInteger(), PDFInteger(certification));
    QCOMPARE(transformParams->get("V").getString(), QByteArray("1.2"));

    const auto results = verifySignedDocument(signedData);
    QCOMPARE(results.size(), size_t(1));
    QVERIFY2(results.front().isSignatureValid(), qPrintable(results.front().getErrors().join('\n')));
}

void SignatureBuilderTest::certificationMustBeFirst()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    PDFCertificateEntry certificate;
    QString password;
    QVERIFY(createTestCertificate(directory, certificate, password));

    PDFDocumentBuilder builder;
    builder.appendPage(QRectF(0, 0, 300, 400));
    const PDFDocument document = builder.build();

    QByteArray approved;
    QCOMPARE(signDocument(document, QByteArray(), certificate, password, "Approval", approved), PDFDocumentSigner::Result::OK);

    QByteArray certified;
    QCOMPARE(signDocument(readDocument(approved), approved, certificate, password, "Certification", certified, PDFDocumentSigner::Certification::NoChanges),
             PDFDocumentSigner::Result::CertificationNotFirst);
    QVERIFY(certified.isEmpty());

    // An approval signature after the certification keeps the certification
    QVERIFY(signDocument(document, QByteArray(), certificate, password, "Certification", certified, PDFDocumentSigner::Certification::FormFilling) == PDFDocumentSigner::Result::OK);
    QByteArray countersigned;
    QCOMPARE(signDocument(readDocument(certified), certified, certificate, password, "Countersignature", countersigned), PDFDocumentSigner::Result::OK);
    const PDFDocument countersignedDocument = readDocument(countersigned);
    QCOMPARE(PDFOCRApplyProcessor::getCertificationPermissions(&countersignedDocument), PDFOCRApplyProcessor::CertificationPermissions::FormFilling);

    // After "no changes" no approval signature is allowed: it would break the certification
    QByteArray locked;
    QCOMPARE(signDocument(document, QByteArray(), certificate, password, "Certification", locked, PDFDocumentSigner::Certification::NoChanges), PDFDocumentSigner::Result::OK);
    QByteArray refused;
    QCOMPARE(signDocument(readDocument(locked), locked, certificate, password, "Countersignature", refused), PDFDocumentSigner::Result::DocumentLockedNoChanges);
    QVERIFY(refused.isEmpty());
}

void SignatureBuilderTest::verificationStamp()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    PDFCertificateEntry certificate;
    QString password;
    QVERIFY(createTestCertificate(directory, certificate, password));

    // The entries of the certificate manager carry the parsed certificate
    certificate.info.setName(PDFCertificateInfo::CommonName, QStringLiteral("Andre Test Signer"));
    certificate.info.setCertificateData(QByteArray("DER encoded certificate"));

    const QByteArray original("%PDF-1.7 original file");
    const QDateTime time(QDate(2026, 10, 18), QTime(14, 30, 0), QTimeZone::UTC);
    const PDFSignatureStamp::Data data = PDFSignatureStamp::create(certificate.info, time, PDFDocumentSigner::Certification::NoChanges, original);

    QCOMPARE(data.signer, QStringLiteral("Andre Test Signer"));
    QCOMPARE(data.documentSha256, QCryptographicHash::hash(original, QCryptographicHash::Sha256));
    QCOMPARE(data.certificateSha256, QCryptographicHash::hash(certificate.info.getCertificateData(), QCryptographicHash::Sha256));

    const QString payload = PDFSignatureStamp::getPayload(data);
    QVERIFY(payload.contains(QStringLiteral("Signed: 2026-10-18T14:30:00Z")));
    QVERIFY(payload.contains(QStringLiteral("Lock: no changes")));
    QVERIFY(payload.contains(QString::fromLatin1(data.documentSha256.toHex().toUpper())));
    QVERIFY(payload.contains(QString::fromLatin1(data.certificateSha256.toHex().toUpper())));
    // B3: the issuer and the serial number name the certificate for a third party
    const auto realEntry = PDFCertificateManager::createEntryFromPkcs12(certificate.pkcs12, password);
    QVERIFY(realEntry);
    const PDFSignatureStamp::Data realData = PDFSignatureStamp::create(realEntry->info, time, PDFDocumentSigner::Certification::NoChanges, original);
    const QString realPayload = PDFSignatureStamp::getPayload(realData);
    QVERIFY(!realData.serialNumber.isEmpty() && realPayload.contains(QStringLiteral("Certificate serial: ") + realData.serialNumber));
    QVERIFY(!realData.issuer.isEmpty() && realPayload.contains(QStringLiteral("Issuer: ") + realData.issuer));
    QVERIFY(realData.issuerName.contains(QStringLiteral("PDF4QT")) || !realData.issuerName.contains(QStringLiteral("CN=")));
    QVERIFY(PDFSignatureStamp::getTextLines(realData).join('\n').contains(QStringLiteral("Issued by ")));
    // R2-A M3: the serial is shown in full (grouped, over lines), a long issuer is elided visibly
    PDFSignatureStamp::Data longData = realData;
    longData.serialNumber = QStringLiteral("0123456789ABCDEF0123456789ABCDEF01234567");
    longData.issuerName = QStringLiteral("LAWtrust Advanced Electronic Signature CA 2026");
    const QString longText = PDFSignatureStamp::getTextLines(longData).join(QChar(' ')).simplified().remove(QChar(' '));
    QVERIFY2(longText.contains(longData.serialNumber), qPrintable(longText));
    QVERIFY(PDFSignatureStamp::getTextLines(longData).join('\n').contains(QChar(0x2026)));
    QVERIFY(PDFSignatureStamp::getTextLines(data).join('\n').indexOf(QStringLiteral("Issued by ")) == -1);   // unreadable certificate: no line
    QVERIFY(PDFSignatureStamp::getTextLines(data).join('\n').contains(PDFSignatureStamp::formatFingerprint(data.documentSha256, 8)));
    QCOMPARE(PDFSignatureStamp::formatFingerprint(QByteArray::fromHex("0123456789abcdef")), QStringLiteral("0123 4567 89AB CDEF"));

    // A new file has no fingerprint of a previous version
    const PDFSignatureStamp::Data newFileData = PDFSignatureStamp::create(certificate.info, time, PDFDocumentSigner::Certification::None, QByteArray());
    QVERIFY(newFileData.documentSha256.isEmpty());
    QVERIFY(!PDFSignatureStamp::getPayload(newFileData).contains(QStringLiteral("File before signing")));

    const std::vector<std::vector<bool>> modules = PDFSignatureStamp::getQrModules(payload);
    QVERIFY(modules.size() >= 21 && (modules.size() - 17) % 4 == 0);

    // Drawn in PDF coordinates (y up) at 4 pixels per point
    const QSizeF size = PDFSignatureStamp::getSize(data);
    QImage image(qCeil(size.width() * 4) + 40, qCeil(size.height() * 4) + 40, QImage::Format_RGB32);
    image.fill(Qt::white);
    {
        QPainter painter(&image);
        painter.translate(20, image.height() - 20);
        painter.scale(4.0, -4.0);
        PDFSignatureStamp::draw(&painter, QRectF(QPointF(0, 0), size), data);
    }
    // Centre of the top left module of the finder pattern, inside the quiet zone of two modules
    QCOMPARE(image.pixelColor(20 + 4 * 2 + 2, 20 + 4 * 2 + 2), QColor(Qt::black));
    QCOMPARE(image.pixelColor(20 + 4 * 1 + 2, 20 + 4 * 1 + 2), QColor(Qt::white));

    // The QR code reads back to the payload, when a QR reader is installed
    const QString zbarimg = QStandardPaths::findExecutable(QStringLiteral("zbarimg"));
    if (zbarimg.isEmpty())
    {
        QSKIP("zbarimg is not installed, the QR code is not decoded");
    }
    const QString imageFile = directory.filePath(QStringLiteral("stamp.png"));
    QVERIFY(image.save(imageFile));
    QProcess process;
    process.start(zbarimg, { QStringLiteral("--raw"), QStringLiteral("-q"), QStringLiteral("-Sbinary"), imageFile });
    QVERIFY(process.waitForFinished(30000));
    QCOMPARE(QString::fromUtf8(process.readAllStandardOutput()).trimmed(), payload);
}

void SignatureBuilderTest::encryptedCertification()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    PDFCertificateEntry certificate;
    QString password;
    QVERIFY(createTestCertificate(directory, certificate, password));

    PDFDocumentBuilder builder;
    builder.appendPage(QRectF(0, 0, 300, 400));
    const PDFDocument document = builder.build();

    auto sign = [&](const PDFDocument& documentToSign, const QByteArray& original, PDFDocumentSigner::Certification certification, QByteArray& result)
    {
        PDFDocumentSigner::Parameters parameters;
        parameters.document = &documentToSign;
        parameters.originalDocumentData = original;
        parameters.certification = certification;
        parameters.encryptionUserPassword = QStringLiteral("open-me");
        parameters.signFunction = [&](const QByteArray& data, QByteArray& signature) { return PDFSignatureFactory::sign(certificate, password, data, signature); };
        parameters.createSignatureFieldFunction = [&](PDFDocumentBuilder& fieldBuilder, PDFObjectReference signatureDictionary)
        {
            return fieldBuilder.createSignatureField("Locked", signatureDictionary, documentToSign.getCatalog()->getPage(0)->getPageReference());
        };
        return PDFDocumentSigner::sign(parameters, result);
    };

    QByteArray locked;
    QCOMPARE(sign(document, QByteArray(), PDFDocumentSigner::Certification::NoChanges, locked), PDFDocumentSigner::Result::OK);

    // Without the password the document does not open
    {
        bool wrongOffered = false;
        PDFDocumentReader reader(nullptr, [&wrongOffered](bool* ok) { *ok = !wrongOffered; wrongOffered = true; return QStringLiteral("wrong"); }, false, false);
        reader.readFromBuffer(locked);
        QVERIFY(reader.getReadingResult() != PDFDocumentReader::Result::OK);
    }

    bool offered = false;
    PDFDocumentReader reader(nullptr, [&offered](bool* ok) { *ok = !offered; offered = true; return QStringLiteral("open-me"); }, false, false);
    const PDFDocument lockedDocument = reader.readFromBuffer(locked);
    QCOMPARE(reader.getReadingResult(), PDFDocumentReader::Result::OK);
    QCOMPARE(PDFOCRApplyProcessor::getCertificationPermissions(&lockedDocument), PDFOCRApplyProcessor::CertificationPermissions::NoChanges);

    const PDFSecurityHandler* securityHandler = lockedDocument.getStorage().getSecurityHandler();
    QVERIFY(securityHandler && securityHandler->getMode() != EncryptionMode::None);
    QVERIFY(securityHandler->isAllowed(PDFSecurityHandler::Permission::PrintHighResolution));
    QVERIFY(securityHandler->isAllowed(PDFSecurityHandler::Permission::CopyContent));
    QVERIFY(!securityHandler->isAllowed(PDFSecurityHandler::Permission::Modify));
    QVERIFY(!securityHandler->isAllowed(PDFSecurityHandler::Permission::ModifyInteractiveItems));

    const PDFForm form = PDFForm::parse(&lockedDocument, lockedDocument.getCatalog()->getFormObject());
    PDFSignatureHandler::Parameters verifyParameters;
    verifyParameters.useSystemCertificateStore = false;
    const auto results = PDFSignatureHandler::verifySignatures(form, locked, verifyParameters);
    QCOMPARE(results.size(), size_t(1));
    QVERIFY2(results.front().isSignatureValid(), qPrintable(results.front().getErrors().join('\n')));

    const PDFDocumentSigner::SignedFileCheck check = PDFDocumentSigner::checkSignedFile(locked, QStringLiteral("Locked"), QStringLiteral("open-me"));
    QVERIFY(check.isOK());
    QVERIFY(check.isEncrypted);
    QCOMPARE(check.certification, PDFDocumentSigner::Certification::NoChanges);
    QVERIFY(!PDFDocumentSigner::checkSignedFile(locked, QStringLiteral("Locked"), QStringLiteral("wrong")).isOpened);
    QVERIFY(!PDFDocumentSigner::checkSignedFile(locked, QStringLiteral("Other"), QStringLiteral("open-me")).isSignatureFound);

    // A changed byte inside the signed range is found
    QByteArray tampered = locked;
    const int index = tampered.indexOf("/Type");
    QVERIFY(index > 0);
    tampered[index + 1] = 't';
    QVERIFY(!PDFDocumentSigner::checkSignedFile(tampered, QStringLiteral("Locked"), QStringLiteral("open-me")).isOK());

    // The other party countersigns an encrypted contract certified "form filling and
    // signing": the password of the document opens it for the incremental update
    QByteArray lockedForOthers;
    QCOMPARE(sign(document, QByteArray(), PDFDocumentSigner::Certification::FormFilling, lockedForOthers), PDFDocumentSigner::Result::OK);
    bool othersOffered = false;
    PDFDocumentReader othersReader(nullptr, [&othersOffered](bool* ok) { *ok = !othersOffered; othersOffered = true; return QStringLiteral("open-me"); }, false, false);
    const PDFDocument lockedForOthersDocument = othersReader.readFromBuffer(lockedForOthers);
    QCOMPARE(othersReader.getReadingResult(), PDFDocumentReader::Result::OK);
    auto countersign = [&](const QString& openPassword, QByteArray& result)
    {
        PDFDocumentSigner::Parameters parameters;
        parameters.document = &lockedForOthersDocument;
        parameters.originalDocumentData = lockedForOthers;
        parameters.openPassword = openPassword;
        parameters.signFunction = [&](const QByteArray& data, QByteArray& signature) { return PDFSignatureFactory::sign(certificate, password, data, signature); };
        parameters.createSignatureFieldFunction = [&](PDFDocumentBuilder& fieldBuilder, PDFObjectReference signatureDictionary)
        {
            return fieldBuilder.createSignatureField("Countersignature", signatureDictionary, lockedForOthersDocument.getCatalog()->getPage(0)->getPageReference());
        };
        return PDFDocumentSigner::sign(parameters, result);
    };
    QByteArray countersigned;
    QVERIFY(countersign(QString(), countersigned) != PDFDocumentSigner::Result::OK);   // without the password: refused, no crash
    QCOMPARE(countersign(QStringLiteral("open-me"), countersigned), PDFDocumentSigner::Result::OK);
    QVERIFY(countersigned.startsWith(lockedForOthers));
    QVERIFY(PDFDocumentSigner::checkSignedFile(countersigned, QStringLiteral("Countersignature"), QStringLiteral("open-me")).isOK());
    const PDFDocumentSigner::SignedFileCheck first = PDFDocumentSigner::checkSignedFile(countersigned, QStringLiteral("Locked"), QStringLiteral("open-me"));
    QVERIFY(first.isSignatureValid && !first.isCoveringWholeFile);   // valid, the countersignature was added after it
    QCOMPARE(first.certification, PDFDocumentSigner::Certification::FormFilling);

    // Bytes appended after a signature are found
    QVERIFY(!PDFDocumentSigner::checkSignedFile(locked + QByteArray("\n%appended"), QStringLiteral("Locked"), QStringLiteral("open-me")).isOK());

    // A signed document cannot be encrypted, its signatures would break
    QByteArray approved;
    QCOMPARE(signDocument(document, QByteArray(), certificate, password, "Approval", approved), PDFDocumentSigner::Result::OK);
    QByteArray encrypted;
    QCOMPARE(sign(readDocument(approved), approved, PDFDocumentSigner::Certification::None, encrypted), PDFDocumentSigner::Result::ExistingSignaturesNotPreserved);
}

void SignatureBuilderTest::signatureLibrary()
{
    PDFSignatureLibrary library;
    for (int i = 0; i < PDFSignatureLibrary::SLOT_COUNT; ++i)
    {
        QVERIFY(library.getSlot(i).isEmpty());
    }
    QCOMPARE(library.getDisplayName(1), QStringLiteral("Signature 2"));

    QImage image(40, 20, QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    library.setSlot(0, { QStringLiteral("Full signature"), PDFSignatureLibrary::toPng(image) });
    library.setSlot(3, { QStringLiteral("Initials"), PDFSignatureLibrary::toPng(image) });

    PDFSignatureLibrary copy;
    QVERIFY(copy.deserialize(library.serialize()));
    for (int i = 0; i < PDFSignatureLibrary::SLOT_COUNT; ++i)
    {
        QVERIFY(copy.getSlot(i) == library.getSlot(i));
    }
    QCOMPARE(copy.getDisplayName(3), QStringLiteral("Initials"));
    QVERIFY(!copy.deserialize(QByteArray("garbage")));

    const QByteArray secret("signature bytes");
    QCOMPARE(PDFSignatureLibrary::unprotect(PDFSignatureLibrary::protect(secret)).value_or(QByteArray()), secret);
    QVERIFY(!PDFSignatureLibrary::unprotect(QByteArray("unknown")));

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString fileName = directory.filePath(QStringLiteral("sub/library.dat"));
    PDFSignatureLibrary loaded;
    QVERIFY(loaded.load(fileName)); // absent file = empty library
    QVERIFY(library.save(fileName));
#ifndef Q_OS_WIN
    QCOMPARE(QFileInfo(fileName).permissions() & (QFile::ReadGroup | QFile::ReadOther), QFile::Permissions());
#endif
    QVERIFY(loaded.load(fileName));
    QVERIFY(loaded.getSlot(0) == library.getSlot(0));

    QFile damaged(fileName);
    QVERIFY(damaged.open(QFile::WriteOnly | QFile::Truncate));
    damaged.write("PLAIN1 not a library");
    damaged.close();
    QString errorMessage;
    QVERIFY(!loaded.load(fileName, &errorMessage));
    QVERIFY(!errorMessage.isEmpty());
}

void SignatureBuilderTest::signatureExtraction()
{
    // A "scan": off-white paper with noise and a dark blue stroke
    QImage scan(400, 200, QImage::Format_RGB32);
    QRandomGenerator random(7);
    for (int y = 0; y < scan.height(); ++y)
    {
        for (int x = 0; x < scan.width(); ++x)
        {
            const int level = 225 + int(random.bounded(20));
            scan.setPixel(x, y, qRgb(level, level, level - 10));
        }
    }
    {
        QPainter painter(&scan);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(QColor(20, 30, 120), 6));
        painter.drawLine(100, 80, 300, 120);
    }

    const QImage signature = PDFSignatureLibrary::extractSignature(scan);
    QVERIFY(!signature.isNull());
    QVERIFY(signature.hasAlphaChannel());

    // Cropped to the stroke plus a small margin
    QVERIFY(qAbs(signature.width() - 206) <= 12);
    QVERIFY(qAbs(signature.height() - 46) <= 12);

    // Paper is transparent, ink is opaque and keeps its colour
    QCOMPARE(qAlpha(signature.pixel(0, signature.height() - 1)), 0);
    const QRgb ink = signature.pixel(signature.width() / 2, signature.height() / 2);
    QCOMPARE(qAlpha(ink), 255);
    QVERIFY(qBlue(ink) > qRed(ink));

    // The ink colour can be replaced, blank paper gives no signature
    const QImage black = PDFSignatureLibrary::extractSignature(scan, -1, Qt::black);
    QCOMPARE(QColor(black.pixel(black.width() / 2, black.height() / 2)).rgb() & 0xFFFFFF, QRgb(0));
    QImage blank(100, 100, QImage::Format_RGB32);
    blank.fill(Qt::white);
    QVERIFY(PDFSignatureLibrary::extractSignature(blank, 128).isNull());

    const int threshold = PDFSignatureLibrary::getAutomaticThreshold(scan.convertToFormat(QImage::Format_Grayscale8));
    QVERIFY(threshold > 40 && threshold < 225);

    // A clean scan with two levels only: the threshold lies in the middle between them
    QImage clean(100, 50, QImage::Format_Grayscale8);
    clean.fill(230);
    for (int x = 20; x < 80; ++x)
    {
        clean.setPixel(x, 25, 20);
    }
    const int cleanThreshold = PDFSignatureLibrary::getAutomaticThreshold(clean);
    QVERIFY2(cleanThreshold > 80 && cleanThreshold < 170, qPrintable(QString::number(cleanThreshold)));
    const QImage cleanSignature = PDFSignatureLibrary::extractSignature(clean);
    QVERIFY(!cleanSignature.isNull());
    QCOMPARE(qAlpha(cleanSignature.pixel(cleanSignature.width() / 2, cleanSignature.height() / 2)), 255);
}

void SignatureBuilderTest::initialsOnEveryPage()
{
    // Page 0 upright, page 1 turned by /Rotate 90, page 2 skipped
    PDFDocumentBuilder builder;
    builder.appendPage(QRectF(0, 0, 300, 400));
    const PDFObjectReference rotated = builder.appendPage(QRectF(0, 0, 300, 400));
    builder.setPageRotation(rotated, PageRotation::Rotate90);
    builder.appendPage(QRectF(0, 0, 300, 400));
    const PDFDocument document = builder.build();

    QImage initials(100, 50, QImage::Format_ARGB32);
    initials.fill(Qt::red);

    PDFPageInitials::Settings settings;
    settings.width = 60;
    settings.margin = 20;
    settings.skippedPages = { 2 };

    // Rectangle in the displayed page: bottom right corner, the height follows the image
    const QRectF upright = PDFPageInitials::getDisplayRectangle(document.getCatalog()->getPage(0), initials.size(), settings);
    QCOMPARE(upright, QRectF(220, 20, 60, 30));
    const QRectF turned = PDFPageInitials::getDisplayRectangle(document.getCatalog()->getPage(1), initials.size(), settings);
    QCOMPARE(turned, QRectF(320, 20, 60, 30));

    const PDFDocumentPointer initialed = PDFPageInitials::apply(&document, initials, settings);
    QVERIFY(initialed);

    // Rendered as the reader sees the pages (y down in the image), the red block is
    // at the bottom right corner of each initialed page and absent on the skipped one
    auto render = [&](PDFInteger pageIndex, QSize size)
    {
        PDFCMSGeneric cms;
        PDFFontCache fontCache(32, 32);
        PDFICCProfileCache iccProfileCache;
        PDFOptionalContentActivity activity(initialed.get(), OCUsage::View, nullptr);
        fontCache.setDocument(PDFModifiedDocument(initialed.get(), &activity));
        iccProfileCache.setDocument(PDFModifiedDocument(initialed.get(), &activity));
        PDFRenderer renderer(initialed.get(), &fontCache, &iccProfileCache, &cms, &activity, PDFRenderer::Features(), PDFMeshQualitySettings());
        QImage image(size, QImage::Format_RGB888);
        image.fill(Qt::white);
        QPainter painter(&image);
        renderer.render(&painter, QRectF(QPointF(0, 0), size), pageIndex);
        painter.end();
        return image;
    };
    auto isRed = [](QColor color) { return color.red() > 200 && color.green() < 60 && color.blue() < 60; };

    const QImage page0 = render(0, QSize(300, 400));
    QVERIFY(isRed(page0.pixelColor(250, 400 - 35)));    // inside the block
    QVERIFY(!isRed(page0.pixelColor(250, 400 - 60)));   // above it
    QVERIFY(!isRed(page0.pixelColor(50, 400 - 35)));    // bottom left stays empty

    const QImage page1 = render(1, QSize(400, 300));
    QVERIFY(isRed(page1.pixelColor(350, 300 - 35)));
    QVERIFY(!isRed(page1.pixelColor(350, 300 - 60)));
    QVERIFY(!isRed(page1.pixelColor(50, 300 - 35)));

    const QImage page2 = render(2, QSize(300, 400));
    QVERIFY(!isRed(page2.pixelColor(250, 400 - 35)));

    // Signature images fitted into areas (signature lines): aspect kept, on the line
    const PDFPage* uprightPage = document.getCatalog()->getPage(0);
    QCOMPARE(PDFPageInitials::getFittedDisplayRectangle(uprightPage, QRectF(100, 200, 200, 36), initials.size()), QRectF(100, 200, 72, 36));
    QCOMPARE(PDFPageInitials::getFittedDisplayRectangle(uprightPage, QRectF(100, 200, 40, 36), initials.size()), QRectF(100, 200, 40, 20));
    const PDFDocumentPointer drawn = PDFPageInitials::drawImages(&document, initials, { { 0, QRectF(100, 200, 200, 36) }, { 1, QRectF(50, 100, 60, 30) } });
    QVERIFY(drawn);
    QVERIFY(!PDFPageInitials::drawImages(&document, initials, { }));

    // Every page skipped: nothing to change
    settings.skippedPages = { 0, 1, 2 };
    QVERIFY(!PDFPageInitials::apply(&document, initials, settings));

    // Resources inherited from the page tree stay available to the old content
    PDFDocumentBuilder inheritedBuilder;
    inheritedBuilder.appendPage(QRectF(0, 0, 300, 400));
    const PDFDocument plain = inheritedBuilder.build();
    const PDFObjectReference pagesReference = plain.getCatalog()->getPage(0)->getPageReference();
    const PDFObject pageObject = plain.getObjectByReference(pagesReference);
    const PDFObjectReference parentReference = pageObject.getDictionary()->get("Parent").getReference();
    PDFObjectFactory resources;
    resources.beginDictionary();
    resources.beginDictionaryItem("Resources");
    resources.beginDictionary();
    resources.beginDictionaryItem("Font");
    resources.beginDictionary();
    resources.beginDictionaryItem("F9");
    resources << WrapName("Inherited");
    resources.endDictionaryItem();
    resources.endDictionary();
    resources.endDictionaryItem();
    resources.endDictionary();
    resources.endDictionaryItem();
    resources.endDictionary();
    inheritedBuilder.mergeTo(parentReference, resources.takeObject());
    const PDFDocument inherited = inheritedBuilder.build();
    settings.skippedPages.clear();
    const PDFDocumentPointer inheritedInitialed = PDFPageInitials::apply(&inherited, initials, settings);
    QVERIFY(inheritedInitialed);
    const PDFDictionary* pageResources = inheritedInitialed->getDictionaryFromObject(inheritedInitialed->getObject(inheritedInitialed->getCatalog()->getPage(0)->getResources()));
    QVERIFY(pageResources);
    const PDFDictionary* fonts = inheritedInitialed->getDictionaryFromObject(inheritedInitialed->getObject(pageResources->get("Font")));
    QVERIFY(fonts && fonts->hasKey("F9"));
    QVERIFY(pageResources->hasKey("XObject"));
}

void SignatureBuilderTest::signatureLineLabels()
{
    QVERIFY(PDFSignatureLineFinder::isSignatureLabel(QStringLiteral("Signature:")));
    QVERIFY(PDFSignatureLineFinder::isSignatureLabel(QStringLiteral("Signed by the Seller")));
    QVERIFY(PDFSignatureLineFinder::isSignatureLabel(QStringLiteral("Witness 1")));
    QVERIFY(PDFSignatureLineFinder::isSignatureLabel(QStringLiteral("Handtekening van Koper")));
    QVERIFY(PDFSignatureLineFinder::isSignatureLabel(QStringLiteral("Getuie")));
    QVERIFY(PDFSignatureLineFinder::isSignatureLabel(QStringLiteral("Initials")));
    QVERIFY(!PDFSignatureLineFinder::isSignatureLabel(QStringLiteral("Name of signatory")));
    QVERIFY(!PDFSignatureLineFinder::isSignatureLabel(QStringLiteral("Datum")));
    QVERIFY(!PDFSignatureLineFinder::isSignatureLabel(QStringLiteral("Purchase price")));
    // Attestation clause, signature next to the date, parties (South African contracts)
    QVERIFY(!PDFSignatureLineFinder::isSignatureLabel(QStringLiteral("Signed at Rustenburg on this")));
    QVERIFY(!PDFSignatureLineFinder::isSignatureLabel(QStringLiteral("day of October 2026")));
    QVERIFY(!PDFSignatureLineFinder::isSignatureLabel(QStringLiteral("Geteken te Rustenburg op hierdie")));
    QVERIFY(PDFSignatureLineFinder::isSignatureLabel(QStringLiteral("Signature & Date")));
    QVERIFY(PDFSignatureLineFinder::isSignatureLabel(QStringLiteral("Name and Signature")));
    QVERIFY(PDFSignatureLineFinder::isSignatureLabel(QStringLiteral("For the Purchaser:")));
    QVERIFY(PDFSignatureLineFinder::isSignatureLabel(QStringLiteral("For and on behalf of the Seller")));
    QVERIFY(PDFSignatureLineFinder::isSignatureLabel(QStringLiteral("Vir die Koper:")));
    // Review R0 verification N1
    QVERIFY(PDFSignatureLineFinder::isSignatureLabel(QStringLiteral("Signed on behalf of the Seller")));
    QVERIFY(PDFSignatureLineFinder::isSignatureLabel(QStringLiteral("Signature / Date")));
    QVERIFY(PDFSignatureLineFinder::isSignatureLabel(QStringLiteral("Initials")));
    QVERIFY(!PDFSignatureLineFinder::isSignatureLabel(QStringLiteral("Date of signature")));
    QVERIFY(!PDFSignatureLineFinder::isSignatureLabel(QStringLiteral("Place of signature")));
    QVERIFY(!PDFSignatureLineFinder::isSignatureLabel(QStringLiteral("Signature date")));
    QVERIFY(!PDFSignatureLineFinder::isSignatureLabel(QStringLiteral("Signed on 18 October 2026")));
    QVERIFY(!PDFSignatureLineFinder::isSignatureLabel(QStringLiteral("Initials and Surname")));
    QVERIFY(!PDFSignatureLineFinder::isSignatureLabel(QStringLiteral("Surname and initials")));
}

void SignatureBuilderTest::signatureLineFinder()
{
    PDFDocumentBuilder builder;
    const PDFObjectReference page1 = builder.appendPage(QRectF(0, 0, 595, 842));
    const PDFObjectReference page2 = builder.appendPage(QRectF(0, 0, 595, 842));
    {
        PDFPageContentStreamBuilder content(&builder, PDFContentStreamBuilder::CoordinateSystem::Qt, PDFPageContentStreamBuilder::Mode::Replace);
        QPainter* painter = content.begin(page1);
        QFont font("DejaVu Sans");
        font.setPixelSize(11);
        painter->setFont(font);
        painter->drawText(QPointF(60, 100), QStringLiteral("Signed at Rustenburg on 18 October 2026."));
        painter->drawText(QPointF(60, 200), QStringLiteral("Signature: ________________________"));
        painter->drawText(QPointF(60, 240), QStringLiteral("Name: ________________________"));
        painter->drawText(QPointF(60, 320), QStringLiteral("________________________"));
        painter->drawText(QPointF(60, 336), QStringLiteral("Witness"));
        painter->drawText(QPointF(60, 400), QStringLiteral("Date: ______________"));
        painter->drawText(QPointF(60, 480), QStringLiteral("Handtekening:"));
        content.end(painter);
    }
    {
        PDFPageContentStreamBuilder content(&builder, PDFContentStreamBuilder::CoordinateSystem::Qt, PDFPageContentStreamBuilder::Mode::Replace);
        QPainter* painter = content.begin(page2);
        QFont font("DejaVu Sans");
        font.setPixelSize(11);
        painter->setFont(font);
        painter->drawText(QPointF(60, 700), QStringLiteral("Signature ______________________"));
        content.end(painter);
    }
    // An unsigned signature field over the line of page 2 (y up: 842 - 700 = 142)
    const PDFObjectReference buyerField = builder.createSignatureField(QStringLiteral("Buyer"), PDFObjectReference(), page2);
    PDFObjectFactory rectangle;
    rectangle.beginDictionary();
    rectangle.beginDictionaryItem("Rect");
    rectangle << QRectF(60, 140, 180, 40);
    rectangle.endDictionaryItem();
    rectangle.endDictionary();
    builder.mergeTo(buyerField, rectangle.takeObject());
    const PDFDocument document = builder.build();

    const std::vector<PDFSignatureLineFinder::SignatureLine> lines = PDFSignatureLineFinder::find(&document);
    QStringList found;
    for (const auto& line : lines)
    {
        found << QString("%1:%2:%3").arg(line.pageIndex).arg(int(line.source)).arg(line.label);
    }
    QVERIFY2(lines.size() == size_t(4), qPrintable(found.join(" | ")));

    // Page 1 from the top: the "Signature:" line, the line over "Witness", the label alone
    QCOMPARE(lines[0].pageIndex, PDFInteger(0));
    QCOMPARE(lines[0].source, PDFSignatureLineFinder::Source::Line);
    QVERIFY2(lines[0].label.startsWith(QStringLiteral("Signature")), qPrintable(found.join(" | ")));
    QVERIFY(qAbs(lines[0].rectangle.top() - (842 - 200)) < 6);     // sits on the baseline
    QVERIFY(lines[0].rectangle.left() > 100);                     // right of the label
    QCOMPARE(lines[0].rectangle.height(), PDFSignatureLineFinder::SIGNATURE_HEIGHT);

    QCOMPARE(lines[1].source, PDFSignatureLineFinder::Source::Line);
    QCOMPARE(lines[1].label, QStringLiteral("Witness"));
    QCOMPARE(lines[2].source, PDFSignatureLineFinder::Source::Label);
    QVERIFY(lines[2].rectangle.left() > 100);

    // Page 2: the field, the line under it is the same place
    QCOMPARE(lines[3].pageIndex, PDFInteger(1));
    QCOMPARE(lines[3].source, PDFSignatureLineFinder::Source::SignatureField);
    QCOMPARE(lines[3].fieldName, QStringLiteral("Buyer"));

    // A page turned by /Rotate 90: the text reads horizontally on the screen, the line
    // is found as a wide area on the displayed page
    PDFDocumentBuilder rotatedBuilder;
    const PDFObjectReference rotatedPage = rotatedBuilder.appendPage(QRectF(0, 0, 595, 842));
    rotatedBuilder.setPageRotation(rotatedPage, PageRotation::Rotate90);
    {
        PDFPageContentStreamBuilder content(&rotatedBuilder, PDFContentStreamBuilder::CoordinateSystem::Qt, PDFPageContentStreamBuilder::Mode::Replace);
        QPainter* painter = content.begin(rotatedPage);
        QFont font("DejaVu Sans");
        font.setPixelSize(11);
        painter->setFont(font);
        // Displayed point (x, y up) is page point (595 - y, x); Qt y = 842 - page y
        painter->translate(595 - 300, 842 - 100);
        painter->rotate(-90);
        painter->drawText(QPointF(0, 0), QStringLiteral("Signature: ________________________"));
        content.end(painter);
    }
    const PDFDocument rotated = rotatedBuilder.build();
    const std::vector<PDFSignatureLineFinder::SignatureLine> rotatedLines = PDFSignatureLineFinder::find(&rotated);
    QCOMPARE(rotatedLines.size(), size_t(1));
    const QRectF displayed = PDFPageInitials::getDisplayToPageTransform(rotated.getCatalog()->getPage(0)).inverted().mapRect(rotatedLines[0].rectangle);
    QVERIFY2(displayed.width() > 100 && qAbs(displayed.height() - PDFSignatureLineFinder::SIGNATURE_HEIGHT) < 0.01 && qAbs(displayed.top() - 300) < 6,
             qPrintable(QString("%1 %2 %3 %4").arg(displayed.left()).arg(displayed.top()).arg(displayed.width()).arg(displayed.height())));
}

void SignatureBuilderTest::contractPack()
{
    // Six pages: two contracts of three pages, each numbered "Page n of 3"
    PDFDocumentBuilder builder;
    for (int i = 0; i < 6; ++i)
    {
        const PDFObjectReference page = builder.appendPage(QRectF(0, 0, 595, 842));
        PDFPageContentStreamBuilder content(&builder, PDFContentStreamBuilder::CoordinateSystem::Qt, PDFPageContentStreamBuilder::Mode::Replace);
        QPainter* painter = content.begin(page);
        QFont font("DejaVu Sans");
        font.setPixelSize(10);
        painter->setFont(font);
        painter->drawText(QPointF(60, 80), i < 3 ? QStringLiteral("SALE AGREEMENT") : QStringLiteral("LEASE AGREEMENT"));
        painter->drawText(QPointF(260, 820), QString("Page %1 of 3").arg(i % 3 + 1));
        if (i % 3 == 2)
        {
            painter->drawText(QPointF(60, 600), QStringLiteral("Signature: ________________________"));
        }
        content.end(painter);
    }
    // A form field on the first page belongs to the first contract only
    const PDFObjectReference saleField = builder.createSignatureField(QStringLiteral("Sale buyer"), PDFObjectReference(), builder.build().getCatalog()->getPage(0)->getPageReference());
    Q_UNUSED(saleField);
    const PDFDocument document = builder.build();

    using Part = PDFContractPack::Part;
    const PDFContractPack::Parts suggested = PDFContractPack::suggestFromPageNumbers(&document);
    QCOMPARE(suggested.size(), size_t(2));
    QCOMPARE(suggested[0].firstPage, PDFInteger(0));
    QCOMPARE(suggested[0].lastPage, PDFInteger(2));
    QCOMPARE(suggested[1].firstPage, PDFInteger(3));
    QCOMPARE(suggested[1].lastPage, PDFInteger(5));
    QVERIFY(PDFContractPack::suggestFromOutline(&document).empty());
    QCOMPARE(PDFContractPack::toText(suggested), QStringLiteral("1-3, 4-6"));

    QString errorMessage;
    QCOMPARE(PDFContractPack::parse(QStringLiteral("1-3, 4-6"), 6, &errorMessage).size(), size_t(2));
    QVERIFY(errorMessage.isEmpty());
    QVERIFY(PDFContractPack::parse(QStringLiteral("1-3, 3-6"), 6, &errorMessage).empty());
    QVERIFY(!errorMessage.isEmpty());
    QVERIFY(PDFContractPack::parse(QStringLiteral("1-7"), 6, &errorMessage).empty());
    QVERIFY(PDFContractPack::parse(QStringLiteral("a-b"), 6, &errorMessage).empty());

    const PDFContractPack::Parts fromStarts = PDFContractPack::fromStartPages({ 4, 2, 2 }, 6, { QStringLiteral("Lease"), QStringLiteral("Sale"), QStringLiteral("dup") });
    QCOMPARE(fromStarts.size(), size_t(3));
    QCOMPARE(fromStarts[0], (Part{ QStringLiteral("Part 1"), 0, 1 }));
    QCOMPARE(fromStarts[1], (Part{ QStringLiteral("Sale"), 2, 3 }));
    QCOMPARE(fromStarts[2], (Part{ QStringLiteral("Lease"), 4, 5 }));
    QCOMPARE(PDFContractPack::getFileName(QStringLiteral("Pack"), Part{ QStringLiteral("Sale: A/B"), 0, 1 }, 0), QStringLiteral("Pack - 01 Sale A B.pdf"));

    QCOMPARE(PDFContractPack::getFileName(QStringLiteral("Pack"), Part{ QString("Lease") + QChar(0x202E) + QStringLiteral("fdp.exe"), 0, 1 }, 1), QStringLiteral("Pack - 02 Lease fdp.exe.pdf"));
    const PDFDocument sale = PDFContractPack::extract(&document, suggested[0]);
    auto fieldCount = [](const PDFDocument& part)
    {
        const PDFForm form = PDFForm::parse(&part, part.getCatalog()->getFormObject());
        return form.getFormFields().size();
    };
    QCOMPARE(fieldCount(sale), size_t(1));

    // The second contract on its own, signed and certified with a visible signature
    const PDFDocument lease = PDFContractPack::extract(&document, suggested[1]);
    QCOMPARE(fieldCount(lease), size_t(0));
    QCOMPARE(lease.getCatalog()->getPageCount(), size_t(3));
    const std::vector<PDFSignatureLineFinder::SignatureLine> lines = PDFSignatureLineFinder::find(&lease);
    QCOMPARE(lines.size(), size_t(1));
    QCOMPARE(lines[0].pageIndex, PDFInteger(2));

    // A line left to the other party becomes an empty signature field at its place
    const PDFDocumentPointer withField = PDFSignatureLineFinder::addSignatureFields(&lease, lines, QStringLiteral("Other party "));
    QVERIFY(withField);
    const std::vector<PDFSignatureLineFinder::SignatureLine> fieldLines = PDFSignatureLineFinder::find(withField.data());
    QCOMPARE(fieldLines.size(), size_t(1));    // the line and its field are one place
    QCOMPARE(fieldLines[0].source, PDFSignatureLineFinder::Source::SignatureField);
    QCOMPARE(fieldLines[0].fieldName, QStringLiteral("Other party 1"));
    QVERIFY(fieldLines[0].rectangle.intersects(lines[0].rectangle));
    QVERIFY(!PDFSignatureLineFinder::addSignatureFields(withField.data(), fieldLines, QStringLiteral("x")));   // fields are not added twice
    QVERIFY(!PDFDocumentSigner::hasExistingSignatures(withField.data()));

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    PDFCertificateEntry certificate;
    QString password;
    QVERIFY(createTestCertificate(directory, certificate, password));
    certificate.info.setName(PDFCertificateInfo::CommonName, QStringLiteral("Batch Signer"));

    QImage signature(120, 40, QImage::Format_ARGB32);
    signature.fill(Qt::transparent);
    QPainter(&signature).fillRect(10, 10, 100, 20, Qt::darkBlue);

    PDFDocumentSigner::Parameters parameters;
    parameters.document = &lease;
    parameters.certification = PDFDocumentSigner::Certification::NoChanges;
    parameters.signFunction = [&](const QByteArray& data, QByteArray& result) { return PDFSignatureFactory::sign(certificate, password, data, result); };
    parameters.createSignatureFieldFunction = PDFSignatureStamp::createVisibleSignatureField(&lease, QStringLiteral("Lease signature"), lines[0].pageIndex, lines[0].rectangle, signature,
                                                                                            PDFSignatureStamp::create(certificate.info, parameters.signingTime, parameters.certification, QByteArray()));
    QByteArray signedLease;
    QCOMPARE(PDFDocumentSigner::sign(parameters, signedLease), PDFDocumentSigner::Result::OK);

    const PDFDocumentSigner::SignedFileCheck check = PDFDocumentSigner::checkSignedFile(signedLease, QStringLiteral("Lease signature"), QString());
    QVERIFY(check.isOK());
    QCOMPARE(check.certification, PDFDocumentSigner::Certification::NoChanges);

    // The field is visible on the page of the line, with an appearance stream
    const PDFDocument signedDocument = readDocument(signedLease);
    const PDFForm form = PDFForm::parse(&signedDocument, signedDocument.getCatalog()->getFormObject());
    bool hasAppearance = false;
    form.apply([&](const PDFFormField* field)
    {
        for (const PDFFormWidget& widget : field->getWidgets())
        {
            const PDFDictionary* widgetDictionary = signedDocument.getDictionaryFromObject(signedDocument.getObjectByReference(widget.getWidget()));
            hasAppearance = hasAppearance || (widgetDictionary && widgetDictionary->hasKey("AP") &&
                                              signedDocument.getCatalog()->getPageIndexFromPageReference(widget.getPage()) == 2);
        }
    });
    QVERIFY(hasAppearance);
}

void SignatureBuilderTest::sizeReduction()
{
    // A page with a large noisy photo, as a phone scan of a contract page
    QImage photo(2480, 3508, QImage::Format_RGB32);
    QRandomGenerator random(11);
    for (int y = 0; y < photo.height(); ++y)
    {
        QRgb* line = reinterpret_cast<QRgb*>(photo.scanLine(y));
        for (int x = 0; x < photo.width(); ++x)
        {
            const int level = 200 + int(random.bounded(40));
            line[x] = qRgb(level, level - int(random.bounded(10)), level - 20);
        }
    }

    PDFDocumentBuilder builder;
    const PDFObjectReference page = builder.appendPage(QRectF(0, 0, 595, 842));
    {
        PDFPageContentStreamBuilder content(&builder, PDFContentStreamBuilder::CoordinateSystem::Qt, PDFPageContentStreamBuilder::Mode::Replace);
        QPainter* painter = content.begin(page);
        painter->drawImage(QRectF(0, 0, 595, 842), photo);
        content.end(painter);
    }
    const PDFDocument document = builder.build();

    const qint64 originalSize = PDFSizeReducer::getWrittenSize(&document);
    QVERIFY(originalSize > 0);
    const PDFDocument email = PDFSizeReducer::reduce(&document, PDFSizeReducer::Preset::Email);
    const PDFDocument smallest = PDFSizeReducer::reduce(&document, PDFSizeReducer::Preset::Smallest);
    const qint64 emailSize = PDFSizeReducer::getWrittenSize(&email);
    const qint64 smallestSize = PDFSizeReducer::getWrittenSize(&smallest);
    QVERIFY2(emailSize > 0 && emailSize < originalSize / 2, qPrintable(QString("%1 -> %2").arg(originalSize).arg(emailSize)));
    QVERIFY2(smallestSize <= emailSize, qPrintable(QString("%1 -> %2").arg(emailSize).arg(smallestSize)));
    QCOMPARE(email.getCatalog()->getPageCount(), size_t(1));
    QVERIFY(!PDFSizeReducer::getPresetName(PDFSizeReducer::Preset::Print).isEmpty());
}

void SignatureBuilderTest::officeExport()
{
    double value = 0.0;
    QVERIFY(PDFOfficeExporter::parseNumber(QStringLiteral("1 234,56"), value) && qFuzzyCompare(value, 1234.56));
    QVERIFY(PDFOfficeExporter::parseNumber(QStringLiteral("1,234.56"), value) && qFuzzyCompare(value, 1234.56));
    QVERIFY(PDFOfficeExporter::parseNumber(QStringLiteral("R 250.00"), value) && qFuzzyCompare(value, 250.0));
    // Review R0 verification N7: codes and ID numbers in groups stay text
    QVERIFY(!PDFOfficeExporter::parseNumber(QStringLiteral("012 345"), value));
    QVERIFY(!PDFOfficeExporter::parseNumber(QStringLiteral("62 123 456 789"), value));
    QVERIFY(!PDFOfficeExporter::parseNumber(QStringLiteral("1 234 567 890 123"), value));
    QVERIFY(!PDFOfficeExporter::parseNumber(QStringLiteral("0123.45"), value));
    QVERIFY(PDFOfficeExporter::parseNumber(QStringLiteral("0.125"), value) && qFuzzyCompare(value, 0.125));
    QVERIFY(PDFOfficeExporter::parseNumber(QStringLiteral("-12"), value) && qFuzzyCompare(value, -12.0));
    QVERIFY(PDFOfficeExporter::parseNumber(QStringLiteral("1,000,000"), value) && qFuzzyCompare(value, 1000000.0));
    QVERIFY(!PDFOfficeExporter::parseNumber(QStringLiteral("Clay bricks"), value));
    QVERIFY(!PDFOfficeExporter::parseNumber(QStringLiteral("2026-10-18"), value));
    QVERIFY(PDFOfficeExporter::parseNumber(QStringLiteral("0.125"), value) && qFuzzyCompare(value, 0.125));
    QVERIFY(PDFOfficeExporter::parseNumber(QStringLiteral("1,5"), value) && qFuzzyCompare(value, 1.5));
    QVERIFY(PDFOfficeExporter::parseNumber(QStringLiteral("2 450,00"), value) && qFuzzyCompare(value, 2450.0));
    QVERIFY(PDFOfficeExporter::parseNumber(QStringLiteral("1.234,50"), value) && qFuzzyCompare(value, 1234.5));
    QVERIFY(PDFOfficeExporter::parseNumber(QStringLiteral("-R 250.00"), value) && qFuzzyCompare(value, -250.0));
    QVERIFY(!PDFOfficeExporter::parseNumber(QStringLiteral("3.142"), value));          // decimal or thousands: text
    QVERIFY(!PDFOfficeExporter::parseNumber(QStringLiteral("12.500"), value));
    QVERIFY(!PDFOfficeExporter::parseNumber(QStringLiteral("18.10.2026"), value));     // a date
    QVERIFY(!PDFOfficeExporter::parseNumber(QStringLiteral("1.2.3"), value));
    QVERIFY(!PDFOfficeExporter::parseNumber(QStringLiteral("011 234 5678"), value));   // phone numbers
    QVERIFY(!PDFOfficeExporter::parseNumber(QStringLiteral("0821234567"), value));
    QVERIFY(!PDFOfficeExporter::parseNumber(QStringLiteral("8601015009087"), value));  // an ID number

    // A page with a title and a small table of three columns
    PDFDocumentBuilder builder;
    const PDFObjectReference page = builder.appendPage(QRectF(0, 0, 595, 842));
    {
        PDFPageContentStreamBuilder content(&builder, PDFContentStreamBuilder::CoordinateSystem::Qt, PDFPageContentStreamBuilder::Mode::Replace);
        QPainter* painter = content.begin(page);
        QFont font("DejaVu Sans");
        font.setPixelSize(10);
        painter->setFont(font);
        painter->drawText(QPointF(60, 60), QStringLiteral("Quotation & delivery <2026>"));
        painter->drawText(QPointF(60, 760), QString("Bad") + QChar(0xFFFE) + QChar(0xD800) + QStringLiteral("chars"));
        const char* rows[][3] = { { "Item", "Qty", "Price" }, { "Clay bricks", "1 000", "2 450,00" }, { "Cement", "20", "1 790,00" } };
        for (int row = 0; row < 3; ++row)
        {
            painter->drawText(QPointF(60, 100 + row * 16), QString::fromLatin1(rows[row][0]));
            painter->drawText(QPointF(260, 100 + row * 16), QString::fromLatin1(rows[row][1]));
            painter->drawText(QPointF(400, 100 + row * 16), QString::fromLatin1(rows[row][2]));
        }
        content.end(painter);
    }
    const PDFDocument document = builder.build();

    const PDFOfficeExporter::Pages pages = PDFOfficeExporter::extract(&document);
    QCOMPARE(pages.size(), size_t(1));
    QStringList found;
    for (const auto& line : pages[0])
    {
        found << line.cells.join(" | ");
    }
    QVERIFY2(pages[0].size() == 5, qPrintable(found.join("\n")));
    QCOMPARE(pages[0][0].text, QStringLiteral("Quotation & delivery <2026>"));
    QCOMPARE(pages[0][2].cells, (QStringList{ QStringLiteral("Clay bricks"), QStringLiteral("1 000"), QStringLiteral("2 450,00") }));

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString docx = directory.filePath(QStringLiteral("export.docx"));
    const QString xlsx = directory.filePath(QStringLiteral("export.xlsx"));
    QVERIFY(PDFOfficeExporter::writeDocx(docx, pages));
    QVERIFY(PDFOfficeExporter::writeXlsx(xlsx, pages));

    // Valid ZIP containers with well-formed XML and the expected content
    const QString python = QStandardPaths::findExecutable(QStringLiteral("python3"));
    if (python.isEmpty())
    {
        QSKIP("python3 is not installed, the containers are not validated");
    }
    const QString script = QStringLiteral(
        "import sys, zipfile, xml.dom.minidom\n"
        "for name in sys.argv[1:]:\n"
        "    z = zipfile.ZipFile(name)\n"
        "    assert z.testzip() is None\n"
        "    for entry in z.namelist():\n"
        "        xml.dom.minidom.parseString(z.read(entry))\n"
        "d = zipfile.ZipFile(sys.argv[1]).read('word/document.xml').decode()\n"
        "assert 'Quotation &amp; delivery &lt;2026&gt;' in d and '<w:tab/>' in d, d\n"
        "x = zipfile.ZipFile(sys.argv[2]).read('xl/worksheets/sheet1.xml').decode()\n"
        "assert '<c r=\"C3\"><v>2450</v></c>' in x and '<c r=\"B3\"><v>1000</v></c>' in x and 'Clay bricks' in x, x\n"
        "print('ok')\n");
    QProcess process;
    process.start(python, { QStringLiteral("-I"), QStringLiteral("-c"), script, docx, xlsx });
    QVERIFY(process.waitForFinished(30000));
    QVERIFY2(process.exitCode() == 0, qPrintable(QString::fromUtf8(process.readAllStandardError())));
}

void SignatureBuilderTest::textRedline()
{
    auto words = [](const QString& text, PDFInteger page = 0)
    {
        PDFTextRedline::Words result;
        for (const QString& word : text.split(' ', Qt::SkipEmptyParts))
        {
            result.push_back({ word, page });
        }
        return result;
    };

    const PDFTextRedline::Words oldWords = words(QStringLiteral("The lease is for thirty (30) days and the rent is R 5 000 per month payable in advance. The tenant pays water."));
    const PDFTextRedline::Words newWords = words(QStringLiteral("The lease is for sixty (60) days and the rent is R 5 000 per month payable in advance by EFT. The tenant pays."));
    const PDFTextRedline::Changes changes = PDFTextRedline::compare(oldWords, newWords);
    QStringList found;
    for (const auto& change : changes)
    {
        found << QString("%1:[%2]->[%3]").arg(int(change.type)).arg(change.oldText, change.newText);
    }
    QVERIFY2(changes.size() == 3, qPrintable(found.join(" ")));

    QCOMPARE(changes[0].type, PDFTextRedline::ChangeType::Replaced);
    QCOMPARE(changes[0].oldText, QStringLiteral("thirty (30)"));
    QCOMPARE(changes[0].newText, QStringLiteral("sixty (60)"));
    QCOMPARE(changes[0].before, QStringLiteral("The lease is for"));
    QCOMPARE(changes[1].type, PDFTextRedline::ChangeType::Replaced);   // "advance." -> "advance by EFT."
    QCOMPARE(changes[2].type, PDFTextRedline::ChangeType::Replaced);   // "pays water." -> "pays."

    const PDFTextRedline::Changes deletion = PDFTextRedline::compare(words(QStringLiteral("a b c d")), words(QStringLiteral("a d")));
    QCOMPARE(deletion.size(), size_t(1));
    QCOMPARE(deletion[0].type, PDFTextRedline::ChangeType::Deleted);
    QCOMPARE(deletion[0].oldText, QStringLiteral("b c"));
    const PDFTextRedline::Changes insertion = PDFTextRedline::compare(words(QStringLiteral("a d")), words(QStringLiteral("a b c d")));
    QCOMPARE(insertion.size(), size_t(1));
    QCOMPARE(insertion[0].type, PDFTextRedline::ChangeType::Inserted);
    QVERIFY(PDFTextRedline::compare(oldWords, oldWords).empty());
    QCOMPARE(PDFTextRedline::compare(PDFTextRedline::Words(), words(QStringLiteral("new text"))).size(), size_t(1));

    // The report is a PDF whose text holds the changes
    const PDFDocument report = PDFTextRedline::createReport(changes, QStringLiteral("Lease v1.pdf"), QStringLiteral("Lease v2.pdf"));
    QVERIFY(report.getCatalog()->getPageCount() >= 1);
    QStringList reportWords;
    for (const PDFTextRedline::Word& word : PDFTextRedline::getWords(&report))
    {
        reportWords << word.text;
    }
    const QString reportText = reportWords.join(' ');
    QVERIFY2(reportText.contains(QStringLiteral("sixty (60)")) && reportText.contains(QStringLiteral("thirty (30)")) && reportText.contains(QStringLiteral("Lease v2.pdf")), qPrintable(reportText));

    // Two unrelated texts of 3000 words: within the edit limit, one replaced block or
    // many changes, quickly and without a huge memory
    PDFTextRedline::Words left;
    PDFTextRedline::Words right;
    for (int i = 0; i < 3000; ++i)
    {
        left.push_back({ QString("a%1").arg(i), 0 });
        right.push_back({ QString("b%1").arg(i), 0 });
    }
    QElapsedTimer timer;
    timer.start();
    const PDFTextRedline::Changes unrelated = PDFTextRedline::compare(left, right);
    QCOMPARE(unrelated.size(), size_t(1));
    QCOMPARE(unrelated.front().type, PDFTextRedline::ChangeType::Replaced);
    QVERIFY2(timer.elapsed() < 30000, qPrintable(QString::number(timer.elapsed())));

    // Many changes flow over several pages
    PDFTextRedline::Changes many(120, changes.front());
    QVERIFY(PDFTextRedline::createReport(many, QStringLiteral("a"), QStringLiteral("b")).getCatalog()->getPageCount() > 2);
}

void SignatureBuilderTest::fontNames()
{
    auto check = [](const char* baseFont, const char* family, bool bold, bool italic)
    {
        const PDFFontIdentifier::FontName name = PDFFontIdentifier::parsePdfFontName(baseFont);
        return name.family == QLatin1String(family) && name.bold == bold && name.italic == italic;
    };
    QVERIFY(check("ABCDEF+TimesNewRomanPS-BoldMT", "Times New Roman", true, false));
    QVERIFY(check("ArialMT", "Arial", false, false));
    QVERIFY(check("Arial-BoldItalicMT", "Arial", true, true));
    QVERIFY(check("Helvetica-Oblique", "Helvetica", false, true));
    QVERIFY(check("QWERTY+Calibri", "Calibri", false, false));
    QVERIFY(check("OpenSans-SemiBold", "Open Sans", true, false));
    QVERIFY(check("SourceSansPro-Regular", "Source Sans", false, false));
    QVERIFY(check("Times-Roman", "Times New Roman", false, false));
    QVERIFY(check("TimesNewRoman,Bold", "Times New Roman", true, false));
    QVERIFY(check("TimesNewRoman", "Times New Roman", false, false));
    QVERIFY(check("ArialBold", "Arial", true, false));
    QVERIFY(check("Heavyweight", "Heavyweight", false, false));

    QCOMPARE(PDFFontIdentifier::getMetricCompatibleSubstitutes(QStringLiteral("arial")).value(0), QStringLiteral("Liberation Sans"));
    QCOMPARE(PDFFontIdentifier::getMetricCompatibleSubstitutes(QStringLiteral("Calibri")), QStringList{ QStringLiteral("Carlito") });
    QVERIFY(PDFFontIdentifier::getMetricCompatibleSubstitutes(QStringLiteral("Comic Neue")).isEmpty());
    QVERIFY(!PDFFontIdentifier::isInstalled(QStringLiteral("No Such Font Family 12345")));
}

void SignatureBuilderTest::fontMatching()
{
    const QStringList families = { QStringLiteral("DejaVu Sans"), QStringLiteral("DejaVu Serif"), QStringLiteral("DejaVu Sans Mono"),
                                   QStringLiteral("Nimbus Sans"), QStringLiteral("Nimbus Roman"), QStringLiteral("URW Bookman"), QStringLiteral("Nimbus Mono PS") };
    for (const QString& family : families)
    {
        if (!PDFFontIdentifier::isInstalled(family))
        {
            QSKIP(qPrintable(QString("The font %1 is not installed").arg(family)));
        }
    }

    // A "scan": the text in a known font on off-white paper, blurred by scaling and noisy
    auto makeScan = [](const QString& family, bool bold, const QString& text)
    {
        QFont font(family);
        font.setPixelSize(36);
        font.setBold(bold);
        QImage image(QFontMetrics(font).horizontalAdvance(text) + 30, 60, QImage::Format_RGB32);
        image.fill(QColor(242, 240, 232));
        {
            QPainter painter(&image);
            painter.setRenderHint(QPainter::TextAntialiasing);
            painter.setFont(font);
            painter.setPen(QColor(25, 25, 60));
            painter.drawText(QPointF(15, 45), text);
        }
        image = image.scaled(image.width() / 2, image.height() / 2, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                     .scaled(image.width(), image.height(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        QRandomGenerator random(5);
        for (int i = 0; i < image.width() * image.height() / 50; ++i)
        {
            image.setPixel(random.bounded(image.width()), random.bounded(image.height()), qRgb(200, 200, 200));
        }
        return image;
    };

    const QString text = QStringLiteral("Purchase Agreement 2026");
    const std::vector<PDFFontIdentifier::Match> serif = PDFFontIdentifier::matchScan(makeScan(QStringLiteral("DejaVu Serif"), false, text), text, families);
    QStringList found;
    for (const auto& match : serif)
    {
        found << QString("%1%2 %3").arg(match.family, match.bold ? " bold" : "").arg(match.score, 0, 'f', 3);
    }
    QVERIFY2(!serif.empty() && serif.front().family == QStringLiteral("DejaVu Serif") && !serif.front().bold, qPrintable(found.join(", ")));
    QVERIFY(serif.front().pixelSize > 20 && serif.front().pixelSize < 50);

    const std::vector<PDFFontIdentifier::Match> bold = PDFFontIdentifier::matchScan(makeScan(QStringLiteral("DejaVu Sans"), true, text), text, families);
    found.clear();
    for (const auto& match : bold)
    {
        found << QString("%1%2 %3").arg(match.family, match.bold ? " bold" : "").arg(match.score, 0, 'f', 3);
    }
    QVERIFY2(!bold.empty() && bold.front().family == QStringLiteral("DejaVu Sans") && bold.front().bold, qPrintable(found.join(", ")));

    // Among every installed font the true one is still among the best three
    const QStringList all = PDFFontIdentifier::getLatinFamilies();
    const std::vector<PDFFontIdentifier::Match> amongAll = PDFFontIdentifier::matchScan(makeScan(QStringLiteral("DejaVu Serif"), false, text), text, all, 3);
    found.clear();
    for (const auto& match : amongAll)
    {
        found << QString("%1%2 %3").arg(match.family, match.bold ? " bold" : "").arg(match.score, 0, 'f', 3);
    }
    QVERIFY2(std::any_of(amongAll.cbegin(), amongAll.cend(), [](const auto& match) { return match.family == QStringLiteral("DejaVu Serif"); }), qPrintable(found.join(", ")));

    QColor paper;
    QColor ink;
    PDFFontIdentifier::getScanColors(makeScan(QStringLiteral("DejaVu Sans"), false, text), paper, ink);
    QVERIFY(paper.lightness() > 220 && ink.lightness() < 120);
    QVERIFY(PDFFontIdentifier::matchScan(QImage(), text, families).empty());
}

void SignatureBuilderTest::fontInstaller()
{
    QFile regularFile(QStringLiteral("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"));
    QFile boldFile(QStringLiteral("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"));
    if (!regularFile.open(QFile::ReadOnly) || !boldFile.open(QFile::ReadOnly))
    {
        QSKIP("The DejaVu fonts are not installed, they stand in for the downloaded fonts");
    }
    const QByteArray regular = regularFile.readAll();
    const QByteArray bold = boldFile.readAll();

    // A catalogue on the loopback interface, as the Fontsource API answers
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    const QString base = QString("http://127.0.0.1:%1").arg(server.serverPort());
    QStringList requests;
    connect(&server, &QTcpServer::newConnection, &server, [&]()
    {
        QTcpSocket* socket = server.nextPendingConnection();
        connect(socket, &QTcpSocket::readyRead, socket, [&, socket]()
        {
            const QByteArray request = socket->readAll();
            const QByteArray path = request.split(' ').value(1);
            requests << QString::fromLatin1(path);
            QByteArray body;
            QByteArray status = "200 OK";
            if (path == "/v1/fonts?family=Test%20Font")
            {
                body = R"([{"id":"test-font","family":"Test Font","license":"OFL-1.1","category":"sans-serif"}])";
            }
            else if (path == "/v1/fonts?family=Evil%20Font")
            {
                body = R"([{"id":"../../evil","family":"Evil Font","license":"OFL-1.1","category":"serif"}])";
            }
            else if (path == "/v1/fonts?family=File%20Font")
            {
                body = R"([{"id":"file-font","family":"File Font","license":"OFL-1.1","category":"serif"}])";
            }
            else if (path == "/v1/fonts/file-font")
            {
                body = R"({"id":"file-font","variants":{"400":{"normal":{"latin":{"url":{"ttf":"file:///etc/hostname"}}}}}})";
            }
            else if (path == "/v1/fonts?family=Paid%20Font")
            {
                body = R"([{"id":"paid-font","family":"Paid Font","license":"Proprietary","category":"serif"}])";
            }
            else if (path.startsWith("/v1/fonts?"))
            {
                body = "[]";
            }
            else if (path == "/v1/fonts/test-font")
            {
                body = QString(R"({"id":"test-font","variants":{"400":{"normal":{"latin":{"url":{"ttf":"%1/files/regular.ttf"}}}},"700":{"normal":{"latin":{"url":{"ttf":"%1/files/bold.ttf"}}}}}})").arg(base).toLatin1();
            }
            else if (path == "/files/regular.ttf")
            {
                body = regular;
            }
            else if (path == "/files/bold.ttf")
            {
                body = bold;
            }
            else
            {
                status = "404 Not Found";
            }
            socket->write("HTTP/1.0 " + status + "\r\nContent-Length: " + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
            socket->disconnectFromHost();
        });
    });

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QStandardPaths::setTestModeEnabled(true);
    PDFFontInstaller::setTestConfiguration(base + QStringLiteral("/v1"), directory.filePath(QStringLiteral("fonts")));

    QVERIFY(PDFFontInstaller::isFreeLicense(QStringLiteral("OFL-1.1")));
    QVERIFY(!PDFFontInstaller::isFreeLicense(QStringLiteral("Proprietary")));

    QString errorMessage;
    const std::optional<PDFFontInstaller::FontInfo> font = PDFFontInstaller::find(QStringLiteral("Test Font"), &errorMessage);
    QVERIFY2(font, qPrintable(errorMessage));
    QCOMPARE(font->id, QStringLiteral("test-font"));
    QCOMPARE(font->category, QStringLiteral("sans-serif"));
    QVERIFY(!PDFFontInstaller::find(QStringLiteral("Missing Font"), &errorMessage));
    QVERIFY(errorMessage.isEmpty());   // not found is not an error

    // A font without a free licence is never downloaded
    const std::optional<PDFFontInstaller::FontInfo> paid = PDFFontInstaller::find(QStringLiteral("Paid Font"));
    QVERIFY(paid);
    QVERIFY(PDFFontInstaller::download(*paid, 400, false, &errorMessage).isEmpty());
    QVERIFY(!errorMessage.isEmpty());
    QVERIFY(!requests.contains(QStringLiteral("/v1/fonts/paid-font")));

    // Regular and bold exist, italics not: two files installed and loaded
    QVERIFY2(PDFFontInstaller::install(*font, &errorMessage), qPrintable(errorMessage));
    const QStringList installed = QDir(directory.filePath(QStringLiteral("fonts"))).entryList({ QStringLiteral("*.ttf") });
    QCOMPARE(installed, (QStringList{ QStringLiteral("andrepdf-test-font-bold.ttf"), QStringLiteral("andrepdf-test-font-regular.ttf") }));

    // A hostile catalogue: an identifier with a path, an installer address of another scheme
    QVERIFY(!PDFFontInstaller::find(QStringLiteral("Evil Font")));
    const std::optional<PDFFontInstaller::FontInfo> fileFont = PDFFontInstaller::find(QStringLiteral("File Font"));
    QVERIFY(fileFont);
    QVERIFY(PDFFontInstaller::download(*fileFont, 400, false, &errorMessage).isEmpty());
    QVERIFY(errorMessage.contains(QStringLiteral("not of the font catalogue")));
    PDFFontInstaller::FontInfo forged = *fileFont;
    forged.id = QStringLiteral("../x");
    QVERIFY(!PDFFontInstaller::install(forged, &errorMessage));
    QCOMPARE(QDir(directory.filePath(QStringLiteral("fonts"))).entryList({ QStringLiteral("*.ttf") }).size(), 2);
    QVERIFY(PDFFontIdentifier::isInstalled(QStringLiteral("DejaVu Sans")));

    // A font for the trial of the matching is cached, the second time without the network
    QCOMPARE(PDFFontInstaller::loadForTrial(QStringLiteral("Test Font"), &errorMessage), QStringLiteral("DejaVu Sans"));
    const qsizetype requestCount = requests.size();
    QCOMPARE(PDFFontInstaller::loadForTrial(QStringLiteral("Test Font"), &errorMessage), QStringLiteral("DejaVu Sans"));
    QCOMPARE(requests.size(), requestCount);
    QVERIFY(PDFFontInstaller::loadForTrial(QStringLiteral("Missing Font")).isEmpty());

    QVERIFY(PDFFontInstaller::getCandidateFamilies().contains(QStringLiteral("Tinos")));
    PDFFontInstaller::setTestConfiguration(QStringLiteral("https://api.fontsource.org/v1"), QString());
    QStandardPaths::setTestModeEnabled(false);
}

void SignatureBuilderTest::scanTextEdit()
{
    const QStringList families = { QStringLiteral("DejaVu Sans"), QStringLiteral("DejaVu Serif"), QStringLiteral("DejaVu Sans Mono"), QStringLiteral("Nimbus Sans"), QStringLiteral("Nimbus Roman") };
    for (const QString& family : families)
    {
        if (!PDFFontIdentifier::isInstalled(family))
        {
            QSKIP(qPrintable(QString("The font %1 is not installed").arg(family)));
        }
    }

    // A page with a line of a contract in a serif font (Qt coordinates: baseline 200 from the top)
    PDFDocumentBuilder builder;
    const PDFObjectReference page = builder.appendPage(QRectF(0, 0, 595, 842));
    {
        PDFPageContentStreamBuilder content(&builder, PDFContentStreamBuilder::CoordinateSystem::Qt, PDFPageContentStreamBuilder::Mode::Replace);
        QPainter* painter = content.begin(page);
        QFont font("DejaVu Serif");
        font.setPointSizeF(14);
        painter->setFont(font);
        painter->drawText(QPointF(60, 200), QStringLiteral("Purchase price R 950 000"));
        painter->drawText(QPointF(60, 260), QStringLiteral("Other line stays"));
        content.end(painter);
    }
    const PDFDocument document = builder.build();

    const QRectF area(55, 842 - 212, 260, 30);
    QCOMPARE(PDFScanTextEditor::getText(&document, 0, area), QStringLiteral("Purchase price R 950 000"));

    const QImage image = PDFScanTextEditor::renderArea(&document, 0, area, 300);
    QCOMPARE(image.width(), qCeil(260 * 300 / 72.0));
    const std::vector<PDFFontIdentifier::Match> matches = PDFFontIdentifier::matchScan(image, QStringLiteral("Purchase price R 950 000"), families, 3);
    QVERIFY(!matches.empty());
    QCOMPARE(matches.front().family, QStringLiteral("DejaVu Serif"));

    // The size of the identified font on the page is about the original 14 points
    const double pointSize = matches.front().pixelSize * 72.0 / 300.0;
    QVERIFY2(qAbs(pointSize - 14.0) < 2.0, qPrintable(QString::number(pointSize)));

    const QRectF ink = PDFScanTextEditor::getInkRectangle(image, area);
    QVERIFY(area.contains(ink) && ink.width() > 150 && ink.height() > 8 && ink.height() < 20);

    QColor paper;
    QColor inkColor;
    PDFFontIdentifier::getScanColors(image, paper, inkColor);

    PDFScanTextEditor::Edit edit;
    edit.pageIndex = 0;
    edit.area = area;
    edit.inkRectangle = ink;
    edit.text = QStringLiteral("Purchase price R 875 000");
    edit.font = QFont(matches.front().family);
    edit.font.setPointSizeF(pointSize);
    edit.ink = inkColor;
    edit.paper = paper;
    const PDFDocumentPointer edited = PDFScanTextEditor::apply(&document, edit);
    QVERIFY(edited);

    // The new text is on the page, where the old one was, in the same font and size
    const QImage after = PDFScanTextEditor::renderArea(edited.data(), 0, area, 300);
    const std::vector<PDFFontIdentifier::Match> afterMatches = PDFFontIdentifier::matchScan(after, edit.text, families, 1);
    QVERIFY(!afterMatches.empty());
    QCOMPARE(afterMatches.front().family, QStringLiteral("DejaVu Serif"));
    const QRectF afterInk = PDFScanTextEditor::getInkRectangle(after, area);
    QVERIFY2(qAbs(afterInk.left() - ink.left()) < 2.0 && qAbs(afterInk.center().y() - ink.center().y()) < 2.0 && qAbs(afterInk.height() - ink.height()) < 2.0,
             qPrintable(QString("%1 %2 %3 / %4 %5 %6").arg(ink.left()).arg(ink.center().y()).arg(ink.height()).arg(afterInk.left()).arg(afterInk.center().y()).arg(afterInk.height())));
    // Born-digital text under the edit stays in the content (only covered) - the
    // editor reports it; the own OCR layer of a scan is removed (none here)
    QVERIFY(!PDFScanTextEditor::hasOwnOcrLayer(&document, 0));
    bool isOcrLayerRemoved = true;
    QVERIFY(PDFScanTextEditor::apply(&document, edit, &isOcrLayerRemoved));
    QVERIFY(!isOcrLayerRemoved);
    QCOMPARE(PDFScanTextEditor::getText(edited.data(), 0, QRectF(55, 842 - 272, 260, 30)), QStringLiteral("Other line stays"));
}

void SignatureBuilderTest::scanFont()
{
    // Outline tracing: a square with a square hole gives an outer and an inner outline
    QImage ring(10, 10, QImage::Format_Grayscale8);
    ring.fill(255);
    for (int y = 1; y < 9; ++y)
    {
        for (int x = 1; x < 9; ++x)
        {
            if (x < 3 || x > 6 || y < 3 || y > 6)
            {
                ring.setPixel(x, y, 0);
            }
        }
    }
    const std::vector<QPolygonF> contours = PDFScanFontBuilder::traceContours(ring);
    QCOMPARE(contours.size(), size_t(2));
    auto area = [](const QPolygonF& polygon)
    {
        double sum = 0.0;
        for (qsizetype i = 0; i < polygon.size(); ++i)
        {
            const QPointF& a = polygon[i];
            const QPointF& b = polygon[(i + 1) % polygon.size()];
            sum += a.x() * b.y() - b.x() * a.y();
        }
        return sum * 0.5;
    };
    // On the screen (y down) the outer outline is clockwise (positive area), the hole not
    QVERIFY((area(contours[0]) > 0) != (area(contours[1]) > 0));
    QCOMPARE(qAbs(area(contours[0])) + qAbs(area(contours[1])), 64.0 + 16.0);

    if (!PDFFontIdentifier::isInstalled(QStringLiteral("DejaVu Serif")) || !PDFFontIdentifier::isInstalled(QStringLiteral("DejaVu Sans")))
    {
        QSKIP("The DejaVu fonts are not installed");
    }

    // A page of a contract in DejaVu Serif, its letters become the font
    PDFDocumentBuilder builder;
    const PDFObjectReference page = builder.appendPage(QRectF(0, 0, 595, 842));
    {
        PDFPageContentStreamBuilder content(&builder, PDFContentStreamBuilder::CoordinateSystem::Qt, PDFPageContentStreamBuilder::Mode::Replace);
        QPainter* painter = content.begin(page);
        QFont font("DejaVu Serif");
        font.setPointSizeF(12);
        painter->setFont(font);
        const QStringList lines = { QStringLiteral("The purchaser shall pay the amount of R 950 000 to the seller"),
                                    QStringLiteral("within thirty days of the signature of this agreement 2026"),
                                    QStringLiteral("Both parties confirm that they read and understood every clause"),
                                    QStringLiteral("Signed at Rustenburg by the buyer and seller on 18 October") };
        for (int i = 0; i < lines.size(); ++i)
        {
            painter->drawText(QPointF(50, 100 + i * 20), lines[i]);
        }
        content.end(painter);
    }
    const PDFDocument document = builder.build();

    PDFScanFontBuilder::Metrics metrics;
    const std::vector<PDFScanFontBuilder::Glyph> glyphs = PDFScanFontBuilder::harvest(&document, { 0 }, 300.0, &metrics);
    QString characters;
    for (const auto& glyph : glyphs)
    {
        characters += glyph.character;
    }
    QVERIFY2(glyphs.size() > 25, qPrintable(characters));
    QVERIFY2(characters.contains('9') && characters.contains('5') && characters.contains('0') && characters.contains('a'), qPrintable(characters));
    QVERIFY(metrics.xHeight > 15 && metrics.xHeight < 35);   // 12 pt at 300 dpi

    const QByteArray ttf = PDFScanFontBuilder::createTrueTypeFont(glyphs, metrics, QStringLiteral("Scan Font Test"));
    QVERIFY(ttf.size() > 1000);
    const QRawFont raw(ttf, 40);
    QVERIFY(raw.isValid());
    QCOMPARE(raw.familyName(), QStringLiteral("Scan Font Test"));
    QVERIFY(raw.supportsCharacter(QChar('9')) && raw.supportsCharacter(QChar('a')) && !raw.supportsCharacter(QChar('X')));

    // OS/2 version 4 is 96 bytes; the metrics of a 40 px font are about 40 px
    const QByteArray os2 = raw.fontTable("OS/2");
    QCOMPARE(os2.size(), 96);
    auto u16 = [&os2](int offset) { return int(quint16((uchar(os2[offset]) << 8) | uchar(os2[offset + 1]))); };
    auto s16 = [&u16](int offset) { return int(qint16(u16(offset))); };
    QCOMPARE(u16(0), 4);
    QVERIFY(u16(64) <= u16(66));                   // usFirstCharIndex <= usLastCharIndex
    QVERIFY(s16(68) > 0 && s16(70) < 0);           // sTypoAscender > 0 > sTypoDescender
    QVERIFY(u16(74) > 0 && u16(74) < 2000);        // usWinAscent
    QVERIFY(s16(86) > 300 && s16(86) < 700);       // sxHeight in units of 1000 per em
    QVERIFY(raw.ascent() > 20 && raw.ascent() < 60);

    const int id = QFontDatabase::addApplicationFontFromData(ttf);
    QVERIFY(id >= 0);
    QCOMPARE(QFontDatabase::applicationFontFamilies(id).value(0), QStringLiteral("Scan Font Test"));

    // Text drawn in the scan font is recognized as DejaVu Serif: the shapes survived
    const QString text = QStringLiteral("seller 950 000");
    QFont scanFont(QStringLiteral("Scan Font Test"));
    scanFont.setPixelSize(48);
    scanFont.setStyleStrategy(QFont::NoFontMerging);
    QImage image(QFontMetrics(scanFont).horizontalAdvance(text) + 40, 80, QImage::Format_RGB32);
    image.fill(Qt::white);
    {
        QPainter painter(&image);
        painter.setRenderHint(QPainter::TextAntialiasing);
        painter.setFont(scanFont);
        painter.setPen(Qt::black);
        painter.drawText(QPointF(20, 60), text);
    }
    const std::vector<PDFFontIdentifier::Match> matches = PDFFontIdentifier::matchScan(image, text, { QStringLiteral("DejaVu Serif"), QStringLiteral("DejaVu Sans"), QStringLiteral("DejaVu Sans Mono") }, 3);
    QStringList found;
    for (const auto& match : matches)
    {
        found << QString("%1 %2").arg(match.family).arg(match.score, 0, 'f', 3);
    }
    QVERIFY2(!matches.empty() && matches.front().family == QStringLiteral("DejaVu Serif") && matches.front().score > 0.6, qPrintable(found.join(", ")));

    QString built;
    QVERIFY(!PDFScanFontBuilder::build(&document, { 0 }, QStringLiteral("Scan Font Build"), &built).isEmpty());
    QCOMPARE(built, characters);
}

void SignatureBuilderTest::versionHistory()
{
    QVERIFY(PDFVersionHistory::compare(QStringLiteral("1.10.0"), QStringLiteral("1.9.2")) > 0);
    QVERIFY(PDFVersionHistory::compare(QStringLiteral("1.0"), QStringLiteral("1.0.0")) == 0);
    QVERIFY(PDFVersionHistory::compare(QStringLiteral("1.0.0"), QStringLiteral("1.0.1")) < 0);
    QVERIFY(PDFVersionHistory::isValidVersion(QStringLiteral("2.3.4")));
    QVERIFY(!PDFVersionHistory::isValidVersion(QStringLiteral("2.3.x")));
    QVERIFY(!PDFVersionHistory::isValidVersion(QStringLiteral("1.0.0; rm -rf")));

    const QString markdown = QStringLiteral(
        "# History\n\n"
        "## 1.1.0 - 2026-11-01\n\nSummary: Faster signing.\n\n- Signing is faster.\n- A fix\n  continued here.\n\n"
        "## 1.10.0 - 2027-01-05\n\nSummary: Big one.\n\n- New\n\n"
        "## 1.0.0 - 2026-10-19\n\nSummary: First.\n\n- Everything\n");
    const PDFVersionHistory::Releases releases = PDFVersionHistory::parse(markdown);
    QCOMPARE(releases.size(), size_t(3));
    QCOMPARE(releases[0].version, QStringLiteral("1.10.0"));   // sorted numerically, newest first
    QCOMPARE(releases[1].summary, QStringLiteral("Faster signing."));
    QCOMPARE(releases[1].changes, (QStringList{ QStringLiteral("Signing is faster."), QStringLiteral("A fix continued here.") }));
    QCOMPARE(releases[1].date, QDate(2026, 11, 1));

    const PDFVersionHistory::Releases newer = PDFVersionHistory::getNewer(releases, QStringLiteral("1.0.0"));
    QCOMPARE(newer.size(), size_t(2));
    QVERIFY(PDFVersionHistory::getNewer(releases, QStringLiteral("1.10.0")).empty());
    QVERIFY(PDFVersionHistory::toMarkdown(newer).contains(QStringLiteral("**Big one\\.**")));

    // The program carries its own history, its newest entry is its version
    const PDFVersionHistory::Releases embedded = PDFVersionHistory::parse(PDFVersionHistory::getEmbeddedHistory());
    QVERIFY(!embedded.empty());
    QCOMPARE(embedded.front().version, QStringLiteral(ANDRE_PDF_VERSION));
    for (const PDFVersionHistory::Release& release : embedded)
    {
        QVERIFY2(!release.summary.isEmpty() && !release.changes.isEmpty() && release.date.isValid(), qPrintable(release.version));
    }
}

void SignatureBuilderTest::scanTextForeignOcr_data()
{
    QTest::addColumn<QByteArray>("ocrText");
    QTest::newRow("text object per line") << QByteArray("BT 3 Tr /AndreOcr 11 Tf 58 620 Td (Purchase price R 950 000) Tj ET\nBT 3 Tr /AndreOcr 11 Tf 58 400 Td (Elsewhere) Tj ET\n");
    // As Tesseract writes it: one text object for the page (review R0 verification N2)
    QTest::newRow("one text object") << QByteArray("BT 3 Tr /AndreOcr 11 Tf 58 620 Td (Purchase price R 950 000) Tj 0 -220 Td (Elsewhere) Tj 0 -100 Td (Third line) Tj ET\n");
}

void SignatureBuilderTest::scanTextForeignOcr()
{
    QFETCH(QByteArray, ocrText);

    if (!PDFFontIdentifier::isInstalled(QStringLiteral("DejaVu Serif")))
    {
        QSKIP("The font DejaVu Serif is not installed");
    }

    // A scan: an image of the text, and the invisible OCR text of another program
    // (render mode 3, a standard font) at the same place
    QImage scan(1000, 120, QImage::Format_RGB32);
    scan.fill(Qt::white);
    {
        QPainter painter(&scan);
        QFont font("DejaVu Serif");
        font.setPixelSize(58);
        painter.setFont(font);
        painter.setPen(Qt::black);
        painter.drawText(QPointF(10, 80), QStringLiteral("Purchase price R 950 000"));
    }
    PDFDocumentBuilder builder;
    const PDFObjectReference page = builder.appendPage(QRectF(0, 0, 595, 842));
    {
        PDFPageContentStreamBuilder content(&builder, PDFContentStreamBuilder::CoordinateSystem::Qt, PDFPageContentStreamBuilder::Mode::Replace);
        QPainter* painter = content.begin(page);
        painter->drawImage(QRectF(55, 842 - 230, 250, 30), scan);
        content.end(painter);
    }
    PDFDictionaryBuilder streamDictionary;
    streamDictionary.setEntry(PDFInplaceOrMemoryString("Length"), PDFObject::createInteger(ocrText.size()));
    const PDFObjectReference ocrStream = builder.addObject(PDFObject::createStream(PDFStream(std::move(streamDictionary), QByteArray(ocrText))));
    {
        const PDFDocument draft = builder.build();
        const PDFObject pageObject = draft.getObjectByReference(page);
        const PDFDictionary* pageDictionary = pageObject.getDictionary();
        PDFObjectFactory update;
        update.beginDictionary();
        update.beginDictionaryItem("Contents");
        update.beginArray();
        // The OCR text first: the stream of the content builder leaves its coordinate
        // system (y down) for the streams after it
        update << ocrStream;
        const PDFObject contents = pageDictionary->get("Contents");
        if (contents.isReference())
        {
            update << contents.getReference();
        }
        else
        {
            for (const PDFObject& item : *draft.getObject(contents).getArray())
            {
                update << item.getReference();
            }
        }
        update.endArray();
        update.endDictionaryItem();
        update.endDictionary();
        builder.mergeTo(page, update.takeObject());

        PDFObjectFactory font;
        font.beginDictionary();
        font.beginDictionaryItem("Font");
        font.beginDictionary();
        font.beginDictionaryItem("AndreOcr");
        font.beginDictionary();
        font.beginDictionaryItem("Type"); font << WrapName("Font"); font.endDictionaryItem();
        font.beginDictionaryItem("Subtype"); font << WrapName("Type1"); font.endDictionaryItem();
        font.beginDictionaryItem("BaseFont"); font << WrapName("Helvetica"); font.endDictionaryItem();
        font.endDictionary();
        font.endDictionaryItem();
        font.endDictionary();
        font.endDictionaryItem();
        font.endDictionary();
        const PDFObject resources = pageDictionary->get("Resources");
        builder.mergeTo(resources.isReference() ? resources.getReference() : page, font.takeObject());
    }
    const PDFDocument document = builder.build();

    const QRectF area(55, 842 - 232, 260, 34);
    QVERIFY2(PDFScanTextEditor::getText(&document, 0, area).contains(QStringLiteral("950 000")), qPrintable(PDFScanTextEditor::getText(&document, 0, area)));
    QVERIFY(!PDFScanTextEditor::hasOwnOcrLayer(&document, 0));

    PDFScanTextEditor::Edit edit;
    edit.pageIndex = 0;
    edit.area = area;
    edit.inkRectangle = PDFScanTextEditor::getInkRectangle(PDFScanTextEditor::renderArea(&document, 0, area, 150), area);
    edit.text = QStringLiteral("Purchase price R 875 000");
    edit.font = QFont(QStringLiteral("DejaVu Serif"));
    edit.font.setPointSizeF(13);
    PDFScanTextEditor::HiddenText hidden = PDFScanTextEditor::HiddenText::None;
    const PDFDocumentPointer edited = PDFScanTextEditor::apply(&document, edit, nullptr, &hidden);
    QVERIFY(edited);
    QCOMPARE(hidden, PDFScanTextEditor::HiddenText::ForeignRemoved);

    // The old amount is gone from the text, the new one is there, the other words stay
    const QString text = PDFScanTextEditor::getText(edited.data(), 0, area);
    QVERIFY2(!text.contains(QStringLiteral("950")) && text.contains(QStringLiteral("875 000")), qPrintable(text));
    QVERIFY(PDFScanTextEditor::getText(edited.data(), 0, QRectF(50, 390, 200, 30)).contains(QStringLiteral("Elsewhere")));
    if (ocrText.contains("Third line"))
    {
        QVERIFY(PDFScanTextEditor::getText(edited.data(), 0, QRectF(50, 290, 200, 30)).contains(QStringLiteral("Third line")));
    }

    // The image of the scan is still drawn (outside the edited area)
    const QImage rendered = PDFScanTextEditor::renderArea(edited.data(), 0, QRectF(55, 842 - 230, 250, 30), 72);
    QVERIFY(!rendered.isNull());

    // A born-digital page is not rewritten
    QVERIFY(!PDFScanTextEditor::removeInvisibleText(edited.data(), 0, QRectF(50, 390, 200, 30)));
}

QTEST_MAIN(SignatureBuilderTest)
#include "tst_signaturebuildertest.moc"
