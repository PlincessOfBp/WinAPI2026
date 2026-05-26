#include <Windows.h>
#include <tchar.h>
#include <stdlib.h>
#include <time.h>
#include <string>
#include <math.h>
#include <wingdi.h>
#pragma comment(lib, "msimg32.lib")

HINSTANCE g_hInst;
LPCTSTR lpszClass = L"Window Class";
LPCTSTR lpszWindowName = L"windows program 1";


enum GameScene {
	SCENE_FARM = 0,    // 농장 화면 (조성현 담당)
	SCENE_FISHING = 1  // 낚시 화면 (문선우 담당)
};
static GameScene g_currentScene = SCENE_FARM;

#define CLIENT_W 800
#define CLIENT_H 800

// 플레이어 바라보는 방향
enum PlayerDir {
	DIR_DOWN = 0,
	DIR_UP = 1,
	DIR_LEFT = 2,
	DIR_RIGHT = 3
};


#define PLAYER_SPRITE_FRAME_W       16   // 시트에서 한 프레임 가로 픽셀
#define PLAYER_SPRITE_FRAME_H       32   // 시트에서 한 프레임 세로 픽셀
#define PLAYER_SPRITE_COLS           4   // 한 줄에 들어있는 걷기 프레임 수

#define PLAYER_ROW_DOWN              0
#define PLAYER_ROW_UP                1
#define PLAYER_ROW_LEFT              2
#define PLAYER_ROW_RIGHT             3

#define PLAYER_DISPLAY_W            32
#define PLAYER_DISPLAY_H            64

#define PLAYER_ANIM_TICKS_PER_FRAME  6
// 스프라이트 배경 투명색 (마젠타)
#define PLAYER_SPRITE_TRANSPARENT   RGB(255, 0, 255)

// 플레이어 구조체
struct Player {
	int x, y;          // 좌상단 좌표 (픽셀)
	int w, h;          // 충돌/표시 크기
	int speed;         // 한 프레임당 이동 거리 (픽셀)
	PlayerDir dir;     // 마지막으로 바라본 방향
	int frameIndex;    // 현재 애니메이션 프레임 (0 ~ COLS-1)
	int frameTimer;    // 프레임 전환 카운터
	bool isMoving;     // 이동 중 여부 (정지 시 idle 프레임)
	// base 크기 16x28
	HBITMAP base[9]; // 0~2 앞, 3~5 옆, 6~8뒤 01020102, 34353435, 68676867 순으로 걸을 때마다 애니메이션 진행. 멈춰있을 땐 각각 0,3,6 프레임을 사용. 옆면은 참고로 오른쪽 방향 보고있음. 왼쪽 볼 땐 좌우반전 필요.
	// arm 크기 16x12
	HBITMAP base_arm[9]; // base와 동일.
	HBITMAP fishing_arm[10]; // 0~4 앞, 5~9 옆, 낚시 시작할 때 01234 순으로 애니메이션 진행. 이후 낚시 기다림 시간 중 4 고정. 물고기 물어서 낚시 게임 진행할 때엔 0101 반복. 옆면도 똑같이 낚시 시작 시 56789 애니메이션 진행. 낚시 기다릴 때 9 고정. 물고기가 물어서 낚시 게임 진행중일 때 5656 애니메이션 반복. 오른쪽 방향 보고있음. 왼쪽 볼 땐 좌우반전 필요.
};

static Player g_player = { 380, 380, PLAYER_DISPLAY_W, PLAYER_DISPLAY_H, 4, DIR_DOWN, 0, 0, false };

void 플레이어참고(HDC hDC, HBITMAP base[9], HBITMAP base_arm[])
{

	HDC hFishDC = CreateCompatibleDC(hDC);
	// 몸 베이스
	SelectObject(hFishDC, base[3]);
	TransparentBlt(hDC, 0, 0, 16, 28, hFishDC, 0, 0, 16, 28, RGB(255, 0, 255));

	//정면 팔
	// SelectObject(hFishDC, base_arm[4]);
	// TransparentBlt(hDC, 0, 4, 16, 17, hFishDC, 0, 0, 16, 17, RGB(255, 0, 255));

	// 옆면 팔
	SelectObject(hFishDC, base_arm[9]);
	TransparentBlt(hDC, 1, 5, 16, 14, hFishDC, 0, 0, 16, 14, RGB(255, 0, 255));

	DeleteDC(hFishDC);
}

// 입력 상태
static bool g_keyLeft = false;
static bool g_keyRight = false;
static bool g_keyUp = false;
static bool g_keyDown = false;

// 농장 → 낚시 트리거 (오른쪽 위, 빨간 지붕 집 옆쪽)
// 사용자가 표시한 빨간 동그라미 위치에 맞춰 조정.
#define TRIGGER_TO_FISH_X   720
#define TRIGGER_TO_FISH_Y   200
#define TRIGGER_TO_FISH_W    80
#define TRIGGER_TO_FISH_H    80

// 씬 전환 직후 같은 트리거에서 핑퐁 방지용 쿨다운
static int g_sceneCooldown = 0;

// 낚시터 이동 불가 영역
#define FISHING_BLOCK_COUNT 21
static RECT g_fishingBlockRects[FISHING_BLOCK_COUNT] = {
	{ 325, 380, 415, 600 },
	{ 415, 400, 430, 600 },
	{ 430, 420, 445, 600 },
	{ 445, 435, 465, 600 },
	{ 465, 455, 470, 600 },
	{ 465, 455, 770, 550 },
	{ 770, 435, 800, 550 },
	{ 530, 550, 800, 600 },
	{ 545, 600, 800, 620 },
	{ 610, 620, 800, 800 },
	{ 250, 660, 610, 800 },
	{ 105, 445, 250, 800 },
	{ 85,  445, 250, 655 },
	{ 0,   710, 105, 800 },
	{ 0,   655,  15, 710 },
	{ 0,   305,  30, 655 },
	{ 85,  305, 125, 405 },
	{ 125, 325, 140, 405 },
	{ 140, 340, 165, 405 },
	{ 165, 360, 180, 405 },
	{ 180, 380, 250, 405 },
};

// 윗부분 이동 불가 영역 (별도 묶음)
#define FISHING_BLOCK_TOP_COUNT 4
static RECT g_fishingBlockTopRects[FISHING_BLOCK_TOP_COUNT] = {
	{ 0,   0, 285,  75 },
	{ 285, 0, 340,  55 },
	{ 340, 0, 470,  40 },
	{ 545, 0, 800,  80 },
};

// 낚시 가능 영역
struct FishingAreaInfo {
	RECT rect;
	PlayerDir dir; // 이 영역에서 낚시 가능한 플레이어 방향
};

#define FISHING_AREA_COUNT 7
static FishingAreaInfo g_fishingAreaInfos[FISHING_AREA_COUNT] = {
	{ { 250, 635, 610, 655 }, DIR_DOWN  }, // 아래
	{ {  15, 690, 100, 710 }, DIR_DOWN  }, // 아래
	{ {  85, 425, 250, 445 }, DIR_DOWN  }, // 아래
	{ { 250, 445, 270, 655 }, DIR_LEFT  }, // 왼쪽 방향
	{ { 590, 620, 610, 655 }, DIR_RIGHT }, // 오른쪽 방향
	{ {  80, 660, 100, 710 }, DIR_RIGHT }, // 오른쪽 방향
	{ {  65, 445,  85, 655 }, DIR_RIGHT }, // 오른쪽 방향
};

