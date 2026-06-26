#include "GameWidget.h"
#include "AudioEngine.h"

#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QConicalGradient>
#include <QFont>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QRandomGenerator>
#include <QSettings>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {
constexpr float kProj = 0.55f;
constexpr float kMaxJumpDist = 400.0f;
constexpr float kMinPlatformDist = 150.0f;
constexpr float kMaxPlatformDist = 340.0f;

struct Palette { QColor top; QColor side; QColor accent; };
const Palette kPalette[] = {
    { QColor(244,246,251), QColor(206,210,224), QColor(228,232,244) },
    { QColor(243,228,238), QColor(203,178,202), QColor(228,208,222) },
    { QColor(226,238,241), QColor(176,204,214), QColor(206,224,230) },
    { QColor(244,233,213), QColor(214,196,168), QColor(232,220,196) },
    { QColor(228,238,222), QColor(184,206,178), QColor(208,224,200) },
    { QColor(238,228,228), QColor(202,182,182), QColor(226,212,212) },
    { QColor(247,240,224), QColor(215,200,170), QColor(235,226,200) },
    { QColor(232,224,244), QColor(192,184,214), QColor(218,212,232) },
};
const int kPaletteSize = int(sizeof(kPalette) / sizeof(Palette));

// 伪随机(基于种子,保证每帧绘制一致)
float rnd(int seed, int i) {
    unsigned int v = unsigned(seed * 2654435761u + i * 2246822519u);
    v ^= v >> 13; v *= 1597334677u; v ^= v >> 15;
    return float(v & 0xFFFF) / 65535.0f;
}

float lerp(float a, float b, float t) { return a + (b - a) * t; }
float damp(float a, float b, float t) { return a + (b - a) * (1.0f - std::exp(-t)); }
float ease(float t) { return t * t * (3.0f - 2.0f * t); }
}

GameWidget::GameWidget(QWidget *parent) : QWidget(parent)
{
    setWindowTitle("JumpAJump");
    setMinimumSize(420, 720);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_OpaquePaintEvent);

    m_audio = new AudioEngine(this);
    if (m_audio->isAvailable()) m_audio->startBgm();

    // 读取历史最高分
    QSettings s("JumpAJump", "JumpAJump");
    m_bestScore = s.value("best", 0).toInt();

    resetGame();

    m_clock.start();
    m_lastNs = m_clock.nsecsElapsed();
    connect(&m_timer, &QTimer::timeout, this, &GameWidget::tick);
    m_timer.start(16);
}

GameWidget::~GameWidget()
{
    QSettings s("JumpAJump", "JumpAJump");
    s.setValue("best", m_bestScore);
}

void GameWidget::resetGame()
{
    m_history.clear();
    m_score = 0;
    m_combo = 0;
    m_playerScaleX = 1.0f;
    m_playerScaleY = 1.0f;
    m_playerRot = 0.0f;
    m_playerSquash = 0;
    m_perfectFx = 0.0f;
    m_chargeTime = 0.0;
    m_keyHeld = false;
    m_particles.clear();
    m_ripples.clear();
    m_shake = QPointF(0, 0);
    m_shakeAmount = 0;
    for (int i = 0; i < 6; ++i) { m_playerTrail[i] = QPointF(); m_playerTrailZ[i] = 0; }
    m_trailIdx = 0;

    m_current.pos = QPointF(0, 0);
    m_current.width = 92;
    m_current.depth = 92;
    m_current.height = 48;
    m_current.shape = Platform::Cylinder;
    const Palette &p0 = kPalette[0];
    m_current.topColor = p0.top;
    m_current.sideColor = p0.side;
    m_current.accentColor = p0.accent;
    m_current.seed = 1;
    m_current.perfectHit = false;

    m_playerPos = m_current.pos;
    m_playerZ = 0;

    m_cameraTarget = m_current.pos + QPointF(0, 160);
    m_camera = m_cameraTarget;

    generateNextPlatform();
    m_state = GameState::Ready;
}

