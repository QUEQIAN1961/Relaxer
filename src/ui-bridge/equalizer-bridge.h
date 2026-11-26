#pragma once
#include "library.h"
#include "audio/peq-api.h"
#include <QTimer>
#include <QJsonArray>
#include <QPointer>
#include <QVariantList>
#include <QThreadPool>
#include <atomic>
#include <memory>
namespace relaxer {
class PlaybackFacade;
class TrackListModel;
class EqualizerBridge final:public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap settings READ settings NOTIFY changed)
    Q_PROPERTY(QVariantList response READ response NOTIFY changed)
    Q_PROPERTY(QVariantList presets READ presets NOTIFY changed)
    Q_PROPERTY(QVariantList targets READ targets NOTIFY changed)
    Q_PROPERTY(QString targetKey READ targetKey NOTIFY changed)
    Q_PROPERTY(QVariantMap target READ target NOTIFY changed)
    Q_PROPERTY(QVariantMap active READ active NOTIFY changed)
    Q_PROPERTY(QString currentSongKey READ currentSongKey NOTIFY changed)
    Q_PROPERTY(int sampleRate READ sampleRate NOTIFY changed)
    Q_PROPERTY(double headroom READ headroom NOTIFY changed)
    Q_PROPERTY(bool mediaAvailable READ mediaAvailable CONSTANT)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool unsavedChanges READ unsavedChanges NOTIFY editStateChanged)
    Q_PROPERTY(bool hasUnsavedChanges READ hasUnsavedChanges NOTIFY editStateChanged)
    Q_PROPERTY(QString presetDraft READ presetDraft WRITE setPresetDraft NOTIFY editStateChanged)
    Q_PROPERTY(QString songQuery READ songQuery WRITE setSongQuery NOTIFY songSearchChanged)
    Q_PROPERTY(QVariantList songResults READ songResults NOTIFY songSearchChanged)
    Q_PROPERTY(bool songSearching READ songSearching NOTIFY songSearchChanged)
    Q_PROPERTY(QString songSearchMessage READ songSearchMessage NOTIFY songSearchChanged)
public:
    explicit EqualizerBridge(Library *library,QObject *parent=nullptr);
    ~EqualizerBridge()override;
    void attach(PlaybackFacade *facade,TrackListModel *source);
    void rememberCloudSongs(const QString &provider,const QVariantMap &playlist,const QVariantList &songs);
    QString songQuery()const{return m_songQuery;}
    QVariantList songResults()const{return m_songResults;}
    bool songSearching()const{return m_songSearching;}
    QString songSearchMessage()const{return m_songSearchMessage;}
    void setSongQuery(const QString &query);
    Q_INVOKABLE void setSongSearchActive(bool active);
    Q_INVOKABLE bool selectSongResult(const QString &key);
    QVariantMap settings()const;
    QVariantList response()const;
    QVariantList presets()const;
    QVariantList targets()const;
    QVariantMap target()const;
    QVariantMap active()const;
    QString targetKey()const{return m_targetKey;}
    QString currentSongKey()const{return m_songKey;}
    double headroom()const;
    int sampleRate()const{return m_sampleRate;}
    bool mediaAvailable()const{return audio::relaxer_peq_media_available()!=0;}
    QString status()const{return m_status;}
    Q_INVOKABLE bool selectTarget(const QString &key);
    Q_INVOKABLE bool createOverride();
    Q_INVOKABLE bool removeOverride();
    Q_INVOKABLE bool requestSong(const QString &key);
    Q_INVOKABLE bool requestOnlineSong(const QString &provider,const QVariantMap &song);
    Q_INVOKABLE bool requestPlaylist(const QString &key);
    Q_INVOKABLE bool requestPlatform(const QString &provider);
    Q_INVOKABLE bool setOption(const QString &key,const QVariant &value);
    Q_INVOKABLE bool setBand(int index,const QString &key,const QVariant &value);
    Q_INVOKABLE bool setBandPoint(int index,double frequency,double gain);
    Q_INVOKABLE void reset();
    Q_INVOKABLE bool usePreset(const QString &id);
    Q_INVOKABLE bool savePreset(const QString &name);
    Q_INVOKABLE bool removePreset(const QString &id);
    Q_INVOKABLE bool flush();
    bool unsavedChanges()const{return m_dirty;}
    bool hasUnsavedChanges()const{return m_dirty||!m_presetDraft.trimmed().isEmpty();}
    QString presetDraft()const{return m_presetDraft;}
    void setPresetDraft(const QString &text);
    Q_INVOKABLE bool saveChanges(){return flush();}
    Q_INVOKABLE bool saveAllChanges();
    Q_INVOKABLE void discardChanges();
    Q_INVOKABLE bool continueTargetChange();
    Q_INVOKABLE void cancelTargetChange();
signals:
    void changed();
    void statusChanged();
    void editorRequested();
    void songSearchChanged();
    void editStateChanged();
    void targetChangeConfirmationRequested();
private:
    Library *m_library;
    QPointer<PlaybackFacade> m_facade;
    QPointer<TrackListModel> m_source;
    audio::PeqConfig m_config,m_global,m_effective;
    QJsonArray m_presets;
    QJsonObject m_rules;
    QVariantList m_baseTargets;
    QVariantMap m_requestedTarget;
    QString m_targetKey="global",m_songKey,m_playlistKey,m_provider,m_trackTitle,m_effectiveKey="global";
    QTimer m_rateTimer;
    QJsonObject m_savedDocument;QString m_presetDraft;
    QVariantMap m_pendingTarget;bool m_pendingOpenEditor=false;
    int m_sampleRate=48000;
    bool m_dirty=false,m_saving=false,m_published=false;
    QString m_status;
    QString m_songQuery,m_songSearchMessage;
    QVariantList m_songResults;
    QHash<QString,QVariantMap> m_songMatches,m_liveCloud;
    QThreadPool m_songSearchPool;QTimer m_songSearchTimer;
    std::shared_ptr<std::atomic_bool> m_songSearchCancel;
    quint64 m_songSearchRevision=0;
    bool m_songSearchActive=false,m_songSearching=false;
    void scheduleSongSearch();
    void searchSongs();
    void load(const QJsonObject &data);
    void rebuildTargets();
    QString playlistKey(const QString &localKey)const;
    QString resolveKey(bool includeSong=true)const;
    QString inheritedKey(const QString &key)const;
    QVariantMap describe(const QString &key)const;
    audio::PeqConfig configuration(const QString &key)const;
    void refreshPlayback();
    void publishEffective();
    void update();
    void markDirty();
    QJsonObject document()const;
    bool requestTarget(const QVariantMap &row,bool openEditor);
    bool activateTarget(const QVariantMap &row,bool openEditor);
    bool editSong(const Track &track,const QString &parentKey={});
    bool openTarget(const QString &key);
    void message(const QString &text);
};
}
