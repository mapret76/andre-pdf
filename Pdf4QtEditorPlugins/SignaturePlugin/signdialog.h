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

#ifndef SIGNDIALOG_H
#define SIGNDIALOG_H

#include "pdfcertificatemanager.h"
#include "pdfdocumentsigner.h"

#include <QDialog>

namespace Ui
{
class SignDialog;
}

namespace pdfplugin
{

class SignDialog : public QDialog
{
    Q_OBJECT

public:
    /// \param parent Parent widget
    /// \param isSceneEmpty No signature graphics were created
    /// \param canCertify Document has no signature yet, so the signature can certify it
    explicit SignDialog(QWidget* parent, bool isSceneEmpty, bool canCertify = true);
    virtual ~SignDialog() override;

    virtual void accept() override;

    enum SignMethod
    {
        SignDigitally,
        SignDigitallyInvisible
    };

    enum SignatureType
    {
        /// Digital signature of the document
        SignatureOnly,

        /// Digital signature of the document, the time of the signing is
        /// attested by a timestamp authority
        SignatureWithTimestamp,

        /// Document timestamp, which attests, that the document existed at
        /// the time of the timestamp, without signing it
        TimestampOnly
    };

    SignMethod getSignMethod() const;
    SignatureType getSignatureType() const;
    QString getPassword() const;
    QString getReasonText() const;
    QString getContactInfoText() const;
    QString getTimestampUrl() const;
    pdf::PDFDocumentSigner::Certification getCertification() const;

    /// Preselects the certification (the contract workflow knows, whether another
    /// party still signs); ignored, when the document cannot be certified
    void setCertification(pdf::PDFDocumentSigner::Certification certification);

    /// Returns the password needed to open the signed document, when it is to be encrypted
    std::optional<QString> getEncryptionPassword() const;

    /// Returns true, if the signature is to be extended to PAdES B-LTA (only with a timestamp)
    bool isLongTermValidation() const;
    const pdf::PDFCertificateEntry* getCertificate() const;

private:
    /// Enables only the settings, which are used by the selected signature type
    void updateUi();

    void loadSettings();
    void saveSettings();

    Ui::SignDialog* ui;
    pdf::PDFCertificateEntries m_certificates;
    bool m_canCertify;
};

}   // namespace pdfplugin

#endif // SIGNDIALOG_H