void GameWidget::generateNextPlatform()
{
    // 难度递增: 分数越高, 平台越远越小
    float minDist, maxDist, minSize, maxSize;
    if (m_score < 5) {            // 入门: 近且大
        minDist = 118; maxDist = 188;
        minSize = 92;  maxSize = 118;
    } else if (m_score < 15) {    // 简单
        minDist = 142; maxDist = 244;
        minSize = 76;  maxSize = 110;
    } else if (m_score < 30) {    // 中等
        minDist = 168; maxDist = 296;
        minSize = 64;  maxSize = 96;
    } else if (m_score < 60) {    // 困难
        minDist = 196; maxDist = 326;
        minSize = 56;  maxSize = 86;
    } else {                      // 极难
        minDist = 218; maxDist = 342;
        minSize = 52;  maxSize = 76;
    }

    const float dist = minDist
        + float(QRandomGenerator::global()->bounded(int((maxDist - minDist) * 10))) / 10.0f;
    // 难度越高, 角度偏移范围越大(更难瞄准)
    const float angRange = 0.40f + std::min(0.30f, m_score * 0.005f);
    const float ang = (QRandomGenerator::global()->bounded(1000) - 500) / 1000.0f * angRange;
    const float dx = std::sin(ang) * dist;
    const float dy = std::cos(ang) * dist;

    m_next.pos = m_current.pos + QPointF(dx, dy);
    const float size = minSize + float(QRandomGenerator::global()->bounded(int(maxSize - minSize)));
    m_next.width = size;
    m_next.depth = size;
    m_next.height = 34.0f + float(QRandomGenerator::global()->bounded(40));
    // 困难时更多立方体(更难判定中心)
    const int boxChance = (m_score < 15) ? 22 : (m_score < 30 ? 32 : 42);
    m_next.shape = (QRandomGenerator::global()->bounded(100) < boxChance) ? Platform::Box : Platform::Cylinder;
    const Palette &pal = kPalette[QRandomGenerator::global()->bounded(kPaletteSize)];
    m_next.topColor = pal.top;
    m_next.sideColor = pal.side;
    m_next.accentColor = pal.accent;
    m_next.seed = QRandomGenerator::global()->bounded(1, 100000);
    m_next.perfectHit = false;
}

QPointF GameWidget::worldToScreen(const QPointF &w) const
{
    return QPointF(w.x() - m_camera.x() + width() / 2.0 + m_shake.x(),
                   (w.y() - m_camera.y()) * kProj + height() / 2.0 + m_shake.y());
}

float GameWidget::worldToScreenY(const QPointF &w, float z) const
{
    return float((w.y() - m_camera.y()) * kProj + height() / 2.0 + m_shake.y()) - z;
}

void GameWidget::startCharge()
{
    if (m_state != GameState::Idle) return;
    m_state = GameState::Charging;
    m_chargeTime = 0.0;
    m_chargeTimer.start();
    m_keyHeld = true;
    if (m_audio) m_audio->startCharge();
}

void GameWidget::releaseJump()
{
    if (m_state != GameState::Charging) return;
    m_keyHeld = false;
    if (m_audio) { m_audio->stopCharge(); m_audio->playJump(); }

    const float ratio = float(std::min(m_chargeTime / m_maxCharge, 1.0));
    const float jumpDist = ratio * kMaxJumpDist;

    QPointF dir = m_next.pos - m_current.pos;
    const float d = float(std::sqrt(dir.x() * dir.x() + dir.y() * dir.y()));
    if (d < 0.001f) dir = QPointF(0, 1);
    else dir /= d;

    m_jumpStart = m_playerPos;
    m_jumpEnd = m_playerPos + dir * jumpDist;
    m_jumpPeak = 80.0f + jumpDist * 0.30f;
    m_jumpDuration = 0.48 + jumpDist / kMaxJumpDist * 0.36;
    m_jumpElapsed = 0;

    m_playerScaleX = 1.0f;
    m_playerScaleY = 1.0f;
    m_state = GameState::Jumping;
}

