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

#include "signaturelibrarydialog.h"

#include <QLabel>
#include <QSlider>
#include <QPainter>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QGroupBox>
#include <QImageReader>
#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardPaths>
#include <QDialogButtonBox>

namespace pdfplugin
{

// Pixmap of the signature on a light checkerboard, which shows the transparent background
class SignatureLibraryDialogHelper
{
public:
    SignatureLibraryDialogHelper() = delete;

    static QPixmap createPreview(const QImage& signature, QSize size)
    {
        QPixmap pixmap(size);
        pixmap.fill(Qt::white);
        QPainter painter(&pixmap);
        constexpr int cell = 8;
        for (int y = 0; y < size.height(); y += cell)
        {
            for (int x = 0; x < size.width(); x += cell)
            {
                if (((x + y) / cell) % 2)
                {
                    painter.fillRect(x, y, cell, cell, QColor(235, 235, 235));
                }
            }
        }

        if (!signature.isNull())
        {
            const QImage scaled = signature.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            painter.drawImage((size.width() - scaled.width()) / 2, (size.height() - scaled.height()) / 2, scaled);
        }
        return pixmap;
    }
};

static constexpr QSize SLOT_PREVIEW_SIZE(240, 80);
static constexpr QSize IMPORT_PREVIEW_SIZE(480, 200);

SignatureLibraryDialog::SignatureLibraryDialog(QWidget* parent, const pdf::PDFSignatureLibrary& library) :
    QDialog(parent),
    m_library(library)
{
    setWindowTitle(tr("Signature Library"));

    QVBoxLayout* layout = new QVBoxLayout(this);
    QLabel* hint = new QLabel(tr("Sign a white sheet of paper, scan or photograph it and import the image. "
                                 "The paper is removed, only the ink is kept. The library is stored in your "
                                 "user profile, encrypted for your Windows account."), this);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    QGridLayout* grid = new QGridLayout();
    for (int i = 0; i < pdf::PDFSignatureLibrary::SLOT_COUNT; ++i)
    {
        m_previews[i] = new QLabel(this);
        m_previews[i]->setFixedSize(SLOT_PREVIEW_SIZE);
        m_previews[i]->setFrameShape(QFrame::StyledPanel);

        m_names[i] = new QLineEdit(m_library.getSlot(i).name, this);
        m_names[i]->setPlaceholderText(tr("Signature %1").arg(i + 1));

        QPushButton* importButton = new QPushButton(tr("Import from Image..."), this);
        QPushButton* clearButton = new QPushButton(tr("Clear"), this);
        connect(importButton, &QPushButton::clicked, this, [this, i]() { importSlot(i); });
        connect(clearButton, &QPushButton::clicked, this, [this, i]() { clearSlot(i); });

        QVBoxLayout* controls = new QVBoxLayout();
        controls->addWidget(m_names[i]);
        QHBoxLayout* buttons = new QHBoxLayout();
        buttons->addWidget(importButton);
        buttons->addWidget(clearButton);
        controls->addLayout(buttons);

        grid->addWidget(new QLabel(QString::number(i + 1), this), i, 0);
        grid->addWidget(m_previews[i], i, 1);
        grid->addLayout(controls, i, 2);
        updateSlot(i);
    }
    layout->addLayout(grid);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttonBox);
}

pdf::PDFSignatureLibrary SignatureLibraryDialog::getLibrary() const
{
    pdf::PDFSignatureLibrary library = m_library;
    for (int i = 0; i < pdf::PDFSignatureLibrary::SLOT_COUNT; ++i)
    {
        pdf::PDFSignatureLibrary::Slot slot = library.getSlot(i);
        slot.name = m_names[i]->text().trimmed();
        library.setSlot(i, std::move(slot));
    }
    return library;
}

