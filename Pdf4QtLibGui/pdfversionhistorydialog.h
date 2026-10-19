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

#ifndef PDFVERSIONHISTORYDIALOG_H
#define PDFVERSIONHISTORYDIALOG_H

#include "pdfversionhistory.h"

#include <QDialog>

class QTextBrowser;

namespace pdfviewer
{

/// Shows versions of Andre PDF and their changes: the full history, or the
/// versions new since an update ("What's new"), or the versions an update brings
class PDFVersionHistoryDialog : public QDialog
{
    Q_OBJECT

public:
    /// \param parent Parent widget
    /// \param title Title of the dialog
    /// \param introduction Text above the versions, can be empty
    /// \param releases Versions shown, newest first
    explicit PDFVersionHistoryDialog(QWidget* parent, const QString& title, const QString& introduction, const pdf::PDFVersionHistory::Releases& releases);

    /// Returns the text shown (for the tests)
    QString getText() const;

    /// Shows the versions new since the version the user ran last, once after an
    /// update; remembers the current version in the settings
    static void showWhatsNewOnce(QWidget* parent);

private:
    QTextBrowser* m_browser;
};

}   // namespace pdfviewer

#endif // PDFVERSIONHISTORYDIALOG_H
