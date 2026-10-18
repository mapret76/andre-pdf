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

#ifndef INITIALSDIALOG_H
#define INITIALSDIALOG_H

#include "pdfpageinitials.h"
#include "pdfsignaturelibrary.h"

#include <QDialog>

class QComboBox;
class QCheckBox;
class QLineEdit;
class QDoubleSpinBox;

namespace pdfplugin
{

/// Settings of the initials on every page: which slot of the signature library,
/// where on the page, how large, and which pages are skipped
class InitialsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit InitialsDialog(QWidget* parent, const pdf::PDFSignatureLibrary& library, pdf::PDFInteger pageCount);

    virtual void accept() override;

    int getSlotIndex() const;
    const pdf::PDFPageInitials::Settings& getSettings() const { return m_settings; }

private:
    pdf::PDFInteger m_pageCount;
    pdf::PDFPageInitials::Settings m_settings;
    QComboBox* m_slotCombo = nullptr;
    QComboBox* m_positionCombo = nullptr;
    QDoubleSpinBox* m_widthSpin = nullptr;
    QDoubleSpinBox* m_marginSpin = nullptr;
    QCheckBox* m_skipLastCheckBox = nullptr;
    QLineEdit* m_skipPagesEdit = nullptr;
};

}   // namespace pdfplugin

#endif // INITIALSDIALOG_H
