#pragma once

#include <QAction>
#include <QFuture>
#include <QFutureWatcher>
#include <QMenu>
#include <QPointer>
#include <QSqlDatabase>

#include "library/baseexternallibraryfeature.h"
#include "library/basetrackcache.h"
#include "library/rekordboxdirect/RekordboxDirectTrackModel.h"
#include "library/rekordboxdirect/RekordboxDirectReader.h"
#include "library/treeitemmodel.h"
#include "preferences/usersettings.h"
#include "util/parented_ptr.h"
#include "widget/wlibrarysidebar.h"

#ifdef RAIDJ_ESSENTIA
#include "analyzer/essentia/EssentiaAnalyzer.h"
#endif

class Library;

class RekordboxDirectFeature : public BaseExternalLibraryFeature {
    Q_OBJECT
  public:
    RekordboxDirectFeature(Library* pLibrary, UserSettingsPointer pConfig);
    ~RekordboxDirectFeature() override;

    QVariant title() override;
    static bool isSupported();
    TreeItemModel* sidebarModel() const override;

  public slots:
    void activate() override;
    void activateChild(const QModelIndex& index) override;
    void onRightClick(const QPoint& globalPos) override;
    void onRightClickChild(const QPoint& globalPos, const QModelIndex& index) override;
    void onTracksLoaded();
#ifdef RAIDJ_ESSENTIA
    void onEssentiaAnalysisDone();
#endif

  private:
    void createTable();
    void populateTable(const QList<RekordboxTrack>& tracks);
#ifdef RAIDJ_ESSENTIA
    void startEssentiaAnalysis();
#endif

    parented_ptr<TreeItemModel> m_pSidebarModel;
    QList<RekordboxTrack> m_tracks;
    bool m_isActivated = false;

    QFutureWatcher<QList<RekordboxTrack>> m_futureWatcher;
    QFuture<QList<RekordboxTrack>> m_future;

    QSharedPointer<BaseTrackCache> m_trackSource;
    RekordboxDirectTrackModel* m_pTrackModel;
    QHash<QString, QList<RekordboxCue>> m_cueMap;
    QHash<QString, int> m_durationMap;

#ifdef RAIDJ_ESSENTIA
    QHash<QString, EssentiaResult> m_essentiaResults;
    QFutureWatcher<QList<EssentiaResult>> m_essentiaWatcher;
    QFuture<QList<EssentiaResult>> m_essentiaFuture;
    bool m_essentiaAnalyzing = false;
#endif

    QString m_title;

#ifdef RAIDJ_ESSENTIA
    parented_ptr<QAction> m_pEssentiaAction;
#endif
};
