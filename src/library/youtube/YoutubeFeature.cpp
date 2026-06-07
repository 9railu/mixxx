#include "library/youtube/YoutubeFeature.h"
#include "moc_YoutubeFeature.cpp"

#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QIcon>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QMessageBox>
#include <QProcess>
#include <QProgressDialog>
#include <QSqlQuery>
#include <QStandardPaths>

#include "library/library.h"
#include "library/trackcollection.h"
#include "library/trackcollectionmanager.h"
#include "library/treeitem.h"
#include "library/treeitemmodel.h"
#include "track/globaltrackcache.h"
#include "track/track.h"
#include "util/parented_ptr.h"

namespace {
constexpr char kTableName[] = "youtube_tracks";
const ConfigKey kYtdlpPathKey("[RAIDJ]", "ytdlp_path");
const ConfigKey kDownloadDirKey("[RAIDJ]", "youtube_dl_dir");
} // anonymous namespace

YoutubeFeature::YoutubeFeature(Library* pLibrary, UserSettingsPointer pConfig)
        : BaseExternalLibraryFeature(pLibrary, pConfig, QStringLiteral("youtube")),
          m_pSidebarModel(make_parented<TreeItemModel>(this)),
          m_pTrackModel(nullptr) {

    const QStringList columns = {
            "id", "artist", "title", "album", "genre",
            "location", "bpm", "key", "duration", "rating"};
    const QStringList searchColumns = {"artist", "title", "location"};

    createTable();

    m_trackSource = QSharedPointer<BaseTrackCache>::create(
            m_pTrackCollection,
            QLatin1String(kTableName),
            QStringLiteral("id"),
            columns,
            searchColumns,
            false);

    m_pTrackModel = new YoutubeTrackModel(
            this,
            pLibrary->trackCollectionManager(),
            "mixxx.db.model.youtube",
            QLatin1String(kTableName),
            m_trackSource);
    m_pTrackModel->setSearch("");

    m_pDownloadAction = make_parented<QAction>(tr("URL/検索ワードでダウンロード..."), this);
    connect(m_pDownloadAction, &QAction::triggered, this, [this] {
        if (m_isDownloading) {
            return;
        }
        const QString input = QInputDialog::getText(
                nullptr,
                tr("YouTubeダウンロード"),
                tr("YouTube の URL、または検索ワードを入力してください:"),
                QLineEdit::Normal);
        const QString trimmed = input.trimmed();
        if (trimmed.isEmpty()) {
            return;
        }
        startDownload(resolveDownloadQuery(trimmed));
    });

    m_pFetchYtdlpAction = make_parented<QAction>(tr("yt-dlp を自動ダウンロード..."), this);
    connect(m_pFetchYtdlpAction, &QAction::triggered, this, [this] {
        fetchYtdlp();
    });

    m_pSetYtdlpAction = make_parented<QAction>(tr("yt-dlp のパスを手動設定..."), this);
    connect(m_pSetYtdlpAction, &QAction::triggered, this, [this] {
        const QString path = QFileDialog::getOpenFileName(
                nullptr,
                tr("yt-dlp を選択"),
                QString(),
                tr("実行ファイル (*.exe);;すべてのファイル (*)"));
        if (!path.isEmpty()) {
            m_pConfig->set(kYtdlpPathKey, ConfigValue(path));
        }
    });

    m_pSetDirAction = make_parented<QAction>(tr("ダウンロードフォルダを設定..."), this);
    connect(m_pSetDirAction, &QAction::triggered, this, [this] {
        const QString dir = QFileDialog::getExistingDirectory(
                nullptr,
                tr("ダウンロードフォルダを選択"),
                downloadDir());
        if (!dir.isEmpty()) {
            m_pConfig->set(kDownloadDirKey, ConfigValue(dir));
        }
    });

    refreshSidebar();

    // Mixxx's standard analyzer (BPM/Key) writes results into its own internal
    // library table, not into youtube_tracks (which this model's BaseTrackCache
    // queries directly for display). Without this, the BPM/Key columns would
    // stay blank forever after analysis, regardless of which view (YouTube
    // sidebar or the main library) the user triggered "Analyze" from. Mirror
    // results back into youtube_tracks whenever any track changes.
    connect(m_pTrackCollection,
            &TrackCollection::tracksChanged,
            this,
            &YoutubeFeature::syncAnalysisResults);
}

YoutubeFeature::~YoutubeFeature() {
    delete m_pTrackModel;
}

QVariant YoutubeFeature::title() {
    return tr("YouTube");
}

TreeItemModel* YoutubeFeature::sidebarModel() const {
    return m_pSidebarModel;
}

void YoutubeFeature::activate() {
    emit showTrackModel(m_pTrackModel);
    emit enableCoverArtDisplay(false);
}

