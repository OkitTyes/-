#include "ThunderStrike.h"
#include <Windows.h>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <cmath>

#pragma comment(lib, "Msimg32.lib")
#pragma comment(lib, "gdi32.lib")

using namespace std;

const int GAME_WIDTH = 800;
const int GAME_HEIGHT = 600;

const int PLAYER_W = 40;
const int PLAYER_H = 48;

const int BULLET_W = 6;
const int BULLET_H = 18;
const int DEFAULT_SHOOT_INTERVAL = 15;
const int BULLET_SPEED = 14;

const int ENEMY_W = 44;
const int ENEMY_H = 36;
const float BASE_ENEMY_SPEED = 4.0f;
const float MAX_ENEMY_SPEED = 12.0f;

const int ITEM_W = 24;
const int ITEM_H = 24;

enum GameState { STATE_MENU, STATE_PLAYING, STATE_GAMEOVER };
GameState g_state = STATE_MENU;

int  g_score = 0;
int  g_hp = 5;
int  g_playerX, g_playerY;
int  g_shootCounter = 0;
int  g_currentShootInterval = DEFAULT_SHOOT_INTERVAL;

bool g_keyUp, g_keyDown, g_keyLeft, g_keyRight;
int  g_frameCount = 0;

int  g_invincibleFrames = 0;
int  g_shootBoostFrames = 0;
int  g_lastScoreForSpeed = 0;

struct Bullet { int x, y; };
vector<Bullet> g_bullets;

struct Enemy { int x, y; };
vector<Enemy> g_enemies;

struct Item {
    int x, y;
    int type;
    int animFrame;
};
vector<Item> g_items;

struct Star {
    int x, y, speed;
    BYTE bright;
};
vector<Star> g_stars;

struct HeartParticle {
    float x, y;
    float vx, vy;
    int life;
};
vector<HeartParticle> g_heartParticles;

struct ExplosionParticle {
    float x, y;
    float vx, vy;
    int life;
    BYTE r, g, b;
};
vector<ExplosionParticle> g_explosions;

// 全屏与字体
bool g_fullscreen = false;
RECT g_windowRect;
HFONT g_hHeartFont = nullptr;

float g_currentEnemySpeed = BASE_ENEMY_SPEED;
int  g_highScore = 0;

void InitStarField() {
    g_stars.clear();
    for (int i = 0; i < 200; ++i) {
        Star s;
        s.x = rand() % GAME_WIDTH;
        s.y = rand() % GAME_HEIGHT;
        s.speed = rand() % 2 + 1;
        s.bright = rand() % 120 + 135;
        g_stars.push_back(s);
    }
}

void CreateExplosion(float cx, float cy) {
    int count = 12 + rand() % 12;
    for (int i = 0; i < count; ++i) {
        ExplosionParticle p;
        p.x = cx + (rand() % 20 - 10);
        p.y = cy + (rand() % 20 - 10);
        float angle = (rand() % 360) * 3.14159f / 180.0f;
        float speed = (rand() % 40 + 20) / 10.0f;
        p.vx = cos(angle) * speed;
        p.vy = sin(angle) * speed;
        p.life = 20 + rand() % 25;
        p.r = 200 + rand() % 56;
        p.g = 80 + rand() % 100;
        p.b = 20 + rand() % 40;
        g_explosions.push_back(p);
    }
}

void TryDropItem(float x, float y) {
    if (rand() % 100 < 30) {
        Item it;
        it.x = (int)x - ITEM_W / 2;
        it.y = (int)y - ITEM_H / 2;
        it.type = rand() % 3;
        it.animFrame = 0;
        g_items.push_back(it);
    }
}

void ResetGame() {
    g_state = STATE_PLAYING;
    g_score = 0;
    g_hp = 5;
    g_shootCounter = 0;
    g_currentShootInterval = DEFAULT_SHOOT_INTERVAL;
    g_invincibleFrames = 0;
    g_shootBoostFrames = 0;
    g_currentEnemySpeed = BASE_ENEMY_SPEED;
    g_lastScoreForSpeed = 0;

    g_playerX = GAME_WIDTH / 2 - PLAYER_W / 2;
    g_playerY = GAME_HEIGHT - 120;

    g_bullets.clear();
    g_enemies.clear();
    g_items.clear();
    g_heartParticles.clear();
    g_explosions.clear();
    InitStarField();

    g_keyUp = g_keyDown = g_keyLeft = g_keyRight = false;
    g_frameCount = 0;
}

