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

#ifndef PDFDOCUMENTSIGNER_H
#define PDFDOCUMENTSIGNER_H

#include "pdfglobal.h"
#include "pdfobject.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDateTime>

#include <optional>
#include <QString>
#include <QStringList>

#include <functional>

namespace pdf
{
class PDFDocument;
class PDFDocumentBuilder;
class PDFForm;
class PDFProgress;

/// Creates a digitally signed document.
///
/// The size of a digital signature is not stable - signing the same data twice
/// can produce results which differ by a few bytes, because the length of the
/// DER encoded values varies. The space reserved for the signature in the
/// document therefore cannot be derived from a single trial signature. This
/// class reserves a padded area, checks that the final signature fits into it,
/// and repeats the whole cycle with a larger reservation when it does not.
///
/// When the data of the original document are available, the signed document
/// is written as an incremental update of them, so the bytes covered by the
/// signatures already present in the document are not touched and these
/// signatures stay valid. Only the newly created signature is verified before
/// the document is handed over to the caller - the signatures already present
/// can be invalid for reasons unrelated to the signing, but a document with a
/// damaged new signature is never produced.
class PDF4QTLIBCORESHARED_EXPORT PDFDocumentSigner
{
    Q_DECLARE_TR_FUNCTIONS(pdf::PDFDocumentSigner)

public:
    PDFDocumentSigner() = delete;

    /// Creates the digital signature of the given data. Returns false, if the
    /// signature cannot be created.
    using SignFunction = std::function<bool(const QByteArray& dataToBeSigned, QByteArray& signature)>;

    /// Creates the signature form field referencing the given signature
    /// dictionary, sets the optional entries of the signature dictionary and
    /// returns the reference of the created field. It can be called repeatedly,
    /// because the document is rebuilt when the space reserved for the signature
    /// must be enlarged.
    using CreateSignatureFieldFunction = std::function<PDFObjectReference(PDFDocumentBuilder& builder, PDFObjectReference signatureDictionary)>;

    enum class Result
    {
        OK,                             ///< Document was signed
        NoDocument,                     ///< No document to be signed was given
        SigningFailed,                  ///< Digital signature could not be created
        WriteFailed,                    ///< Document could not be written
        PlaceholderNotFound,            ///< Space reserved for the signature was not found in the written document
        SignatureTooLarge,              ///< Signature did not fit into the reserved space
        VerificationFailed,             ///< Newly created signature of the written document could not be verified
        ExistingSignaturesNotPreserved, ///< Document contains signatures, which would be damaged by writing the document
        CertificationNotFirst           ///< Certification signature was requested, but it would not be the first signature of the document
    };

    /// Certification (DocMDP) of the document by the created signature, the value
    /// is the permission /P of the transform parameters (ISO 32000-2, 12.8.2.2).
    /// A certification signature must be the first signature of the document.
    enum class Certification
    {
        None = 0,                       ///< Approval signature, the document is not certified
        NoChanges = 1,                  ///< No changes of the document are permitted
        FormFilling = 2,                ///< Filling of forms, instantiating of page templates and signing are permitted
        FormFillingAndAnnotations = 3   ///< As FormFilling, plus creation, deletion and modification of annotations
    };

    struct Parameters
    {
        /// Document to be signed
        const PDFDocument* document = nullptr;

        /// Data of the file the document was loaded from. When they are given,
        /// the signed document is written as an incremental update of them and
        /// the signatures already present in the document stay valid. Without
        /// them the document is written as a whole, which is possible only when
        /// it contains no signatures yet.
        QByteArray originalDocumentData;

        /// Creates the digital signature, must be set
        SignFunction signFunction;

        /// Creates the signature form field, must be set
        CreateSignatureFieldFunction createSignatureFieldFunction;

        QByteArray filter = "Adobe.PPKLite";
        QByteArray subfilter = "ETSI.CAdES.detached";

        /// Type of the created signature dictionary - "Sig" for a digital
        /// signature and "DocTimeStamp" for a document timestamp
        QByteArray signatureDictionaryType = "Sig";

        QDateTime signingTime = QDateTime::currentDateTime();

        /// Certification of the document, only for a signature (not for a document timestamp)
        Certification certification = Certification::None;

        /// When set, the document is encrypted (AES-256) before it is signed, and the
        /// password is needed to open it; an empty password opens it, but its owner
        /// permissions still apply. The owner password is random, the permissions
        /// follow the certification (printing, copying and accessibility always,
        /// form filling and comments as the certification allows them). The whole
        /// document is written again, so it must not be signed yet.
        std::optional<QString> encryptionUserPassword;

        /// Password opening the document, when it is already encrypted (the signed
        /// document is read back with it to verify the new signature)
        QString openPassword;

        /// Progress reporting of the document writer, can be nullptr
        PDFProgress* progress = nullptr;
    };

