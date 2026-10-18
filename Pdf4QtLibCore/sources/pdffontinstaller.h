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

#ifndef PDFFONTINSTALLER_H
#define PDFFONTINSTALLER_H

#include "pdfglobal.h"

#include <QString>
#include <QStringList>
#include <QByteArray>
#include <QCoreApplication>

#include <optional>

namespace pdf
{

/// Finds free fonts in the Fontsource catalogue (the Google Fonts and other open
/// fonts, https://fontsource.org), downloads them and installs them for the user.
/// Only fonts under a free licence are downloaded; a commercial font is never
/// fetched from anywhere - its free metric-compatible substitute is offered instead
/// (PDFFontIdentifier::getMetricCompatibleSubstitutes).
class PDF4QTLIBCORESHARED_EXPORT PDFFontInstaller
{
    Q_DECLARE_TR_FUNCTIONS(pdf::PDFFontInstaller)

public:
    PDFFontInstaller() = delete;

    struct FontInfo
    {
        QString id;
        QString family;
        QString license;
        QString category;       ///< serif, sans-serif, monospace, display, handwriting
    };

    /// Returns true, if the licence allows to download, use and embed the font freely
    static bool isFreeLicense(const QString& license);

    /// Finds the family in the catalogue
    /// \param family Family name
    /// \param errorMessage Error message, when nothing is returned because of an error
    static std::optional<FontInfo> find(const QString& family, QString* errorMessage = nullptr);

    /// Downloads the TrueType font of the Latin subset (which covers English and Afrikaans)
    static QByteArray download(const FontInfo& font, int weight, bool italic, QString* errorMessage = nullptr);

    /// Families of common document fonts in the catalogue, tried when the font of a
    /// scan is not installed
    static QStringList getCandidateFamilies();

    /// Downloads the regular style into the cache of the application and loads it for
    /// this session (not installed), so that it can be tried by the font matching.
    /// Returns the family as loaded, empty on failure.
    static QString loadForTrial(const QString& family, QString* errorMessage = nullptr);

    /// Downloads the regular, bold, italic and bold italic styles and installs them
    /// for the user (no administrator rights needed) and for this session
    static bool install(const FontInfo& font, QString* errorMessage = nullptr);

    /// Directory the fonts are installed into: on Windows the fonts of the user
    /// (%LOCALAPPDATA%\Microsoft\Windows\Fonts), elsewhere ~/.local/share/fonts
    static QString getInstallDirectory();

    /// For the tests: the catalogue is read from another address and the fonts are
    /// installed into another directory (and not registered in the system)
    static void setTestConfiguration(const QString& apiBaseUrl, const QString& installDirectory);
};

}   // namespace pdf

#endif // PDFFONTINSTALLER_H