void UpdateStar() {
    for (auto& s : g_stars) {
        s.y += s.speed;
        if (s.y > GAME_HEIGHT) {
            s.y = 0;
            s.x = rand() % GAME_WIDTH;
        }
    }
}

void UpdateEnemySpeed() {
    if (g_score == g_lastScoreForSpeed) return;
    g_lastScoreForSpeed = g_score;
    float newSpeed = BASE_ENEMY_SPEED + (g_score / 200.0f) * 0.8f;
    if (newSpeed > MAX_ENEMY_SPEED) newSpeed = MAX_ENEMY_SPEED;
    g_currentEnemySpeed = newSpeed;
}

void GameUpdatePlaying() {
    g_frameCount++;

    if (g_keyLeft && g_playerX > 20) g_playerX -= 6;
    if (g_keyRight && g_playerX < GAME_WIDTH - PLAYER_W - 20) g_playerX += 6;
    if (g_keyUp && g_playerY > 20) g_playerY -= 6;
    if (g_keyDown&& g_playerY < GAME_HEIGHT - PLAYER_H - 20) g_playerY += 6;

    for (auto& b : g_bullets) b.y -= BULLET_SPEED;
    g_bullets.erase(remove_if(g_bullets.begin(), g_bullets.end(),
        [](const Bullet& b) { return b.y < -BULLET_H; }), g_bullets.end());

    for (auto& e : g_enemies) e.y += (int)g_currentEnemySpeed;
    g_enemies.erase(remove_if(g_enemies.begin(), g_enemies.end(),
        [](const Enemy& e) { return e.y > GAME_HEIGHT; }), g_enemies.end());

    for (auto& it : g_items) it.y += 3;
    g_items.erase(remove_if(g_items.begin(), g_items.end(),
        [](const Item& it) { return it.y > GAME_HEIGHT; }), g_items.end());

    g_shootCounter++;
    if (g_shootCounter >= g_currentShootInterval) {
        Bullet b;
        b.x = g_playerX + PLAYER_W / 2 - BULLET_W / 2;
        b.y = g_playerY;
        g_bullets.push_back(b);
        g_shootCounter = 0;
    }

    int spawnChance = max(12, 28 - g_score / 180);
    if (rand() % spawnChance == 0) {
        Enemy e;
        e.x = rand() % (GAME_WIDTH - ENEMY_W);
        e.y = -ENEMY_H;
        g_enemies.push_back(e);
    }

    for (auto itB = g_bullets.begin(); itB != g_bullets.end();) {
        bool hit = false;
        for (auto itE = g_enemies.begin(); itE != g_enemies.end();) {
            bool collide = itB->x < itE->x + ENEMY_W &&
                itB->x + BULLET_W > itE->x &&
                itB->y < itE->y + ENEMY_H &&
                itB->y + BULLET_H > itE->y;
            if (collide) {
                CreateExplosion(itE->x + ENEMY_W / 2.0f, itE->y + ENEMY_H / 2.0f);
                TryDropItem(itE->x + ENEMY_W / 2.0f, itE->y + ENEMY_H / 2.0f);
                itE = g_enemies.erase(itE);
                hit = true;
                g_score += 20;
                UpdateEnemySpeed();
                break;
            }
            ++itE;
        }
        if (hit) itB = g_bullets.erase(itB);
        else ++itB;
    }

    for (auto it = g_items.begin(); it != g_items.end();) {
        bool collide = g_playerX < it->x + ITEM_W &&
            g_playerX + PLAYER_W > it->x &&
            g_playerY < it->y + ITEM_H &&
            g_playerY + PLAYER_H > it->y;
        if (collide) {
            switch (it->type) {
            case 0:
                if (g_hp < 9) g_hp++;
                for (int i = 0; i < 8; i++) {
                    HeartParticle hp;
                    hp.x = g_playerX + PLAYER_W / 2.0f;
                    hp.y = g_playerY + PLAYER_H / 2.0f;
                    float a = (rand() % 360) * 3.14159f / 180.0f;
                    hp.vx = cos(a) * 2.5f;
                    hp.vy = sin(a) * 2.5f;
                    hp.life = 20;
                    g_heartParticles.push_back(hp);
                }
                break;
            case 1:
                g_shootBoostFrames = 480;
                g_currentShootInterval = DEFAULT_SHOOT_INTERVAL / 2;
                break;
            case 2:
                g_invincibleFrames = 300;
                break;
            }
            it = g_items.erase(it);
        }
        else ++it;
    }

    if (g_shootBoostFrames > 0) {
        g_shootBoostFrames--;
        if (g_shootBoostFrames == 0) g_currentShootInterval = DEFAULT_SHOOT_INTERVAL;
    }
    if (g_invincibleFrames > 0) g_invincibleFrames--;

    for (auto itE = g_enemies.begin(); itE != g_enemies.end();) {
        bool collide = g_playerX < itE->x + ENEMY_W &&
            g_playerX + PLAYER_W > itE->x &&
            g_playerY < itE->y + ENEMY_H &&
            g_playerY + PLAYER_H > itE->y;
        if (collide) {
            CreateExplosion(itE->x + ENEMY_W / 2.0f, itE->y + ENEMY_H / 2.0f);
            itE = g_enemies.erase(itE);
            if (g_invincibleFrames <= 0) {
                g_hp--;
                for (int i = 0; i < 12; ++i) {
                    HeartParticle p;
                    p.x = g_playerX + PLAYER_W / 2.0f;
                    p.y = g_playerY + PLAYER_H / 2.0f;
                    float angle = (rand() % 360) * 3.14159f / 180.0f;
                    float speed = (rand() % 30 + 20) / 10.0f;
                    p.vx = cos(angle) * speed;
                    p.vy = sin(angle) * speed - 1.5f;
                    p.life = 30 + rand() % 20;
                    g_heartParticles.push_back(p);
                }
                if (g_hp <= 0) {
                    g_state = STATE_GAMEOVER;
                    if (g_score > g_highScore) g_highScore = g_score;
                    return;
                }
                g_invincibleFrames = 30;
            }
        }
        else ++itE;
    }

    for (auto& hp : g_heartParticles) {
        hp.x += hp.vx;
        hp.y += hp.vy;
        hp.vy += 0.08f;
        hp.life--;
    }
    g_heartParticles.erase(remove_if(g_heartParticles.begin(), g_heartParticles.end(),
        [](const HeartParticle& p) { return p.life <= 0; }), g_heartParticles.end());

    for (auto& ep : g_explosions) {
        ep.x += ep.vx;
        ep.y += ep.vy;
        ep.life--;
        ep.vx *= 0.98f;
        ep.vy *= 0.98f;
    }
    g_explosions.erase(remove_if(g_explosions.begin(), g_explosions.end(),
        [](const ExplosionParticle& p) { return p.life <= 0; }), g_explosions.end());

    for (auto& it : g_items) it.animFrame++;
}

