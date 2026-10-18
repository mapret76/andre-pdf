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

#include "pdfcms.h"
#include "pdfdocumentbuilder.h"
#include "pdfdrawwidget.h"
#include "pdfdrawspacecontroller.h"
#include "pdfprogress.h"
#include "pdfoptionalcontent.h"
#include "pdfpagecontentelements.h"
#include "pdfwidgetannotation.h"
#include "pdfwidgetformmanager.h"
#include "pdfpagecontentprocessor.h"
#include "pdfsignaturelibrary.h"
#include "signdialog.h"
#include "signaturelibrarydialog.h"
#include "initialsdialog.h"

#include <QtTest>
#include <QLineEdit>
#include <QCheckBox>
#include <QSlider>
#include <QTimer>
#include <QSettings>
#include <QComboBox>
#include <QPainter>

using namespace pdf;

class SignatureAppearanceTest : public QObject
{
    Q_OBJECT
private slots:
    void strokeBounds();
    void dialogMethods();
    void dialogLockAndEncryption();
    void signatureLibraryDialogs();
    void signatureImageKeepsTransparency();
    void initialsDialog();
    void rendering_data();
    void rendering();
    void fallback_data();
    void fallback();
};

namespace signatureappearancetest
{
struct Fixture
{
    PDFDocument document;
    PDFOptionalContentActivity optionalContentActivity{&document, OCUsage::View, nullptr};
    PDFCMSManager cms{nullptr};
    PDFProgress progress{nullptr};
    PDFWidget widget{&cms, RendererEngine::QPainter, nullptr};
    PDFWidgetAnnotationManager annotations{widget.getDrawWidgetProxy(), nullptr};
    PDFWidgetFormManager forms{widget.getDrawWidgetProxy(), nullptr};

    Fixture(PDFDocument doc, bool verified, bool valid) : document(std::move(doc))
    {
        widget.getDrawWidgetProxy()->setProgress(&progress);
        widget.setAnnotationManager(&annotations);
        widget.setFormManager(&forms);
        forms.setAnnotationManager(&annotations);
        annotations.setFormManager(&forms);
        PDFSignatureVerificationResult result;
        result.setSignatureFieldQualifiedName("Signature");
        result.setFlag(PDFSignatureVerificationResult::OK, valid);
        const PDFModifiedDocument modified(&document, &optionalContentActivity);
        widget.setDocument(modified, verified ? std::vector{result} : std::vector<PDFSignatureVerificationResult>());
        forms.setDocument(modified);
        forms.setAppearanceFlags({});
        annotations.setDocument(modified);
    }

    ~Fixture()
    {
        forms.setDocument(PDFModifiedDocument());
        annotations.setDocument(PDFModifiedDocument());
        widget.setDocument(PDFModifiedDocument(), {});
        widget.setFormManager(nullptr);
        widget.setAnnotationManager(nullptr);
    }
};

PDFObjectReference makeAppearance(PDFDocumentBuilder& builder, QPointF origin)
{
    PDFContentStreamBuilder content(QSizeF(300, 400), PDFContentStreamBuilder::CoordinateSystem::PDF);
    QPainter* painter = content.begin();
    painter->translate(-origin);
    painter->fillRect(QRectF(20, 30, 80, 40).translated(origin), Qt::red);
    painter->fillRect(QRectF(20, 30, 40, 20).translated(origin), Qt::blue);
    const auto stream = content.end(painter);
    const auto objects = builder.copyFrom({stream.resources, stream.contents}, stream.document.getStorage(), true);
    PDFObjectFactory factory;
    factory.beginDictionary();
    factory.beginDictionaryItem("Type");
    factory << WrapName("XObject");
    factory.endDictionaryItem();
    factory.beginDictionaryItem("Subtype");
    factory << WrapName("Form");
    factory.endDictionaryItem();
    factory.beginDictionaryItem("BBox");
    factory << QRectF(20, 30, 80, 40);
    factory.endDictionaryItem();
    factory.beginDictionaryItem("Resources");
    factory << objects[0];
    factory.endDictionaryItem();
    factory.endDictionary();
    builder.mergeTo(objects[1].getReference(), factory.takeObject());
    return objects[1].getReference();
}
}   // namespace signatureappearancetest

