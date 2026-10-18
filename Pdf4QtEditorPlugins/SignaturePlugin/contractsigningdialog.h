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

#ifndef CONTRACTSIGNINGDIALOG_H
#define CONTRACTSIGNINGDIALOG_H

#include "pdfsignaturelinefinder.h"
#include "pdfsignaturelibrary.h"

#include <QDialog>

#include <functional>
#include <vector>

class QLabel;
class QComboBox;
class QPushButton;
class QTableWidget;
class QButtonGroup;

namespace pdfplugin
{

/// Checklist of the signature lines of a contract. Every line must be decided -
/// signed by the user, or left for another party - before the contract can be
/// signed and locked, so no line is forgotten.
class ContractSigningDialog : public QDialog
{
    Q_OBJECT

public:
    enum class Decision
    {
        Undecided,
        Sign,
        OtherParty
    };

    /// \param parent Parent widget
    /// \param lines Signature lines found in the document
    /// \param library Signature library, the signature image is chosen from it
    /// \param goToPage Shows the page of a line in the document, can be empty
    explicit ContractSigningDialog(QWidget* parent,
                                   std::vector<pdf::PDFSignatureLineFinder::SignatureLine> lines,
                                   const pdf::PDFSignatureLibrary& library,
                                   std::function<void(pdf::PDFInteger)> goToPage);

    const std::vector<pdf::PDFSignatureLineFinder::SignatureLine>& getLines() const { return m_lines; }
    std::vector<Decision> getDecisions() const;
    int getSlotIndex() const;

    /// Sets the decision of a line (as the user does by the radio buttons)
    void setDecision(size_t line, Decision decision);

    /// Returns true, if every line is decided and at least one is signed by the user
    bool isComplete() const;

private:
    void updateStatus();

    std::vector<pdf::PDFSignatureLineFinder::SignatureLine> m_lines;
    std::vector<QButtonGroup*> m_groups;
    QTableWidget* m_table = nullptr;
    QComboBox* m_slotCombo = nullptr;
    QLabel* m_statusLabel = nullptr;
    QPushButton* m_signButton = nullptr;
};

}   // namespace pdfplugin

#endif // CONTRACTSIGNINGDIALOG_H