    /// Signs the document. Returns Result::OK and fills \p signedDocument only
    /// when the newly created signature of the written document has been
    /// verified, otherwise \p signedDocument is left untouched.
    /// \param parameters Signing parameters
    /// \param signedDocument Bytes of the signed document
    static Result sign(const Parameters& parameters, QByteArray& signedDocument);

    /// Returns the localized message describing the result
    /// \param result Result
    static QString getResultMessage(Result result);

    /// What a reader finds in a signed file, read back from its bytes
    struct SignedFileCheck
    {
        bool isOpened = false;              ///< The file was read (with the password, when it is encrypted)
        bool isEncrypted = false;           ///< The file is encrypted
        bool isSignatureFound = false;      ///< The signature field of the given name holds a signature
        bool isSignatureValid = false;      ///< The signature is mathematically valid
        bool isCoveringWholeFile = false;   ///< The signature covers every byte of the file (nothing appended after it)
        Certification certification = Certification::None; ///< Certification of the document
        QStringList errors;                 ///< Errors of the signature verification

        /// Returns true, if everything checked is in order
        bool isOK() const { return isOpened && isSignatureFound && isSignatureValid && isCoveringWholeFile; }
    };

    /// Reads a signed file back and checks the signature of the given field. The
    /// trust in the certificate is not checked, it is a decision of the reader.
    /// \param signedFile Bytes of the file
    /// \param signatureFieldName Fully qualified name of the signature field
    /// \param password Password opening the file, when it is encrypted
    static SignedFileCheck checkSignedFile(const QByteArray& signedFile, const QString& signatureFieldName, const QString& password);

    /// Returns true, if the document contains at least one signed signature field
    /// \param document Document
    static bool hasExistingSignatures(const PDFDocument* document);

private:
    /// Space added to the size of the trial signature. It is only the initial
    /// estimate - when the final signature does not fit, the reserved space is
    /// enlarged and the document is signed again.
    static constexpr int INITIAL_RESERVE_EXTRA_BYTES = 1024;

    /// Maximal count of the signing attempts
    static constexpr int MAXIMAL_ATTEMPT_COUNT = 4;

    /// Value of the Type entry of the signature dictionary of a document timestamp
    static constexpr const char* DOCUMENT_TIMESTAMP_TYPE = "DocTimeStamp";

    /// Placeholder written into the byte range array, it is replaced by the real
    /// offsets when the document is written
    static constexpr const char* BYTE_RANGE_MARK_STRING = "123456789123";
    static constexpr PDFInteger BYTE_RANGE_MARK = 123456789123;

    /// Adds the DocMDP signature reference to the signature dictionary and the
    /// catalog /Perms /DocMDP entry referencing the signature dictionary
    /// \param builder Builder of the signed document
    /// \param signatureDictionary Signature dictionary
    /// \param certification Certification, must not be None
    /// Returns the permissions of the encrypted document for the certification
    static uint32_t getEncryptionPermissions(Certification certification);

    static void addCertification(PDFDocumentBuilder& builder, PDFObjectReference signatureDictionary, Certification certification);

    /// Returns the data signed only to find out the size of the signature
    static QByteArray getTrialData();

    /// Signs the document once. When the signature does not fit into the
    /// reserved space, \p requiredSignatureSize is set to the size of the
    /// signature which did not fit.
    /// \param parameters Signing parameters
    /// \param trialSignature Signature of the trial data, used as the placeholder
    /// \param reservedSignatureSize Size of the space reserved for the signature
    /// \param preserveExistingSignatures Document must be written as an incremental update
    /// \param signedDocument Bytes of the signed document
    /// \param requiredSignatureSize Size of the signature, which did not fit
    static Result signAttempt(const Parameters& parameters,
                              const QByteArray& trialSignature,
                              int reservedSignatureSize,
                              bool preserveExistingSignatures,
                              QByteArray& signedDocument,
                              int& requiredSignatureSize);

    /// Verifies the signature of the given field in the written document
    /// \param signedDocument Bytes of the signed document
    /// \param signatureField Field holding the signature being verified
    /// \param isDocumentTimestamp Field holds a document timestamp
    /// \param password Password opening the document, when it is encrypted
    static bool verifySignedDocument(const QByteArray& signedDocument,
                                     PDFObjectReference signatureField,
                                     bool isDocumentTimestamp,
                                     const QString& password);

    /// Verifies, that the timestamp token of the given field belongs to the
    /// bytes of the written document covered by it. The certificate chain of
    /// the timestamp authority is not verified - it is unknown here and the
    /// trust in the authority is a decision of the user, not a property of
    /// the signing process.
    /// \param form Form of the written document
    /// \param signedDocument Bytes of the signed document
    /// \param signatureField Field holding the timestamp being verified
    static bool verifyDocumentTimestamp(const PDFForm& form,
                                        const QByteArray& signedDocument,
                                        PDFObjectReference signatureField);
};

}   // namespace pdf

#endif // PDFDOCUMENTSIGNER_H
