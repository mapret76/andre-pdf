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

#include "pdfupdatechecker.h"

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QSaveFile>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QRegularExpression>
#include <QNetworkAccessManager>

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/bio.h>

#include <algorithm>

#include "pdfdbgheap.h"

namespace pdf
{

class PDFUpdateCheckerHelper
{
public:
    PDFUpdateCheckerHelper() = delete;

    struct Configuration
    {
        QString baseUrl = QStringLiteral("https://github.com/mapret76/andre-pdf/releases/latest/download");
        QByteArray publicKey;
        QStringList allowedHosts = { QStringLiteral("github.com"), QStringLiteral("objects.githubusercontent.com"), QStringLiteral("release-assets.githubusercontent.com") };
        bool requireHttps = true;
    };

    static Configuration& configuration()
    {
        static Configuration value;
        return value;
    }

    /// Downloads a URL with a size limit, synchronously; redirects to other origins
    /// are followed only to HTTPS (the release files of GitHub are redirected)
    static std::optional<QByteArray> get(const QUrl& url, qint64 limit, QString* errorMessage, QIODevice* output = nullptr, const std::function<void(qint64, qint64)>& progress = nullptr)
    {
        QNetworkAccessManager manager;
        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Andre PDF updater"));
        request.setTransferTimeout(60000);
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, configuration().requireHttps ? QNetworkRequest::NoLessSafeRedirectPolicy : QNetworkRequest::SameOriginRedirectPolicy);
        QNetworkReply* reply = manager.get(request);

        QByteArray data;
        bool isTooLarge = false;
        QObject::connect(reply, &QNetworkReply::readyRead, reply, [&]()
        {
            const QByteArray chunk = reply->readAll();
            const qint64 received = (output ? output->size() : data.size()) + chunk.size();
            if (received > limit)
            {
                isTooLarge = true;
                reply->abort();
                return;
            }
            if (output)
            {
                output->write(chunk);
            }
            else
            {
                data.append(chunk);
            }
            if (progress)
            {
                progress(received, reply->header(QNetworkRequest::ContentLengthHeader).toLongLong());
            }
        });

        QEventLoop loop;
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        if (!reply->isFinished())
        {
            loop.exec();
        }

        const bool isOK = reply->error() == QNetworkReply::NoError && !isTooLarge;
        if (!isOK && errorMessage)
        {
            *errorMessage = isTooLarge ? PDFUpdateChecker::tr("%1 is larger than expected.").arg(url.toString())
                                       : PDFUpdateChecker::tr("Download of %1 failed: %2").arg(url.toString(), reply->errorString());
        }
        reply->deleteLater();
        return isOK ? std::optional<QByteArray>(data) : std::nullopt;
    }
};

QByteArray PDFUpdateChecker::getReleasePublicKey()
{
    if (!PDFUpdateCheckerHelper::configuration().publicKey.isEmpty())
    {
        return PDFUpdateCheckerHelper::configuration().publicKey;
    }

    QFile file(QStringLiteral(":/andre-pdf/release-key.pub.pem"));
    return file.open(QFile::ReadOnly) ? file.readAll() : QByteArray();
}

QString PDFUpdateChecker::getPlatform()
{
#if defined(Q_OS_ANDROID)
    return QStringLiteral("android");
#elif defined(Q_OS_WIN)
    return QStringLiteral("windows");
#else
    return QStringLiteral("linux");
#endif
}

void PDFUpdateChecker::setTestConfiguration(const QString& baseUrl, const QByteArray& publicKeyPem, const QStringList& allowedHosts)
{
    PDFUpdateCheckerHelper::Configuration& configuration = PDFUpdateCheckerHelper::configuration();
    configuration = PDFUpdateCheckerHelper::Configuration();
    if (!baseUrl.isEmpty())
    {
        configuration.baseUrl = baseUrl;
        configuration.publicKey = publicKeyPem;
        configuration.allowedHosts = allowedHosts;
        configuration.requireHttps = false;
    }
}

