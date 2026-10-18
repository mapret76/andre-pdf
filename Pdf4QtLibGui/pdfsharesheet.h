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

#ifndef PDFSHARESHEET_H
#define PDFSHARESHEET_H

#include <QString>

class QWidget;

namespace pdfviewer
{

/// The share sheet of the operating system (Windows 10/11: e-mail, WhatsApp, Teams,
/// nearby sharing, ... - whichever apps are installed as share targets)
class PDFShareSheet
{
public:
    PDFShareSheet() = delete;

    /// Returns true, if the share sheet exists in this build
    static bool isAvailable();

    /// Shows the share sheet for a file, returns false when it cannot be shown
    /// \param window Window the share sheet belongs to
    /// \param fileName File to be shared
    /// \param title Title of the shared content
    static bool share(QWidget* window, const QString& fileName, const QString& title);
};

}   // namespace pdfviewer

#endif // PDFSHARESHEET_H
