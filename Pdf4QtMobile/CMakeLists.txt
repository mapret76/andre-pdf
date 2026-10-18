# MIT License
#
# Copyright (c) 2018-2026 Jakub Melka and Contributors
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

# Andre PDF for Android (and, for development, the desktop): a Qt Quick app on the
# core library - read, sign with a certificate file, lock, signature library

qt_add_executable(Pdf4QtMobile
    main.cpp
    pdfmobilecontroller.cpp
    pdfmobilecontroller.h
)

qt_add_qml_module(Pdf4QtMobile
    URI AndrePdf
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    QML_FILES Main.qml
)

target_link_libraries(Pdf4QtMobile PRIVATE Pdf4QtLibCore Qt6::Core Qt6::Gui Qt6::Quick Qt6::QuickControls2)

set_target_properties(Pdf4QtMobile PROPERTIES
    OUTPUT_NAME AndrePdfMobile
    QT_ANDROID_PACKAGE_NAME "za.co.carouselworld.andrepdf"
    QT_ANDROID_APP_NAME "Andre PDF"
    QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android"
    QT_ANDROID_VERSION_NAME "${PDF4QT_VERSION}"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}
)

install(TARGETS Pdf4QtMobile RUNTIME DESTINATION ${PDF4QT_INSTALL_BIN_DIR} BUNDLE DESTINATION ${PDF4QT_INSTALL_BIN_DIR} LIBRARY DESTINATION ${PDF4QT_INSTALL_LIB_DIR})
