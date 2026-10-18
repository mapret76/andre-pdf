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

#include "pdfsharesheet.h"

#include <QDir>
#include <QWidget>

#if defined(Q_OS_WIN) && defined(_MSC_VER) && __has_include(<winrt/Windows.ApplicationModel.DataTransfer.h>)
#define PDF4QT_WINDOWS_SHARE_SHEET
#include <map>
#include <Windows.h>
#include <shobjidl_core.h>
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.ApplicationModel.DataTransfer.h>
#endif

namespace pdfviewer
{

bool PDFShareSheet::isAvailable()
{
#ifdef PDF4QT_WINDOWS_SHARE_SHEET
    return true;
#else
    return false;
#endif
}

bool PDFShareSheet::share(QWidget* window, const QString& fileName, const QString& title)
{
#ifdef PDF4QT_WINDOWS_SHARE_SHEET
    namespace DataTransfer = winrt::Windows::ApplicationModel::DataTransfer;
    namespace Storage = winrt::Windows::Storage;

    try
    {
        const HWND windowHandle = reinterpret_cast<HWND>(window->window()->winId());
        auto interop = winrt::get_activation_factory<DataTransfer::DataTransferManager, IDataTransferManagerInterop>();
        DataTransfer::DataTransferManager manager{ nullptr };
        winrt::check_hresult(interop->GetForWindow(windowHandle, winrt::guid_of<DataTransfer::DataTransferManager>(), winrt::put_abi(manager)));

        // One handler of the request per window, the handler of the previous share is removed
        static std::map<HWND, std::pair<DataTransfer::DataTransferManager, winrt::event_token>> handlers;
        if (auto it = handlers.find(windowHandle); it != handlers.end())
        {
            it->second.first.DataRequested(it->second.second);
            handlers.erase(it);
        }

        const winrt::hstring path(QDir::toNativeSeparators(fileName).toStdWString());
        const winrt::hstring shareTitle(title.toStdWString());
        const winrt::event_token token = manager.DataRequested([path, shareTitle](const DataTransfer::DataTransferManager&, const DataTransfer::DataRequestedEventArgs& arguments)
        {
            DataTransfer::DataRequest request = arguments.Request();
            request.Data().Properties().Title(shareTitle);
            DataTransfer::DataRequestDeferral deferral = request.GetDeferral();
            Storage::StorageFile::GetFileFromPathAsync(path).Completed([request, deferral](const auto& operation, winrt::Windows::Foundation::AsyncStatus status)
            {
                if (status == winrt::Windows::Foundation::AsyncStatus::Completed)
                {
                    const Storage::IStorageItem item = operation.GetResults();
                    request.Data().SetStorageItems(winrt::single_threaded_vector<Storage::IStorageItem>({ item }));
                }
                else
                {
                    request.FailWithDisplayText(L"The file cannot be shared.");
                }
                deferral.Complete();
            });
        });
        handlers.emplace(windowHandle, std::make_pair(manager, token));

        winrt::check_hresult(interop->ShowShareUIForWindow(windowHandle));
        return true;
    }
    catch (const winrt::hresult_error&)
    {
        return false;
    }
#else
    Q_UNUSED(window);
    Q_UNUSED(fileName);
    Q_UNUSED(title);
    return false;
#endif
}

}   // namespace pdfviewer
