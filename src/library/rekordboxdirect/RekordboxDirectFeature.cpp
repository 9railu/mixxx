#include "library/rekordboxdirect/RekordboxDirectFeature.h"
#include "moc_RekordboxDirectFeature.cpp"

#include <QIcon>
#include <QtConcurrentRun>

#include "library/library.h"
#include "library/treeitem.h"
#include "library/treeitemmodel.h"
#include "util/parented_ptr.h"
#include "widget/wlibrary.h"
#include "widget/wlibrarytextbrowser.h"

namespace {
constexpr char kViewName[] = "REKORDBOXDIRECTHOME";
} // anonymous namespace

RekordboxDirectFeature::RekordboxDirectFeature(
        Library* pLibrary, UserSettingsPointer pConfig)
        : LibraryFeature(pLibrary, pConfig, QStringLiteral("rekordboxdirect")),
          m_pSidebarModel(make_parented<TreeItemModel>(this)),
          m_title(tr("Rekordbox Collection")) {
    auto pRootItem = TreeItem::newRoot(this);
    pRootItem->appendChild(tr("Loading..."));
    m_pSidebarModel->setRootItem(std::move(pRootItem));

    connect(&m_futureWatcher,
            &QFutureWatcher<QList<RekordboxTrack>>::finished,
            this,
            &RekordboxDirectFeature::onTracksLoaded);
}

RekordboxDirectFeature::~RekordboxDirectFeature() {
    m_future.cancel();
    m_futureWatcher.waitForFinished();
}

QVariant RekordboxDirectFeature::title() {
    return m_title;
}

bool RekordboxDirectFeature::isSupported() {
    // Available when pyrekordbox is installed and master.db exists.
    const QString appdata = qEnvironmentVariable("APPDATA");
    return !appdata.isEmpty() &&
            QFile::exists(appdata + QStringLiteral("/Pioneer/rekordbox/master.db"));
}

TreeItemModel* RekordboxDirectFeature::sidebarModel() const {
    return m_pSidebarModel;
}

void RekordboxDirectFeature::bindLibraryWidget(
        WLibrary* libraryWidget, KeyboardEventFilter* keyboard) {
    Q_UNUSED(keyboard);
    parented_ptr<WLibraryTextBrowser> pBrowser =
            make_parented<WLibraryTextBrowser>(libraryWidget);
    pBrowser->setHtml(QStringLiteral(
            "<h2>Rekordbox Collection</h2>"
            "<p>Loading tracks from Rekordbox database...</p>"));
    m_pBrowser = pBrowser.get();
    libraryWidget->registerView(QLatin1String(kViewName), pBrowser);
}

void RekordboxDirectFeature::activate() {
    emit switchToView(QLatin1String(kViewName));

    if (!m_futureWatcher.isRunning() && m_tracks.isEmpty()) {
        m_future = QtConcurrent::run([this]() {
            return RekordboxDirectReader::readAllTracks();
        });
        m_futureWatcher.setFuture(m_future);
    }
}

void RekordboxDirectFeature::activateChild(const QModelIndex& index) {
    Q_UNUSED(index);
    activate();
}

void RekordboxDirectFeature::onTracksLoaded() {
    m_tracks = m_futureWatcher.result();

    // Update sidebar count.
    auto pRootItem = TreeItem::newRoot(this);
    pRootItem->appendChild(tr("%1 tracks").arg(m_tracks.size()));
    m_pSidebarModel->setRootItem(std::move(pRootItem));
    emit featureIsLoading(this, false);

    // Update HTML view with track list.
    showTracks(m_tracks);
}

void RekordboxDirectFeature::showTracks(const QList<RekordboxTrack>& tracks) {
    if (m_pBrowser) {
        m_pBrowser->setHtml(formatHtmlView(tracks));
    }
    emit switchToView(QLatin1String(kViewName));
}

QString RekordboxDirectFeature::formatHtmlView(
        const QList<RekordboxTrack>& tracks) const {
    if (tracks.isEmpty()) {
        const QString err = RekordboxDirectReader::lastError();
        return QStringLiteral(
                       "<h2>Rekordbox Collection</h2>"
                       "<p style='color:red'>Failed to load: %1</p>")
                .arg(err.isEmpty() ? tr("No tracks found") : err);
    }

    QString html = QStringLiteral(
            "<h2>Rekordbox Collection</h2>"
            "<p>%1 tracks loaded.</p>"
            "<table border='0' cellspacing='4'>"
            "<tr><th align='left'>Title</th><th>Artist</th>"
            "<th>BPM</th><th>Key</th><th>Cues</th></tr>")
                           .arg(tracks.size());

    for (const RekordboxTrack& t : tracks) {
        html += QStringLiteral(
                        "<tr>"
                        "<td>%1</td><td>%2</td>"
                        "<td align='right'>%3</td><td>%4</td><td align='right'>%5</td>"
                        "</tr>")
                        .arg(t.title.toHtmlEscaped(),
                                t.artist.toHtmlEscaped(),
                                t.bpm > 0 ? QString::number(t.bpm, 'f', 1)
                                          : QStringLiteral("-"),
                                t.key.toHtmlEscaped(),
                                QString::number(t.cues.size()));
    }
    html += QStringLiteral("</table>");
    return html;
}
