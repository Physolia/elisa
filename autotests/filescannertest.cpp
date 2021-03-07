/*
   SPDX-FileCopyrightText: 2018 (c) Alexander Stippich <a.stippich@gmx.net>

   SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "filescanner.h"
#include "config-upnp-qt.h"

#include <QObject>
#include <QList>
#include <QUrl>


#include <QtTest>

class FileScannerTest: public QObject
{
    Q_OBJECT

public:

    explicit FileScannerTest(QObject *aParent = nullptr) : QObject(aParent)
    {
    }

public:

    QString createTrackUrl(const QString &subpath) {
        return QStringLiteral(LOCAL_FILE_TESTS_SAMPLE_FILES_PATH) + QStringLiteral("/cover_art") + subpath;
    }

    QList<QString> mTestTracksForDirectory = {
        createTrackUrl(QStringLiteral("/artist1/album1/not_existing.ogg")),
        createTrackUrl(QStringLiteral("/artist1/album2/not_existing.ogg")),
        createTrackUrl(QStringLiteral("/artist1/album3/not_existing.ogg")),
        createTrackUrl(QStringLiteral("/artist2/album1/not_existing.ogg")),
        createTrackUrl(QStringLiteral("/artist2/album2/not_existing.ogg")),
        createTrackUrl(QStringLiteral("/artist2/album3/not_existing.ogg")),
        createTrackUrl(QStringLiteral("/artist3/album1/not_existing.ogg")),
        createTrackUrl(QStringLiteral("/artist3/album2/not_existing.ogg")),
        createTrackUrl(QStringLiteral("/artist3/album3/not_existing.ogg")),
    };

    QList<QString> mTestTracksForMetaData = {
        createTrackUrl(QStringLiteral("/artist4/test.ogg")),
        createTrackUrl(QStringLiteral("/artist4/test.flac")),
        createTrackUrl(QStringLiteral("/artist4/test.mp3")),
    };

    QList<QString> mTestTracksForMetaDataWithCoverInDirectory = {
        createTrackUrl(QStringLiteral("/artist1/album1/test.ogg")),
        createTrackUrl(QStringLiteral("/artist1/album1/test.m4a")),
        createTrackUrl(QStringLiteral("/artist1/album1/test.mp3")),
    };

private Q_SLOTS:

    void initTestCase()
    {

    }

    void testFileMetaDataScan()
    {
        FileScanner fileScanner;
        auto scannedTrack = fileScanner.scanOneFile(QUrl::fromLocalFile(QStringLiteral(LOCAL_FILE_TESTS_SAMPLE_FILES_PATH) + QStringLiteral("/music/test.ogg")));
        QCOMPARE(scannedTrack.title(), QStringLiteral("Title"));
        QCOMPARE(scannedTrack.genre(), QStringLiteral("Genre"));
        QCOMPARE(scannedTrack.album(), QStringLiteral("Test"));
        QCOMPARE(scannedTrack.artist(), QStringLiteral("Artist"));

        auto scannedTrackCover1 = fileScanner.scanOneFile(QUrl::fromLocalFile(mTestTracksForMetaData.at(0)));
        QCOMPARE(scannedTrackCover1.hasEmbeddedCover(), true);
        auto trackCoverUrl1 = scannedTrackCover1.albumCover();
        QVERIFY(!trackCoverUrl1.isEmpty());
        QCOMPARE(trackCoverUrl1.toString(), QStringLiteral("image://cover/") + mTestTracksForMetaData.at(0));

        auto scannedTrackCover2 = fileScanner.scanOneFile(QUrl::fromLocalFile(mTestTracksForMetaData.at(1)));
        QCOMPARE(scannedTrackCover2.hasEmbeddedCover(), true);
        auto trackCoverUrl2 = scannedTrackCover2.albumCover();
        QVERIFY(!trackCoverUrl2.isEmpty());
        QCOMPARE(trackCoverUrl2.toString(), QStringLiteral("image://cover/") + mTestTracksForMetaData.at(1));

        auto scannedTrackCover3 = fileScanner.scanOneFile(QUrl::fromLocalFile(mTestTracksForMetaData.at(2)));
        QCOMPARE(scannedTrackCover3.hasEmbeddedCover(), true);
        auto trackCoverUrl3 = scannedTrackCover3.albumCover();
        QVERIFY(!trackCoverUrl3.isEmpty());
        QCOMPARE(trackCoverUrl3.toString(), QStringLiteral("image://cover/") + mTestTracksForMetaData.at(2));
    }

    void testFindCoverInDirectory()
    {
        FileScanner fileScanner;
        QVERIFY(!fileScanner.searchForCoverFile(mTestTracksForDirectory.at(0)).isEmpty());
        QVERIFY(!fileScanner.searchForCoverFile(mTestTracksForDirectory.at(1)).isEmpty());
        QVERIFY(!fileScanner.searchForCoverFile(mTestTracksForDirectory.at(2)).isEmpty());
        QVERIFY(!fileScanner.searchForCoverFile(mTestTracksForDirectory.at(3)).isEmpty());
        QVERIFY(!fileScanner.searchForCoverFile(mTestTracksForDirectory.at(4)).isEmpty());
        QVERIFY(!fileScanner.searchForCoverFile(mTestTracksForDirectory.at(5)).isEmpty());
        QVERIFY(!fileScanner.searchForCoverFile(mTestTracksForDirectory.at(6)).isEmpty());
        QVERIFY(!fileScanner.searchForCoverFile(mTestTracksForDirectory.at(7)).isEmpty());
        QVERIFY(!fileScanner.searchForCoverFile(mTestTracksForDirectory.at(8)).isEmpty());
    }


    void testFileMetaDataScanWithCoverInDirectory()
    {
        FileScanner fileScanner;

        auto vExpectedImageUrl = QUrl::fromLocalFile(createTrackUrl(QStringLiteral("/artist1/album1/cover.jpg"))).toString();

        auto scannedTrackCover1 = fileScanner.scanOneFile(QUrl::fromLocalFile(mTestTracksForMetaDataWithCoverInDirectory.at(0)));
        QCOMPARE(scannedTrackCover1.hasEmbeddedCover(), false);
        auto trackCoverUrl1 = scannedTrackCover1.albumCover();
        QVERIFY(!trackCoverUrl1.isEmpty());
        QCOMPARE(trackCoverUrl1.toString(), vExpectedImageUrl);

        auto scannedTrackCover2 = fileScanner.scanOneFile(QUrl::fromLocalFile(mTestTracksForMetaDataWithCoverInDirectory.at(1)));
        QCOMPARE(scannedTrackCover2.hasEmbeddedCover(), false);
        auto trackCoverUrl2 = scannedTrackCover2.albumCover();
        QVERIFY(!trackCoverUrl2.isEmpty());
        QCOMPARE(trackCoverUrl2.toString(), vExpectedImageUrl);

        auto scannedTrackCover3 = fileScanner.scanOneFile(QUrl::fromLocalFile(mTestTracksForMetaDataWithCoverInDirectory.at(2)));
        QCOMPARE(scannedTrackCover3.hasEmbeddedCover(), false);
        auto trackCoverUrl3 = scannedTrackCover3.albumCover();
        QVERIFY(!trackCoverUrl3.isEmpty());
        QCOMPARE(trackCoverUrl3.toString(), vExpectedImageUrl);
    }

    void benchmarkFileScan()
    {
        FileScanner fileScanner;
        QBENCHMARK {
            for (int i = 0; i < 100; i++) {
                auto scannedTrack = fileScanner.scanOneFile(QUrl::fromLocalFile(QStringLiteral(LOCAL_FILE_TESTS_SAMPLE_FILES_PATH) + QStringLiteral("/music/test.ogg")));
                auto scannedTrack2 = fileScanner.scanOneFile(QUrl::fromLocalFile(QStringLiteral(LOCAL_FILE_TESTS_SAMPLE_FILES_PATH) + QStringLiteral("/music/testMultiple.ogg")));
                auto scannedTrack3 = fileScanner.scanOneFile(QUrl::fromLocalFile(QStringLiteral(LOCAL_FILE_TESTS_SAMPLE_FILES_PATH) + QStringLiteral("/music/testMany.ogg")));
            }
        }
    }

    void benchmarkCoverInDirectory()
    {
        FileScanner fileScanner;
        QBENCHMARK {
            fileScanner.searchForCoverFile(mTestTracksForDirectory.at(0));
            fileScanner.searchForCoverFile(mTestTracksForDirectory.at(1));
            fileScanner.searchForCoverFile(mTestTracksForDirectory.at(2));
            fileScanner.searchForCoverFile(mTestTracksForDirectory.at(3));
            fileScanner.searchForCoverFile(mTestTracksForDirectory.at(4));
            fileScanner.searchForCoverFile(mTestTracksForDirectory.at(5));
            fileScanner.searchForCoverFile(mTestTracksForDirectory.at(6));
            fileScanner.searchForCoverFile(mTestTracksForDirectory.at(7));
            fileScanner.searchForCoverFile(mTestTracksForDirectory.at(8));
        }
    }
};

QTEST_GUILESS_MAIN(FileScannerTest)


#include "filescannertest.moc"