void GameWidget::onLand()
{
    const QPointF diff = m_jumpEnd - m_next.pos;
    const float dist = float(std::sqrt(diff.x() * diff.x() + diff.y() * diff.y()));
    const float radius = m_next.width * 0.5f * 0.88f;

    if (dist <= radius) {
        const bool perfect = dist < 8.5f;
        const QPointF screen = worldToScreen(m_next.pos);

        if (perfect) {
            if (m_audio) m_audio->playPerfect();
            ++m_combo;
            m_score += 2 + 2 * m_combo;
            m_perfectFx = 0.0001f;
            m_next.perfectHit = true;
            spawnRipple(screen, QColor(255, 220, 80));
            spawnDust(screen, 18, 1.4f);
            shakeCamera(3.5f);
        } else {
            if (m_audio) m_audio->playLand();
            m_combo = 0;
            m_score += 1;
            spawnDust(screen, 10, 0.9f);
            shakeCamera(1.6f);
        }

        m_playerSquash = 1.0f;

        m_history.append(m_current);
        if (m_history.size() > 10) m_history.removeFirst();
        m_current = m_next;
        m_playerPos = m_next.pos;
        m_playerZ = 0;
        m_cameraTarget = m_current.pos + QPointF(0, 160);
        generateNextPlatform();
        m_state = GameState::Idle;

        if (m_score > m_bestScore) m_bestScore = m_score;
    } else {
        if (m_audio) m_audio->playFail();
        // 失败时先落在落点附近的位置,产生尘土,然后下坠
        const QPointF failScreen = worldToScreen(m_jumpEnd);
        spawnDust(failScreen, 14, 1.1f);
        shakeCamera(5.0f);
        m_state = GameState::Falling;
        m_jumpElapsed = 0;
        m_jumpDuration = 0.55;
        if (m_score > m_bestScore) m_bestScore = m_score;
    }
}

void GameWidget::spawnDust(const QPointF &screenPos, int count, float power)
{
    for (int i = 0; i < count; ++i) {
        Particle pt;
        pt.pos = screenPos;
        const float a = float(QRandomGenerator::global()->bounded(628)) / 100.0f;
        const float sp = (40.0f + float(QRandomGenerator::global()->bounded(90))) * power;
        pt.vel = QPointF(std::cos(a) * sp, std::sin(a) * sp * 0.5f - 60.0f * power);
        pt.maxLife = 0.4f + float(QRandomGenerator::global()->bounded(40)) / 100.0f;
        pt.life = pt.maxLife;
        pt.size = 2.0f + float(QRandomGenerator::global()->bounded(30)) / 10.0f;
        const int g = 200 + QRandomGenerator::global()->bounded(40);
        pt.color = QColor(g, g, g + 8, 220);
        m_particles.append(pt);
    }
}

void GameWidget::spawnRipple(const QPointF &screenPos, const QColor &c)
{
    Ripple r;
    r.pos = screenPos;
    r.r = 0;
    r.maxR = 70.0f;
    r.color = c;
    m_ripples.append(r);
}

void GameWidget::updateParticles(float dt)
{
    for (int i = m_particles.size() - 1; i >= 0; --i) {
        Particle &pt = m_particles[i];
        pt.life -= dt;
        if (pt.life <= 0) { m_particles.removeAt(i); continue; }
        pt.vel.setY(pt.vel.y() + 360.0f * dt);   // 重力
        pt.pos += pt.vel * dt;
        pt.vel *= (1.0f - 1.2f * dt);            // 阻力
    }
    for (int i = m_ripples.size() - 1; i >= 0; --i) {
        Ripple &r = m_ripples[i];
        r.r += dt * 180.0f;
        if (r.r >= r.maxR) m_ripples.removeAt(i);
    }
}

void GameWidget::shakeCamera(float amount)
{
    m_shakeAmount = std::max(m_shakeAmount, amount);
}

