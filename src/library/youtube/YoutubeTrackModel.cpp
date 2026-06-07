#include "library/youtube/YoutubeTrackModel.h"
#include "moc_YoutubeTrackModel.cpp"

YoutubeTrackModel::YoutubeTrackModel(
        QObject* parent,
        TrackCollectionManager* pTrackCollectionManager,
        const char* settingsNamespace,
        const QString& trackTable,
        QSharedPointer<BaseTrackCache> trackSource)
        : BaseExternalTrackModel(parent,
                  pTrackCollectionManager,
                  settingsNamespace,
                  trackTable,
                  trackSource) {
}

TrackModel::Capabilities YoutubeTrackModel::getCapabilities() const {
    // EditMetadata routes through Track::setXxx() into Mixxx's own library DB,
    // so it's safe here. It also gates Analyze/Reset/BPM/Color in WTrackMenu
    // (AND-combined check with EditMetadata).
    return BaseExternalTrackModel::getCapabilities() |
            Capability::EditMetadata |
            Capability::Analyze |
            Capability::Properties |
            Capability::ResetPlayed;
}
