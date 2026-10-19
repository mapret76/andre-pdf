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

#include "pdfversionhistorydialog.h"
#include "pdfwidgetutils.h"

#include <QSettings>
#include <QVBoxLayout>
#include <QTextBrowser>
#include <QApplication>
#include <QDialogButtonBox>

namespace pdfviewer
{

PDFVersionHistoryDialog::PDFVersionHistoryDialog(QWidget* parent, const QString& title, const QString& introduction, const pdf::PDFVersionHistory::Releases& releases) :
    QDialog(parent),
    m_browser(new QTextBrowser(this))
{
    setWindowTitle(title);
    QVBoxLayout* layout = new QVBoxLayout(this);
    m_browser->setOpenExternalLinks(true);
    QString markdown;
    if (!introduction.isEmpty())
    {
        markdown += introduction + QStringLiteral("\n\n");
    }
    markdown += pdf::PDFVersionHistory::toMarkdown(releases);
    m_browser->setMarkdown(markdown);
    layout->addWidget(m_browser);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttonBox);
    pdf::PDFWidgetUtils::scaleWidget(this, QSize(600, 520));
}

QString PDFVersionHistoryDialog::getText() const
{
    return m_browser->toPlainText();
}

void PDFVersionHistoryDialog::showWhatsNewOnce(QWidget* parent)
{
    const QString current = QApplication::applicationVersion();
    QSettings settings;
    settings.beginGroup(QStringLiteral("AndrePDF"));
    const QString lastRun = settings.value(QStringLiteral("LastRunVersion")).toString();
    settings.setValue(QStringLiteral("LastRunVersion"), current);
    settings.endGroup();

    // Nothing on the first start, nor without an update since the last one
    if (lastRun.isEmpty() || !pdf::PDFVersionHistory::isValidVersion(lastRun) || pdf::PDFVersionHistory::compare(current, lastRun) <= 0)
    {
        return;
    }

    pdf::PDFVersionHistory::Releases releases = pdf::PDFVersionHistory::getNewer(pdf::PDFVersionHistory::parse(pdf::PDFVersionHistory::getEmbeddedHistory()), lastRun);
    if (!releases.empty())
    {
        PDFVersionHistoryDialog dialog(parent, tr("What's New in Andre PDF %1").arg(current), tr("Andre PDF was updated from version %1.").arg(lastRun), releases);
        dialog.exec();
    }
}

}   // namespace pdfviewer
