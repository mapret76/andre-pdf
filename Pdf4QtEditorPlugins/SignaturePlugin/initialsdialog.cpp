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

#include "initialsdialog.h"
#include "pdfutils.h"

#include <QLabel>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QSettings>
#include <QFormLayout>
#include <QMessageBox>
#include <QDoubleSpinBox>
#include <QDialogButtonBox>

namespace pdfplugin
{

static constexpr qreal POINTS_PER_MILLIMETER = 72.0 / 25.4;
static constexpr const char* INITIALS_SETTINGS_GROUP = "SignaturePlugin";

InitialsDialog::InitialsDialog(QWidget* parent, const pdf::PDFSignatureLibrary& library, pdf::PDFInteger pageCount) :
    QDialog(parent),
    m_pageCount(pageCount)
{
    setWindowTitle(tr("Initial All Pages"));

    QFormLayout* layout = new QFormLayout(this);

    m_slotCombo = new QComboBox(this);
    for (int i = 0; i < pdf::PDFSignatureLibrary::SLOT_COUNT; ++i)
    {
        if (!library.getSlot(i).isEmpty())
        {
            m_slotCombo->addItem(library.getDisplayName(i), i);
        }
    }

    m_positionCombo = new QComboBox(this);
    m_positionCombo->addItem(tr("Bottom right"), int(pdf::PDFPageInitials::Position::BottomRight));
    m_positionCombo->addItem(tr("Bottom centre"), int(pdf::PDFPageInitials::Position::BottomCenter));
    m_positionCombo->addItem(tr("Bottom left"), int(pdf::PDFPageInitials::Position::BottomLeft));
    m_positionCombo->addItem(tr("Top right"), int(pdf::PDFPageInitials::Position::TopRight));
    m_positionCombo->addItem(tr("Top left"), int(pdf::PDFPageInitials::Position::TopLeft));

    m_widthSpin = new QDoubleSpinBox(this);
    m_widthSpin->setRange(5.0, 100.0);
    m_widthSpin->setSuffix(tr(" mm"));
    m_marginSpin = new QDoubleSpinBox(this);
    m_marginSpin->setRange(0.0, 50.0);
    m_marginSpin->setSuffix(tr(" mm"));

    m_skipLastCheckBox = new QCheckBox(tr("Skip the last page (it carries the full signature)"), this);
    m_skipPagesEdit = new QLineEdit(this);
    m_skipPagesEdit->setPlaceholderText(tr("for example 1, 3-4"));

    layout->addRow(tr("Initials"), m_slotCombo);
    layout->addRow(tr("Position"), m_positionCombo);
    layout->addRow(tr("Width"), m_widthSpin);
    layout->addRow(tr("Distance from the edges"), m_marginSpin);
    layout->addRow(QString(), m_skipLastCheckBox);
    layout->addRow(tr("Also skip pages"), m_skipPagesEdit);
    layout->addRow(new QLabel(tr("Initial the pages before signing: a change after the first signature breaks it."), this));

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &InitialsDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addRow(buttonBox);

    QSettings settings;
    settings.beginGroup(INITIALS_SETTINGS_GROUP);
    const pdf::PDFPageInitials::Settings defaults;
    m_slotCombo->setCurrentIndex(qMax(0, m_slotCombo->findData(settings.value("InitialsSlot", pdf::PDFSignatureLibrary::SLOT_COUNT - 1).toInt())));
    m_positionCombo->setCurrentIndex(qMax(0, m_positionCombo->findData(settings.value("InitialsPosition", int(defaults.position)).toInt())));
    m_widthSpin->setValue(settings.value("InitialsWidth", defaults.width / POINTS_PER_MILLIMETER).toDouble());
    m_marginSpin->setValue(settings.value("InitialsMargin", defaults.margin / POINTS_PER_MILLIMETER).toDouble());
    m_skipLastCheckBox->setChecked(settings.value("InitialsSkipLast", true).toBool());
    settings.endGroup();
}

int InitialsDialog::getSlotIndex() const
{
    return m_slotCombo->currentData().toInt();
}

void InitialsDialog::accept()
{
    if (m_slotCombo->count() == 0)
    {
        QMessageBox::critical(this, tr("Error"), tr("The signature library is empty. Import your initials in the Signature Library first."));
        return;
    }

    pdf::PDFPageInitials::Settings settings;
    settings.position = static_cast<pdf::PDFPageInitials::Position>(m_positionCombo->currentData().toInt());
    settings.width = m_widthSpin->value() * POINTS_PER_MILLIMETER;
    settings.margin = m_marginSpin->value() * POINTS_PER_MILLIMETER;

    if (!m_skipPagesEdit->text().trimmed().isEmpty())
    {
        QString errorMessage;
        const pdf::PDFClosedIntervalSet pages = pdf::PDFClosedIntervalSet::parse(1, m_pageCount, m_skipPagesEdit->text(), &errorMessage);
        if (!errorMessage.isEmpty())
        {
            QMessageBox::critical(this, tr("Error"), errorMessage);
            m_skipPagesEdit->setFocus();
            return;
        }
        for (pdf::PDFInteger page : pages.unfold())
        {
            if (page < 1 || page > m_pageCount)
            {
                QMessageBox::critical(this, tr("Error"), tr("Page %1 does not exist, the document has %2 pages.").arg(page).arg(m_pageCount));
                m_skipPagesEdit->setFocus();
                return;
            }
            settings.skippedPages.insert(page - 1);
        }
    }
    if (m_skipLastCheckBox->isChecked() && m_pageCount > 1)
    {
        settings.skippedPages.insert(m_pageCount - 1);
    }
    m_settings = std::move(settings);

    QSettings storedSettings;
    storedSettings.beginGroup(INITIALS_SETTINGS_GROUP);
    storedSettings.setValue("InitialsSlot", getSlotIndex());
    storedSettings.setValue("InitialsPosition", int(m_settings.position));
    storedSettings.setValue("InitialsWidth", m_widthSpin->value());
    storedSettings.setValue("InitialsMargin", m_marginSpin->value());
    storedSettings.setValue("InitialsSkipLast", m_skipLastCheckBox->isChecked());
    storedSettings.endGroup();

    QDialog::accept();
}

}   // namespace pdfplugin
