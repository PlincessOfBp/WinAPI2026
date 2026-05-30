#include <Windows.h>
#include <tchar.h>
#include <stdlib.h>
#include <time.h>
#include <string>
#include <vector>
#include <math.h>
#include <wingdi.h>
#pragma comment(lib, "msimg32.lib")
//test.case1
HINSTANCE g_hInst;
LPCTSTR lpszClass = L"Window Class";
LPCTSTR lpszWindowName = L"windows program 1";

enum GameScene {
	SCENE_FARM = 0,    // 농장 화면 (조성현 담당)
	SCENE_FISHING = 1, // 낚시 화면 (문선우 담당)
	SCENE_SHOP = 2     // 상점 화면 (집 문으로 진입)
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
	TransparentBlt(hDC, 0, 0, 160, 280, hFishDC, 0, 0, 16, 28, RGB(255, 0, 255));

	//정면 팔
	// SelectObject(hFishDC, base_arm[4]);
	// TransparentBlt(hDC, 0, 4, 16, 17, hFishDC, 0, 0, 16, 17, RGB(255, 0, 255));

	// 옆면 팔
	SelectObject(hFishDC, base_arm[9]);
	TransparentBlt(hDC, - 160, - 120, 440, 330, hFishDC, 0, 0, 44, 33, RGB(255, 0, 255));

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

// ============================================================================
// [확장 시스템] 카메라 / 절벽충돌 / 애니집 / 나무벌목 / 농사 / 인벤토리(스와핑)
//   * 모든 렌더링은 비트맵 + TransparentBlt(투명색 RGB(255,0,255)) 사용.
//   * GDI 기본 도형(Rectangle/Ellipse 등)은 일절 사용하지 않음.
//   * 기존 씬/낚시/트리거/플레이어 로직은 손대지 않고 추가됨.
// ============================================================================

// ---------- 맵/타일 상수 ----------
#define TILE_SIZE         32
#define MAP_TILE_W        40
#define MAP_TILE_H        40
#define MAP_PIXEL_W       (TILE_SIZE * MAP_TILE_W)   // 1280
#define MAP_PIXEL_H       (TILE_SIZE * MAP_TILE_H)   // 1280

// 화면 안쪽 데드존: 플레이어가 이 안쪽에 있을 때까지는 카메라 정지,
// 데드존 경계에 닿으면 카메라가 따라감.
// (이 값을 0에 가깝게 하면 카메라가 즉시 플레이어 추적)
#define CAMERA_DEADZONE_MARGIN  120

// 카메라 clamp 한계 (맵 1280, 화면 800 → 0 ~ 480)
#define CAMERA_MAX_X      (MAP_PIXEL_W - CLIENT_W)   // 480
#define CAMERA_MAX_Y      (MAP_PIXEL_H - CLIENT_H)   // 480

// ---------- 카메라 ----------
static int cameraX = 0;
static int cameraY = 0;

// ---------- 전방 선언: 아래쪽에 정의된 함수 미리 알려주기 ----------
static bool RectOverlap(int ax, int ay, int aw, int ah,
	int bx, int by, int bw, int bh);

// ---------- 절벽(이동 불가) 충돌 영역: 좌측 상단 큰 사각형 ----------
// 사진 비율 기준: left=0, top=0, right=600, bottom=450
// (위치/크기 안 맞으면 이 4개 값만 조정)
#define CLIFF_RECT_COUNT 1
static RECT g_cliffRects[CLIFF_RECT_COUNT] = {
	{ 0, 0, 630, 500 }
};

// ---------- 친구 맵 포탈 트리거 (맵 우측 상단 흙길 끝) ----------
// 사용자 지정: worldX > 1200 && worldY < 250 부근
#define PORTAL_X_THRESHOLD  1200
#define PORTAL_Y_THRESHOLD   250
static bool g_portalTriggered = false;   // 한 번 알리고 다시 안 알리도록

// ---------- 동적 오브젝트: 나무 (3종) ----------
enum TreeKind {
	TREE_KIND_1 = 0,  // Tree1 118x140
	TREE_KIND_2 = 1,  // Tree2 56x72
	TREE_KIND_3 = 2   // Tree3 50x62
};
struct Tree {
	int x, y;       // 월드 좌표 (좌상단)
	int w, h;       // 표시 크기 (= 원본 크기)
	int kind;       // TreeKind
	int hp;         // 체력 (0이면 벌목됨)
	bool isAlive;
};
static std::vector<Tree> g_trees;
#define TREE_COUNT      15
#define TREE_HP_DEFAULT  3   // 도끼 3번 치면 베어짐

// ---------- 동적 오브젝트: 애니메이션 집 ----------
// house.bmp 768x96 = 96x96 × 8 프레임 (가로 스트립)
struct AnimatedHouse {
	int x, y;                // 월드 좌표 (좌상단)
	int currentFrame;        // 현재 프레임
	int frameTimer;          // 프레임 전환 카운터
	bool isDoorOpening;      // 문 열기 애니 진행 중
	bool doorAnimDone;       // 문 애니 끝남
	int shopTransitionTimer; // 문 애니 끝난 후 씬 전환까지 카운트다운 (30ms tick 기준)
};
static AnimatedHouse g_house = { 600, 350, 0, 0, false, false, 0 };

#define HOUSE_FRAME_W       96
#define HOUSE_FRAME_H       96
#define HOUSE_FRAME_COUNT    8       // 0~7
#define HOUSE_DRAW_W       192       // 화면에 2배 확대 출력
#define HOUSE_DRAW_H       192
#define HOUSE_ANIM_TICKS    10       // 프레임 전환 간격

// 집의 문 충돌 박스: 절대 좌표 기준 1타일(32x32), 집 바로 아래 (현관문 바로 앞)
// 캐릭터의 worldX/worldY가 이 RECT 안일 때만 문 열림 애니 시작.
#define HOUSE_DOOR_W       32
#define HOUSE_DOOR_H       32
#define HOUSE_DOOR_X       (g_house.x + (HOUSE_DRAW_W - HOUSE_DOOR_W) / 2)
#define HOUSE_DOOR_Y       (g_house.y + HOUSE_DRAW_H)   // 집 바로 아래

// ---------- 농사 2D 배열 ----------
// 0=기본 땅, 1=경작된 땅, 2=젖은 땅
static int farmState[MAP_TILE_H][MAP_TILE_W] = { 0, };

// 농사 타일 시트에서 잘라낼 좌표 (사용자 지정값)
#define FARM_TILE_TILLED_SRC_X    96
#define FARM_TILE_TILLED_SRC_Y  1152
#define FARM_TILE_WET_SRC_X      352
#define FARM_TILE_WET_SRC_Y      128

// ---------- 도구 ----------
enum ToolType {
	TOOL_NONE = 0,
	TOOL_HOE = 1,    // 호미
	TOOL_WATER = 2,  // 물뿌리개
	TOOL_POLE = 3,   // 낚싯대
	TOOL_AXE = 4     // 도끼 (벌목용 - 4x4에서 4번 슬롯에 장착)
};
static ToolType g_currentTool = TOOL_NONE;
static bool g_isInventoryOpen = false;

// ---------- 인벤토리 슬롯 데이터 ----------
// 1줄 퀵슬롯 4칸: [0]=HOE 고정 [1]=WATER 고정 [2]=POLE 고정 [3]=장착(빈칸 가능)
#define QUICKSLOT_COUNT  4
static ToolType g_quickSlot[QUICKSLOT_COUNT] = {
	TOOL_HOE, TOOL_WATER, TOOL_POLE, TOOL_NONE
};
static int g_selectedQuickSlot = -1;  // 현재 선택된 퀵슬롯 인덱스 (없으면 -1)

// 4x4 인벤토리 16칸 - 도끼와 일반 아이템 예시
#define INV4_COUNT 16
static ToolType g_inv4[INV4_COUNT] = {
	TOOL_AXE,  TOOL_NONE, TOOL_NONE, TOOL_NONE,
	TOOL_NONE, TOOL_NONE, TOOL_NONE, TOOL_NONE,
	TOOL_NONE, TOOL_NONE, TOOL_NONE, TOOL_NONE,
	TOOL_NONE, TOOL_NONE, TOOL_NONE, TOOL_NONE
};
static int g_selectedInv4 = -1;       // 4x4에서 선택된 아이템 슬롯 인덱스

// ---------- UI 화면 좌표 ----------
// 1줄 퀵슬롯: inventory_1.bmp 300x96, 화면 하단 중앙
#define QUICKSLOT_BG_W        300
#define QUICKSLOT_BG_H         96
#define QUICKSLOT_BG_X        (CLIENT_W / 2 - (QUICKSLOT_BG_W + 64) / 2)   // 가방 옆에 붙여 그리므로 합쳐 중앙
#define QUICKSLOT_BG_Y        (CLIENT_H - QUICKSLOT_BG_H - 20)
#define QUICKSLOT_ICON_SIZE    48
#define QUICKSLOT_PAD          12

// 가방 아이콘 (Inventory.bmp 64x68) - 1줄 인벤토리 바로 옆
#define BAG_W                 64
#define BAG_H                 68
#define BAG_X                 (QUICKSLOT_BG_X + QUICKSLOT_BG_W + 4)
#define BAG_Y                 (QUICKSLOT_BG_Y + (QUICKSLOT_BG_H - BAG_H) / 2)

// 4x4 인벤토리: inventory_4.bmp 300x300, 화면 정중앙
#define INV4_BG_W             300
#define INV4_BG_H             300
#define INV4_BG_X             (CLIENT_W / 2 - INV4_BG_W / 2)
#define INV4_BG_Y             (CLIENT_H / 2 - INV4_BG_H / 2)
#define INV4_CELL_SIZE        48    // 16칸 셀 크기 (4*48=192, 양쪽 여백 (300-192)/2=54)
#define INV4_CELL_OFFSET_X    54
#define INV4_CELL_OFFSET_Y    54

// ---------- 비트맵 핸들 (확장 리소스) ----------
static HBITMAP g_hBitmap_map        = NULL; // background/map.bmp 1280x1280
static HBITMAP g_hBitmap_house      = NULL; // 집/house.bmp 768x96 (96x96 × 8프레임)
static HBITMAP g_hBitmap_tree1      = NULL; // tree/Tree1_118x140.bmp
static HBITMAP g_hBitmap_tree2      = NULL; // tree/Tree2_56x72.bmp
static HBITMAP g_hBitmap_tree3      = NULL; // tree/Tree3_50x62.bmp
static HBITMAP g_hBitmap_cultivated = NULL; // farming/cultivated_land.bmp 32x32
static HBITMAP g_hBitmap_wet        = NULL; // farming/wet_ground.bmp 32x32
static HBITMAP g_hBitmap_icon_hoe   = NULL;
static HBITMAP g_hBitmap_icon_water = NULL;
static HBITMAP g_hBitmap_icon_pole  = NULL;
static HBITMAP g_hBitmap_inv_quick  = NULL; // inventory_1.bmp 300x96
static HBITMAP g_hBitmap_inv_main   = NULL; // inventory_4.bmp 300x300
static HBITMAP g_hBitmap_inv_bag    = NULL; // Inventory.bmp 64x68

// 공용 투명색
#define EXT_TRANSPARENT      RGB(255, 0, 255)

// ---------- 헬퍼: 두 점 거리(타일 단위) ----------
static int TileChebyshevDist(int ax, int ay, int bx, int by) {
	int dx = ax - bx; if (dx < 0) dx = -dx;
	int dy = ay - by; if (dy < 0) dy = -dy;
	return (dx > dy) ? dx : dy;
}

// ---------- 카메라 업데이트 ----------
// 플레이어 월드 좌표 기준, 화면 데드존을 벗어나면 카메라가 따라가도록.
void UpdateCamera() {
	// 플레이어가 화면상 어디에 있어야 하는지 (현재 카메라 기준)
	int playerScreenX = g_player.x - cameraX;
	int playerScreenY = g_player.y - cameraY;

	int leftBound  = CAMERA_DEADZONE_MARGIN;
	int rightBound = CLIENT_W - CAMERA_DEADZONE_MARGIN;
	int topBound   = CAMERA_DEADZONE_MARGIN;
	int botBound   = CLIENT_H - CAMERA_DEADZONE_MARGIN;

	if (playerScreenX < leftBound)  cameraX -= (leftBound  - playerScreenX);
	if (playerScreenX > rightBound) cameraX += (playerScreenX - rightBound);
	if (playerScreenY < topBound)   cameraY -= (topBound   - playerScreenY);
	if (playerScreenY > botBound)   cameraY += (playerScreenY - botBound);

	// Clamp 0 ~ 480
	if (cameraX < 0) cameraX = 0;
	if (cameraY < 0) cameraY = 0;
	if (cameraX > CAMERA_MAX_X) cameraX = CAMERA_MAX_X;
	if (cameraY > CAMERA_MAX_Y) cameraY = CAMERA_MAX_Y;
}

// ---------- 절벽(이동불가) 영역 충돌 검사 ----------
static bool IsBlockedByCliff(int px, int py, int pw, int ph) {
	for (int i = 0; i < CLIFF_RECT_COUNT; i++) {
		RECT& r = g_cliffRects[i];
		if (RectOverlap(px, py, pw, ph,
			r.left, r.top, r.right - r.left, r.bottom - r.top)) return true;
	}
	return false;
}

// 한 점이 절벽 영역 안에 있는가
static bool PointInCliff(int x, int y) {
	for (int i = 0; i < CLIFF_RECT_COUNT; i++) {
		RECT& r = g_cliffRects[i];
		if (x >= r.left && x < r.right && y >= r.top && y < r.bottom) return true;
	}
	return false;
}

// 한 점이 집 발판 위치 안에 있는가 (나무 스폰 시 회피용)
static bool PointInHouseFootprint(int x, int y) {
	return (x >= g_house.x - 10 && x < g_house.x + HOUSE_DRAW_W + 10 &&
		    y >= g_house.y - 10 && y < g_house.y + HOUSE_DRAW_H + 10);
}

// ---------- 나무 초기화: 절벽/집 피해서 15그루 ----------
void InitTrees() {
	g_trees.clear();
	int placed = 0;
	int tries = 0;
	while (placed < TREE_COUNT && tries < TREE_COUNT * 50) {
		tries++;
		Tree t;
		t.kind = rand() % 3;
		if (t.kind == TREE_KIND_1) { t.w = 118; t.h = 140; }
		else if (t.kind == TREE_KIND_2) { t.w = 56; t.h = 72; }
		else { t.w = 50; t.h = 62; }
		t.x = 80 + rand() % (MAP_PIXEL_W - t.w - 160);
		t.y = 80 + rand() % (MAP_PIXEL_H - t.h - 160);
		// 절벽/집 회피
		bool bad = false;
		// 발판(나무 밑동)이 절벽이면 거부
		int footX = t.x + t.w / 2;
		int footY = t.y + t.h - 6;
		if (PointInCliff(footX, footY)) bad = true;
		if (PointInHouseFootprint(footX, footY)) bad = true;
		// 다른 나무와 너무 겹쳐도 거부
		for (size_t k = 0; k < g_trees.size() && !bad; k++) {
			Tree& o = g_trees[k];
			if (RectOverlap(t.x, t.y, t.w, t.h, o.x, o.y, o.w, o.h)) bad = true;
		}
		if (bad) continue;
		t.hp = TREE_HP_DEFAULT;
		t.isAlive = true;
		g_trees.push_back(t);
		placed++;
	}
}

// ---------- 나무 그리기 (3종 비트맵, 카메라 오프셋) ----------
void DrawTrees(HDC hDC) {
	if (g_trees.empty()) return;
	HDC memDC = CreateCompatibleDC(hDC);

	for (size_t i = 0; i < g_trees.size(); i++) {
		Tree& t = g_trees[i];
		if (!t.isAlive) continue;
		int sx = t.x - cameraX;
		int sy = t.y - cameraY;
		if (sx + t.w < 0 || sx > CLIENT_W) continue;
		if (sy + t.h < 0 || sy > CLIENT_H) continue;

		HBITMAP src = NULL;
		if (t.kind == TREE_KIND_1)      src = g_hBitmap_tree1;
		else if (t.kind == TREE_KIND_2) src = g_hBitmap_tree2;
		else                            src = g_hBitmap_tree3;
		if (src == NULL) continue; // 이미지 없으면 건너뜀 (도형 fallback 사용 금지)

		HBITMAP oldB = (HBITMAP)SelectObject(memDC, src);
		TransparentBlt(hDC, sx, sy, t.w, t.h,
			memDC, 0, 0, t.w, t.h,
			EXT_TRANSPARENT);
		SelectObject(memDC, oldB);
	}
	DeleteDC(memDC);
}

// ---------- 집 애니메이션 업데이트 ----------
// 평상시: 프레임 0~3 (굴뚝 연기) 반복
// 문 열기: 프레임 4~7 진행 후 정지 + doorAnimDone=true
void UpdateHouseAnim() {
	g_house.frameTimer++;
	if (g_house.frameTimer < HOUSE_ANIM_TICKS) return;
	g_house.frameTimer = 0;

	if (g_house.isDoorOpening) {
		if (g_house.currentFrame < 4) g_house.currentFrame = 4; // 문 애니 시작 프레임
		else g_house.currentFrame++;
		if (g_house.currentFrame >= HOUSE_FRAME_COUNT) {
			g_house.currentFrame = HOUSE_FRAME_COUNT - 1;
			g_house.doorAnimDone = true;
		}
	}
	else {
		// 0~3 굴뚝 연기 루프
		g_house.currentFrame = (g_house.currentFrame + 1) % 4;
	}
}

// ---------- 집 그리기 (house.bmp 768x96 = 96x96 × 8프레임, 2배 확대) ----------
void DrawHouseAnimated(HDC hDC) {
	if (g_hBitmap_house == NULL) return; // 이미지 없으면 그리지 않음
	int sx = g_house.x - cameraX;
	int sy = g_house.y - cameraY;

	HDC memDC = CreateCompatibleDC(hDC);
	HBITMAP oldB = (HBITMAP)SelectObject(memDC, g_hBitmap_house);
	int srcX = g_house.currentFrame * HOUSE_FRAME_W;
	int srcY = 0;
	TransparentBlt(hDC,
		sx, sy, HOUSE_DRAW_W, HOUSE_DRAW_H,
		memDC,
		srcX, srcY, HOUSE_FRAME_W, HOUSE_FRAME_H,
		EXT_TRANSPARENT);
	SelectObject(memDC, oldB);
	DeleteDC(memDC);
}

// ---------- 농사 타일 그리기 (개별 비트맵 cultivated_land / wet_ground) ----------
void DrawFarmTiles(HDC hDC) {
	if (g_hBitmap_cultivated == NULL && g_hBitmap_wet == NULL) return;
	HDC memDC = CreateCompatibleDC(hDC);
	for (int r = 0; r < MAP_TILE_H; r++) {
		for (int c = 0; c < MAP_TILE_W; c++) {
			int s = farmState[r][c];
			if (s == 0) continue;
			int sx = c * TILE_SIZE - cameraX;
			int sy = r * TILE_SIZE - cameraY;
			if (sx + TILE_SIZE < 0 || sx > CLIENT_W) continue;
			if (sy + TILE_SIZE < 0 || sy > CLIENT_H) continue;

			HBITMAP src = (s == 1) ? g_hBitmap_cultivated : g_hBitmap_wet;
			if (src == NULL) continue;
			HBITMAP oldB = (HBITMAP)SelectObject(memDC, src);
			TransparentBlt(hDC, sx, sy, TILE_SIZE, TILE_SIZE,
				memDC, 0, 0, TILE_SIZE, TILE_SIZE,
				EXT_TRANSPARENT);
			SelectObject(memDC, oldB);
		}
	}
	DeleteDC(memDC);
}

// ---------- 도구 → 아이콘 비트맵 헬퍼 ----------
static HBITMAP GetToolIconBitmap(ToolType t) {
	switch (t) {
	case TOOL_HOE:   return g_hBitmap_icon_hoe;
	case TOOL_WATER: return g_hBitmap_icon_water;
	case TOOL_POLE:  return g_hBitmap_icon_pole;
	case TOOL_AXE:   return g_hBitmap_icon_pole; // (도끼 별도 아이콘 없으니 임시로 pole)
	default: return NULL;
	}
}

// 퀵슬롯 i번 슬롯의 화면 사각형 계산
static void GetQuickSlotRect(int i, int* outX, int* outY) {
	int slotPitch = QUICKSLOT_ICON_SIZE + QUICKSLOT_PAD;
	int totalSlotsW = QUICKSLOT_COUNT * QUICKSLOT_ICON_SIZE + (QUICKSLOT_COUNT - 1) * QUICKSLOT_PAD;
	int startX = QUICKSLOT_BG_X + (QUICKSLOT_BG_W - totalSlotsW) / 2;
	int iconY = QUICKSLOT_BG_Y + (QUICKSLOT_BG_H - QUICKSLOT_ICON_SIZE) / 2;
	if (outX) *outX = startX + i * slotPitch;
	if (outY) *outY = iconY;
}

// 4x4 i번 셀(0..15)의 화면 사각형 계산
static void GetInv4CellRect(int i, int* outX, int* outY) {
	int row = i / 4;
	int col = i % 4;
	if (outX) *outX = INV4_BG_X + INV4_CELL_OFFSET_X + col * INV4_CELL_SIZE;
	if (outY) *outY = INV4_BG_Y + INV4_CELL_OFFSET_Y + row * INV4_CELL_SIZE;
}

// ---------- 1줄 퀵슬롯 그리기 (비트맵만) ----------
void DrawQuickSlot(HDC hDC) {
	HDC memDC = CreateCompatibleDC(hDC);

	// 배경 패널 (inventory_1.bmp)
	if (g_hBitmap_inv_quick != NULL) {
		HBITMAP oldB = (HBITMAP)SelectObject(memDC, g_hBitmap_inv_quick);
		TransparentBlt(hDC,
			QUICKSLOT_BG_X, QUICKSLOT_BG_Y, QUICKSLOT_BG_W, QUICKSLOT_BG_H,
			memDC, 0, 0, QUICKSLOT_BG_W, QUICKSLOT_BG_H,
			EXT_TRANSPARENT);
		SelectObject(memDC, oldB);
	}

	// 슬롯 4칸 아이콘
	for (int i = 0; i < QUICKSLOT_COUNT; i++) {
		int ix, iy;
		GetQuickSlotRect(i, &ix, &iy);
		HBITMAP src = GetToolIconBitmap(g_quickSlot[i]);
		if (src != NULL) {
			HBITMAP oldB = (HBITMAP)SelectObject(memDC, src);
			TransparentBlt(hDC, ix, iy, QUICKSLOT_ICON_SIZE, QUICKSLOT_ICON_SIZE,
				memDC, 0, 0, QUICKSLOT_ICON_SIZE, QUICKSLOT_ICON_SIZE,
				EXT_TRANSPARENT);
			SelectObject(memDC, oldB);
		}
		// 선택된 슬롯은 동일 아이콘을 한 번 더 살짝 옆으로 겹쳐 그려 '하이라이트' 표현
		// (도형 사용 금지 규칙 준수: 빨간 사각형 테두리 못 그리니, 선택 표시는 아이콘 자체로)
		if (g_selectedQuickSlot == i && src != NULL) {
			HBITMAP oldB = (HBITMAP)SelectObject(memDC, src);
			// 위에 한 번 더 살짝 위로
			TransparentBlt(hDC, ix, iy - 4, QUICKSLOT_ICON_SIZE, QUICKSLOT_ICON_SIZE,
				memDC, 0, 0, QUICKSLOT_ICON_SIZE, QUICKSLOT_ICON_SIZE,
				EXT_TRANSPARENT);
			SelectObject(memDC, oldB);
		}
	}

	// 가방 아이콘 (1줄 옆에 붙여서)
	if (g_hBitmap_inv_bag != NULL) {
		HBITMAP oldB = (HBITMAP)SelectObject(memDC, g_hBitmap_inv_bag);
		TransparentBlt(hDC, BAG_X, BAG_Y, BAG_W, BAG_H,
			memDC, 0, 0, BAG_W, BAG_H,
			EXT_TRANSPARENT);
		SelectObject(memDC, oldB);
	}

	DeleteDC(memDC);
}

// ---------- 4x4 인벤토리 (비트맵만, 열렸을 때만) ----------
void DrawInventoryPanel(HDC hDC) {
	if (!g_isInventoryOpen) return;
	HDC memDC = CreateCompatibleDC(hDC);

	// 배경 패널 (inventory_4.bmp 300x300)
	if (g_hBitmap_inv_main != NULL) {
		HBITMAP oldB = (HBITMAP)SelectObject(memDC, g_hBitmap_inv_main);
		TransparentBlt(hDC, INV4_BG_X, INV4_BG_Y, INV4_BG_W, INV4_BG_H,
			memDC, 0, 0, INV4_BG_W, INV4_BG_H,
			EXT_TRANSPARENT);
		SelectObject(memDC, oldB);
	}

	// 16칸 아이템
	for (int i = 0; i < INV4_COUNT; i++) {
		int ix, iy;
		GetInv4CellRect(i, &ix, &iy);
		HBITMAP src = GetToolIconBitmap(g_inv4[i]);
		if (src != NULL) {
			HBITMAP oldB = (HBITMAP)SelectObject(memDC, src);
			TransparentBlt(hDC, ix, iy, QUICKSLOT_ICON_SIZE, QUICKSLOT_ICON_SIZE,
				memDC, 0, 0, QUICKSLOT_ICON_SIZE, QUICKSLOT_ICON_SIZE,
				EXT_TRANSPARENT);
			SelectObject(memDC, oldB);
		}
		// 선택된 셀: 아이콘을 위로 살짝 띄워 하이라이트
		if (g_selectedInv4 == i && src != NULL) {
			HBITMAP oldB = (HBITMAP)SelectObject(memDC, src);
			TransparentBlt(hDC, ix, iy - 4, QUICKSLOT_ICON_SIZE, QUICKSLOT_ICON_SIZE,
				memDC, 0, 0, QUICKSLOT_ICON_SIZE, QUICKSLOT_ICON_SIZE,
				EXT_TRANSPARENT);
			SelectObject(memDC, oldB);
		}
	}

	DeleteDC(memDC);
}

// ---------- 가방 아이콘 클릭 판정 ----------
static bool IsClickInBag(int mx, int my) {
	return (mx >= BAG_X && mx < BAG_X + BAG_W &&
		    my >= BAG_Y && my < BAG_Y + BAG_H);
}

// 퀵슬롯 어느 슬롯 클릭? (-1이면 슬롯 아님)
static int HitQuickSlot(int mx, int my) {
	for (int i = 0; i < QUICKSLOT_COUNT; i++) {
		int ix, iy;
		GetQuickSlotRect(i, &ix, &iy);
		if (mx >= ix && mx < ix + QUICKSLOT_ICON_SIZE &&
			my >= iy && my < iy + QUICKSLOT_ICON_SIZE) return i;
	}
	return -1;
}

// 4x4 어느 셀 클릭? (-1이면 셀 아님)
static int HitInv4Cell(int mx, int my) {
	if (!g_isInventoryOpen) return -1;
	for (int i = 0; i < INV4_COUNT; i++) {
		int ix, iy;
		GetInv4CellRect(i, &ix, &iy);
		if (mx >= ix && mx < ix + QUICKSLOT_ICON_SIZE &&
			my >= iy && my < iy + QUICKSLOT_ICON_SIZE) return i;
	}
	return -1;
}

// ---------- 클릭이 UI 영역(퀵슬롯/가방/열린 인벤토리) 위에 있는가 ----------
static bool IsClickOnUI(int mx, int my) {
	if (mx >= QUICKSLOT_BG_X && mx < QUICKSLOT_BG_X + QUICKSLOT_BG_W &&
		my >= QUICKSLOT_BG_Y && my < QUICKSLOT_BG_Y + QUICKSLOT_BG_H) return true;
	if (IsClickInBag(mx, my)) return true;
	if (g_isInventoryOpen &&
		mx >= INV4_BG_X && mx < INV4_BG_X + INV4_BG_W &&
		my >= INV4_BG_Y && my < INV4_BG_Y + INV4_BG_H) return true;
	return false;
}

// 퀵슬롯 i번 도구를 현재 도구로 선택
static void SelectQuickSlot(int i) {
	if (i < 0 || i >= QUICKSLOT_COUNT) return;
	g_selectedQuickSlot = i;
	g_currentTool = g_quickSlot[i]; // TOOL_NONE 일 수도 (빈 슬롯)
}

// 4x4 -> 퀵슬롯 4번에 장착 (스와핑)
static void EquipInv4ToQuickslot(int inv4Idx, int quickIdx) {
	if (inv4Idx < 0 || inv4Idx >= INV4_COUNT) return;
	if (quickIdx < 0 || quickIdx >= QUICKSLOT_COUNT) return;
	ToolType a = g_inv4[inv4Idx];
	ToolType b = g_quickSlot[quickIdx];
	g_quickSlot[quickIdx] = a;
	g_inv4[inv4Idx] = b;
	g_selectedInv4 = -1;
}

// ---------- 마우스 → 농사 상호작용 ----------
void HandleFarmClick(int mx, int my) {
	int worldX = mx + cameraX;
	int worldY = my + cameraY;
	int gridX = worldX / TILE_SIZE;
	int gridY = worldY / TILE_SIZE;
	if (gridX < 0 || gridX >= MAP_TILE_W) return;
	if (gridY < 0 || gridY >= MAP_TILE_H) return;

	// 플레이어 주변 거리(1~2칸)
	int playerGX = (g_player.x + g_player.w / 2) / TILE_SIZE;
	int playerGY = (g_player.y + g_player.h / 2) / TILE_SIZE;
	int dist = TileChebyshevDist(gridX, gridY, playerGX, playerGY);
	if (dist > 2) return;

	// 도구별 상태 변경
	if (g_currentTool == TOOL_HOE && farmState[gridY][gridX] == 0) {
		farmState[gridY][gridX] = 1; // 경작
	}
	else if (g_currentTool == TOOL_WATER && farmState[gridY][gridX] == 1) {
		farmState[gridY][gridX] = 2; // 물주기
	}
}

// ---------- 도끼로 나무 벌목 시도 ----------
// 클릭한 위치(월드 좌표)에 있는 나무를 찾아 HP 깎고, 0이면 벌목.
// 플레이어 주변 일정 거리 안의 나무만 가능.
static void TryChopTree(int mx, int my) {
	if (g_currentTool != TOOL_AXE) return;
	int worldX = mx + cameraX;
	int worldY = my + cameraY;

	int playerCX = g_player.x + g_player.w / 2;
	int playerCY = g_player.y + g_player.h / 2;

	for (size_t i = 0; i < g_trees.size(); i++) {
		Tree& t = g_trees[i];
		if (!t.isAlive) continue;
		// 클릭이 나무 영역 안인지
		if (worldX < t.x || worldX >= t.x + t.w) continue;
		if (worldY < t.y || worldY >= t.y + t.h) continue;
		// 플레이어와 나무 중심 거리 (픽셀) 제한
		int treeCX = t.x + t.w / 2;
		int treeCY = t.y + t.h - 10;
		int dx = playerCX - treeCX;
		int dy = playerCY - treeCY;
		int sqDist = dx * dx + dy * dy;
		if (sqDist > 120 * 120) return; // 너무 멀면 무시
		// HP 감소
		t.hp--;
		if (t.hp <= 0) t.isAlive = false;
		return; // 한 번 클릭에 한 그루만
	}
}

// 낚시터 타일맵 
// 타일 1개 = 16x16px, 맵 40x40타일 = 640x640px
// 화면 800x800으로 StretchBlt → 화면 기준 타일 1개 = 20x20px
// 0: 이동 가능
// 1: 이동 불가
// 2: 낚시 가능 (아래, DIR_DOWN)
// 3: 낚시 가능 (왼쪽, DIR_LEFT)
// 4: 낚시 가능 (오른쪽, DIR_RIGHT)
// 5: 낚시 가능 (왼쪽+아래, DIR_LEFT & DIR_DOWN)
// 6: 낚시 가능 (오른쪽+아래, DIR_RIGHT & DIR_DOWN)
// 7: 농장으로 이동 트리거
// 8 - Tree1 9 - Tree2 10 - Tree3 나무들의 밑둥. 이동 불가능. 두 칸이 하나의 나무 밑둥.
#define FISHING_MAP_COLS 40
#define FISHING_MAP_ROWS 40
#define FISHING_TILE_SCREEN 20  // 화면 기준 타일 크기 (px)

static int g_fishingTileMap[FISHING_MAP_ROWS][FISHING_MAP_COLS] = {
	{ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 },
	{ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
	{ 10,10,0,0,9,9,0,0,0,0,0,0,9,9,0,0,0,10,10,0,0,0,0,0,10,10,0,0,0,0,0,0,0,10,10,0,0,0,0,0 },
	{ 0,0,0,0,0,0,0,10,10,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,9,9,0 },
	{ 7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
	{ 7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
	{ 7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
	{ 0,10,10,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,9,9,0,0 },
	{ 0,0,0,0,0,0,0,0,0,0,0,10,10,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
	{ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,8,8,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
	{ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
	{ 0,0,0,0,0,0,0,0,0,9,9,0,0,0,0,0,0,0,0,0,0,0,0,0,9,9,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
	{ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1 },
	{ 0,0,0,8,8,0,0,0,0,0,0,0,0,0,0,0,9,9,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1 },
	{ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1 },
	{ 9,9,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1 },
	{ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1 },
	{ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1 },
	{ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 },
	{ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 },
	{ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 },
	{ 1,1,1,1,1,1,1,1,1,0,0,0,1,1,1,1,1,1,1,1,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 },
	{ 1,1,1,1,1,1,1,1,1,0,0,0,1,1,1,1,1,1,1,1,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 },
	{ 1,1,1,1,1,1,1,1,1,0,0,0,1,1,1,1,1,1,1,1,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 },
	{ 1,1,1,1,1,1,1,1,1,0,0,0,2,2,2,2,2,2,2,2,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 },
	{ 1,1,1,1,1,1,1,1,1,3,0,4,1,1,1,1,1,1,1,1,3,0,4,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 },
	{ 1,1,1,1,1,1,1,1,1,3,0,4,1,1,1,1,1,1,1,1,3,0,4,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 },
	{ 1,1,1,1,1,1,1,1,1,3,0,4,1,1,1,1,1,1,1,1,3,0,4,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 },
	{ 1,1,1,1,1,1,1,1,1,3,0,4,1,1,1,1,1,1,1,1,3,0,4,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 },
	{ 1,1,1,1,1,1,1,1,1,3,0,4,1,1,1,1,1,1,1,1,3,0,4,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 },
	{ 1,1,1,1,1,1,1,1,1,5,2,6,1,1,1,1,1,1,1,1,3,0,0,0,0,0,0,0,0,0,0,4,1,1,1,1,1,1,1,1 },
	{ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,3,0,0,0,0,0,0,0,0,0,0,4,1,1,1,1,1,1,1,1 },
	{ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,5,2,2,2,2,2,2,2,2,2,2,6,1,1,1,1,1,1,1,1 },
	{ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 },
	{ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 },
	{ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 },
	{ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 },
	{ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 },
	{ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 },
	{ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 },
};

// 두 사각형 겹침 검사 (농장 씬 트리거에서 사용)
static bool RectOverlap(int ax, int ay, int aw, int ah,
	int bx, int by, int bw, int bh) {
	return (ax + aw > bx) && (ax < bx + bw) &&
		(ay + ah > by) && (ay < by + bh);
}

// 화면 좌표 → 타일 인덱스 변환
static int ScreenToTileCol(int screenX) { return screenX / FISHING_TILE_SCREEN; }
static int ScreenToTileRow(int screenY) { return screenY / FISHING_TILE_SCREEN; }

// 발 영역이 닿는 타일들 중 특정 값이 있는지 검사
// footX, footY, footW, footH: 화면 픽셀 기준 발 영역
static bool FishingTileHasValue(int footX, int footY, int footW, int footH, int tileVal) {
	int colL = ScreenToTileCol(footX);
	int colR = ScreenToTileCol(footX + footW - 1);
	int rowT = ScreenToTileRow(footY);
	int rowB = ScreenToTileRow(footY + footH - 1);

	// 범위 클램프
	if (colL < 0) colL = 0;
	if (rowT < 0) rowT = 0;
	if (colR >= FISHING_MAP_COLS) colR = FISHING_MAP_COLS - 1;
	if (rowB >= FISHING_MAP_ROWS) rowB = FISHING_MAP_ROWS - 1;

	int r, c;
	for (r = rowT; r <= rowB; r++) {
		for (c = colL; c <= colR; c++) {
			if (g_fishingTileMap[r][c] == tileVal)
				return true;
		}
	}
	return false;
}

// 이동 불가(1) 및 나무 밑둥(8,9,10) 타일과 충돌하는지 검사
static bool IsBlockedInFishing(int footX, int footY, int footW, int footH) {
	int colL = ScreenToTileCol(footX);
	int colR = ScreenToTileCol(footX + footW - 1);
	int rowT = ScreenToTileRow(footY);
	int rowB = ScreenToTileRow(footY + footH - 1);

	if (colL < 0) colL = 0;
	if (rowT < 0) rowT = 0;
	if (colR >= FISHING_MAP_COLS) colR = FISHING_MAP_COLS - 1;
	if (rowB >= FISHING_MAP_ROWS) rowB = FISHING_MAP_ROWS - 1;

	int r, c, tile;
	for (r = rowT; r <= rowB; r++) {
		for (c = colL; c <= colR; c++) {
			tile = g_fishingTileMap[r][c];
			if (tile == 1)  return true;
			if (tile == 8)  return true;
			if (tile == 9)  return true;
			if (tile == 10) return true;
		}
	}
	return false;
}

// 낚시 가능 여부 + 방향 검사
static bool IsInFishingArea(int footX, int footY, int footW, int footH, PlayerDir dir) {
	int colL = ScreenToTileCol(footX);
	int colR = ScreenToTileCol(footX + footW - 1);
	int rowT = ScreenToTileRow(footY);
	int rowB = ScreenToTileRow(footY + footH - 1);

	if (colL < 0) colL = 0;
	if (rowT < 0) rowT = 0;
	if (colR >= FISHING_MAP_COLS) colR = FISHING_MAP_COLS - 1;
	if (rowB >= FISHING_MAP_ROWS) rowB = FISHING_MAP_ROWS - 1;

	int r, c, tile;
	for (r = rowT; r <= rowB; r++) {
		for (c = colL; c <= colR; c++) {
			tile = g_fishingTileMap[r][c];
			// 2: 아래, 3: 왼, 4: 오른, 5: 왼&아래, 6: 오른&아래
			if (tile == 2 && dir == DIR_DOWN)  return true;
			if (tile == 3 && dir == DIR_LEFT)  return true;
			if (tile == 4 && dir == DIR_RIGHT) return true;
			if (tile == 5 && dir == DIR_LEFT)  return true;
			if (tile == 5 && dir == DIR_DOWN)  return true;
			if (tile == 6 && dir == DIR_RIGHT) return true;
			if (tile == 6 && dir == DIR_DOWN)  return true;
		}
	}
	return false;
}

// 농장 이동 트리거(7) 타일에 닿았는지 검사
static bool IsOnFarmTrigger(int footX, int footY, int footW, int footH) {
	return FishingTileHasValue(footX, footY, footW, footH, 7);
}

// 나무 출력 크기 (원본 1280px 기준 → 화면 800px 비율: *800/1280 = *5/8)
// Tree1: 118x140 → 74x88
// Tree2:  56x72  → 35x45
// Tree3:  50x62  → 32x39
struct TreeInfo {
	int tileVal;  // 타일맵 값 (8, 9, 10)
	int bmpIdx;   // hBitmap_fishTree 인덱스 (0, 1, 2)
	int srcW, srcH; // 원본 픽셀 크기
	int dstW, dstH; // 화면 출력 크기
};
static TreeInfo g_treeInfos[3] = {
	{ 8,  0, 118, 140,  74, 88 },
	{ 9,  1,  56,  72,  35, 45 },
	{ 10, 2,  50,  62,  32, 39 },
};

// drawBelow == true  : 플레이어 발보다 아래에 뿌리가 있는 나무 (플레이어 앞 → 나무가 플레이어를 가림)
// drawBelow == false : 플레이어 발보다 위에 뿌리가 있는 나무  (플레이어 뒤 → 플레이어가 나무를 가림)
static void DrawTrees(HDC hDC, HBITMAP hBitmap_fishTree[3], int playerFootY, bool drawBelow) {
	HDC memDC = CreateCompatibleDC(hDC);
	int r, c, t;
	for (r = 0; r < FISHING_MAP_ROWS; r++) {
		// 나무 뿌리의 화면 Y = 타일 행 아랫변
		int treeFootY = (r + 1) * FISHING_TILE_SCREEN;

		if (drawBelow) {
			if (treeFootY <= playerFootY) continue; // 플레이어 뒤 나무는 건너뜀
		}
		else {
			if (treeFootY > playerFootY) continue;  // 플레이어 앞 나무는 건너뜀
		}

		for (c = 0; c < FISHING_MAP_COLS - 1; c++) {
			int tile = g_fishingTileMap[r][c];
			if (tile < 8 || tile > 10) continue;
			// 오른쪽 칸도 같은 값이어야 나무 한 그루
			if (g_fishingTileMap[r][c + 1] != tile) continue;

			// 해당 타일값의 TreeInfo 찾기
			for (t = 0; t < 3; t++) {
				if (g_treeInfos[t].tileVal != tile) continue;

				if (hBitmap_fishTree[g_treeInfos[t].bmpIdx] == NULL) break;

				SelectObject(memDC, hBitmap_fishTree[g_treeInfos[t].bmpIdx]);

				// 나무 밑둥 영역 너비 = 타일 2칸 = FISHING_TILE_SCREEN * 2
				// 중앙 정렬: 영역 왼쪽 끝 + (영역 너비 - 나무 출력 너비) / 2
				int treeAreaW = FISHING_TILE_SCREEN * 2;
				int treeX = c * FISHING_TILE_SCREEN + (treeAreaW - g_treeInfos[t].dstW) / 2;
				int treeY = treeFootY - g_treeInfos[t].dstH;

				TransparentBlt(hDC, treeX, treeY, g_treeInfos[t].dstW, g_treeInfos[t].dstH,
					memDC, 0, 0, g_treeInfos[t].srcW, g_treeInfos[t].srcH, RGB(255, 0, 255));
				break;
			}
			c++; // 오른쪽 칸은 같은 나무이므로 건너뜀
		}
	}
	DeleteDC(memDC);
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
		// [확장] 농장 씬: 절벽(좌상단 ㄷ자) 영역 충돌 처리
		if (g_currentScene == SCENE_FARM) {
			int footOffX = g_player.w / 4;
			int footOffY = g_player.h * 3 / 4;
			int footW = g_player.w / 2;
			int footH = g_player.h / 4;

			// 안전장치: 이미 절벽 안에 끼어있으면 충돌 무시 (탈출 허용)
			bool alreadyInside = IsBlockedByCliff(
				g_player.x + footOffX, g_player.y + footOffY, footW, footH);

			int nextX = g_player.x + dx;
			if (!alreadyInside &&
				IsBlockedByCliff(nextX + footOffX, g_player.y + footOffY, footW, footH)) {
				nextX = g_player.x;
				dx = 0;
			}
			int nextY = g_player.y + dy;
			if (!alreadyInside &&
				IsBlockedByCliff(nextX + footOffX, nextY + footOffY, footW, footH)) {
				nextY = g_player.y;
				dy = 0;
			}
			g_player.x = nextX;
			g_player.y = nextY;
		}
		else {
			g_player.x += dx;
			g_player.y += dy;
		}
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

	// 맵 경계 클램프 (씬에 따라 다른 맵 사이즈 사용)
	// - SCENE_FARM: 1280x1280 맵
	// - 그 외 (낚시 등): 800x800 화면
	{
		int maxX = (g_currentScene == SCENE_FARM) ? MAP_PIXEL_W : CLIENT_W;
		int maxY = (g_currentScene == SCENE_FARM) ? MAP_PIXEL_H : CLIENT_H;
		if (g_player.x < 0) g_player.x = 0;
		if (g_player.y < 0) g_player.y = 0;
		if (g_player.x + g_player.w > maxX) g_player.x = maxX - g_player.w;
		if (g_player.y + g_player.h > maxY) g_player.y = maxY - g_player.h;
	}

	// 씬 전환 쿨다운
	if (g_sceneCooldown > 0) {
		g_sceneCooldown--;
		return;
	}

	// 낚시터 가는 길에 닿으면 SCENE_FISHING으로 전환
	if (g_currentScene == SCENE_FARM && RectOverlap(g_player.x, g_player.y, g_player.w, g_player.h,
		TRIGGER_TO_FISH_X, TRIGGER_TO_FISH_Y, TRIGGER_TO_FISH_W, TRIGGER_TO_FISH_H)) {
		g_currentScene = SCENE_FISHING;
		// 낚시터 출입구(타일 7, 행4~6 열0) 바로 오른쪽에 스폰
		g_player.x = 1 * FISHING_TILE_SCREEN;
		g_player.y = 5 * FISHING_TILE_SCREEN - g_player.h / 2;
		g_player.dir = DIR_RIGHT;
		g_sceneCooldown = 30;
	}

	// [확장] 친구 맵 포탈 트리거: 우측 상단 흙길 끝 (worldX>1200 && worldY<250)
	if (g_currentScene == SCENE_FARM) {
		int footCX = g_player.x + g_player.w / 2;
		int footCY = g_player.y + g_player.h / 2;
		if (footCX > PORTAL_X_THRESHOLD && footCY < PORTAL_Y_THRESHOLD) {
			if (!g_portalTriggered) {
				g_portalTriggered = true;
				OutputDebugString(TEXT("[PORTAL] 친구 맵으로 이동!\n"));
				//MessageBox(NULL, TEXT("친구 맵으로 이동!"), TEXT("포탈"), MB_OK);
			}
		}
		else {
			// 포탈 영역에서 벗어나면 트리거 리셋 (재진입 가능)
			g_portalTriggered = false;
		}
	}

	// 농장 가는 길에 닿으면 SCENE_FARM으로 전환 (타일 7 기반)
	if (g_currentScene == SCENE_FISHING) {
		int footOffX = g_player.w / 4;
		int footOffY = g_player.h * 3 / 4;
		int footW = g_player.w / 2;
		int footH = g_player.h / 4;
		if (IsOnFarmTrigger(g_player.x + footOffX, g_player.y + footOffY, footW, footH)) {
			g_currentScene = SCENE_FARM;
			// 농장 출입구(TRIGGER_TO_FISH) 바로 왼쪽에 스폰
			g_player.x = TRIGGER_TO_FISH_X - g_player.w - 4;
			g_player.y = TRIGGER_TO_FISH_Y + TRIGGER_TO_FISH_H / 2 - g_player.h / 2;
			g_player.dir = DIR_LEFT;
			g_sceneCooldown = 30;
		}
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
	int dispFArmFrontW = 16*2; int dispFArmFrontH = 41*2; // 앞면 낚시팔 
	int dispFArmSideW = 44*2; int dispFArmSideH = 33*2; // 옆면 낚시팔 

	bool isFishingState = (g_fishingPhase != FISHING_PHASE_NONE);

	if (g_player.dir == DIR_LEFT) {
		// 몸 (좌우반전)
		DrawFlipped(hDC, memDC, g_player.base[baseIdx],
			x, y, dispBodyW, dispBodyH, 16, 28);

		if (isFishingState) {
			// 낚시 팔 (옆면, 좌우반전)
			if (g_player.fishing_arm[g_fishingArmIdx] != NULL) {
				DrawFlipped(hDC, memDC, g_player.fishing_arm[g_fishingArmIdx],
					x + 26, y - 24, dispFArmSideW, dispFArmSideH, 44, 33);
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
					TransparentBlt(hDC, x, y - 14, dispFArmFrontW, dispFArmFrontH,
						memDC, 0, 0, 16, 41, RGB(255, 0, 255));
				}
				else {
					TransparentBlt(hDC, x - 26, y - 24, dispFArmSideW, dispFArmSideH,
						memDC, 0, 0, 44, 33, RGB(255, 0, 255));
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
	static HBITMAP hBitmap_fishingGround; // 낚시 배경 이미지
	static HBITMAP hBitmap_fishTree[3]; // 0-Tree1 1-Tree2 2-Tree3
	static struct GreenBar greenBar; // 초록 게이지 바 정보
	static struct TargetFish targetFish; // 게이지 안 속을 움직이는 물고기
	static struct FishingGage fishingGage; // 낚시 외부 게이지 정보

	switch (iMessage)
	{
	case WM_CREATE:
		// 농장 배경 로드
		// (파일명: 농장배경.bmp - 공백 없음. 위치: Project1\이미지소스\농장\)
		hBitmap_farm = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\농장배경.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);

		// ----- [확장 시스템] 추가 리소스 로드 + 초기화 -----
		g_hBitmap_map        = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\background\\map.bmp"),         IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_hBitmap_house      = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\집\\house.bmp"),                     IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_hBitmap_tree1      = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\tree\\Tree1_118x140.bmp"),     IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_hBitmap_tree2      = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\tree\\Tree2_56x72.bmp"),       IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_hBitmap_tree3      = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\tree\\Tree3_50x62.bmp"),       IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_hBitmap_cultivated = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\farming\\cultivated_land.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_hBitmap_wet        = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\farming\\wet_ground.bmp"),     IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_hBitmap_icon_hoe   = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\icon\\icon_hoe.bmp"),           IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_hBitmap_icon_water = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\icon\\icon_water.bmp"),         IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_hBitmap_icon_pole  = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\icon\\fishing pole.bmp"),       IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_hBitmap_inv_quick  = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\inventory\\inventory_1.bmp"),   IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_hBitmap_inv_main   = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\inventory\\inventory_4.bmp"),   IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_hBitmap_inv_bag    = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\inventory\\Inventory.bmp"),     IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);

		InitTrees();
		g_house.x = 600; g_house.y = 350;   // 절대 좌표 고정 (흙길 끝 빨간 영역)
		g_house.currentFrame = 0; g_house.frameTimer = 0;
		g_house.isDoorOpening = false; g_house.doorAnimDone = false;
		cameraX = 0; cameraY = 0;
		g_currentTool = TOOL_NONE;
		g_isInventoryOpen = false;
		g_selectedQuickSlot = -1;
		g_selectedInv4 = -1;
		// 퀵슬롯 초기 구성 재확인 (1=HOE, 2=WATER, 3=POLE, 4=빈)
		g_quickSlot[0] = TOOL_HOE;
		g_quickSlot[1] = TOOL_WATER;
		g_quickSlot[2] = TOOL_POLE;
		g_quickSlot[3] = TOOL_NONE;
		// 4x4: 0번 칸에 도끼 1개 시드
		for (int kk = 0; kk < INV4_COUNT; kk++) g_inv4[kk] = TOOL_NONE;
		g_inv4[0] = TOOL_AXE;
		for (int rr = 0; rr < MAP_TILE_H; rr++)
			for (int cc = 0; cc < MAP_TILE_W; cc++) farmState[rr][cc] = 0;

		// 플레이어 이미지 로드 (아래 base/base_arm 로드에서 수행)

		//씬/플레이어 초기화
		g_currentScene = SCENE_FARM;
		g_player.x = 700; g_player.y = 700;  // 절벽 RECT(0,0~600,450) 밖에서 시작
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
		g_player.fishing_arm[0] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\사람\\farmer_fishing_앞면1_16x41.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_player.fishing_arm[1] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\사람\\farmer_fishing_앞면2_16x41.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_player.fishing_arm[2] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\사람\\farmer_fishing_앞면3_16x41.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_player.fishing_arm[3] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\사람\\farmer_fishing_앞면4_16x41.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_player.fishing_arm[4] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\사람\\farmer_fishing_앞면5_16x41.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_player.fishing_arm[5] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\사람\\farmer_fishing_옆면1_44x33.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_player.fishing_arm[6] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\사람\\farmer_fishing_옆면2_44x33.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_player.fishing_arm[7] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\사람\\farmer_fishing_옆면3_44x33.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_player.fishing_arm[8] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\사람\\farmer_fishing_옆면4_44x33.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_player.fishing_arm[9] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\사람\\farmer_fishing_옆면5_44x33.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);

		//낚시 로드
		hBitmap = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\낚시\\fishingmap_640x640.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);

		hBitmap_fishing[0] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\낚시\\fishing_37x149.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		hBitmap_fishing[1] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\낚시\\green_10x10.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION); // 3배정도 길게 늘려서 사용. 10x50 정도의 크기.
		hBitmap_fishing[2] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\낚시\\fish_39x20.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION); // 19x20 씩 잘라서 사용해야 함. 19x20, 1픽셀 띄우고 다시 19x20 이렇게.
		hBitmap_fishing[3] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\낚시\\물었다_74x28.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_hBitmap_biteNotice = hBitmap_fishing[3]; // DrawPlayer에서 접근용

		hBitmap_fishingGround = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\낚시\\fishingmap_1280x1280.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		hBitmap_fishTree[0] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\낚시\\Tree1_118x140.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		hBitmap_fishTree[1] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\낚시\\Tree2_56x72.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		hBitmap_fishTree[2] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\낚시\\Tree3_50x62.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);

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
				// 농장배경.bmp 원본 사이즈 (실측: 2290x1856)
				const int FARM_BG_SRC_W = 2290;
				const int FARM_BG_SRC_H = 1856;
				HDC hFarmDC = CreateCompatibleDC(backDC);
				HBITMAP hOldFarm = (HBITMAP)SelectObject(hFarmDC, hBitmap_farm);
				SetStretchBltMode(backDC, HALFTONE);
				StretchBlt(backDC, 0, 0, CLIENT_W, CLIENT_H,
					hFarmDC, 0, 0, FARM_BG_SRC_W, FARM_BG_SRC_H,
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

			if (g_hBitmap_map != NULL) {
				HDC hMapDC = CreateCompatibleDC(backDC);
				HBITMAP hOldMap = (HBITMAP)SelectObject(hMapDC, g_hBitmap_map);
				BitBlt(backDC, 0, 0, CLIENT_W, CLIENT_H,
					hMapDC, cameraX, cameraY, SRCCOPY);
				SelectObject(hMapDC, hOldMap);
				DeleteDC(hMapDC);
			}


			DrawFarmTiles(backDC);


			DrawTrees(backDC);
			DrawHouseAnimated(backDC);


			{
				int savedX = g_player.x, savedY = g_player.y;
				g_player.x -= cameraX;
				g_player.y -= cameraY;
				DrawPlayer(backDC);
				g_player.x = savedX;
				g_player.y = savedY;
			}

			// 안내
			SetBkMode(backDC, TRANSPARENT);
			SetTextColor(backDC, RGB(255, 255, 255));
			const wchar_t* info = L"[농장] 방향키/WASD 이동, 1/2/3 도구, E 인벤토리";
			TextOut(backDC, 10, 10, info, (int)wcslen(info));

			// ----- [확장] UI (화면 고정, 최상단) -----
			DrawQuickSlot(backDC);
			DrawInventoryPanel(backDC);
			break;
		}

		case SCENE_FISHING:
		{
			// 낚시 씬 배경 
			if (!hBitmap_fishingGround) {
				HBRUSH seaBrush = CreateSolidBrush(RGB(60, 110, 170));
				RECT seaRect = { 0, 0, CLIENT_W, CLIENT_H };
				FillRect(backDC, &seaRect, seaBrush);
				DeleteObject(seaBrush);
			}
			else {
				HDC hFishingDC = CreateCompatibleDC(backDC);
				HBITMAP hOldFarm = (HBITMAP)SelectObject(hFishingDC, hBitmap_fishingGround);
				SetStretchBltMode(backDC, HALFTONE);
				StretchBlt(backDC, 0, 0, CLIENT_W, CLIENT_H,
					hFishingDC, 0, 0, 1280, 1280,
					SRCCOPY);
				SelectObject(hFishingDC, hOldFarm);
				DeleteDC(hFishingDC);
			}

			// 낚시터플레이어이동못하는영역그리기(backDC);
			// 낚시가능영역그리기(backDC);
			// 농장으로이동영역그리기(backDC);

			// 플레이어 발 Y 계산
			int playerFootY = g_player.y + g_player.h * 3 / 4;

			// 플레이어 뒤에 있는 나무 먼저 (플레이어가 나무 앞에 서있는 경우)
			DrawTrees(backDC, hBitmap_fishTree, playerFootY, false);

			// 플레이어 (스프라이트 or 도형)
			DrawPlayer(backDC);

			// 플레이어 앞에 있는 나무 나중에 (나무가 플레이어를 가리는 경우)
			DrawTrees(backDC, hBitmap_fishTree, playerFootY, true);

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

		case SCENE_SHOP:
		{
			// 상점 씬: 임시 회색 배경 + 안내 텍스트
			HBRUSH shopBg = CreateSolidBrush(RGB(120, 120, 120));
			RECT r = { 0, 0, CLIENT_W, CLIENT_H };
			FillRect(backDC, &r, shopBg);
			DeleteObject(shopBg);

			SetBkMode(backDC, TRANSPARENT);
			SetTextColor(backDC, RGB(255, 255, 255));
			const wchar_t* msg = L"상점 씬입니다";
			TextOut(backDC, CLIENT_W / 2 - 60, CLIENT_H / 2 - 20, msg, (int)wcslen(msg));
			const wchar_t* hint = L"(추후 상점 UI 구현 예정)";
			TextOut(backDC, CLIENT_W / 2 - 90, CLIENT_H / 2 + 10, hint, (int)wcslen(hint));
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

		if (g_currentScene == SCENE_FARM) {
			int mx = LOWORD(lParam);
			int my = HIWORD(lParam);

			// 1) 가방 아이콘 클릭 → 인벤토리 토글
			if (IsClickInBag(mx, my)) {
				g_isInventoryOpen = !g_isInventoryOpen;
				g_selectedInv4 = -1;
				InvalidateRect(hWnd, NULL, FALSE);
				break;
			}

			// 2) 4x4 인벤토리 셀 클릭 (열려있을 때)
			if (g_isInventoryOpen) {
				int cell = HitInv4Cell(mx, my);
				if (cell >= 0) {
					// 빈 셀은 선택하지 않음
					if (g_inv4[cell] != TOOL_NONE) g_selectedInv4 = cell;
					InvalidateRect(hWnd, NULL, FALSE);
					break;
				}
			}

			// 3) 퀵슬롯 클릭
			int qs = HitQuickSlot(mx, my);
			if (qs >= 0) {
				// 4번째(빈 슬롯) + 4x4에서 선택된 아이템 있으면 → 스와핑
				if (qs == 3 && g_selectedInv4 >= 0) {
					EquipInv4ToQuickslot(g_selectedInv4, qs);
				}
				else {
					// 그 외엔 도구 선택
					SelectQuickSlot(qs);
				}
				InvalidateRect(hWnd, NULL, FALSE);
				break;
			}

			// 4) UI 다른 영역(배경 패널 등)은 그냥 흡수
			if (IsClickOnUI(mx, my)) {
				InvalidateRect(hWnd, NULL, FALSE);
				break;
			}

			// 5) 맵 클릭 처리 (인벤토리 닫혀있을 때만)
			if (!g_isInventoryOpen) {
				// 도끼 들고 있으면 나무 벌목 시도
				if (g_currentTool == TOOL_AXE) {
					TryChopTree(mx, my);
				}
				else {
					HandleFarmClick(mx, my);
				}
				InvalidateRect(hWnd, NULL, FALSE);
				break;
			}
		}

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
				// 낚시터 씬일 때 낚시 가능 여부 갱신 (발 기준)
				if (g_currentScene == SCENE_FISHING) {
					int footOffX = g_player.w / 4;
					int footOffY = g_player.h * 3 / 4;
					int footW = g_player.w / 2;
					int footH = g_player.h / 4;
					if (IsInFishingArea(g_player.x + footOffX, g_player.y + footOffY, footW, footH, g_player.dir)) {
						canFishing = true;
					}
					else {
						canFishing = false;
					}
				}
				// [확장] 농장 씬에서는 카메라 따라가기 + 집 애니메이션 + 문열림 완료 시 SCENE_SHOP
				if (g_currentScene == SCENE_FARM) {
					UpdateCamera();
					UpdateHouseAnim();

					// 플레이어가 집 문 충돌박스에 들어오면 문 열기 시작
					// 문 애니 시작: 캐릭터의 절대 좌표가 1타일 문 트리거 안에 있을 때만
					if (!g_house.isDoorOpening &&
						RectOverlap(g_player.x, g_player.y, g_player.w, g_player.h,
							HOUSE_DOOR_X, HOUSE_DOOR_Y, HOUSE_DOOR_W, HOUSE_DOOR_H)) {
						g_house.isDoorOpening = true;
						g_house.currentFrame = 0;
						g_house.frameTimer = 0;
						g_house.doorAnimDone = false;
						g_house.shopTransitionTimer = 0;
					}
					// 문 애니 끝나면 1초 카운트다운 시작 후 SCENE_SHOP으로 전환
					if (g_house.doorAnimDone && g_sceneCooldown == 0) {
						g_house.shopTransitionTimer++;
						// WM_TIMER 0001 이 30ms 간격 → 약 33틱이면 1초
						if (g_house.shopTransitionTimer >= 33) {
							g_currentScene = SCENE_SHOP;
							g_sceneCooldown = 30;
							// 다음에 농장 돌아왔을 때 다시 문에 안 끼게 아래로 보냄
							g_player.x = g_house.x + HOUSE_DRAW_W / 2 - g_player.w / 2;
							g_player.y = g_house.y + HOUSE_DRAW_H + HOUSE_DOOR_H + 10;
							g_house.isDoorOpening = false;
							g_house.doorAnimDone = false;
							g_house.currentFrame = 0;
							g_house.shopTransitionTimer = 0;
						}
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
				//낚시 실패 처리
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
		// 방향키/WASD 입력 
		switch (wParam) {
		//  도구 단축키 / 인벤토리 토글 
		case '1': SelectQuickSlot(0); break;
		case '2': SelectQuickSlot(1); break;
		case '3': SelectQuickSlot(2); break;
		case '4': SelectQuickSlot(3); break;
		case 'E': g_isInventoryOpen = !g_isInventoryOpen; g_selectedInv4 = -1; break;
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

	SetProcessDPIAware();

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

	RECT desiredRc = { 0, 0, CLIENT_W, CLIENT_H };
	AdjustWindowRect(&desiredRc, WS_OVERLAPPEDWINDOW, FALSE);
	int winW = desiredRc.right - desiredRc.left;
	int winH = desiredRc.bottom - desiredRc.top;

	hWnd = CreateWindow(lpszClass, lpszWindowName, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, winW, winH, NULL, (HMENU)NULL, hInstance, NULL);
	
	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	while (GetMessage(&Message, 0, 0, 0)) {
		TranslateMessage(&Message);
		DispatchMessage(&Message);
	}
	return Message.wParam;
}