using namespace signatureappearancetest;

void SignatureAppearanceTest::strokeBounds()
{
    for (int kind : {0, 1, 2})
    {
        PDFPageContentScene scene(nullptr);
        PDFPageContentStyledElement* element = nullptr;
        if (kind == 2)
        {
            auto* dot = new PDFPageContentElementDot();
            dot->setPoint(QPointF(50, 50));
            element = dot;
        }
        else
        {
            auto* line = new PDFPageContentElementLine();
            line->setLine(kind == 0 ? QLineF(20, 50, 80, 50) : QLineF(50, 20, 50, 80));
            element = line;
        }
        element->setPageIndex(0);
        element->setPen(QPen(Qt::black, 6.0, Qt::SolidLine, Qt::SquareCap));
        scene.addElement(element);
        const QRectF bounds = scene.getBoundingBox(0, true);
        QVERIFY(!bounds.isEmpty());
        QCOMPARE(scene.getBoundingBox(0), element->getBoundingBox());
        QVERIFY(scene.getBoundingBox(1, true).isEmpty());
        QImage image(100, 100, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::white);
        QPainter painter(&image);
        PDFTextLayoutGetter getter(nullptr, 0);
        QList<PDFRenderError> errors;
        scene.drawElements(&painter, 0, getter, QTransform(), nullptr, PDFColorConvertor(), errors);
        painter.end();
        QVERIFY(errors.isEmpty());
        int paintedPixels = 0;
        for (int y = 0; y < image.height(); ++y)
        {
            for (int x = 0; x < image.width(); ++x)
            {
                if (image.pixelColor(x, y) != QColor(Qt::white))
                {
                    ++paintedPixels;
                    QVERIFY(bounds.contains(QPointF(x + 0.5, y + 0.5)));
                }
            }
        }
        QVERIFY(paintedPixels > 0);
    }
}

void SignatureAppearanceTest::dialogMethods()
{
    for (bool empty : {false, true})
    {
        pdfplugin::SignDialog dialog(nullptr, empty);
        auto* combo = dialog.findChild<QComboBox*>("methodCombo");
        QVERIFY(combo);
        QCOMPARE(combo->count(), 2);
        QCOMPARE(dialog.getSignMethod(), empty ? pdfplugin::SignDialog::SignDigitallyInvisible : pdfplugin::SignDialog::SignDigitally);
        combo->setCurrentIndex(0);
        QCOMPARE(dialog.getSignMethod(), pdfplugin::SignDialog::SignDigitally);
        combo->setCurrentIndex(1);
        QCOMPARE(dialog.getSignMethod(), pdfplugin::SignDialog::SignDigitallyInvisible);
    }
}

void SignatureAppearanceTest::rendering_data()
{
    QTest::addColumn<QPointF>("origin");
    QTest::addColumn<int>("rotation");
    QTest::addColumn<bool>("valid");
    QTest::addColumn<bool>("print");
    for (QPointF origin : {QPointF(), QPointF(100, 200), QPointF(-100, -200)})
    {
        for (int rotation : {0, 90, 180, 270})
        {
            for (bool valid : {false, true})
            {
                for (bool print : {false, true})
                {
                    const QByteArray name = QByteArray::number(origin.x()) + "-" + QByteArray::number(rotation) + "-" + QByteArray::number(valid) + "-" + QByteArray::number(print);
                    QTest::newRow(name.constData()) << origin << rotation << valid << print;
                }
            }
        }
    }
}