bool PDFUpdateChecker::verifySignature(const QByteArray& data, const QByteArray& signatureBase64, const QByteArray& publicKeyPem)
{
    const QByteArray signature = QByteArray::fromBase64(signatureBase64.trimmed(), QByteArray::AbortOnBase64DecodingErrors);
    if (signature.isEmpty() || publicKeyPem.isEmpty())
    {
        return false;
    }

    BIO* bio = BIO_new_mem_buf(publicKeyPem.constData(), int(publicKeyPem.size()));
    EVP_PKEY* key = bio ? PEM_read_bio_PUBKEY(bio, nullptr, nullptr, nullptr) : nullptr;
    BIO_free(bio);
    if (!key)
    {
        return false;
    }

    // Only an EC key of the curve P-256 is accepted
    bool isVerified = false;
    if (EVP_PKEY_is_a(key, "EC") && EVP_PKEY_get_bits(key) == 256)
    {
        EVP_MD_CTX* context = EVP_MD_CTX_new();
        if (context && EVP_DigestVerifyInit(context, nullptr, EVP_sha256(), nullptr, key) == 1 &&
            EVP_DigestVerifyUpdate(context, data.constData(), size_t(data.size())) == 1)
        {
            isVerified = EVP_DigestVerifyFinal(context, reinterpret_cast<const unsigned char*>(signature.constData()), size_t(signature.size())) == 1;
        }
        EVP_MD_CTX_free(context);
    }
    EVP_PKEY_free(key);
    return isVerified;
}

std::optional<PDFUpdateChecker::Update> PDFUpdateChecker::parseManifest(const QByteArray& manifest, const QString& currentVersion, const QString& platform, QString* errorMessage)
{
    auto fail = [errorMessage](const QString& message)
    {
        if (errorMessage)
        {
            *errorMessage = message;
        }
        return std::nullopt;
    };
    if (errorMessage)
    {
        errorMessage->clear();
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(manifest, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        return fail(tr("The update information is damaged."));
    }

    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("product")).toString() != QStringLiteral("Andre PDF"))
    {
        return fail(tr("The update information is for another product."));
    }

    Update update;
    update.version = root.value(QStringLiteral("version")).toString();
    if (!PDFVersionHistory::isValidVersion(update.version))
    {
        return fail(tr("The update information has no valid version."));
    }
    if (PDFVersionHistory::compare(update.version, currentVersion) <= 0)
    {
        return std::nullopt;  // up to date (an older manifest replayed is ignored, too)
    }

    PDFVersionHistory::Releases releases;
    for (const QJsonValue& value : root.value(QStringLiteral("releases")).toArray())
    {
        const QJsonObject object = value.toObject();
        PDFVersionHistory::Release release;
        release.version = object.value(QStringLiteral("version")).toString();
        release.date = QDate::fromString(object.value(QStringLiteral("date")).toString(), Qt::ISODate);
        release.summary = object.value(QStringLiteral("summary")).toString();
        for (const QJsonValue& change : object.value(QStringLiteral("changes")).toArray())
        {
            release.changes << change.toString();
        }
        if (PDFVersionHistory::isValidVersion(release.version) && PDFVersionHistory::compare(release.version, update.version) <= 0)
        {
            releases.push_back(release);
        }
    }
    update.releases = PDFVersionHistory::getNewer(releases, currentVersion);
    std::stable_sort(update.releases.begin(), update.releases.end(), [](const auto& a, const auto& b) { return PDFVersionHistory::compare(a.version, b.version) > 0; });
    if (update.releases.empty() || update.releases.front().version != update.version || update.releases.front().summary.isEmpty())
    {
        return fail(tr("The update information has no summary of version %1.").arg(update.version));
    }

    const QJsonObject installer = root.value(platform).toObject();
    update.url = QUrl(installer.value(QStringLiteral("url")).toString());
    update.sha256 = QByteArray::fromHex(installer.value(QStringLiteral("sha256")).toString().toLatin1());
    update.size = qint64(installer.value(QStringLiteral("size")).toDouble());
    if (!update.url.isValid() || update.url.isRelative() || installer.isEmpty())
    {
        return fail(tr("Version %1 has no installer for this computer.").arg(update.version));
    }
    const PDFUpdateCheckerHelper::Configuration& configuration = PDFUpdateCheckerHelper::configuration();
    if ((configuration.requireHttps && update.url.scheme() != QStringLiteral("https")) || !configuration.allowedHosts.contains(update.url.host(), Qt::CaseInsensitive))
    {
        return fail(tr("The installer of version %1 is not offered from GitHub over HTTPS.").arg(update.version));
    }
    if (update.sha256.size() != 32 || update.size <= 0 || update.size > MAXIMAL_INSTALLER_SIZE)
    {
        return fail(tr("The installer of version %1 has no valid size or SHA-256.").arg(update.version));
    }
    return update;
}

