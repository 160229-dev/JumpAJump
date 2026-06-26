#pragma once
#include <QWidget>
#include <QPointF>
#include <QColor>
#include <QTimer>
#include <QElapsedTimer>
#include <QVector>
#include <QPainterPath>

class AudioEngine;

struct Platform {
    QPointF pos;        // 世界坐标(平台顶面中心)
    float width = 80;   // 顶面横向尺寸
    float depth = 80;   // 顶面纵向尺寸(等距压缩前)
    float height = 50;  // 平台厚度(z方向,屏幕像素)
    QColor topColor;
    QColor sideColor;
    QColor accentColor;     // 木纹/装饰色
    enum Shape { Cylinder, Box } shape = Cylinder;
    int seed = 0;            // 用于稳定的纹理细节
    bool perfectHit = false; // 该平台是否已被完美命中
};

struct Particle {
    QPointF pos;     // 屏幕坐标
    QPointF vel;
    float life = 0;
    float maxLife = 0;
    float size = 2;
    QColor color;
};

enum class GameState { Ready, Idle, Charging, Jumping, Falling, GameOver };

class GameWidget : public QWidget {
    Q_OBJECT
public:
    explicit GameWidget(QWidget *parent = nullptr);
    ~GameWidget() override;

protected:
    void paintEvent(QPaintEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
    void keyReleaseEvent(QKeyEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void resizeEvent(QResizeEvent *) override;

private slots:
    void tick();

private:
    QPointF worldToScreen(const QPointF &w) const;
    float   worldToScreenY(const QPointF &w, float z) const;

    void resetGame();
    void generateNextPlatform();
    void startCharge();
    void releaseJump();
    void onLand();
    void spawnDust(const QPointF &screenPos, int count, float power);
    void spawnRipple(const QPointF &screenPos, const QColor &c);
    void updateParticles(float dt);
    void shakeCamera(float amount);

    // 绘制
    void drawBackground(QPainter &p);
    void drawGround(QPainter &p);
    void drawPlatform(QPainter &p, const Platform &pl);
    void drawPlayer(QPainter &p);
    void drawShadow(QPainter &p);
    void drawTrail(QPainter &p);
    void drawParticles(QPainter &p);
    void drawPerfectFx(QPainter &p);
    void drawHUD(QPainter &p);
    void drawReadyScreen(QPainter &p);

    AudioEngine *m_audio = nullptr;
    QTimer m_timer;
    QElapsedTimer m_clock;
    qint64 m_lastNs = 0;

    GameState m_state = GameState::Ready;

    Platform m_current;
    Platform m_next;
    QVector<Platform> m_history;

    // 玩家
    QPointF m_playerPos;
    float   m_playerZ = 0;
    float   m_playerScaleX = 1.0f;
    float   m_playerScaleY = 1.0f;
    float   m_playerRot = 0.0f;
    float   m_playerSquash = 0;      // 落地瞬间的额外压扁动画
    QPointF m_playerTrail[6];        // 残影位置(屏幕坐标)
    float   m_playerTrailZ[6] = {0};
    int     m_trailIdx = 0;

    // 相机
    QPointF m_camera;
    QPointF m_cameraTarget;
    QPointF m_shake;                 // 抖动偏移
    float   m_shakeAmount = 0;

    // 蓄力
    QElapsedTimer m_chargeTimer;
    double m_chargeTime = 0.0;
    const double m_maxCharge = 1.4;
    bool m_keyHeld = false;

    // 跳跃
    QPointF m_jumpStart;
    QPointF m_jumpEnd;
    float   m_jumpPeak = 0;
    double  m_jumpDuration = 0;
    double  m_jumpElapsed = 0;

    // 完美落地特效
    float   m_perfectFx = 0.0f;

    // 粒子
    QVector<Particle> m_particles;
    struct Ripple { QPointF pos; float r; float maxR; QColor color; };
    QVector<Ripple> m_ripples;

    // 评分
    int m_score = 0;
    int m_combo = 0;
    int m_bestScore = 0;
};