void SignatureAppearanceTest::rendering()
{
    QFETCH(QPointF, origin);
    QFETCH(int, rotation);
    QFETCH(bool, valid);
    QFETCH(bool, print);
    PDFDocumentBuilder builder;
    const auto page = builder.appendPage(QRectF(origin, QSizeF(300, 400)));
    builder.setPageRotation(page, static_cast<PageRotation>(rotation / 90));
    const auto appearance = makeAppearance(builder, origin);
    const auto signature = builder.createSignatureDictionary("Adobe.PPKLite", "adbe.pkcs7.detached", "test", QDateTime::currentDateTime(), 0);
    const auto field = builder.createSignatureField("Signature", signature, page, appearance, QRectF(20, 30, 80, 40).translated(origin));
    Fixture fixture(builder.build(), true, valid);
    QVERIFY(fixture.forms.getFormFieldForWidget(field));
    QVERIFY(!fixture.forms.isEditorDrawEnabled(field));
    fixture.annotations.setTarget(print ? PDFAnnotationManager::Target::Print : PDFAnnotationManager::Target::View);
    const auto* pdfPage = fixture.document.getCatalog()->getPage(0);
    QCOMPARE(pdfPage->getPageRotation(), static_cast<PageRotation>(rotation / 90));
    const bool quarterTurn = rotation == 90 || rotation == 270;
    QImage image(quarterTurn ? QSize(800, 600) : QSize(600, 800), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);
    const auto transform = PDFRenderer::createPagePointToDevicePointMatrix(pdfPage, QRectF(image.rect()));
    QCOMPARE(transform.mapRect(pdfPage->getMediaBox()), QRectF(image.rect()));
    QPainter painter(&image);
    PDFTextLayoutGetter getter(nullptr, 0);
    QList<PDFRenderError> errors;
    fixture.annotations.drawPage(&painter, 0, nullptr, getter, transform, PDFColorConvertor(), errors);
    painter.end();
    QVERIFY(errors.isEmpty());
    QCOMPARE(image.pixelColor(transform.map(origin + QPointF(40, 40)).toPoint()), QColor(Qt::blue));
    QCOMPARE(image.pixelColor(transform.map(origin + QPointF(80, 60)).toPoint()), QColor(Qt::red));
    QCOMPARE(image.pixelColor(transform.map(origin + QPointF(150, 150)).toPoint()), QColor(Qt::white));
}

void SignatureAppearanceTest::fallback_data()
{
    QTest::addColumn<int>("appearanceKind");
    QTest::addColumn<bool>("emptyRect");
    QTest::addColumn<bool>("verified");
    for (int kind : {0, 1, 2, 3})
    {
        for (bool empty : {false, true})
        {
            for (bool verified : {false, true})
            {
                const QByteArray name = QByteArray::number(kind) + "-" + QByteArray::number(empty) + "-" + QByteArray::number(verified);
                QTest::newRow(name.constData()) << kind << empty << verified;
            }
        }
    }
}

void SignatureAppearanceTest::fallback()
{
    QFETCH(int, appearanceKind);
    QFETCH(bool, emptyRect);
    QFETCH(bool, verified);
    PDFDocumentBuilder builder;
    const auto page = builder.appendPage(QRectF(0, 0, 300, 400));
    const auto appearance = makeAppearance(builder, QPointF());
    const auto signature = builder.createSignatureDictionary("Adobe.PPKLite", "adbe.pkcs7.detached", "test", QDateTime::currentDateTime(), 0);
    const auto field = builder.createSignatureField("Signature", signature, page, appearance, QRectF(20, 30, 80, 40));
    // Exercise existing PDFs with missing, malformed, direct or indirect appearances.
    PDFDictionaryBuilder widget(*builder.getStorage()->getObjectByReference(field).getDictionary());
    if (emptyRect)
    {
        PDFObjectFactory factory;
        factory << QRectF();
        widget.setEntry(PDFInplaceOrMemoryString("Rect"), factory.takeObject());
    }
    if (appearanceKind == 0)
    {
        widget.setEntry(PDFInplaceOrMemoryString("AP"), PDFObject());
    }
    else if (appearanceKind == 1)
    {
        PDFDictionaryBuilder ap;
        ap.setEntry(PDFInplaceOrMemoryString("N"), PDFObject::createInteger(7));
        widget.setEntry(PDFInplaceOrMemoryString("AP"), PDFObject::createDictionary(std::move(ap)));
    }
    else if (appearanceKind == 3)
    {
        const auto ap = builder.addObject(widget.get("AP"));
        widget.setEntry(PDFInplaceOrMemoryString("AP"), PDFObject::createReference(ap));
    }
    builder.setObject(field, PDFObject::createDictionary(std::move(widget)));
    Fixture fixture(builder.build(), verified, false);
    QVERIFY(fixture.forms.getFormFieldForWidget(field));
    QCOMPARE(fixture.forms.isEditorDrawEnabled(field), verified && (emptyRect || appearanceKind < 2));
}

