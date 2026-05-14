#include "BalatroGame.h"
#include <Windows.h>
#include <vector>
#include <string>
#include <algorithm>
#include <ctime>
#include <cstdlib>
#include <map>
#include <cmath>

using namespace std;

const int GAME_WIDTH = 1024;
const int GAME_HEIGHT = 768;
const int CARD_WIDTH = 80;
const int CARD_HEIGHT = 110;
const int HAND_X = 50;
const int HAND_Y = 500;
const int HAND_SPACING = 10;
const int MAX_HAND_SIZE = 8;
const int TARGET_BASE = 300;

static bool g_fullscreen = false;
static RECT g_windowRect;

enum Suit
{
    CLUB,
    DIAMOND,
    HEART,
    SPADE,
    WILD
};
const wchar_t *suitSymbol[] = {L"♣", L"♦", L"♥", L"♠", L"★"};
const wchar_t *rankName[] = {L"", L"A", L"2", L"3", L"4", L"5", L"6", L"7", L"8", L"9", L"10", L"J", L"Q", L"K"};

struct Card
{
    int number;
    int suit;
    string enhance;
    string seal;
    bool selected;
    int getRank() const { return number == 1 ? 14 : number; }
    int getNumber() const { return number; }
    int getSuit() const { return (enhance == "Stone") ? 5 : suit; }
    bool isWild() const { return enhance == "Wild" || suit == 4; }
};

vector<Card> deck, hand;
int score = 0, targetScore, remainingHands = 4, remainingDiscards = 4, money = 10, roundNum = 1;
bool gameActive = true, roundWon = false;
HWND g_hWnd;
HDC g_hMemDC;
HBITMAP g_hMemBmp;

struct LastPlay
{
    vector<Card> cards;
    int points;
    int framesLeft;
} lastPlay = {{}, 0, 0};

map<string, int> handLevels = {
    {"High Card", 1}, {"Pair", 1}, {"Two Pair", 1}, {"Three of a Kind", 1}, {"Straight", 1}, {"Flush", 1}, {"Full House", 1}, {"Four of a Kind", 1}, {"Straight Flush", 1}};
map<string, pair<int, int>> handBase = {
    {"High Card", {5, 1}}, {"Pair", {10, 2}}, {"Two Pair", {20, 2}}, {"Three of a Kind", {30, 3}}, {"Straight", {30, 4}}, {"Flush", {35, 4}}, {"Full House", {40, 4}}, {"Four of a Kind", {60, 7}}, {"Straight Flush", {100, 8}}};

void InitDeck()
{
    deck.clear();
    for (int suit = 0; suit < 4; suit++)
        for (int num = 1; num <= 13; num++)
            deck.push_back({num, suit, "", "", false});
    random_shuffle(deck.begin(), deck.end());
}

void DrawCards(int count)
{
    for (int i = 0; i < count && !deck.empty(); i++)
    {
        hand.push_back(deck.back());
        deck.pop_back();
    }
}

void ResetRound()
{
    InitDeck();
    random_shuffle(deck.begin(), deck.end());
    hand.clear();
    DrawCards(MAX_HAND_SIZE);
    remainingHands = 4;
    remainingDiscards = 4;
    score = 0;
    roundWon = false;
    gameActive = true;
    lastPlay.framesLeft = 0;
}