void GameWidget::tick()
{
    const qint64 now = m_clock.nsecsElapsed();
    float dt = float(now - m_lastNs) / 1e9f;
    m_lastNs = now;
    if (dt > 0.05f) dt = 0.05f;

    // 蓄力时玩家震动
    float chargeShakeX = 0, chargeShakeY = 0;
    if (m_state == GameState::Charging) {
        m_chargeTime = std::min(m_chargeTime + double(dt), m_maxCharge);
        const float t = float(m_chargeTime / m_maxCharge);
        m_playerScaleY = 1.0f - t * 0.46f;
        m_playerScaleX = 1.0f + t * 0.26f;
        // 蓄力越久抖动越明显
        const float amp = t * 1.6f;
        chargeShakeX = (float(QRandomGenerator::global()->bounded(1000)) / 500.0f - 1.0f) * amp;
        chargeShakeY = (float(QRandomGenerator::global()->bounded(1000)) / 500.0f - 1.0f) * amp;
    }

    switch (m_state) {
    case GameState::Idle:
        m_playerScaleX = damp(m_playerScaleX, 1.0f, 14.0f * dt);
        m_playerScaleY = damp(m_playerScaleY, 1.0f, 14.0f * dt);
        m_playerSquash = damp(m_playerSquash, 0.0f, 10.0f * dt);
        break;
    case GameState::Jumping: {
        m_jumpElapsed += double(dt);
        float p = float(m_jumpElapsed / m_jumpDuration);
        // 记录残影
        m_playerTrail[m_trailIdx] = m_playerPos;
        m_playerTrailZ[m_trailIdx] = m_playerZ;
        m_trailIdx = (m_trailIdx + 1) % 6;
        if (p >= 1.0f) {
            p = 1.0f;
            m_playerPos = m_jumpEnd;
            m_playerZ = 0;
            onLand();
        } else {
            const float ep = ease(p);
            m_playerPos = QPointF(lerp(float(m_jumpStart.x()), float(m_jumpEnd.x()), ep),
                                  lerp(float(m_jumpStart.y()), float(m_jumpEnd.y()), ep));
            m_playerZ = m_jumpPeak * 4.0f * p * (1.0f - p);
            m_playerRot = p * 360.0f;
        }
        break;
    }
    case GameState::Falling: {
        m_jumpElapsed += double(dt);
        const float p = float(std::min(m_jumpElapsed / m_jumpDuration, 1.0));
        m_playerPos = QPointF(lerp(float(m_jumpStart.x()), float(m_jumpEnd.x()), p),
                              lerp(float(m_jumpStart.y()), float(m_jumpEnd.y()), p));
        m_playerZ -= 520.0f * dt;
        m_playerRot += 760.0f * dt;
        if (m_playerZ < -280.0f) m_state = GameState::GameOver;
        break;
    }
    default: break;
    }

    // 相机
    m_camera.setX(damp(float(m_camera.x()), float(m_cameraTarget.x()), 6.0f * dt));
    m_camera.setY(damp(float(m_camera.y()), float(m_cameraTarget.y()), 6.0f * dt));

    // 抖动衰减
    if (m_shakeAmount > 0.01f) {
        m_shake.setX((float(QRandomGenerator::global()->bounded(1000)) / 500.0f - 1.0f) * m_shakeAmount);
        m_shake.setY((float(QRandomGenerator::global()->bounded(1000)) / 500.0f - 1.0f) * m_shakeAmount);
        m_shakeAmount *= std::exp(-9.0f * dt);
    } else {
        m_shake = QPointF(0, 0);
        m_shakeAmount = 0;
    }

    // 蓄力抖动叠加(不进入相机,只作用于玩家绘制)
    m_shake.setX(m_shake.x() + chargeShakeX);
    m_shake.setY(m_shake.y() + chargeShakeY);

    if (m_perfectFx > 0.0f && m_perfectFx < 1.0f) {
        m_perfectFx = std::min(1.0f, m_perfectFx + dt * 1.8f);
    }

    updateParticles(dt);
    update();
}

// ---------------- 绘制 ----------------
void GameWidget::drawBackground(QPainter &p)
{
    QLinearGradient g(0, 0, 0, height());
    g.setColorAt(0.0, QColor(248, 249, 253));
    g.setColorAt(0.5, QColor(236, 240, 248));
    g.setColorAt(1.0, QColor(206, 214, 230));
    p.fillRect(rect(), g);

    // 顶部柔光
    QRadialGradient glow(width() / 2.0, height() * 0.30, width() * 0.8);
    glow.setColorAt(0.0, QColor(255, 255, 255, 80));
    glow.setColorAt(1.0, QColor(255, 255, 255, 0));
    p.fillRect(rect(), glow);

    // 微弱网格点(营造空间感)
    p.setPen(QPen(QColor(180, 188, 210, 28), 1));
    const float step = 36.0f;
    const float offX = float(fmod(-m_camera.x(), step));
    const float offY = float(fmod(-m_camera.y() * kProj, step));
    for (float x = offX; x < width(); x += step) {
        for (float y = offY; y < height(); y += step) {
            p.drawPoint(QPointF(x, y));
        }
    }
}

