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

#ifndef PDFUPDATEDIALOG_H
#define PDFUPDATEDIALOG_H

#include "pdfupdatechecker.h"

#include <QDialog>

class QTextBrowser;

namespace pdfviewer
{

/// Offers an update: the new version, a summary of every version since the installed
/// one, and the choice to install it, to be reminded later or to skip the version
class PDFUpdateDialog : public QDialog
{
    Q_OBJECT

public:
    enum class Choice
    {
        Later,
        Install,
        Skip
    };

    explicit PDFUpdateDialog(QWidget* parent, const pdf::PDFUpdateChecker::Update& update, const QString& currentVersion, bool isAutomatic = false);

    Choice getChoice() const { return m_choice; }

    /// Returns the text shown (for the tests)
    QString getText() const;

private:
    Choice m_choice = Choice::Later;
    QTextBrowser* m_browser;
};

}   // namespace pdfviewer

#endif // PDFUPDATEDIALOG_H
