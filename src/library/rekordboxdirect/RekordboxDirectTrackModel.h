#pragma once

#include <QHash>
#include <QList>
#include <QPointer>

#include "library/baseexternaltrackmodel.h"
#include "library/basetrackcache.h"
#include "library/rekordboxdirect/RekordboxCueBarDelegate.h"
#include "library/rekordboxdirect/RekordboxDirectReader.h"

// BaseExternalTrackModel subclass for Rekordbox Direct Collection.
//
// Extends the standard external track view with:
//   - Hot cue import when a track is loaded to a deck (getTrack override)
//   - A custom "Cues" column showing coloured cue-position bars
class RekordboxDirectTrackModel : public BaseExternalTrackModel {
    Q_OBJECT
  public:
    RekordboxDirectTrackModel(QObject* parent,
            TrackCollectionManager* pTrackCollectionManager,
            const char* settingsNamespace,
            const QString& trackTable,
            QSharedPointer<BaseTrackCache> trackSource);

    // Sets cue + duration data used by both getTrack() and the cue bar delegate.
    void setCueData(const QHash<QString, QList<RekordboxCue>>& cueMap,
                    const QHash<QString, int>& durationMap);

    TrackPointer getTrack(const QModelIndex& index) const override;
    Capabilities getCapabilities() const override;

  protected:
    QAbstractItemDelegate* additionalDelegateForColumn(
            int column, QObject* pParent) override;

  private:
    QHash<QString, QList<RekordboxCue>> m_cueMap;
    QHash<QString, int> m_durationMap;
    QPointer<RekordboxCueBarDelegate> m_pCueDelegate;
};
