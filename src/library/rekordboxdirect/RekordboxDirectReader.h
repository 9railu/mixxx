#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QString>

// Represents a single hot cue imported from Rekordbox.
struct RekordboxCue {
    int id;
    double inMsec;
    double outMsec;  // -1 if not a loop
    int kind;        // hot cue slot number (1-8)
    int color;       // ARGB, -1 if none
    QString comment;
    bool isHotCue;
};

// Represents a track imported from Rekordbox master.db.
struct RekordboxTrack {
    QString id;
    QString title;
    QString artist;
    QString album;
    QString genre;
    double bpm;
    QString key;
    int durationSec;
    QString filePath;
    int rating;
    QList<RekordboxCue> cues;
};

// Reads Rekordbox master.db via rekordbox_export.py (pyrekordbox bridge).
// All operations are synchronous and must be called from a worker thread.
class RekordboxDirectReader {
  public:
    // Returns all tracks. Empty list on error.
    static QList<RekordboxTrack> readAllTracks(const QString& dbPath = QString());

    // Returns a single track by Rekordbox content ID, or null on error.
    static bool readSingleTrack(
            const QString& contentId,
            RekordboxTrack* pOut,
            const QString& dbPath = QString());

    static QString lastError();

  private:
    static QList<RekordboxTrack> parseTracksJson(const QJsonObject& root);
    static RekordboxTrack parseTrackJson(const QJsonObject& obj);
    static RekordboxCue parseCueJson(const QJsonObject& obj);
    static QString findPythonScript();
    static QString runScript(const QStringList& args, QString* pError);

    static QString s_lastError;
};