void YoutubeFeature::activateChild(const QModelIndex& index) {
    Q_UNUSED(index);
    activate();
}

void YoutubeFeature::onRightClickChild(
        const QPoint& globalPos, const QModelIndex& index) {
    Q_UNUSED(index);
    onRightClick(globalPos);
}

void YoutubeFeature::onRightClick(const QPoint& globalPos) {
    const bool ytdlpReady = !ytdlpPath().isEmpty() && QFile::exists(ytdlpPath());

    m_pDownloadAction->setEnabled(!m_isDownloading && ytdlpReady);
    m_pDownloadAction->setText(m_isDownloading ? tr("ダウンロード中...") : tr("URLをダウンロード..."));

    m_pFetchYtdlpAction->setEnabled(!m_isFetchingYtdlp);
    if (m_isFetchingYtdlp) {
        m_pFetchYtdlpAction->setText(tr("yt-dlp 取得中..."));
    } else if (ytdlpReady) {
        m_pFetchYtdlpAction->setText(tr("yt-dlp を再ダウンロード（最新版）"));
    } else {
        m_pFetchYtdlpAction->setText(tr("yt-dlp を自動ダウンロード"));
    }

    QMenu menu;
    menu.addAction(m_pDownloadAction);
    menu.addSeparator();
    menu.addAction(m_pFetchYtdlpAction);
    menu.addAction(m_pSetYtdlpAction);
    menu.addAction(m_pSetDirAction);
    menu.exec(globalPos);
}

void YoutubeFeature::syncAnalysisResults(const QSet<TrackId>& trackIds) {
    if (trackIds.isEmpty()) {
        return;
    }

    // Read straight from the live in-memory Track objects rather than the
    // `library` table: TrackDAO persists changes lazily (on eviction/exit),
    // so a freshly analyzed bpm/key may not be visible in the database yet
    // when this signal fires. The GlobalTrackCache always has the current
    // values for any track that is loaded.
    bool anyUpdated = false;
    {
        GlobalTrackCacheLocker cacheLocker;
        for (const TrackId& id : trackIds) {
            TrackPointer pTrack = cacheLocker.lookupTrackById(id);
            if (!pTrack) {
                continue;
            }
            const double bpm = pTrack->getBpm();
            const QString keyText = pTrack->getKeyText();

            QStringList setClauses;
            if (bpm > 0) {
                setClauses << QStringLiteral("bpm = :bpm");
            }
            if (!keyText.isEmpty()) {
                setClauses << QStringLiteral("key = :key");
            }
            if (setClauses.isEmpty()) {
                continue;
            }

            QSqlQuery q(m_pTrackCollection->database());
            q.prepare(QStringLiteral("UPDATE youtube_tracks SET ") +
                    setClauses.join(QStringLiteral(", ")) +
                    QStringLiteral(" WHERE location = :location"));
            if (bpm > 0) {
                q.bindValue(QStringLiteral(":bpm"), bpm);
            }
            if (!keyText.isEmpty()) {
                q.bindValue(QStringLiteral(":key"), keyText);
            }
            q.bindValue(QStringLiteral(":location"), pTrack->getLocation());
            if (q.exec() && q.numRowsAffected() > 0) {
                anyUpdated = true;
            }
        }
    }

    if (anyUpdated) {
        m_pTrackModel->select();
    }
}

void YoutubeFeature::createTable() {
    QSqlQuery q(m_pTrackCollection->database());
    q.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS youtube_tracks ("
            "  id       INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  location TEXT UNIQUE,"
            "  title    TEXT,"
            "  artist   TEXT,"
            "  album    TEXT DEFAULT '',"
            "  genre    TEXT DEFAULT '',"
            "  bpm      REAL,"
            "  key      TEXT,"
            "  duration INTEGER DEFAULT 0,"
            "  rating   INTEGER DEFAULT 0,"
            "  url      TEXT"
            ")"));
}

QString YoutubeFeature::ytdlpPath() {
    return m_pConfig->getValue(kYtdlpPathKey, QString());
}

QString YoutubeFeature::downloadDir() {
    const QString saved = m_pConfig->getValue(kDownloadDirKey, QString());
    if (!saved.isEmpty()) {
        return saved;
    }
    const QString music = QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
    return music + QStringLiteral("/RAIDJ YouTube");
}

