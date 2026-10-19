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

#include "scantextdialog.h"
#include "pdffontinstaller.h"
#include "pdfscanfontbuilder.h"
#include "pdfdocument.h"
#include "pdfpage.h"

#include <QLabel>
#include <QPainter>
#include <QFontDatabase>
#include <QLineEdit>
#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QApplication>
#include <QTableWidget>
#include <QProgressDialog>
#include <QDialogButtonBox>

namespace pdfplugin
{

static constexpr double SCAN_DPI = 300.0;

ScanTextDialog::ScanTextDialog(QWidget* parent, const pdf::PDFDocument* document, pdf::PDFInteger pageIndex, QRectF area) :
    QDialog(parent),
    m_document(document),
    m_pageIndex(pageIndex),
    m_area(area)
{
    setWindowTitle(tr("Edit Scanned Text"));
    QApplication::setOverrideCursor(Qt::WaitCursor);
    m_image = pdf::PDFScanTextEditor::renderArea(document, pageIndex, area, SCAN_DPI);
    pdf::PDFFontIdentifier::getScanColors(m_image, m_paper, m_ink);
    const QString text = pdf::PDFScanTextEditor::getText(document, pageIndex, area);
    for (const pdf::PDFFontIdentifier::FontName& font : getPageFonts(document, pageIndex))
    {
        m_pageFamilies << font.family;
    }
    m_pageFamilies.removeDuplicates();
    QApplication::restoreOverrideCursor();

    QVBoxLayout* layout = new QVBoxLayout(this);
    m_scanLabel = new QLabel(this);
    m_scanLabel->setPixmap(QPixmap::fromImage(m_image.scaled(QSize(560, 120), Qt::KeepAspectRatio, Qt::SmoothTransformation)));
    layout->addWidget(m_scanLabel);

    QFormLayout* form = new QFormLayout();
    m_scanTextEdit = new QLineEdit(text, this);
    m_scanTextEdit->setPlaceholderText(tr("No OCR text here - type exactly what the scan shows"));
    form->addRow(tr("Text in the scan"), m_scanTextEdit);
    layout->addLayout(form);

    QHBoxLayout* identifyLayout = new QHBoxLayout();
    QPushButton* identifyButton = new QPushButton(tr("Identify Font"), this);
    QPushButton* onlineButton = new QPushButton(tr("Search Free Fonts Online..."), this);
    onlineButton->setToolTip(tr("Downloads common free fonts (Google Fonts and others, from Fontsource) to try them. Only the fonts are downloaded, the scan never leaves this computer."));
    m_installButton = new QPushButton(tr("Install Font..."), this);
    QPushButton* scanFontButton = new QPushButton(tr("Build Font from This Scan"), this);
    scanFontButton->setToolTip(tr("When no font is close: the letters of this page are collected, averaged and made into a font, so the edit uses the letters of the document itself."));
    identifyLayout->addWidget(identifyButton);
    identifyLayout->addWidget(onlineButton);
    identifyLayout->addWidget(scanFontButton);
    identifyLayout->addStretch();
    identifyLayout->addWidget(m_installButton);
    layout->addLayout(identifyLayout);

    m_matchTable = new QTableWidget(0, 4, this);
    m_matchTable->setHorizontalHeaderLabels({ tr("Font"), tr("Style"), tr("Likeness"), tr("Status") });
    m_matchTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_matchTable->setSelectionBehavior(QTableWidget::SelectRows);
    m_matchTable->setSelectionMode(QTableWidget::SingleSelection);
    m_matchTable->setEditTriggers(QTableWidget::NoEditTriggers);
    m_matchTable->verticalHeader()->setVisible(false);
    m_matchTable->setMinimumHeight(150);
    layout->addWidget(m_matchTable);

    QFormLayout* newForm = new QFormLayout();
    m_newTextEdit = new QLineEdit(text, this);
    newForm->addRow(tr("New text"), m_newTextEdit);
    layout->addLayout(newForm);
    m_previewLabel = new QLabel(this);
    m_previewLabel->setMinimumHeight(60);
    layout->addWidget(m_previewLabel);
    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    layout->addWidget(m_statusLabel);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    buttonBox->addButton(tr("Replace Text"), QDialogButtonBox::AcceptRole);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &ScanTextDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttonBox);

