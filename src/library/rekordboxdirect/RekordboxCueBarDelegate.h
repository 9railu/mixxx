#pragma once

#include <QHash>
#include <QList>
#include <QStyledItemDelegate>

#include "library/rekordboxdirect/RekordboxDirectReader.h"

class TrackModel;

// Draws colored vertical cue-position bars in the Rekordbox Collection table.
class RekordboxCueBarDelegate : public QStyledItemDelegate {
    Q_OBJECT
  public:
    RekordboxCueBarDelegate(TrackModel* pTrackModel, QObject* parent);

    void setCueData(const QHash<QString, QList<RekordboxCue>>& cueMap,
            const QHash<QString, int>& durationMap);

    void paint(QPainter* painter,
               const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override;

  private:
    TrackModel* m_pTrackModel;
    QHash<QString, QList<RekordboxCue>> m_cueMap;
    QHash<QString, int> m_durationMap;
};
