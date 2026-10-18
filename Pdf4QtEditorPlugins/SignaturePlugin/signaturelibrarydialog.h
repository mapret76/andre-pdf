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

#ifndef SIGNATURELIBRARYDIALOG_H
#define SIGNATURELIBRARYDIALOG_H

#include "pdfsignaturelibrary.h"

#include <QDialog>

#include <array>

class QLabel;
class QSlider;
class QCheckBox;
class QComboBox;
class QLineEdit;

namespace pdfplugin
{

/// Edits the signature library: a name and an image for each slot, the image
/// imported from a scan or photo of a signature on paper
class SignatureLibraryDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SignatureLibraryDialog(QWidget* parent, const pdf::PDFSignatureLibrary& library);

    pdf::PDFSignatureLibrary getLibrary() const;

private:
    void importSlot(int index);
    void clearSlot(int index);
    void updateSlot(int index);

    pdf::PDFSignatureLibrary m_library;
    std::array<QLabel*, pdf::PDFSignatureLibrary::SLOT_COUNT> m_previews = { };
    std::array<QLineEdit*, pdf::PDFSignatureLibrary::SLOT_COUNT> m_names = { };
};

/// Turns a scanned image into a signature with a transparent background, with a
/// live preview, an adjustable threshold and the colour of the ink
class SignatureImportDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SignatureImportDialog(QWidget* parent, QImage scan);

    /// Returns the extracted signature
    QImage getSignature() const { return m_signature; }

private:
    void updatePreview();

    QImage m_scan;
    QImage m_signature;
    QLabel* m_preview = nullptr;
    QSlider* m_thresholdSlider = nullptr;
    QCheckBox* m_automaticCheckBox = nullptr;
    QComboBox* m_inkCombo = nullptr;
};

}   // namespace pdfplugin

#endif // SIGNATURELIBRARYDIALOG_H
