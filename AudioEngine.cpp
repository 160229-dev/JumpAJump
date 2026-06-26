#include "AudioEngine.h"
#include <QAudioSink>
#include <QMediaDevices>
#include <QBuffer>
#include <cmath>
#include <cstring>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static const int kSampleRate = 44100;

// ---------------- ChargeGenerator ----------------
ChargeGenerator::ChargeGenerator(QObject *parent) : QIODevice(parent)
{
    open(QIODevice::ReadOnly);
}

void ChargeGenerator::start()
{
    m_active = true;
    m_elapsed = 0.0;
    m_phase = 0.0;
}

void ChargeGenerator::stop()
{
    m_active = false;
}

qint64 ChargeGenerator::readData(char *data, qint64 maxlen)
{
    if (!m_active) {
        std::memset(data, 0, size_t(maxlen));
        return maxlen;
    }
    qint64 frames = maxlen / 2;
    qint16 *out = reinterpret_cast<qint16 *>(data);
    for (qint64 i = 0; i < frames; ++i) {
        m_elapsed += 1.0 / kSampleRate;
        // 频率从 220Hz 上升到 1000Hz
        double freq = 220.0 + std::min(m_elapsed * 520.0, 780.0);
        m_phase += 2.0 * M_PI * freq / kSampleRate;
        // 基波 + 二次谐波 + 三次谐波,叠加轻微颤音
        const double vib = 1.0 + 0.015 * std::sin(m_elapsed * 28.0);
        double s  = std::sin(m_phase * vib);
        s += 0.40 * std::sin(2.0 * m_phase);
        s += 0.18 * std::sin(3.0 * m_phase);
        s /= 1.58;
        // 起音包络
        double amp = 0.34;
        if (m_elapsed < 0.05) amp *= m_elapsed / 0.05;
        out[i] = qint16(amp * 32767.0 * s);
    }
    return frames * 2;
}

// ---------------- LoopDevice ----------------
LoopDevice::LoopDevice(const QByteArray &data, QObject *parent)
    : QIODevice(parent), m_data(data)
{
    open(QIODevice::ReadOnly);
}

qint64 LoopDevice::readData(char *data, qint64 maxlen)
{
    qint64 total = 0;
    while (total < maxlen) {
        qint64 remain = m_data.size() - m_pos;
        qint64 toRead = std::min(maxlen - total, remain);
        if (toRead > 0) {
            std::memcpy(data + total, m_data.constData() + m_pos, size_t(toRead));
            m_pos += toRead;
            total += toRead;
        }
        if (m_pos >= m_data.size()) m_pos = 0;
    }
    return total;
}

// ---------------- AudioEngine ----------------
AudioEngine::AudioEngine(QObject *parent) : QObject(parent)
{
    m_format.setSampleRate(kSampleRate);
    m_format.setChannelCount(1);
    m_format.setSampleFormat(QAudioFormat::Int16);

    const auto dev = QMediaDevices::defaultAudioOutput();
    if (dev.isNull()) {
        m_ok = false;
        return;
    }

    m_chargeSink = new QAudioSink(dev, m_format, this);
    m_chargeGen = new ChargeGenerator(this);

    m_sfxSink = new QAudioSink(dev, m_format, this);

    m_bgmSink = new QAudioSink(dev, m_format, this);
    m_bgmSink->setVolume(0.4);

    // 预生成短音效(更丰富的音色)
    m_jumpPcm    = genTone(440, 980, 0.13, 0.50, 6.0);
    m_landPcm    = genTone(240, 100, 0.11, 0.55, 8.0);
    m_perfectPcm = genChord();
    m_failPcm    = genTone(320, 70, 0.60, 0.55, 1.4);

    // 背景音乐
    const QByteArray bgm = genBgm();
    m_bgmLoop = new LoopDevice(bgm, this);

    m_ok = true;
}

AudioEngine::~AudioEngine() = default;

QByteArray AudioEngine::genTone(double fStart, double fEnd, double duration, double amp, double decay)
{
    QByteArray out;
    const int total = int(kSampleRate * duration);
    out.resize(total * 2);
    qint16 *p = reinterpret_cast<qint16 *>(out.data());
    double phase = 0.0;
    for (int i = 0; i < total; ++i) {
        const double t = double(i) / kSampleRate;
        const double ratio = duration > 0 ? t / duration : 0.0;
        const double freq = fStart + (fEnd - fStart) * ratio;
        phase += 2.0 * M_PI * freq / kSampleRate;
        const double env = std::exp(-decay * t);
        p[i] = qint16(amp * 32767.0 * env * std::sin(phase));
    }
    return out;
}