    connect(identifyButton, &QPushButton::clicked, this, [this]() { identify(); });
    connect(onlineButton, &QPushButton::clicked, this, &ScanTextDialog::searchOnline);
    connect(scanFontButton, &QPushButton::clicked, this, &ScanTextDialog::buildScanFont);
    connect(m_installButton, &QPushButton::clicked, this, &ScanTextDialog::installSelected);
    connect(m_matchTable, &QTableWidget::itemSelectionChanged, this, &ScanTextDialog::updatePreview);
    connect(m_newTextEdit, &QLineEdit::textChanged, this, &ScanTextDialog::updatePreview);

    resize(640, 560);
    if (!text.isEmpty())
    {
        identify();
    }
    updatePreview();
}

std::vector<pdf::PDFFontIdentifier::FontName> ScanTextDialog::getPageFonts(const pdf::PDFDocument* document, pdf::PDFInteger pageIndex)
{
    std::vector<pdf::PDFFontIdentifier::FontName> fonts;
    const pdf::PDFPage* page = document->getCatalog()->getPage(pageIndex);
    if (!page)
    {
        return fonts;
    }

    const pdf::PDFDictionary* resources = document->getDictionaryFromObject(page->getResources());
    const pdf::PDFDictionary* fontDictionary = resources ? document->getDictionaryFromObject(resources->get("Font")) : nullptr;
    for (size_t i = 0; fontDictionary && i < fontDictionary->getCount(); ++i)
    {
        const pdf::PDFDictionary* font = document->getDictionaryFromObject(fontDictionary->getValue(i));
        const pdf::PDFObject& baseFont = font ? document->getObject(font->get("BaseFont")) : pdf::PDFObject();
        if (baseFont.isName())
        {
            const pdf::PDFFontIdentifier::FontName name = pdf::PDFFontIdentifier::parsePdfFontName(baseFont.getString());
            // The glyphless font of the OCR layer is no candidate
            if (!name.family.isEmpty() && !name.family.contains(QStringLiteral("Glyph"), Qt::CaseInsensitive))
            {
                fonts.push_back(name);
            }
        }
    }
    return fonts;
}

void ScanTextDialog::identify(QStringList extraFamilies)
{
    const QString text = m_scanTextEdit->text().simplified();
    if (text.isEmpty())
    {
        m_statusLabel->setText(tr("Type the text of the scan first, the font is identified by it."));
        return;
    }

    QStringList families = pdf::PDFFontIdentifier::getLatinFamilies();
    for (const QString& family : m_pageFamilies)
    {
        if (pdf::PDFFontIdentifier::isInstalled(family))
        {
            families << family;
        }
    }
    families << extraFamilies << QStringList(m_trialFamilies.cbegin(), m_trialFamilies.cend()) << m_scanFonts.keys();
    families.removeDuplicates();

    QApplication::setOverrideCursor(Qt::WaitCursor);
    m_matches = pdf::PDFFontIdentifier::matchScan(m_image, text, families, 8);
    QApplication::restoreOverrideCursor();

    m_matchTable->setRowCount(0);
    int row = 0;
    // The fonts named by the page come first, when they are missing on this computer
    for (const QString& family : m_pageFamilies)
    {
        if (!pdf::PDFFontIdentifier::isInstalled(family))
        {
            m_matchTable->insertRow(row);
            m_matchTable->setItem(row, 0, new QTableWidgetItem(family));
            m_matchTable->setItem(row, 1, new QTableWidgetItem(QString()));
            m_matchTable->setItem(row, 2, new QTableWidgetItem(tr("named by the PDF")));
            m_matchTable->setItem(row, 3, new QTableWidgetItem(tr("not installed")));
            ++row;
        }
    }
    for (const pdf::PDFFontIdentifier::Match& match : m_matches)
    {
        QString style = match.bold ? (match.italic ? tr("Bold Italic") : tr("Bold")) : (match.italic ? tr("Italic") : tr("Regular"));
        QString status = m_scanFonts.contains(match.family) ? tr("from this scan (%1 letters)").arg(m_scanFonts.value(match.family).size())
                                                           : (m_trialFamilies.contains(match.family) ? tr("free download, not installed") : tr("installed"));
        m_matchTable->insertRow(row);
        m_matchTable->setItem(row, 0, new QTableWidgetItem(match.family));
        m_matchTable->setItem(row, 1, new QTableWidgetItem(style));
        m_matchTable->setItem(row, 2, new QTableWidgetItem(QString("%1%").arg(qRound(match.score * 100))));
        m_matchTable->setItem(row, 3, new QTableWidgetItem(status));
        m_matchTable->item(row, 0)->setData(Qt::UserRole, int(&match - m_matches.data()));
        ++row;
    }

    if (!m_matches.empty())
    {
        m_matchTable->selectRow(row - int(m_matches.size()));
    }
    if (!m_matches.empty() && m_matches.front().score < 0.6)
    {
        m_statusLabel->setText(tr("No installed font is a close match. Search the free fonts online, or install the font of the document if you know it."));
    }
    else
    {
        m_statusLabel->clear();
    }
    updatePreview();
}

