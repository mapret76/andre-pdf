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

#include "pdfsizereducer.h"
#include "pdfimageoptimizer.h"
#include "pdfoptimizer.h"
#include "pdfdocumentwriter.h"

#include <QBuffer>

#include "pdfdbgheap.h"

namespace pdf
{

QString PDFSizeReducer::getPresetName(Preset preset)
{
    switch (preset)
    {
        case Preset::Email:
            return tr("E-mail and screen (150 dpi)");
        case Preset::Smallest:
            return tr("Smallest file (110 dpi)");
        case Preset::Print:
            return tr("Printing (300 dpi)");
    }
    return QString();
}

PDFDocument PDFSizeReducer::reduce(const PDFDocument* document, Preset preset, PDFProgress* progress)
{
    PDFImageOptimizer::Settings settings = PDFImageOptimizer::Settings::createDefault();
    settings.enabled = true;
    int dpi = 150;
    int quality = 75;
    switch (preset)
    {
        case Preset::Email:
            settings.goal = PDFImageOptimizer::OptimizationGoal::MinimumSize;
            break;
        case Preset::Smallest:
            settings.goal = PDFImageOptimizer::OptimizationGoal::MinimumSize;
            dpi = 110;
            quality = 60;
            break;
        case Preset::Print:
            settings.goal = PDFImageOptimizer::OptimizationGoal::PreferQuality;
            dpi = 300;
            quality = 85;
            break;
    }
    settings.colorProfile.targetDpi = dpi;
    settings.grayProfile.targetDpi = dpi;
    settings.colorProfile.jpegQuality = quality;
    settings.grayProfile.jpegQuality = quality;
    settings.bitonalProfile.targetDpi = qMax(dpi, 200);  // text scans stay sharp

    PDFImageOptimizer imageOptimizer;
    const PDFDocument withImages = imageOptimizer.optimize(document, settings, PDFImageOptimizer::ImageOverrides(), progress);

    PDFOptimizer optimizer(PDFOptimizer::All, nullptr);
    optimizer.setDocument(&withImages);
    optimizer.optimize();
    return optimizer.takeOptimizedDocument();
}

qint64 PDFSizeReducer::getWrittenSize(const PDFDocument* document)
{
    QBuffer buffer;
    buffer.open(QBuffer::WriteOnly);
    PDFDocumentWriter writer(nullptr);
    if (!writer.write(&buffer, document))
    {
        return -1;
    }
    return buffer.size();
}

}   // namespace pdf
