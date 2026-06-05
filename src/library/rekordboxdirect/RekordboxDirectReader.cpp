#include "library/rekordboxdirect/RekordboxDirectReader.h"

#include <QCoreApplication>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>

QString RekordboxDirectReader::s_lastError;

QString RekordboxDirectReader::lastError() {
    return s_lastError;
}

QString RekordboxDirectReader::findPythonScript() {
    // Look for rekordbox_export.py relative to the application binary.
    QDir appDir(QCoreApplication::applicationDirPath());
    const QString relPath = QStringLiteral("tools/rekordbox_export.py");

    // Traverse up to find the repo root (dev build layout).
    QDir dir = appDir;
    for (int i = 0; i < 5; ++i) {
        if (dir.exists(relPath)) {
            return dir.filePath(relPath);
        }
        if (!dir.cdUp()) {
            break;
        }
    }
    return QString();
}

QString RekordboxDirectReader::runScript(const QStringList& args, QString* pError) {
    const QString scriptPath = findPythonScript();
    if (scriptPath.isEmpty()) {
        if (pError) {
            *pError = QStringLiteral("rekordbox_export.py not found");
        }
        return QString();
    }

    QProcess process;
    process.setProgram(QStringLiteral("python"));
    process.setArguments(QStringList{scriptPath} + args);
    process.start();

    if (!process.waitForFinished(30000)) {
        if (pError) {
            *pError = QStringLiteral("rekordbox_export.py timed out");
        }
        process.kill();
        return QString();
    }

    if (process.exitCode() != 0) {
        if (pError) {
            *pError = QString::fromUtf8(process.readAllStandardError());
        }
        return QString();
    }

    // pyrekordbox outputs a debug `{}` line before the real JSON.
    // Find the line that starts with the actual JSON payload.
    const QByteArray output = process.readAllStandardOutput();
    for (const QByteArray& line : output.split('\n')) {
        const QByteArray trimmed = line.trimmed();
        if (trimmed.startsWith('{') && trimmed.size() > 2) {
            return QString::fromUtf8(trimmed);
        }
    }

    if (pError) {
        *pError = QStringLiteral("No valid JSON in rekordbox_export.py output");
    }
    return QString();
}

RekordboxCue RekordboxDirectReader::parseCueJson(const QJsonObject& obj) {
    RekordboxCue cue;
    cue.id = obj.value(QLatin1String("id")).toInt();
    cue.inMsec = obj.value(QLatin1String("in_msec")).toDouble();
    cue.outMsec = obj.value(QLatin1String("out_msec")).toDouble(-1.0);
    cue.kind = obj.value(QLatin1String("kind")).toInt();
    cue.color = obj.value(QLatin1String("color")).toInt(-1);
    cue.comment = obj.value(QLatin1String("comment")).toString();
    cue.isHotCue = obj.value(QLatin1String("is_hot_cue")).toBool();
    return cue;
}

RekordboxTrack RekordboxDirectReader::parseTrackJson(const QJsonObject& obj) {
    RekordboxTrack track;
    track.id = obj.value(QLatin1String("id")).toVariant().toString();
    track.title = obj.value(QLatin1String("title")).toString();
    track.artist = obj.value(QLatin1String("artist")).toString();
    track.album = obj.value(QLatin1String("album")).toString();
    track.genre = obj.value(QLatin1String("genre")).toString();
    track.bpm = obj.value(QLatin1String("bpm")).toDouble();
    track.key = obj.value(QLatin1String("key")).toString();
    track.durationSec = obj.value(QLatin1String("duration_sec")).toInt();
    track.filePath = obj.value(QLatin1String("file_path")).toString();
    track.rating = obj.value(QLatin1String("rating")).toInt();

    const QJsonArray cueArray = obj.value(QLatin1String("cues")).toArray();
    for (const QJsonValue& v : cueArray) {
        track.cues.append(parseCueJson(v.toObject()));
    }
    return track;
}

QList<RekordboxTrack> RekordboxDirectReader::parseTracksJson(const QJsonObject& root) {
    QList<RekordboxTrack> tracks;
    const QJsonArray arr = root.value(QLatin1String("tracks")).toArray();
    tracks.reserve(arr.size());
    for (const QJsonValue& v : arr) {
        tracks.append(parseTrackJson(v.toObject()));
    }
    return tracks;
}

QList<RekordboxTrack> RekordboxDirectReader::readAllTracks(const QString& dbPath) {
    QStringList args;
    if (!dbPath.isEmpty()) {
        args << QStringLiteral("--db-path") << dbPath;
    }

    QString error;
    const QString json = runScript(args, &error);
    if (json.isEmpty()) {
        s_lastError = error;
        return {};
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        s_lastError = parseError.errorString();
        return {};
    }

    const QJsonObject root = doc.object();
    if (root.contains(QLatin1String("error"))) {
        s_lastError = root.value(QLatin1String("error")).toString();
        return {};
    }

    s_lastError.clear();
    return parseTracksJson(root);
}

bool RekordboxDirectReader::readSingleTrack(
        const QString& contentId, RekordboxTrack* pOut, const QString& dbPath) {
    QStringList args;
    args << QStringLiteral("--content-id") << contentId;
    if (!dbPath.isEmpty()) {
        args << QStringLiteral("--db-path") << dbPath;
    }

    QString error;
    const QString json = runScript(args, &error);
    if (json.isEmpty()) {
        s_lastError = error;
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        s_lastError = parseError.errorString();
        return false;
    }

    const QJsonObject root = doc.object();
    if (root.contains(QLatin1String("error"))) {
        s_lastError = root.value(QLatin1String("error")).toString();
        return false;
    }

    s_lastError.clear();
    *pOut = parseTrackJson(root);
    return true;
}