void YoutubeFeature::fetchYtdlp() {
    if (m_isFetchingYtdlp) {
        return;
    }
    m_isFetchingYtdlp = true;

    const QString dir = downloadDir();
    QDir().mkpath(dir);
    const QString savePath = dir + QStringLiteral("/yt-dlp.exe");

    // Use the Windows-built-in curl.exe (available since Windows 10 1803).
    // It handles HTTPS/redirects natively, avoiding Qt TLS setup issues.
    const QString curlPath = QStringLiteral("C:/Windows/System32/curl.exe");
    if (!QFile::exists(curlPath)) {
        m_isFetchingYtdlp = false;
        QMessageBox::warning(nullptr, tr("curl が見つかりません"),
                tr("curl.exe が見つかりませんでした。\n"
                   "ブラウザから yt-dlp.exe をダウンロードして\n"
                   "「yt-dlp のパスを手動設定」から登録してください。"));
        return;
    }

    auto* progress = new QProgressDialog(
            tr("yt-dlp をダウンロード中..."), tr("キャンセル"), 0, 0);
    progress->setWindowTitle(tr("yt-dlp 取得"));
    progress->setWindowModality(Qt::ApplicationModal);
    progress->setMinimumDuration(0);
    progress->setValue(0);
    progress->show();

    auto* proc = new QProcess(this);

    connect(progress, &QProgressDialog::canceled, proc, [proc] {
        proc->kill();
    });

    connect(proc, &QProcess::finished, this,
            [this, proc, progress, savePath](int exitCode, QProcess::ExitStatus) {
                progress->deleteLater();
                proc->deleteLater();
                m_isFetchingYtdlp = false;

                if (exitCode != 0) {
                    // Killed by cancel or real error.
                    if (QFile::exists(savePath) && QFileInfo(savePath).size() == 0) {
                        QFile::remove(savePath);
                    }
                    if (exitCode != -1) { // -1 = killed (cancel)
                        QMessageBox::warning(nullptr, tr("yt-dlp ダウンロードエラー"),
                                tr("ダウンロードに失敗しました（curl 終了コード %1）。")
                                        .arg(exitCode));
                    }
                    return;
                }

                if (!QFile::exists(savePath) || QFileInfo(savePath).size() == 0) {
                    QMessageBox::warning(nullptr, tr("yt-dlp エラー"),
                            tr("ファイルの保存に失敗しました:\n%1").arg(savePath));
                    return;
                }

                m_pConfig->set(kYtdlpPathKey, ConfigValue(savePath));
                QMessageBox::information(nullptr, tr("yt-dlp 取得完了"),
                        tr("yt-dlp を取得しました:\n%1").arg(savePath));
            });

    proc->start(curlPath,
            {QStringLiteral("-L"), // follow redirects
             QStringLiteral("--output"), savePath,
             QStringLiteral("https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp.exe")});
}

QString YoutubeFeature::resolveDownloadQuery(const QString& input) {
    // If the input already looks like a URL, pass it through untouched.
    // Otherwise treat it as a search keyword and let yt-dlp grab the top result.
    if (input.startsWith(QStringLiteral("http://"), Qt::CaseInsensitive) ||
            input.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive) ||
            input.startsWith(QStringLiteral("ytsearch"), Qt::CaseInsensitive)) {
        return input;
    }
    return QStringLiteral("ytsearch1:") + input;
}

