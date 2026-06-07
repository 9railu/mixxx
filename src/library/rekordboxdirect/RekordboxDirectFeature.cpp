#include "library/rekordboxdirect/RekordboxDirectFeature.h"
#include "moc_RekordboxDirectFeature.cpp"

#include <QDir>
#include <QIcon>
#include <QMenu>
#include <QSqlQuery>
#include <QtConcurrentRun>

#include "library/library.h"
#include "library/rekordboxdirect/RekordboxDirectTrackModel.h"
#include "library/trackcollection.h"
#include "library/trackcollectionmanager.h"
#include "library/treeitem.h"
#include "library/treeitemmodel.h"
#include "util/parented_ptr.h"

namespace {
constexpr char kTableName[] = "rekordbox_direct_library";
} // anonymous namespace

RekordboxDirectFeature::RekordboxDirectFeature(
        Library* pLibrary, UserSettingsPointer pConfig)
        : BaseExternalLibraryFeature(pLibrary, pConfig, QStringLiteral("rekordboxdirect")),
          m_pSidebarModel(make_parented<TreeItemModel>(this)),
          m_pTrackModel(nullptr),
          m_title(tr("Rekordbox Collection")) {

    QStringList columns = {
            "id",
            "artist",
            "title",
            "album",
            "genre",
            "location",
            "bpm",
            "key",
            "duration",
            "rating"};
    // Note: cues_display is a view-only placeholder column handled by
    // RekordboxDirectTrackModel; it must not be in the BaseTrackCache columns.
    QStringList searchColumns = {"artist", "title", "album", "genre", "location"};

    createTable();

    m_trackSource = QSharedPointer<BaseTrackCache>::create(
            m_pTrackCollection,
            QLatin1String(kTableName),
            QStringLiteral("id"),
            columns,
            searchColumns,
            false);

    m_pTrackModel = new RekordboxDirectTrackModel(
            this,
            pLibrary->trackCollectionManager(),
            "mixxx.db.model.rekordboxdirect",
            QLatin1String(kTableName),
            m_trackSource);
    // Provide initial (empty) cue data so the model is fully initialised.
    m_pTrackModel->setCueData({}, {});
    m_pTrackModel->setSearch("");

#ifdef RAIDJ_ESSENTIA
    m_pEssentiaAction = make_parented<QAction>(
            tr("Essentiaで全曲のBPM/Keyを解析"), this);
    connect(m_pEssentiaAction,
            &QAction::triggered,
            this,
            &RekordboxDirectFeature::startEssentiaAnalysis);
#endif

    connect(&m_futureWatcher,
            &QFutureWatcher<QList<RekordboxTrack>>::finished,
            this,
            &RekordboxDirectFeature::onTracksLoaded);

#ifdef RAIDJ_ESSENTIA
    connect(&m_essentiaWatcher,
            &QFutureWatcher<QList<EssentiaResult>>::finished,
            this,
            &RekordboxDirectFeature::onEssentiaAnalysisDone);
#endif

    auto pRootItem = TreeItem::newRoot(this);
    pRootItem->appendChild(tr("Loading..."));
    m_pSidebarModel->setRootItem(std::move(pRootItem));
}

RekordboxDirectFeature::~RekordboxDirectFeature() {
    m_future.cancel();
    m_futureWatcher.waitForFinished();
#ifdef RAIDJ_ESSENTIA
    m_essentiaFuture.cancel();
    m_essentiaWatcher.waitForFinished();
#endif
    delete m_pTrackModel;
}

QVariant RekordboxDirectFeature::title() {
    return m_title;
}

bool RekordboxDirectFeature::isSupported() {
    const QString appdata = qEnvironmentVariable("APPDATA");
    return !appdata.isEmpty() &&
            QFile::exists(appdata + QStringLiteral("/Pioneer/rekordbox/master.db"));
}

TreeItemModel* RekordboxDirectFeature::sidebarModel() const {
    return m_pSidebarModel;
}

void RekordboxDirectFeature::createTable() {
    QSqlDatabase db = m_pTrackCollection->database();
    QSqlQuery q(db);
    q.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS rekordbox_direct_library ("
            "  id           TEXT PRIMARY KEY,"
            "  artist       TEXT,"
            "  title        TEXT,"
            "  album        TEXT,"
            "  genre        TEXT,"
            "  location     TEXT,"
            "  bpm          REAL,"
            "  key          TEXT,"
            "  duration     INTEGER,"
            "  rating       INTEGER,"
            "  cues_display TEXT"
            ")"));
    // Migration for existing DBs that don't yet have cues_display.
    // This will silently fail (and be ignored) if the column already exists.
    q.exec(QStringLiteral(
            "ALTER TABLE rekordbox_direct_library ADD COLUMN cues_display TEXT"));
}

void RekordboxDirectFeature::populateTable(const QList<RekordboxTrack>& tracks) {
    QSqlDatabase db = m_pTrackCollection->database();
    QSqlQuery q(db);

    db.transaction();
    q.exec(QStringLiteral("DELETE FROM rekordbox_direct_library"));

    q.prepare(QStringLiteral(
            "INSERT INTO rekordbox_direct_library "
            "(id, artist, title, album, genre, location, bpm, key, duration, rating) "
            "VALUES (:id,:artist,:title,:album,:genre,:location,:bpm,:key,:duration,:rating)"));

    for (const RekordboxTrack& t : tracks) {
        q.bindValue(":id", t.id);
        q.bindValue(":artist", t.artist);
        q.bindValue(":title", t.title);
        q.bindValue(":album", t.album);
        q.bindValue(":genre", t.genre);
        q.bindValue(":location", t.filePath);
        q.bindValue(":bpm", t.bpm > 0 ? t.bpm : QVariant());
        q.bindValue(":key", t.key);
        q.bindValue(":duration", t.durationSec);
        q.bindValue(":rating", t.rating);
        q.exec();
    }
    db.commit();
}