// 낚시터 → 농장 이동 트리거
#define TRIGGER_TO_FARM_X    0
#define TRIGGER_TO_FARM_Y   75
#define TRIGGER_TO_FARM_W   20
#define TRIGGER_TO_FARM_H  230

// 두 사각형 겹침 검사
static bool RectOverlap(int ax, int ay, int aw, int ah,
	int bx, int by, int bw, int bh) {
	return (ax + aw > bx) && (ax < bx + bw) &&
		(ay + ah > by) && (ay < by + bh);
}

// 낚시터 이동 불가 영역과 충돌 검사
static bool IsBlockedInFishing(int px, int py, int pw, int ph) {
	int i;
	for (i = 0; i < FISHING_BLOCK_COUNT; i++) {
		if (RectOverlap(px, py, pw, ph,
			g_fishingBlockRects[i].left,
			g_fishingBlockRects[i].top,
			g_fishingBlockRects[i].right - g_fishingBlockRects[i].left,
			g_fishingBlockRects[i].bottom - g_fishingBlockRects[i].top))
			return true;
	}
	for (i = 0; i < FISHING_BLOCK_TOP_COUNT; i++) {
		if (RectOverlap(px, py, pw, ph,
			g_fishingBlockTopRects[i].left,
			g_fishingBlockTopRects[i].top,
			g_fishingBlockTopRects[i].right - g_fishingBlockTopRects[i].left,
			g_fishingBlockTopRects[i].bottom - g_fishingBlockTopRects[i].top))
			return true;
	}
	return false;
}

// 낚시 가능 영역과 겹치는지 + 플레이어 방향이 맞는지 검사
static bool IsInFishingArea(int px, int py, int pw, int ph, PlayerDir dir) {
	int i;
	for (i = 0; i < FISHING_AREA_COUNT; i++) {
		if (g_fishingAreaInfos[i].dir != dir)
			continue;
		if (RectOverlap(px, py, pw, ph,
			g_fishingAreaInfos[i].rect.left,
			g_fishingAreaInfos[i].rect.top,
			g_fishingAreaInfos[i].rect.right - g_fishingAreaInfos[i].rect.left,
			g_fishingAreaInfos[i].rect.bottom - g_fishingAreaInfos[i].rect.top))
			return true;
	}
	return false;
}

// 플레이어 한 프레임 업데이트
void UpdatePlayer() {
	int dx = 0, dy = 0;
	if (g_keyLeft)  dx -= g_player.speed;
	if (g_keyRight) dx += g_player.speed;
	if (g_keyUp)    dy -= g_player.speed;
	if (g_keyDown)  dy += g_player.speed;

	// 8방향 이동 시 대각선 속도 보정 
	if (dx != 0 && dy != 0) {
		dx = (dx * 7) / 10;
		dy = (dy * 7) / 10;
	}

	// 낚시터 씬: 이동 불가 영역 충돌 처리
	// 발 영역: 플레이어 하단 중앙 (좌우 여백 w/4, 상단 h*3/4 아래)
	if (g_currentScene == SCENE_FISHING) {
		int footOffX = g_player.w / 4;
		int footOffY = g_player.h * 3 / 4;
		int footW = g_player.w / 2;
		int footH = g_player.h / 4;

		// X축 먼저 시도
		int nextX = g_player.x + dx;
		if (IsBlockedInFishing(nextX + footOffX, g_player.y + footOffY, footW, footH)) {
			nextX = g_player.x; // X 이동 취소
			dx = 0;
		}
		// Y축 시도
		int nextY = g_player.y + dy;
		if (IsBlockedInFishing(nextX + footOffX, nextY + footOffY, footW, footH)) {
			nextY = g_player.y; // Y 이동 취소
			dy = 0;
		}
		g_player.x = nextX;
		g_player.y = nextY;
	}
	else {
		g_player.x += dx;
		g_player.y += dy;
	}

	// 마지막으로 바라본 방향 갱신 (좌우 우선)
	if (dx < 0)      g_player.dir = DIR_LEFT;
	else if (dx > 0) g_player.dir = DIR_RIGHT;
	else if (dy < 0) g_player.dir = DIR_UP;
	else if (dy > 0) g_player.dir = DIR_DOWN;

	// 애니메이션 프레임 갱신
	// 각 방향 걷기 시퀀스: 정면 0,1,0,2 / 옆면 3,4,3,5 / 후면 6,8,6,7
	static int animSeqFront[4] = { 0, 1, 0, 2 };
	static int animSeqSide[4] = { 3, 4, 3, 5 };
	static int animSeqBack[4] = { 6, 8, 6, 7 };

	bool moving = (dx != 0 || dy != 0);
	g_player.isMoving = moving;
	if (moving) {
		g_player.frameTimer++;
		if (g_player.frameTimer >= PLAYER_ANIM_TICKS_PER_FRAME) {
			g_player.frameTimer = 0;
			g_player.frameIndex = (g_player.frameIndex + 1) % 4;
		}
	}
	else {
		g_player.frameIndex = 0;  // 정지 시 첫 프레임(idle)
		g_player.frameTimer = 0;
	}

	// 화면 경계 충돌 처리 
	if (g_player.x < 0) g_player.x = 0;
	if (g_player.y < 0) g_player.y = 0;
	if (g_player.x + g_player.w > CLIENT_W) g_player.x = CLIENT_W - g_player.w;
	if (g_player.y + g_player.h > CLIENT_H) g_player.y = CLIENT_H - g_player.h;

	// 씬 전환 쿨다운
	if (g_sceneCooldown > 0) {
		g_sceneCooldown--;
		return;
	}

	//낚시터 가는 길에 닿으면 SCENE_FISHING으로 전환
	if (g_currentScene == SCENE_FARM && RectOverlap(g_player.x, g_player.y, g_player.w, g_player.h,
		TRIGGER_TO_FISH_X, TRIGGER_TO_FISH_Y, TRIGGER_TO_FISH_W, TRIGGER_TO_FISH_H)) {
		g_currentScene = SCENE_FISHING;
		// 낚시터 출입구 앞에 배치 (농장으로이동영역그리기 트리거 오른쪽)
		g_player.x = TRIGGER_TO_FARM_X + TRIGGER_TO_FARM_W + 10;
		g_player.y = TRIGGER_TO_FARM_Y + TRIGGER_TO_FARM_H / 2 - g_player.h / 2;
		g_sceneCooldown = 30;
	}

	// 농장 가는 길에 닿으면 SCENE_FARM으로 전환
	if (g_currentScene == SCENE_FISHING && RectOverlap(g_player.x, g_player.y, g_player.w, g_player.h,
		TRIGGER_TO_FARM_X, TRIGGER_TO_FARM_Y, TRIGGER_TO_FARM_W, TRIGGER_TO_FARM_H)) {
		g_currentScene = SCENE_FARM;
		// 농장 출입구 앞에 배치 (농장→낚시 트리거 왼쪽)
		g_player.x = TRIGGER_TO_FISH_X - g_player.w - 10;
		g_player.y = TRIGGER_TO_FISH_Y + TRIGGER_TO_FISH_H / 2 - g_player.h / 2;
		g_sceneCooldown = 30;
	}
}


