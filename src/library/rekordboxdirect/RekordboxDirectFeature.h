#pragma once

#include <QFuture>
#include <QFutureWatcher>
#include <QStandardItemModel>

#include "library/libraryfeature.h"
#include "library/rekordboxdirect/RekordboxDirectReader.h"
#include "library/treeitemmodel.h"
#include "preferences/usersettings.h"
#include "util/parented_ptr.h"
#include "widget/wlibrarytextbrowser.h"

class Library;
class WLibrary;
class KeyboardEventFilter;

// Displays tracks from the locally installed Rekordbox collection (master.db).
// Reads via rekordbox_export.py (pyrekordbox bridge). Hot cues are imported
// into Mixxx's cue system when a track is loaded from this feature.
class RekordboxDirectFeature : public LibraryFeature {
    Q_OBJECT
  public:
    RekordboxDirectFeature(Library* pLibrary, UserSettingsPointer pConfig);
    ~RekordboxDirectFeature() override;

    QVariant title() override;
    static bool isSupported();

    void bindLibraryWidget(
            WLibrary* libraryWidget, KeyboardEventFilter* keyboard) override;
    TreeItemModel* sidebarModel() const override;

  public slots:
    void activate() override;
    void activateChild(const QModelIndex& index) override;
    void onTracksLoaded();

  private:
    QList<RekordboxTrack> loadTracksAsync();
    void showTracks(const QList<RekordboxTrack>& tracks);
    QString formatHtmlView(const QList<RekordboxTrack>& tracks) const;

    parented_ptr<TreeItemModel> m_pSidebarModel;
    QList<RekordboxTrack> m_tracks;

    QFutureWatcher<QList<RekordboxTrack>> m_futureWatcher;
    QFuture<QList<RekordboxTrack>> m_future;

    QPointer<WLibraryTextBrowser> m_pBrowser;
    QString m_title;
};