std::optional<PDFUpdateChecker::Update> PDFUpdateChecker::check(const QString& currentVersion, QString* errorMessage)
{
    const QString base = PDFUpdateCheckerHelper::configuration().baseUrl;
    const std::optional<QByteArray> manifest = PDFUpdateCheckerHelper::get(QUrl(base + QStringLiteral("/andre-pdf-update.json")), MAXIMAL_MANIFEST_SIZE, errorMessage);
    if (!manifest)
    {
        return std::nullopt;
    }
    const std::optional<QByteArray> signature = PDFUpdateCheckerHelper::get(QUrl(base + QStringLiteral("/andre-pdf-update.json.sig")), 4096, errorMessage);
    if (!signature)
    {
        return std::nullopt;
    }
    if (!verifySignature(*manifest, *signature, getReleasePublicKey()))
    {
        if (errorMessage)
        {
            *errorMessage = tr("The update information is not signed with the release key of Andre PDF. Nothing was downloaded.");
        }
        return std::nullopt;
    }
    return parseManifest(*manifest, currentVersion, getPlatform(), errorMessage);
}

QString PDFUpdateChecker::download(const Update& update, const QString& directory, const std::function<void(qint64, qint64)>& progress, QString* errorMessage)
{
    QDir().mkpath(directory);
    // The name is ours, nothing of the manifest goes into the path
    const QString fileName = QDir(directory).filePath(QString("AndrePDF_%1%2").arg(update.version, getPlatform() == QStringLiteral("windows") ? QStringLiteral(".msi") : QStringLiteral(".bin")));

    QFile file(fileName);
    if (!file.open(QFile::WriteOnly | QFile::Truncate))
    {
        if (errorMessage)
        {
            *errorMessage = tr("The installer cannot be saved: %1").arg(file.errorString());
        }
        return QString();
    }

    const std::optional<QByteArray> result = PDFUpdateCheckerHelper::get(update.url, update.size, errorMessage, &file, progress);
    file.close();

    QByteArray hash;
    if (result && QFile::exists(fileName))
    {
        QFile check(fileName);
        if (check.open(QFile::ReadOnly))
        {
            QCryptographicHash sha256(QCryptographicHash::Sha256);
            sha256.addData(&check);
            hash = sha256.result();
        }
    }

    if (!result || QFileInfo(fileName).size() != update.size || hash != update.sha256)
    {
        QFile::remove(fileName);
        if (result && errorMessage)
        {
            *errorMessage = tr("The downloaded installer does not match the signed update information; it was deleted.");
        }
        return QString();
    }
    return fileName;
}

bool PDFUpdateChecker::startInstaller(const QString& fileName, QString* errorMessage)
{
#ifdef Q_OS_WIN
    // msiexec of the system, not one found in the path
    const QString msiexec = QDir::fromNativeSeparators(qEnvironmentVariable("SystemRoot", QStringLiteral("C:\\Windows"))) + QStringLiteral("/System32/msiexec.exe");
    if (QProcess::startDetached(QDir::toNativeSeparators(msiexec), { QStringLiteral("/i"), QDir::toNativeSeparators(fileName) }))
    {
        return true;
    }
    if (errorMessage)
    {
        *errorMessage = tr("The installer could not be started.");
    }
    return false;
#else
    Q_UNUSED(fileName);
    if (errorMessage)
    {
        *errorMessage = tr("Install the downloaded version with the package manager of this system: %1").arg(fileName);
    }
    return false;
#endif
}

}   // namespace pdf