// 낚시 단계
enum FishingPhase {
	FISHING_PHASE_NONE = 0,        // 낚시 안 함
	FISHING_PHASE_CAST = 1,        // 캐스팅 애니메이션 (0→4 or 5→9)
	FISHING_PHASE_WAIT = 2,        // 물고기 기다리는 중 (마지막 프레임 고정)
	FISHING_PHASE_BITE_NOTICE = 3, // 물었다! 표시 (~1초)
	FISHING_PHASE_GAME = 4         // 낚시 미니게임 진행
};
static FishingPhase g_fishingPhase = FISHING_PHASE_NONE;

// 물고기 기다림 시간 범위 (단위: 타이머 틱, 타이머 2002는 100ms마다 발생)
// 5초 = 50틱, 10초 = 100틱. 외부에서 조정 가능.
static int g_fishWaitMin = 50;  // 최소 기다림 틱 수
static int g_fishWaitMax = 100; // 최대 기다림 틱 수
static int g_fishWaitTimer = 0; // 현재 기다림 카운터
static int g_fishWaitTarget = 0; // 이번에 기다릴 틱 수 (랜덤)

// 캐스팅 애니메이션 상태
static int g_castFrameIdx = 0;   // 현재 캐스팅 진행 프레임 (0~4)
static int g_castFrameTimer = 0; // 프레임 유지 카운터
#define CAST_TICKS_PER_FRAME 4   // 캐스팅 프레임 하나당 유지할 타이머 틱 수

// 물었다! 표시 타이머 (100ms 타이머 기준, 10틱 = 1초)
static int g_biteNoticeTimer = 0;
#define BITE_NOTICE_TICKS 10     // 물었다! 표시 유지 틱 수 (1초)

// 낚시 중 팔 인덱스 (DrawPlayer에서 사용)
static int g_fishingArmIdx = 0;

// 낚시 게임 중 팔 애니메이션 (0101 or 5656 반복)
static int g_fishGameArmTimer = 0;
static int g_fishGameArmFrame = 0; // 0 or 1 (앞면: 0,1 / 옆면: 5,6 중 선택용)
#define FISH_GAME_ARM_TICKS 5    // 낚시 게임 중 팔 프레임 전환 속도

// 물었다! 비트맵 (DrawPlayer에서 접근하기 위해 전역으로 관리)
static HBITMAP g_hBitmap_biteNotice = NULL;

// 비트맵을 좌우반전하여 TransparentBlt로 출력하는 헬퍼 함수
// srcW, srcH: 원본 픽셀 크기 / dstW, dstH: 화면 출력 크기
static void DrawFlipped(HDC hDC, HDC memDC, HBITMAP bmp,
	int dstX, int dstY, int dstW, int dstH,
	int srcW, int srcH) {
	HDC flipDC = CreateCompatibleDC(hDC);
	HBITMAP flipBmp = CreateCompatibleBitmap(hDC, srcW, srcH);
	HBITMAP flipOld = (HBITMAP)SelectObject(flipDC, flipBmp);
	SelectObject(memDC, bmp);
	StretchBlt(flipDC, srcW - 1, 0, -srcW, srcH, memDC, 0, 0, srcW, srcH, SRCCOPY);
	TransparentBlt(hDC, dstX, dstY, dstW, dstH, flipDC, 0, 0, srcW, srcH, RGB(255, 0, 255));
	SelectObject(flipDC, flipOld);
	DeleteObject(flipBmp);
	DeleteDC(flipDC);
}

void DrawPlayer(HDC hDC) {
	int x = g_player.x;
	int y = g_player.y;

	// 방향별 걷기 시퀀스
	static int animSeqFront[4] = { 0, 1, 0, 2 };
	static int animSeqSide[4] = { 3, 4, 3, 5 };
	static int animSeqBack[4] = { 6, 8, 6, 7 };

	// 몸 인덱스 결정
	int baseIdx = 0;
	if (g_fishingPhase != FISHING_PHASE_NONE) {
		// 낚시 중: 정지 프레임 고정 (앞면=0, 옆면=3, 후면=6)
		if (g_player.dir == DIR_DOWN)       baseIdx = 0;
		else if (g_player.dir == DIR_UP)    baseIdx = 6;
		else                                baseIdx = 3;
	}
	else {
		if (g_player.dir == DIR_DOWN)       baseIdx = animSeqFront[g_player.frameIndex];
		else if (g_player.dir == DIR_UP)    baseIdx = animSeqBack[g_player.frameIndex];
		else                                baseIdx = animSeqSide[g_player.frameIndex];
	}

	if (g_player.base[baseIdx] == NULL)
		return;

	HDC memDC = CreateCompatibleDC(hDC);

	int dispBodyW = 32; int dispBodyH = 56;
	int dispArmW = 32; int dispArmH = 24;
	int armOffY = 20; // 원본과 동일

	// 낚시 팔 크기 (임시) 
	int dispFArmFrontW = 32; int dispFArmFrontH = 34; // 앞면 낚시팔 (16x17 → 2배)
	int dispFArmSideW = 32; int dispFArmSideH = 28; // 옆면 낚시팔 (16x14 → 2배)
	// 낚시 팔 X 오프셋 
	int fishArmOffX = 0;

	bool isFishingState = (g_fishingPhase != FISHING_PHASE_NONE);

	if (g_player.dir == DIR_LEFT) {
		// 몸 (좌우반전)
		DrawFlipped(hDC, memDC, g_player.base[baseIdx],
			x, y, dispBodyW, dispBodyH, 16, 28);

		if (isFishingState) {
			// 낚시 팔 (옆면, 좌우반전)
			if (g_player.fishing_arm[g_fishingArmIdx] != NULL) {
				DrawFlipped(hDC, memDC, g_player.fishing_arm[g_fishingArmIdx],
					x + fishArmOffX - 2, y + armOffY - 10, dispFArmSideW, dispFArmSideH, 16, 14);
			}
		}
		else {
			// 일반 팔 — 원본과 동일 (좌우반전)
			if (g_player.base_arm[baseIdx] != NULL) {
				DrawFlipped(hDC, memDC, g_player.base_arm[baseIdx],
					x, y + armOffY, dispArmW, dispArmH, 16, 12);
			}
		}
	}
	else {
		// 몸
		SelectObject(memDC, g_player.base[baseIdx]);
		TransparentBlt(hDC, x, y, dispBodyW, dispBodyH,
			memDC, 0, 0, 16, 28, RGB(255, 0, 255));

		if (isFishingState) {
			// 낚시 팔
			if (g_player.fishing_arm[g_fishingArmIdx] != NULL) {
				SelectObject(memDC, g_player.fishing_arm[g_fishingArmIdx]);
				if (g_player.dir == DIR_DOWN) {
					TransparentBlt(hDC, x + fishArmOffX, y + armOffY - 10 , dispFArmFrontW, dispFArmFrontH,
						memDC, 0, 0, 16, 17, RGB(255, 0, 255));
				}
				else {
					TransparentBlt(hDC, x + fishArmOffX + 2, y + armOffY - 10, dispFArmSideW, dispFArmSideH,
						memDC, 0, 0, 16, 14, RGB(255, 0, 255));
				}
			}
		}
		else {
			// 일반 팔 — 원본과 동일
			if (g_player.base_arm[baseIdx] != NULL) {
				SelectObject(memDC, g_player.base_arm[baseIdx]);
				TransparentBlt(hDC, x, y + armOffY, dispArmW, dispArmH,
					memDC, 0, 0, 16, 12, RGB(255, 0, 255));
			}
		}
	}

	// 물었다! 표시 (머리 위 약 1초간)
	if (g_fishingPhase == FISHING_PHASE_BITE_NOTICE) {
		if (g_hBitmap_biteNotice != NULL) {
			SelectObject(memDC, g_hBitmap_biteNotice);
			// 원본 74x28, 머리 위에 표시 (x 중앙 정렬, y - 30)
			TransparentBlt(hDC, x - 21, y - 30, 74, 28,
				memDC, 0, 0, 74, 28, RGB(255, 0, 255));
		}
	}

	DeleteDC(memDC);
}


