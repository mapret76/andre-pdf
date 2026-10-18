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

#ifndef PACKSIGNINGDIALOG_H
#define PACKSIGNINGDIALOG_H

#include "pdfcontractpack.h"
#include "pdfsignaturelibrary.h"

#include <QDialog>

class QLabel;
class QCheckBox;
class QComboBox;
class QLineEdit;
class QTableWidget;
class QRadioButton;
class QPushButton;

namespace pdfplugin
{

/// Splits a contract pack into its contracts and sets up how each of them is
/// initialled and signed in one batch
class PackSigningDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PackSigningDialog(QWidget* parent, const pdf::PDFDocument* document, const pdf::PDFSignatureLibrary& library, const QString& fileName);

    virtual void accept() override;

    pdf::PDFContractPack::Parts getParts() const;
    QString getOutputDirectory() const;
    QString getBaseName() const;

    /// Slot of the initials, -1 when the pages are not initialled
    int getInitialsSlot() const;
    bool isLastPageSkippedByInitials() const;
    int getSignatureSlot() const;

    /// Text, which a label of a line must contain to be signed by the user; empty
    /// when every line is signed by the user
    QString getLineFilter() const;

    /// Sets the parts shown in the table
    void setParts(const pdf::PDFContractPack::Parts& parts);

private:
    void applyRanges();

    const pdf::PDFDocument* m_document;
    QTableWidget* m_partsTable = nullptr;
    QLineEdit* m_rangesEdit = nullptr;
    QLineEdit* m_directoryEdit = nullptr;
    QLineEdit* m_baseNameEdit = nullptr;
    QCheckBox* m_initialsCheckBox = nullptr;
    QComboBox* m_initialsCombo = nullptr;
    QCheckBox* m_skipLastCheckBox = nullptr;
    QComboBox* m_signatureCombo = nullptr;
    QRadioButton* m_allLinesRadio = nullptr;
    QRadioButton* m_filteredLinesRadio = nullptr;
    QLineEdit* m_lineFilterEdit = nullptr;
};

}   // namespace pdfplugin

#endif // PACKSIGNINGDIALOG_H
