#include "library/rekordboxdirect/RekordboxCueBarDelegate.h"
#include "moc_RekordboxCueBarDelegate.cpp"

#include <QDir>
#include <QPainter>
#include <QPen>
#include <QPolygon>

#include "library/trackmodel.h"

namespace {
// Default hot cue colors (slots A–H), used when the cue has no color set.
const QColor kDefaultCueColors[8] = {
    QColor(0xEF, 0x3C, 0x33), // A: Red
    QColor(0xF8, 0x90, 0x40), // B: Orange
    QColor(0xF7, 0xE0, 0x33), // C: Yellow
    QColor(0x2D, 0xC6, 0x3E), // D: Green
    QColor(0x17, 0xB3, 0xC0), // E: Cyan
    QColor(0x1F, 0x5C, 0xE6), // F: Blue
    QColor(0x8B, 0x25, 0xD4), // G: Purple
    QColor(0xEF, 0x2D, 0x89), // H: Pink
};
} // namespace

RekordboxCueBarDelegate::RekordboxCueBarDelegate(TrackModel* pTrackModel, QObject* parent)
        : QStyledItemDelegate(parent), m_pTrackModel(pTrackModel) {
}

void RekordboxCueBarDelegate::setCueData(
        const QHash<QString, QList<RekordboxCue>>& cueMap,
        const QHash<QString, int>& durationMap) {
    m_cueMap = cueMap;
    m_durationMap = durationMap;
}

void RekordboxCueBarDelegate::paint(QPainter* painter,
        const QStyleOptionViewItem& option, const QModelIndex& index) const {
    // Draw standard cell background (handles selected / hover state).
    QStyledItemDelegate::paint(painter, option, index);

    if (!m_pTrackModel || m_cueMap.isEmpty()) {
        return;
    }

    const QString location = m_pTrackModel->getTrackLocation(index);
    if (location.isEmpty()) {
        return;
    }

    const int durationSec = m_durationMap.value(location, 0);
    if (durationSec <= 0) {
        return;
    }

    const QList<RekordboxCue>& cues = m_cueMap.value(location);
    if (cues.isEmpty()) {
        return;
    }

    const QRect& r = option.rect;
    const int margin = 3;
    const int left = r.left() + margin;
    const int right = r.right() - margin;
    const int usableWidth = right - left;
    if (usableWidth <= 0) {
        return;
    }

    painter->save();
    painter->setClipRect(r);
    painter->setRenderHint(QPainter::Antialiasing, false);

    // Thin gray base line representing the full track duration.
    const int midY = r.top() + r.height() / 2;
    painter->setPen(QPen(QColor(120, 120, 120, 140), 1));
    painter->drawLine(left, midY, right, midY);

    for (const RekordboxCue& cue : cues) {
        if (!cue.isHotCue && cue.kind <= 0) {
            continue;
        }
        const double relPos =
                (cue.inMsec / 1000.0) / static_cast<double>(durationSec);
        if (relPos < 0.0 || relPos > 1.0) {
            continue;
        }

        const int x = left + static_cast<int>(relPos * usableWidth);

        QColor color;
        if (cue.color >= 0) {
            const int rgb = cue.color & 0x00FFFFFF;
            color = QColor((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
        } else {
            const int idx = cue.kind - 1;
            color = (idx >= 0 && idx < 8) ? kDefaultCueColors[idx] : QColor(Qt::white);
        }

        // Vertical marker line spanning the full cell height.
        painter->setPen(QPen(color, 2));
        painter->drawLine(x, r.top() + 2, x, r.bottom() - 2);

        // Small filled triangle at the top edge.
        painter->setPen(Qt::NoPen);
        painter->setBrush(color);
        const QPoint pts[3] = {
            QPoint(x - 3, r.top() + 2),
            QPoint(x + 3, r.top() + 2),
            QPoint(x,     r.top() + 7),
        };
        painter->drawPolygon(pts, 3);
    }

    painter->restore();
}

QSize RekordboxCueBarDelegate::sizeHint(
        const QStyleOptionViewItem& option, const QModelIndex& index) const {
    Q_UNUSED(index);
    return QSize(130, option.fontMetrics.height() + 6);
}