void ScanTextDialog::searchOnline()
{
    const QStringList candidates = pdf::PDFFontInstaller::getCandidateFamilies();
    QProgressDialog progress(tr("Downloading free fonts to try..."), tr("Stop"), 0, int(candidates.size()), this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);

    QStringList loaded;
    QString lastError;
    for (int i = 0; i < candidates.size() && !progress.wasCanceled(); ++i)
    {
        progress.setValue(i);
        progress.setLabelText(tr("Downloading %1...").arg(candidates[i]));
        QApplication::processEvents();
        if (pdf::PDFFontIdentifier::isInstalled(candidates[i]))
        {
            continue;
        }
        QString errorMessage;
        const QString family = pdf::PDFFontInstaller::loadForTrial(candidates[i], &errorMessage);
        if (!family.isEmpty())
        {
            m_trialFamilies.insert(family);
            loaded << family;
        }
        else if (!errorMessage.isEmpty())
        {
            lastError = errorMessage;
        }
    }
    progress.setValue(int(candidates.size()));

    if (loaded.isEmpty() && !lastError.isEmpty())
    {
        QMessageBox::warning(this, windowTitle(), tr("No font could be downloaded: %1").arg(lastError));
    }
    identify(loaded);
}

void ScanTextDialog::buildScanFont()
{
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString characters;
    const QString family = tr("Scan Font %1").arg(m_scanFonts.size() + 1);
    const QByteArray font = pdf::PDFScanFontBuilder::build(m_document, { m_pageIndex }, family, &characters);
    const int id = font.isEmpty() ? -1 : QFontDatabase::addApplicationFontFromData(font);
    QApplication::restoreOverrideCursor();

    if (id < 0)
    {
        QMessageBox::information(this, windowTitle(), tr("Too few letters of this page could be read to build a font. The page needs OCR text, and its letters must not touch each other."));
        return;
    }

    const QString loaded = QFontDatabase::applicationFontFamilies(id).value(0, family);
    m_scanFonts.insert(loaded, characters);
    identify({ loaded });
}

const pdf::PDFFontIdentifier::Match* ScanTextDialog::getSelectedMatch() const
{
    const QList<QTableWidgetItem*> selected = m_matchTable->selectedItems();
    if (selected.isEmpty())
    {
        return nullptr;
    }
    const QVariant index = m_matchTable->item(selected.front()->row(), 0)->data(Qt::UserRole);
    return index.isValid() ? &m_matches.at(index.toInt()) : nullptr;
}

