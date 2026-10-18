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

#ifndef SCANTEXTDIALOG_H
#define SCANTEXTDIALOG_H

#include "pdfdocument.h"
#include "pdffontidentifier.h"
#include "pdfscantexteditor.h"

#include <QDialog>
#include <QSet>

class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;

namespace pdfplugin
{

/// Edits the words in an area of a scanned page: the font of the scan is
/// identified, installed when it is missing, and the new text replaces the old one
/// in the same font, size and colours
class ScanTextDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ScanTextDialog(QWidget* parent, const pdf::PDFDocument* document, pdf::PDFInteger pageIndex, QRectF area);

    virtual void accept() override;

    /// Returns the edit chosen by the user
    const pdf::PDFScanTextEditor::Edit& getEdit() const { return m_edit; }

    /// Returns the fonts named in the resources of the page (born-digital text), parsed
    static std::vector<pdf::PDFFontIdentifier::FontName> getPageFonts(const pdf::PDFDocument* document, pdf::PDFInteger pageIndex);

    /// Identifies the font of the scan among the given families
    void identify(QStringList extraFamilies = QStringList());

private:
    void searchOnline();
    void installSelected();
    void updatePreview();
    const pdf::PDFFontIdentifier::Match* getSelectedMatch() const;
    bool ensureInstalled(const QString& family);

    const pdf::PDFDocument* m_document;
    pdf::PDFInteger m_pageIndex;
    QRectF m_area;
    QImage m_image;
    QColor m_paper;
    QColor m_ink;
    std::vector<pdf::PDFFontIdentifier::Match> m_matches;
    QSet<QString> m_trialFamilies;    ///< Downloaded for the trial, not installed
    QStringList m_pageFamilies;       ///< Named by the fonts of the page
    pdf::PDFScanTextEditor::Edit m_edit;

    QLabel* m_scanLabel = nullptr;
    QLineEdit* m_scanTextEdit = nullptr;
    QTableWidget* m_matchTable = nullptr;
    QLineEdit* m_newTextEdit = nullptr;
    QLabel* m_previewLabel = nullptr;
    QLabel* m_statusLabel = nullptr;
    QPushButton* m_installButton = nullptr;
};

}   // namespace pdfplugin

#endif // SCANTEXTDIALOG_H