string EvaluateHand(vector<Card> cards, vector<Card> &scoringCards)
{
    if (cards.empty())
        return "None";
    if (cards.size() < 5)
    {
        scoringCards = cards;
        return "High Card";
    }
    sort(cards.begin(), cards.end(), [](const Card &a, const Card &b)
         { return a.getRank() < b.getRank(); });
    bool flush = true, straight = true;
    int suit0 = cards[0].getSuit();
    for (size_t i = 1; i < cards.size(); i++)
        if (cards[i].getSuit() != suit0 && !cards[i].isWild())
            flush = false;
    for (size_t i = 1; i < cards.size(); i++)
        if (cards[i].getRank() != cards[i - 1].getRank() + 1)
            straight = false;
    if (flush && straight)
    {
        scoringCards = cards;
        return "Straight Flush";
    }
    map<int, int> freq;
    for (auto &c : cards)
        freq[c.getNumber()]++;
    vector<int> counts;
    for (auto &p : freq)
        counts.push_back(p.second);
    sort(counts.rbegin(), counts.rend());
    if (counts[0] == 4)
    {
        scoringCards.clear();
        for (auto &c : cards)
            if (freq[c.getNumber()] == 4)
                scoringCards.push_back(c);
        return "Four of a Kind";
    }
    if (counts[0] == 3 && counts[1] == 2)
    {
        scoringCards.clear();
        for (auto &c : cards)
            if (freq[c.getNumber()] == 3 || freq[c.getNumber()] == 2)
                scoringCards.push_back(c);
        return "Full House";
    }
    if (flush)
    {
        scoringCards = cards;
        return "Flush";
    }
    if (straight)
    {
        scoringCards = cards;
        return "Straight";
    }
    if (counts[0] == 3)
    {
        scoringCards.clear();
        for (auto &c : cards)
            if (freq[c.getNumber()] == 3)
                scoringCards.push_back(c);
        return "Three of a Kind";
    }
    if (counts[0] == 2 && counts[1] == 2)
    {
        scoringCards.clear();
        for (auto &c : cards)
            if (freq[c.getNumber()] == 2)
                scoringCards.push_back(c);
        return "Two Pair";
    }
    if (counts[0] == 2)
    {
        scoringCards.clear();
        for (auto &c : cards)
            if (freq[c.getNumber()] == 2)
                scoringCards.push_back(c);
        return "Pair";
    }
    scoringCards.clear();
    scoringCards.push_back(*max_element(cards.begin(), cards.end(),
                                        [](const Card &a, const Card &b)
                                        { return a.getRank() < b.getRank(); }));
    return "High Card";
}

int CalculateScore(vector<Card> played)
{
    if (played.empty())
        return 0;
    vector<Card> scoringCards;
    string handType = EvaluateHand(played, scoringCards);
    int level = handLevels[handType];
    auto base = handBase[handType];
    int chips = base.first + (level - 1) * 10;
    int mult = base.second + (level - 1) * 2;
    for (auto &c : scoringCards)
    {
        if (c.enhance == "Stone")
            chips += 50;
        else
            chips += min(c.getNumber(), 10);
        if (c.enhance == "Bonus")
            chips += 30;
        if (c.enhance == "Mult")
            mult += 4;
    }
    return chips * mult;
}

void PlayHand()
{
    vector<Card> selected;
    vector<int> indices;
    for (size_t i = 0; i < hand.size(); i++)
    {
        if (hand[i].selected)
        {
            selected.push_back(hand[i]);
            indices.push_back((int)i);
        }
    }
    if (selected.empty())
        return;

    int addScore = CalculateScore(selected);
    score += addScore;
    lastPlay.cards = selected;
    lastPlay.points = addScore;
    lastPlay.framesLeft = 60;

    for (int i = (int)indices.size() - 1; i >= 0; i--)
        hand.erase(hand.begin() + indices[i]);
    DrawCards((int)indices.size());
    remainingHands--;
    for (auto &c : hand)
        c.selected = false;

    if (score >= targetScore)
        roundWon = true;
    else if (remainingHands == 0)
        gameActive = false;
}

void DiscardSelected()
{
    vector<int> indices;
    for (size_t i = 0; i < hand.size(); i++)
    {
        if (hand[i].selected)
            indices.push_back((int)i);
    }
    if (indices.empty())
        return;
    for (int i = (int)indices.size() - 1; i >= 0; i--)
        hand.erase(hand.begin() + indices[i]);
    DrawCards((int)indices.size());
    remainingDiscards--;
    for (auto &c : hand)
        c.selected = false;
}

