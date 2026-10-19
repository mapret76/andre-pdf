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

#include "pdfupdatechecker.h"
#include "pdfversionhistory.h"

#include <QTextDocument>
#include <atomic>
#include <QtTest>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QProcess>
#include <QStandardPaths>
#include <QCryptographicHash>

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/ec.h>

using namespace pdf;

/// Updates: signed manifest, versions, installer download and its checks
class UpdateTest : public QObject
{
    Q_OBJECT

private slots:
    void signatureVerification();
    void manifestRules();
    void checkAndDownload();
    void releaseKeyIsEmbedded();
    void releaseToolsInterop();

private:
    struct KeyPair
    {
        QByteArray privatePem;
        QByteArray publicPem;
    };
    static KeyPair createKey();
    static QByteArray sign(const QByteArray& data, const QByteArray& privatePem);
    static QByteArray manifest(const QString& version, const QString& url, const QByteArray& installer, const QString& product = QStringLiteral("Andre PDF"));
};

UpdateTest::KeyPair UpdateTest::createKey()
{
    EVP_PKEY* key = EVP_EC_gen("P-256");
    KeyPair pair;
    BIO* bio = BIO_new(BIO_s_mem());
    PEM_write_bio_PrivateKey(bio, key, nullptr, nullptr, 0, nullptr, nullptr);
    char* data = nullptr;
    long length = BIO_get_mem_data(bio, &data);
    pair.privatePem = QByteArray(data, int(length));
    BIO_free(bio);
    bio = BIO_new(BIO_s_mem());
    PEM_write_bio_PUBKEY(bio, key);
    length = BIO_get_mem_data(bio, &data);
    pair.publicPem = QByteArray(data, int(length));
    BIO_free(bio);
    EVP_PKEY_free(key);
    return pair;
}

QByteArray UpdateTest::sign(const QByteArray& data, const QByteArray& privatePem)
{
    BIO* bio = BIO_new_mem_buf(privatePem.constData(), int(privatePem.size()));
    EVP_PKEY* key = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);
    EVP_MD_CTX* context = EVP_MD_CTX_new();
    size_t length = 0;
    EVP_DigestSignInit(context, nullptr, EVP_sha256(), nullptr, key);
    EVP_DigestSign(context, nullptr, &length, reinterpret_cast<const unsigned char*>(data.constData()), size_t(data.size()));
    QByteArray signature(int(length), '\0');
    EVP_DigestSign(context, reinterpret_cast<unsigned char*>(signature.data()), &length, reinterpret_cast<const unsigned char*>(data.constData()), size_t(data.size()));
    signature.resize(int(length));
    EVP_MD_CTX_free(context);
    EVP_PKEY_free(key);
    return signature.toBase64();
}

QByteArray UpdateTest::manifest(const QString& version, const QString& url, const QByteArray& installer, const QString& product)
{
    const QString json = QString(R"({"product":"%1","version":"%2","releases":[
        {"version":"%2","date":"2026-11-01","summary":"Faster signing.","changes":["Signing is faster."]},
        {"version":"1.0.5","date":"2026-10-25","summary":"Fixes.","changes":["A fix."]},
        {"version":"1.0.0","date":"2026-10-19","summary":"First.","changes":["Everything."]}],
        "windows":{"url":"%3","sha256":"%4","size":%5},
        "linux":{"url":"%3","sha256":"%4","size":%5}})")
        .arg(product, version, url, QString::fromLatin1(QCryptographicHash::hash(installer, QCryptographicHash::Sha256).toHex())).arg(installer.size());
    return json.toUtf8();
}

void UpdateTest::signatureVerification()
{
    const KeyPair key = createKey();
    const KeyPair other = createKey();
    const QByteArray data("manifest bytes");
    const QByteArray signature = sign(data, key.privatePem);
    QVERIFY(PDFUpdateChecker::verifySignature(data, signature, key.publicPem));
    QVERIFY(!PDFUpdateChecker::verifySignature(data + " ", signature, key.publicPem));      // changed data
    QVERIFY(!PDFUpdateChecker::verifySignature(data, signature, other.publicPem));          // other key
    QVERIFY(!PDFUpdateChecker::verifySignature(data, QByteArray("bm90IGEgc2lnbmF0dXJl"), key.publicPem));
    QVERIFY(!PDFUpdateChecker::verifySignature(data, QByteArray("***"), key.publicPem));
    QVERIFY(!PDFUpdateChecker::verifySignature(data, signature, QByteArray("not a key")));
}