void GameUpdate() {
    UpdateStar();
    if (g_state == STATE_PLAYING) GameUpdatePlaying();
}

void DrawCircle(HDC hdc, int cx, int cy, int r, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    SelectObject(hdc, brush);
    SelectObject(hdc, pen);
    Ellipse(hdc, cx - r, cy - r, cx + r, cy + r);
    DeleteObject(brush);
    DeleteObject(pen);
}

void DrawItem(HDC hdc, const Item& it) {
    int x = it.x, y = it.y;
    bool blink = (it.animFrame / 6) % 2 == 0;
    SetBkMode(hdc, TRANSPARENT);
    switch (it.type) {
    case 0:
        SetTextColor(hdc, blink ? RGB(255, 180, 200) : RGB(255, 80, 120));
        TextOutW(hdc, x + 4, y, L"\u2665", 1);
        break;
    case 1:
        SetTextColor(hdc, blink ? RGB(255, 255, 150) : RGB(255, 220, 50));
        TextOutW(hdc, x + 4, y, L"\u26A1", 1);
        break;
    case 2:
        SetTextColor(hdc, blink ? RGB(180, 240, 255) : RGB(100, 200, 255));
        TextOutW(hdc, x + 2, y, L"\U0001F6E1", 1);
        break;
    }
}