void DrawCard(HDC hdc, int x, int y, const Card &card, bool selected)
{
    HBRUSH bgBrush = CreateSolidBrush(selected ? RGB(200, 230, 255) : RGB(255, 255, 240));
    HPEN pen = CreatePen(PS_SOLID, 2, RGB(0, 0, 0));
    SelectObject(hdc, pen);
    SelectObject(hdc, bgBrush);
    Rectangle(hdc, x, y, x + CARD_WIDTH, y + CARD_HEIGHT);
    DeleteObject(bgBrush);
    DeleteObject(pen);
    SetBkMode(hdc, TRANSPARENT);
    HFONT font = CreateFont(24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                            DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    HFONT oldFont = (HFONT)SelectObject(hdc, font);
    COLORREF color = (card.suit == 1 || card.suit == 2) ? RGB(200, 0, 0) : RGB(0, 0, 0);
    SetTextColor(hdc, color);
    wchar_t text[16];
    swprintf_s(text, L"%s", rankName[card.number]);
    TextOutW(hdc, x + 10, y + 10, text, (int)wcslen(text));
    swprintf_s(text, L"%s", suitSymbol[card.suit]);
    TextOutW(hdc, x + CARD_WIDTH - 30, y + CARD_HEIGHT - 30, text, (int)wcslen(text));
    if (card.enhance == "Bonus")
    {
        SetTextColor(hdc, RGB(0, 100, 0));
        TextOutW(hdc, x + 5, y + 50, L"+30", 3);
    }
    else if (card.enhance == "Mult")
    {
        SetTextColor(hdc, RGB(0, 100, 0));
        TextOutW(hdc, x + 5, y + 50, L"x4", 2);
    }
    SelectObject(hdc, oldFont);
    DeleteObject(font);
}

void DrawGame(HDC hdc)
{
    RECT rect = {0, 0, GAME_WIDTH, GAME_HEIGHT};
    HBRUSH bg = CreateSolidBrush(RGB(30, 60, 30));
    FillRect(hdc, &rect, bg);
    DeleteObject(bg);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 255, 255));
    HFONT infoFont = CreateFont(24, 0, 0, 0, FW_NORMAL, 0, 0, 0,
                                DEFAULT_CHARSET, 0, 0, 0, 0, L"Consolas");
    HFONT oldFont = (HFONT)SelectObject(hdc, infoFont);
    wchar_t info[256];
    swprintf_s(info, L"回合: %d  目标: %d  得分: %d  手牌: %d  弃牌: %d  金币: $%d",
               roundNum, targetScore, score, remainingHands, remainingDiscards, money);
    TextOutW(hdc, 20, 20, info, (int)wcslen(info));
    SelectObject(hdc, oldFont);
    DeleteObject(infoFont);

    for (size_t i = 0; i < hand.size(); i++)
    {
        int x = HAND_X + i * (CARD_WIDTH + HAND_SPACING);
        DrawCard(hdc, x, HAND_Y, hand[i], hand[i].selected);
    }

    if (lastPlay.framesLeft > 0)
    {
        SetTextColor(hdc, RGB(255, 220, 100));
        HFONT playFont = CreateFont(22, 0, 0, 0, FW_BOLD, 0, 0, 0,
                                    DEFAULT_CHARSET, 0, 0, 0, 0, L"Segoe UI");
        oldFont = (HFONT)SelectObject(hdc, playFont);
        wchar_t playInfo[256];
        swprintf_s(playInfo, L"打出的牌: ");
        TextOutW(hdc, 30, 400, playInfo, (int)wcslen(playInfo));
        int xOffset = 30 + 120;
        for (size_t i = 0; i < lastPlay.cards.size(); i++)
        {
            wchar_t cardStr[32];
            swprintf_s(cardStr, L"%s%s", rankName[lastPlay.cards[i].number], suitSymbol[lastPlay.cards[i].suit]);
            TextOutW(hdc, xOffset, 400, cardStr, (int)wcslen(cardStr));
            xOffset += 70;
        }
        swprintf_s(playInfo, L"  +%d 分", lastPlay.points);
        TextOutW(hdc, xOffset, 400, playInfo, (int)wcslen(playInfo));
        SelectObject(hdc, oldFont);
        DeleteObject(playFont);
        lastPlay.framesLeft--;
    }

    SetTextColor(hdc, RGB(255, 255, 0));
    HFONT tipFont = CreateFont(20, 0, 0, 0, FW_NORMAL, 0, 0, 0,
                               DEFAULT_CHARSET, 0, 0, 0, 0, L"Segoe UI");
    oldFont = (HFONT)SelectObject(hdc, tipFont);
    TextOutW(hdc, 50, HAND_Y - 60, L"单击牌选中/取消", 10);
    TextOutW(hdc, 300, HAND_Y - 60, L"按 P 打出所有选中牌", 14);
    TextOutW(hdc, 550, HAND_Y - 60, L"按 D 弃掉所有选中牌", 14);
    TextOutW(hdc, 800, HAND_Y - 60, L"按 R 重置回合", 10);
    TextOutW(hdc, 50, HAND_Y - 90, L"按 F11 全屏/窗口", 13);
    SelectObject(hdc, oldFont);
    DeleteObject(tipFont);
}

