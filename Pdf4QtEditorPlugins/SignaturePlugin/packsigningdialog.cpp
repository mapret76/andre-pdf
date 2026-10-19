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

#include "packsigningdialog.h"

#include <QDir>
#include <QApplication>
#include <QLabel>
#include <QCheckBox>
#include <QComboBox>
#include <QFileInfo>
#include <QLineEdit>
#include <QGroupBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QTableWidget>
#include <QDialogButtonBox>

namespace pdfplugin
{

PackSigningDialog::PackSigningDialog(QWidget* parent, const pdf::PDFDocument* document, const pdf::PDFSignatureLibrary& library, const QString& fileName) :
    QDialog(parent),
    m_document(document)
{
    setWindowTitle(tr("Split and Sign Contract Pack"));
    QVBoxLayout* layout = new QVBoxLayout(this);

    // Parts of the pack
    QGroupBox* partsBox = new QGroupBox(tr("Contracts in the pack"), this);
    QVBoxLayout* partsLayout = new QVBoxLayout(partsBox);
    m_partsTable = new QTableWidget(0, 2, partsBox);
    m_partsTable->setHorizontalHeaderLabels({ tr("Name"), tr("Pages") });
    m_partsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_partsTable->verticalHeader()->setVisible(false);
    partsLayout->addWidget(m_partsTable);

    QHBoxLayout* rangesLayout = new QHBoxLayout();
    m_rangesEdit = new QLineEdit(partsBox);
    m_rangesEdit->setPlaceholderText(tr("Page ranges, for example 1-5, 6-9"));
    QPushButton* applyButton = new QPushButton(tr("Apply"), partsBox);
    QPushButton* outlineButton = new QPushButton(tr("From Bookmarks"), partsBox);
    QPushButton* numbersButton = new QPushButton(tr("From Page Numbers"), partsBox);
    rangesLayout->addWidget(m_rangesEdit, 1);
    rangesLayout->addWidget(applyButton);
    rangesLayout->addWidget(outlineButton);
    rangesLayout->addWidget(numbersButton);
    partsLayout->addLayout(rangesLayout);
    layout->addWidget(partsBox);

    connect(applyButton, &QPushButton::clicked, this, &PackSigningDialog::applyRanges);
    connect(m_rangesEdit, &QLineEdit::returnPressed, this, &PackSigningDialog::applyRanges);
    connect(outlineButton, &QPushButton::clicked, this, [this]()
    {
        const pdf::PDFContractPack::Parts parts = pdf::PDFContractPack::suggestFromOutline(m_document);
        if (parts.empty())
        {
            QMessageBox::information(this, windowTitle(), tr("The document has no bookmarks marking its contracts."));
            return;
        }
        setParts(parts);
    });
    connect(numbersButton, &QPushButton::clicked, this, [this]()
    {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        const pdf::PDFContractPack::Parts parts = pdf::PDFContractPack::suggestFromPageNumbers(m_document);
        QApplication::restoreOverrideCursor();
        if (parts.empty())
        {
            QMessageBox::information(this, windowTitle(), tr("No page numbering starting again at \"Page 1 of ...\" was found."));
            return;
        }
        setParts(parts);
    });

    // Signing
    QGroupBox* signingBox = new QGroupBox(tr("Signing"), this);
    QFormLayout* signingLayout = new QFormLayout(signingBox);
    m_signatureCombo = new QComboBox(signingBox);
    m_initialsCombo = new QComboBox(signingBox);
    for (int i = 0; i < pdf::PDFSignatureLibrary::SLOT_COUNT; ++i)
    {
        if (!library.getSlot(i).isEmpty())
        {
            m_signatureCombo->addItem(library.getDisplayName(i), i);
            m_initialsCombo->addItem(library.getDisplayName(i), i);
        }
    }
    m_initialsCombo->setCurrentIndex(m_initialsCombo->count() - 1);
    m_initialsCheckBox = new QCheckBox(tr("Initial every page with"), signingBox);
    m_initialsCheckBox->setChecked(true);
    m_skipLastCheckBox = new QCheckBox(tr("not the last page of each contract"), signingBox);
    m_skipLastCheckBox->setChecked(true);
    QHBoxLayout* initialsLayout = new QHBoxLayout();
    initialsLayout->addWidget(m_initialsCombo, 1);
    initialsLayout->addWidget(m_skipLastCheckBox);
    signingLayout->addRow(m_initialsCheckBox, initialsLayout);
    signingLayout->addRow(tr("Signature"), m_signatureCombo);

    m_allLinesRadio = new QRadioButton(tr("Sign every signature line"), signingBox);
    m_filteredLinesRadio = new QRadioButton(tr("Sign only lines labelled with"), signingBox);
    m_lineFilterEdit = new QLineEdit(signingBox);
    m_lineFilterEdit->setPlaceholderText(tr("for example Seller, or your company name"));
    // No choice preselected: signing every line would put the signature on the lines of
    // the other parties and of the witnesses too
    signingLayout->addRow(m_allLinesRadio);
    signingLayout->addRow(m_filteredLinesRadio, m_lineFilterEdit);
    signingLayout->addRow(new QLabel(tr("Lines left for another party keep the contract open for their signature (certified: form filling and signing allowed)."), signingBox));
    layout->addWidget(signingBox);

    // Output
    QGroupBox* outputBox = new QGroupBox(tr("Signed files"), this);
    QFormLayout* outputLayout = new QFormLayout(outputBox);
    const QFileInfo fileInfo(fileName);
    m_directoryEdit = new QLineEdit(QDir::toNativeSeparators(fileInfo.absolutePath()), outputBox);
    QPushButton* browseButton = new QPushButton(tr("Browse..."), outputBox);
    QHBoxLayout* directoryLayout = new QHBoxLayout();
    directoryLayout->addWidget(m_directoryEdit, 1);
    directoryLayout->addWidget(browseButton);
    connect(browseButton, &QPushButton::clicked, this, [this]()
    {
        const QString directory = QFileDialog::getExistingDirectory(this, tr("Folder of the Signed Files"), m_directoryEdit->text());
        if (!directory.isEmpty())
        {
            m_directoryEdit->setText(QDir::toNativeSeparators(directory));
        }
    });
    m_baseNameEdit = new QLineEdit(fileInfo.completeBaseName() + tr(" SIGNED"), outputBox);
    outputLayout->addRow(tr("Folder"), directoryLayout);
    outputLayout->addRow(tr("Name"), m_baseNameEdit);
    layout->addWidget(outputBox);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    buttonBox->addButton(tr("Sign All..."), QDialogButtonBox::AcceptRole);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &PackSigningDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttonBox);