void DrawPlayerPlane(HDC hdc, int x, int y) {
    if (g_invincibleFrames > 0 && (g_frameCount / 4) % 2 == 0) {
        HPEN flashPen = CreatePen(PS_SOLID, 3, RGB(255, 255, 255));
        HPEN oldPen = (HPEN)SelectObject(hdc, flashPen);
        Rectangle(hdc, x - 2, y - 2, x + PLAYER_W + 2, y + PLAYER_H + 2);
        SelectObject(hdc, oldPen);
        DeleteObject(flashPen);
    }

    int flameH1 = 8 + (int)(3 * sin(g_frameCount * 0.5f));
    int flameH2 = 8 + (int)(3 * cos(g_frameCount * 0.5f + 1));

    POINT flameL[3] = { { x + 5, y + PLAYER_H }, { x + 12, y + PLAYER_H + flameH1 }, { x + 19, y + PLAYER_H } };
    HBRUSH hFlameL = CreateSolidBrush(RGB(255, 140, 0));
    HPEN hPenFlame = CreatePen(PS_SOLID, 1, RGB(255, 100, 0));
    SelectObject(hdc, hPenFlame);
    SelectObject(hdc, hFlameL);
    Polygon(hdc, flameL, 3);
    DeleteObject(hFlameL);

    POINT flameL2[3] = { { x + 9, y + PLAYER_H }, { x + 12, y + PLAYER_H + flameH1 - 3 }, { x + 15, y + PLAYER_H } };
    HBRUSH hFlameL2 = CreateSolidBrush(RGB(255, 255, 0));
    SelectObject(hdc, hFlameL2);
    Polygon(hdc, flameL2, 3);
    DeleteObject(hFlameL2);

    POINT flameR[3] = { { x + PLAYER_W - 19, y + PLAYER_H }, { x + PLAYER_W - 12, y + PLAYER_H + flameH2 }, { x + PLAYER_W - 5, y + PLAYER_H } };
    HBRUSH hFlameR = CreateSolidBrush(RGB(255, 140, 0));
    SelectObject(hdc, hFlameR);
    Polygon(hdc, flameR, 3);
    DeleteObject(hFlameR);

    POINT flameR2[3] = { { x + PLAYER_W - 15, y + PLAYER_H }, { x + PLAYER_W - 12, y + PLAYER_H + flameH2 - 3 }, { x + PLAYER_W - 9, y + PLAYER_H } };
    HBRUSH hFlameR2 = CreateSolidBrush(RGB(255, 255, 0));
    SelectObject(hdc, hFlameR2);
    Polygon(hdc, flameR2, 3);
    DeleteObject(hFlameR2);
    DeleteObject(hPenFlame);

    POINT outer[3] = { { x + PLAYER_W / 2, y }, { x, y + PLAYER_H }, { x + PLAYER_W, y + PLAYER_H } };
    HBRUSH hOuter = CreateSolidBrush(RGB(0, 80, 160));
    HPEN hPenOuter = CreatePen(PS_SOLID, 2, RGB(100, 180, 255));
    SelectObject(hdc, hPenOuter);
    SelectObject(hdc, hOuter);
    Polygon(hdc, outer, 3);
    DeleteObject(hOuter);

    int inset = 6;
    POINT inner[3] = { { x + PLAYER_W / 2, y + inset }, { x + inset, y + PLAYER_H - inset }, { x + PLAYER_W - inset, y + PLAYER_H - inset } };
    HBRUSH hInner = CreateSolidBrush(RGB(0, 160, 240));
    HPEN hPenInner = CreatePen(PS_SOLID, 1, RGB(0, 200, 255));
    SelectObject(hdc, hPenInner);
    SelectObject(hdc, hInner);
    Polygon(hdc, inner, 3);
    DeleteObject(hInner);
    DeleteObject(hPenInner);

    HBRUSH hCockpit = CreateSolidBrush(RGB(200, 230, 255));
    SelectObject(hdc, hCockpit);
    Ellipse(hdc, x + PLAYER_W / 2 - 7, y + 12, x + PLAYER_W / 2 + 7, y + 28);
    DeleteObject(hCockpit);

    HBRUSH hCockpitH = CreateSolidBrush(RGB(255, 255, 255));
    SelectObject(hdc, hCockpitH);
    Ellipse(hdc, x + PLAYER_W / 2 - 3, y + 14, x + PLAYER_W / 2 + 2, y + 22);
    DeleteObject(hCockpitH);
    DeleteObject(hPenOuter);
}