void YoutubeFeature::startDownload(const QString& url) {
    // Ensure yt-dlp is configured.
    QString ytdlp = ytdlpPath();
    if (ytdlp.isEmpty() || !QFile::exists(ytdlp)) {
        ytdlp = QFileDialog::getOpenFileName(
                nullptr,
                tr("yt-dlp を選択してください"),
                QString(),
                tr("実行ファイル (*.exe);;すべてのファイル (*)"));
        if (ytdlp.isEmpty()) {
            return;
        }
        m_pConfig->set(kYtdlpPathKey, ConfigValue(ytdlp));
    }

    m_isDownloading = true;
    m_pendingUrl = url;
    m_pendingVideoId.clear();
    m_pendingTitle.clear();
    m_pendingArtist.clear();
    m_pendingDuration = 0;

    // Step 1: Fetch metadata without downloading.
    auto* proc = new QProcess(this);
    connect(proc, &QProcess::finished, this,
            [this, proc, ytdlp](int exitCode, QProcess::ExitStatus) {
                proc->deleteLater();

                if (exitCode != 0) {
                    m_isDownloading = false;
                    const QString errOut = proc->readAllStandardError().trimmed();
                    QMessageBox::warning(nullptr, tr("yt-dlp エラー"),
                            tr("メタデータの取得に失敗しました。\nURLを確認してください。\n\n%1")
                                    .arg(errOut.left(400)));
                    return;
                }

                const QJsonDocument doc =
                        QJsonDocument::fromJson(proc->readAllStandardOutput());
                QJsonObject obj = doc.object();

                // ytsearch queries return a playlist wrapper with "entries".
                // Pick the first result and use it as the actual track.
                if (obj.value("_type").toString() == QLatin1String("playlist")) {
                    const QJsonArray entries = obj.value("entries").toArray();
                    if (entries.isEmpty()) {
                        m_isDownloading = false;
                        QMessageBox::warning(nullptr, tr("yt-dlp エラー"),
                                tr("検索結果が見つかりませんでした。"));
                        return;
                    }
                    obj = entries.first().toObject();
                }

                m_pendingVideoId = obj.value("id").toString();
                m_pendingTitle = obj.value("title").toString();
                m_pendingArtist = obj.value("uploader").toString();
                m_pendingDuration = static_cast<int>(obj.value("duration").toDouble());

                if (m_pendingVideoId.isEmpty()) {
                    m_isDownloading = false;
                    QMessageBox::warning(nullptr, tr("yt-dlp エラー"),
                            tr("動画IDを取得できませんでした。"));
                    return;
                }

                // Resolve to the concrete video URL so the download step fetches
                // exactly the video we just inspected (search results can change).
                const QString webpageUrl = obj.value("webpage_url").toString();
                m_pendingUrl = !webpageUrl.isEmpty()
                        ? webpageUrl
                        : QStringLiteral("https://www.youtube.com/watch?v=") + m_pendingVideoId;

                // Step 2: Download audio.
                const QString dir = downloadDir();
                QDir().mkpath(dir);

                auto* dlProc = new QProcess(this);
                connect(dlProc, &QProcess::finished, this,
                        [this, dlProc, dir](int dlExit, QProcess::ExitStatus) {
                            dlProc->deleteLater();
                            m_isDownloading = false;

                            if (dlExit != 0) {
                                const QString errOut =
                                        dlProc->readAllStandardError().trimmed();
                                QMessageBox::warning(nullptr, tr("yt-dlp エラー"),
                                        tr("ダウンロードに失敗しました。\n\n%1")
                                                .arg(errOut.left(400)));
                                return;
                            }

                            // Search for the downloaded file (extension varies by format).
                            const QStringList exts = {
                                    "m4a", "webm", "opus", "ogg", "mp3", "aac", "flac"};
                            QString location;
                            for (const QString& ext : exts) {
                                const QString candidate = dir + QStringLiteral("/") +
                                        m_pendingVideoId + QStringLiteral(".") + ext;
                                if (QFile::exists(candidate)) {
                                    location = candidate;
                                    break;
                                }
                            }
                            if (location.isEmpty()) {
                                QMessageBox::warning(nullptr, tr("yt-dlp"),
                                        tr("ダウンロードされたファイルが見つかりません。\n"
                                           "フォルダ: %1").arg(dir));
                                return;
                            }

                            addTrackToDb(location,
                                    m_pendingTitle,
                                    m_pendingArtist,
                                    m_pendingDuration,
                                    m_pendingUrl);

                            m_pTrackModel->select();
                            refreshSidebar();
                            emit showTrackModel(m_pTrackModel);

                            QMessageBox::information(nullptr, tr("ダウンロード完了"),
                                    tr("「%1」のダウンロードが完了しました。").arg(m_pendingTitle));
                        });

                // m4a (AAC) is preferred: no ffmpeg needed, fast seek, Mixxx-compatible.
                // 140 = YouTube's m4a 128kbps stream (very widely available).
                // Fall back to best available m4a, then any best audio.
                dlProc->start(ytdlp,
                        {"--no-playlist",
                         "--no-part",
                         "-f", "140/bestaudio[ext=m4a]/bestaudio",
                         "-o", dir + "/%(id)s.%(ext)s",
                         m_pendingUrl});
            });

    proc->start(ytdlp, {"--no-playlist", "--dump-single-json", url});
}

void YoutubeFeature::addTrackToDb(const QString& location,
        const QString& title,
        const QString& artist,
        int durationSec,
        const QString& url) {
    QSqlQuery q(m_pTrackCollection->database());
    q.prepare(QStringLiteral(
            "INSERT OR REPLACE INTO youtube_tracks"
            " (location, title, artist, duration, url)"
            " VALUES (:location, :title, :artist, :duration, :url)"));
    q.bindValue(":location", location);
    q.bindValue(":title", title.isEmpty() ? QFileInfo(location).baseName() : title);
    q.bindValue(":artist", artist);
    q.bindValue(":duration", durationSec);
    q.bindValue(":url", url);
    q.exec();
}

void YoutubeFeature::refreshSidebar() {
    QSqlQuery q(m_pTrackCollection->database());
    q.exec(QStringLiteral("SELECT COUNT(*) FROM youtube_tracks"));
    int count = 0;
    if (q.next()) {
        count = q.value(0).toInt();
    }
    auto pRoot = TreeItem::newRoot(this);
    pRoot->appendChild(tr("%1 曲").arg(count));
    m_pSidebarModel->setRootItem(std::move(pRoot));
}
