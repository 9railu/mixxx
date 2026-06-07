#include "library/rekordboxdirect/RekordboxDirectTrackModel.h"
#include "moc_RekordboxDirectTrackModel.cpp"

#include <QDir>
#include <QSqlQuery>
#include <QtDebug>

#include "audio/types.h"
#include "library/columncache.h"
#include "track/cue.h"
#include "track/track.h"

RekordboxDirectTrackModel::RekordboxDirectTrackModel(
        QObject* parent,
        TrackCollectionManager* pTrackCollectionManager,
        const char* settingsNamespace,
        const QString& trackTable,
        QSharedPointer<BaseTrackCache> trackSource)
        : BaseExternalTrackModel(parent, pTrackCollectionManager, settingsNamespace,
                  trackTable, trackSource) {
    // BaseExternalTrackModel creates the SQL view with {id, preview} only.
    // Recreate it to include cues_display so the cue bar delegate column is
    // accessible via fieldIndex("cues_display").
    const QString viewTable = trackTable + QStringLiteral("_view");
    QSqlQuery q(m_database);
    q.exec(QStringLiteral("DROP VIEW IF EXISTS ") + viewTable);
    q.exec(QStringLiteral("CREATE TEMPORARY VIEW IF NOT EXISTS ") + viewTable +
           QStringLiteral(" AS SELECT id, '' AS preview, '' AS cues_display FROM ") +
           trackTable);
    setTable(viewTable,
             QStringLiteral("id"),
             {QStringLiteral("id"),
              QStringLiteral("preview"),
              QStringLiteral("cues_display")},
             trackSource);
    setDefaultSort(fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_ARTIST), Qt::AscendingOrder);

    // Set the header label for the cue-position bar column.
    const int cueCol = fieldIndex(QStringLiteral("cues_display"));
    if (cueCol >= 0) {
        setHeaderData(cueCol, Qt::Horizontal, tr("Cues"), Qt::DisplayRole);
    }
}

void RekordboxDirectTrackModel::setCueData(
        const QHash<QString, QList<RekordboxCue>>& cueMap,
        const QHash<QString, int>& durationMap) {
    m_cueMap = cueMap;
    m_durationMap = durationMap;
    if (m_pCueDelegate) {
        m_pCueDelegate->setCueData(cueMap, durationMap);
    }
}

QAbstractItemDelegate* RekordboxDirectTrackModel::additionalDelegateForColumn(
        int column, QObject* pParent) {
    if (column == fieldIndex(QStringLiteral("cues_display"))) {
        auto* pDelegate = new RekordboxCueBarDelegate(this, pParent);
        if (!m_cueMap.isEmpty()) {
            pDelegate->setCueData(m_cueMap, m_durationMap);
        }
        m_pCueDelegate = pDelegate;
        return pDelegate;
    }
    return nullptr;
}

TrackModel::Capabilities RekordboxDirectTrackModel::getCapabilities() const {
    // EditMetadata routes through Track::setXxx() into Mixxx's own library DB
    // (not the foreign Rekordbox database), so it's safe to enable here. It
    // also gates Analyze/Reset/BPM/Color in WTrackMenu (AND-combined check).
    return BaseExternalTrackModel::getCapabilities() |
            Capability::EditMetadata |
            Capability::Analyze |
            Capability::Properties |
            Capability::ResetPlayed;
}

TrackPointer RekordboxDirectTrackModel::getTrack(const QModelIndex& index) const {
    TrackPointer pTrack = BaseExternalTrackModel::getTrack(index);
    if (!pTrack) {
        return pTrack;
    }

    // Mixxx normalises paths with forward slashes; Rekordbox uses backslashes.
    const QString location = QDir::fromNativeSeparators(pTrack->getLocation());
    if (!m_cueMap.contains(location)) {
        qDebug() << "[RekordboxDirect] cueMap miss:" << location
                 << "mapSize=" << m_cueMap.size();
        return pTrack;
    }

    // Don't overwrite hot cues the user may have already set.
    for (const CuePointer& c : pTrack->getCuePoints()) {
        if (c->getHotCue() >= 0) {
            return pTrack;
        }
    }

    const QList<RekordboxCue>& cues = m_cueMap[location];
    if (cues.isEmpty()) {
        return pTrack;
    }

    double sampleRate = static_cast<double>(pTrack->getSampleRate());
    if (sampleRate <= 0.0) {
        sampleRate = 44100.0;
    }
    const double msToFrames = sampleRate / 1000.0;

    for (const RekordboxCue& rbCue : cues) {
        if (!rbCue.isHotCue && rbCue.kind <= 0) {
            continue;
        }
        const auto startPos = mixxx::audio::FramePos(rbCue.inMsec * msToFrames);
        mixxx::audio::FramePos endPos;
        if (rbCue.outMsec >= 0.0) {
            endPos = mixxx::audio::FramePos(rbCue.outMsec * msToFrames);
        }
        const mixxx::CueType type =
                endPos.isValid() ? mixxx::CueType::Loop : mixxx::CueType::HotCue;
        const int hotCueId = rbCue.kind - 1; // kind 1-8 → Mixxx 0-based

        CuePointer pCue = pTrack->createAndAddCue(type, hotCueId, startPos, endPos);
        if (pCue) {
            pCue->setLabel(rbCue.comment);
            if (rbCue.color >= 0) {
                pCue->setColor(mixxx::RgbColor(rbCue.color & 0x00FFFFFF));
            }
        }
    }

    return pTrack;
}
