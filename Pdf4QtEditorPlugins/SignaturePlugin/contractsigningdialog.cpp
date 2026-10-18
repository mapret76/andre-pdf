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

#include "contractsigningdialog.h"

#include <QLabel>
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPushButton>
#include <QRadioButton>
#include <QButtonGroup>
#include <QTableWidget>
#include <QDialogButtonBox>

namespace pdfplugin
{

ContractSigningDialog::ContractSigningDialog(QWidget* parent,
                                             std::vector<pdf::PDFSignatureLineFinder::SignatureLine> lines,
                                             const pdf::PDFSignatureLibrary& library,
                                             std::function<void(pdf::PDFInteger)> goToPage) :
    QDialog(parent),
    m_lines(std::move(lines))
{
    setWindowTitle(tr("Sign Contract"));

    QVBoxLayout* layout = new QVBoxLayout(this);
    QLabel* hint = new QLabel(tr("Decide every signature line: sign it yourself, or leave it for another party. "
                                 "Your signature image is placed on your lines, and one digital signature "
                                 "certifies the document. Double-click a line to see it."), this);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    m_table = new QTableWidget(int(m_lines.size()), 4, this);
    m_table->setHorizontalHeaderLabels({ tr("Page"), tr("Line"), tr("I sign"), tr("Other party") });
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->setEditTriggers(QTableWidget::NoEditTriggers);
    m_table->setSelectionBehavior(QTableWidget::SelectRows);
    m_table->verticalHeader()->setVisible(false);

    for (int row = 0; row < int(m_lines.size()); ++row)
    {
        const pdf::PDFSignatureLineFinder::SignatureLine& line = m_lines[row];
        QString source;
        switch (line.source)
        {
            case pdf::PDFSignatureLineFinder::Source::SignatureField:
                source = tr("signature field %1").arg(line.fieldName);
                break;
            case pdf::PDFSignatureLineFinder::Source::Line:
                source = line.label;
                break;
            case pdf::PDFSignatureLineFinder::Source::Label:
                source = tr("%1 (no line)").arg(line.label);
                break;
        }

        m_table->setItem(row, 0, new QTableWidgetItem(QString::number(line.pageIndex + 1)));
        m_table->setItem(row, 1, new QTableWidgetItem(source));

        QButtonGroup* group = new QButtonGroup(this);
        QRadioButton* sign = new QRadioButton(this);
        QRadioButton* other = new QRadioButton(this);
        group->addButton(sign, int(Decision::Sign));
        group->addButton(other, int(Decision::OtherParty));
        m_table->setCellWidget(row, 2, sign);
        m_table->setCellWidget(row, 3, other);
        connect(group, &QButtonGroup::idToggled, this, &ContractSigningDialog::updateStatus);
        m_groups.push_back(group);
    }

    if (goToPage)
    {
        connect(m_table, &QTableWidget::cellDoubleClicked, this, [this, goToPage](int row, int) { goToPage(m_lines[row].pageIndex); });
    }
    layout->addWidget(m_table);

    QHBoxLayout* bottom = new QHBoxLayout();
    bottom->addWidget(new QLabel(tr("Signature"), this));
    m_slotCombo = new QComboBox(this);
    for (int i = 0; i < pdf::PDFSignatureLibrary::SLOT_COUNT; ++i)
    {
        if (!library.getSlot(i).isEmpty())
        {
            m_slotCombo->addItem(library.getDisplayName(i), i);
        }
    }
    bottom->addWidget(m_slotCombo, 1);
    QPushButton* allMine = new QPushButton(tr("I sign all"), this);
    connect(allMine, &QPushButton::clicked, this, [this]() { for (size_t i = 0; i < m_lines.size(); ++i) { setDecision(i, Decision::Sign); } });
    bottom->addWidget(allMine);
    layout->addLayout(bottom);

    m_statusLabel = new QLabel(this);
    layout->addWidget(m_statusLabel);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    m_signButton = buttonBox->addButton(tr("Sign && Lock..."), QDialogButtonBox::AcceptRole);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttonBox);

    resize(640, 420);
    updateStatus();
}

std::vector<ContractSigningDialog::Decision> ContractSigningDialog::getDecisions() const
{
    std::vector<Decision> decisions;
    for (const QButtonGroup* group : m_groups)
    {
        decisions.push_back(group->checkedId() < 0 ? Decision::Undecided : static_cast<Decision>(group->checkedId()));
    }
    return decisions;
}

int ContractSigningDialog::getSlotIndex() const
{
    return m_slotCombo->currentData().toInt();
}

void ContractSigningDialog::setDecision(size_t line, Decision decision)
{
    if (QAbstractButton* button = m_groups.at(line)->button(int(decision)))
    {
        button->setChecked(true);
    }
}

bool ContractSigningDialog::isComplete() const
{
    const std::vector<Decision> decisions = getDecisions();
    const bool allDecided = std::none_of(decisions.cbegin(), decisions.cend(), [](Decision decision) { return decision == Decision::Undecided; });
    const bool anySigned = std::any_of(decisions.cbegin(), decisions.cend(), [](Decision decision) { return decision == Decision::Sign; });
    return allDecided && anySigned && m_slotCombo->count() > 0;
}

void ContractSigningDialog::updateStatus()
{
    const std::vector<Decision> decisions = getDecisions();
    const qsizetype undecided = std::count(decisions.cbegin(), decisions.cend(), Decision::Undecided);
    const qsizetype mine = std::count(decisions.cbegin(), decisions.cend(), Decision::Sign);
    const qsizetype others = std::count(decisions.cbegin(), decisions.cend(), Decision::OtherParty);

    QString status;
    if (m_slotCombo->count() == 0)
    {
        status = tr("The signature library is empty - import your signature in the Signature Library first.");
    }
    else if (undecided > 0)
    {
        status = tr("%1 of %2 lines still to decide. The contract cannot be locked before every line is decided.").arg(undecided).arg(decisions.size());
    }
    else if (mine == 0)
    {
        status = tr("You sign no line.");
    }
    else if (others > 0)
    {
        status = tr("You sign %1 lines, %2 are left for the other party. The document is certified so that they can still sign.").arg(mine).arg(others);
    }
    else
    {
        status = tr("You sign all %1 lines. The document is certified: no changes allowed.").arg(mine);
    }
    m_statusLabel->setText(status);
    m_signButton->setEnabled(isComplete());
}

}   // namespace pdfplugin