void GameWidget::drawGround(QPainter &p)
{
    // 大椭圆地面: 跟随相机, 让平台看起来立在地面上而非悬浮
    // 地面中心略低于玩家所在位置(世界 y 更大 = 屏幕更下)
    const QPointF gc = worldToScreen(QPointF(m_camera.x(), m_camera.y() - 30));
    const float rx = width() * 0.95f;
    const float ry = rx * kProj;
    // 地面径向渐变: 中心实色, 边缘渐隐到背景
    QRadialGradient rg(gc, rx);
    rg.setColorAt(0.0,  QColor(222, 228, 240, 200));
    rg.setColorAt(0.55, QColor(216, 222, 236, 130));
    rg.setColorAt(0.85, QColor(210, 218, 232, 40));
    rg.setColorAt(1.0,  QColor(210, 218, 232, 0));
    p.setPen(Qt::NoPen);
    p.setBrush(rg);
    p.drawEllipse(gc, rx, ry);

    // 地面纹理: 同心椭圆环(轻微,营造地面延伸感)
    p.setBrush(Qt::NoBrush);
    for (int i = 1; i <= 3; ++i) {
        const float k = float(i) / 3.0f;
        const int a = int(50 * (1.0f - k));
        p.setPen(QPen(QColor(190, 198, 216, a), 1.0f));
        p.drawEllipse(gc, rx * k, ry * k);
    }
}

void GameWidget::drawPlatform(QPainter &p, const Platform &pl)
{
    const QPointF c = worldToScreen(pl.pos);
    const float w = pl.width;
    const float d = pl.depth * kProj;
    const float h = pl.height;

    p.setRenderHint(QPainter::Antialiasing, true);

    // 地面投影
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 38));
    p.drawEllipse(QPointF(c.x(), c.y() + h + 6), w * 0.55f, d * 0.45f);

    const QRectF topRect(c.x() - w / 2.0, c.y() - d / 2.0, w, d);
    const QRectF botRect(c.x() - w / 2.0, c.y() - d / 2.0 + h, w, d);

    if (pl.shape == Platform::Cylinder) {
        // 侧面
        QPainterPath side;
        side.moveTo(topRect.left(), topRect.center().y());
        side.arcTo(topRect, 180, -180);
        side.lineTo(botRect.right(), botRect.center().y());
        side.arcTo(botRect, 0, 180);
        side.closeSubpath();
        // 侧面渐变(左暗右亮)
        QLinearGradient sg(c.x() - w / 2.0, 0, c.x() + w / 2.0, 0);
        sg.setColorAt(0.0, pl.sideColor.darker(112));
        sg.setColorAt(0.5, pl.sideColor);
        sg.setColorAt(1.0, pl.sideColor.lighter(108));
        p.setBrush(sg);
        p.drawPath(side);

        // 顶面
        QRadialGradient tg(c.x() - w * 0.12, c.y() - d * 0.20, w * 0.7);
        tg.setColorAt(0.0, pl.topColor.lighter(106));
        tg.setColorAt(1.0, pl.topColor);
        p.setBrush(tg);
        p.drawEllipse(topRect);

        // 同心圆木纹
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(pl.accentColor, 1.0));
        const int rings = 3;
        for (int i = 1; i <= rings; ++i) {
            const float k = float(i) / (rings + 1);
            // 用 seed 制造不规则
            const float jx = (rnd(pl.seed, i) - 0.5f) * 4.0f;
            const float jy = (rnd(pl.seed, i + 9) - 0.5f) * 3.0f;
            p.drawEllipse(QPointF(c.x() + jx, c.y() + jy),
                          w * 0.5f * k, d * 0.5f * k);
        }
        // 顶面边缘描边
        p.setPen(QPen(pl.sideColor.darker(130), 1.2f));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(topRect);
    } else {
        // 立方体
        QPainterPath top;
        top.moveTo(c.x(), c.y() - d / 2.0);
        top.lineTo(c.x() + w / 2.0, c.y());
        top.lineTo(c.x(), c.y() + d / 2.0);
        top.lineTo(c.x() - w / 2.0, c.y());
        top.closeSubpath();
        QRadialGradient tg(c.x(), c.y() - d * 0.2, w * 0.6);
        tg.setColorAt(0.0, pl.topColor.lighter(106));
        tg.setColorAt(1.0, pl.topColor);
        p.setBrush(tg);
        p.drawPath(top);

        QPainterPath sideRight;
        sideRight.moveTo(c.x() + w / 2.0, c.y());
        sideRight.lineTo(c.x(), c.y() + d / 2.0);
        sideRight.lineTo(c.x(), c.y() + d / 2.0 + h);
        sideRight.lineTo(c.x() + w / 2.0, c.y() + h);
        sideRight.closeSubpath();
        p.setBrush(pl.sideColor);
        p.drawPath(sideRight);

        QPainterPath sideLeft;
        sideLeft.moveTo(c.x() - w / 2.0, c.y());
        sideLeft.lineTo(c.x(), c.y() + d / 2.0);
        sideLeft.lineTo(c.x(), c.y() + d / 2.0 + h);
        sideLeft.lineTo(c.x() - w / 2.0, c.y() + h);
        sideLeft.closeSubpath();
        p.setBrush(pl.sideColor.darker(112));
        p.drawPath(sideLeft);

        // 顶面描边
        p.setPen(QPen(pl.sideColor.darker(130), 1.0f));
        p.setBrush(Qt::NoBrush);
        p.drawPath(top);
    }

    // 完美中心小红点(微信原版提示)
    if (!pl.perfectHit) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(220, 70, 70, 230));
        p.drawEllipse(c, 3.0f, 2.0f * kProj + 1.2f);
        p.setBrush(QColor(255, 255, 255, 150));
        p.drawEllipse(QPointF(c.x() - 0.6f, c.y() - 0.6f), 1.0f, 0.8f);
    }
}

