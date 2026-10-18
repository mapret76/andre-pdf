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

#ifndef PDFSIZEREDUCER_H
#define PDFSIZEREDUCER_H

#include "pdfglobal.h"
#include "pdfdocument.h"

#include <QCoreApplication>

namespace pdf
{
class PDFProgress;

/// One-step reduction of the file size for a purpose: the images are resampled
/// and recompressed for it, then the objects are optimized. It rewrites the whole
/// document, so it is done before signing - after the first signature it breaks it.
class PDF4QTLIBCORESHARED_EXPORT PDFSizeReducer
{
    Q_DECLARE_TR_FUNCTIONS(pdf::PDFSizeReducer)

public:
    PDFSizeReducer() = delete;

    enum class Preset
    {
        Email,      ///< Screen and e-mail: 150 dpi, JPEG quality 75
        Smallest,   ///< As small as still readable: 110 dpi, JPEG quality 60
        Print       ///< Printing: 300 dpi, JPEG quality 85, quality first
    };

    /// Returns the name of the preset shown to the user
    static QString getPresetName(Preset preset);

    /// Reduces the document for the preset
    static PDFDocument reduce(const PDFDocument* document, Preset preset, PDFProgress* progress = nullptr);

    /// Returns the size of the written document in bytes, -1 when it cannot be written
    static qint64 getWrittenSize(const PDFDocument* document);
};

}   // namespace pdf

#endif // PDFSIZEREDUCER_H