void DrawEnemyPlane(HDC hdc, int x, int y) {
    POINT outer[4] = { { x + ENEMY_W / 2, y }, { x + ENEMY_W, y + ENEMY_H / 2 }, { x + ENEMY_W / 2, y + ENEMY_H }, { x, y + ENEMY_H / 2 } };
    HBRUSH hOuter = CreateSolidBrush(RGB(180, 40, 70));
    HPEN hPen = CreatePen(PS_SOLID, 2, RGB(255, 80, 110));
    SelectObject(hdc, hPen);
    SelectObject(hdc, hOuter);
    Polygon(hdc, outer, 4);
    DeleteObject(hOuter);

    int inset = 7;
    POINT inner[4] = { { x + ENEMY_W / 2, y + inset }, { x + ENEMY_W - inset, y + ENEMY_H / 2 }, { x + ENEMY_W / 2, y + ENEMY_H - inset }, { x + inset, y + ENEMY_H / 2 } };
    HBRUSH hInner = CreateSolidBrush(RGB(100, 20, 40));
    HPEN hPenInner = CreatePen(PS_SOLID, 1, RGB(150, 50, 70));
    SelectObject(hdc, hPenInner);
    SelectObject(hdc, hInner);
    Polygon(hdc, inner, 4);
    DeleteObject(hInner);
    DeleteObject(hPenInner);

    int eyeY = y + ENEMY_H / 2 - 10;
    HBRUSH hWhite = CreateSolidBrush(RGB(255, 255, 255));
    SelectObject(hdc, hWhite);
    Ellipse(hdc, x + ENEMY_W / 2 - 14, eyeY - 5, x + ENEMY_W / 2 - 4, eyeY + 5);
    Ellipse(hdc, x + ENEMY_W / 2 + 4, eyeY - 5, x + ENEMY_W / 2 + 14, eyeY + 5);
    DeleteObject(hWhite);

    HBRUSH hPupil = CreateSolidBrush(RGB(0, 0, 0));
    SelectObject(hdc, hPupil);
    Ellipse(hdc, x + ENEMY_W / 2 - 10, eyeY - 3, x + ENEMY_W / 2 - 6, eyeY + 2);
    Ellipse(hdc, x + ENEMY_W / 2 + 6, eyeY - 3, x + ENEMY_W / 2 + 10, eyeY + 2);
    DeleteObject(hPupil);

    HPEN hTooth = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
    SelectObject(hdc, hTooth);
    MoveToEx(hdc, x + ENEMY_W / 2 - 6, y + ENEMY_H / 2 + 4, NULL);
    LineTo(hdc, x + ENEMY_W / 2, y + ENEMY_H / 2 + 12);
    LineTo(hdc, x + ENEMY_W / 2 + 6, y + ENEMY_H / 2 + 4);
    DeleteObject(hTooth);
    DeleteObject(hPen);
}

