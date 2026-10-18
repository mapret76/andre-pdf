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

#include "pdfmobilecontroller.h"
#include "pdfconstants.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickImageProvider>
#include <QQuickStyle>

namespace pdfmobile
{

/// Pages of the document for the QML Image items: "image://page/<revision>/<index>"
class PDFPageImageProvider : public QQuickImageProvider
{
public:
    explicit PDFPageImageProvider(const PDFMobileController* controller) :
        QQuickImageProvider(QQuickImageProvider::Image, QQmlImageProviderBase::ForceAsynchronousImageLoading),
        m_controller(controller)
    {

    }

    virtual QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override
    {
        const int pageIndex = id.section(QChar('/'), 1, 1).toInt();
        const int width = requestedSize.width() > 0 ? requestedSize.width() : 1000;
        const QSize imageSize(width, qRound(width * m_controller->getPageAspectRatio(pageIndex)));
        const QImage image = m_controller->renderPage(pageIndex, imageSize);
        if (size)
        {
            *size = image.size();
        }
        return image;
    }

private:
    const PDFMobileController* m_controller;
};

}   // namespace pdfmobile

int main(int argc, char* argv[])
{
    QGuiApplication application(argc, argv);
    QCoreApplication::setOrganizationName("MelkaJ");
    QCoreApplication::setApplicationName("Andre PDF Mobile");
    QCoreApplication::setApplicationVersion(pdf::PDF_LIBRARY_VERSION);
    QGuiApplication::setApplicationDisplayName(QGuiApplication::translate("Application", "Andre PDF"));
    QQuickStyle::setStyle(QStringLiteral("Material"));

    pdfmobile::PDFMobileController controller;
    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("page"), new pdfmobile::PDFPageImageProvider(&controller));
    engine.rootContext()->setContextProperty(QStringLiteral("controller"), &controller);
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/AndrePdf/Main.qml")));
    if (engine.rootObjects().isEmpty())
    {
        return 1;
    }
    return application.exec();
}