void SignatureAppearanceTest::dialogLockAndEncryption()
{
    // A document without signatures: certified "no changes" by default, encryption off
    {
        pdfplugin::SignDialog dialog(nullptr, false, true);
        auto* certification = dialog.findChild<QComboBox*>("certificationCombo");
        auto* encrypt = dialog.findChild<QCheckBox*>("encryptCheckBox");
        auto* password = dialog.findChild<QLineEdit*>("openPasswordEdit");
        QVERIFY(certification && encrypt && password);
        QVERIFY(certification->isEnabled());
        QCOMPARE(certification->count(), 4);
        QVERIFY(!dialog.getEncryptionPassword());
        QVERIFY(!password->isEnabled());

        encrypt->setChecked(true);
        QVERIFY(password->isEnabled());
        password->setText(QStringLiteral("secret"));
        QCOMPARE(dialog.getEncryptionPassword().value_or(QString()), QStringLiteral("secret"));

        certification->setCurrentIndex(certification->count() - 1);
        QCOMPARE(dialog.getCertification(), pdf::PDFDocumentSigner::Certification::None);
        certification->setCurrentIndex(0);
        QCOMPARE(dialog.getCertification(), pdf::PDFDocumentSigner::Certification::NoChanges);
    }

    // An already signed document can be neither certified nor encrypted
    {
        pdfplugin::SignDialog dialog(nullptr, false, false);
        auto* certification = dialog.findChild<QComboBox*>("certificationCombo");
        auto* encrypt = dialog.findChild<QCheckBox*>("encryptCheckBox");
        QVERIFY(certification && encrypt);
        QVERIFY(!certification->isEnabled());
        QVERIFY(!encrypt->isEnabled());
        encrypt->setChecked(true);
        QCOMPARE(dialog.getCertification(), pdf::PDFDocumentSigner::Certification::None);
        QVERIFY(!dialog.getEncryptionPassword());
    }
}

void SignatureAppearanceTest::signatureLibraryDialogs()
{
    // A scan with a dark stroke on grey paper
    QImage scan(300, 120, QImage::Format_RGB32);
    scan.fill(QColor(230, 230, 225));
    {
        QPainter painter(&scan);
        painter.setPen(QPen(QColor(10, 20, 90), 5));
        painter.drawLine(40, 60, 260, 70);
    }

    pdfplugin::SignatureImportDialog importDialog(nullptr, scan);
    QVERIFY(!importDialog.getSignature().isNull());
    QCOMPARE(qAlpha(importDialog.getSignature().pixel(0, 0)), 0);

    // Moving the threshold to the paper level keeps no ink, the automatic mode is switched off
    auto* slider = importDialog.findChild<QSlider*>();
    auto* automatic = importDialog.findChild<QCheckBox*>();
    QVERIFY(slider && automatic);
    slider->setValue(1);
    QVERIFY(!automatic->isChecked());
    QVERIFY(importDialog.getSignature().isNull());
    automatic->setChecked(true);
    QVERIFY(!importDialog.getSignature().isNull());

    pdf::PDFSignatureLibrary library;
    library.setSlot(2, { QStringLiteral("Initials"), pdf::PDFSignatureLibrary::toPng(importDialog.getSignature()) });
    pdfplugin::SignatureLibraryDialog libraryDialog(nullptr, library);
    const QList<QLineEdit*> names = libraryDialog.findChildren<QLineEdit*>();
    QCOMPARE(names.size(), pdf::PDFSignatureLibrary::SLOT_COUNT);
    names[0]->setText(QStringLiteral("  Full signature "));
    const pdf::PDFSignatureLibrary edited = libraryDialog.getLibrary();
    QCOMPARE(edited.getSlot(0).name, QStringLiteral("Full signature"));
    QCOMPARE(edited.getSlot(2).png, library.getSlot(2).png);
    QVERIFY(!libraryDialog.grab().isNull());
}

