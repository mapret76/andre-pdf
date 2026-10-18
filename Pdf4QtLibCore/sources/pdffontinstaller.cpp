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

#include "pdffontinstaller.h"

#include <QDir>
#include <QUrl>
#include <QFile>
#include <QTimer>
#include <QSaveFile>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonObject>
#include <QUrlQuery>
#include <QFontDatabase>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QStandardPaths>
#include <QNetworkAccessManager>

#ifdef Q_OS_WIN
#include <QSettings>
#include <Windows.h>
#endif

#include "pdfdbgheap.h"

namespace pdf
{

class PDFFontInstallerHelper
{
public:
    PDFFontInstallerHelper() = delete;

    static QString& apiBaseUrl()
    {
        static QString url = QStringLiteral("https://api.fontsource.org/v1");
        return url;
    }

    static QString& testInstallDirectory()
    {
        static QString directory;
        return directory;
    }

    /// Downloads a URL synchronously (with a timeout), returns nothing on failure
    static std::optional<QByteArray> get(const QUrl& url, QString* errorMessage)
    {
        QNetworkAccessManager manager;
        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Andre PDF"));
        request.setTransferTimeout(30000);
        QNetworkReply* reply = manager.get(request);

        QEventLoop loop;
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();

        const QByteArray data = reply->readAll();
        const bool isOK = reply->error() == QNetworkReply::NoError;
        if (!isOK && errorMessage)
        {
            *errorMessage = PDFFontInstaller::tr("Download of %1 failed: %2").arg(url.toString(), reply->errorString());
        }
        reply->deleteLater();
        return isOK ? std::optional<QByteArray>(data) : std::nullopt;
    }

