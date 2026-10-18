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

#include "scantextplugin.h"
#include "scantextdialog.h"
#include "pdfdrawwidget.h"
#include "pdfwidgettool.h"
#include "pdfscantexteditor.h"
#include "pdfdocumentsigner.h"

#include <QAction>
#include <QMainWindow>
#include <QMessageBox>

namespace pdfplugin
{

ScanTextTool::ScanTextTool(pdf::PDFDrawWidgetProxy* proxy, QAction* action, QObject* parent) :
    BaseClass(proxy, action, parent),
    m_pickTool(nullptr)
{
    m_pickTool = new pdf::PDFPickTool(proxy, pdf::PDFPickTool::Mode::Rectangles, this);
    addTool(m_pickTool);
    connect(m_pickTool, &pdf::PDFPickTool::rectanglePicked, this, &ScanTextTool::areaPicked);
}

ScanTextPlugin::ScanTextPlugin() :
    pdf::PDFPlugin(nullptr),
    m_editAction(nullptr),
    m_tool(nullptr)
{

}

void ScanTextPlugin::setWidget(pdf::PDFWidget* widget)
{
    Q_ASSERT(!m_widget);
    BaseClass::setWidget(widget);

    m_editAction = new QAction(QIcon(":/pdfplugins/scantextplugin/edit-scanned-text.svg"), tr("Edit &Scanned Text..."), this);
    m_editAction->setObjectName("scantext_editAction");
    m_editAction->setCheckable(true);
    m_editAction->setToolTip(tr("Drag a rectangle over the words of a scanned page to edit them in the font of the scan"));

    m_tool = new ScanTextTool(widget->getDrawWidgetProxy(), m_editAction, this);
    widget->getToolManager()->addTool(m_tool);
    connect(m_tool, &ScanTextTool::areaPicked, this, &ScanTextPlugin::onAreaPicked);

    updateActions();
}

void ScanTextPlugin::setDocument(const pdf::PDFModifiedDocument& document)
{
    BaseClass::setDocument(document);

    if (document.hasReset())
    {
        updateActions();
    }
}

std::vector<QAction*> ScanTextPlugin::getActions() const
{
    return { m_editAction };
}

QString ScanTextPlugin::getPluginMenuName() const
{
    return tr("Scan &Text");
}

void ScanTextPlugin::onAreaPicked(pdf::PDFInteger pageIndex, QRectF pageRectangle)
{
    m_tool->setActive(false);
    if (!m_document || pageRectangle.width() < 2.0 || pageRectangle.height() < 2.0)
    {
        return;
    }

    if (pdf::PDFDocumentSigner::hasExistingSignatures(m_document) &&
        QMessageBox::warning(m_widget, tr("Edit Scanned Text"), tr("The document is signed. An edit changes the signed content, the signatures will report it. Continue?"),
                             QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
    {
        return;
    }

    ScanTextDialog dialog(m_dataExchangeInterface->getMainWindow(), m_document, pageIndex, pageRectangle);
    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    const bool hadText = !pdf::PDFScanTextEditor::getText(m_document, pageIndex, pageRectangle).isEmpty();
    bool isOcrLayerRemoved = false;
    pdf::PDFDocumentPointer document = pdf::PDFScanTextEditor::apply(m_document, dialog.getEdit(), &isOcrLayerRemoved);
    if (!document)
    {
        QMessageBox::critical(m_widget, tr("Edit Scanned Text"), tr("The text could not be replaced."));
        return;
    }

    pdf::PDFModifiedDocument::ModificationFlags flags;
    flags.setFlag(pdf::PDFModifiedDocument::PageContents);
    Q_EMIT m_widget->getToolManager()->documentModified(pdf::PDFModifiedDocument(document, nullptr, flags));

    if (isOcrLayerRemoved)
    {
        QMessageBox::information(m_widget, tr("Edit Scanned Text"),
                                 tr("The text was replaced. The hidden OCR text of this page held the old words and was removed; "
                                    "run OCR on the page again (Tools) to make the rest of the page searchable."));
    }
    else if (hadText)
    {
        QMessageBox::information(m_widget, tr("Edit Scanned Text"),
                                 tr("The text was replaced on the page. The old words are covered, but the text layer of the document "
                                    "still holds them. For born-digital text use Edit Page Content, which changes the text itself."));
    }
}

void ScanTextPlugin::updateActions()
{
    if (m_editAction)
    {
        m_editAction->setEnabled(m_document);
    }
}

}   // namespace pdfplugin