void UpdateTest::manifestRules()
{
    PDFUpdateChecker::setTestConfiguration(QString(), QByteArray(), QStringList());   // production rules: HTTPS, GitHub
    const QByteArray installer(2048, 'M');
    const QString url = QStringLiteral("https://github.com/mapret76/andre-pdf/releases/download/v1.1.0/AndrePDF_1.1.0.msi");
    QString errorMessage;

    std::optional<PDFUpdateChecker::Update> update = PDFUpdateChecker::parseManifest(manifest("1.1.0", url, installer), "1.0.0", "windows", &errorMessage);
    QVERIFY2(update, qPrintable(errorMessage));
    QCOMPARE(update->version, QStringLiteral("1.1.0"));
    QCOMPARE(update->releases.size(), size_t(2));       // 1.1.0 and 1.0.5, newest first
    QCOMPARE(update->releases[0].summary, QStringLiteral("Faster signing."));
    QCOMPARE(update->releases[1].version, QStringLiteral("1.0.5"));
    QCOMPARE(update->size, qint64(2048));

    // Up to date, or an older manifest replayed: nothing offered, no error
    QVERIFY(!PDFUpdateChecker::parseManifest(manifest("1.1.0", url, installer), "1.1.0", "windows", &errorMessage));
    QVERIFY(errorMessage.isEmpty());
    QVERIFY(!PDFUpdateChecker::parseManifest(manifest("1.1.0", url, installer), "2.0.0", "windows", &errorMessage));

    // Not acceptable
    QVERIFY(!PDFUpdateChecker::parseManifest(manifest("1.1.0", url, installer, "Other"), "1.0.0", "windows", &errorMessage));
    QVERIFY(!errorMessage.isEmpty());
    QVERIFY(!PDFUpdateChecker::parseManifest(manifest("1.1.0", "http://github.com/x.msi", installer), "1.0.0", "windows", &errorMessage));
    QVERIFY(!PDFUpdateChecker::parseManifest(manifest("1.1.0", "https://evil.example.com/x.msi", installer), "1.0.0", "windows", &errorMessage));
    QVERIFY(!PDFUpdateChecker::parseManifest(manifest("1.1.x", url, installer), "1.0.0", "windows", &errorMessage));
    QVERIFY(!PDFUpdateChecker::parseManifest(manifest("1.1.0", url, installer), "1.0.0", "android", &errorMessage));   // no installer
    QVERIFY(!PDFUpdateChecker::parseManifest(QByteArray("{not json"), "1.0.0", "windows", &errorMessage));
    QByteArray noSummary = manifest("1.2.0", url, installer);
    noSummary.replace("{\"version\":\"1.2.0\",\"date\"", "{\"version\":\"1.1.9\",\"date\"");
    QVERIFY(!PDFUpdateChecker::parseManifest(noSummary, "1.0.0", "windows", &errorMessage));   // newest version without its entry

    // Review B45 L3: strict fields - a version with a line break, a fractional size,
    // non-hex characters in the SHA-256, a URL with user name or port
    QVERIFY(!PDFVersionHistory::isValidVersion(QStringLiteral("1.0.0\n")));
    QByteArray fractional = manifest("1.1.0", url, installer);
    fractional.replace("\"size\":2048", "\"size\":2048.5");
    QVERIFY(fractional.contains("2048.5"));
    QVERIFY(!PDFUpdateChecker::parseManifest(fractional, "1.0.0", "windows", &errorMessage));
    QByteArray badHash = manifest("1.1.0", url, installer);
    badHash.replace("\"sha256\":\"", "\"sha256\":\"zz");
    QVERIFY(!PDFUpdateChecker::parseManifest(badHash, "1.0.0", "windows", &errorMessage));
    QVERIFY(!PDFUpdateChecker::parseManifest(manifest("1.1.0", "https://x@github.com/a.msi", installer), "1.0.0", "windows", &errorMessage));
    QVERIFY(!PDFUpdateChecker::parseManifest(manifest("1.1.0", "https://github.com:4444/a.msi", installer), "1.0.0", "windows", &errorMessage));

    // Review B45 M1: changelog text is shown as written, no markup or HTML
    PDFVersionHistory::Release release;
    release.version = QStringLiteral("1.2.0");
    release.summary = QStringLiteral("Press <Enter> to sign; files <5 MB\n## 99.0.0");
    release.changes << QStringLiteral("A list <!-- hidden") << QStringLiteral("*bold* [link](http://x)");
    const QString markdown = PDFVersionHistory::toMarkdown({ release });
    QVERIFY(!markdown.contains(QStringLiteral("<Enter>")));
    QVERIFY(!markdown.contains(QStringLiteral("\n## 99")));
    QTextDocument document;
    document.setMarkdown(markdown, QTextDocument::MarkdownFeatures(QTextDocument::MarkdownDialectGitHub | QTextDocument::MarkdownNoHTML));
    const QString shown = document.toPlainText();
    QVERIFY2(shown.contains(QStringLiteral("Press <Enter> to sign; files <5 MB ## 99.0.0")), qPrintable(shown));
    QVERIFY2(shown.contains(QStringLiteral("A list <!-- hidden")) && shown.contains(QStringLiteral("*bold* [link](http://x)")), qPrintable(shown));

    // A heading of an invalid version takes no lines from the release before it
    const PDFVersionHistory::Releases parsed = PDFVersionHistory::parse(QStringLiteral("## 1.1.0 - 2026-11-01\nSummary: One.\n- a\n## 1.2.3.4.5 - 2026-12-01\nSummary: Bad.\n- b\n"));
    QCOMPARE(parsed.size(), size_t(1));
    QCOMPARE(parsed.front().changes, QStringList{ QStringLiteral("a") });
}

