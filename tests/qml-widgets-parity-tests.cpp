#include "ui-bridge/qml-shell.h"
#include "window.h"
#include <QApplication>
#include <QDataStream>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>
using namespace relaxer;
class QmlWidgetsParity : public QObject {
    Q_OBJECT
  private slots:
    void realWidgetsAndQmlShareSessionAndCommands() {
        QTemporaryDir dir; Library library(dir.path()); QVERIFY(library.open());
        QFile wave(library.absolute("Music/parity.wav")); QVERIFY(wave.open(QIODevice::WriteOnly));
        QDataStream out(&wave); out.setByteOrder(QDataStream::LittleEndian);
        out.writeRawData("RIFF",4);out<<quint32(160036);out.writeRawData("WAVEfmt ",8);
        out<<quint32(16)<<quint16(1)<<quint16(1)<<quint32(8000)<<quint32(16000)<<quint16(2)<<quint16(16);
        out.writeRawData("data",4);out<<quint32(160000);const QByteArray pcm(160000,'\0');out.writeRawData(pcm.data(),pcm.size());wave.close();
        QVector<qint64> ids;
        for(int i=0;i<3;++i) { Track t;t.hash=QString::number(i);t.title=QString("Parity %1").arg(i);t.file="Music/parity.wav";t.duration=10000;ids.append(library.insert(t));QVERIFY(ids.last()); }
        const auto connections=QSqlDatabase::connectionNames();
        {
            QmlShell qml(&library,nullptr,true); QVERIFY(qml.load());
            qml.libraryBridge()->play(QString::number(ids[1])); QTRY_VERIFY(qml.playbackBridge()->seekable());
            qml.playbackBridge()->pause();qml.playbackBridge()->seek(1700);QTRY_VERIFY(qAbs(qml.facade()->position()-1700)<150);
            qml.playbackBridge()->setVolume(43);qml.playbackBridge()->setMode(2);QVERIFY(qml.shutdown());
        }
        QVector<qint64> storedQueue;
        {
            Window widgets(&library);widgets.show();auto *facade=widgets.playbackFacade();
            QCOMPARE(widgets.findChildren<Playback *>().size(),1);QCOMPARE(QSqlDatabase::connectionNames(),connections);
            QCOMPARE(facade->current().id,ids[1]);QCOMPARE(facade->volume(),43);QCOMPARE(facade->mode(),Mode::SingleLoop);
            QTRY_VERIFY(facade->seekable());QTRY_VERIFY(qAbs(facade->position()-1700)<200);QVERIFY(!facade->playing());
            facade->toggle();QTRY_VERIFY(facade->playing());facade->toggle();QTRY_VERIFY(!facade->playing());
            facade->setMode(Mode::ListLoop);facade->next();QTRY_VERIFY(facade->current().id!=ids[1]);facade->previous();
            QTRY_COMPARE(facade->current().id,ids[1]);QTRY_VERIFY(facade->seekable());
            facade->toggle(); // Pause after the previous command resumes playback.
            QTRY_VERIFY(!facade->playing());facade->seek(2300);QTRY_VERIFY(qAbs(facade->position()-2300)<200);
            facade->setVolume(61);storedQueue=facade->queue().items;QVERIFY(facade->shutdown());widgets.close();
        }
        {
            QmlShell qml(&library,nullptr,false);QVERIFY(qml.load());
            QTRY_COMPARE(qml.queueModel()->ids(),storedQueue);QCOMPARE(qml.facade()->current().id,ids[1]);
            QCOMPARE(qml.playbackBridge()->volume(),61);QCOMPARE(qml.playbackBridge()->mode(),1);
            QTRY_VERIFY(qml.playbackBridge()->seekable());QTRY_VERIFY(qAbs(qml.playbackBridge()->position()-2300)<200);
            QVERIFY(!qml.playbackBridge()->playing());QCOMPARE(qml.findChildren<Playback *>().size(),1);
            QCOMPARE(QSqlDatabase::connectionNames(),connections);QVERIFY(qml.shutdown());QVERIFY(qml.warnings().isEmpty());
        }
        QCOMPARE(library.trackCount(),qint64(3));
    }
};
int main(int argc,char **argv) {
    QApplication app(argc,argv);app.setStyle("Fusion");app.setQuitOnLastWindowClosed(false);
    QmlWidgetsParity tests;return QTest::qExec(&tests,argc,argv);
}
#include "qml-widgets-parity-tests.moc"