void GameWidget::drawShadow(QPainter &p)
{
    const QPointF c = worldToScreen(m_playerPos);
    const float h = m_playerZ;
    const float k = 1.0f + h / 200.0f;
    const float alpha = 110.0f * std::max(0.0f, 1.0f - h / 240.0f);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, int(alpha)));
    // 阴影放在脚底(y=0,即屏幕 y=c.y())
    p.drawEllipse(QPointF(c.x(), c.y() + 2.0f), 16.0f * k, 5.0f * k * kProj + 1.8f);
}

// 角色几何常量(脚底在 y=0,头在 y=-HEAD_Y - 2*HEAD_R)
// 微信原版"打火机人": 黑色圆头 + 细长方形身体
namespace {
constexpr float BODY_W     = 14.0f;   // 身体宽度
constexpr float BODY_H     = 26.0f;   // 身体高度
constexpr float BODY_TOP_Y = -26.0f;  // 身体顶面 y
constexpr float HEAD_R     = 9.0f;    // 头部半径
constexpr float HEAD_CX    = 0.0f;    // 头部 x 中心
constexpr float HEAD_CY    = -35.0f;  // 头部 y 中心
}

void GameWidget::drawTrail(QPainter &p)
{
    if (m_state != GameState::Jumping) return;
    p.setPen(Qt::NoPen);
    for (int i = 0; i < 6; ++i) {
        const int idx = (m_trailIdx + i) % 6;
        if (m_playerTrail[idx].isNull()) continue;
        const QPointF c = worldToScreen(m_playerTrail[idx]);
        const float sy = float(c.y()) - m_playerTrailZ[idx];
        const float alpha = float(i) / 6.0f;
        p.setBrush(QColor(30, 32, 40, int(70 * (1.0f - alpha))));
        p.save();
        p.translate(c.x(), sy);
        // 身体
        p.drawRect(QRectF(-BODY_W / 2.0, BODY_TOP_Y, BODY_W, BODY_H));
        // 头
        p.drawEllipse(QPointF(HEAD_CX, HEAD_CY), HEAD_R, HEAD_R);
        p.restore();
    }
}

void GameWidget::drawPlayer(QPainter &p)
{
    const QPointF c = worldToScreen(m_playerPos);
    const float sy = float(c.y()) - m_playerZ;

    p.save();
    p.translate(c.x(), sy);

    // 蓄力抖动 + 落地压扁
    const float squashY = m_playerSquash > 0 ? (1.0f - m_playerSquash * 0.30f) : 1.0f;
    const float squashX = m_playerSquash > 0 ? (1.0f + m_playerSquash * 0.22f) : 1.0f;
    p.scale(m_playerScaleX * squashX, m_playerScaleY * squashY);
    p.rotate(m_playerRot);

    p.setPen(Qt::NoPen);

    // ---- 身体(细长方形,带渐变) ----
    QLinearGradient bg(-BODY_W / 2.0, 0, BODY_W / 2.0, 0);
    bg.setColorAt(0.0,  QColor(40, 44, 58));
    bg.setColorAt(0.45, QColor(22, 24, 34));
    bg.setColorAt(1.0,  QColor(8, 10, 16));
    p.setBrush(bg);
    p.drawRect(QRectF(-BODY_W / 2.0, BODY_TOP_Y, BODY_W, BODY_H));

    // 身体左侧高光条
    p.setBrush(QColor(180, 190, 220, 55));
    p.drawRect(QRectF(-BODY_W / 2.0 + 1.0, BODY_TOP_Y + 1.5, 1.2f, BODY_H - 3.0f));

    // 身体底部反光
    p.setBrush(QColor(130, 140, 170, 60));
    p.drawRect(QRectF(-BODY_W / 2.0 + 1.5, BODY_TOP_Y + BODY_H - 3.0f, BODY_W - 3.0f, 1.2f));

    // ---- 头部(黑色圆球,带径向渐变) ----
    QRadialGradient hg(HEAD_CX - HEAD_R * 0.35, HEAD_CY - HEAD_R * 0.35, HEAD_R * 1.2);
    hg.setColorAt(0.0,  QColor(80, 84, 100));
    hg.setColorAt(0.45, QColor(34, 36, 46));
    hg.setColorAt(1.0,  QColor(6, 8, 14));
    p.setBrush(hg);
    p.drawEllipse(QPointF(HEAD_CX, HEAD_CY), HEAD_R, HEAD_R);

    // 头部高光点
    p.setBrush(QColor(255, 255, 255, 110));
    p.drawEllipse(QPointF(HEAD_CX - HEAD_R * 0.35, HEAD_CY - HEAD_R * 0.40), HEAD_R * 0.20f, HEAD_R * 0.26f);

    p.restore();
}

