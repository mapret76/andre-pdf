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

#ifndef PDFSIGNATURELIBRARY_H
#define PDFSIGNATURELIBRARY_H

#include "pdfglobal.h"

#include <QColor>
#include <QImage>
#include <QString>
#include <QByteArray>
#include <QCoreApplication>

#include <array>
#include <optional>

namespace pdf
{

/// The user's own handwritten signatures and initials, scanned from paper and
/// kept for reuse: a fixed number of named slots, each holding a PNG image with
/// a transparent background. The library is stored in the user's profile; on
/// Windows it is encrypted for the user account by DPAPI, so another account or
/// a copied file cannot use it.
class PDF4QTLIBCORESHARED_EXPORT PDFSignatureLibrary
{
    Q_DECLARE_TR_FUNCTIONS(pdf::PDFSignatureLibrary)

public:
    static constexpr int SLOT_COUNT = 4;

    struct Slot
    {
        QString name;
        QByteArray png;     ///< PNG with a transparent background, empty for an empty slot

        bool isEmpty() const { return png.isEmpty(); }
        bool operator==(const Slot&) const = default;
    };

    const Slot& getSlot(int index) const { return m_slots.at(index); }
    void setSlot(int index, Slot slot) { m_slots.at(index) = std::move(slot); }

    /// Returns the name of the slot shown to the user, a default name for an unnamed slot
    QString getDisplayName(int index) const;

    /// Serializes the library (not protected)
    QByteArray serialize() const;

    /// Restores the library from serialized data, returns false for invalid data
    bool deserialize(const QByteArray& data);

    /// Loads the library from a file, an absent file gives an empty library
    /// \param fileName File name
    /// \param errorMessage Error message, when false is returned
    bool load(const QString& fileName, QString* errorMessage = nullptr);

    /// Saves the library into a file readable only by the user
    /// \param fileName File name
    /// \param errorMessage Error message, when false is returned
    bool save(const QString& fileName, QString* errorMessage = nullptr) const;

    /// Returns the file of the library in the user's profile
    static QString getDefaultFileName();

    /// Encrypts data for the current user account (DPAPI on Windows). On the other
    /// systems the data are only marked, the file permissions protect them.
    static QByteArray protect(const QByteArray& data);

    /// Reverses protect(), returns nothing when the data cannot be decrypted
    static std::optional<QByteArray> unprotect(const QByteArray& data);

    /// Extracts a signature from a scan or a photo of paper: the paper becomes
    /// transparent, the ink keeps its colour (or gets the given one), and the image
    /// is cropped to the ink and limited in size.
    /// \param scan Scanned image
    /// \param threshold Gray level (0-255) separating ink from paper, automatic when negative
    /// \param inkColor Colour of the ink, the colour of the scan is kept when invalid
    static QImage extractSignature(const QImage& scan, int threshold = -1, const QColor& inkColor = QColor());

    /// Returns the gray level separating ink from paper by Otsu's method
    static int getAutomaticThreshold(const QImage& grayscale);

    /// Encodes an image as PNG
    static QByteArray toPng(const QImage& image);

private:
    std::array<Slot, SLOT_COUNT> m_slots;
};

}   // namespace pdf

#endif // PDFSIGNATURELIBRARY_H
