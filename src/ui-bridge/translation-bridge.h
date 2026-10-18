#pragma once
#include "lyrics-model.h"
#include "translation-coordinator.h"
namespace relaxer {
class TranslationBridge final:public QObject {
    Q_OBJECT
    Q_PROPERTY(QString taskKey READ taskKey NOTIFY changed)
    Q_PROPERTY(QString consentTask READ consentTask NOTIFY changed)
    Q_PROPERTY(QString state READ state NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString details READ details NOTIFY changed)
    Q_PROPERTY(bool pending READ pending NOTIFY changed)
    Q_PROPERTY(bool preview READ preview NOTIFY changed)
    Q_PROPERTY(int classification READ classification NOTIFY changed)
    Q_PROPERTY(int completedLines READ completedLines NOTIFY changed)
    Q_PROPERTY(int totalLines READ totalLines NOTIFY changed)
    Q_PROPERTY(bool unfinished READ unfinished NOTIFY changed)
public:
    TranslationBridge(Library *library,PlaybackFacade *facade,TranslationCoordinator *service,LyricsModel *lyrics,QObject *parent=nullptr);
    QString taskKey()const;QString consentTask()const{return m_consent?QString::number(m_consent):QString();}
    QString state()const;QString status()const;QString details()const;
    bool pending()const{return summary().pending();}bool preview()const{return summary().hasPreview();}
    int classification()const;
    int completedLines()const{return summary().completedLines;}int totalLines()const{return summary().totalLines;}
    bool unfinished()const;
    Q_INVOKABLE bool classify(int value);
    Q_INVOKABLE QString prepareAi();
    Q_INVOKABLE QString prepareBrowser(const QString &provider,const QString &language);
    Q_INVOKABLE bool start(const QString &exactConsentTask);
    Q_INVOKABLE void declineConsent();
    Q_INVOKABLE bool cancel();Q_INVOKABLE bool discard();Q_INVOKABLE bool save();Q_INVOKABLE bool resume();
    TranslationSummary summary()const;
signals:void changed();
private:
    Library *m_library;PlaybackFacade *m_facade;TranslationCoordinator *m_service;LyricsModel *m_lyrics;
    QHash<qint64,quint64> m_tasks;quint64 m_consent=0;QString m_error;
    QString prepared(quint64 task,const QString &error);void refresh();bool canPrepare();
};
}