void GameWidget::drawParticles(QPainter &p)
{
    p.setPen(Qt::NoPen);
    for (const Particle &pt : m_particles) {
        const float t = pt.life / pt.maxLife;
        QColor c = pt.color;
        c.setAlpha(int(c.alpha() * t));
        p.setBrush(c);
        p.drawEllipse(pt.pos, pt.size * t, pt.size * t);
    }
    for (const Ripple &r : m_ripples) {
        const float t = r.r / r.maxR;
        QColor c = r.color;
        c.setAlpha(int(220 * (1.0f - t)));
        p.setPen(QPen(c, 2.5f));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(r.pos, r.r, r.r * kProj + 2.0f);
    }
}

void GameWidget::drawPerfectFx(QPainter &p)
{
    if (m_perfectFx <= 0.0f || m_perfectFx >= 1.0f) return;
    const QPointF c = worldToScreen(m_current.pos);
    const float r = m_perfectFx * m_current.width * 1.6f;
    const int alpha = int(230 * (1.0f - m_perfectFx));
    p.setPen(QPen(QColor(255, 215, 80, alpha), 3));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(c, r, r * kProj + 5);
    // 内圈
    p.setPen(QPen(QColor(255, 240, 180, alpha / 2), 2));
    p.drawEllipse(c, r * 0.6f, r * 0.6f * kProj + 3);
}

void GameWidget::drawHUD(QPainter &p)
{
    p.setRenderHint(QPainter::TextAntialiasing, true);

    // 分数(带阴影)
    QFont f("Arial", 30, QFont::Bold);
    p.setFont(f);
    const QString score = QString::number(m_score);
    p.setPen(QColor(255, 255, 255, 180));
    p.drawText(rect().adjusted(-1, 29, -25, 0), Qt::AlignTop | Qt::AlignRight, score);
    p.setPen(QColor(56, 60, 78));
    p.drawText(rect().adjusted(0, 30, -26, 0), Qt::AlignTop | Qt::AlignRight, score);

    // 最高分
    QFont bf("Arial", 10);
    p.setFont(bf);
    p.setPen(QColor(140, 148, 168));
    p.drawText(rect().adjusted(0, 66, -26, 0), Qt::AlignTop | Qt::AlignRight,
               QString("Best %1").arg(m_bestScore));

    // 连击
    if (m_combo > 0) {
        QFont cf("Arial", 13, QFont::Bold);
        p.setFont(cf);
        p.setPen(QColor(255, 170, 60));
        p.drawText(rect().adjusted(0, 82, -26, 0), Qt::AlignTop | Qt::AlignRight,
                   QString("Combo x%1").arg(m_combo));
    }

    // 蓄力条(圆形进度环绕玩家)
    if (m_state == GameState::Charging) {
        const float ratio = float(m_chargeTime / m_maxCharge);
        const QPointF c = worldToScreen(m_playerPos);
        const float sy = float(c.y()) - m_playerZ;
        const float R = 28.0f;
        // 底环
        p.setPen(QPen(QColor(0, 0, 0, 60), 4));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QPointF(c.x(), sy - 28), R, R);
        // 进度环
        QConicalGradient cg(c.x(), sy - 28, 90);
        cg.setColorAt(0.0, QColor(120, 200, 255));
        cg.setColorAt(0.6, QColor(255, 200, 90));
        cg.setColorAt(1.0, QColor(255, 90, 90));
        QPen pen(QBrush(cg), 4);
        pen.setCapStyle(Qt::RoundCap);
        p.setPen(pen);
        QRectF arcRect(c.x() - R, sy - 28 - R, 2 * R, 2 * R);
        p.drawArc(arcRect, 90 * 16, int(-ratio * 360 * 16));
    }

    // 提示文字
    QFont hf("Arial", 11);
    p.setFont(hf);
    p.setPen(QColor(120, 124, 140));
    QString hint;
    switch (m_state) {
    case GameState::Idle:     hint = "Hold SPACE / Mouse to charge, release to jump"; break;
    case GameState::Charging: hint = "Charging... release to jump"; break;
    case GameState::Ready:    hint = "Press SPACE / Click to start"; break;
    case GameState::GameOver: hint = "Press SPACE to restart"; break;
    default: break;
    }
    if (!hint.isEmpty())
        p.drawText(rect().adjusted(0, -44, 0, 0), Qt::AlignBottom | Qt::AlignHCenter, hint);
}