void DrawFishTriggerHint(HDC hDC) {
	// 반투명 느낌의 외곽선만 표시
	HPEN p = CreatePen(PS_DOT, 2, RGB(255, 255, 0));
	HPEN op = (HPEN)SelectObject(hDC, p);
	HBRUSH nb = (HBRUSH)GetStockObject(NULL_BRUSH);
	HBRUSH ob = (HBRUSH)SelectObject(hDC, nb);
	Rectangle(hDC, TRIGGER_TO_FISH_X, TRIGGER_TO_FISH_Y,
		TRIGGER_TO_FISH_X + TRIGGER_TO_FISH_W,
		TRIGGER_TO_FISH_Y + TRIGGER_TO_FISH_H);
	SelectObject(hDC, op);
	SelectObject(hDC, ob);
	DeleteObject(p);

	SetBkMode(hDC, TRANSPARENT);
	SetTextColor(hDC, RGB(255, 255, 220));
	const wchar_t* hint = L"→ 낚시터";
	TextOut(hDC, TRIGGER_TO_FISH_X + 10, TRIGGER_TO_FISH_Y + 30, hint, (int)wcslen(hint));
}



// 낚시 단계
enum FishMovementType {
	FISH_MOVE_RANDOM = 0,      // 기본: 랜덤하게 위아래로 움직임
	FISH_MOVE_FAST_UP = 1,     // 위로 올라갈 때 빠르게
	FISH_MOVE_FAST_DOWN = 2,   // 아래로 내려갈 때 빠르게
	FISH_MOVE_IRREGULAR = 3    // 움직임이 크고 불규칙적
};

// 각 움직임 유형별 등장 비율 (합산 100 기준, 외부에서 조정 가능)
static int fishMovementRatio[4] = { 25, 25, 25, 25 };

struct GreenBar {
	int x, y; // 게이지 바의 위치
	int width, height; // 게이지 바의 크기
};

struct TargetFish {
	int x, y; // 물고기의 위치
	int width, height; // 물고기의 크기
	bool inGreenBar; // 물고기가 초록색 영역 안에 있는지 여부.
	int moveDirY;       // 현재 이동 방향 (1: 아래, -1: 위)
	int moveTimer;      // 방향 전환 타이머 카운터
	int moveInterval;   // 방향 유지 프레임 수
	FishMovementType movementType; // 이번 낚시의 물고기 움직임 유형
};

struct FishingGage { // 낚시 외부 게이지 정보
	int x, y; // 게이지의 위치
	int width, height; // 게이지의 크기
	int current;  // 현재 게이지 값 (0 ~ maxVal)
	int maxVal;   // 게이지 최대값
};

// 물고기 움직임 유형을 등장 비율에 따라 랜덤 선택
FishMovementType SelectFishMovementType() {
	int total = 0;
	for (int i = 0; i < 4; i++) total += fishMovementRatio[i];
	int r = rand() % total;
	int acc = 0;
	for (int i = 0; i < 4; i++) {
		acc += fishMovementRatio[i];
		if (r < acc) return (FishMovementType)i;
	}
	return FISH_MOVE_RANDOM;
}

// 물고기가 초록 게이지 영역 안에 있는지 체크
bool CheckFishInGreenBar(const struct TargetFish& fish, const struct GreenBar& bar) {
	int fishCenterY = fish.y + fish.height / 2;
	return (fishCenterY >= bar.y && fishCenterY <= bar.y + bar.height);
}

// 물고기 움직임 처리 (타이머마다 호출)
void UpdateFishMovement(struct TargetFish& fish) {
	const int FISH_MIN_Y = 5;
	const int FISH_MAX_Y = 125;

	fish.moveTimer++;

	switch (fish.movementType) {
	case FISH_MOVE_RANDOM:
		if (fish.moveTimer >= fish.moveInterval) {
			if (rand() % 2 == 0) {
				fish.moveDirY = 1;
			}
			else {
				fish.moveDirY = -1;
			}
			fish.moveInterval = 5 + rand() % 8;
			fish.moveTimer = 0;
		}
		fish.y += fish.moveDirY * 3;
		break;

	case FISH_MOVE_FAST_UP:
		if (fish.moveTimer >= fish.moveInterval) {
			if (rand() % 2 == 0) {
				fish.moveDirY = 1;
			}
			else {
				fish.moveDirY = -1;
			}
			fish.moveInterval = 6 + rand() % 6;
			fish.moveTimer = 0;
		}
		fish.y += fish.moveDirY * (fish.moveDirY == -1 ? 7 : 2); // 위로 올라갈 때 빠름
		break;

	case FISH_MOVE_FAST_DOWN:
		if (fish.moveTimer >= fish.moveInterval) {
			if (rand() % 2 == 0) {
				fish.moveDirY = 1;
			}
			else {
				fish.moveDirY = -1;
			}
			fish.moveInterval = 6 + rand() % 6;
			fish.moveTimer = 0;
		}
		fish.y += fish.moveDirY * (fish.moveDirY == 1 ? 7 : 2); // 아래로 내려갈 때 빠름
		break;

	case FISH_MOVE_IRREGULAR:
		if (fish.moveTimer >= fish.moveInterval) {
			if (rand() % 2 == 0) {
				fish.moveDirY = 1;
			}
			else {
				fish.moveDirY = -1;
			}
			fish.moveInterval = 1 + rand() % 4; // 매우 짧은 간격으로 방향 전환
			fish.moveTimer = 0;
		}
		fish.y += fish.moveDirY * (4 + rand() % 5); // 속도도 매번 랜덤
		break;
	}

	// 범위 제한
	if (fish.y < FISH_MIN_Y) {
		fish.y = FISH_MIN_Y;
		fish.moveDirY = 1;
	}
	if (fish.y > FISH_MAX_Y) {
		fish.y = FISH_MAX_Y;
		fish.moveDirY = -1;
	}
}