QByteArray AudioEngine::genChord()
{
    // 完美命中: 琶音和弦 C-E-G-C,明亮悦耳
    static const double freqs[] = {523.25, 659.25, 783.99, 1046.50};
    const int n = int(sizeof(freqs) / sizeof(double));
    const double total = 0.45;
    const int totalSamples = int(kSampleRate * total);
    QByteArray out;
    out.resize(totalSamples * 2);
    qint16 *p = reinterpret_cast<qint16 *>(out.data());
    std::memset(out.data(), 0, out.size());
    const double step = total / n;
    for (int k = 0; k < n; ++k) {
        const int start = int(kSampleRate * step * k);
        const int len = totalSamples - start;
        double phase = 0.0;
        for (int i = 0; i < len; ++i) {
            const double t = double(i) / kSampleRate;
            phase += 2.0 * M_PI * freqs[k] / kSampleRate;
            const double env = (1.0 - std::exp(-90.0 * t)) * std::exp(-2.2 * t);
            const double s = std::sin(phase) + 0.30 * std::sin(2.0 * phase);
            p[start + i] = qint16(std::clamp(p[start + i] + 0.18 * 32767.0 * env * s / 1.30,
                                              -32767.0, 32767.0));
        }
    }
    return out;
}

QByteArray AudioEngine::genBgm()
{
    // 多声部: 旋律(分解和弦) + 低音(根音) + 轻微和声
    // C - Am - F - G  经典进行
    static const double melody[] = {523.25, 659.25, 783.99, 659.25,
                                    523.25, 659.25, 783.99, 1046.50,
                                    440.00, 523.25, 659.25, 523.25,
                                    440.00, 523.25, 659.25, 880.00,
                                    349.23, 440.00, 523.25, 440.00,
                                    349.23, 440.00, 523.25, 698.46,
                                    392.00, 493.88, 587.33, 493.88,
                                    392.00, 493.88, 587.33, 783.99};
    static const double bass[] = {130.81, 130.81, 130.81, 130.81,
                                  110.00, 110.00, 110.00, 110.00,
                                  87.31,  87.31,  87.31,  87.31,
                                  87.31,  87.31,  87.31,  87.31,
                                  174.61, 174.61, 174.61, 174.61,
                                  174.61, 174.61, 174.61, 174.61,
                                  196.00, 196.00, 196.00, 196.00,
                                  196.00, 196.00, 196.00, 196.00};
    const int noteCount = int(sizeof(melody) / sizeof(double));
    const double noteDur = 0.42;
    const int totalSamples = int(kSampleRate * noteDur * noteCount);
    QByteArray out;
    out.resize(totalSamples * 2);
    qint16 *p = reinterpret_cast<qint16 *>(out.data());

    double mPhase = 0.0, bPhase = 0.0, lastM = melody[0], lastB = bass[0];
    for (int i = 0; i < totalSamples; ++i) {
        const double t = double(i) / kSampleRate;
        const int noteIdx = int(t / noteDur) % noteCount;
        const double mf = melody[noteIdx];
        const double bf = bass[noteIdx];
        if (mf != lastM) { mPhase = 0.0; lastM = mf; }
        if (bf != lastB) { bPhase = 0.0; lastB = bf; }
        const double localT = t - int(t / noteDur) * noteDur;
        const double env = (1.0 - std::exp(-80.0 * localT)) * std::exp(-2.4 * localT);
        mPhase += 2.0 * M_PI * mf / kSampleRate;
        bPhase += 2.0 * M_PI * bf / kSampleRate;
        const double mel = std::sin(mPhase) + 0.30 * std::sin(2.0 * mPhase);
        // 低音包络更慢,音量更小
        const double bEnv = (1.0 - std::exp(-30.0 * localT)) * std::exp(-1.5 * localT);
        const double bs = std::sin(bPhase) + 0.20 * std::sin(2.0 * bPhase);
        const double s = mel / 1.30 * env * 0.16 + bs / 1.20 * bEnv * 0.10;
        p[i] = qint16(32767.0 * s);
    }
    return out;
}

void AudioEngine::playSfx(const QByteArray &pcm)
{
    if (!m_ok || m_muted || !m_sfxSink) return;
    m_sfxSink->stop();
    delete m_sfxDev;
    auto *buf = new QBuffer;
    buf->setData(pcm);
    buf->open(QIODevice::ReadOnly);
    m_sfxDev = buf;
    m_sfxSink->start(m_sfxDev);
}

void AudioEngine::startCharge()
{
    if (!m_ok || m_muted || !m_chargeSink) return;
    m_chargeGen->start();
    m_chargeSink->start(m_chargeGen);
}

void AudioEngine::stopCharge()
{
    if (!m_chargeSink) return;
    m_chargeGen->stop();
    m_chargeSink->stop();
}

void AudioEngine::playJump()    { playSfx(m_jumpPcm); }
void AudioEngine::playLand()    { playSfx(m_landPcm); }
void AudioEngine::playPerfect() { playSfx(m_perfectPcm); }
void AudioEngine::playFail()    { playSfx(m_failPcm); }

void AudioEngine::startBgm()
{
    if (!m_ok || m_muted || !m_bgmSink || !m_bgmLoop) return;
    m_bgmSink->start(m_bgmLoop);
}

void AudioEngine::stopBgm()
{
    if (!m_bgmSink) return;
    m_bgmSink->stop();
}

void AudioEngine::setMuted(bool m)
{
    m_muted = m;
    if (m_muted) {
        stopCharge();
        if (m_bgmSink) m_bgmSink->stop();
        if (m_sfxSink) m_sfxSink->stop();
    } else {
        startBgm();
    }
}