    // Bookmarks first, a single part (the whole document) otherwise; the page
    // numbers are searched on demand, it reads the text of every page
    pdf::PDFContractPack::Parts parts = pdf::PDFContractPack::suggestFromOutline(document);
    if (parts.empty())
    {
        parts = pdf::PDFContractPack::fromStartPages({ 0 }, pdf::PDFInteger(document->getCatalog()->getPageCount()));
    }
    setParts(parts);
    resize(680, 560);
}

void PackSigningDialog::setParts(const pdf::PDFContractPack::Parts& parts)
{
    m_partsTable->setRowCount(int(parts.size()));
    for (int row = 0; row < int(parts.size()); ++row)
    {
        m_partsTable->setItem(row, 0, new QTableWidgetItem(parts[row].name));
        QTableWidgetItem* pages = new QTableWidgetItem(pdf::PDFContractPack::toText({ parts[row] }));
        pages->setFlags(pages->flags() & ~Qt::ItemIsEditable);
        pages->setData(Qt::UserRole, QVariant::fromValue(QPoint(int(parts[row].firstPage), int(parts[row].lastPage))));
        m_partsTable->setItem(row, 1, pages);
    }
    m_rangesEdit->setText(pdf::PDFContractPack::toText(parts));
}

void PackSigningDialog::applyRanges()
{
    QString errorMessage;
    const pdf::PDFContractPack::Parts parts = pdf::PDFContractPack::parse(m_rangesEdit->text(), pdf::PDFInteger(m_document->getCatalog()->getPageCount()), &errorMessage);
    if (!errorMessage.isEmpty() || parts.empty())
    {
        QMessageBox::critical(this, tr("Error"), errorMessage.isEmpty() ? tr("Enter the page ranges of the contracts.") : errorMessage);
        return;
    }
    setParts(parts);
}

pdf::PDFContractPack::Parts PackSigningDialog::getParts() const
{
    pdf::PDFContractPack::Parts parts;
    for (int row = 0; row < m_partsTable->rowCount(); ++row)
    {
        const QPoint pages = m_partsTable->item(row, 1)->data(Qt::UserRole).toPoint();
        parts.push_back({ m_partsTable->item(row, 0)->text().trimmed(), pages.x(), pages.y() });
    }
    return parts;
}

QString PackSigningDialog::getOutputDirectory() const
{
    return QDir::fromNativeSeparators(m_directoryEdit->text().trimmed());
}

QString PackSigningDialog::getBaseName() const
{
    return m_baseNameEdit->text().trimmed();
}

int PackSigningDialog::getInitialsSlot() const
{
    return m_initialsCheckBox->isChecked() && m_initialsCombo->count() > 0 ? m_initialsCombo->currentData().toInt() : -1;
}

bool PackSigningDialog::isLastPageSkippedByInitials() const
{
    return m_skipLastCheckBox->isChecked();
}

int PackSigningDialog::getSignatureSlot() const
{
    return m_signatureCombo->currentData().toInt();
}

QString PackSigningDialog::getLineFilter() const
{
    return m_filteredLinesRadio->isChecked() ? m_lineFilterEdit->text().trimmed() : QString();
}

void PackSigningDialog::accept()
{
    if (m_signatureCombo->count() == 0)
    {
        QMessageBox::critical(this, tr("Error"), tr("The signature library is empty - import your signature in the Signature Library first."));
        return;
    }
    if (!m_allLinesRadio->isChecked() && !m_filteredLinesRadio->isChecked())
    {
        QMessageBox::critical(this, tr("Error"), tr("Choose whether you sign every signature line, or only the lines labelled with your name or company."));
        return;
    }
    if (m_filteredLinesRadio->isChecked() && getLineFilter().isEmpty())
    {
        QMessageBox::critical(this, tr("Error"), tr("Enter the text of the labels of your lines."));
        m_lineFilterEdit->setFocus();
        return;
    }
    if (!QFileInfo(getOutputDirectory()).isDir())
    {
        QMessageBox::critical(this, tr("Error"), tr("The folder of the signed files does not exist."));
        m_directoryEdit->setFocus();
        return;
    }
    if (getBaseName().isEmpty() || getParts().empty())
    {
        QMessageBox::critical(this, tr("Error"), tr("Enter the name of the signed files and the contracts of the pack."));
        return;
    }

    QDialog::accept();
}

}   // namespace pdfplugin