void FishingGameLogic(HDC hDC, HBRUSH hBrush, HBRUSH oldBrush, HPEN hPen, HPEN oldPen, HBITMAP hBitmap_fishing[4], struct GreenBar greenBar, struct TargetFish targetFish, struct FishingGage fishingGage) {
	// 낚시 게임의 핵심 로직을 구현하는 함수
	// 게이지 바 속에서 물고기 위 아래로 움직이는 것
	// 게이지 바 속 초록색 움직이는 것
	// 초록색 영역 안에 물고기 있으면 게이지 바 옆에 다른 게이지 채워지고 아니면 게이지 하락. 끝까지 하락하면 낚시 실패. 게이지 다 차면 낚시 성공.
	// 물고기 움직임 유형 : 기본 움직임 (랜덤하게 위아래로 움직인다.), 위로 올라갈 때 빨라짐, 아래로 내려갈 때 빨라짐, 움직임 크고 불규칙적.

	HDC hFishDC = CreateCompatibleDC(hDC);
	// 낚시 게이지 바 배경
	SelectObject(hFishDC, hBitmap_fishing[0]);
	TransparentBlt(hDC, 0, 0, 37, 149, hFishDC, 0, 0, 37, 149, RGB(255, 0, 255));

	// 물고기를 안에 둬야 하는 초록 게이지
	SelectObject(hFishDC, hBitmap_fishing[1]);
	TransparentBlt(hDC, greenBar.x, greenBar.y, greenBar.width, greenBar.height, hFishDC, 0, 0, 10, 10, RGB(255, 0, 255));

	if (targetFish.inGreenBar) {
		SelectObject(hFishDC, hBitmap_fishing[2]);
		TransparentBlt(hDC, targetFish.x, targetFish.y, targetFish.width, targetFish.height, hFishDC, 20, 0, 19, 20, RGB(255, 0, 255));
	}
	else {
		SelectObject(hFishDC, hBitmap_fishing[2]);
		TransparentBlt(hDC, targetFish.x, targetFish.y, targetFish.width, targetFish.height, hFishDC, 0, 0, 19, 20, RGB(255, 0, 255));
	}

	DeleteDC(hFishDC);

	hPen = CreatePen(PS_SOLID, 1, RGB(255, 0, 0));
	oldPen = (HPEN)SelectObject(hDC, hPen);
	hBrush = CreateSolidBrush(RGB(255, 0, 0));
	oldBrush = (HBRUSH)SelectObject(hDC, hBrush);

	int gageTop = fishingGage.height;                          // 전체 게이지의 top 좌표
	int gageBottom = fishingGage.y + fishingGage.height;          // 전체 게이지의 bottom 좌표 
	int gageTotalH = gageBottom - gageTop;                        // 전체 게이지 높이 = fishingGage.y
	int filledTop = gageBottom - (gageTotalH * fishingGage.current / fishingGage.maxVal);
	Rectangle(hDC, fishingGage.width, filledTop, fishingGage.x + fishingGage.width, gageBottom);
	SelectObject(hDC, oldPen);
	SelectObject(hDC, oldBrush);
	DeleteObject(hPen);
	DeleteObject(hBrush);
}

