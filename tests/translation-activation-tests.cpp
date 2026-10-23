#include "lyricslookup.h"
#include "gui-test-main.h"
#include <QFile>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>
#ifdef Q_OS_WIN
#define NOMINMAX
#include <windows.h>
#endif
using namespace relaxer;
namespace relaxer {
class LyricsLookupDiagnostics {
public:
    static bool busy(const LyricsLookup &d){return d.m_translationAuditInFlight||d.m_translationAuditPending||(d.m_translationAuditDebounce&&d.m_translationAuditDebounce->isActive());}
    static quint64 audits(const LyricsLookup &d){return d.m_filterMetrics.auditPasses;}
    static quint64 revision(const LyricsLookup &d){return d.m_translationAssessmentRevision;}
};
}
class TranslationActivationTests final:public QObject {
    Q_OBJECT
    QStringList events;
    bool eventFilter(QObject *o,QEvent *e)override{
        if(e->type()==QEvent::WindowActivate||e->type()==QEvent::WindowDeactivate||e->type()==QEvent::ActivationChange)
            events<<QString("%1:%2:%3").arg(o->objectName()).arg(int(e->type())).arg(qobject_cast<QWidget *>(o)->isActiveWindow());
        return false;
    }
private slots:
    void realOwnedWindowsActivationRefreshesAudit(){
#ifndef Q_OS_WIN
        QFAIL("BLOCKED: Windows interactive desktop required");
#else
        if(QGuiApplication::platformName()!=QStringLiteral("windows"))QFAIL("BLOCKED: native windows platform required; use -platform windows");
        LASTINPUTINFO input{sizeof(LASTINPUTINFO),0};QVERIFY(GetLastInputInfo(&input));const auto initialInput=input.dwTime;
        if(GetTickCount()-initialInput<2000)QFAIL("BLOCKED: recent user input; no activation attempted");
        QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());Track track;track.hash="native-activation";track.title="Activation";track.file="Music/synthetic.mid";
        const auto id=library.insert(track);QVERIFY(id>0);QVERIFY(library.saveLyrics(id,"[00:01.00]Hello my friend\n[00:02.00]Good morning"));
        LyricsLookup dialog(&library,{id},nullptr,QUrl("http://127.0.0.1:9/api"));dialog.setObjectName("owned-dialog");dialog.installEventFilter(this);
        // A production LyricsLookup is application-modal. A separate owned
        // child window can receive focus without bypassing that modal boundary.
        QWidget peer(&dialog,Qt::Window);peer.setObjectName("owned-peer");peer.setWindowTitle("Relaxer owned activation fixture");peer.installEventFilter(this);
        auto noUserInput=[&]{LASTINPUTINFO now{sizeof(LASTINPUTINFO),0};return GetLastInputInfo(&now)&&now.dwTime==initialInput;};
        auto ownedForeground=[&](QWidget &w){return GetForegroundWindow()==reinterpret_cast<HWND>(w.winId());};
        dialog.findChild<QCheckBox *>("lyricsMissingTranslationOnly")->setChecked(true);dialog.show();
        QTRY_VERIFY2_WITH_TIMEOUT(dialog.isActiveWindow()&&ownedForeground(dialog),"BLOCKED: OS did not grant initial owned foreground; foreground lock/user desktop left untouched",3000);
        QVERIFY2(noUserInput(),"BLOCKED: user input changed during owned test");
        QTRY_VERIFY_WITH_TIMEOUT(!LyricsLookupDiagnostics::busy(dialog),5000);
        const auto before=LyricsLookupDiagnostics::audits(dialog),revision=LyricsLookupDiagnostics::revision(dialog);
        auto *row=dialog.findChild<QListWidget *>("lyricsTracks")->item(0);QVERIFY(!row->isHidden());
        // Switch while the current foreground is still our own dialog. If we
        // minimized first, Windows could give focus back to the user's app;
        // this test must never attempt to reclaim it from that application.
        QVERIFY(ownedForeground(dialog));peer.show();peer.activateWindow();
        QTRY_VERIFY2_WITH_TIMEOUT(peer.isActiveWindow()&&ownedForeground(peer)&&!dialog.isActiveWindow(),"BLOCKED: controlled peer activation unavailable",3000);
        QVERIFY2(noUserInput(),"BLOCKED: user input changed; owned test stopping");
        QFile file(library.absolute(library.track(id).lyrics));QVERIFY(file.open(QIODevice::WriteOnly|QIODevice::Truncate));file.write(QStringLiteral("你好世界").toUtf8());file.close();
        // At this point only our peer owns the foreground. Do not force
        // foreground access to a user window or use synthetic Qt events.
        QVERIFY(ownedForeground(peer));dialog.activateWindow();
        QTRY_VERIFY_WITH_TIMEOUT(dialog.isActiveWindow()&&ownedForeground(dialog),3000);
        QTRY_VERIFY_WITH_TIMEOUT(LyricsLookupDiagnostics::audits(dialog)>before,5000);
        QTRY_VERIFY_WITH_TIMEOUT(!LyricsLookupDiagnostics::busy(dialog),5000);
        QVERIFY(LyricsLookupDiagnostics::revision(dialog)>revision);QVERIFY(row->isHidden());
        QVERIFY2(noUserInput(),"BLOCKED: user input changed; result not counted as interactive evidence");
        qInfo()<<"OS activation 1 -> 0 -> 1; foreground HWND matched owned windows; native events"<<events;
        QVERIFY(events.filter(":24:").size()>0); // QEvent::WindowActivate
#endif
    }
};
RELAXER_GUI_TEST_MAIN(TranslationActivationTests)
#include "translation-activation-tests.moc"
