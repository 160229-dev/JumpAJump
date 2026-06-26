#pragma once
#include <QObject>
#include <QIODevice>
#include <QAudioFormat>
#include <QByteArray>

class QAudioSink;

// 持续输出蓄力音(频率随时间上升)的 IO 设备
class ChargeGenerator : public QIODevice {
    Q_OBJECT
public:
    explicit ChargeGenerator(QObject *parent = nullptr);
    void start();
    void stop();
    qint64 readData(char *data, qint64 maxlen) override;
    qint64 writeData(const char *, qint64) override { return 0; }
private:
    bool m_active = false;
    double m_phase = 0.0;
    double m_elapsed = 0.0;
};

// 循环读取一段 PCM 数据的 IO 设备(用于背景音乐)
class LoopDevice : public QIODevice {
    Q_OBJECT
public:
    explicit LoopDevice(const QByteArray &data, QObject *parent = nullptr);
    qint64 readData(char *data, qint64 maxlen) override;
    qint64 writeData(const char *, qint64) override { return 0; }
private:
    QByteArray m_data;
    qint64 m_pos = 0;
};

class AudioEngine : public QObject {
    Q_OBJECT
public:
    explicit AudioEngine(QObject *parent = nullptr);
    ~AudioEngine();

    void startCharge();
    void stopCharge();
    void playJump();
    void playLand();
    void playPerfect();
    void playFail();
    void startBgm();
    void stopBgm();
    void setMuted(bool m);
    bool isMuted() const { return m_muted; }
    bool isAvailable() const { return m_ok; }

private:
    QByteArray genTone(double fStart, double fEnd, double duration, double amp, double decay = 3.0);
    QByteArray genChord();
    QByteArray genBgm();
    void playSfx(const QByteArray &pcm);

    bool m_ok = false;
    bool m_muted = false;
    QAudioFormat m_format;

    QAudioSink *m_chargeSink = nullptr;
    ChargeGenerator *m_chargeGen = nullptr;

    QAudioSink *m_sfxSink = nullptr;
    QIODevice *m_sfxDev = nullptr;

    QAudioSink *m_bgmSink = nullptr;
    LoopDevice *m_bgmLoop = nullptr;

    // 预生成的短音效
    QByteArray m_jumpPcm, m_landPcm, m_perfectPcm, m_failPcm;
};