void UpdateTest::checkAndDownload()
{
    const KeyPair key = createKey();
    const QByteArray installer = QByteArray(300000, 'A') + QByteArray("installer");

    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    const QString base = QString("http://127.0.0.1:%1").arg(server.serverPort());
    QByteArray currentManifest;
    QByteArray currentSignature;
    QByteArray currentInstaller = installer;
    connect(&server, &QTcpServer::newConnection, &server, [&]()
    {
        QTcpSocket* socket = server.nextPendingConnection();
        connect(socket, &QTcpSocket::readyRead, socket, [&, socket]()
        {
            const QByteArray path = socket->readAll().split(' ').value(1);
            QByteArray body = path.endsWith(".json") ? currentManifest : path.endsWith(".sig") ? currentSignature : path.endsWith(".msi") ? currentInstaller : QByteArray();
            socket->write("HTTP/1.0 200 OK\r\nContent-Length: " + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
            socket->disconnectFromHost();
        });
    });
    PDFUpdateChecker::setTestConfiguration(base, key.publicPem, { QStringLiteral("127.0.0.1") });

    currentManifest = manifest("1.1.0", base + "/AndrePDF_1.1.0.msi", installer);
    currentSignature = sign(currentManifest, key.privatePem);
    QString errorMessage;
    const std::optional<PDFUpdateChecker::Update> update = PDFUpdateChecker::check("1.0.0", &errorMessage);
    QVERIFY2(update, qPrintable(errorMessage));

    // A manifest changed after signing is refused
    const QByteArray goodManifest = currentManifest;
    currentManifest.replace("Faster signing.", "Faster signing!");
    QVERIFY(!PDFUpdateChecker::check("1.0.0", &errorMessage));
    QVERIFY(errorMessage.contains(QStringLiteral("not signed")));
    currentManifest = goodManifest;

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    qint64 lastProgress = 0;
    const QString file = PDFUpdateChecker::download(*update, directory.path(), [&lastProgress](qint64 received, qint64) { lastProgress = received; }, &errorMessage);
    QVERIFY2(!file.isEmpty(), qPrintable(errorMessage));
    QVERIFY(QFileInfo(file).fileName().startsWith(QStringLiteral("AndrePDF_1.1.0_")));
    QVERIFY(QFileInfo(file).fileName().endsWith(PDFUpdateChecker::getPlatform() == "windows" ? ".msi" : ".bin"));
    QCOMPARE(QFileInfo(file).size(), qint64(installer.size()));
    QCOMPARE(lastProgress, qint64(installer.size()));

    // Review B45 M2: each download is a new file, the earlier one is removed; the
    // installer is checked again before it starts
    const QString second = PDFUpdateChecker::download(*update, directory.path(), nullptr, &errorMessage);
    QVERIFY2(!second.isEmpty() && second != file && !QFile::exists(file), qPrintable(errorMessage));
    QCOMPARE(PDFUpdateChecker::getFileSha256(second), update->sha256);
    {
        QFile changed(second);
        QVERIFY(changed.open(QFile::ReadWrite));
        changed.write("X");
    }
    QVERIFY(!PDFUpdateChecker::startInstaller(second, update->sha256, &errorMessage));
    QVERIFY(errorMessage.contains(QStringLiteral("changed")));

    // Review B45 M3: a cancelled download stops and leaves no file
    std::atomic_bool cancel = true;
    QVERIFY(PDFUpdateChecker::download(*update, directory.path(), nullptr, &errorMessage, &cancel).isEmpty());
    QVERIFY(errorMessage.contains(QStringLiteral("cancelled")));
    QVERIFY(QDir(directory.path()).entryList({ QStringLiteral("AndrePDF_*") }, QDir::Files).isEmpty());

    // An installer of the same size but other bytes, or a larger one, is refused and deleted
    currentInstaller = QByteArray(installer.size(), 'B');
    QVERIFY(PDFUpdateChecker::download(*update, directory.path(), nullptr, &errorMessage).isEmpty());
    QVERIFY(QDir(directory.path()).entryList({ QStringLiteral("AndrePDF_*") }, QDir::Files).isEmpty());
    currentInstaller = installer + QByteArray(1000, 'C');
    QVERIFY(PDFUpdateChecker::download(*update, directory.path(), nullptr, &errorMessage).isEmpty());
    QVERIFY(QDir(directory.path()).entryList({ QStringLiteral("AndrePDF_*") }, QDir::Files).isEmpty());

    PDFUpdateChecker::setTestConfiguration(QString(), QByteArray(), QStringList());
}

void UpdateTest::releaseKeyIsEmbedded()
{
    const QByteArray key = PDFUpdateChecker::getReleasePublicKey();
    QVERIFY(key.startsWith("-----BEGIN PUBLIC KEY-----"));
    QVERIFY(!key.contains("PRIVATE"));
}

void UpdateTest::releaseToolsInterop()
{
    // The manifest written by tools/make_update_manifest.py and signed by the openssl
    // command (as tools/andre-release.sh does) is accepted by the program
    const QString python = QStandardPaths::findExecutable(QStringLiteral("python3"));
    const QString openssl = QStandardPaths::findExecutable(QStringLiteral("openssl"));
    const QString sourceDirectory = QStringLiteral(PDF4QT_TEST_SOURCE_DIRECTORY);
    if (python.isEmpty() || openssl.isEmpty() || !QFile::exists(sourceDirectory + "/tools/make_update_manifest.py"))
    {
        QSKIP("python3, openssl or the release tools are not available");
    }

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const KeyPair key = createKey();
    QFile keyFile(directory.filePath("key.pem"));
    QVERIFY(keyFile.open(QFile::WriteOnly));
    keyFile.write(key.privatePem);
    keyFile.close();
    QFile installer(directory.filePath("AndrePDF_9.0.0.msi"));
    QVERIFY(installer.open(QFile::WriteOnly));
    installer.write(QByteArray(5000, 'I'));
    installer.close();
    QFile changelog(directory.filePath("CHANGELOG.md"));
    QVERIFY(changelog.open(QFile::WriteOnly));
    changelog.write("# History\n\n## 9.0.0 - 2027-01-01\n\nSummary: Version nine.\n\n- Change one.\n\n## 1.0.0 - 2026-10-19\n\nSummary: First.\n\n- All.\n");
    changelog.close();

    QProcess process;
    process.start(python, { "-I", sourceDirectory + "/tools/make_update_manifest.py", "--version", "9.0.0", "--msi", installer.fileName(),
                            "--url", "https://github.com/mapret76/andre-pdf/releases/download/v9.0.0/AndrePDF_9.0.0.msi", "--changelog", changelog.fileName() });
    QVERIFY(process.waitForFinished(30000));
    QCOMPARE(process.exitCode(), 0);
    const QByteArray manifestData = process.readAllStandardOutput();

    QFile manifestFile(directory.filePath("manifest.json"));
    QVERIFY(manifestFile.open(QFile::WriteOnly));
    manifestFile.write(manifestData);
    manifestFile.close();
    process.start(openssl, { "dgst", "-sha256", "-sign", keyFile.fileName(), manifestFile.fileName() });
    QVERIFY(process.waitForFinished(30000));
    const QByteArray signature = process.readAllStandardOutput().toBase64();

    QVERIFY(PDFUpdateChecker::verifySignature(manifestData, signature, key.publicPem));
    PDFUpdateChecker::setTestConfiguration(QString(), QByteArray(), QStringList());
    QString errorMessage;
    const std::optional<PDFUpdateChecker::Update> update = PDFUpdateChecker::parseManifest(manifestData, "1.0.0", "windows", &errorMessage);
    QVERIFY2(update, qPrintable(errorMessage));
    QCOMPARE(update->releases.front().summary, QStringLiteral("Version nine."));
    QCOMPARE(update->size, qint64(5000));

    // An entry without its summary stops the release tool
    QVERIFY(changelog.open(QFile::WriteOnly | QFile::Truncate));
    changelog.write("## 9.0.0 - 2027-01-01\n\n- Change one.\n");
    changelog.close();
    process.start(python, { "-I", sourceDirectory + "/tools/make_update_manifest.py", "--version", "9.0.0", "--msi", installer.fileName(),
                            "--url", "https://github.com/x", "--changelog", changelog.fileName() });
    QVERIFY(process.waitForFinished(30000));
    QVERIFY(process.exitCode() != 0);
}

QTEST_MAIN(UpdateTest)
#include "tst_updatetest.moc"