void DrawMenu(HDC hdc) {
    RECT full = { 0,0,GAME_WIDTH,GAME_HEIGHT };
    HBRUSH dark = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(hdc, &full, dark);
    DeleteObject(dark);

    int panelX = GAME_WIDTH / 2 - 280, panelY = GAME_HEIGHT / 2 - 200;
    int panelW = 560, panelH = 400;
    HPEN penGlow = CreatePen(PS_SOLID, 3, RGB(80, 160, 255));
    HBRUSH bgBrush = CreateSolidBrush(RGB(20, 25, 45));
    SelectObject(hdc, penGlow);
    SelectObject(hdc, bgBrush);
    RoundRect(hdc, panelX, panelY, panelX + panelW, panelY + panelH, 30, 30);
    DeleteObject(penGlow);
    DeleteObject(bgBrush);

    SetBkMode(hdc, TRANSPARENT);
    HFONT hBigFont = CreateFont(58, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    HFONT oldFont = (HFONT)SelectObject(hdc, hBigFont);
    SetTextColor(hdc, RGB(80, 180, 255));
    TextOutW(hdc, panelX + panelW / 2 - 160, panelY + 40, L"THUNDER STRIKE", 14);
    SelectObject(hdc, oldFont);
    DeleteObject(hBigFont);

    HFONT hSmall = CreateFont(22, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    oldFont = (HFONT)SelectObject(hdc, hSmall);
    SetTextColor(hdc, RGB(200, 220, 255));
    TextOutW(hdc, panelX + panelW / 2 - 110, panelY + 120, L"WASD / 方向键 移动", 18);
    TextOutW(hdc, panelX + panelW / 2 - 110, panelY + 160, L"自动连射", 8);
    TextOutW(hdc, panelX + panelW / 2 - 110, panelY + 200, L"道具: ❤️ 加血  ⚡ 双倍射速  🛡 无敌", 28);
    TextOutW(hdc, panelX + panelW / 2 - 110, panelY + 250, L"F11 全屏 / 窗口", 15);
    SelectObject(hdc, oldFont);
    DeleteObject(hSmall);

    int alpha = 128 + (int)(127 * sin(g_frameCount * 0.1f));
    SetTextColor(hdc, RGB(alpha, 255, alpha));
    HFONT hStart = CreateFont(32, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    oldFont = (HFONT)SelectObject(hdc, hStart);
    TextOutW(hdc, panelX + panelW / 2 - 130, panelY + 320, L"按下 SPACE 或 ENTER", 18);
    SelectObject(hdc, oldFont);
    DeleteObject(hStart);
}

void DrawGameOver(HDC hdc) {
    RECT full = { 0,0,GAME_WIDTH,GAME_HEIGHT };
    HBRUSH dark = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(hdc, &full, dark);
    DeleteObject(dark);

    int panelX = GAME_WIDTH / 2 - 220, panelY = GAME_HEIGHT / 2 - 150;
    int panelW = 440, panelH = 300;
    HPEN penRed = CreatePen(PS_SOLID, 3, RGB(220, 80, 80));
    HBRUSH bgBrush = CreateSolidBrush(RGB(30, 20, 40));
    SelectObject(hdc, penRed);
    SelectObject(hdc, bgBrush);
    RoundRect(hdc, panelX, panelY, panelX + panelW, panelY + panelH, 25, 25);
    DeleteObject(penRed);
    DeleteObject(bgBrush);

    SetBkMode(hdc, TRANSPARENT);
    HFONT hTitle = CreateFont(42, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    HFONT oldFont = (HFONT)SelectObject(hdc, hTitle);
    SetTextColor(hdc, RGB(255, 100, 100));
    TextOutW(hdc, panelX + panelW / 2 - 70, panelY + 30, L"GAME OVER", 9);
    SelectObject(hdc, oldFont);
    DeleteObject(hTitle);

    char buf[64];
    sprintf_s(buf, "得分: %d", g_score);
    HFONT hScore = CreateFont(30, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    oldFont = (HFONT)SelectObject(hdc, hScore);
    SetTextColor(hdc, RGB(255, 220, 100));
    TextOutA(hdc, panelX + panelW / 2 - 60, panelY + 100, buf, strlen(buf));
    sprintf_s(buf, "最高分: %d", g_highScore);
    TextOutA(hdc, panelX + panelW / 2 - 60, panelY + 140, buf, strlen(buf));
    SelectObject(hdc, oldFont);
    DeleteObject(hScore);

    HFONT hTip = CreateFont(22, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    oldFont = (HFONT)SelectObject(hdc, hTip);
    SetTextColor(hdc, RGB(180, 200, 255));
    TextOutW(hdc, panelX + panelW / 2 - 60, panelY + 200, L"按 R 重新开始", 12);
    TextOutW(hdc, panelX + panelW / 2 - 60, panelY + 240, L"ESC 退出", 8);
    SelectObject(hdc, oldFont);
    DeleteObject(hTip);
}

void GameDraw(HDC hdc) {
    TRIVERTEX vert[2] = { {0,0, 5,8,30,0xFF00}, {GAME_WIDTH,GAME_HEIGHT, 0,3,15,0xFF00} };
    GRADIENT_RECT gRect = { 0,1 };
    GradientFill(hdc, vert, 2, &gRect, 1, GRADIENT_FILL_RECT_V);

    for (auto& s : g_stars) {
        HBRUSH hStar = CreateSolidBrush(RGB(s.bright, s.bright, 255));
        HBRUSH oldBr = (HBRUSH)SelectObject(hdc, hStar);
        Ellipse(hdc, s.x, s.y, s.x + 2, s.y + 2);
        SelectObject(hdc, oldBr);
        DeleteObject(hStar);
    }

    for (auto& ep : g_explosions) {
        int size = 3 + (ep.life % 3);
        int alpha = min(255, ep.life * 10);
        BYTE r = (BYTE)(ep.r * alpha / 255);
        BYTE gv = (BYTE)(ep.g * alpha / 255);
        BYTE bv = (BYTE)(ep.b * alpha / 255);
        HBRUSH spark = CreateSolidBrush(RGB(r, gv, bv));
        HPEN penSpark = CreatePen(PS_SOLID, 1, RGB(r, gv, bv));
        SelectObject(hdc, penSpark);
        SelectObject(hdc, spark);
        Ellipse(hdc, (int)ep.x - size, (int)ep.y - size, (int)ep.x + size, (int)ep.y + size);
        DeleteObject(spark);
        DeleteObject(penSpark);
    }

    if (!g_items.empty()) {
        HFONT oldFont = (HFONT)SelectObject(hdc, g_hHeartFont);
        SetBkMode(hdc, TRANSPARENT);
        for (auto& it : g_items) DrawItem(hdc, it);
        SelectObject(hdc, oldFont);
    }

    if (g_state == STATE_PLAYING || g_state == STATE_GAMEOVER) {
        DrawPlayerPlane(hdc, g_playerX, g_playerY);
        for (auto& e : g_enemies) DrawEnemyPlane(hdc, e.x, e.y);
    }

    if (!g_heartParticles.empty()) {
        HFONT oldFont = (HFONT)SelectObject(hdc, g_hHeartFont);
        SetBkMode(hdc, TRANSPARENT);
        for (auto& hp : g_heartParticles) {
            int alpha = min(255, hp.life * 8);
            int r = 255, gv = 40, bv = 40;
            if (hp.life < 15) { r = r * hp.life / 15; gv = gv * hp.life / 15; bv = bv * hp.life / 15; }
            SetTextColor(hdc, RGB(r, gv, bv));
            TextOutW(hdc, (int)hp.x - 10, (int)hp.y - 10, L"\u2665", 1);
        }
        SelectObject(hdc, oldFont);
    }

    HBRUSH hBullet = CreateSolidBrush(RGB(255, 235, 80));
    HBRUSH oldBu = (HBRUSH)SelectObject(hdc, hBullet);
    for (auto& b : g_bullets) Rectangle(hdc, b.x, b.y, b.x + BULLET_W, b.y + BULLET_H);
    SelectObject(hdc, oldBu);
    DeleteObject(hBullet);

    if (g_state == STATE_PLAYING || g_state == STATE_GAMEOVER) {
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));
        char text[128];
        sprintf_s(text, "得分: %d", g_score);
        TextOutA(hdc, 30, 30, text, strlen(text));
        sprintf_s(text, "生命: %d", g_hp);
        TextOutA(hdc, 30, 60, text, strlen(text));
        sprintf_s(text, "速度: %.1f", g_currentEnemySpeed);
        TextOutA(hdc, 30, 90, text, strlen(text));
        if (g_shootBoostFrames > 0) {
            SetTextColor(hdc, RGB(255, 200, 50));
            TextOutW(hdc, 30, 120, L"\u26A1 双倍射速", 6);
        }
        if (g_invincibleFrames > 0) {
            SetTextColor(hdc, RGB(100, 200, 255));
            TextOutW(hdc, 30, 150, L"\U0001F6E1 无敌", 5);
        }
        SetTextColor(hdc, RGB(200, 200, 200));
        TextOutW(hdc, GAME_WIDTH - 145, 30, L"R: 重置", 5);
        TextOutW(hdc, GAME_WIDTH - 145, 55, L"F11: 全屏", 7);
    }

    if (g_state == STATE_MENU) DrawMenu(hdc);
    else if (g_state == STATE_GAMEOVER) DrawGameOver(hdc);
}

void ToggleFullscreen(HWND hWnd) {
    if (g_fullscreen) {
        SetWindowLongPtr(hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);
        SetWindowPos(hWnd, NULL, g_windowRect.left, g_windowRect.top,
            g_windowRect.right - g_windowRect.left, g_windowRect.bottom - g_windowRect.top,
            SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        g_fullscreen = false;
    }
    else {
        GetWindowRect(hWnd, &g_windowRect);
        SetWindowLongPtr(hWnd, GWL_STYLE, WS_POPUP);
        SetWindowPos(hWnd, NULL, 0, 0,
            GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
            SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        g_fullscreen = true;
    }
    SetFocus(hWnd);
}

LRESULT CALLBACK ThunderWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HDC hMemDC;
    static HBITMAP hMemBmp;

    switch (msg) {
    case WM_CREATE: {
        HDC hdc = GetDC(hWnd);
        hMemDC = CreateCompatibleDC(hdc);
        hMemBmp = CreateCompatibleBitmap(hdc, GAME_WIDTH, GAME_HEIGHT);
        SelectObject(hMemDC, hMemBmp);
        ReleaseDC(hWnd, hdc);
        g_hHeartFont = CreateFont(24, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        ResetGame();
        g_state = STATE_MENU;
        SetTimer(hWnd, 1, 15, NULL);
        break;
    }
    case WM_TIMER: {
        GameUpdate();
        HDC hdc = GetDC(hWnd);
        GameDraw(hMemDC);
        RECT client;
        GetClientRect(hWnd, &client);
        StretchBlt(hdc, 0, 0, client.right - client.left, client.bottom - client.top,
            hMemDC, 0, 0, GAME_WIDTH, GAME_HEIGHT, SRCCOPY);
        ReleaseDC(hWnd, hdc);
        break;
    }
    case WM_KEYDOWN: {
        if (wParam == VK_F11) { ToggleFullscreen(hWnd); return 0; }
        if (wParam == VK_ESCAPE) { DestroyWindow(hWnd); return 0; }
        if (wParam == 'R') { ResetGame(); return 0; }
        if (g_state == STATE_MENU && (wParam == VK_SPACE || wParam == VK_RETURN)) {
            ResetGame();
            return 0;
        }
        if (g_state == STATE_GAMEOVER) return 0;
        if (g_state == STATE_PLAYING) {
            switch (wParam) {
            case 'W': case VK_UP:    g_keyUp = true; break;
            case 'S': case VK_DOWN:  g_keyDown = true; break;
            case 'A': case VK_LEFT:  g_keyLeft = true; break;
            case 'D': case VK_RIGHT: g_keyRight = true; break;
            }
        }
        break;
    }
    case WM_KEYUP: {
        if (g_state == STATE_PLAYING) {
            switch (wParam) {
            case 'W': case VK_UP:    g_keyUp = false; break;
            case 'S': case VK_DOWN:  g_keyDown = false; break;
            case 'A': case VK_LEFT:  g_keyLeft = false; break;
            case 'D': case VK_RIGHT: g_keyRight = false; break;
            }
        }
        break;
    }
    case WM_CLOSE:
        DestroyWindow(hWnd);
        break;
    case WM_DESTROY:
        if (g_hHeartFont) DeleteObject(g_hHeartFont);
        DeleteObject(hMemBmp);
        DeleteDC(hMemDC);
        KillTimer(hWnd, 1);
        break;
    default:
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}

void RunThunderStrike(HINSTANCE hInstance) {
    WNDCLASSEX wc = { sizeof(WNDCLASSEX) };
    wc.lpfnWndProc = ThunderWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"ThunderStrikeGameClass";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    RegisterClassEx(&wc);

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    RECT rc = { 0,0,GAME_WIDTH,GAME_HEIGHT };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    int winW = rc.right - rc.left, winH = rc.bottom - rc.top;
    int x = (screenW - winW) / 2, y = (screenH - winH) / 2;
    HWND hWnd = CreateWindowEx(0, L"ThunderStrikeGameClass", L"雷霆战机",
        WS_OVERLAPPEDWINDOW, x, y, winW, winH,
        NULL, NULL, hInstance, NULL);
    if (!hWnd) return;

    GetWindowRect(hWnd, &g_windowRect);
    g_fullscreen = false;

    ShowWindow(hWnd, SW_SHOW);
    UpdateWindow(hWnd);
    SetFocus(hWnd);

    MSG msg;
    while (IsWindow(hWnd) && GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    UnregisterClass(L"ThunderStrikeGameClass", hInstance);
}