void SignatureLibraryDialog::importSlot(int index)
{
    QStringList suffixes;
    for (const QByteArray& format : QImageReader::supportedImageFormats())
    {
        suffixes << QString("*.%1").arg(QString::fromLatin1(format).toLower());
    }
    const QStringList pictures = QStandardPaths::standardLocations(QStandardPaths::PicturesLocation);
    const QString fileName = QFileDialog::getOpenFileName(this, tr("Import Signature"), pictures.isEmpty() ? QString() : pictures.constFirst(),
                                                          tr("Images (%1)").arg(suffixes.join(' ')));
    if (fileName.isEmpty())
    {
        return;
    }

    QImageReader reader(fileName);
    reader.setAutoTransform(true); // phone photos carry their rotation in EXIF
    const QImage scan = reader.read();
    if (scan.isNull())
    {
        QMessageBox::critical(this, tr("Error"), tr("The image cannot be read: %1").arg(reader.errorString()));
        return;
    }

    SignatureImportDialog dialog(this, scan);
    if (dialog.exec() == QDialog::Accepted && !dialog.getSignature().isNull())
    {
        pdf::PDFSignatureLibrary::Slot slot = m_library.getSlot(index);
        slot.png = pdf::PDFSignatureLibrary::toPng(dialog.getSignature());
        m_library.setSlot(index, std::move(slot));
        updateSlot(index);
    }
}

void SignatureLibraryDialog::clearSlot(int index)
{
    m_library.setSlot(index, pdf::PDFSignatureLibrary::Slot());
    m_names[index]->clear();
    updateSlot(index);
}

void SignatureLibraryDialog::updateSlot(int index)
{
    QImage image;
    image.loadFromData(m_library.getSlot(index).png, "PNG");
    m_previews[index]->setPixmap(SignatureLibraryDialogHelper::createPreview(image, SLOT_PREVIEW_SIZE));
    m_previews[index]->setToolTip(image.isNull() ? tr("Empty slot") : tr("%1 x %2 pixels").arg(image.width()).arg(image.height()));
}

SignatureImportDialog::SignatureImportDialog(QWidget* parent, QImage scan) :
    QDialog(parent),
    m_scan(std::move(scan))
{
    setWindowTitle(tr("Import Signature"));

    QVBoxLayout* layout = new QVBoxLayout(this);
    m_preview = new QLabel(this);
    m_preview->setFixedSize(IMPORT_PREVIEW_SIZE);
    m_preview->setFrameShape(QFrame::StyledPanel);
    layout->addWidget(m_preview);

    QGridLayout* grid = new QGridLayout();
    m_automaticCheckBox = new QCheckBox(tr("Automatic"), this);
    m_automaticCheckBox->setChecked(true);
    m_thresholdSlider = new QSlider(Qt::Horizontal, this);
    m_thresholdSlider->setRange(1, 254);
    m_thresholdSlider->setValue(pdf::PDFSignatureLibrary::getAutomaticThreshold(m_scan.convertToFormat(QImage::Format_Grayscale8)));
    m_thresholdSlider->setToolTip(tr("Move right to keep fainter strokes, left to remove more of the paper"));
    m_inkCombo = new QComboBox(this);
    m_inkCombo->addItem(tr("Colour of the scan"), QColor());
    m_inkCombo->addItem(tr("Black"), QColor(Qt::black));
    m_inkCombo->addItem(tr("Dark blue"), QColor(0, 32, 128));

    grid->addWidget(new QLabel(tr("Ink threshold"), this), 0, 0);
    grid->addWidget(m_thresholdSlider, 0, 1);
    grid->addWidget(m_automaticCheckBox, 0, 2);
    grid->addWidget(new QLabel(tr("Ink colour"), this), 1, 0);
    grid->addWidget(m_inkCombo, 1, 1, 1, 2);
    layout->addLayout(grid);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttonBox);

    connect(m_thresholdSlider, &QSlider::valueChanged, this, [this]() { m_automaticCheckBox->setChecked(false); updatePreview(); });
    connect(m_automaticCheckBox, &QCheckBox::toggled, this, &SignatureImportDialog::updatePreview);
    connect(m_inkCombo, &QComboBox::currentIndexChanged, this, &SignatureImportDialog::updatePreview);
    updatePreview();
}

void SignatureImportDialog::updatePreview()
{
    const int threshold = m_automaticCheckBox->isChecked() ? -1 : m_thresholdSlider->value();
    m_signature = pdf::PDFSignatureLibrary::extractSignature(m_scan, threshold, m_inkCombo->currentData().value<QColor>());
    m_preview->setPixmap(SignatureLibraryDialogHelper::createPreview(m_signature, IMPORT_PREVIEW_SIZE));
}

}   // namespace pdfplugin