static void ToggleFullscreen(HWND hWnd)
{
    if (g_fullscreen)
    {
        SetWindowLongPtr(hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);
        SetWindowPos(hWnd, NULL, g_windowRect.left, g_windowRect.top,
                     g_windowRect.right - g_windowRect.left, g_windowRect.bottom - g_windowRect.top,
                     SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        g_fullscreen = false;
    }
    else
    {
        GetWindowRect(hWnd, &g_windowRect);
        SetWindowLongPtr(hWnd, GWL_STYLE, WS_POPUP);
        SetWindowPos(hWnd, NULL, 0, 0,
                     GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
                     SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        g_fullscreen = true;
    }
    SetFocus(hWnd);
}

LRESULT CALLBACK BalatroWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        HDC hdc = GetDC(hWnd);
        g_hMemDC = CreateCompatibleDC(hdc);
        g_hMemBmp = CreateCompatibleBitmap(hdc, GAME_WIDTH, GAME_HEIGHT);
        SelectObject(g_hMemDC, g_hMemBmp);
        ReleaseDC(hWnd, hdc);
        SetTimer(hWnd, 1, 16, NULL);
        break;
    }
    case WM_SIZE:
    {
        InvalidateRect(hWnd, NULL, FALSE);
        break;
    }
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT client;
        GetClientRect(hWnd, &client);
        SetStretchBltMode(hdc, HALFTONE);
        StretchBlt(hdc, 0, 0, client.right, client.bottom,
                   g_hMemDC, 0, 0, GAME_WIDTH, GAME_HEIGHT, SRCCOPY);
        EndPaint(hWnd, &ps);
        break;
    }
    case WM_TIMER:
    {
        HDC hdc = GetDC(hWnd);
        DrawGame(g_hMemDC);
        InvalidateRect(hWnd, NULL, FALSE);
        ReleaseDC(hWnd, hdc);
        break;
    }
    case WM_LBUTTONDOWN:
    {
        int x = LOWORD(lParam), y = HIWORD(lParam);
        RECT client;
        GetClientRect(hWnd, &client);
        float scaleX = (float)GAME_WIDTH / client.right;
        float scaleY = (float)GAME_HEIGHT / client.bottom;
        int gameX = (int)(x * scaleX);
        int gameY = (int)(y * scaleY);
        if (gameY >= HAND_Y && gameY <= HAND_Y + CARD_HEIGHT)
        {
            int idx = (gameX - HAND_X) / (CARD_WIDTH + HAND_SPACING);
            if (idx >= 0 && idx < (int)hand.size())
            {
                hand[idx].selected = !hand[idx].selected;
                InvalidateRect(hWnd, NULL, FALSE);
            }
        }
        break;
    }
    case WM_KEYDOWN:
    {
        if (wParam == VK_F11)
        {
            ToggleFullscreen(hWnd);
            return 0;
        }
        if (wParam == VK_ESCAPE)
        {
            DestroyWindow(hWnd);
            return 0;
        }
        if (wParam == 'P' && remainingHands > 0 && !roundWon)
            PlayHand();
        else if (wParam == 'D' && remainingDiscards > 0)
            DiscardSelected();
        else if (wParam == 'R')
            ResetRound();
        InvalidateRect(hWnd, NULL, FALSE);
        break;
    }
    case WM_CLOSE:
        DestroyWindow(hWnd);
        break;
    case WM_DESTROY:
        KillTimer(hWnd, 1);
        DeleteObject(g_hMemBmp);
        DeleteDC(g_hMemDC);
        break;
    default:
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}

void RunBalatroGame()
{
    srand((unsigned)time(NULL));
    targetScore = TARGET_BASE;
    InitDeck();
    DrawCards(MAX_HAND_SIZE);
    remainingHands = 4;
    remainingDiscards = 4;
    score = 0;
    money = 10;
    roundNum = 1;
    gameActive = true;
    roundWon = false;

    WNDCLASSEX wc = {sizeof(WNDCLASSEX)};
    wc.lpfnWndProc = BalatroWndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"BalatroGraphicClass";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    RegisterClassEx(&wc);

    HWND hWnd = CreateWindowEx(0, L"BalatroGraphicClass", L"图形卡牌 - 支持多弃牌 + 全屏",
                               WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                               GAME_WIDTH, GAME_HEIGHT, NULL, NULL, GetModuleHandle(NULL), NULL);
    if (!hWnd)
        return;

    GetWindowRect(hWnd, &g_windowRect);
    g_fullscreen = false;

    ShowWindow(hWnd, SW_SHOW);
    UpdateWindow(hWnd);

    MSG msg;
    while (IsWindow(hWnd) && GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    UnregisterClass(L"BalatroGraphicClass", GetModuleHandle(NULL));
}