void SignatureAppearanceTest::signatureImageKeepsTransparency()
{
    // A signature from the library is a PNG with a transparent background. Drawn into
    // the appearance it must stay transparent (an image with a soft mask), otherwise it
    // would cover the signature line with a white box.
    QImage signature(120, 40, QImage::Format_ARGB32);
    signature.fill(Qt::transparent);
    {
        QPainter painter(&signature);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(QColor(0, 0, 120), 4));
        painter.drawLine(5, 30, 115, 10);
    }

    PDFPageContentScene scene(nullptr);
    PDFPageContentImageElement* element = new PDFPageContentImageElement();
    element->setPageIndex(0);
    element->setRectangle(QRectF(50, 50, 120, 40));
    element->setContent(pdf::PDFSignatureLibrary::toPng(signature));
    QVERIFY(!element->getImage().isNull());
    scene.addElement(element);

    PDFContentStreamBuilder content(QSizeF(300, 400), PDFContentStreamBuilder::CoordinateSystem::PDF);
    QPainter* painter = content.begin();
    PDFColorConvertor convertor;
    QList<PDFRenderError> errors;
    PDFTextLayoutGetter nullGetter(nullptr, 0);
    scene.drawElements(painter, 0, nullGetter, QTransform(), nullptr, convertor, errors);
    const PDFContentStreamBuilder::ContentStream stream = content.end(painter);

    bool hasImageWithMask = false;
    const PDFObjectStorage& storage = stream.document.getStorage();
    for (const PDFObjectStorage::Entry& entry : storage.getObjects())
    {
        // Soft edges give a soft mask, an image whose alpha is only on/off a stencil mask
        const PDFDictionary* dictionary = entry.object.isStream() ? entry.object.getStream()->getDictionary() : nullptr;
        if (dictionary && dictionary->get("Subtype") == PDFObject::createName("Image") && (dictionary->hasKey("SMask") || dictionary->hasKey("Mask")))
        {
            hasImageWithMask = true;
        }
    }
    QVERIFY(hasImageWithMask);
}

void SignatureAppearanceTest::initialsDialog()
{
    QCoreApplication::setOrganizationName("PDF4QT-Tests");
    QCoreApplication::setApplicationName("InitialsDialog");
    QSettings().clear();

    pdf::PDFSignatureLibrary library;
    QImage image(10, 10, QImage::Format_ARGB32);
    image.fill(Qt::black);
    library.setSlot(3, { QStringLiteral("AP"), pdf::PDFSignatureLibrary::toPng(image) });

    pdfplugin::InitialsDialog dialog(nullptr, library, 6);
    auto* slotCombo = dialog.findChildren<QComboBox*>().value(0);
    QVERIFY(slotCombo);
    QCOMPARE(slotCombo->count(), 1);    // only the filled slots
    QCOMPARE(dialog.getSlotIndex(), 3);

    auto* skipPages = dialog.findChild<QLineEdit*>();
    QVERIFY(skipPages);
    skipPages->setText(QStringLiteral("1, 3-4"));
    dialog.accept();
    QCOMPARE(dialog.result(), int(QDialog::Accepted));
    QCOMPARE(dialog.getSettings().skippedPages, (std::set<pdf::PDFInteger>{ 0, 2, 3, 5 })); // last page skipped by default
    QVERIFY(qAbs(dialog.getSettings().width - 56.0) < 1.0);  // 20 mm

    // A wrong page range keeps the dialog open
    pdfplugin::InitialsDialog wrong(nullptr, library, 6);
    wrong.findChild<QLineEdit*>()->setText(QStringLiteral("9"));
    QTimer::singleShot(0, []() { if (QWidget* box = QApplication::activeModalWidget()) { box->close(); } });
    wrong.accept();
    QVERIFY(wrong.result() != int(QDialog::Accepted));
}

QTEST_MAIN(SignatureAppearanceTest)
#include "tst_signatureappearancetest.moc"