bool ScanTextDialog::ensureInstalled(const QString& family)
{
    if (m_scanFonts.contains(family))
    {
        // Built from the document itself: embedded in the PDF, nothing to install
        return true;
    }
    if (pdf::PDFFontIdentifier::isInstalled(family) && !m_trialFamilies.contains(family))
    {
        return true;
    }

    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString errorMessage;
    const std::optional<pdf::PDFFontInstaller::FontInfo> font = pdf::PDFFontInstaller::find(family, &errorMessage);
    QApplication::restoreOverrideCursor();

    if (!font || !pdf::PDFFontInstaller::isFreeLicense(font->license))
    {
        // A commercial font is never downloaded from an unofficial source
        const QStringList substitutes = pdf::PDFFontIdentifier::getMetricCompatibleSubstitutes(family);
        QString message = !errorMessage.isEmpty() ? errorMessage
                                                  : tr("The font %1 is not installed and is not available as a free font. Install it from its vendor (with its licence) to edit in it.").arg(family);
        if (!substitutes.isEmpty())
        {
            message += tr("\n\nFree fonts with the same letter widths: %1. Install one of them by its name to use it as a substitute.").arg(substitutes.join(", "));
        }
        QMessageBox::information(this, windowTitle(), message);
        return false;
    }

    if (QMessageBox::question(this, windowTitle(),
                              tr("The font %1 is not installed. Download it from Fontsource and install it for your user?\n\nLicence: %2 (free to use and to embed in documents).\n"
                                 "The download comes from api.fontsource.org and cdn.jsdelivr.net, which see the address of your computer.").arg(font->family, font->license),
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) != QMessageBox::Yes)
    {
        return false;
    }

    QApplication::setOverrideCursor(Qt::WaitCursor);
    const bool isInstalled = pdf::PDFFontInstaller::install(*font, &errorMessage);
    QApplication::restoreOverrideCursor();
    if (!isInstalled)
    {
        QMessageBox::critical(this, windowTitle(), errorMessage);
        return false;
    }
    if (!errorMessage.isEmpty())
    {
        // Installed, but not every style (the message names them)
        QMessageBox::warning(this, windowTitle(), errorMessage);
    }

    m_trialFamilies.remove(family);
    return true;
}

void ScanTextDialog::installSelected()
{
    const QList<QTableWidgetItem*> selected = m_matchTable->selectedItems();
    if (selected.isEmpty())
    {
        return;
    }
    const QString family = m_matchTable->item(selected.front()->row(), 0)->text();
    if (ensureInstalled(family))
    {
        identify({ family });
    }
}

void ScanTextDialog::updatePreview()
{
    const pdf::PDFFontIdentifier::Match* match = getSelectedMatch();
    m_installButton->setEnabled(!m_matchTable->selectedItems().isEmpty());
    if (!match || m_newTextEdit->text().isEmpty())
    {
        m_previewLabel->clear();
        return;
    }

    QFont font(match->family);
    font.setBold(match->bold);
    font.setItalic(match->italic);
    font.setPixelSize(qMax(8, qRound(match->pixelSize * 120.0 / qMax(1, m_image.height()))));
    QImage preview(560, 60, QImage::Format_RGB32);
    preview.fill(m_paper);
    QPainter painter(&preview);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setFont(font);
    painter.setPen(m_ink);
    painter.drawText(preview.rect().adjusted(6, 0, -6, 0), Qt::AlignLeft | Qt::AlignVCenter, m_newTextEdit->text());
    painter.end();
    m_previewLabel->setPixmap(QPixmap::fromImage(preview));
}

void ScanTextDialog::accept()
{
    const pdf::PDFFontIdentifier::Match* match = getSelectedMatch();
    if (!match)
    {
        QMessageBox::critical(this, windowTitle(), tr("Identify the font and choose it in the list first."));
        return;
    }

    // The font is installed before it is used, as it would otherwise exist only for
    // this session; the document embeds it in any case
    if (m_trialFamilies.contains(match->family) && !ensureInstalled(match->family))
    {
        return;
    }

    // Letters the scan did not show are drawn in another font
    if (m_scanFonts.contains(match->family))
    {
        QString missing;
        for (QChar c : m_newTextEdit->text())
        {
            if (!c.isSpace() && !m_scanFonts.value(match->family).contains(c) && !missing.contains(c))
            {
                missing += c;
            }
        }
        if (!missing.isEmpty() &&
            QMessageBox::question(this, windowTitle(), tr("The letters \"%1\" do not appear on this page, they are drawn in a similar installed font. Continue?").arg(missing),
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) != QMessageBox::Yes)
        {
            return;
        }
    }

    m_edit.pageIndex = m_pageIndex;
    m_edit.area = m_area;
    m_edit.inkRectangle = pdf::PDFScanTextEditor::getInkRectangle(m_image, m_area);
    m_edit.text = m_newTextEdit->text();
    m_edit.font = QFont(match->family);
    m_edit.font.setBold(match->bold);
    m_edit.font.setItalic(match->italic);
    m_edit.font.setPointSizeF(match->pixelSize * 72.0 / SCAN_DPI);
    m_edit.ink = m_ink;
    m_edit.paper = m_paper;
    QDialog::accept();
}

}   // namespace pdfplugin