void RekordboxDirectFeature::activate() {
    if (!m_isActivated) {
        m_isActivated = true;
        m_future = QtConcurrent::run([this]() {
            return RekordboxDirectReader::readAllTracks();
        });
        m_futureWatcher.setFuture(m_future);
        emit featureIsLoading(this, true);
    }
    emit showTrackModel(m_pTrackModel);
    emit enableCoverArtDisplay(false);
}

void RekordboxDirectFeature::activateChild(const QModelIndex& index) {
    Q_UNUSED(index);
    activate();
}

void RekordboxDirectFeature::onRightClickChild(
        const QPoint& globalPos, const QModelIndex& index) {
    Q_UNUSED(index);
    onRightClick(globalPos);
}

void RekordboxDirectFeature::onRightClick(const QPoint& globalPos) {
#ifdef RAIDJ_ESSENTIA
    m_pEssentiaAction->setEnabled(!m_tracks.isEmpty() && !m_essentiaAnalyzing);
    if (m_essentiaAnalyzing) {
        m_pEssentiaAction->setText(tr("Essentia解析中..."));
    } else if (!m_essentiaResults.isEmpty()) {
        m_pEssentiaAction->setText(tr("Essentiaで再解析（全曲）"));
    } else {
        m_pEssentiaAction->setText(tr("Essentiaで全曲のBPM/Keyを解析"));
    }
    QMenu menu;
    menu.addAction(m_pEssentiaAction);
    menu.exec(globalPos);
#else
    Q_UNUSED(globalPos);
#endif
}

void RekordboxDirectFeature::onTracksLoaded() {
    m_tracks = m_futureWatcher.result();

    populateTable(m_tracks);

    // Build normalised-path maps for hot cue import (getTrack) and cue bar display.
    m_cueMap.clear();
    m_durationMap.clear();
    for (const RekordboxTrack& t : m_tracks) {
        const QString normalizedPath = QDir::fromNativeSeparators(t.filePath);
        if (!t.cues.isEmpty()) {
            m_cueMap[normalizedPath] = t.cues;
        }
        if (t.durationSec > 0) {
            m_durationMap[normalizedPath] = t.durationSec;
        }
    }
    m_pTrackModel->setCueData(m_cueMap, m_durationMap);

    auto pRootItem = TreeItem::newRoot(this);
    pRootItem->appendChild(tr("%1 tracks").arg(m_tracks.size()));
    m_pSidebarModel->setRootItem(std::move(pRootItem));
    emit featureIsLoading(this, false);

    m_pTrackModel->select();
    emit showTrackModel(m_pTrackModel);
    // showTrackModel triggers loadTrackModel → delegateForColumn → new delegate.
    // Re-push data so the freshly created delegate already has cue info.
    m_pTrackModel->setCueData(m_cueMap, m_durationMap);
}

#ifdef RAIDJ_ESSENTIA
void RekordboxDirectFeature::startEssentiaAnalysis() {
    if (m_essentiaAnalyzing || m_tracks.isEmpty()) {
        return;
    }
    m_essentiaAnalyzing = true;

    QList<RekordboxTrack> tracks = m_tracks;
    m_essentiaFuture = QtConcurrent::run([tracks]() {
        QList<EssentiaResult> results;
        results.reserve(tracks.size());
        for (const RekordboxTrack& t : tracks) {
            results.append(EssentiaAnalyzer::analyze(t.filePath));
        }
        return results;
    });
    m_essentiaWatcher.setFuture(m_essentiaFuture);
}

void RekordboxDirectFeature::onEssentiaAnalysisDone() {
    m_essentiaAnalyzing = false;
    const QList<EssentiaResult> results = m_essentiaWatcher.result();
    for (int i = 0; i < m_tracks.size() && i < results.size(); ++i) {
        m_essentiaResults[m_tracks[i].id] = results[i];
    }

    // Essentia結果をDBに書き戻す（BPMとKey）
    QSqlDatabase db = m_pTrackCollection->database();
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
            "UPDATE rekordbox_direct_library SET bpm=:bpm, key=:key WHERE id=:id"));
    db.transaction();
    for (int i = 0; i < m_tracks.size() && i < results.size(); ++i) {
        const EssentiaResult& r = results[i];
        if (r.success) {
            q.bindValue(":bpm", r.bpm > 0 ? QVariant(r.bpm) : QVariant());
            q.bindValue(":key", r.key.isEmpty() ? QVariant() : QVariant(r.key));
            q.bindValue(":id", m_tracks[i].id);
            q.exec();
        }
    }
    db.commit();

    m_pTrackModel->select();
    emit showTrackModel(m_pTrackModel);
    // showTrackModel() may have recreated the delegate; push cue data again.
    m_pTrackModel->setCueData(m_cueMap, m_durationMap);
}
#endif
