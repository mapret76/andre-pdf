# Third-party notices and credits

Andre PDF is free software built almost entirely on the work of others. This file
credits every component it is built from, distributes or adapts, with its
licence. The full licence texts ship with the program in `THIRD-PARTY-NOTICES.txt`,
next to the program files. That file is assembled at install time from this list,
`LICENSE`, `3rdparty_licenses/` and, for the Windows installer, the `copyright` file
that vcpkg installs with every library. Help -> About lists the same components with
their versions.

Rule for contributors: a component goes in here **in the same commit** that brings
it in, and its licence text goes in `3rdparty_licenses/` unless the vcpkg copyright
file already carries it.

## The base program

| Component | Author | Licence | Use |
| --- | --- | --- | --- |
| [PDF4QT](https://github.com/JakubMelka/PDF4QT) 1.6 | Jakub Melka and contributors | MIT (`LICENSE`) | Andre PDF is a fork of PDF4QT: the PDF engine, Editor, Viewer, PageMaster, DocDiff, PdfTool, OCR integration, the installer scripts. Nearly all of the code is his. |
| Andre PDF changes | Andre Pretorius, written with Claude (Anthropic) | MIT, same as PDF4QT | Certification (DocMDP) signing, signing workflow, contract tools |

## Code included in the source tree

| Component | Author | Licence | Use |
| --- | --- | --- | --- |
| [QR Code generator library](https://github.com/nayuki/QR-Code-generator) (C++, commit 3c6d0b3), `Pdf4QtLibCore/sources/qrcodegen/` | Project Nayuki | MIT (`3rdparty_licenses/qrcodegen_LICENSE.txt`) | QR code in the signature verification stamp |

## Libraries distributed with the program (Windows installer)

| Component | Licence | Use |
| --- | --- | --- |
| [Qt 6](https://www.qt.io/) (Core, Gui, Widgets, Svg, Xml, Network, PrintSupport, TextToSpeech, Concurrent, Multimedia, image format and platform plugins) | **LGPL-3.0** (`3rdparty_licenses/Qt_LGPL-3.0.txt`, which refers to `GPL-3.0.txt`) | User interface and platform layer. Linked dynamically as unmodified DLLs, so they can be replaced by the user. Source: <https://download.qt.io/official_releases/qt/>. Qt itself contains third-party code, listed at <https://doc.qt.io/qt-6/licenses-used-in-qt.html>. |
| [OpenSSL](https://www.openssl.org/) 3 | Apache-2.0 | Cryptography for digital signatures, timestamps, encryption |
| [FreeType](https://freetype.org/) | FreeType Licence (FTL) | Font rendering. Portions of this software are copyright © The FreeType Project (www.freetype.org). All rights reserved. |
| [libjpeg-turbo](https://libjpeg-turbo.org/) | IJG, BSD-3-Clause, zlib | JPEG images. This software is based in part on the work of the Independent JPEG Group. |
| [OpenJPEG](https://www.openjpeg.org/) | BSD-2-Clause | JPEG 2000 images |
| [Little CMS 2](https://www.littlecms.com/) | MIT | Colour management |
| [zlib](https://zlib.net/) | zlib | Compression |
| [libpng](http://www.libpng.org/pub/png/libpng.html) | PNG Reference Library Licence v2 | PNG images |
| [Blend2D](https://blend2d.com/) | zlib | Fast 2D rendering backend |
| [AsmJit](https://asmjit.com/) | zlib | Code generation for Blend2D |
| [Clipper2](https://github.com/AngusJohnson/Clipper2) | Boost Software Licence 1.0 | Polygon clipping |
| [oneTBB](https://github.com/uxlfoundation/oneTBB) | Apache-2.0 | Parallel processing |
| [hwloc](https://www.open-mpi.org/projects/hwloc/) | BSD-3-Clause | Processor topology for oneTBB |
| [Tesseract OCR](https://github.com/tesseract-ocr/tesseract) 5.5 | Apache-2.0 | Text recognition (OCR) |
| [Leptonica](http://www.leptonica.org/) | BSD-2-Clause (Leptonica licence) | Image processing for Tesseract |
| [tessdata_fast](https://github.com/tesseract-ocr/tessdata_fast) language models | Apache-2.0 | Built-in OCR languages |
| [libtiff](https://libtiff.gitlab.io/libtiff/) | libtiff licence (BSD-style) | TIFF images (Leptonica) |
| [giflib](https://giflib.sourceforge.net/) | MIT | GIF images (Leptonica) |
| [libwebp](https://chromium.googlesource.com/webm/libwebp) | BSD-3-Clause | WebP images (Leptonica) |
| [libarchive](https://libarchive.org/) | BSD-2-Clause | Archive reading (Tesseract) |
| [curl](https://curl.se/) | curl licence (MIT-style) | URL input (Tesseract) |
| [Brotli](https://github.com/google/brotli) | MIT | Compression (FreeType) |
| [bzip2](https://sourceware.org/bzip2/) | bzip2 licence (BSD-style) | Compression |
| [XZ Utils / liblzma](https://tukaani.org/xz/) | 0BSD | Compression |
| [LZ4](https://lz4.org/) | BSD-2-Clause | Compression |
| [Zstandard](https://facebook.github.io/zstd/) | BSD-3-Clause | Compression |
| Microsoft Visual C++ Runtime | Microsoft Visual Studio redistributable terms | C++ runtime DLLs |

## Services used at run time (on request of the user)

| Service | Terms | Use |
| --- | --- | --- |
| [Fontsource](https://fontsource.org) API and its CDN (jsDelivr) | MIT (Fontsource), fonts under their own free licences (mostly OFL-1.1, Apache-2.0) | Finding and downloading free fonts for editing scanned text in its original font. Only fonts with a free licence are downloaded, and only after the user agrees; the services see the address of the computer. A downloaded font keeps its own licence, which the font file carries. |

## Tools used to build it (not distributed)

| Tool | Licence |
| --- | --- |
| [GCC](https://gcc.gnu.org/) (Linux builds and tests) | GPL-3.0 with the GCC Runtime Library Exception (the exception means programs compiled by GCC are not bound by the GPL) |
| Microsoft Visual C++ (Windows builds) | Microsoft Visual Studio licence |
| [CMake](https://cmake.org/) | BSD-3-Clause |
| [Ninja](https://ninja-build.org/) | Apache-2.0 |
| [vcpkg](https://github.com/microsoft/vcpkg) | MIT |
| [WiX Toolset](https://wixtoolset.org/) 3 | Microsoft Reciprocal Licence (MS-RL) |
| [aqtinstall](https://github.com/miurahr/aqtinstall), [install-qt-action](https://github.com/jurplel/install-qt-action) | MIT |
| [GitHub Actions](https://github.com/features/actions) | GitHub service terms |
| Ubuntu packages on the development VM (Qt, Tesseract, OpenSSL and others above) | their own licences, as above |

## Standards implemented

ISO 32000-2 (PDF 2.0, made available free of charge by the PDF Association), ETSI
EN 319 142 (PAdES), RFC 3161 (timestamps), RFC 5652 (CMS). No commercial PDF
product's code was studied, decompiled or copied.
