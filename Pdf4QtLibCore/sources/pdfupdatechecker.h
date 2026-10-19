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

#ifndef PDFUPDATECHECKER_H
#define PDFUPDATECHECKER_H

#include "pdfglobal.h"
#include "pdfversionhistory.h"

#include <QUrl>
#include <QString>
#include <QByteArray>
#include <QCoreApplication>

#include <functional>
#include <optional>

namespace pdf
{

/// Updates of Andre PDF. Every release on GitHub carries a manifest (version, the
/// history of the versions, address, size and SHA-256 of the installer) and its
/// ECDSA P-256 signature made with the release key of Andre PDF. An update is offered
/// only when the signature verifies with the public key built into the program, the
/// manifest names a newer version and the installer comes from GitHub over HTTPS;
/// the downloaded installer must have the size and the SHA-256 of the manifest.
class PDF4QTLIBCORESHARED_EXPORT PDFUpdateChecker
{
    Q_DECLARE_TR_FUNCTIONS(pdf::PDFUpdateChecker)

public:
    PDFUpdateChecker() = delete;

    struct Update
    {
        QString version;
        PDFVersionHistory::Releases releases;   ///< The versions newer than the installed one, newest first
        QUrl url;                               ///< Installer
        QByteArray sha256;                      ///< SHA-256 of the installer (raw bytes)
        qint64 size = 0;                        ///< Size of the installer in bytes
    };

    /// Verifies the signature (base64 of a DER ECDSA signature) of the data
    static bool verifySignature(const QByteArray& data, const QByteArray& signatureBase64, const QByteArray& publicKeyPem);

    /// Reads a verified manifest. Returns nothing, when it offers no newer version
    /// (errorMessage empty) or when it is not acceptable (errorMessage says why).
    /// \param manifest Manifest (JSON)
    /// \param currentVersion Installed version
    /// \param platform "windows" (the installer of the platform)
    /// \param errorMessage Error message
    static std::optional<Update> parseManifest(const QByteArray& manifest, const QString& currentVersion, const QString& platform, QString* errorMessage);

    /// Downloads and verifies the manifest of the latest release and returns the
    /// update it offers. Blocking: run it in a worker thread.
    static std::optional<Update> check(const QString& currentVersion, QString* errorMessage);

    /// Downloads the installer into the directory and verifies its size and SHA-256.
    /// Returns the file, empty on failure (a partial or wrong file is deleted).
    /// \param progress Called with the received and the total bytes, can be empty
    static QString download(const Update& update, const QString& directory, const std::function<void(qint64, qint64)>& progress, QString* errorMessage);

    /// Starts the installer (Windows: msiexec), the program should quit then
    static bool startInstaller(const QString& fileName, QString* errorMessage);

    /// Returns the public key of the releases (PEM)
    static QByteArray getReleasePublicKey();

    /// Returns the name of the platform of this build ("windows", "linux", "android")
    static QString getPlatform();

    /// For the tests: another address of the release files, another public key, and
    /// the hosts allowed for the installer
    static void setTestConfiguration(const QString& baseUrl, const QByteArray& publicKeyPem, const QStringList& allowedHosts);

    /// Largest manifest accepted, bytes
    static constexpr qint64 MAXIMAL_MANIFEST_SIZE = 1024 * 1024;

    /// Largest installer accepted, bytes
    static constexpr qint64 MAXIMAL_INSTALLER_SIZE = 600ll * 1024 * 1024;
};

}   // namespace pdf

#endif // PDFUPDATECHECKER_H
