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

#include "signdialog.h"
#include "ui_signdialog.h"

#include "pdfcertificatemanager.h"
#include "pdfcertificatelisthelper.h"

#include <QCheckBox>
#include <QComboBox>
#include <QMessageBox>
#include <QSettings>
#include <QUrl>

namespace pdfplugin
{

namespace signdialog
{

constexpr const char* SETTINGS_GROUP = "SignaturePlugin";
constexpr const char* SETTINGS_SIGNATURE_TYPE = "SignatureType";
constexpr const char* SETTINGS_LONG_TERM = "LongTermValidation";
constexpr const char* SETTINGS_TIMESTAMP_URL = "TimestampUrl";
constexpr const char* SETTINGS_CERTIFICATION = "Certification";

/// Timestamp authorities offered to the user. The combo box is editable, so
/// any other authority can be used, too.
constexpr const char* DEFAULT_TIMESTAMP_URLS[] =
{
    "http://timestamp.digicert.com",
    "http://timestamp.sectigo.com",
    "https://freetsa.org/tsr"
};

}   // namespace signdialog

using namespace signdialog;

SignDialog::SignDialog(QWidget* parent, bool isSceneEmpty, bool canCertify) :
    QDialog(parent),
    ui(new Ui::SignDialog),
    m_canCertify(canCertify)
{
    ui->setupUi(this);

    ui->methodCombo->addItem(tr("Sign digitally (visible signature)"), SignDigitally);
    ui->methodCombo->addItem(tr("Sign digitally (invisible signature)"), SignDigitallyInvisible);
    ui->methodCombo->setCurrentIndex(isSceneEmpty ? 1 : 0);

    ui->signatureTypeCombo->addItem(tr("Signature"), SignatureOnly);
    ui->signatureTypeCombo->addItem(tr("Signature with timestamp"), SignatureWithTimestamp);
    ui->signatureTypeCombo->addItem(tr("Document timestamp only"), TimestampOnly);
    ui->signatureTypeCombo->setCurrentIndex(1);

    for (const char* timestampUrl : DEFAULT_TIMESTAMP_URLS)
    {
        ui->timestampUrlCombo->addItem(QString::fromLatin1(timestampUrl));
    }
    ui->timestampUrlCombo->setCurrentIndex(0);

    // The certification locks the document, it must be its first signature.
    // "No changes" is the default for a contract signed by one party.
    ui->certificationCombo->addItem(tr("No changes allowed"), int(pdf::PDFDocumentSigner::Certification::NoChanges));
    ui->certificationCombo->addItem(tr("Allow form filling and signing"), int(pdf::PDFDocumentSigner::Certification::FormFilling));
    ui->certificationCombo->addItem(tr("Allow form filling, signing and comments"), int(pdf::PDFDocumentSigner::Certification::FormFillingAndAnnotations));
    ui->certificationCombo->addItem(tr("Do not lock (approval signature)"), int(pdf::PDFDocumentSigner::Certification::None));
    ui->certificationCombo->setCurrentIndex(m_canCertify ? 0 : ui->certificationCombo->count() - 1);
    ui->certificationCombo->setToolTip(m_canCertify ? tr("Certifies the document by this signature. A reader is warned about any later change not allowed here.")
                                                    : tr("The document is already signed, only its first signature can lock it."));

    m_certificates = pdf::PDFCertificateManager::getCertificates(pdf::PDFCertificateUsageFilter::DigitalSignature);

    pdf::PDFCertificateListHelper::initComboBox(ui->certificateCombo);
    pdf::PDFCertificateListHelper::fillComboBox(ui->certificateCombo, m_certificates);

    loadSettings();

    connect(ui->signatureTypeCombo, &QComboBox::currentIndexChanged, this, &SignDialog::updateUi);
    connect(ui->encryptCheckBox, &QCheckBox::toggled, this, &SignDialog::updateUi);
    ui->longTermCheckBox->setToolTip(tr("PAdES B-LTA: the certificates and their revocation status (OCSP, CRL) are embedded and protected by a document timestamp, so a third party can still verify the signature after the certificate has expired."));
    ui->encryptCheckBox->setToolTip(m_canCertify ? tr("The document is encrypted with AES-256 before it is signed, so encryption never breaks the signature.")
                                                 : tr("The document is already signed, encrypting it would break the signatures."));
    updateUi();
}

SignDialog::~SignDialog()
{
    delete ui;
}

SignDialog::SignMethod SignDialog::getSignMethod() const
{
    return static_cast<SignMethod>(ui->methodCombo->currentData().toInt());
}

SignDialog::SignatureType SignDialog::getSignatureType() const
{
    return static_cast<SignatureType>(ui->signatureTypeCombo->currentData().toInt());
}

QString SignDialog::getPassword() const
{
    return ui->certificatePasswordEdit->text();
}

QString SignDialog::getReasonText() const
{
    return ui->reasonEdit->text();
}

QString SignDialog::getContactInfoText() const
{
    return ui->contactInfoEdit->text();
}

bool SignDialog::isLongTermValidation() const
{
    return getSignatureType() == SignatureWithTimestamp && ui->longTermCheckBox->isChecked();
}

QString SignDialog::getTimestampUrl() const
{
    return ui->timestampUrlCombo->currentText().trimmed();
}

pdf::PDFDocumentSigner::Certification SignDialog::getCertification() const
{
    if (!m_canCertify || getSignatureType() == TimestampOnly)
    {
        return pdf::PDFDocumentSigner::Certification::None;
    }

    return static_cast<pdf::PDFDocumentSigner::Certification>(ui->certificationCombo->currentData().toInt());
}

void SignDialog::setCertification(pdf::PDFDocumentSigner::Certification certification)
{
    const int index = ui->certificationCombo->findData(int(certification));
    if (m_canCertify && index != -1)
    {
        ui->certificationCombo->setCurrentIndex(index);
    }
}

std::optional<QString> SignDialog::getEncryptionPassword() const
{
    if (!m_canCertify || getSignatureType() == TimestampOnly || !ui->encryptCheckBox->isChecked())
    {
        return std::nullopt;
    }

    return ui->openPasswordEdit->text();
}

const pdf::PDFCertificateEntry* SignDialog::getCertificate() const
{
    const int index = ui->certificateCombo->currentIndex();
    if (index >= 0 && index < m_certificates.size())
    {
        return &m_certificates.at(index);
    }

    return nullptr;
}

void SignDialog::accept()
{
    const SignatureType signatureType = getSignatureType();

    // A document timestamp is not signed by a certificate of the user, it is
    // signed by the timestamp authority, so no certificate is needed for it.
    if (signatureType != TimestampOnly)
    {
        const pdf::PDFCertificateEntry* certificate = getCertificate();

        // Check certificate
        if (!certificate)
        {
            QMessageBox::critical(this, tr("Error"), tr("Certificate does not exist."));
            ui->certificateCombo->setFocus();
            return;
        }

        // Check we can access the certificate
        if (!pdf::PDFCertificateManager::isCertificateValid(*certificate, ui->certificatePasswordEdit->text()))
        {
            QMessageBox::critical(this, tr("Error"), tr("Password to open certificate is invalid."));
            ui->certificatePasswordEdit->setFocus();
            return;
        }
    }

    if (getEncryptionPassword())
    {
        if (ui->openPasswordEdit->text().isEmpty())
        {
            QMessageBox::critical(this, tr("Error"), tr("Enter the password needed to open the encrypted document."));
            ui->openPasswordEdit->setFocus();
            return;
        }

        if (ui->openPasswordEdit->text() != ui->openPasswordConfirmEdit->text())
        {
            QMessageBox::critical(this, tr("Error"), tr("The passwords to open the document do not match."));
            ui->openPasswordConfirmEdit->setFocus();
            return;
        }
    }

    if (signatureType != SignatureOnly)
    {
        const QUrl timestampUrl(getTimestampUrl());
        if (!timestampUrl.isValid() || timestampUrl.scheme().isEmpty() || timestampUrl.host().isEmpty())
        {
            QMessageBox::critical(this, tr("Error"), tr("Address of the timestamp authority is not valid."));
            ui->timestampUrlCombo->setFocus();
            return;
        }
    }

    saveSettings();
    QDialog::accept();
}

void SignDialog::updateUi()
{
    const SignatureType signatureType = getSignatureType();
    const bool isCertificateUsed = signatureType != TimestampOnly;
    const bool isTimestampUsed = signatureType != SignatureOnly;

    // A document timestamp has no visible appearance and no signer, so the
    // settings of the signature are not used by it.
    ui->methodLabel->setEnabled(isCertificateUsed);
    ui->methodCombo->setEnabled(isCertificateUsed);
    ui->certificateLabel->setEnabled(isCertificateUsed);
    ui->certificateCombo->setEnabled(isCertificateUsed);
    ui->passwordLabel->setEnabled(isCertificateUsed);
    ui->certificatePasswordEdit->setEnabled(isCertificateUsed);
    ui->reasonLabel->setEnabled(isCertificateUsed);
    ui->reasonEdit->setEnabled(isCertificateUsed);
    ui->contactInfoLabel->setEnabled(isCertificateUsed);
    ui->contactInfoEdit->setEnabled(isCertificateUsed);
    ui->certificationLabel->setEnabled(isCertificateUsed && m_canCertify);
    ui->certificationCombo->setEnabled(isCertificateUsed && m_canCertify);

    const bool canEncrypt = isCertificateUsed && m_canCertify;
    const bool isEncrypted = canEncrypt && ui->encryptCheckBox->isChecked();
    ui->encryptCheckBox->setEnabled(canEncrypt);
    ui->openPasswordLabel->setEnabled(isEncrypted);
    ui->openPasswordEdit->setEnabled(isEncrypted);
    ui->openPasswordConfirmLabel->setEnabled(isEncrypted);
    ui->openPasswordConfirmEdit->setEnabled(isEncrypted);

    ui->timestampUrlLabel->setEnabled(isTimestampUsed);
    ui->timestampUrlCombo->setEnabled(isTimestampUsed);
    ui->longTermCheckBox->setEnabled(signatureType == SignatureWithTimestamp);
}

void SignDialog::loadSettings()
{
    QSettings settings;
    settings.beginGroup(SETTINGS_GROUP);

    const int signatureTypeIndex = ui->signatureTypeCombo->findData(settings.value(SETTINGS_SIGNATURE_TYPE, SignatureWithTimestamp).toInt());
    if (signatureTypeIndex != -1)
    {
        ui->signatureTypeCombo->setCurrentIndex(signatureTypeIndex);
    }

    ui->longTermCheckBox->setChecked(settings.value(SETTINGS_LONG_TERM, true).toBool());

    const QString timestampUrl = settings.value(SETTINGS_TIMESTAMP_URL).toString();
    if (!timestampUrl.isEmpty())
    {
        ui->timestampUrlCombo->setCurrentText(timestampUrl);
    }

    const int certificationIndex = ui->certificationCombo->findData(settings.value(SETTINGS_CERTIFICATION, int(pdf::PDFDocumentSigner::Certification::NoChanges)).toInt());
    if (m_canCertify && certificationIndex != -1)
    {
        ui->certificationCombo->setCurrentIndex(certificationIndex);
    }

    settings.endGroup();
}

void SignDialog::saveSettings()
{
    QSettings settings;
    settings.beginGroup(SETTINGS_GROUP);
    settings.setValue(SETTINGS_SIGNATURE_TYPE, int(getSignatureType()));
    settings.setValue(SETTINGS_LONG_TERM, ui->longTermCheckBox->isChecked());
    settings.setValue(SETTINGS_TIMESTAMP_URL, getTimestampUrl());
    if (m_canCertify)
    {
        settings.setValue(SETTINGS_CERTIFICATION, ui->certificationCombo->currentData().toInt());
    }
    settings.endGroup();
}

}   // namespace pdfplugin



