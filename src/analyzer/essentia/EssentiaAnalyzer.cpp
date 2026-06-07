#include "analyzer/essentia/EssentiaAnalyzer.h"

#include <essentia/essentia.h>
#include <essentia/algorithmfactory.h>
#include <essentia/pool.h>

#include <QFileInfo>

using namespace essentia;
using namespace essentia::standard;

void EssentiaAnalyzer::init() {
    essentia::init();
}

void EssentiaAnalyzer::shutdown() {
    essentia::shutdown();
}

EssentiaResult EssentiaAnalyzer::analyze(const QString& filePath) {
    EssentiaResult result;

    if (!QFileInfo::exists(filePath)) {
        result.error = QStringLiteral("File not found: ") + filePath;
        return result;
    }

    AlgorithmFactory& factory = AlgorithmFactory::instance();

    // --- Audio loader ---
    Algorithm* audioLoader = factory.create("MonoLoader",
            "filename", filePath.toStdString(),
            "sampleRate", 44100,
            "downmix", std::string("mix"));

    std::vector<Real> audio;
    audioLoader->output("audio").set(audio);
    audioLoader->compute();
    delete audioLoader;

    if (audio.empty()) {
        result.error = QStringLiteral("Failed to decode audio: ") + filePath;
        return result;
    }

    // --- BPM via RhythmExtractor2013 ---
    Algorithm* rhythmExtractor = factory.create("RhythmExtractor2013",
            "method", std::string("multifeature"));

    Real bpm;
    std::vector<Real> ticks, estimates, bpmIntervals;
    Real confidence;

    rhythmExtractor->input("signal").set(audio);
    rhythmExtractor->output("bpm").set(bpm);
    rhythmExtractor->output("ticks").set(ticks);
    rhythmExtractor->output("estimates").set(estimates);
    rhythmExtractor->output("bpmIntervals").set(bpmIntervals);
    rhythmExtractor->output("confidence").set(confidence);
    rhythmExtractor->compute();
    delete rhythmExtractor;

    result.bpm = static_cast<double>(bpm);

    // --- Key via KeyExtractor ---
    Algorithm* keyExtractor = factory.create("KeyExtractor",
            "profileType", std::string("temperley"));

    std::string key, scale;
    Real strength;

    keyExtractor->input("audio").set(audio);
    keyExtractor->output("key").set(key);
    keyExtractor->output("scale").set(scale);
    keyExtractor->output("strength").set(strength);
    keyExtractor->compute();
    delete keyExtractor;

    result.key = QString::fromStdString(key + " " + scale);
    result.keyStrength = static_cast<double>(strength);
    result.success = true;
    return result;
}
