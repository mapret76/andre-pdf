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

#ifndef PDFMOBILECONTROLLER_H
#define PDFMOBILECONTROLLER_H

#include "pdfdocument.h"
#include "pdfsignaturelibrary.h"
#include "pdfsignaturelinefinder.h"

#include <QUrl>
#include <QImage>
#include <QObject>
#include <QStringList>

#include <mutex>

namespace pdfmobile
{

/// Backend of the mobile app: opens a document, renders its pages, keeps the
/// signature library, and signs (with a certificate file, .pfx) on the signature
/// lines found in the document or at a place the user taps. The work is done by the
/// same core library as on the desktop.
class PDFMobileController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int pageCount READ getPageCount NOTIFY documentChanged)
    Q_PROPERTY(QString fileName READ getFileName NOTIFY documentChanged)
    Q_PROPERTY(int revision READ getRevision NOTIFY documentChanged)
    Q_PROPERTY(QString status READ getStatus NOTIFY statusChanged)
    Q_PROPERTY(QStringList signatureSlots READ getSignatureSlots NOTIFY libraryChanged)
    Q_PROPERTY(int signatureLineCount READ getSignatureLineCount NOTIFY documentChanged)
    Q_PROPERTY(QUrl signedFileUrl READ getSignedFileUrl NOTIFY statusChanged)

public:
    explicit PDFMobileController(QObject* parent = nullptr);

    int getPageCount() const;
    QString getFileName() const { return m_fileName; }
    int getRevision() const { return m_revision; }
    QString getStatus() const { return m_status; }
    QStringList getSignatureSlots() const;
    int getSignatureLineCount() const { return int(m_lines.size()); }
    QUrl getSignedFileUrl() const { return m_signedFileUrl; }

    /// Opens a document (a local file or a content URI of Android)
    Q_INVOKABLE bool open(const QUrl& url);

    /// Returns the height of the displayed page divided by its width
    Q_INVOKABLE double getPageAspectRatio(int pageIndex) const;

    /// Imports a signature from a photo or scan into a slot of the library
    Q_INVOKABLE bool importSignature(int slot, const QUrl& imageUrl, const QString& name);

    /// Signs and certifies the document with the certificate file. With pageIndex < 0
    /// the signature image goes on every signature line found (the last one is the
    /// visible digital signature), otherwise the signature is placed at the point
    /// (x, y from 0 to 1, from the top left corner of the displayed page).
    /// \param certificateUrl Certificate file (.pfx, .p12)
    /// \param password Password of the certificate
    /// \param certification 1 no changes, 2 form filling and signing, 3 also comments, 0 not locked
    /// \param signatureSlot Slot of the signature library, -1 for the stamp only
    Q_INVOKABLE bool sign(const QUrl& certificateUrl, const QString& password, int certification, int signatureSlot, int pageIndex, double x, double y);

    /// Renders a page to the size (thread safe, used by the image provider)
    QImage renderPage(int pageIndex, const QSize& size) const;

    /// File the signed document is written to: next to the original, or in the
    /// documents of the app when the original is not writable
    QString getSignedFileName() const;

Q_SIGNALS:
    void documentChanged();
    void statusChanged();
    void libraryChanged();

private:
    void setStatus(const QString& status);

    mutable std::mutex m_mutex;     ///< Guards the document against the rendering threads
    pdf::PDFDocument m_document;
    QByteArray m_documentData;
    QString m_fileName;
    QString m_localFileName;
    int m_revision = 0;
    QString m_status;
    QUrl m_signedFileUrl;
    pdf::PDFSignatureLibrary m_library;
    std::vector<pdf::PDFSignatureLineFinder::SignatureLine> m_lines;
};

}   // namespace pdfmobile

#endif // PDFMOBILECONTROLLER_H
