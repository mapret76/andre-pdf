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

#include "pdfmobilecontroller.h"
#include "pdfdocumentbuilder.h"
#include "pdfdocumentwriter.h"
#include "pdfdocumentsigner.h"
#include "pdfcertificatemanager.h"
#include "pdfsignaturelibrary.h"

#include <QtTest>
#include <QPainter>
#include <QTemporaryDir>
#include <QStandardPaths>

using namespace pdf;

/// Backend of the mobile app: open, signature library, sign every line or at a tap
class MobileTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void openSignAndCheck();
};

void MobileTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName("PDF4QT-Tests");
    QCoreApplication::setApplicationName("Mobile");
}

void MobileTest::openSignAndCheck()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    // A contract page with a signature line
    PDFDocumentBuilder builder;
    const PDFObjectReference page = builder.appendPage(QRectF(0, 0, 595, 842));
    {
        PDFPageContentStreamBuilder content(&builder, PDFContentStreamBuilder::CoordinateSystem::Qt, PDFPageContentStreamBuilder::Mode::Replace);
        QPainter* painter = content.begin(page);
        QFont font("DejaVu Sans");
        font.setPixelSize(11);
        painter->setFont(font);
        painter->drawText(QPointF(60, 120), QStringLiteral("LEASE AGREEMENT"));
        painter->drawText(QPointF(60, 600), QStringLiteral("Signature: ________________________"));
        content.end(painter);
    }
    const PDFDocument document = builder.build();
    const QString documentFile = directory.filePath(QStringLiteral("Lease.pdf"));
    PDFDocumentWriter writer(nullptr);
    QVERIFY(writer.write(documentFile, &document, false));

    // Certificate file as on a phone (.pfx)
    PDFCertificateManager::NewCertificateInfo info;
    info.fileName = directory.filePath(QStringLiteral("andre.pfx"));
    info.privateKeyPasword = QStringLiteral("pin-1234");
    info.certCommonName = QStringLiteral("Andre Mobile Test");
    info.rsaKeyLength = 2048;
    PDFCertificateManager().createCertificate(info);
    QVERIFY(QFile::exists(info.fileName));

    QFile file(info.fileName);
    QVERIFY(file.open(QFile::ReadOnly));
    const std::optional<PDFCertificateEntry> entry = PDFCertificateManager::createEntryFromPkcs12(file.readAll(), info.privateKeyPasword);
    QVERIFY(entry);
    QCOMPARE(entry->info.getName(PDFCertificateInfo::CommonName), QStringLiteral("Andre Mobile Test"));
    QVERIFY(!PDFCertificateManager::createEntryFromPkcs12(entry->pkcs12, QStringLiteral("wrong")));

    pdfmobile::PDFMobileController controller;
    QVERIFY(controller.open(QUrl::fromLocalFile(documentFile)));
    QCOMPARE(controller.getPageCount(), 1);
    QCOMPARE(controller.getSignatureLineCount(), 1);
    QCOMPARE(controller.getFileName(), QStringLiteral("Lease.pdf"));
    QVERIFY(qAbs(controller.getPageAspectRatio(0) - 842.0 / 595.0) < 0.001);

    // The page renders (dark text on white)
    const QImage rendered = controller.renderPage(0, QSize(300, 425));
    bool hasInk = false;
    for (int y = 0; y < rendered.height() && !hasInk; ++y)
    {
        for (int x = 0; x < rendered.width() && !hasInk; ++x)
        {
            hasInk = qGray(rendered.pixel(x, y)) < 100;
        }
    }
    QVERIFY(hasInk);

    // A photo of a signature into the library
    QImage photo(400, 160, QImage::Format_RGB32);
    photo.fill(QColor(235, 235, 228));
    {
        QPainter painter(&photo);
        painter.setPen(QPen(QColor(10, 20, 100), 6));
        painter.drawLine(40, 120, 360, 40);
    }
    const QString photoFile = directory.filePath(QStringLiteral("signature.jpg"));
    QVERIFY(photo.save(photoFile));
    QVERIFY(controller.importSignature(0, QUrl::fromLocalFile(photoFile), QStringLiteral("Andre")));
    QCOMPARE(controller.getSignatureSlots().value(0), QStringLiteral("Andre"));

    // Every line signed, certified "no changes", written next to the original and checked
    QVERIFY2(controller.sign(QUrl::fromLocalFile(info.fileName), info.privateKeyPasword, 1, 0, -1, 0, 0), qPrintable(controller.getStatus()));
    const QString signedFile = controller.getSignedFileUrl().toLocalFile();
    QCOMPARE(QFileInfo(signedFile).fileName(), QStringLiteral("Lease_SIGNED.pdf"));
    QFile signedData(signedFile);
    QVERIFY(signedData.open(QFile::ReadOnly));
    const QByteArray signedBytes = signedData.readAll();
    QVERIFY(signedBytes.startsWith(QByteArray("%PDF")));

    // The signed file opened again: signed, so only an approval signature (tap) is possible
    QVERIFY(controller.open(QUrl::fromLocalFile(signedFile)));
    QVERIFY2(controller.sign(QUrl::fromLocalFile(info.fileName), info.privateKeyPasword, 1, 0, 0, 0.5, 0.9), qPrintable(controller.getStatus()));
    QVERIFY(!controller.sign(QUrl::fromLocalFile(info.fileName), QStringLiteral("wrong"), 1, 0, 0, 0.5, 0.9));
}

QTEST_MAIN(MobileTest)
#include "tst_mobiletest.moc"