/*
void 낚시터플레이어이동못하는영역그리기(HDC hDC) {
	// 낚시터에서 플레이어가 이동할 수 없는 영역을 그리는 함수
	// 여기서 그린 영역의 Rect 참고해서 플레이어 이동 업데이트 시 해당 영역과 충돌 검사해서 이동 막아야 함.
	HBRUSH Brush = CreateSolidBrush(RGB(255, 0, 0));
	RECT Rect;
	Rect = { 325, 380, 415, 600 }; //중앙 집 부분 부터 시작
	FillRect(hDC, &Rect, Brush);
	Rect = { 415, 400, 430, 600 };
	FillRect(hDC, &Rect, Brush);
	Rect = { 430, 420, 445, 600 };
	FillRect(hDC, &Rect, Brush);
	Rect = { 445, 435, 465, 600 };
	FillRect(hDC, &Rect, Brush);
	Rect = { 465, 455, 470, 600 };
	FillRect(hDC, &Rect, Brush);
	Rect = { 465, 455, 770, 550 };
	FillRect(hDC, &Rect, Brush);
	Rect = { 770, 435, 800, 550 };// 오른쪽 끝으로 도달함
	FillRect(hDC, &Rect, Brush);
	Rect = { 530, 550, 800, 600 };
	FillRect(hDC, &Rect, Brush);
	Rect = { 545, 600, 800, 620 };
	FillRect(hDC, &Rect, Brush);
	Rect = { 610, 620, 800, 800 };
	FillRect(hDC, &Rect, Brush);
	Rect = { 250, 660, 610, 800 }; // 아래쪽 바다 부분
	FillRect(hDC, &Rect, Brush);
	Rect = { 105, 445, 250, 800 };
	FillRect(hDC, &Rect, Brush);
	Rect = { 85, 445, 250, 655 };
	FillRect(hDC, &Rect, Brush);
	Rect = { 0, 710, 105, 800 }; // 왼쪽 끝으로 도달함
	FillRect(hDC, &Rect, Brush);
	Rect = { 0, 655, 15, 710 };
	FillRect(hDC, &Rect, Brush);
	Rect = { 0, 305, 30, 655 };
	FillRect(hDC, &Rect, Brush);
	Rect = { 85, 305, 125, 405 };
	FillRect(hDC, &Rect, Brush);
	Rect = { 125, 325, 140, 405 };
	FillRect(hDC, &Rect, Brush);
	Rect = { 140, 340, 165, 405 };
	FillRect(hDC, &Rect, Brush);
	Rect = { 165, 360, 180, 405 };
	FillRect(hDC, &Rect, Brush);
	Rect = { 180, 380, 250, 405 };
	FillRect(hDC, &Rect, Brush);
	Rect = { 0, 0, 285, 75 }; // 윗부분
	FillRect(hDC, &Rect, Brush);
	Rect = { 285, 0, 340, 55 };
	FillRect(hDC, &Rect, Brush);
	Rect = { 340, 0, 470, 40 };
	FillRect(hDC, &Rect, Brush);
	Rect = { 545, 0, 800, 80 };
	FillRect(hDC, &Rect, Brush);
	DeleteObject(Brush);
}

void 낚시가능영역그리기(HDC hDC)
{
	// 낚시터에서 플레이어가 낚시할 수 있는 영역을 그리는 함수
	// 여기서 그린 영역의 Rect 참고해서 플레이어 이동 업데이트 시 해당 영역과 겹치는지 검사해서 낚시 가능 여부 설정해야 함.
	HBRUSH Brush = CreateSolidBrush(RGB(0, 255, 0));
	RECT Rect;
	Rect = { 250, 635, 610, 655 }; // 낚시 가능한 영역 (아래)
	FillRect(hDC, &Rect, Brush);
	Rect = { 15, 690, 100, 710 };
	FillRect(hDC, &Rect, Brush);
	Rect = { 85, 425, 250, 445 };
	FillRect(hDC, &Rect, Brush);
	Rect = { 250, 445, 270, 655 }; // 낚시 가능한 영역 (옆) 왼쪽 방향
	FillRect(hDC, &Rect, Brush);
	Rect = { 590, 620, 610, 655 }; //오른쪽 방향
	FillRect(hDC, &Rect, Brush);
	Rect = { 80, 660, 100, 710 }; // 오른쪽 방향
	FillRect(hDC, &Rect, Brush);
	Rect = { 65, 445, 85, 655 }; // 오른쪽 방향
	FillRect(hDC, &Rect, Brush);
	DeleteObject(Brush);
}

void 농장으로이동영역그리기(HDC hDC)
{
	// 낚시터에서 농장으로 이동할 수 있는 영역을 그리는 함수
	// 여기서 그린 영역의 Rect 참고해서 플레이어 이동 업데이트 시 해당 영역과 겹치는지 검사해서 농장으로 이동 여부 설정해야 함.
	HBRUSH Brush = CreateSolidBrush(RGB(0, 255, 255));
	RECT Rect;
	Rect = { 0, 75, 20, 305 }; // 농장으로 이동하는 트리거 영역
	FillRect(hDC, &Rect, Brush);
	DeleteObject(Brush);
}
*/
LRESULT CALLBACK WndProc(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam)
{
	HDC hDC, hMemDC;
	PAINTSTRUCT ps;
	HBRUSH hBrush = (HBRUSH)GetStockObject(BLACK_BRUSH), oldBrush = (HBRUSH)GetStockObject(BLACK_BRUSH);
	HPEN hPen = (HPEN)GetStockObject(BLACK_PEN), oldPen = (HPEN)GetStockObject(BLACK_PEN);
	TCHAR str[100];

	static HBITMAP hBitmap;
	static RECT rc;

	// 농장 배경 (조성현 담당)
	static HBITMAP hBitmap_farm; // 농장 풍경 배경 이미지

	static bool canFishing; // 특정 영역에서 낚시 가능 여부
	static bool isFishing; // 낚시 중인지 여부
	static bool floatingGreenBar; // 초록 게이지가 위로 올라가는지. true면 올라가는 것.
	static HBITMAP hBitmap_fishing[4]; //0 게이지 바탕 //1 초록 게이지 //2 게이지 안에서 움직이는 물고기 //3 물었다! 표시
	static HBITMAP hBItmap_fishingGround; // 낚시 배경 이미지
	static struct GreenBar greenBar; // 초록 게이지 바 정보
	static struct TargetFish targetFish; // 게이지 안 속을 움직이는 물고기
	static struct FishingGage fishingGage; // 낚시 외부 게이지 정보

	switch (iMessage)
	{
	case WM_CREATE:
		// 농장 배경 로드
		hBitmap_farm = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\농장 배경.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);

		// 플레이어 이미지 로드 (아래 base/base_arm 로드에서 수행)

		//씬/플레이어 초기화
		g_currentScene = SCENE_FARM;
		g_player.x = 380; g_player.y = 380;
		g_player.w = PLAYER_DISPLAY_W;
		g_player.h = PLAYER_DISPLAY_H;
		g_player.speed = 4;
		g_player.dir = DIR_DOWN;
		g_player.frameIndex = 0;
		g_player.frameTimer = 0;
		g_player.isMoving = false;
		g_keyLeft = g_keyRight = g_keyUp = g_keyDown = false;
		g_sceneCooldown = 0;
		// [플레이어] 업데이트 타이머 (약 33fps) — 플레이어 이동/트리거 체크
		SetTimer(hWnd, 0001, 30, NULL);
		// 플레이어 이미지 로드
		g_player.base[0] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\사람\\farmer_base_정면1_16x28.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_player.base[1] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\사람\\farmer_base_정면2_16x28.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_player.base[2] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\사람\\farmer_base_정면3_16x28.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_player.base[3] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\사람\\farmer_base_옆면1_16x28.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_player.base[4] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\사람\\farmer_base_옆면2_16x28.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_player.base[5] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\사람\\farmer_base_옆면3_16x28.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_player.base[6] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\사람\\farmer_base_후면1_16x28.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_player.base[7] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\사람\\farmer_base_후면2_16x28.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_player.base[8] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\사람\\farmer_base_후면3_16x28.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_player.base_arm[0] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\사람\\farmer_arm_정면1_16x12.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_player.base_arm[1] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\사람\\farmer_arm_정면2_16x12.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_player.base_arm[2] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\사람\\farmer_arm_정면3_16x12.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_player.base_arm[3] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\사람\\farmer_arm_옆면1_16x12.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_player.base_arm[4] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\사람\\farmer_arm_옆면2_16x12.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_player.base_arm[5] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\사람\\farmer_arm_옆면3_16x12.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_player.base_arm[6] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\사람\\farmer_arm_후면1_16x12.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_player.base_arm[7] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\사람\\farmer_arm_후면2_16x12.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_player.base_arm[8] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\사람\\farmer_arm_후면3_16x12.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_player.fishing_arm[0] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\사람\\farmer_fishing_앞면1_16x17.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_player.fishing_arm[1] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\사람\\farmer_fishing_앞면2_16x17.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_player.fishing_arm[2] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\사람\\farmer_fishing_앞면3_16x17.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_player.fishing_arm[3] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\사람\\farmer_fishing_앞면4_16x17.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_player.fishing_arm[4] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\사람\\farmer_fishing_앞면5_16x17.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_player.fishing_arm[5] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\사람\\farmer_fishing_옆면1_16x14.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_player.fishing_arm[6] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\사람\\farmer_fishing_옆면2_16x14.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_player.fishing_arm[7] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\사람\\farmer_fishing_옆면3_16x14.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_player.fishing_arm[8] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\사람\\farmer_fishing_옆면4_16x14.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_player.fishing_arm[9] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\사람\\farmer_fishing_옆면5_16x14.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);

		hBitmap = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\낚시\\570x540-Beach_Overview.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);

		hBitmap_fishing[0] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\낚시\\fishing_37x149.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		hBitmap_fishing[1] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\낚시\\green_10x10.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION); // 3배정도 길게 늘려서 사용. 10x50 정도의 크기.
		hBitmap_fishing[2] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\낚시\\fish_39x20.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION); // 19x20 씩 잘라서 사용해야 함. 19x20, 1픽셀 띄우고 다시 19x20 이렇게.
		hBitmap_fishing[3] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\낚시\\물었다_74x28.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_hBitmap_biteNotice = hBitmap_fishing[3]; // DrawPlayer에서 접근용

		hBItmap_fishingGround = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\낚시\\570x540-Beach_Overview.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);

		canFishing = false; // 낚시 가능 영역에 있을 때만 true로 설정됨
		isFishing = false;
		floatingGreenBar = false;
		greenBar.width = 10;
		greenBar.height = 50;
		greenBar.x = 15;
		greenBar.y = 5; // 5 ~ 95 사이에서 움직이도록 설정해야 함
		targetFish.width = 19;
		targetFish.height = 20;
		targetFish.x = 10;
		targetFish.y = 125; // 5 ~ 125 사이에서 움직이도록 설정해야 함
		targetFish.inGreenBar = false;
		targetFish.moveDirY = -1;
		targetFish.moveTimer = 0;
		targetFish.moveInterval = 5 + rand() % 8;
		targetFish.movementType = FISH_MOVE_RANDOM;
		fishingGage.width = 32;
		fishingGage.height = 5;
		fishingGage.x = 4;
		fishingGage.y = 145; // 5 ~ 145 사이에서 움직이도록 설정해야 함
		fishingGage.current = 50;
		fishingGage.maxVal = 100;

		break;

	case WM_PAINT:
	{
		hDC = BeginPaint(hWnd, &ps);


		HDC backDC = CreateCompatibleDC(hDC);
		HBITMAP backBmp = CreateCompatibleBitmap(hDC, CLIENT_W, CLIENT_H);
		HBITMAP oldBackBmp = (HBITMAP)SelectObject(backDC, backBmp);

		// 낚시 비트맵 select용
		hMemDC = CreateCompatibleDC(backDC);

		switch (g_currentScene) {
		case SCENE_FARM:
		{
			// 농장 배경 그리기 -- 백버퍼에 축소 출력
			if (hBitmap_farm != NULL) {
				HDC hFarmDC = CreateCompatibleDC(backDC);
				HBITMAP hOldFarm = (HBITMAP)SelectObject(hFarmDC, hBitmap_farm);
				SetStretchBltMode(backDC, HALFTONE);
				StretchBlt(backDC, 0, 0, CLIENT_W, CLIENT_H,
					hFarmDC, 0, 0, 2288, 1856,
					SRCCOPY);
				SelectObject(hFarmDC, hOldFarm);
				DeleteDC(hFarmDC);
			}
			else {
				HBRUSH bg = CreateSolidBrush(RGB(150, 200, 130));
				RECT r = { 0, 0, CLIENT_W, CLIENT_H };
				FillRect(backDC, &r, bg);
				DeleteObject(bg);
			}

			// 낚시터 트리거 안내 
			DrawFishTriggerHint(backDC);

			// 플레이어 (스프라이트 or 도형)
			DrawPlayer(backDC);

			// 안내
			SetBkMode(backDC, TRANSPARENT);
			SetTextColor(backDC, RGB(255, 255, 255));
			const wchar_t* info = L"[농장] 방향키/WASD 이동, 오른쪽 위 길로 가면 낚시터";
			TextOut(backDC, 10, 10, info, (int)wcslen(info));
			break;
		}

		case SCENE_FISHING:
		{
			// 낚시 씬 배경 
			if (!hBItmap_fishingGround) {
				HBRUSH seaBrush = CreateSolidBrush(RGB(60, 110, 170));
				RECT seaRect = { 0, 0, CLIENT_W, CLIENT_H };
				FillRect(backDC, &seaRect, seaBrush);
				DeleteObject(seaBrush);
			}
			else {
				HDC hFishingDC = CreateCompatibleDC(backDC);
				HBITMAP hOldFarm = (HBITMAP)SelectObject(hFishingDC, hBItmap_fishingGround);
				SetStretchBltMode(backDC, HALFTONE);
				StretchBlt(backDC, 0, 0, CLIENT_W, CLIENT_H,
					hFishingDC, 0, 0, 570, 540,
					SRCCOPY);
				SelectObject(hFishingDC, hOldFarm);
				DeleteDC(hFishingDC);
			}

			// 낚시터플레이어이동못하는영역그리기(backDC);
			// 낚시가능영역그리기(backDC);
			// 농장으로이동영역그리기(backDC);

			// 플레이어 (스프라이트 or 도형)
			DrawPlayer(backDC);

			SetBkMode(backDC, TRANSPARENT);
			SetTextColor(backDC, RGB(255, 255, 255));
			const wchar_t* hint = L"[낚시터] 좌클릭으로 낚시 시작";
			TextOut(backDC, 10, 10, hint, (int)wcslen(hint));

			//낚시 그리기 코드 
			SelectObject(hMemDC, hBitmap);
			if (g_fishingPhase == FISHING_PHASE_GAME)
				FishingGameLogic(backDC, hBrush, oldBrush, hPen, oldPen, hBitmap_fishing, greenBar, targetFish, fishingGage);
			break;
		}
		}

		// 플레이어참고(backDC, g_player.base, g_player.fishing_arm);

		// 화면으로 한 번에 복사 
		BitBlt(hDC, 0, 0, CLIENT_W, CLIENT_H, backDC, 0, 0, SRCCOPY);

		// 정리
		DeleteDC(hMemDC);
		SelectObject(backDC, oldBackBmp);
		DeleteObject(backBmp);
		DeleteDC(backDC);
		EndPaint(hWnd, &ps);
		break;
	}

	// 배경 자동 지우기 차단 
	case WM_ERASEBKGND:
		return 1;

	case WM_LBUTTONDOWN:
	{
		if (g_fishingPhase == FISHING_PHASE_GAME) {
			floatingGreenBar = true; // 낚시 게임 중 마우스 누르면 초록 게이지 올라감
		}

		if (canFishing && g_fishingPhase == FISHING_PHASE_NONE) {
			// 낚시 시작: 캐스팅 단계로 진입
			g_fishingPhase = FISHING_PHASE_CAST;
			g_castFrameIdx = 0;
			g_castFrameTimer = 0;

			// 방향에 따라 fishing_arm 시작 인덱스 결정
			if (g_player.dir == DIR_DOWN) {
				g_fishingArmIdx = 0; // 앞면 첫 프레임
			}
			else {
				g_fishingArmIdx = 5; // 옆면 첫 프레임
			}

			// 물고기 움직임 유형 미리 결정
			targetFish.movementType = SelectFishMovementType();
			targetFish.moveTimer = 0;
			targetFish.moveInterval = 5 + rand() % 8;
			if (rand() % 2 == 0) {
				targetFish.moveDirY = 1;
			}
			else {
				targetFish.moveDirY = -1;
			}
			fishingGage.current = 50;

			SetTimer(hWnd, 2002, 100, NULL); // 캐스팅/기다림 전용 타이머 (100ms)
		}

		InvalidateRect(hWnd, NULL, FALSE);
		break;
	}

	case WM_LBUTTONUP:
	{
		if (g_fishingPhase == FISHING_PHASE_GAME) {
			floatingGreenBar = false; // 낚시 게임 중 마우스 떼면 초록 게이지 내려감
		}
		InvalidateRect(hWnd, NULL, FALSE);
		break;
	}

	case WM_MOUSEMOVE:
	{

		break;
	}

	case WM_TIMER:
	{
		switch (wParam) { // 타이머 규칙 : 0XXX : 플레이어 관련 타이머, 1XXX : 농사 관련 타이머, 2XXX : 낚시 관련 타이머, 3XXX : 디펜스 관련 타이머
		case 0001: // 플레이어 이동 및 트리거 체크 타이머
			if (g_currentScene == SCENE_FARM || g_currentScene == SCENE_FISHING) {
				UpdatePlayer();
				// 낚시터 씬일 때 낚시 가능 여부 갱신
				if (g_currentScene == SCENE_FISHING) {
					if (IsInFishingArea(g_player.x, g_player.y, g_player.w, g_player.h, g_player.dir)) {
						canFishing = true;
					}
					else {
						canFishing = false;
					}
				}
				InvalidateRect(hWnd, NULL, FALSE);
			}
			break;
		case 1001: // [농장] 플레이어 이동/트리거 체크

			break;

		case 2002: // 캐스팅 애니메이션 / 기다림 / 물었다! 타이머
		{
			if (g_fishingPhase == FISHING_PHASE_CAST) {
				// 캐스팅 애니메이션 진행 (0→4 앞면, 5→9 옆면)
				g_castFrameTimer++;
				if (g_castFrameTimer >= CAST_TICKS_PER_FRAME) {
					g_castFrameTimer = 0;
					g_castFrameIdx++;

					if (g_player.dir == DIR_DOWN) {
						// 앞면: 0→4 (5프레임)
						g_fishingArmIdx = g_castFrameIdx;
						if (g_castFrameIdx >= 4) {
							// 캐스팅 완료 → 기다림 단계
							g_fishingArmIdx = 4; // 마지막 프레임 고정
							g_fishingPhase = FISHING_PHASE_WAIT;
							g_fishWaitTimer = 0;
							g_fishWaitTarget = g_fishWaitMin + rand() % (g_fishWaitMax - g_fishWaitMin + 1);
						}
					}
					else {
						// 옆면: 5→9 (5프레임)
						g_fishingArmIdx = 5 + g_castFrameIdx;
						if (g_castFrameIdx >= 4) {
							g_fishingArmIdx = 9; // 마지막 프레임 고정
							g_fishingPhase = FISHING_PHASE_WAIT;
							g_fishWaitTimer = 0;
							g_fishWaitTarget = g_fishWaitMin + rand() % (g_fishWaitMax - g_fishWaitMin + 1);
						}
					}
				}
			}
			else if (g_fishingPhase == FISHING_PHASE_WAIT) {
				// 기다리는 중: 마지막 프레임 고정, 타이머 카운트
				g_fishWaitTimer++;
				if (g_fishWaitTimer >= g_fishWaitTarget) {
					// 물었다! 단계로 전환
					g_fishingPhase = FISHING_PHASE_BITE_NOTICE;
					g_biteNoticeTimer = 0;
				}
			}
			else if (g_fishingPhase == FISHING_PHASE_BITE_NOTICE) {
				// 물었다! 표시 약 1초 후 낚시 게임 시작
				g_biteNoticeTimer++;
				if (g_biteNoticeTimer >= BITE_NOTICE_TICKS) {
					g_fishingPhase = FISHING_PHASE_GAME;
					isFishing = true;
					g_fishGameArmTimer = 0;
					g_fishGameArmFrame = 0;
					// 낚시 게임 팔 초기 인덱스 (앞면=0, 옆면=5)
					if (g_player.dir == DIR_DOWN) {
						g_fishingArmIdx = 0;
					}
					else {
						g_fishingArmIdx = 5;
					}
					KillTimer(hWnd, 2002);
					SetTimer(hWnd, 2001, 100, NULL); // 낚시 게임 타이머 시작
				}
			}
			break;
		}

		case 2001: // 낚시 게임 타이머 (FISHING_PHASE_GAME 중에만 동작)
		{
			if (g_fishingPhase != FISHING_PHASE_GAME) {
				break;
			}

			// 낚시 게임 중 팔 애니메이션 (앞면: 0,1,0,1 / 옆면: 5,6,5,6 반복)
			g_fishGameArmTimer++;
			if (g_fishGameArmTimer >= FISH_GAME_ARM_TICKS) {
				g_fishGameArmTimer = 0;
				g_fishGameArmFrame = (g_fishGameArmFrame + 1) % 2;
				if (g_player.dir == DIR_DOWN) {
					g_fishingArmIdx = g_fishGameArmFrame; // 0 or 1
				}
				else {
					g_fishingArmIdx = 5 + g_fishGameArmFrame; // 5 or 6
				}
			}

			if (floatingGreenBar) {
				greenBar.y -= 5;
				if (greenBar.y < 5)
					greenBar.y = 5;
			}
			else {
				greenBar.y += 5;
				if (greenBar.y > 95)
					greenBar.y = 95;
			}

			UpdateFishMovement(targetFish);
			targetFish.inGreenBar = CheckFishInGreenBar(targetFish, greenBar);

			if (targetFish.inGreenBar) {
				fishingGage.current += 1;
				if (fishingGage.current > fishingGage.maxVal)
					fishingGage.current = fishingGage.maxVal;
			}
			else {
				fishingGage.current -= 2;
				if (fishingGage.current < 0)
					fishingGage.current = 0;
			}

			if (fishingGage.current >= fishingGage.maxVal) {
				KillTimer(hWnd, 2001);
				isFishing = false;
				g_fishingPhase = FISHING_PHASE_NONE;
				floatingGreenBar = false;
				// TODO: 아이템 드랍 처리 - targetFish.movementType 에 따라 다른 아이템 드랍
				MessageBox(hWnd, TEXT("낚시 성공!"), TEXT("낚시"), MB_OK);
			}

			if (fishingGage.current <= 0) {
				KillTimer(hWnd, 2001);
				isFishing = false;
				g_fishingPhase = FISHING_PHASE_NONE;
				floatingGreenBar = false;
				// TODO: 낚시 실패 처리
				MessageBox(hWnd, TEXT("낚시 실패..."), TEXT("낚시"), MB_OK);
			}

			break;
		}
		}
		InvalidateRect(hWnd, NULL, FALSE);
		break;
	}

	case WM_RBUTTONDOWN:
	{

		InvalidateRect(hWnd, NULL, FALSE);
		break;
	}

	case WM_KEYDOWN:
		// [농장] 방향키/WASD 입력 (씬과 무관하게 상태만 기록 - 농장 씬에서만 UpdatePlayer가 사용)
		switch (wParam) {
		case VK_LEFT:  case 'A': g_keyLeft = true;  break;
		case VK_RIGHT: case 'D': g_keyRight = true; break;
		case VK_UP:    case 'W': g_keyUp = true;    break;
		case VK_DOWN:  case 'S': g_keyDown = true;  break;
		}
		// 낚시 중 방향키 입력 시 낚시 취소 (모든 단계)
		if (g_fishingPhase != FISHING_PHASE_NONE) {
			if (wParam == VK_LEFT || wParam == 'A' ||
				wParam == VK_RIGHT || wParam == 'D' ||
				wParam == VK_UP || wParam == 'W' ||
				wParam == VK_DOWN || wParam == 'S') {
				KillTimer(hWnd, 2001);
				KillTimer(hWnd, 2002);
				isFishing = false;
				g_fishingPhase = FISHING_PHASE_NONE;
				floatingGreenBar = false;
			}
		}
		InvalidateRect(hWnd, NULL, FALSE);
		break;

	case WM_KEYUP:
		switch (wParam) {
		case VK_LEFT:  case 'A': g_keyLeft = false;  break;
		case VK_RIGHT: case 'D': g_keyRight = false; break;
		case VK_UP:    case 'W': g_keyUp = false;    break;
		case VK_DOWN:  case 'S': g_keyDown = false;  break;
		}
		break;

	case WM_DESTROY:
		KillTimer(hWnd, 1001);
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProc(hWnd, iMessage, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdParam, int nCmdShow)
{
	HWND hWnd;
	MSG Message;
	WNDCLASSEX WndClass;
	g_hInst = hInstance;

	srand(time(NULL));

	WndClass.cbSize = sizeof(WndClass);
	WndClass.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
	WndClass.lpfnWndProc = (WNDPROC)WndProc;
	WndClass.cbClsExtra = 0;
	WndClass.cbWndExtra = 0;
	WndClass.hInstance = hInstance;
	WndClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	WndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	WndClass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	WndClass.lpszMenuName = NULL;
	WndClass.lpszClassName = lpszClass;
	WndClass.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
	RegisterClassEx(&WndClass);

	hWnd = CreateWindow(lpszClass, lpszWindowName, WS_OVERLAPPEDWINDOW,
		0, 0, 800, 800, NULL, (HMENU)NULL, hInstance, NULL);

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	while (GetMessage(&Message, 0, 0, 0)) {
		TranslateMessage(&Message);
		DispatchMessage(&Message);
	}
	return Message.wParam;
}