    static QString getCacheDirectory()
    {
        return QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/fonts");
    }
};

bool PDFFontInstaller::isFreeLicense(const QString& license)
{
    static const QStringList freeLicenses = { QStringLiteral("OFL-1.1"), QStringLiteral("Apache-2.0"), QStringLiteral("UFL-1.0"),
                                              QStringLiteral("MIT"), QStringLiteral("CC0-1.0"), QStringLiteral("GPL-2.0-with-font-exception") };
    return freeLicenses.contains(license, Qt::CaseInsensitive);
}

std::optional<PDFFontInstaller::FontInfo> PDFFontInstaller::find(const QString& family, QString* errorMessage)
{
    QUrl url(PDFFontInstallerHelper::apiBaseUrl() + QStringLiteral("/fonts"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("family"), family);
    url.setQuery(query);

    const std::optional<QByteArray> data = PDFFontInstallerHelper::get(url, errorMessage);
    if (!data)
    {
        return std::nullopt;
    }

    for (const QJsonValue& value : QJsonDocument::fromJson(*data).array())
    {
        const QJsonObject object = value.toObject();
        FontInfo info;
        info.id = object.value(QStringLiteral("id")).toString();
        info.family = object.value(QStringLiteral("family")).toString();
        info.license = object.value(QStringLiteral("license")).toString();
        info.category = object.value(QStringLiteral("category")).toString();
        if (info.family.compare(family, Qt::CaseInsensitive) == 0 && !info.id.isEmpty())
        {
            return info;
        }
    }

    if (errorMessage)
    {
        errorMessage->clear();
    }
    return std::nullopt;
}

QByteArray PDFFontInstaller::download(const FontInfo& font, int weight, bool italic, QString* errorMessage)
{
    if (!isFreeLicense(font.license))
    {
        if (errorMessage)
        {
            *errorMessage = tr("The font %1 has the licence %2, which is not free; it is not downloaded.").arg(font.family, font.license);
        }
        return QByteArray();
    }

    const std::optional<QByteArray> data = PDFFontInstallerHelper::get(QUrl(PDFFontInstallerHelper::apiBaseUrl() + QStringLiteral("/fonts/") + font.id), errorMessage);
    if (!data)
    {
        return QByteArray();
    }

    const QJsonObject variants = QJsonDocument::fromJson(*data).object().value(QStringLiteral("variants")).toObject();
    const QJsonObject styles = variants.value(QString::number(weight)).toObject();
    const QString ttf = styles.value(italic ? QStringLiteral("italic") : QStringLiteral("normal")).toObject()
                              .value(QStringLiteral("latin")).toObject()
                              .value(QStringLiteral("url")).toObject()
                              .value(QStringLiteral("ttf")).toString();
    if (ttf.isEmpty())
    {
        if (errorMessage)
        {
            errorMessage->clear();  // the style does not exist, not an error
        }
        return QByteArray();
    }

    return PDFFontInstallerHelper::get(QUrl(ttf), errorMessage).value_or(QByteArray());
}

QStringList PDFFontInstaller::getCandidateFamilies()
{
    // Free fonts common in contracts, letters and forms, and the free twins of the
    // commercial ones (Arimo = Arial, Tinos = Times New Roman, Carlito = Calibri, ...)
    return {
        QStringLiteral("Arimo"), QStringLiteral("Tinos"), QStringLiteral("Cousine"), QStringLiteral("Carlito"), QStringLiteral("Caladea"),
        QStringLiteral("Open Sans"), QStringLiteral("Roboto"), QStringLiteral("Lato"), QStringLiteral("Noto Sans"), QStringLiteral("Noto Serif"),
        QStringLiteral("Source Sans 3"), QStringLiteral("Source Serif 4"), QStringLiteral("PT Sans"), QStringLiteral("PT Serif"),
        QStringLiteral("Montserrat"), QStringLiteral("Raleway"), QStringLiteral("Merriweather"), QStringLiteral("Lora"),
        QStringLiteral("EB Garamond"), QStringLiteral("Libre Baskerville"), QStringLiteral("Gelasio"), QStringLiteral("Nunito Sans"),
        QStringLiteral("Work Sans"), QStringLiteral("Inter"), QStringLiteral("Roboto Mono"), QStringLiteral("Source Code Pro")
    };
}

QString PDFFontInstaller::loadForTrial(const QString& family, QString* errorMessage)
{
    const QString cacheDirectory = PDFFontInstallerHelper::getCacheDirectory();
    const QString fileName = cacheDirectory + "/" + QString(family).replace(QChar(' '), QChar('_')) + QStringLiteral("-400-normal.ttf");

    QByteArray data;
    QFile cached(fileName);
    if (cached.open(QFile::ReadOnly))
    {
        data = cached.readAll();
        cached.close();
    }
    else
    {
        const std::optional<FontInfo> font = find(family, errorMessage);
        if (!font)
        {
            return QString();
        }
        data = download(*font, 400, false, errorMessage);
        if (data.isEmpty())
        {
            return QString();
        }
        QDir().mkpath(cacheDirectory);
        QSaveFile file(fileName);
        if (file.open(QFile::WriteOnly))
        {
            file.write(data);
            file.commit();
        }
    }

    const int id = QFontDatabase::addApplicationFontFromData(data);
    const QStringList families = id >= 0 ? QFontDatabase::applicationFontFamilies(id) : QStringList();
    return families.value(0);
}

QString PDFFontInstaller::getInstallDirectory()
{
    if (!PDFFontInstallerHelper::testInstallDirectory().isEmpty())
    {
        return PDFFontInstallerHelper::testInstallDirectory();
    }

#ifdef Q_OS_WIN
    // %LOCALAPPDATA%\Microsoft\Windows\Fonts holds the fonts installed for the user (Windows 10 1809+)
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/Microsoft/Windows/Fonts");
#else
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/fonts");
#endif
}

bool PDFFontInstaller::install(const FontInfo& font, QString* errorMessage)
{
    const QString directory = getInstallDirectory();
    if (!QDir().mkpath(directory))
    {
        if (errorMessage)
        {
            *errorMessage = tr("The folder %1 cannot be created.").arg(QDir::toNativeSeparators(directory));
        }
        return false;
    }

    int installed = 0;
    struct Style { int weight; bool italic; const char* name; };
    for (const Style& style : { Style{ 400, false, "Regular" }, Style{ 700, false, "Bold" }, Style{ 400, true, "Italic" }, Style{ 700, true, "Bold Italic" } })
    {
        QString styleError;
        const QByteArray data = download(font, style.weight, style.italic, &styleError);
        if (data.isEmpty())
        {
            if (!styleError.isEmpty())
            {
                if (errorMessage)
                {
                    *errorMessage = styleError;
                }
                return false;
            }
            continue;   // the family has no such style
        }

        const QString fileName = QString("%1/%2-%3.ttf").arg(directory, QString(font.family).remove(QChar(' ')), QString::fromLatin1(style.name).remove(QChar(' ')));
        QSaveFile file(fileName);
        if (!file.open(QFile::WriteOnly) || file.write(data) != data.size() || !file.commit())
        {
            if (errorMessage)
            {
                *errorMessage = tr("The font file %1 cannot be written.").arg(QDir::toNativeSeparators(fileName));
            }
            return false;
        }

#ifdef Q_OS_WIN
        if (PDFFontInstallerHelper::testInstallDirectory().isEmpty())
        {
            // Registered for the user, so every program finds it; AddFontResource makes
            // it available to the running programs too
            QSettings fonts(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows NT\\CurrentVersion\\Fonts"), QSettings::NativeFormat);
            fonts.setValue(QString("%1 %2 (TrueType)").arg(font.family, QString::fromLatin1(style.name)), QDir::toNativeSeparators(fileName));
            AddFontResourceW(reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(fileName).utf16()));
        }
#endif

        QFontDatabase::addApplicationFont(fileName);
        ++installed;
    }

#ifdef Q_OS_WIN
    if (installed > 0 && PDFFontInstallerHelper::testInstallDirectory().isEmpty())
    {
        SendMessageTimeoutW(HWND_BROADCAST, WM_FONTCHANGE, 0, 0, SMTO_ABORTIFHUNG, 1000, nullptr);
    }
#endif

    if (installed == 0 && errorMessage)
    {
        *errorMessage = tr("No style of the font %1 could be downloaded.").arg(font.family);
    }
    return installed > 0;
}

void PDFFontInstaller::setTestConfiguration(const QString& apiBaseUrl, const QString& installDirectory)
{
    PDFFontInstallerHelper::apiBaseUrl() = apiBaseUrl;
    PDFFontInstallerHelper::testInstallDirectory() = installDirectory;
}

}   // namespace pdf
