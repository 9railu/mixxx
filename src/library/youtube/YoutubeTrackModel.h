#pragma once

#include "library/baseexternaltrackmodel.h"

// BaseExternalTrackModel subclass for downloaded YouTube tracks.
//
// Adds capabilities that are safe for an external-style table backed by a
// RAIDJ-managed SQLite table (as opposed to a foreign read-only database):
// analysis results and play counts live in Mixxx's own library, Properties is
// a read-only dialog, and the files are fully owned by RAIDJ so removal is safe.
class YoutubeTrackModel : public BaseExternalTrackModel {
    Q_OBJECT
  public:
    YoutubeTrackModel(QObject* parent,
            TrackCollectionManager* pTrackCollectionManager,
            const char* settingsNamespace,
            const QString& trackTable,
            QSharedPointer<BaseTrackCache> trackSource);

    Capabilities getCapabilities() const override;
};
