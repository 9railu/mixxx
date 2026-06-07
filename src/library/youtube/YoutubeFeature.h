#pragma once

#include <QAction>
#include <QPointer>
#include <QProcess>
#include <QProgressDialog>
#include <QSet>

#include "library/baseexternallibraryfeature.h"
#include "library/basetrackcache.h"
#include "library/treeitemmodel.h"
#include "library/youtube/YoutubeTrackModel.h"
#include "preferences/usersettings.h"
#include "track/trackid.h"
#include "util/parented_ptr.h"

class Library;

class YoutubeFeature : public BaseExternalLibraryFeature {
    Q_OBJECT
  public:
    YoutubeFeature(Library* pLibrary, UserSettingsPointer pConfig);
    ~YoutubeFeature() override;

    QVariant title() override;
    TreeItemModel* sidebarModel() const override;

  public slots:
    void activate() override;
    void activateChild(const QModelIndex& index) override;
    void onRightClick(const QPoint& globalPos) override;
    void onRightClickChild(const QPoint& globalPos, const QModelIndex& index) override;

  private:
    void createTable();
    void syncAnalysisResults(const QSet<TrackId>& trackIds);
    QString resolveDownloadQuery(const QString& input);
    QString ytdlpPath();
    QString downloadDir();
    void startDownload(const QString& url);
    void fetchYtdlp();
    void addTrackToDb(const QString& location, const QString& title,
                      const QString& artist, int durationSec,
                      const QString& url);
    void refreshSidebar();

    parented_ptr<TreeItemModel> m_pSidebarModel;
    QSharedPointer<BaseTrackCache> m_trackSource;
    YoutubeTrackModel* m_pTrackModel;

    parented_ptr<QAction> m_pDownloadAction;
    parented_ptr<QAction> m_pFetchYtdlpAction;
    parented_ptr<QAction> m_pSetYtdlpAction;
    parented_ptr<QAction> m_pSetDirAction;

    bool m_isDownloading = false;
    bool m_isFetchingYtdlp = false;

    // State kept across the two-process download sequence.
    QString m_pendingUrl;
    QString m_pendingVideoId;
    QString m_pendingTitle;
    QString m_pendingArtist;
    int m_pendingDuration = 0;
};