void GameWidget::drawReadyScreen(QPainter &p)
{
    if (m_state != GameState::Ready) return;
    // 半透明遮罩
    p.fillRect(rect(), QColor(248, 249, 253, 200));
    p.setRenderHint(QPainter::TextAntialiasing, true);

    QFont title("Arial", 48, QFont::Bold);
    p.setFont(title);
    p.setPen(QColor(255, 255, 255, 200));
    p.drawText(rect().adjusted(0, -190, 0, 0), Qt::AlignCenter, "JUMP");
    p.setPen(QColor(50, 54, 72));
    p.drawText(rect().adjusted(0, -191, 0, 0), Qt::AlignCenter, "JUMP");

    QFont sub("Arial", 13);
    p.setFont(sub);
    p.setPen(QColor(120, 126, 144));
    p.drawText(rect().adjusted(0, -120, 0, 0), Qt::AlignCenter,
               "Hold to charge · Release to jump · Land on the center for bonus");
    p.drawText(rect().adjusted(0, -98, 0, 0), Qt::AlignCenter,
               "Press M to toggle sound");
}

void GameWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    drawBackground(p);
    drawGround(p);

    for (const Platform &pl : m_history) drawPlatform(p, pl);
    drawPlatform(p, m_current);
    drawPlatform(p, m_next);

    drawShadow(p);
    drawTrail(p);
    drawPlayer(p);
    drawParticles(p);
    drawPerfectFx(p);
    drawHUD(p);
    drawReadyScreen(p);

    if (m_state == GameState::GameOver) {
        p.fillRect(rect(), QColor(20, 22, 32, 140));
        p.setRenderHint(QPainter::TextAntialiasing, true);
        QFont gf("Arial", 46, QFont::Bold);
        p.setFont(gf);
        p.setPen(QColor(255, 255, 255, 230));
        p.drawText(rect().adjusted(0, -60, 0, 0), Qt::AlignCenter, "GAME OVER");
        QFont sf("Arial", 16);
        p.setFont(sf);
        p.setPen(QColor(220, 224, 240));
        p.drawText(rect().adjusted(0, 10, 0, 0), Qt::AlignCenter,
                   QString("Score %1   Best %2").arg(m_score).arg(m_bestScore));
    }
}

void GameWidget::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Space) {
        if (m_state == GameState::Ready || m_state == GameState::GameOver) {
            resetGame();
            m_state = GameState::Idle;
            return;
        }
        if (!m_keyHeld) startCharge();
        e->accept();
    } else if (e->key() == Qt::Key_M) {
        if (m_audio) m_audio->setMuted(!m_audio->isMuted());
    }
    QWidget::keyPressEvent(e);
}

void GameWidget::keyReleaseEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Space && !e->isAutoRepeat()) {
        if (m_state == GameState::Charging) releaseJump();
        e->accept();
        return;
    }
    QWidget::keyReleaseEvent(e);
}

void GameWidget::mousePressEvent(QMouseEvent *e)
{
    if (m_state == GameState::Ready || m_state == GameState::GameOver) {
        resetGame();
        m_state = GameState::Idle;
        return;
    }
    if (!m_keyHeld) startCharge();
    e->accept();
}

void GameWidget::mouseReleaseEvent(QMouseEvent *e)
{
    if (m_state == GameState::Charging) releaseJump();
    e->accept();
}

void GameWidget::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
}
