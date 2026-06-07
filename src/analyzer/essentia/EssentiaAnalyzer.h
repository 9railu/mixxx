#pragma once

#include <QString>

struct EssentiaResult {
    double bpm = 0.0;
    QString key;        // e.g. "A major", "C# minor"
    double keyStrength = 0.0;
    bool success = false;
    QString error;
};

// Synchronous Essentia BPM + Key analysis for a single audio file.
// Must be called from a worker thread (not the UI thread).
class EssentiaAnalyzer {
  public:
    // Initialize Essentia library (call once at startup).
    static void init();
    // Shut down Essentia library (call once at shutdown).
    static void shutdown();

    static EssentiaResult analyze(const QString& filePath);
};
