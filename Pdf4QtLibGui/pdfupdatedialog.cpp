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

#include "pdfupdatedialog.h"
#include "pdfwidgetutils.h"

#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QVBoxLayout>
#include <QTextBrowser>
#include <QDialogButtonBox>

namespace pdfviewer
{

PDFUpdateDialog::PDFUpdateDialog(QWidget* parent, const pdf::PDFUpdateChecker::Update& update, const QString& currentVersion) :
    QDialog(parent),
    m_browser(new QTextBrowser(this))
{
    setWindowTitle(tr("Andre PDF %1 Is Available").arg(update.version));
    QVBoxLayout* layout = new QVBoxLayout(this);

    QLabel* label = new QLabel(tr("<b>Andre PDF %1</b> is available - you have version %2. The download is %3 and its signature has been "
                                  "checked. What changes:").arg(update.version, currentVersion, QLocale::system().formattedDataSize(update.size)), this);
    label->setWordWrap(true);
    layout->addWidget(label);

    m_browser->setMarkdown(pdf::PDFVersionHistory::toMarkdown(update.releases));
    layout->addWidget(m_browser);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(this);
    QPushButton* install = buttonBox->addButton(tr("Download and Install"), QDialogButtonBox::AcceptRole);
    QPushButton* later = buttonBox->addButton(tr("Remind Me Later"), QDialogButtonBox::RejectRole);
    QPushButton* skip = buttonBox->addButton(tr("Skip This Version"), QDialogButtonBox::DestructiveRole);
    install->setDefault(true);
    connect(install, &QPushButton::clicked, this, [this]() { m_choice = Choice::Install; accept(); });
    connect(later, &QPushButton::clicked, this, [this]() { m_choice = Choice::Later; reject(); });
    connect(skip, &QPushButton::clicked, this, [this]() { m_choice = Choice::Skip; reject(); });
    layout->addWidget(buttonBox);
    pdf::PDFWidgetUtils::scaleWidget(this, QSize(560, 480));
}

QString PDFUpdateDialog::getText() const
{
    return m_browser->toPlainText();
}

}   // namespace pdfviewer
