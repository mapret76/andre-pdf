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

#include "signatureplugin.h"
#include "pdfdrawwidget.h"
#include "pdfutils.h"
#include "pdfpagecontenteditorwidget.h"
#include "pdfpagecontenteditorstylesettings.h"
#include "pdfdocumentbuilder.h"
#include "pdfcertificatemanagerdialog.h"
#include "signdialog.h"
#include "pdfdocumentsigner.h"
#include "pdfdocumentreader.h"
#include "pdfsecurityhandler.h"
#include "pdfsignaturestamp.h"
#include "signaturelibrarydialog.h"
#include "initialsdialog.h"
#include "contractsigningdialog.h"
#include "packsigningdialog.h"
#include "pdfcontractpack.h"
#include "pdfsignaturelinefinder.h"
#include "pdfpageinitials.h"

#include <QBuffer>
#include <QAction>
#include <QApplication>
#include <QToolButton>
#include <QMainWindow>
#include <QMessageBox>
#include <QFileDialog>
#include <QLineEdit>
#include <QInputDialog>
#include <QDir>

namespace pdfplugin
{

SignaturePlugin::SignaturePlugin() :
    pdf::PDFPlugin(nullptr),
    m_actions({ }),
    m_tools({ }),
    m_editorWidget(nullptr),
    m_scene(nullptr),
    m_sceneSelectionChangeEnabled(true)
{

}

void SignaturePlugin::setWidget(pdf::PDFWidget* widget)
{
    Q_ASSERT(!m_widget);

    BaseClass::setWidget(widget);

    QAction* activateAction = new QAction(QIcon(":/pdfplugins/signatureplugin/activate.svg"), tr("&Activate signature creator"), this);
    QAction* createTextAction = new QAction(QIcon(":/pdfplugins/signatureplugin/create-text.svg"), tr("Create &Text Label"), this);
    QAction* createFreehandCurveAction = new QAction(QIcon(":/pdfplugins/signatureplugin/create-freehand-curve.svg"), tr("Create &Freehand Curve"), this);
    QAction* createAcceptMarkAction = new QAction(QIcon(":/pdfplugins/signatureplugin/create-yes-mark.svg"), tr("Create &Accept Mark"), this);
    QAction* createRejectMarkAction = new QAction(QIcon(":/pdfplugins/signatureplugin/create-no-mark.svg"), tr("Create Reject &Mark"), this);
    QAction* createRectangleAction = new QAction(QIcon(":/pdfplugins/signatureplugin/create-rectangle.svg"), tr("Create &Rectangle"), this);
    QAction* createRoundedRectangleAction = new QAction(QIcon(":/pdfplugins/signatureplugin/create-rounded-rectangle.svg"), tr("Create R&ounded Rectangle"), this);
    QAction* createHorizontalLineAction = new QAction(QIcon(":/pdfplugins/signatureplugin/create-horizontal-line.svg"), tr("Create &Horizontal Line"), this);
    QAction* createVerticalLineAction = new QAction(QIcon(":/pdfplugins/signatureplugin/create-vertical-line.svg"), tr("Create &Vertical Line"), this);
    QAction* createLineAction = new QAction(QIcon(":/pdfplugins/signatureplugin/create-line.svg"), tr("Create &Line"), this);
    QAction* createDotAction = new QAction(QIcon(":/pdfplugins/signatureplugin/create-dot.svg"), tr("Create &Dot"), this);
    QAction* createSvgImageAction = new QAction(QIcon(":/pdfplugins/signatureplugin/create-svg-image.svg"), tr("Create SVG &Image"), this);
    std::array<QAction*, pdf::PDFSignatureLibrary::SLOT_COUNT> signatureSlotActions = { };
    for (int i = 0; i < pdf::PDFSignatureLibrary::SLOT_COUNT; ++i)
    {
        signatureSlotActions[i] = new QAction(QIcon(QString(":/pdfplugins/signatureplugin/signature-slot-%1.svg").arg(i + 1)), tr("Place Signature %1").arg(i + 1), this);
        signatureSlotActions[i]->setObjectName(QString("signaturetool_signatureSlot%1Action").arg(i + 1));
        signatureSlotActions[i]->setCheckable(true);
        m_actions[SignatureSlot1 + i] = signatureSlotActions[i];
    }
    QAction* signatureLibraryAction = new QAction(QIcon(":/pdfplugins/signatureplugin/signature-library.svg"), tr("Signature &Library..."), this);
    signatureLibraryAction->setObjectName("signaturetool_signatureLibraryAction");
    m_actions[SignatureLibrary] = signatureLibraryAction;
    QAction* initialPagesAction = new QAction(QIcon(":/pdfplugins/signatureplugin/initial-pages.svg"), tr("&Initial All Pages..."), this);
    initialPagesAction->setObjectName("signaturetool_initialPagesAction");
    m_actions[InitialPages] = initialPagesAction;
    QAction* signContractAction = new QAction(QIcon(":/pdfplugins/signatureplugin/sign-contract.svg"), tr("Sign &Contract..."), this);
    signContractAction->setObjectName("signaturetool_signContractAction");
    m_actions[SignContract] = signContractAction;
    QAction* signPackAction = new QAction(QIcon(":/pdfplugins/signatureplugin/sign-pack.svg"), tr("Split && Sign Contract &Pack..."), this);
    signPackAction->setObjectName("signaturetool_signPackAction");
    m_actions[SignPack] = signPackAction;
    QAction* clearAction = new QAction(QIcon(":/pdfplugins/signatureplugin/clear.svg"), tr("Clear All &Graphics"), this);
    QAction* signElectronicallyAction = new QAction(QIcon(":/pdfplugins/signatureplugin/sign-electronically.svg"), tr("Sign &Electronically"), this);
    QAction* signDigitallyAction = new QAction(QIcon(":/pdfplugins/signatureplugin/sign-digitally.svg"), tr("Sign Digitally &With Certificate"), this);
    QAction* certificatesAction = new QAction(QIcon(":/pdfplugins/signatureplugin/certificates.svg"), tr("Certificates &Manager"), this);

    activateAction->setObjectName("signaturetool_activateAction");
    createTextAction->setObjectName("signaturetool_createTextAction");
    createFreehandCurveAction->setObjectName("signaturetool_createFreehandCurveAction");
    createAcceptMarkAction->setObjectName("signaturetool_createAcceptMarkAction");
    createRejectMarkAction->setObjectName("signaturetool_createRejectMarkAction");
    createRectangleAction->setObjectName("signaturetool_createRectangleAction");
    createRoundedRectangleAction->setObjectName("signaturetool_createRoundedRectangleAction");
    createHorizontalLineAction->setObjectName("signaturetool_createHorizontalLineAction");
    createVerticalLineAction->setObjectName("signaturetool_createVerticalLineAction");
    createLineAction->setObjectName("signaturetool_createLineAction");
    createDotAction->setObjectName("signaturetool_createDotAction");
    createSvgImageAction->setObjectName("signaturetool_createSvgImageAction");
    clearAction->setObjectName("signaturetool_clearAction");
    signElectronicallyAction->setObjectName("signaturetool_signElectronicallyAction");
    signDigitallyAction->setObjectName("signaturetool_signDigitallyAction");
    certificatesAction->setObjectName("signaturetool_certificatesAction");

    activateAction->setCheckable(true);
    createTextAction->setCheckable(true);
    createFreehandCurveAction->setCheckable(true);
    createAcceptMarkAction->setCheckable(true);
    createRejectMarkAction->setCheckable(true);
    createRectangleAction->setCheckable(true);
    createRoundedRectangleAction->setCheckable(true);
    createHorizontalLineAction->setCheckable(true);
    createVerticalLineAction->setCheckable(true);
    createLineAction->setCheckable(true);
    createDotAction->setCheckable(true);
    createSvgImageAction->setCheckable(true);

    m_actions[Activate] = activateAction;
    m_actions[Text] = createTextAction;
    m_actions[FreehandCurve] = createFreehandCurveAction;
    m_actions[AcceptMark] = createAcceptMarkAction;
    m_actions[RejectMark] = createRejectMarkAction;
    m_actions[Rectangle] = createRectangleAction;
    m_actions[RoundedRectangle] = createRoundedRectangleAction;
    m_actions[HorizontalLine] = createHorizontalLineAction;
    m_actions[VerticalLine] = createVerticalLineAction;
    m_actions[Line] = createLineAction;
    m_actions[Dot] = createDotAction;
    m_actions[SvgImage] = createSvgImageAction;
    m_actions[Clear] = clearAction;
    m_actions[SignElectronically] = signElectronicallyAction;
    m_actions[SignDigitally] = signDigitallyAction;
    m_actions[Certificates] = certificatesAction;

    QFile acceptMarkFile(":/pdfplugins/signatureplugin/accept-mark.svg");
    QByteArray acceptMarkContent;
    if (acceptMarkFile.open(QFile::ReadOnly))
    {
        acceptMarkContent = acceptMarkFile.readAll();
        acceptMarkFile.close();
    }

    QFile rejectMarkFile(":/pdfplugins/signatureplugin/reject-mark.svg");
    QByteArray rejectMarkContent;
    if (rejectMarkFile.open(QFile::ReadOnly))
    {
        rejectMarkContent = rejectMarkFile.readAll();
        rejectMarkFile.close();
    }

    m_tools[TextTool] = new pdf::PDFCreatePCElementTextTool(widget->getDrawWidgetProxy(), &m_scene, createTextAction, this);
    m_tools[FreehandCurveTool] = new pdf::PDFCreatePCElementFreehandCurveTool(widget->getDrawWidgetProxy(), &m_scene, createFreehandCurveAction, this);
    m_tools[AcceptMarkTool] = new pdf::PDFCreatePCElementImageTool(widget->getDrawWidgetProxy(), &m_scene, createAcceptMarkAction, acceptMarkContent, false, this);
    m_tools[RejectMarkTool] = new pdf::PDFCreatePCElementImageTool(widget->getDrawWidgetProxy(), &m_scene, createRejectMarkAction, rejectMarkContent, false, this);
    m_tools[RectangleTool] = new pdf::PDFCreatePCElementRectangleTool(widget->getDrawWidgetProxy(), &m_scene, createRectangleAction, false, this);
    m_tools[RoundedRectangleTool] = new pdf::PDFCreatePCElementRectangleTool(widget->getDrawWidgetProxy(), &m_scene, createRoundedRectangleAction, true, this);
    m_tools[HorizontalLineTool] = new pdf::PDFCreatePCElementLineTool(widget->getDrawWidgetProxy(), &m_scene, createHorizontalLineAction, true, false, this);
    m_tools[VerticalLineTool] = new pdf::PDFCreatePCElementLineTool(widget->getDrawWidgetProxy(), &m_scene, createVerticalLineAction, false, true, this);
    m_tools[LineTool] = new pdf::PDFCreatePCElementLineTool(widget->getDrawWidgetProxy(), &m_scene, createLineAction, false, false, this);
    m_tools[DotTool] = new pdf::PDFCreatePCElementDotTool(widget->getDrawWidgetProxy(), &m_scene, createDotAction, this);
    m_tools[ImageTool] = new pdf::PDFCreatePCElementImageTool(widget->getDrawWidgetProxy(), &m_scene, createSvgImageAction, QByteArray(), true, this);
    for (int i = 0; i < pdf::PDFSignatureLibrary::SLOT_COUNT; ++i)
    {
        m_tools[SignatureSlot1Tool + i] = new pdf::PDFCreatePCElementImageTool(widget->getDrawWidgetProxy(), &m_scene, signatureSlotActions[i], QByteArray(), false, this);
    }

    QString libraryErrorMessage;
    if (!m_signatureLibrary.load(pdf::PDFSignatureLibrary::getDefaultFileName(), &libraryErrorMessage))
    {
        QMessageBox::warning(widget, tr("Signature Library"), libraryErrorMessage);
    }
    updateSignatureSlots();

    pdf::PDFToolManager* toolManager = widget->getToolManager();
    for (pdf::PDFWidgetTool* tool : m_tools)
    {
        toolManager->addTool(tool);
        connect(tool, &pdf::PDFWidgetTool::toolActivityChanged, this, &SignaturePlugin::onToolActivityChanged);
    }

    m_widget->addInputInterface(&m_scene);
    m_widget->getDrawWidgetProxy()->registerDrawInterface(&m_scene);
    m_scene.setWidget(m_widget);
    connect(&m_scene, &pdf::PDFPageContentScene::sceneChanged, this, &SignaturePlugin::onSceneChanged);
    connect(&m_scene, &pdf::PDFPageContentScene::selectionChanged, this, &SignaturePlugin::onSceneSelectionChanged);
    connect(&m_scene, &pdf::PDFPageContentScene::editElementRequest, this, &SignaturePlugin::onSceneEditElement);
    connect(clearAction, &QAction::triggered, &m_scene, &pdf::PDFPageContentScene::clear);
    connect(activateAction, &QAction::triggered, this, &SignaturePlugin::setActive);
    connect(signElectronicallyAction, &QAction::triggered, this, &SignaturePlugin::onSignElectronically);
    connect(signDigitallyAction, &QAction::triggered, this, &SignaturePlugin::onSignDigitally);
    connect(certificatesAction, &QAction::triggered, this, &SignaturePlugin::onOpenCertificatesManager);
    connect(signatureLibraryAction, &QAction::triggered, this, &SignaturePlugin::onOpenSignatureLibrary);
    connect(initialPagesAction, &QAction::triggered, this, &SignaturePlugin::onInitialPages);
    connect(signContractAction, &QAction::triggered, this, &SignaturePlugin::onSignContract);
    connect(signPackAction, &QAction::triggered, this, &SignaturePlugin::onSignPack);
    connect(m_widget, &pdf::PDFWidget::sceneActivityChanged, this, &SignaturePlugin::onSceneActivityChanged);

    updateActions();
}

void SignaturePlugin::setDocument(const pdf::PDFModifiedDocument& document)
{
    BaseClass::setDocument(document);

    if (document.hasReset())
    {
        setActive(false);
        updateActions();
    }
}

std::vector<QAction*> SignaturePlugin::getActions() const
{
    std::vector<QAction*> result;

    result.push_back(m_actions[Activate]);
    result.push_back(m_actions[SignElectronically]);
    result.push_back(m_actions[SignDigitally]);
    result.push_back(m_actions[Certificates]);
    result.push_back(m_actions[SignatureLibrary]);
    result.push_back(m_actions[InitialPages]);
    result.push_back(m_actions[SignContract]);
    result.push_back(m_actions[SignPack]);

    return result;
}

QString SignaturePlugin::getPluginMenuName() const
{
    return tr("Si&gnature");
}

void SignaturePlugin::onSceneChanged(bool graphicsOnly)
{
    if (!graphicsOnly)
    {
        updateActions();
    }

    if (m_editorWidget)
    {
        m_editorWidget->updateItemsInListWidget();
    }

    updateGraphics();
}

void SignaturePlugin::onSceneSelectionChanged()
{
    if (m_editorWidget && m_sceneSelectionChangeEnabled)
    {
        m_editorWidget->setSelection(m_scene.getSelectedElementIds());
    }
}

void SignaturePlugin::onWidgetSelectionChanged()
{
    Q_ASSERT(m_editorWidget);

    pdf::PDFTemporaryValueChange guard(&m_sceneSelectionChangeEnabled, false);
    m_scene.setSelectedElementIds(m_editorWidget->getSelection());
}

pdf::PDFWidgetTool* SignaturePlugin::getActiveTool()
{
    for (pdf::PDFWidgetTool* currentTool : m_tools)
    {
        if (currentTool->isActive())
        {
            return currentTool;
        }
    }

    return nullptr;
}

void SignaturePlugin::onToolActivityChanged()
{
    if (m_editorWidget)
    {
        pdf::PDFWidgetTool* activeTool = getActiveTool();

        const pdf::PDFPageContentElement* element = nullptr;
        pdf::PDFCreatePCElementTool* tool = qobject_cast<pdf::PDFCreatePCElementTool*>(activeTool);
        if (tool)
        {
            element = tool->getElement();
        }

        m_editorWidget->loadStyleFromElement(element);
    }
}

void SignaturePlugin::onSceneEditElement(const std::set<pdf::PDFInteger>& elements)
{
    if (elements.empty())
    {
        return;
    }

    pdf::PDFPageContentElement* element = nullptr;
    for (pdf::PDFInteger id : elements)
    {
        element = m_scene.getElementById(id);
        if (element)
        {
            break;
        }
    }

    if (!element)
    {
        return;
    }

    if (pdf::PDFPageContentEditorStyleSettings::showEditElementStyleDialog(m_dataExchangeInterface->getMainWindow(), element))
    {
        updateGraphics();
    }
}

void SignaturePlugin::onSignElectronically()
{
    Q_ASSERT(m_document);
    Q_ASSERT(!m_scene.isEmpty());

    pdf::PDFColorConvertor convertor;

    if (QMessageBox::question(m_dataExchangeInterface->getMainWindow(), tr("Confirm Signature"), tr("Document will be signed electronically. Do you want to continue?"), QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes)
    {
        pdf::PDFDocumentModifier modifier(m_document);

        std::set<pdf::PDFInteger> pageIndices = m_scene.getPageIndices();
        for (pdf::PDFInteger pageIndex : pageIndices)
        {
            const pdf::PDFPage* page = m_document->getCatalog()->getPage(pageIndex);
            pdf::PDFPageContentStreamBuilder pageContentStreamBuilder(modifier.getBuilder(),
                                                                      pdf::PDFContentStreamBuilder::CoordinateSystem::PDF,
                                                                      pdf::PDFPageContentStreamBuilder::Mode::PlaceAfter);
            QPainter* painter = pageContentStreamBuilder.begin(page->getPageReference());
            QList<pdf::PDFRenderError> errors;
            pdf::PDFTextLayoutGetter nullGetter(nullptr, pageIndex);
            m_scene.drawElements(painter, pageIndex, nullGetter, QTransform(), nullptr, convertor, errors);
            pageContentStreamBuilder.end(painter);
            modifier.markPageContentsChanged();
        }
        m_scene.clear();

        if (modifier.finalize())
        {
            Q_EMIT m_widget->getToolManager()->documentModified(pdf::PDFModifiedDocument(modifier.getDocument(), nullptr, modifier.getFlags()));
        }
    }
}

std::optional<QString> SignaturePlugin::getOpenPassword(const QByteArray& originalData)
{
    auto opens = [&originalData](const QString& password)
    {
        bool isOffered = false;
        pdf::PDFDocumentReader reader(nullptr, [&](bool* ok) { *ok = !isOffered; isOffered = true; return password; }, false, false);
        reader.readFromBuffer(originalData);
        return reader.getReadingResult() == pdf::PDFDocumentReader::Result::OK;
    };
    if (opens(QString()))
    {
        return QString();
    }

    for (int attempt = 0; attempt < 3; ++attempt)
    {
        bool ok = false;
        const QString password = QInputDialog::getText(m_dataExchangeInterface->getMainWindow(), tr("Encrypted Document"),
                                                       attempt == 0 ? tr("Password of the document (needed to add the signature to it):") : tr("The password is wrong. Password of the document:"),
                                                       QLineEdit::Password, QString(), &ok);
        if (!ok)
        {
            return std::nullopt;
        }
        if (opens(password))
        {
            return password;
        }
    }
    return std::nullopt;
}

bool SignaturePlugin::onSignDigitally()
{
    // Jakub Melka: do we have certificates? If not,
    // open certificate dialog, so the user can create
    // a new one. A document timestamp is signed by the timestamp authority
    // and not by the user, so we continue even when no certificate exists -
    // the dialog reports the missing certificate for the other signature types.
    if (pdf::PDFCertificateManager::getCertificates(pdf::PDFCertificateUsageFilter::DigitalSignature).empty())
    {
        onOpenCertificatesManager();
    }

    SignDialog dialog(m_dataExchangeInterface->getMainWindow(), m_scene.isEmpty(), !pdf::PDFDocumentSigner::hasExistingSignatures(m_document));
    if (m_suggestedCertification)
    {
        dialog.setCertification(*m_suggestedCertification);
        m_suggestedCertification.reset();
    }
    if (dialog.exec() != SignDialog::Accepted)
    {
        return false;
    }
    {
        const SignDialog::SignatureType signatureType = dialog.getSignatureType();

        // A document timestamp only attests, that the document existed at the time
        // of the timestamp - it has no signer and no visible appearance.
        const bool isDocumentTimestamp = signatureType == SignDialog::TimestampOnly;

        bool visibleSignature = !isDocumentTimestamp && dialog.getSignMethod() == SignDialog::SignDigitally;
        const std::set<pdf::PDFInteger> pageIndices = m_scene.getPageIndices();
        if (visibleSignature && pageIndices.empty())
        {
            if (QMessageBox::warning(m_widget, tr("Confirm Signature"),
                                    tr("No signature graphics were created, the signature will have no visible appearance. Continue?"),
                                    QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
            {
                return false;
            }
            visibleSignature = false;
        }
        else if (visibleSignature && pageIndices.size() > 1)
        {
            if (QMessageBox::warning(m_widget, tr("Confirm Signature"),
                                    tr("Signature graphics span multiple pages. Only the first page containing graphics will be used. Continue?"),
                                    QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
            {
                return false;
            }
        }

        const pdf::PDFCatalog* catalog = m_document->getCatalog();
        if (catalog->getPageCount() == 0)
        {
            QMessageBox::critical(m_widget, tr("Error"), tr("The document has no page for a signature widget."));
            return false;
        }

        const pdf::PDFCertificateEntry* certificate = dialog.getCertificate();
        Q_ASSERT(certificate || isDocumentTimestamp);

        const QString signatureNamePrefix = isDocumentTimestamp ? QString("pdf4qt_timestamp_") : QString("pdf4qt_signature_");
        const QString signatureName = signatureNamePrefix + QString::number(QDateTime::currentMSecsSinceEpoch());
        const QString reasonText = isDocumentTimestamp ? QString() : dialog.getReasonText();
        const QString contactInfoText = isDocumentTimestamp ? QString() : dialog.getContactInfoText();

        pdf::PDFDocumentSigner::Parameters parameters;
        parameters.document = m_document;
        parameters.progress = m_widget->getDrawWidgetProxy()->getProgress();
        parameters.certification = dialog.getCertification();
        parameters.encryptionUserPassword = dialog.getEncryptionPassword();

        if (isDocumentTimestamp)
        {
            parameters.subfilter = "ETSI.RFC3161";
            parameters.signatureDictionaryType = "DocTimeStamp";
        }

        // The signed document is written as an incremental update of the file
        // the document was loaded from, so the signatures already present in it
        // stay valid.
        QFile originalFile(m_dataExchangeInterface->getOriginalFileName());
        if (originalFile.open(QFile::ReadOnly))
        {
            parameters.originalDocumentData = originalFile.readAll();
            originalFile.close();
        }

        // An encrypted original is read again for the incremental update, with its password
        const pdf::PDFSecurityHandler* securityHandler = m_document->getStorage().getSecurityHandler();
        if (securityHandler && securityHandler->getMode() != pdf::EncryptionMode::None && !parameters.originalDocumentData.isEmpty())
        {
            const std::optional<QString> openPassword = getOpenPassword(parameters.originalDocumentData);
            if (!openPassword)
            {
                return false;
            }
            parameters.openPassword = *openPassword;
        }

        // A lock "no changes" would stop the other party, which still has to sign
        if (m_isOtherPartySigning && parameters.certification == pdf::PDFDocumentSigner::Certification::NoChanges &&
            QMessageBox::warning(m_widget, tr("Sign Contract"),
                                 tr("Lines are left for another party, but the lock \"No changes allowed\" would stop them from signing. Lock anyway?"),
                                 QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        {
            return false;
        }

        // Verification stamp drawn under the visible signature: who signed, when, how
        // the document is locked, and the fingerprints of the certificate and of the
        // file before this signature, also as a QR code. The proof itself is the
        // digital signature, which the reader verifies in the signature panel.
        std::optional<pdf::PDFSignatureStamp::Data> stamp;
        if (visibleSignature)
        {
            // The file fingerprint can be checked only where the signature is appended to the
            // original bytes; encryption writes the document anew
            const QByteArray fingerprinted = parameters.encryptionUserPassword ? QByteArray() : parameters.originalDocumentData;
            stamp = pdf::PDFSignatureStamp::create(certificate->info, parameters.signingTime, parameters.certification, fingerprinted);
        }

        pdf::PDFSignatureFactory::TimestampSettings timestampSettings;
        timestampSettings.url = dialog.getTimestampUrl();

        // Jakub Melka: the failures of the timestamping are described in detail,
        // because most of them are caused by the timestamp authority and not by
        // the signing itself.
        QString timestampErrorMessage;

        switch (signatureType)
        {
            case SignDialog::SignatureOnly:
                parameters.signFunction = [certificate, &dialog](const QByteArray& dataToBeSigned, QByteArray& signature)
                {
                    return pdf::PDFSignatureFactory::sign(*certificate, dialog.getPassword(), dataToBeSigned, signature);
                };
                break;

            case SignDialog::SignatureWithTimestamp:
                parameters.signFunction = [certificate, &dialog, timestampSettings, &timestampErrorMessage](const QByteArray& dataToBeSigned, QByteArray& signature)
                {
                    return pdf::PDFSignatureFactory::signWithTimestamp(*certificate, dialog.getPassword(), dataToBeSigned, timestampSettings, signature, timestampErrorMessage);
                };
                break;

            case SignDialog::TimestampOnly:
                parameters.signFunction = [timestampSettings, &timestampErrorMessage](const QByteArray& dataToBeSigned, QByteArray& signature)
                {
                    return pdf::PDFSignatureFactory::createTimestampToken(dataToBeSigned, timestampSettings, signature, timestampErrorMessage);
                };
                break;

            default:
                Q_ASSERT(false);
                return false;
        }

        // Jakub Melka: the signature field is created by the signer, because the
        // document must be built again, when the space reserved for the signature
        // turns out to be too small.
        parameters.createSignatureFieldFunction = [&](pdf::PDFDocumentBuilder& builder, pdf::PDFObjectReference signatureDictionary)
        {
            pdf::PDFObjectReference signatureField;

            if (!visibleSignature)
            {
                signatureField = builder.createSignatureField(signatureName, signatureDictionary, catalog->getPage(0)->getPageReference());
            }
            else
            {
                const pdf::PDFInteger pageIndex = *pageIndices.begin();
                const pdf::PDFPage* page = catalog->getPage(pageIndex);
                pdf::PDFColorConvertor convertor;

                pdf::PDFContentStreamBuilder contentBuilder(page->getMediaBox().size(), pdf::PDFContentStreamBuilder::CoordinateSystem::PDF);
                QPainter* painter = contentBuilder.begin();
                // Scene elements use unrotated PDF coordinates, including the MediaBox origin.
                // QPdfWriter's temporary page starts at zero; keep the Form BBox in that space.
                const QPointF mediaOrigin = page->getMediaBox().topLeft();
                painter->translate(-mediaOrigin);
                QList<pdf::PDFRenderError> errors;
                pdf::PDFTextLayoutGetter nullGetter(nullptr, pageIndex);
                m_scene.drawElements(painter, pageIndex, nullGetter, QTransform(), nullptr, convertor, errors);

                // The stamp is placed under the signature graphics (PDF coordinates, y goes up)
                QRectF boundingRect = m_scene.getBoundingBox(pageIndex, true);
                if (stamp)
                {
                    const QSizeF stampSize = pdf::PDFSignatureStamp::getSize(*stamp);
                    QRectF stampRect(boundingRect.left(), boundingRect.top() - stampSize.height(), stampSize.width(), stampSize.height());
                    if (stampRect.top() < page->getMediaBox().top())
                    {
                        // No room under the graphics, the stamp goes above them
                        stampRect.moveTop(boundingRect.bottom());
                    }
                    pdf::PDFSignatureStamp::draw(painter, stampRect, *stamp);
                    boundingRect = boundingRect.united(stampRect);
                }

                pdf::PDFContentStreamBuilder::ContentStream contentStream = contentBuilder.end(painter);
                std::vector<pdf::PDFObject> copiedObjects = builder.copyFrom({ contentStream.resources, contentStream.contents }, contentStream.document.getStorage(), true);
                Q_ASSERT(copiedObjects.size() == 2);

                pdf::PDFObjectReference resourcesReference = copiedObjects[0].getReference();
                pdf::PDFObjectReference formReference = copiedObjects[1].getReference();

                // Create form object
                pdf::PDFObjectFactory formFactory;

                formFactory.beginDictionary();

                formFactory.beginDictionaryItem("Type");
                formFactory << pdf::WrapName("XObject");
                formFactory.endDictionaryItem();

                formFactory.beginDictionaryItem("Subtype");
                formFactory << pdf::WrapName("Form");
                formFactory.endDictionaryItem();

                formFactory.beginDictionaryItem("BBox");
                formFactory << boundingRect.translated(-mediaOrigin);
                formFactory.endDictionaryItem();

                formFactory.beginDictionaryItem("Resources");
                formFactory << resourcesReference;
                formFactory.endDictionaryItem();

                formFactory.endDictionary();

                builder.mergeTo(formReference, formFactory.takeObject());

                signatureField = builder.createSignatureField(signatureName, signatureDictionary, page->getPageReference(), formReference, boundingRect);
            }

            if (!reasonText.isEmpty())
            {
                builder.setSignatureReason(signatureDictionary, reasonText);
            }

            if (!contactInfoText.isEmpty())
            {
                builder.setSignatureContactInfo(signatureDictionary, contactInfoText);
            }

            return signatureField;
        };

        // Creating the timestamp needs the timestamp authority to be contacted over
        // the network, so the signing can take a while.
        QByteArray signedDocument;
        QApplication::setOverrideCursor(Qt::WaitCursor);
        pdf::PDFDocumentSigner::Result signingResult = pdf::PDFDocumentSigner::Result::SigningFailed;
        QString exceptionMessage;
        try
        {
            signingResult = pdf::PDFDocumentSigner::sign(parameters, signedDocument);
        }
        catch (const std::exception& exception)
        {
            exceptionMessage = QString::fromLocal8Bit(exception.what());
        }
        QApplication::restoreOverrideCursor();

        if (signingResult != pdf::PDFDocumentSigner::Result::OK)
        {
            const QString errorMessage = !exceptionMessage.isEmpty() ? tr("The document could not be signed: %1").arg(exceptionMessage)
                                                                     : (!timestampErrorMessage.isEmpty() ? timestampErrorMessage : pdf::PDFDocumentSigner::getResultMessage(signingResult));
            QMessageBox::critical(m_widget, tr("Error"), errorMessage);
            return false;
        }

        QString fileName = QFileDialog::getSaveFileName(m_dataExchangeInterface->getMainWindow(), tr("Save Signed Document"), getSignedFileName(), tr("Portable Document (*.pdf);;All files (*.*)"));
        if (fileName.isEmpty())
        {
            return false;
        }

        QFile signedFile(fileName);
        if (!signedFile.open(QFile::WriteOnly | QFile::Truncate) || signedFile.write(signedDocument) != signedDocument.size())
        {
            QMessageBox::critical(m_widget, tr("Error"), tr("Failed to save the signed document."));
            return false;
        }
        signedFile.close();
        checkSavedFile(fileName, signatureName, parameters.encryptionUserPassword.value_or(parameters.openPassword), isDocumentTimestamp);
        return true;
    }
}

void SignaturePlugin::checkSavedFile(const QString& fileName, const QString& signatureName, const QString& password, bool isDocumentTimestamp)
{
    // The file is read back from the disk, as a reader will get it, so that what is
    // reported is what was really saved
    QFile file(fileName);
    QByteArray data;
    if (file.open(QFile::ReadOnly))
    {
        data = file.readAll();
        file.close();
    }

    const pdf::PDFDocumentSigner::SignedFileCheck check = pdf::PDFDocumentSigner::checkSignedFile(data, signatureName, password);
    if (!check.isOK())
    {
        QString message = tr("The saved file %1 did not pass the check after saving: ").arg(QDir::toNativeSeparators(fileName));
        if (!check.isOpened)
        {
            message += tr("it cannot be opened.");
        }
        else if (!check.isSignatureFound)
        {
            message += tr("the signature was not found.");
        }
        else
        {
            message += tr("the signature is not valid.\n%1").arg(check.errors.join('\n'));
        }
        QMessageBox::critical(m_widget, tr("Signed Document Check"), message);
        return;
    }

    QStringList lines;
    lines << (isDocumentTimestamp ? tr("The document timestamp was saved and checked:") : tr("The document was signed, saved and checked:"));
    lines << tr("- the signature is valid and covers the whole file");
    const QString certificationText = pdf::PDFSignatureStamp::getCertificationText(check.certification);
    lines << (certificationText.isEmpty() ? tr("- the document is not locked (approval signature)") : tr("- %1").arg(certificationText));
    if (check.isEncrypted)
    {
        lines << tr("- the document is encrypted, the password is needed to open it");
    }
    lines << QString();
    lines << tr("Readers check the signature in their signature panel. Trust in the certificate is their decision.");
    QMessageBox::information(m_widget, tr("Signed Document Check"), lines.join('\n'));
}

void SignaturePlugin::onOpenSignatureLibrary()
{
    SignatureLibraryDialog dialog(m_dataExchangeInterface->getMainWindow(), m_signatureLibrary);
    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    pdf::PDFSignatureLibrary library = dialog.getLibrary();
    QString errorMessage;
    if (!library.save(pdf::PDFSignatureLibrary::getDefaultFileName(), &errorMessage))
    {
        QMessageBox::critical(m_widget, tr("Signature Library"), errorMessage);
        return;
    }

    m_signatureLibrary = std::move(library);
    updateSignatureSlots();
    updateActions();
}

void SignaturePlugin::onInitialPages()
{
    Q_ASSERT(m_document);

    if (pdf::PDFDocumentSigner::hasExistingSignatures(m_document))
    {
        QMessageBox::critical(m_widget, tr("Initial All Pages"), tr("The document is already signed. Initialling the pages now would break the signatures - initial before signing."));
        return;
    }

    InitialsDialog dialog(m_dataExchangeInterface->getMainWindow(), m_signatureLibrary, pdf::PDFInteger(m_document->getCatalog()->getPageCount()));
    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    QImage initials;
    initials.loadFromData(m_signatureLibrary.getSlot(dialog.getSlotIndex()).png, "PNG");
    pdf::PDFDocumentPointer document = pdf::PDFPageInitials::apply(m_document, initials, dialog.getSettings());
    if (!document)
    {
        QMessageBox::information(m_widget, tr("Initial All Pages"), tr("No page was initialled."));
        return;
    }

    pdf::PDFModifiedDocument::ModificationFlags flags;
    flags.setFlag(pdf::PDFModifiedDocument::PageContents);
    Q_EMIT m_widget->getToolManager()->documentModified(pdf::PDFModifiedDocument(document, nullptr, flags));
}

void SignaturePlugin::onSignContract()
{
    Q_ASSERT(m_document);

    if (pdf::PDFDocumentSigner::hasExistingSignatures(m_document))
    {
        QMessageBox::information(m_widget, tr("Sign Contract"),
                                 tr("The document is already signed by another party. Place your signature with the Place Signature tools "
                                    "and add your signature with Sign Digitally - the existing signatures stay valid."));
        return;
    }

    QApplication::setOverrideCursor(Qt::WaitCursor);
    std::vector<pdf::PDFSignatureLineFinder::SignatureLine> lines = pdf::PDFSignatureLineFinder::find(m_document);
    QApplication::restoreOverrideCursor();

    if (lines.empty())
    {
        QMessageBox::information(m_widget, tr("Sign Contract"),
                                 tr("No signature lines were found. Place your signature with the Place Signature tools and sign with Sign Digitally."));
        return;
    }

    pdf::PDFDrawWidgetProxy* proxy = m_widget->getDrawWidgetProxy();
    ContractSigningDialog dialog(m_dataExchangeInterface->getMainWindow(), std::move(lines), m_signatureLibrary, [proxy](pdf::PDFInteger pageIndex) { proxy->goToPage(pageIndex); });
    if (dialog.exec() != QDialog::Accepted || !dialog.isComplete())
    {
        return;
    }

    QImage signature;
    signature.loadFromData(m_signatureLibrary.getSlot(dialog.getSlotIndex()).png, "PNG");

    std::vector<pdf::PDFPageInitials::Placement> placements;
    std::vector<pdf::PDFSignatureLineFinder::SignatureLine> otherLines;
    const std::vector<ContractSigningDialog::Decision> decisions = dialog.getDecisions();
    for (size_t i = 0; i < decisions.size(); ++i)
    {
        if (decisions[i] == ContractSigningDialog::Decision::Sign)
        {
            placements.push_back({ dialog.getLines()[i].pageIndex, dialog.getLines()[i].rectangle });
        }
        else if (decisions[i] == ContractSigningDialog::Decision::OtherParty)
        {
            otherLines.push_back(dialog.getLines()[i]);
        }
    }

    // The document before the workflow comes back, when it does not end signed
    const pdf::PDFDocumentPointer original(new pdf::PDFDocument(*m_document));

    // The image of the last line is the visible appearance of the digital signature,
    // with the verification stamp; the other lines get the image into the page. On a
    // page turned by /Rotate the appearance would be drawn turned, so there every line
    // gets the image into the page and the digital signature is invisible.
    const pdf::PDFPageInitials::Placement lastPlacement = placements.back();
    const pdf::PDFPage* lastPage = m_document->getCatalog()->getPage(lastPlacement.pageIndex);
    const bool isLastOnRotatedPage = lastPage->getPageRotation() != pdf::PageRotation::None;
    if (!isLastOnRotatedPage)
    {
        placements.pop_back();
    }

    pdf::PDFDocumentPointer document = original;
    if (!placements.empty())
    {
        document = pdf::PDFPageInitials::drawImages(document.data(), signature, placements);
        if (!document)
        {
            QMessageBox::critical(m_widget, tr("Sign Contract"), tr("The signature could not be placed on the lines."));
            return;
        }
    }

    // The other party gets empty signature fields on its lines, which a reader offers to sign
    if (!otherLines.empty())
    {
        if (pdf::PDFDocumentPointer withFields = pdf::PDFSignatureLineFinder::addSignatureFields(document.data(), otherLines, tr("Signature of other party ")))
        {
            document = withFields;
        }
    }

    pdf::PDFModifiedDocument::ModificationFlags flags;
    flags.setFlag(pdf::PDFModifiedDocument::PageContents);
    flags.setFlag(pdf::PDFModifiedDocument::FormField);
    flags.setFlag(pdf::PDFModifiedDocument::Annotation);
    if (document != original)
    {
        Q_EMIT m_widget->getToolManager()->documentModified(pdf::PDFModifiedDocument(document, nullptr, flags));
    }

    setActive(true);
    m_scene.clear();
    if (!isLastOnRotatedPage)
    {
        pdf::PDFPageContentImageElement* element = new pdf::PDFPageContentImageElement();
        element->setPageIndex(lastPlacement.pageIndex);
        element->setRectangle(pdf::PDFPageInitials::getDisplayToPageTransform(lastPage).mapRect(pdf::PDFPageInitials::getFittedDisplayRectangle(lastPage, lastPlacement.area, signature.size())));
        element->setContent(m_signatureLibrary.getSlot(dialog.getSlotIndex()).png);
        m_scene.addElement(element);
    }

    // Another party still signs: certify so that signing stays allowed (P = 2)
    m_isOtherPartySigning = !otherLines.empty();
    m_suggestedCertification = m_isOtherPartySigning ? pdf::PDFDocumentSigner::Certification::FormFilling : pdf::PDFDocumentSigner::Certification::NoChanges;
    const bool isSigned = onSignDigitally();
    m_suggestedCertification.reset();
    m_isOtherPartySigning = false;

    if (!isSigned)
    {
        // Nothing half done stays in the document
        m_scene.clear();
        if (document != original)
        {
            Q_EMIT m_widget->getToolManager()->documentModified(pdf::PDFModifiedDocument(original, nullptr, flags));
        }
        QMessageBox::information(m_widget, tr("Sign Contract"), tr("The contract was not signed; the document is as it was before."));
    }
}

void SignaturePlugin::onSignPack()
{
    Q_ASSERT(m_document);

    if (pdf::PDFDocumentSigner::hasExistingSignatures(m_document))
    {
        QMessageBox::information(m_widget, tr("Split and Sign Contract Pack"), tr("The pack is already signed, splitting it would break the signatures."));
        return;
    }

    PackSigningDialog packDialog(m_dataExchangeInterface->getMainWindow(), m_document, m_signatureLibrary, m_dataExchangeInterface->getOriginalFileName());
    if (packDialog.exec() != QDialog::Accepted)
    {
        return;
    }

    if (pdf::PDFCertificateManager::getCertificates(pdf::PDFCertificateUsageFilter::DigitalSignature).empty())
    {
        onOpenCertificatesManager();
    }

    // One certificate, one password for the whole pack
    SignDialog signDialog(m_dataExchangeInterface->getMainWindow(), false, true);
    signDialog.setCertification(pdf::PDFDocumentSigner::Certification::NoChanges);
    if (signDialog.exec() != QDialog::Accepted)
    {
        return;
    }
    if (signDialog.getSignatureType() == SignDialog::TimestampOnly || !signDialog.getCertificate())
    {
        QMessageBox::critical(m_widget, tr("Split and Sign Contract Pack"), tr("Choose a signature with your certificate, a document timestamp alone does not sign the contracts."));
        return;
    }

    const pdf::PDFCertificateEntry* certificate = signDialog.getCertificate();
    QImage signature;
    signature.loadFromData(m_signatureLibrary.getSlot(packDialog.getSignatureSlot()).png, "PNG");
    QImage initials;
    if (packDialog.getInitialsSlot() >= 0)
    {
        initials.loadFromData(m_signatureLibrary.getSlot(packDialog.getInitialsSlot()).png, "PNG");
    }

    QString timestampErrorMessage;
    pdf::PDFSignatureFactory::TimestampSettings timestampSettings;
    timestampSettings.url = signDialog.getTimestampUrl();
    pdf::PDFDocumentSigner::SignFunction signFunction;
    if (signDialog.getSignatureType() == SignDialog::SignatureWithTimestamp)
    {
        signFunction = [certificate, &signDialog, timestampSettings, &timestampErrorMessage](const QByteArray& data, QByteArray& result)
        {
            return pdf::PDFSignatureFactory::signWithTimestamp(*certificate, signDialog.getPassword(), data, timestampSettings, result, timestampErrorMessage);
        };
    }
    else
    {
        signFunction = [certificate, &signDialog](const QByteArray& data, QByteArray& result)
        {
            return pdf::PDFSignatureFactory::sign(*certificate, signDialog.getPassword(), data, result);
        };
    }

    QStringList report;
    int failures = 0;
    const pdf::PDFContractPack::Parts parts = packDialog.getParts();
    QApplication::setOverrideCursor(Qt::WaitCursor);
    for (size_t partIndex = 0; partIndex < parts.size(); ++partIndex)
    {
        const pdf::PDFContractPack::Part& part = parts[partIndex];
        pdf::PDFDocumentPointer document(new pdf::PDFDocument(pdf::PDFContractPack::extract(m_document, part)));
        if (!document->getCatalog() || document->getCatalog()->getPageCount() == 0)
        {
            report << tr("%1: the pages could not be extracted.").arg(part.name);
            ++failures;
            continue;
        }

        // Order of the rules: split -> initial -> sign the lines -> lock last
        if (!initials.isNull())
        {
            pdf::PDFPageInitials::Settings settings;
            if (packDialog.isLastPageSkippedByInitials() && document->getCatalog()->getPageCount() > 1)
            {
                settings.skippedPages.insert(pdf::PDFInteger(document->getCatalog()->getPageCount()) - 1);
            }
            if (pdf::PDFDocumentPointer initialled = pdf::PDFPageInitials::apply(document.data(), initials, settings))
            {
                document = initialled;
            }
        }

        const QString filter = packDialog.getLineFilter();
        std::vector<pdf::PDFPageInitials::Placement> placements;
        std::vector<pdf::PDFSignatureLineFinder::SignatureLine> otherPartyLines;
        QStringList signedLabels;
        for (const pdf::PDFSignatureLineFinder::SignatureLine& line : pdf::PDFSignatureLineFinder::find(document.data()))
        {
            if (filter.isEmpty() || line.label.contains(filter, Qt::CaseInsensitive) || line.fieldName.contains(filter, Qt::CaseInsensitive))
            {
                placements.push_back({ line.pageIndex, line.rectangle });
                signedLabels << tr("p. %1 \"%2\"").arg(line.pageIndex + 1).arg(line.label.left(40));
            }
            else
            {
                otherPartyLines.push_back(line);
            }
        }
        const size_t otherLines = otherPartyLines.size();
        if (placements.empty())
        {
            report << tr("%1: no signature line of yours was found, the contract was not signed.").arg(part.name);
            ++failures;
            continue;
        }

        const pdf::PDFPageInitials::Placement lastPlacement = placements.back();
        placements.pop_back();
        if (!placements.empty())
        {
            if (pdf::PDFDocumentPointer drawn = pdf::PDFPageInitials::drawImages(document.data(), signature, placements))
            {
                document = drawn;
            }
        }
        // The lines of the other parties get empty signature fields to sign in
        if (!otherPartyLines.empty())
        {
            if (pdf::PDFDocumentPointer withFields = pdf::PDFSignatureLineFinder::addSignatureFields(document.data(), otherPartyLines, tr("Signature of other party ")))
            {
                document = withFields;
            }
        }

        pdf::PDFDocumentSigner::Parameters parameters;
        parameters.document = document.data();
        parameters.certification = signDialog.getCertification();
        if (otherLines > 0 && parameters.certification == pdf::PDFDocumentSigner::Certification::NoChanges)
        {
            // The other party must still be able to sign
            parameters.certification = pdf::PDFDocumentSigner::Certification::FormFilling;
        }
        parameters.encryptionUserPassword = signDialog.getEncryptionPassword();
        parameters.signFunction = signFunction;

        const pdf::PDFPage* lastPage = document->getCatalog()->getPage(lastPlacement.pageIndex);
        const QRectF imageRectangle = pdf::PDFPageInitials::getDisplayToPageTransform(lastPage).mapRect(pdf::PDFPageInitials::getFittedDisplayRectangle(lastPage, lastPlacement.area, signature.size()));
        const QString fieldName = QString("andrepdf_signature_%1").arg(QDateTime::currentMSecsSinceEpoch());
        parameters.createSignatureFieldFunction = pdf::PDFSignatureStamp::createVisibleSignatureField(document.data(), fieldName, lastPlacement.pageIndex, imageRectangle, signature,
                                                                                                     pdf::PDFSignatureStamp::create(certificate->info, parameters.signingTime, parameters.certification, QByteArray()));

        QByteArray signedDocument;
        timestampErrorMessage.clear();
        pdf::PDFDocumentSigner::Result result = pdf::PDFDocumentSigner::Result::SigningFailed;
        try
        {
            result = pdf::PDFDocumentSigner::sign(parameters, signedDocument);
        }
        catch (const std::exception& exception)
        {
            timestampErrorMessage = QString::fromLocal8Bit(exception.what());
        }
        if (result != pdf::PDFDocumentSigner::Result::OK)
        {
            report << tr("%1: %2").arg(part.name, !timestampErrorMessage.isEmpty() ? timestampErrorMessage : pdf::PDFDocumentSigner::getResultMessage(result));
            ++failures;
            continue;
        }

        // Existing files are never overwritten
        QString fileName = packDialog.getOutputDirectory() + "/" + pdf::PDFContractPack::getFileName(packDialog.getBaseName(), part, partIndex);
        for (int attempt = 2; QFile::exists(fileName); ++attempt)
        {
            fileName = packDialog.getOutputDirectory() + "/" + pdf::PDFContractPack::getFileName(packDialog.getBaseName(), part, partIndex).chopped(4) + QString(" (%1).pdf").arg(attempt);
        }
        QFile file(fileName);
        if (!file.open(QFile::WriteOnly) || file.write(signedDocument) != signedDocument.size() || !file.flush())
        {
            report << tr("%1: the file %2 could not be written.").arg(part.name, QDir::toNativeSeparators(fileName));
            ++failures;
            continue;
        }
        file.close();

        // The check reads the file back from the disk, as a reader gets it
        QFile writtenFile(fileName);
        const QByteArray written = writtenFile.open(QFile::ReadOnly) ? writtenFile.readAll() : QByteArray();
        const pdf::PDFDocumentSigner::SignedFileCheck check = pdf::PDFDocumentSigner::checkSignedFile(written, fieldName, parameters.encryptionUserPassword.value_or(QString()));
        report << tr("%1: %2 lines signed (%3), %4 left for another party, %5 - %6")
                      .arg(part.name).arg(placements.size() + 1).arg(signedLabels.join(", ")).arg(otherLines)
                      .arg(check.isOK() ? pdf::PDFSignatureStamp::getCertificationText(check.certification) : tr("CHECK FAILED"))
                      .arg(QFileInfo(fileName).fileName());
        failures += check.isOK() ? 0 : 1;
    }
    QApplication::restoreOverrideCursor();

    const QString title = tr("Split and Sign Contract Pack");
    const QString message = tr("%1 of %2 contracts signed and locked in %3:\n\n%4").arg(parts.size() - failures).arg(parts.size())
                                .arg(QDir::toNativeSeparators(packDialog.getOutputDirectory()), report.join('\n'));
    if (failures > 0)
    {
        QMessageBox::warning(m_widget, title, message);
    }
    else
    {
        QMessageBox::information(m_widget, title, message);
    }
}

void SignaturePlugin::updateSignatureSlots()
{
    for (int i = 0; i < pdf::PDFSignatureLibrary::SLOT_COUNT; ++i)
    {
        const pdf::PDFSignatureLibrary::Slot& slot = m_signatureLibrary.getSlot(i);
        QAction* action = m_actions[SignatureSlot1 + i];
        action->setText(tr("Place %1").arg(m_signatureLibrary.getDisplayName(i)));
        action->setToolTip(slot.isEmpty() ? tr("Slot %1 of the signature library is empty, fill it in the Signature Library").arg(i + 1)
                                          : tr("Place %1 - drag a rectangle on the page").arg(m_signatureLibrary.getDisplayName(i)));

        auto* tool = static_cast<pdf::PDFCreatePCElementImageTool*>(m_tools[SignatureSlot1Tool + i]);
        if (auto* element = static_cast<pdf::PDFPageContentImageElement*>(tool->getElement()))
        {
            element->setContent(slot.png);
        }
    }
}

QString SignaturePlugin::getSignedFileName() const
{
    QFileInfo fileInfo(m_dataExchangeInterface->getOriginalFileName());

    return fileInfo.path() + "/" + fileInfo.baseName() + "_SIGNED.pdf";
}

void SignaturePlugin::onOpenCertificatesManager()
{
    pdf::PDFCertificateManagerDialog dialog(m_dataExchangeInterface->getMainWindow());
    dialog.exec();
}

void SignaturePlugin::onSceneActivityChanged()
{
    updateActions();
}

void SignaturePlugin::onPenChanged(const QPen& pen)
{
    if (pdf::PDFCreatePCElementTool* activeTool = qobject_cast<pdf::PDFCreatePCElementTool*>(getActiveTool()))
    {
        activeTool->setPen(pen);
    }
}

void SignaturePlugin::onBrushChanged(const QBrush& brush)
{
    if (pdf::PDFCreatePCElementTool* activeTool = qobject_cast<pdf::PDFCreatePCElementTool*>(getActiveTool()))
    {
        activeTool->setBrush(brush);
    }
}

void SignaturePlugin::onFontChanged(const QFont& font)
{
    if (pdf::PDFCreatePCElementTool* activeTool = qobject_cast<pdf::PDFCreatePCElementTool*>(getActiveTool()))
    {
        activeTool->setFont(font);
    }
}

void SignaturePlugin::onAlignmentChanged(Qt::Alignment alignment)
{
    if (pdf::PDFCreatePCElementTool* activeTool = qobject_cast<pdf::PDFCreatePCElementTool*>(getActiveTool()))
    {
        activeTool->setAlignment(alignment);
    }
}

void SignaturePlugin::onTextAngleChanged(pdf::PDFReal angle)
{
    if (pdf::PDFCreatePCElementTool* activeTool = qobject_cast<pdf::PDFCreatePCElementTool*>(getActiveTool()))
    {
        activeTool->setTextAngle(angle);
    }
}

void SignaturePlugin::setActive(bool active)
{
    if (m_scene.isActive() != active)
    {
        // Abort active tool, if we are deactivating the plugin
        if (!active)
        {
            if (pdf::PDFWidgetTool* tool = m_widget->getToolManager()->getActiveTool())
            {
                auto it = std::find(m_tools.cbegin(), m_tools.cend(), tool);
                if (it != m_tools.cend())
                {
                    m_widget->getToolManager()->setActiveTool(nullptr);
                }
            }
        }

        m_scene.setActive(active);
        if (!active)
        {
            m_scene.clear();
        }
        else
        {
            updateDockWidget();
        }

        m_actions[Activate]->setChecked(active);
        updateActions();

        // If editor is not active, remove the widget
        if (m_editorWidget && !active)
        {
            delete m_editorWidget;
            m_editorWidget = nullptr;
        }
    }
}

void SignaturePlugin::updateActions()
{
    m_actions[Activate]->setEnabled(m_document);

    if (!m_scene.isActive() || !m_document)
    {
        // Inactive scene - disable all except activate action and certificates
        for (QAction* action : m_actions)
        {
            if (action == m_actions[Activate])
            {
                action->setEnabled(m_widget && !m_widget->isAnySceneActive(&m_scene));
            }

            if (action == m_actions[Activate] ||
                action == m_actions[Certificates] ||
                action == m_actions[SignatureLibrary])
            {
                continue;
            }

            action->setEnabled(false);
        }

        m_actions[SignDigitally]->setEnabled(m_document && m_widget && !m_widget->isAnySceneActive(&m_scene));
        m_actions[InitialPages]->setEnabled(m_document && m_widget && !m_widget->isAnySceneActive(&m_scene));
        m_actions[SignContract]->setEnabled(m_document && m_widget && !m_widget->isAnySceneActive(&m_scene));
        m_actions[SignPack]->setEnabled(m_document && m_widget && !m_widget->isAnySceneActive(&m_scene));
        return;
    }

    const bool isSceneNonempty = !m_scene.isEmpty();

    // Tool actions
    for (auto actionId : { Text, FreehandCurve, AcceptMark, RejectMark,
                           Rectangle, RoundedRectangle, HorizontalLine,
                           VerticalLine, Line, Dot, SvgImage })
    {
        m_actions[actionId]->setEnabled(true);
    }
    for (int i = 0; i < pdf::PDFSignatureLibrary::SLOT_COUNT; ++i)
    {
        m_actions[SignatureSlot1 + i]->setEnabled(!m_signatureLibrary.getSlot(i).isEmpty());
    }
    m_actions[SignatureLibrary]->setEnabled(true);
    m_actions[InitialPages]->setEnabled(m_document);
    m_actions[SignContract]->setEnabled(m_document);
    m_actions[SignPack]->setEnabled(m_document);

    // Clear action
    QAction* clearAction = m_actions[Clear];
    clearAction->setEnabled(isSceneNonempty);

    // Sign actions
    QAction* signElectronicallyAction = m_actions[SignElectronically];
    signElectronicallyAction->setEnabled(isSceneNonempty);
    QAction* signDigitallyAction = m_actions[SignDigitally];
    signDigitallyAction->setEnabled(m_document);
}

void SignaturePlugin::updateGraphics()
{
    if (m_widget)
    {
        m_widget->getDrawWidget()->getWidget()->update();
    }
}

void SignaturePlugin::updateDockWidget()
{
    if (m_editorWidget)
    {
        return;
    }

    m_editorWidget = new pdf::PDFPageContentEditorWidget(m_dataExchangeInterface->getMainWindow());
    m_editorWidget->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_dataExchangeInterface->getMainWindow()->addDockWidget(Qt::RightDockWidgetArea, m_editorWidget, Qt::Vertical);
    m_editorWidget->setFloating(false);
    m_editorWidget->setWindowTitle(tr("Signature Toolbox"));
    m_editorWidget->setScene(&m_scene);
    connect(m_editorWidget, &pdf::PDFPageContentEditorWidget::operationTriggered, &m_scene, &pdf::PDFPageContentScene::performOperation);
    connect(m_editorWidget, &pdf::PDFPageContentEditorWidget::itemSelectionChangedByUser, this, &SignaturePlugin::onWidgetSelectionChanged);
    connect(m_editorWidget, &pdf::PDFPageContentEditorWidget::editElementRequest, this, &SignaturePlugin::onSceneEditSingleElement);

    m_editorWidget->getToolButtonForOperation(static_cast<int>(pdf::PDFPageContentElementManipulator::Operation::AlignTop))->setIcon(QIcon(":/resources/pce-align-top.svg"));
    m_editorWidget->getToolButtonForOperation(static_cast<int>(pdf::PDFPageContentElementManipulator::Operation::AlignCenterVertically))->setIcon(QIcon(":/resources/pce-align-v-center.svg"));
    m_editorWidget->getToolButtonForOperation(static_cast<int>(pdf::PDFPageContentElementManipulator::Operation::AlignBottom))->setIcon(QIcon(":/resources/pce-align-bottom.svg"));
    m_editorWidget->getToolButtonForOperation(static_cast<int>(pdf::PDFPageContentElementManipulator::Operation::AlignLeft))->setIcon(QIcon(":/resources/pce-align-left.svg"));
    m_editorWidget->getToolButtonForOperation(static_cast<int>(pdf::PDFPageContentElementManipulator::Operation::AlignCenterHorizontally))->setIcon(QIcon(":/resources/pce-align-h-center.svg"));
    m_editorWidget->getToolButtonForOperation(static_cast<int>(pdf::PDFPageContentElementManipulator::Operation::AlignRight))->setIcon(QIcon(":/resources/pce-align-right.svg"));
    m_editorWidget->getToolButtonForOperation(static_cast<int>(pdf::PDFPageContentElementManipulator::Operation::SetSameHeight))->setIcon(QIcon(":/resources/pce-same-height.svg"));
    m_editorWidget->getToolButtonForOperation(static_cast<int>(pdf::PDFPageContentElementManipulator::Operation::SetSameWidth))->setIcon(QIcon(":/resources/pce-same-width.svg"));
    m_editorWidget->getToolButtonForOperation(static_cast<int>(pdf::PDFPageContentElementManipulator::Operation::SetSameSize))->setIcon(QIcon(":/resources/pce-same-size.svg"));
    m_editorWidget->getToolButtonForOperation(static_cast<int>(pdf::PDFPageContentElementManipulator::Operation::CenterHorizontally))->setIcon(QIcon(":/resources/pce-center-h.svg"));
    m_editorWidget->getToolButtonForOperation(static_cast<int>(pdf::PDFPageContentElementManipulator::Operation::CenterVertically))->setIcon(QIcon(":/resources/pce-center-v.svg"));
    m_editorWidget->getToolButtonForOperation(static_cast<int>(pdf::PDFPageContentElementManipulator::Operation::CenterHorAndVert))->setIcon(QIcon(":/resources/pce-center-vh.svg"));
    m_editorWidget->getToolButtonForOperation(static_cast<int>(pdf::PDFPageContentElementManipulator::Operation::LayoutVertically))->setIcon(QIcon(":/resources/pce-layout-v.svg"));
    m_editorWidget->getToolButtonForOperation(static_cast<int>(pdf::PDFPageContentElementManipulator::Operation::LayoutHorizontally))->setIcon(QIcon(":/resources/pce-layout-h.svg"));
    m_editorWidget->getToolButtonForOperation(static_cast<int>(pdf::PDFPageContentElementManipulator::Operation::LayoutForm))->setIcon(QIcon(":/resources/pce-layout-form.svg"));
    m_editorWidget->getToolButtonForOperation(static_cast<int>(pdf::PDFPageContentElementManipulator::Operation::LayoutGrid))->setIcon(QIcon(":/resources/pce-layout-grid.svg"));

    for (QAction* action : m_actions)
    {
        m_editorWidget->addAction(action);
    }

    connect(m_editorWidget, &pdf::PDFPageContentEditorWidget::penChanged, this, &SignaturePlugin::onPenChanged);
    connect(m_editorWidget, &pdf::PDFPageContentEditorWidget::brushChanged, this, &SignaturePlugin::onBrushChanged);
    connect(m_editorWidget, &pdf::PDFPageContentEditorWidget::fontChanged, this, &SignaturePlugin::onFontChanged);
    connect(m_editorWidget, &pdf::PDFPageContentEditorWidget::alignmentChanged, this, &SignaturePlugin::onAlignmentChanged);
    connect(m_editorWidget, &pdf::PDFPageContentEditorWidget::textAngleChanged, this, &SignaturePlugin::onTextAngleChanged);
}

void SignaturePlugin::onSceneEditSingleElement(pdf::PDFInteger elementId)
{
    onSceneEditElement({ elementId });
}

}
