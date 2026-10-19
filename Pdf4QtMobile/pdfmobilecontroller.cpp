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
#include "pdfdocumentreader.h"
#include "pdfdocumentsigner.h"
#include "pdfsignaturestamp.h"
#include "pdfpageinitials.h"
#include "pdfcertificatemanager.h"
#include "pdfrenderer.h"
#include "pdfcms.h"
#include "pdffont.h"
#include "pdfcolorspaces.h"
#include "pdfconstants.h"
#include "pdfoptionalcontent.h"

#include <QDir>
#include <QFile>
#include <QPainter>
#include <QFileInfo>
#include <QImageReader>
#include <QStandardPaths>

namespace pdfmobile
{

// Size of a signature placed by a tap, in points
static constexpr qreal TAP_SIGNATURE_WIDTH = 150.0;
static constexpr qreal TAP_SIGNATURE_HEIGHT = 50.0;

PDFMobileController::PDFMobileController(QObject* parent) :
    QObject(parent)
{
    m_library.load(pdf::PDFSignatureLibrary::getDefaultFileName());
}

int PDFMobileController::getPageCount() const
{
    std::lock_guard lock(m_mutex);
    return m_document.getCatalog() ? int(m_document.getCatalog()->getPageCount()) : 0;
}

QStringList PDFMobileController::getSignatureSlots() const
{
    QStringList names;
    for (int i = 0; i < pdf::PDFSignatureLibrary::SLOT_COUNT; ++i)
    {
        names << (m_library.getSlot(i).isEmpty() ? tr("%1 (empty)").arg(m_library.getDisplayName(i)) : m_library.getDisplayName(i));
    }
    return names;
}

void PDFMobileController::setStatus(const QString& status)
{
    m_status = status;
    Q_EMIT statusChanged();
}

bool PDFMobileController::open(const QUrl& url, const QString& password)
{
    m_needsPassword = false;
    // A content URI of Android is read by QFile as well
    const QString fileName = url.isLocalFile() ? url.toLocalFile() : url.toString();
    QFile file(fileName);
    if (!file.open(QFile::ReadOnly))
    {
        setStatus(tr("The file cannot be opened: %1").arg(file.errorString()));
        return false;
    }
    const QByteArray data = file.readAll();
    file.close();

    // The password is offered once; without it an encrypted document does not open
    bool isPasswordOffered = false;
    bool isPasswordAsked = false;
    auto getPassword = [&](bool* ok)
    {
        isPasswordAsked = true;
        *ok = !isPasswordOffered && !password.isEmpty();
        isPasswordOffered = true;
        return password;
    };
    pdf::PDFDocumentReader reader(nullptr, getPassword, true, false);
    pdf::PDFDocument document = reader.readFromBuffer(data);
    if (reader.getReadingResult() != pdf::PDFDocumentReader::Result::OK)
    {
        if (isPasswordAsked)
        {
            m_needsPassword = true;
            setStatus(password.isEmpty() ? tr("The document is protected by a password.") : tr("The password is wrong."));
            return false;
        }
        setStatus(tr("The document cannot be read: %1").arg(reader.getErrorMessage()));
        return false;
    }

    {
        std::lock_guard lock(m_mutex);
        m_document = std::move(document);
        m_documentData = data;
        m_localFileName = url.isLocalFile() ? fileName : QString();
        m_fileName = QFileInfo(url.isLocalFile() ? fileName : url.fileName()).fileName();
        if (m_fileName.isEmpty())
        {
            m_fileName = tr("document.pdf");
        }
        m_lines = pdf::PDFSignatureLineFinder::find(&m_document);
        m_documentPassword = password;
        ++m_revision;
    }
    m_signedFileUrl = QUrl();
    Q_EMIT documentChanged();
    setStatus(m_lines.empty() ? tr("No signature lines found - tap where to sign.") : tr("%1 signature lines found.").arg(m_lines.size()));
    return true;
}

double PDFMobileController::getPageAspectRatio(int pageIndex) const
{
    std::lock_guard lock(m_mutex);
    const pdf::PDFPage* page = m_document.getCatalog() ? m_document.getCatalog()->getPage(pageIndex) : nullptr;
    if (!page)
    {
        return 1.414;
    }
    const QSizeF size = page->getRotatedMediaBox().size();
    return size.width() > 0 ? size.height() / size.width() : 1.414;
}

QImage PDFMobileController::renderPage(int pageIndex, const QSize& size) const
{
    std::lock_guard lock(m_mutex);
    QImage image(size.isValid() ? size : QSize(600, 848), QImage::Format_RGB32);
    image.fill(Qt::white);
    if (!m_document.getCatalog() || !m_document.getCatalog()->getPage(pageIndex))
    {
        return image;
    }

    pdf::PDFCMSGeneric cms;
    pdf::PDFFontCache fontCache(pdf::DEFAULT_FONT_CACHE_LIMIT, pdf::DEFAULT_REALIZED_FONT_CACHE_LIMIT);
    pdf::PDFICCProfileCache iccProfileCache;
    pdf::PDFOptionalContentActivity activity(&m_document, pdf::OCUsage::View, nullptr);
    fontCache.setDocument(pdf::PDFModifiedDocument(const_cast<pdf::PDFDocument*>(&m_document), &activity));
    iccProfileCache.setDocument(pdf::PDFModifiedDocument(const_cast<pdf::PDFDocument*>(&m_document), &activity));
    pdf::PDFRenderer renderer(&m_document, &fontCache, &iccProfileCache, &cms, &activity, pdf::PDFRenderer::getDefaultFeatures(), pdf::PDFMeshQualitySettings());
    QPainter painter(&image);
    renderer.render(&painter, QRectF(QPointF(0, 0), image.size()), size_t(pageIndex));
    painter.end();
    return image;
}

bool PDFMobileController::importSignature(int slot, const QUrl& imageUrl, const QString& name)
{
    QImageReader reader(imageUrl.isLocalFile() ? imageUrl.toLocalFile() : imageUrl.toString());
    reader.setAutoTransform(true);
    const QImage signature = pdf::PDFSignatureLibrary::extractSignature(reader.read());
    if (signature.isNull() || slot < 0 || slot >= pdf::PDFSignatureLibrary::SLOT_COUNT)
    {
        setStatus(tr("No signature was found in the image."));
        return false;
    }

    m_library.setSlot(slot, { name.trimmed(), pdf::PDFSignatureLibrary::toPng(signature) });
    QString errorMessage;
    if (!m_library.save(pdf::PDFSignatureLibrary::getDefaultFileName(), &errorMessage))
    {
        setStatus(errorMessage);
        return false;
    }
    Q_EMIT libraryChanged();
    setStatus(tr("Signature saved in slot %1.").arg(slot + 1));
    return true;
}

QString PDFMobileController::getSignedFileName() const
{
    QString directory = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (!m_localFileName.isEmpty() && QFileInfo(QFileInfo(m_localFileName).absolutePath()).isWritable())
    {
        directory = QFileInfo(m_localFileName).absolutePath();
    }
    QDir().mkpath(directory);

    // An existing file is never overwritten
    const QString baseName = QFileInfo(m_fileName).completeBaseName();
    QString fileName = directory + "/" + baseName + QStringLiteral("_SIGNED.pdf");
    for (int i = 2; QFile::exists(fileName); ++i)
    {
        fileName = directory + "/" + baseName + QString("_SIGNED (%1).pdf").arg(i);
    }
    return fileName;
}

bool PDFMobileController::sign(const QUrl& certificateUrl, const QString& password, int certification, int signatureSlot, int pageIndex, double x, double y)
{
    QFile certificateFile(certificateUrl.isLocalFile() ? certificateUrl.toLocalFile() : certificateUrl.toString());
    if (!certificateFile.open(QFile::ReadOnly))
    {
        setStatus(tr("The certificate file cannot be opened."));
        return false;
    }
    const std::optional<pdf::PDFCertificateEntry> certificate = pdf::PDFCertificateManager::createEntryFromPkcs12(certificateFile.readAll(), password);
    certificateFile.close();
    if (!certificate)
    {
        setStatus(tr("The certificate cannot be opened - check the password."));
        return false;
    }

    std::lock_guard lock(m_mutex);
    if (!m_document.getCatalog() || m_document.getCatalog()->getPageCount() == 0)
    {
        return false;
    }
    if (pdf::PDFDocumentSigner::hasExistingSignatures(&m_document) && certification != 0)
    {
        // Only the first signature can certify; countersigning keeps the others valid
        certification = 0;
    }

    QImage signature;
    if (signatureSlot >= 0 && signatureSlot < pdf::PDFSignatureLibrary::SLOT_COUNT)
    {
        signature.loadFromData(m_library.getSlot(signatureSlot).png, "PNG");
    }

    // Placements: every line found, or the tapped point
    std::vector<pdf::PDFPageInitials::Placement> placements;
    if (pageIndex < 0)
    {
        for (const pdf::PDFSignatureLineFinder::SignatureLine& line : m_lines)
        {
            placements.push_back({ line.pageIndex, line.rectangle });
        }
    }
    else if (const pdf::PDFPage* page = m_document.getCatalog()->getPage(pageIndex))
    {
        const QSizeF displaySize = page->getRotatedMediaBox().size();
        const QRectF displayRectangle(x * displaySize.width() - TAP_SIGNATURE_WIDTH * 0.5, (1.0 - y) * displaySize.height() - TAP_SIGNATURE_HEIGHT * 0.5, TAP_SIGNATURE_WIDTH, TAP_SIGNATURE_HEIGHT);
        placements.push_back({ pageIndex, pdf::PDFPageInitials::getDisplayToPageTransform(page).mapRect(displayRectangle) });
    }
    if (placements.empty())
    {
        setStatus(tr("No signature lines were found - tap where to sign."));
        return false;
    }

    // The image on all lines but the last; the last is the digital signature
    pdf::PDFDocument document = m_document;
    const pdf::PDFPageInitials::Placement last = placements.back();
    placements.pop_back();
    const bool isRewritten = !placements.empty() && !signature.isNull();
    if (isRewritten)
    {
        if (pdf::PDFDocumentPointer drawn = pdf::PDFPageInitials::drawImages(&document, signature, placements))
        {
            document = *drawn;
        }
    }

    const pdf::PDFPage* lastPage = document.getCatalog()->getPage(last.pageIndex);
    const QRectF imageRectangle = signature.isNull() ? last.area : pdf::PDFPageInitials::getDisplayToPageTransform(lastPage).mapRect(pdf::PDFPageInitials::getFittedDisplayRectangle(lastPage, last.area, signature.size()));

    pdf::PDFDocumentSigner::Parameters parameters;
    parameters.document = &document;
    parameters.originalDocumentData = m_documentData;
    parameters.certification = static_cast<pdf::PDFDocumentSigner::Certification>(qBound(0, certification, 3));
    parameters.openPassword = m_documentPassword;
    const pdf::PDFCertificateEntry entry = *certificate;
    parameters.signFunction = [entry, password](const QByteArray& data, QByteArray& result) { return pdf::PDFSignatureFactory::sign(entry, password, data, result); };
    const QString fieldName = QString("andrepdf_signature_%1").arg(QDateTime::currentMSecsSinceEpoch());
    pdf::PDFSignatureStamp::Data stamp = pdf::PDFSignatureStamp::create(entry.info, parameters.signingTime, parameters.certification, m_documentData);
    parameters.requireIncrementalUpdate = !stamp.documentSha256.isEmpty();

    QByteArray signedDocument;
    pdf::PDFDocumentSigner::Result result = pdf::PDFDocumentSigner::Result::SigningFailed;
    QString exceptionMessage;
    try
    {
        parameters.createSignatureFieldFunction = pdf::PDFSignatureStamp::createVisibleSignatureField(&document, fieldName, last.pageIndex, imageRectangle, signature, stamp);
        result = pdf::PDFDocumentSigner::sign(parameters, signedDocument);
        if (result == pdf::PDFDocumentSigner::Result::IncrementalUpdateFailed)
        {
            // The file is written anew, its old fingerprint could not be checked
            stamp.documentSha256.clear();
            parameters.requireIncrementalUpdate = false;
            parameters.createSignatureFieldFunction = pdf::PDFSignatureStamp::createVisibleSignatureField(&document, fieldName, last.pageIndex, imageRectangle, signature, stamp);
            result = pdf::PDFDocumentSigner::sign(parameters, signedDocument);
        }
    }
    catch (const std::exception& exception)
    {
        exceptionMessage = QString::fromLocal8Bit(exception.what());
    }
    if (result != pdf::PDFDocumentSigner::Result::OK)
    {
        setStatus(!exceptionMessage.isEmpty() ? tr("The document could not be signed: %1").arg(exceptionMessage) : pdf::PDFDocumentSigner::getResultMessage(result));
        return false;
    }

    const QString signedFileName = getSignedFileName();
    QFile output(signedFileName);
    if (!output.open(QFile::WriteOnly | QFile::Truncate) || output.write(signedDocument) != signedDocument.size())
    {
        setStatus(tr("The signed file cannot be written."));
        return false;
    }
    output.close();

    const pdf::PDFDocumentSigner::SignedFileCheck check = pdf::PDFDocumentSigner::checkSignedFile(signedDocument, fieldName, m_documentPassword);
    m_signedFileUrl = QUrl::fromLocalFile(signedFileName);
    setStatus(check.isOK() ? tr("Signed and checked: %1. Saved as %2.").arg(pdf::PDFSignatureStamp::getCertificationText(check.certification).isEmpty() ? tr("not locked") : pdf::PDFSignatureStamp::getCertificationText(check.certification), QFileInfo(signedFileName).fileName())
                           : tr("The signed file did not pass the check."));
    return check.isOK();
}

}   // namespace pdfmobile
