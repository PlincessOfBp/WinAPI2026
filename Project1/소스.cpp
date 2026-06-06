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
	SCENE_FARM = 0,        // 농장 화면 (조성현 담당)
	SCENE_FISHING = 1,     // 낚시 화면 (문선우 담당)
	SCENE_FISHING_MAP = 1, // (별칭) 친구 낚시 맵
	SCENE_SHOP = 2         // 상점 화면 (집 문으로 진입)
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

// 아이템 구조체
struct Item {
	int x, y;
	int w, h;
	int count; // 아이템 위 아래로 둥둥 부유하는거
	HBITMAP hBitmap;
};

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
	HBITMAP farming_arm[24]; // 0~4 앞 호미질, 5~9 옆 호미질, 10~14 앞 도끼질, 15~19 옆 도끼질, 20~21 앞 물뿌리개, 22~23 옆 물뿌리개. 01234 순으로 애니메이션 진행. 물뿌리개는 20 21 21, 22 23 23 처럼 뒤의 스프라이트가 더 오래 진행. 옆면은 오른쪽 보고있음. 좌우반전 필요. 뒷면은 앞면을 플레이어 몸 뒤에 그리기.
};

static Player g_player = { 380, 380, PLAYER_DISPLAY_W, PLAYER_DISPLAY_H, 4, DIR_DOWN, 0, 0, false };


void 플레이어참고(HDC hDC, HBITMAP base[9], HBITMAP base_arm[])
{

	HDC hFishDC = CreateCompatibleDC(hDC);
	// 몸 베이스
	SelectObject(hFishDC, base[3]);
	TransparentBlt(hDC, 0, 0, 160, 280, hFishDC, 0, 0, 16, 28, RGB(255, 0, 255));

	// 정면 팔 (호미 & 도끼)
	// SelectObject(hFishDC, base_arm[4]);
	// TransparentBlt(hDC, -80, -60, 320, 370, hFishDC, 0, 0, 32, 37, RGB(255, 0, 255));

	// 정면 & 옆면 팔 (물뿌리개)
	SelectObject(hFishDC, base_arm[23]);
	TransparentBlt(hDC, -20, -100, 200, 270, hFishDC, 0, 0, 20, 27, RGB(255, 0, 255));

	// 옆면 팔 (호미 & 도끼)
	// SelectObject(hFishDC, base_arm[9]);
	// TransparentBlt(hDC, -70, -60, 320, 370, hFishDC, 0, 0, 32, 37, RGB(255, 0, 255));

	DeleteDC(hFishDC);
}

// 입력 상태
static bool g_keyLeft = false;
static bool g_keyRight = false;
static bool g_keyUp = false;
static bool g_keyDown = false;

// 농장 → 낚시 트리거 
#define TRIGGER_TO_FISH_X   1200
#define TRIGGER_TO_FISH_Y   90
#define TRIGGER_TO_FISH_W    80
#define TRIGGER_TO_FISH_H    180

// 씬 전환 직후 같은 트리거에서 핑퐁 방지용 쿨다운
static int g_sceneCooldown = 0;;

// ── 물고기 아이템 ──
enum FishItemType {
	FISH_ITEM_NONE = 0,
	FISH_ITEM_CARP = 1, // 잉어   (FISH_MOVE_RANDOM)
	FISH_ITEM_PUFFER = 2, // 복어   (FISH_MOVE_FAST_UP)
	FISH_ITEM_SQUID = 3, // 오징어 (FISH_MOVE_FAST_DOWN)
	FISH_ITEM_BASS = 4  // 농어   (FISH_MOVE_IRREGULAR)
};

// 바닥에 떨어진 물고기 아이템
struct DroppedFish {
	int   worldX, worldY;   // 월드 좌표 중심
	FishItemType type;
	bool  active;
	int   floatTimer;       // 부유 애니 카운터 (0~39 반복)
	int   graceTick;        // 획득 유예 틱 카운터 (0.5초 = 5틱 @ 100ms)
};
static std::vector<DroppedFish> g_droppedFishes;
// 유예 시간 (타이머 0001 기준 100ms/틱) — 이 값만 바꾸면 외부 조정 가능
// 10 = 1.0초, 5 = 0.5초, 20 = 2.0초
static int g_fishGraceTicks = 10;
#define DROPPED_FISH_FLOAT_RANGE    3   // 위아래 최대 진폭 (픽셀)
#define DROPPED_FISH_FLOAT_PERIOD  40   // 1사이클 틱 수

// 물고기 아이템 인벤토리 (16칸, 별도 관리)
#define FISH_INV_COUNT 16
static FishItemType g_fishInv[FISH_INV_COUNT] = { FISH_ITEM_NONE, };

// 물고기 인벤토리에 아이템 추가 (빈 칸에 넣기)
static void AddFishToInventory(FishItemType type) {
	for (int i = 0; i < FISH_INV_COUNT; i++) {
		if (g_fishInv[i] == FISH_ITEM_NONE) {
			g_fishInv[i] = type;
			return;
		}
	}
	// TODO: 인벤토리가 꽉 찼을 때 처리 (알림 등)
}

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

//여기가 시간 바꾸는데니깐 너가 바꾸고 싶으면 바꿔.
// [낮/밤 시스템] g_timeOfDay 0.0(아침) ~ 1.0(다음날 직전)
//   - 시간대 구간:
//     * 0.00 ~ 0.50  낮            (Alpha = 0)
//     * 0.50 ~ 0.65  저녁 (어두워짐, 0 → 180)
//     * 0.65 ~ 0.90  밤            (Alpha = 180)
//     * 0.90 ~ 1.00  아침 (밝아짐, 180 → 0)
//   - g_timeOfDay 가 1.0 이상이면 g_gameDay++ 하고 0.0 리셋

static int   g_gameDay = 1;       // 현재 일차 (1일차부터 시작)
static float g_timeOfDay = 0.0f;    // 하루의 진행도 (0.0 ~ 1.0)

// 하루 길이 조정용: g_timeOfDay 가 한 틱(30ms)당 얼마나 증가할지
// (현실 1초 ≈ 33틱 → 하루 = 120초 → 4000틱 → 1/4000 = 0.00025)
#define TIME_OF_DAY_STEP   (1.0f / 4000.0f)

// 현재 시각에 따른 밤 오버레이 알파(0~255) 계산
static int CalcNightAlpha() {
	float t = g_timeOfDay;
	if (t < 0.50f) return 0;                                  // 낮
	if (t < 0.65f) return (int)((t - 0.50f) / 0.15f * 180);   // 저녁 fade out
	if (t < 0.90f) return 180;                                // 밤 (최대 어두움)
	return (int)((1.00f - t) / 0.10f * 180);                  // 아침 fade in
}

// 검은(살짝 푸른) 반투명 사각형으로 화면 전체를 덮어 밤 연출.
// 호출 시점: 맵/캐릭터/오브젝트 그린 후, UI 그리기 전.
void DrawNightOverlay(HDC hDC) {
	int alpha = CalcNightAlpha();
	if (alpha <= 0) return;
	HDC memDC = CreateCompatibleDC(hDC);
	HBITMAP bmp = CreateCompatibleBitmap(hDC, CLIENT_W, CLIENT_H);
	HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, bmp);
	// 약간 푸른 빛의 검은색이 자연스러운 밤 느낌
	HBRUSH br = CreateSolidBrush(RGB(5, 10, 40));
	RECT r = { 0, 0, CLIENT_W, CLIENT_H };
	FillRect(memDC, &r, br);
	DeleteObject(br);
	BLENDFUNCTION bf = { AC_SRC_OVER, 0, (BYTE)alpha, 0 };
	AlphaBlend(hDC, 0, 0, CLIENT_W, CLIENT_H,
		memDC, 0, 0, CLIENT_W, CLIENT_H, bf);
	SelectObject(memDC, oldBmp);
	DeleteObject(bmp);
	DeleteDC(memDC);
}

// 전방 선언: 실제 본체는 farmState/g_cropType 등 정의 아래에 있음
void UpdateFarmAtNight();
void UpdateGameTime();

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

// ---------- 나무 스폰 금지 영역 (흙길 + 절벽 옆면) ----------
// InitTrees 가 나무 배치할 때 이 RECT 안에 발 위치가 들어가면 거부.
// 좌표 안 맞으면 left/top/right/bottom 만 조정. 영역 추가/제거하려면 COUNT 와 배열 길이 같이 맞춰주세요.
#define NO_TREE_RECT_COUNT 5
static RECT g_noTreeRects[NO_TREE_RECT_COUNT] = {
	{   0, 420,  610, 480 },   // 좌측 절벽 아래 갈색 옆면 (가로 띠)
	{ 870, 110, 1290, 270 },   // 우상단 흙길 (가로)
	{ 950, 270, 1020, 700 },   // 우측 흙길 (세로)
	{ 410, 660,  970, 720 },   // 가운데 흙길 (가로)
	{ 410, 720,  480, 1290 },  // 아래쪽 흙길 (세로)
};

static bool PointInNoTreeArea(int x, int y) {
	for (int i = 0; i < NO_TREE_RECT_COUNT; i++) {
		RECT& r = g_noTreeRects[i];
		if (x >= r.left && x < r.right && y >= r.top && y < r.bottom) return true;
	}
	return false;
}

// ---------- 친구 맵 포탈 트리거 (맵 우측 상단 흙길 끝) ----------
// 사용자 지정: worldX > 1200 && worldY < 250 부근
#define PORTAL_X_MIN   1200
#define PORTAL_X_MAX   1280
#define PORTAL_Y_MIN     90
#define PORTAL_Y_MAX    270
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
	int shakeTimer; // 흔들림 카운터 (0이면 흔들림 없음)
};
static std::vector<Tree> g_trees;
#define TREE_COUNT      15
#define TREE_HP_DEFAULT  3   // 도끼 N번 치면 베어짐 — 외부에서 이 값 수정 가능
#define TREE_SHAKE_TICKS 8   // 흔들림 지속 틱 수 — 외부에서 조정 가능
#define TREE_SHAKE_OFFX  4   // 흔들림 최대 픽셀 오프셋 — 외부에서 조정 가능

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
static AnimatedHouse g_house = { 705, 470, 0, 0, false, false, 0 };

#define HOUSE_FRAME_W       96
#define HOUSE_FRAME_H       96
#define HOUSE_FRAME_COUNT    8       // 0~7
#define HOUSE_DRAW_W       192       // 화면에 2배 확대 출력
#define HOUSE_DRAW_H       192
#define HOUSE_ANIM_TICKS    10       // 프레임 전환 간격

// 집의 문 충돌 박스: 절대 좌표 기준 1타일(32x32), 집 바로 아래 (현관문 바로 앞)
// 캐릭터의 worldX/worldY가 이 RECT 안일 때만 문 열림 애니 시작.
#define HOUSE_DOOR_W       24
#define HOUSE_DOOR_H       10
#define HOUSE_DOOR_X       (g_house.x + (HOUSE_DRAW_W - HOUSE_DOOR_W) / 2)
#define HOUSE_DOOR_Y       (g_house.y + HOUSE_DRAW_H - HOUSE_DOOR_H)  // 집 아래 끝에서 위로

// ---------- 농사 2D 배열 ----------
// 0=기본 땅, 1=경작된 땅, 2=젖은 땅
static int farmState[MAP_TILE_H][MAP_TILE_W] = { 0, };

// ----- 작물 시스템 -----
// CropType: 어떤 작물이 심어졌는지
enum CropType {
	CROP_NONE = 0,
	CROP_CARROT = 1,
	CROP_POTATO = 2,
	CROP_RHUBARB = 3,
	CROP_STRAWBERRY = 4,
	CROP_COUNT = 5
};

// ===== FarmTile 구조체 (사용자 요청) =====
// 각 타일의 모든 정보를 하나로 묶음.
// 작물의 절대 좌표를 별도로 두지 않고, 항상 farmGrid[r][c] 의 인덱스 = 격자 위치로 종속됨.
// 그래서 다음 날에도 작물이 엉뚱한 곳으로 날아갈 일이 없음.
struct FarmTile {
	int isTilled;   // 경작 여부 (0=일반, 1=경작, 2=물줌) → farmState 와 동일 의미
	int isWatered;  // 0/1 (호환용 — isTilled==2 와 동일하게 동작)
	int cropType;   // CROP_NONE/CARROT/POTATO/RHUBARB/STRAWBERRY
	int growStage;  // 0=seed, 1=grow1, 2=grow2, 3=ready
	int plantedDay; // 심은 일차 (g_gameDay 값)
};
// (실제 데이터는 평행 배열로 유지 — 기존 코드 영향 최소화)
// growStage: 0=seed, 1=grow1, 2=grow2, 3=ready
static int g_cropType[MAP_TILE_H][MAP_TILE_W] = { 0, };
static int g_growStage[MAP_TILE_H][MAP_TILE_W] = { 0, };
static int g_plantedDay[MAP_TILE_H][MAP_TILE_W] = { 0, };

// FarmTile 헬퍼: 격자 인덱스(col, row)로 FarmTile 정보 묶어서 가져오기
static FarmTile GetFarmTile(int col, int row) {
	FarmTile t;
	t.isTilled = farmState[row][col];
	t.isWatered = (farmState[row][col] == 2) ? 1 : 0;
	t.cropType = g_cropType[row][col];
	t.growStage = g_growStage[row][col];
	t.plantedDay = g_plantedDay[row][col];
	return t;
}
static void SetFarmTile(int col, int row, const FarmTile& t) {
	farmState[row][col] = t.isTilled;
	g_cropType[row][col] = t.cropType;
	g_growStage[row][col] = t.growStage;
	g_plantedDay[row][col] = t.plantedDay;
}

// 작물 비트맵 [CropType][growStage 0~3]
static HBITMAP g_hBitmap_crop[CROP_COUNT][4] = { NULL, };

// (SeedToCrop / CropToHarvestItem 본체는 ItemType 정의 후에 위치함 — 아래쪽 참조)

// 농사 타일 시트에서 잘라낼 좌표 (사용자 지정값)
#define FARM_TILE_TILLED_SRC_X    96
#define FARM_TILE_TILLED_SRC_Y  1152
#define FARM_TILE_WET_SRC_X      352
#define FARM_TILE_WET_SRC_Y      128

// ---------- 도구 ----------
// 모든 아이템(도구/씨앗/물고기 등) 통합 enum
// 기존 ToolType 호환 위해 도구 값은 동일하게 유지.
enum ToolType {
	TOOL_NONE = 0,
	TOOL_HOE = 1,           // 호미
	TOOL_WATER = 2,         // 물뿌리개
	TOOL_POLE = 3,          // 낚싯대
	TOOL_AXE = 4,           // 도끼
	// --- 채집/획득 아이템 ---
	ITEM_SEED_CARROT = 5,
	ITEM_SEED_POTATO = 6,
	ITEM_SEED_RHUBARB = 7,
	ITEM_SEED_STRAWBERRY = 8,
	ITEM_FISH_CARP = 9,
	ITEM_FISH_PERCH = 10,
	ITEM_FISH_PUFFERFISH = 11,
	ITEM_FISH_SQUID = 12,
	ITEM_LOG = 13,          // 통나무 (도끼로 나무 벌목 시 드롭)
	ITEM_CARROT = 14,       // 당근 (수확)
	ITEM_POTATO = 15,       // 감자 (수확)
	ITEM_RHUBARB = 16,      // 대황 (수확)
	ITEM_STRAWBERRY = 17,   // 딸기 (수확)
	ITEM_COUNT = 18
};
// ItemType 은 ToolType 동의어 (사용자 요청 이름)
typedef ToolType ItemType;
#define ITEM_NONE   TOOL_NONE
#define ITEM_HOE    TOOL_HOE
#define ITEM_WATER  TOOL_WATER
#define ITEM_ROD    TOOL_POLE
#define ITEM_AXE    TOOL_AXE

// 바닥 드롭 아이템 (통나무/작물) — 물고기 드롭과 동일한 로직
struct DroppedItem {
	int      worldX, worldY;
	ItemType type;
	bool     active;
	int      floatTimer;
	int      graceTick;
};
static std::vector<DroppedItem> g_droppedItems;
static int g_itemGraceTicks = 5;  // 획득 유예 틱 수 — 외부 조정 가능 (5 = 0.5초)
#define DROPPED_ITEM_FLOAT_RANGE    3
#define DROPPED_ITEM_FLOAT_PERIOD  40
#define DROPPED_ITEM_DRAW_W        48
#define DROPPED_ITEM_DRAW_H        48

// ----- ItemType 정의 이후에 위치 — 씨앗/작물 변환 헬퍼 -----
static int SeedToCrop(ItemType seed) {
	switch (seed) {
	case ITEM_SEED_CARROT:     return CROP_CARROT;
	case ITEM_SEED_POTATO:     return CROP_POTATO;
	case ITEM_SEED_RHUBARB:    return CROP_RHUBARB;
	case ITEM_SEED_STRAWBERRY: return CROP_STRAWBERRY;
	default: return CROP_NONE;
	}
}
static ItemType CropToHarvestItem(int crop) {
	switch (crop) {
	case CROP_CARROT:     return ITEM_CARROT;
	case CROP_POTATO:     return ITEM_POTATO;
	case CROP_RHUBARB:    return ITEM_RHUBARB;
	case CROP_STRAWBERRY: return ITEM_STRAWBERRY;
	default: return ITEM_NONE;
	}
}

// ----- 농장 갱신 함수 본체 (farmState/g_cropType/g_growStage 모두 위쪽에 정의됨) -----
void UpdateFarmAtNight() {
	for (int r = 0; r < MAP_TILE_H; r++) {
		for (int c = 0; c < MAP_TILE_W; c++) {
			// 1) 물 준 땅 + 작물 심어진 곳 → growStage++ (최대 3)
			if (farmState[r][c] == 2 && g_cropType[r][c] != CROP_NONE && g_growStage[r][c] < 3) {
				g_growStage[r][c]++;
			}
			// 2) 물 준 땅(2) → 마른 땅(1) 복귀 (다음 날 다시 물 줘야 자람)
			if (farmState[r][c] == 2) {
				farmState[r][c] = 1;
			}
		}
	}
}
void UpdateGameTime() {
	g_timeOfDay += TIME_OF_DAY_STEP;
	if (g_timeOfDay >= 1.0f) {
		g_timeOfDay = 0.0f;
		g_gameDay++;
		UpdateFarmAtNight();   // 새 날 시작 시 농장 갱신
	}
}

static ToolType g_currentTool = TOOL_NONE;
static bool g_isInventoryOpen = false;

// 플레이어 골드
static int g_gold = 0; // 초기 골드 (외부에서 조정 가능)

// 상점 씨앗 데이터
#define SHOP_SEED_COUNT 4
static const wchar_t* g_seedNames[SHOP_SEED_COUNT] = { L"감자 씨앗", L"당근 씨앗", L"대황 씨앗", L"딸기 씨앗" };
static int             g_seedPrices[SHOP_SEED_COUNT] = { 25, 15, 50, 100 };

// ---------- 아이템 판매가 (ItemType 인덱스 기준, 0이면 판매 불가) ----------
// 도구(0~4)는 0으로 두어 판매 불가 처리. 외부에서 이 값만 바꾸면 가격 조정 가능.
static int g_itemSellPrice[ITEM_COUNT] = {
	0,    // TOOL_NONE
	0,    // TOOL_HOE
	0,    // TOOL_WATER
	0,    // TOOL_POLE
	0,    // TOOL_AXE
	15,   // ITEM_SEED_CARROT    (씨앗은 구매가와 동일)
	25,   // ITEM_SEED_POTATO
	50,  // ITEM_SEED_RHUBARB
	100,  // ITEM_SEED_STRAWBERRY
	30,   // ITEM_FISH_CARP      잉어
	55,   // ITEM_FISH_PERCH     농어
	200,  // ITEM_FISH_PUFFERFISH 복어
	80,   // ITEM_FISH_SQUID     오징어
	30,   // ITEM_LOG            나무 (씨앗 초기자금용 — 당근씨앗 2개 살 수 있는 가격)
	35,   // ITEM_CARROT         당근
	80,   // ITEM_POTATO         감자
	220,  // ITEM_RHUBARB        대황
	120,  // ITEM_STRAWBERRY     딸기
};
// 씨앗 아이템 화면 위치 (WM_PAINT의 TransparentBlt 좌표와 동일)
static int g_seedItemX[SHOP_SEED_COUNT] = { 100 + 25, 100 + 95, 100 + 95 + 70, 100 + 90 + 140 };
static int g_seedItemY[SHOP_SEED_COUNT] = { 200 + 25, 200 + 25, 200 + 25,      200 + 25 };
#define SHOP_SEED_W 48
#define SHOP_SEED_H 48

// 현재 호버 중인 씨앗 인덱스 (-1이면 없음)
static int g_shopHoverIdx = -1;
// 상점에서 인벤토리 호버 중인 슬롯 인덱스 (-1이면 없음, bag 기준)
static int g_shopInvHoverSlot = -1;

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

// 사용자 요청 이름과 호환 (같은 배열을 다른 이름으로도 접근 가능)
#define quickSlots  g_quickSlot
#define bagSlots    g_inv4

// ----- 인벤토리 슬롯 (type + count) -----
// 사용자 요청 구조체. 기존 ToolType 배열과 평행하게 count 배열을 둠.
struct InventorySlot {
	ItemType type;
	int      count;
};
static int g_quickCount[QUICKSLOT_COUNT] = { 1, 1, 1, 1 };   // 도구는 1개 기본
static int g_bagCount[INV4_COUNT] = { 0, };           // 가방 빈칸 = 0

// 헬퍼: 슬롯 한 칸 정보 가져오기/넣기
static InventorySlot GetQuickInv(int i) {
	InventorySlot s = { g_quickSlot[i], g_quickCount[i] };
	return s;
}
static void SetQuickInv(int i, InventorySlot s) {
	g_quickSlot[i] = s.type;
	g_quickCount[i] = s.count;
}
static InventorySlot GetBagInv(int i) {
	InventorySlot s = { g_inv4[i], g_bagCount[i] };
	return s;
}
static void SetBagInv(int i, InventorySlot s) {
	g_inv4[i] = s.type;
	g_bagCount[i] = s.count;
}

// 아이템별 아이콘 비트맵 (ItemType 인덱스로 직접 접근)
static HBITMAP g_hBitmap_item[ITEM_COUNT] = { NULL, };

// 드래그 앤 드롭 상태
static int      g_draggedSlotIndex = -1;     // 드래그 시작한 슬롯 인덱스 (-1: 드래그 중 아님)
static bool     g_isDraggingFromQuick = false; // 드래그 시작이 퀵슬롯인지 가방인지
static ItemType g_draggedItem = ITEM_NONE;
static int      g_dragMouseX = 0, g_dragMouseY = 0;  // 현재 마우스 위치 (드래그 아이콘 그리기용)

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
#define INV4_CELL_SIZE        48    // 한 슬롯 아이콘 크기 (드래그/충돌 판정 박스)

// 상점 씬 전용 인벤토리 위치 (기존 hBitmap_test 위치와 동일: 100,400 / 300x300)
#define SHOP_INV_BG_X         100
#define SHOP_INV_BG_Y         400
#define SHOP_INV_BG_W         300
#define SHOP_INV_BG_H         300

// ===== [슬롯 위치 미세 조정용 상수 — 이 값들만 바꾸면 위치 변경됨] =====
// 4x4 인벤토리 (사용자 실측: 첫 슬롯 좌상단 20,20 / 우상단 75,20 / 두번째 슬롯 88,20)
//   → 슬롯 한 칸 가로 = 75 - 20 = 55
//   → 다음 칸 거리   = 88 - 20 = 68
#define BAG_START_X_OFFSET    20    // 인벤토리 이미지 좌측 끝에서 첫 슬롯 좌상단 X
#define BAG_START_Y_OFFSET    20    // 인벤토리 이미지 상단 끝에서 첫 슬롯 좌상단 Y
#define BAG_SLOT_BOX_SIZE     55    // 한 슬롯 칸 한 변 픽셀
#define BAG_SLOT_STEP         68    // 다음 슬롯까지 이동 거리

// 1줄 퀵슬롯 (인벤토리와 같은 디자인 기준이라 동일 수치 사용 — 필요 시 따로 조정)
#define QUICK_START_X_OFFSET  20
#define QUICK_START_Y_OFFSET  20
#define QUICK_SLOT_BOX_SIZE   55
#define QUICK_SLOT_STEP       68

// ---------- 비트맵 핸들 (확장 리소스) ----------
static HBITMAP g_hBitmap_map = NULL; // background/map.bmp 1280x1280
static HBITMAP g_hBitmap_house = NULL; // 집/house.bmp 768x96 (96x96 × 8프레임)
static HBITMAP g_hBitmap_tree1 = NULL; // tree/Tree1_118x140.bmp
static HBITMAP g_hBitmap_tree2 = NULL; // tree/Tree2_56x72.bmp
static HBITMAP g_hBitmap_tree3 = NULL; // tree/Tree3_50x62.bmp
static HBITMAP g_hBitmap_cultivated = NULL; // farming/cultivated_land.bmp 32x32
static HBITMAP g_hBitmap_wet = NULL; // farming/wet_ground.bmp 32x32
static HBITMAP g_hBitmap_icon_hoe = NULL;
static HBITMAP g_hBitmap_icon_water = NULL;
static HBITMAP g_hBitmap_icon_pole = NULL;
static HBITMAP g_hBitmap_inv_quick = NULL; // inventory_1.bmp 300x96
static HBITMAP g_hBitmap_inv_main = NULL; // inventory_4.bmp 300x300
static HBITMAP g_hBitmap_inv_bag = NULL; // Inventory.bmp 64x68

// 물고기 아이템 비트맵 (바닥 드롭 및 인벤토리 표시용)
// 이미지소스\\인벤토리\\Carp_48x48.bmp, Perch_48x48.bmp, Squid_48x48.bmp, Pufferfish_48x48.bmp
static HBITMAP g_hBitmap_fishItem[5] = { NULL, }; // [0]=없음 [1]=잉어 [2]=복어 [3]=오징어 [4]=농어
#define FISH_ITEM_DRAW_W  48
#define FISH_ITEM_DRAW_H  48

// 공용 투명색
#define EXT_TRANSPARENT      RGB(255, 0, 255)

// ---------- 헬퍼: 두 점 거리(타일 단위) ----------
static int TileChebyshevDist(int ax, int ay, int bx, int by) {
	int dx = ax - bx; if (dx < 0) dx = -dx;
	int dy = ay - by; if (dy < 0) dy = -dy;
	return (dx > dy) ? dx : dy;
}

// ---------- 카메라 업데이트 (스타듀밸리식 정중앙 추적) ----------
// 매 프레임 플레이어를 화면 정중앙에 두도록 카메라 좌표 재계산.
// cameraX = player.worldX - (화면너비/2) + (플레이어가로/2)
// cameraY = player.worldY - (화면높이/2) + (플레이어세로/2)
// 그리고 맵 경계(0 ~ 480)로 clamp.
void UpdateCamera() {
	cameraX = g_player.x - (CLIENT_W / 2) + (g_player.w / 2);
	cameraY = g_player.y - (CLIENT_H / 2) + (g_player.h / 2);

	// Clamp 0 ~ 480 (맵 1280 - 화면 800)
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

// 사각형이 집과 겹치는가 (이동 충돌 검사용 — 캐릭터가 집 위로 못 올라가게)
// 집 192x192 통째로 막으면 좌우가 너무 답답하므로, 실제 집 본체만 좁게.
// 좌/우 각 50px, 위 80px(지붕), 아래 10px 여유 → 92 x 102 영역만 차단.
#define HOUSE_BLOCK_PAD_L   50
#define HOUSE_BLOCK_PAD_R   50
#define HOUSE_BLOCK_PAD_T   47
#define HOUSE_BLOCK_PAD_B   10
static bool IsBlockedByHouse(int px, int py, int pw, int ph) {
	return RectOverlap(px, py, pw, ph,
		g_house.x + HOUSE_BLOCK_PAD_L,
		g_house.y + HOUSE_BLOCK_PAD_T,
		HOUSE_DRAW_W - HOUSE_BLOCK_PAD_L - HOUSE_BLOCK_PAD_R,
		HOUSE_DRAW_H - HOUSE_BLOCK_PAD_T - HOUSE_BLOCK_PAD_B);
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
		if (PointInNoTreeArea(footX, footY)) bad = true;   // 흙길/벽 옆면 회피
		// 다른 나무와 너무 겹쳐도 거부
		for (size_t k = 0; k < g_trees.size() && !bad; k++) {
			Tree& o = g_trees[k];
			if (RectOverlap(t.x, t.y, t.w, t.h, o.x, o.y, o.w, o.h)) bad = true;
		}
		if (bad) continue;
		t.hp = TREE_HP_DEFAULT;
		t.shakeTimer = 0;
		t.isAlive = true;
		g_trees.push_back(t);
		placed++;
	}
}

// ---------- 나무 그리기 (농장, 3종 비트맵, 카메라 오프셋) ----------
void DrawFarmTrees(HDC hDC) {
	if (g_trees.empty()) return;
	HDC memDC = CreateCompatibleDC(hDC);

	for (size_t i = 0; i < g_trees.size(); i++) {
		Tree& t = g_trees[i];
		if (!t.isAlive) continue;
		// 흔들림 오프셋: shakeTimer 홀수면 오른쪽, 짝수면 왼쪽
		int shakeOff = 0;
		if (t.shakeTimer > 0) {
			if (t.shakeTimer % 2 == 1) shakeOff = TREE_SHAKE_OFFX;
			else                       shakeOff = -TREE_SHAKE_OFFX;
		}
		int sx = t.x - cameraX + shakeOff;
		int sy = t.y - cameraY;
		if (sx + t.w < 0 || sx > CLIENT_W) continue;
		if (sy + t.h < 0 || sy > CLIENT_H) continue;

		HBITMAP src = NULL;
		if (t.kind == TREE_KIND_1)      src = g_hBitmap_tree1;
		else if (t.kind == TREE_KIND_2) src = g_hBitmap_tree2;
		else                            src = g_hBitmap_tree3;
		if (src == NULL) continue;

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
		//g_house.currentFrame = (g_house.currentFrame + 1) % 4;
		g_house.currentFrame = 0;   // 평상시엔 첫 프레임 고정
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

// ---------- 농사 타일 그리기 (사용자 명시 — 절대좌표 → 카메라 빼서 32x32 그대로) ----------
// 흙 타일은 무조건 32x32 크기로 worldTileX/Y → -cameraX/Y 위치에 먼저 그려져야 함.
// 그 위에 작물(DrawCrops)이 덮어씌워짐.
void DrawFarmTiles(HDC hdc) {
	if (g_hBitmap_cultivated == NULL && g_hBitmap_wet == NULL) return;
	HDC hdcTileMem = CreateCompatibleDC(hdc);
	for (int row = 0; row < MAP_TILE_H; row++) {
		for (int col = 0; col < MAP_TILE_W; col++) {
			int s = farmState[row][col];
			if (s == 0) continue;

			HBITMAP hTileBmp = (s == 1) ? g_hBitmap_cultivated : g_hBitmap_wet;
			if (hTileBmp == NULL) continue;

			// 타일 절대(World) 좌표
			int worldTileX = col * 32;
			int worldTileY = row * 32;

			// 화면 좌표 = 절대 - 카메라
			int screenX = worldTileX - cameraX;
			int screenY = worldTileY - cameraY;

			// 화면 컬링
			if (screenX + 32 < 0 || screenX > CLIENT_W) continue;
			if (screenY + 32 < 0 || screenY > CLIENT_H) continue;

			// 32x32 크기 고정으로 그림 (작물보다 먼저)
			SelectObject(hdcTileMem, hTileBmp);
			TransparentBlt(hdc,
				screenX, screenY, 32, 32,
				hdcTileMem, 0, 0, 32, 32,
				RGB(255, 0, 255));
		}
	}
	DeleteDC(hdcTileMem);
}

// ---------- 작물 그리기 (흙 32x32 안에 딱 맞춰 그림 — TransparentBlt 자동 축소) ----------
// 원본 비트맵 크기가 48x64, 56x56 등으로 다양하지만 모두 32x32 흙 타일 안에 맞춰 출력.
// (TransparentBlt 의 dest 사이즈와 src 사이즈가 다르면 자동 스케일링됨)
void DrawCrops(HDC hdc) {
	HDC hdcCropMem = CreateCompatibleDC(hdc);

	for (int row = 0; row < MAP_TILE_H; row++) {
		for (int col = 0; col < MAP_TILE_W; col++) {
			int cropType = g_cropType[row][col];
			int growStage = g_growStage[row][col];
			if (cropType == CROP_NONE) continue;
			if (growStage < 0 || growStage > 3) continue;

			HBITMAP hCropBmp = g_hBitmap_crop[cropType][growStage];
			if (hCropBmp == NULL) continue;

			// 1) 원본 크기 (src 영역)
			BITMAP bmp;
			GetObject(hCropBmp, sizeof(BITMAP), &bmp);
			int cropW = bmp.bmWidth;
			int cropH = bmp.bmHeight;

			// 2) 타일 절대 좌표 (32x32 격자)
			int worldTileX = col * 32;
			int worldTileY = row * 32;
			int screenX = worldTileX - cameraX;
			int screenY = worldTileY - cameraY;

			// 화면 컬링
			if (screenX + 32 < 0 || screenX > CLIENT_W) continue;
			if (screenY + 32 < 0 || screenY > CLIENT_H) continue;

			// 3) 흙 타일(32x32) 영역 안에 딱 맞게 출력 — 원본은 cropW x cropH, 화면은 32x32
			//    (dest 사이즈와 src 사이즈가 달라서 TransparentBlt 가 자동 축소함)
			SelectObject(hdcCropMem, hCropBmp);
			TransparentBlt(hdc,
				screenX, screenY, 32, 32,            // 화면 32x32 dest
				hdcCropMem, 0, 0, cropW, cropH,      // 원본 cropW x cropH src
				RGB(255, 0, 255));
		}
	}

	DeleteDC(hdcCropMem);
}

// ---------- 아이템(=도구) → 아이콘 비트맵 헬퍼 ----------
static HBITMAP GetToolIconBitmap(ToolType t) {
	if (t <= TOOL_NONE || t >= ITEM_COUNT) return NULL;
	// 아이콘 배열에 등록된 게 있으면 우선 사용
	if (g_hBitmap_item[t] != NULL) return g_hBitmap_item[t];
	// 폴백: 기존 핸들들
	switch (t) {
	case TOOL_HOE:   return g_hBitmap_icon_hoe;
	case TOOL_WATER: return g_hBitmap_icon_water;
	case TOOL_POLE:  return g_hBitmap_icon_pole;
	default: return NULL;
	}
}

// ---------- 자동 적재 (스태킹) ----------
// 1순위: 이미 같은 type 슬롯이 있으면 count++
// 2순위: 빈칸 찾아서 type 대입 + count=1
//
// 검색 범위:
//   - 가방(bag) 16칸 → 퀵슬롯(quick) 4칸 순으로 검사.
// 낚시 성공/작물 수확/벌목 등에서 호출.
void AddItemToBag(ItemType item) {
	if (item == ITEM_NONE) return;

	// 1순위 (중복 탐색) — 가방 먼저
	for (int i = 0; i < INV4_COUNT; i++) {
		if (bagSlots[i] == item) {
			g_bagCount[i]++;
			return;
		}
	}
	for (int i = 0; i < QUICKSLOT_COUNT; i++) {
		if (quickSlots[i] == item) {
			g_quickCount[i]++;
			return;
		}
	}

	// 2순위 (빈칸 탐색) — 가방 우선
	for (int i = 0; i < INV4_COUNT; i++) {
		if (bagSlots[i] == ITEM_NONE) {
			bagSlots[i] = item;
			g_bagCount[i] = 1;
			return;
		}
	}
	for (int i = 0; i < QUICKSLOT_COUNT; i++) {
		if (quickSlots[i] == ITEM_NONE) {
			quickSlots[i] = item;
			g_quickCount[i] = 1;
			return;
		}
	}

	OutputDebugString(TEXT("[Inventory] 가방이 가득 차 아이템을 넣을 수 없음\n"));
}

// ---------- 드래그 앤 드롭 ----------
// 드래그 시작: 슬롯에 아이템이 있으면 들고 있는 상태로
static void BeginDrag(int slotIdx, bool fromQuick) {
	if (slotIdx < 0) return;
	ItemType item = fromQuick ? quickSlots[slotIdx] : bagSlots[slotIdx];
	if (item == ITEM_NONE) return;   // 빈 슬롯은 드래그 시작 안 함
	g_draggedSlotIndex = slotIdx;
	g_isDraggingFromQuick = fromQuick;
	g_draggedItem = item;
}

// 드래그 취소 (원위치)
static void CancelDrag() {
	g_draggedSlotIndex = -1;
	g_isDraggingFromQuick = false;
	g_draggedItem = ITEM_NONE;
}

// 드래그 끝: 놓은 슬롯과 시작 슬롯의 아이템 swap
// dropOnQuick=true 면 퀵슬롯, false 면 가방
static void EndDrag(int dropSlotIdx, bool dropOnQuick) {
	if (g_draggedSlotIndex < 0) return;
	if (dropSlotIdx < 0) {                      // 빈 곳에 놓음 → 원위치 (그냥 취소)
		CancelDrag();
		return;
	}
	// 같은 슬롯에 놓음 → 취소
	if (dropOnQuick == g_isDraggingFromQuick && dropSlotIdx == g_draggedSlotIndex) {
		CancelDrag();
		return;
	}
	// 두 슬롯의 (type + count) 묶음을 swap
	InventorySlot src = g_isDraggingFromQuick ? GetQuickInv(g_draggedSlotIndex) : GetBagInv(g_draggedSlotIndex);
	InventorySlot dst = dropOnQuick ? GetQuickInv(dropSlotIdx) : GetBagInv(dropSlotIdx);

	if (g_isDraggingFromQuick) SetQuickInv(g_draggedSlotIndex, dst);
	else                       SetBagInv(g_draggedSlotIndex, dst);
	if (dropOnQuick)           SetQuickInv(dropSlotIdx, src);
	else                       SetBagInv(dropSlotIdx, src);

	CancelDrag();
}

// 드래그 중인 아이템을 마우스 위치에 그리기 (UI 최상단)
static void DrawDraggedItem(HDC hDC) {
	if (g_draggedSlotIndex < 0 || g_draggedItem == ITEM_NONE) return;
	HBITMAP src = GetToolIconBitmap(g_draggedItem);
	if (src == NULL) return;
	HDC memDC = CreateCompatibleDC(hDC);
	HBITMAP oldB = (HBITMAP)SelectObject(memDC, src);
	// 마우스 커서가 아이콘의 정중앙에 오도록
	int dx = g_dragMouseX - QUICKSLOT_ICON_SIZE / 2;
	int dy = g_dragMouseY - QUICKSLOT_ICON_SIZE / 2;
	TransparentBlt(hDC, dx, dy, QUICKSLOT_ICON_SIZE, QUICKSLOT_ICON_SIZE,
		memDC, 0, 0, QUICKSLOT_ICON_SIZE, QUICKSLOT_ICON_SIZE,
		EXT_TRANSPARENT);
	SelectObject(memDC, oldB);
	DeleteDC(memDC);
}

// 퀵슬롯 i번 슬롯의 화면 사각형 계산
// 슬롯 좌상단 = (QUICKSLOT_BG_X + OFFSET + i*STEP, ...)
// 슬롯 정중앙에 48 아이콘 배치하려면 (SLOT_BOX - ICON)/2 만큼 더 들어감.
static void GetQuickSlotRect(int i, int* outX, int* outY) {
	int slotX = QUICKSLOT_BG_X + QUICK_START_X_OFFSET + i * QUICK_SLOT_STEP;
	int slotY = QUICKSLOT_BG_Y + QUICK_START_Y_OFFSET;
	int cx = (QUICK_SLOT_BOX_SIZE - QUICKSLOT_ICON_SIZE) / 2;
	int cy = (QUICK_SLOT_BOX_SIZE - QUICKSLOT_ICON_SIZE) / 2;
	if (outX) *outX = slotX + cx;
	if (outY) *outY = slotY + cy;
}

// 4x4 i번 셀(0..15) 의 화면 사각형 계산
// row = i / 4, col = i % 4
// 슬롯 좌상단 = (INV4_BG_X + OFFSET + col*STEP, ...)
// 정중앙 보정: (SLOT_BOX - ICON)/2
static void GetInv4CellRect(int i, int* outX, int* outY) {
	int row = i / 4;
	int col = i % 4;
	int slotX = INV4_BG_X + BAG_START_X_OFFSET + col * BAG_SLOT_STEP;
	int slotY = INV4_BG_Y + BAG_START_Y_OFFSET + row * BAG_SLOT_STEP;
	int cx = (BAG_SLOT_BOX_SIZE - QUICKSLOT_ICON_SIZE) / 2;
	int cy = (BAG_SLOT_BOX_SIZE - QUICKSLOT_ICON_SIZE) / 2;
	if (outX) *outX = slotX + cx;
	if (outY) *outY = slotY + cy;
}


// 상점 씬 전용: 인벤토리 i번 셀의 화면 좌표 (SHOP_INV_BG_X/Y 기준)
static void GetShopInvCellRect(int i, int* outX, int* outY) {
	int row = i / 4;
	int col = i % 4;
	int slotX = SHOP_INV_BG_X + BAG_START_X_OFFSET + col * BAG_SLOT_STEP;
	int slotY = SHOP_INV_BG_Y + BAG_START_Y_OFFSET + row * BAG_SLOT_STEP;
	int cx = (BAG_SLOT_BOX_SIZE - QUICKSLOT_ICON_SIZE) / 2;
	int cy = (BAG_SLOT_BOX_SIZE - QUICKSLOT_ICON_SIZE) / 2;
	if (outX) *outX = slotX + cx;
	if (outY) *outY = slotY + cy;
}

// 상점 씬 전용: 마우스가 어느 인벤토리 슬롯 위인지 (-1이면 없음)
static int HitShopInvCell(int mx, int my) {
	for (int i = 0; i < INV4_COUNT; i++) {
		int ix, iy;
		GetShopInvCellRect(i, &ix, &iy);
		if (mx >= ix && mx < ix + QUICKSLOT_ICON_SIZE &&
			my >= iy && my < iy + QUICKSLOT_ICON_SIZE) return i;
	}
	return -1;
}

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
		// 드래그 시작한 슬롯은 빈 것처럼 비워 둠 (드래그 중인 아이콘이 마우스에 따라가게)
		if (g_draggedSlotIndex == i && g_isDraggingFromQuick) continue;
		HBITMAP src = GetToolIconBitmap(quickSlots[i]);
		if (src != NULL) {
			HBITMAP oldB = (HBITMAP)SelectObject(memDC, src);
			TransparentBlt(hDC, ix, iy, QUICKSLOT_ICON_SIZE, QUICKSLOT_ICON_SIZE,
				memDC, 0, 0, QUICKSLOT_ICON_SIZE, QUICKSLOT_ICON_SIZE,
				EXT_TRANSPARENT);
			SelectObject(memDC, oldB);
		}
		// 개수 표시 (count > 1) — 아이콘 우측 상단에 노란색 숫자
		if (src != NULL && g_quickCount[i] > 1) {
			SetBkMode(hDC, TRANSPARENT);
			SetTextColor(hDC, RGB(255, 240, 80));
			wchar_t buf[16];
			wsprintfW(buf, L"%d", g_quickCount[i]);
			int len = (int)wcslen(buf);
			// 우측 상단 정렬: 아이콘 오른쪽 끝에서 글자 너비만큼 안으로
			int tx = ix + QUICKSLOT_ICON_SIZE - 4 - len * 8;
			int ty = iy + 2;
			TextOut(hDC, tx, ty, buf, len);
		}
		// 선택된 슬롯은 동일 아이콘을 한 번 더 살짝 옆으로 겹쳐 그려 '하이라이트' 표현
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
	if (g_currentScene == SCENE_SHOP) return; // 상점에서는 별도 함수로 그림
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
		// 드래그 시작한 셀은 빈 것처럼
		if (g_draggedSlotIndex == i && !g_isDraggingFromQuick) continue;
		HBITMAP src = GetToolIconBitmap(bagSlots[i]);
		if (src != NULL) {
			HBITMAP oldB = (HBITMAP)SelectObject(memDC, src);
			TransparentBlt(hDC, ix, iy, QUICKSLOT_ICON_SIZE, QUICKSLOT_ICON_SIZE,
				memDC, 0, 0, QUICKSLOT_ICON_SIZE, QUICKSLOT_ICON_SIZE,
				EXT_TRANSPARENT);
			SelectObject(memDC, oldB);
		}
		// 개수 표시 (count > 1) — 아이콘 우측 상단 노란색 숫자
		if (src != NULL && g_bagCount[i] > 1) {
			SetBkMode(hDC, TRANSPARENT);
			SetTextColor(hDC, RGB(255, 240, 80));
			wchar_t buf[16];
			wsprintfW(buf, L"%d", g_bagCount[i]);
			int len = (int)wcslen(buf);
			int tx = ix + QUICKSLOT_ICON_SIZE - 4 - len * 8;
			int ty = iy + 2;
			TextOut(hDC, tx, ty, buf, len);
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

// ---------- 상점 전용 인벤토리 패널 (항상 표시, SHOP_INV_BG 위치에 고정) ----------
void DrawShopInventoryPanel(HDC hDC) {
	HDC memDC = CreateCompatibleDC(hDC);

	// 배경 패널 (일반 인벤토리와 동일한 비트맵, 상점 위치에)
	if (g_hBitmap_inv_main != NULL) {
		HBITMAP oldB = (HBITMAP)SelectObject(memDC, g_hBitmap_inv_main);
		TransparentBlt(hDC, SHOP_INV_BG_X, SHOP_INV_BG_Y, SHOP_INV_BG_W, SHOP_INV_BG_H,
			memDC, 0, 0, INV4_BG_W, INV4_BG_H, EXT_TRANSPARENT);
		SelectObject(memDC, oldB);
	}

	// 16칸 아이템
	for (int i = 0; i < INV4_COUNT; i++) {
		int ix, iy;
		GetShopInvCellRect(i, &ix, &iy);
		HBITMAP src = GetToolIconBitmap(bagSlots[i]);
		if (src != NULL) {
			HBITMAP oldB = (HBITMAP)SelectObject(memDC, src);
			TransparentBlt(hDC, ix, iy, QUICKSLOT_ICON_SIZE, QUICKSLOT_ICON_SIZE,
				memDC, 0, 0, QUICKSLOT_ICON_SIZE, QUICKSLOT_ICON_SIZE, EXT_TRANSPARENT);
			SelectObject(memDC, oldB);
		}
		// 개수 표시
		if (src != NULL && g_bagCount[i] > 1) {
			SetBkMode(hDC, TRANSPARENT);
			SetTextColor(hDC, RGB(255, 240, 80));
			wchar_t buf[16];
			wsprintfW(buf, L"%d", g_bagCount[i]);
			int len = (int)wcslen(buf);
			int tx = ix + QUICKSLOT_ICON_SIZE - 4 - len * 8;
			int ty = iy + 2;
			TextOut(hDC, tx, ty, buf, len);
		}
		// 호버 슬롯 하이라이트 (노란 테두리)
		if (g_shopInvHoverSlot == i && bagSlots[i] != ITEM_NONE) {
			HBRUSH hlBrush = CreateSolidBrush(RGB(255, 220, 0));
			RECT hlRect = { ix - 2, iy - 2, ix + QUICKSLOT_ICON_SIZE + 2, iy + QUICKSLOT_ICON_SIZE + 2 };
			FrameRect(hDC, &hlRect, hlBrush);
			DeleteObject(hlBrush);
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
	// [농사 상호작용은 SCENE_FARM 에서만]
	if (g_currentScene != SCENE_FARM) return;

	// ===== 그리드 스냅 (사용자 요청 공식) =====
	// 마우스 화면 좌표 → 월드 좌표 → 32배수로 정규화 → 타일 인덱스
	int worldX = mx + cameraX;
	int worldY = my + cameraY;
	int tileX = (worldX / TILE_SIZE) * TILE_SIZE;   // 32 의 배수로 스냅된 월드 X
	int tileY = (worldY / TILE_SIZE) * TILE_SIZE;   // 32 의 배수로 스냅된 월드 Y
	int gridX = tileX / TILE_SIZE;                  // 배열 인덱스
	int gridY = tileY / TILE_SIZE;
	if (gridX < 0 || gridX >= MAP_TILE_W) return;
	if (gridY < 0 || gridY >= MAP_TILE_H) return;

	// 플레이어 주변 거리(1~2칸)
	int playerGX = (g_player.x + g_player.w / 2) / TILE_SIZE;
	int playerGY = (g_player.y + g_player.h / 2) / TILE_SIZE;
	int dist = TileChebyshevDist(gridX, gridY, playerGX, playerGY);
	if (dist > 2) return;

	// 1) 호미: 일반 땅(0) → 경작(1)
	if (g_currentTool == TOOL_HOE && farmState[gridY][gridX] == 0) {
		// 흙길/절벽/집 등 금지 영역에 걸치면 거부 (스냅된 tileX/tileY 기준)
		for (int i = 0; i < CLIFF_RECT_COUNT; i++) {
			RECT& r = g_cliffRects[i];
			if (RectOverlap(tileX, tileY, TILE_SIZE, TILE_SIZE,
				r.left, r.top, r.right - r.left, r.bottom - r.top)) return;
		}
		for (int i = 0; i < NO_TREE_RECT_COUNT; i++) {
			RECT& r = g_noTreeRects[i];
			if (RectOverlap(tileX, tileY, TILE_SIZE, TILE_SIZE,
				r.left, r.top, r.right - r.left, r.bottom - r.top)) return;
		}
		if (RectOverlap(tileX, tileY, TILE_SIZE, TILE_SIZE,
			g_house.x + HOUSE_BLOCK_PAD_L,
			g_house.y + HOUSE_BLOCK_PAD_T,
			HOUSE_DRAW_W - HOUSE_BLOCK_PAD_L - HOUSE_BLOCK_PAD_R,
			HOUSE_DRAW_H - HOUSE_BLOCK_PAD_T - HOUSE_BLOCK_PAD_B)) return;
		farmState[gridY][gridX] = 1;
		return;
	}

	// 2) 물뿌리개: 경작된 땅(1) → 물 준 땅(2)
	if (g_currentTool == TOOL_WATER && farmState[gridY][gridX] == 1) {
		farmState[gridY][gridX] = 2;
		return;
	}

	// 3) 씨앗 심기: 물 준 땅(2) + 아직 작물 없음 + 씨앗 들고 있음
	int seedCrop = SeedToCrop(g_currentTool);
	if (seedCrop != CROP_NONE && farmState[gridY][gridX] == 2 && g_cropType[gridY][gridX] == CROP_NONE) {
		// 씨앗 1개 소모 — 현재 선택된 퀵슬롯에서 차감
		if (g_selectedQuickSlot >= 0 && quickSlots[g_selectedQuickSlot] == g_currentTool) {
			g_quickCount[g_selectedQuickSlot]--;
			if (g_quickCount[g_selectedQuickSlot] <= 0) {
				quickSlots[g_selectedQuickSlot] = ITEM_NONE;
				g_quickCount[g_selectedQuickSlot] = 0;
				g_currentTool = ITEM_NONE;
			}
		}
		g_cropType[gridY][gridX] = seedCrop;
		g_growStage[gridY][gridX] = 0;        // 0: seed
		g_plantedDay[gridY][gridX] = g_gameDay;
		return;
	}

	// 4) 수확: growStage == 3 (ready) 인 작물 클릭 시 수확 + 드롭
	if (g_currentTool == TOOL_HOE && g_cropType[gridY][gridX] != CROP_NONE && g_growStage[gridY][gridX] >= 3) {
		ItemType harvestItem = CropToHarvestItem(g_cropType[gridY][gridX]);

		// 작물 드롭 (물고기 드롭과 동일한 로직)
		DroppedItem newItem;
		newItem.worldX = tileX + TILE_SIZE / 2;
		newItem.worldY = tileY + TILE_SIZE;
		newItem.type = harvestItem;
		newItem.active = true;
		newItem.floatTimer = 0;
		newItem.graceTick = 0;
		g_droppedItems.push_back(newItem);

		g_cropType[gridY][gridX] = CROP_NONE;
		g_growStage[gridY][gridX] = 0;
		farmState[gridY][gridX] = 1;
		return;
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
		// HP 감소 + 흔들림 시작
		t.hp--;
		t.shakeTimer = TREE_SHAKE_TICKS;
		if (t.hp <= 0) {
			t.isAlive = false;
			// 통나무 드롭 (나무 중심 위치)
			DroppedItem newLog;
			newLog.worldX = t.x + t.w / 2;
			newLog.worldY = t.y + t.h;
			newLog.type = ITEM_LOG;
			newLog.active = true;
			newLog.floatTimer = 0;
			newLog.graceTick = 0;
			g_droppedItems.push_back(newLog);
		}
		return; // 한 번 클릭에 한 그루만
	}
}

// 낚시터 나무 벌목 시도
// 낚시터 타일맵 
// 타일 1개 = 32x32px (농장 맵과 동일, 월드 좌표 기준)
// 맵 40x40타일 = 1280x1280px, 화면 800x800으로 BitBlt + cameraX/Y
// 0: 이동 가능
// 1: 이동 불가
// 2: 낚시 가능 (아래, DIR_DOWN)
// 3: 낚시 가능 (왼쪽, DIR_LEFT)
// 4: 낚시 가능 (오른쪽, DIR_RIGHT)
// 5: 낚시 가능 (왼쪽+아래, DIR_LEFT & DIR_DOWN)
// 6: 낚시 가능 (오른쪽+아래, DIR_RIGHT & DIR_DOWN)
// 7: 농장으로 이동 트리거
// 8 - Tree1 9 - Tree2 10 - Tree3 나무들의 밑둥. 이동 불가능. 두 칸이 하나의 나무 밑둥.

// 낚시터 나무 HP 관리 (타일 위치 row,col 기준 — 왼쪽 밑둥 칸 기준)
struct FishingTreeState {
	int row, col;     // 타일맵에서 나무 밑둥 왼쪽 칸 위치
	int hp;           // 현재 HP
	int shakeTimer;   // 흔들림 카운터
};
static std::vector<FishingTreeState> g_fishingTreeStates;
#define FISHING_MAP_COLS 40
#define FISHING_MAP_ROWS 40
#define FISHING_TILE_SCREEN TILE_SIZE  // 낚시 맵 타일 크기 = 농장과 동일 (32px, 월드 좌표 기준)

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
	{ 1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 },
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
	{ 8,  0, 118, 140, 118, 140 }, // Tree1: 74*1.6=118, 88*1.6=140
	{ 9,  1,  56,  72,  56,  72 }, // Tree2: 35*1.6=56,  45*1.6=72
	{ 10, 2,  50,  62,  51,  62 }, // Tree3: 32*1.6=51,  39*1.6=62
};

// 낚시터 나무 벌목 시도 (g_treeInfos, g_fishingTileMap, g_fishingTreeStates 모두 정의 후에 위치)
static void TryChopFishingTree(int mx, int my) {
	if (g_currentTool != TOOL_AXE) return;
	int worldX = mx + cameraX;
	int worldY = my + cameraY;

	int playerCX = g_player.x + g_player.w / 2;
	int playerCY = g_player.y + g_player.h / 2;

	for (size_t i = 0; i < g_fishingTreeStates.size(); i++) {
		FishingTreeState& fs = g_fishingTreeStates[i];
		if (fs.hp <= 0) continue;

		int tileX = fs.col * FISHING_TILE_SCREEN;
		int treeAreaW = FISHING_TILE_SCREEN * 2;
		int tile = g_fishingTileMap[fs.row][fs.col];
		int tIdx = tile - 8; // 0=Tree1, 1=Tree2, 2=Tree3
		if (tIdx < 0 || tIdx > 2) continue;

		int dstW = g_treeInfos[tIdx].dstW;
		int dstH = g_treeInfos[tIdx].dstH;
		int treeX = tileX + (treeAreaW - dstW) / 2;
		int treeY = (fs.row + 1) * FISHING_TILE_SCREEN - dstH;

		// 클릭이 나무 영역 안인지
		if (worldX < treeX || worldX >= treeX + dstW) continue;
		if (worldY < treeY || worldY >= treeY + dstH) continue;

		// 플레이어 거리 제한
		int treeCX = tileX + treeAreaW / 2;
		int treeCY = (fs.row + 1) * FISHING_TILE_SCREEN;
		int dx = playerCX - treeCX;
		int dy = playerCY - treeCY;
		if (dx * dx + dy * dy > 120 * 120) return;

		// HP 감소 + 흔들림
		fs.hp--;
		fs.shakeTimer = TREE_SHAKE_TICKS;

		if (fs.hp <= 0) {
			// 타일맵 두 칸을 0으로 → 지나갈 수 있게
			g_fishingTileMap[fs.row][fs.col] = 0;
			g_fishingTileMap[fs.row][fs.col + 1] = 0;

			// 통나무 드롭
			DroppedItem newLog;
			newLog.worldX = treeCX;
			newLog.worldY = treeCY;
			newLog.type = ITEM_LOG;
			newLog.active = true;
			newLog.floatTimer = 0;
			newLog.graceTick = 0;
			g_droppedItems.push_back(newLog);
		}
		return;
	}
}

// ---------- 바닥에 떨어진 물고기 그리기 ----------
static void DrawDroppedFish(HDC hDC) {
	for (size_t i = 0; i < g_droppedFishes.size(); i++) {
		DroppedFish& fish = g_droppedFishes[i];
		if (!fish.active) continue;
		if (fish.type == FISH_ITEM_NONE) continue;
		HBITMAP src = g_hBitmap_fishItem[fish.type];
		if (src == NULL) continue;

		int ft = fish.floatTimer;
		int floatOff = 0;
		if (ft < 10)       floatOff = -(ft * DROPPED_FISH_FLOAT_RANGE / 10);
		else if (ft < 20)  floatOff = -((19 - ft) * DROPPED_FISH_FLOAT_RANGE / 10);
		else if (ft < 30)  floatOff = (ft - 20) * DROPPED_FISH_FLOAT_RANGE / 10;
		else               floatOff = (39 - ft) * DROPPED_FISH_FLOAT_RANGE / 10;

		int sx = fish.worldX - FISH_ITEM_DRAW_W / 2 - cameraX;
		int sy = fish.worldY - FISH_ITEM_DRAW_H / 2 + floatOff - cameraY;

		HDC memDC = CreateCompatibleDC(hDC);
		HBITMAP oldB = (HBITMAP)SelectObject(memDC, src);
		TransparentBlt(hDC, sx, sy, FISH_ITEM_DRAW_W, FISH_ITEM_DRAW_H,
			memDC, 0, 0, FISH_ITEM_DRAW_W, FISH_ITEM_DRAW_H, RGB(255, 0, 255));
		SelectObject(memDC, oldB);
		DeleteDC(memDC);
	}
}

// ---------- 바닥에 떨어진 통나무/작물 그리기 (물고기 드롭과 동일 로직) ----------
static void DrawDroppedItem(HDC hDC) {
	for (size_t i = 0; i < g_droppedItems.size(); i++) {
		DroppedItem& item = g_droppedItems[i];
		if (!item.active) continue;
		if (item.type == ITEM_NONE) continue;
		if (item.type >= ITEM_COUNT) continue;
		HBITMAP src = g_hBitmap_item[item.type];
		if (src == NULL) continue;

		int ft = item.floatTimer;
		int floatOff = 0;
		if (ft < 10)       floatOff = -(ft * DROPPED_ITEM_FLOAT_RANGE / 10);
		else if (ft < 20)  floatOff = -((19 - ft) * DROPPED_ITEM_FLOAT_RANGE / 10);
		else if (ft < 30)  floatOff = (ft - 20) * DROPPED_ITEM_FLOAT_RANGE / 10;
		else               floatOff = (39 - ft) * DROPPED_ITEM_FLOAT_RANGE / 10;

		int sx = item.worldX - DROPPED_ITEM_DRAW_W / 2 - cameraX;
		int sy = item.worldY - DROPPED_ITEM_DRAW_H / 2 + floatOff - cameraY;

		HDC memDC = CreateCompatibleDC(hDC);
		HBITMAP oldB = (HBITMAP)SelectObject(memDC, src);
		TransparentBlt(hDC, sx, sy, DROPPED_ITEM_DRAW_W, DROPPED_ITEM_DRAW_H,
			memDC, 0, 0, DROPPED_ITEM_DRAW_W, DROPPED_ITEM_DRAW_H, EXT_TRANSPARENT);
		SelectObject(memDC, oldB);
		DeleteDC(memDC);
	}
}
// drawBelow == false : 플레이어 발보다 위에 뿌리가 있는 나무  (플레이어 뒤 → 플레이어가 나무를 가림)
static void DrawFishingTrees(HDC hDC, HBITMAP hBitmap_fishTree[3], int playerFootY, bool drawBelow) {
	HDC memDC = CreateCompatibleDC(hDC);
	int r, c, t;
	for (r = 0; r < FISHING_MAP_ROWS; r++) {
		// 나무 뿌리의 월드 Y = 타일 행 아랫변
		int treeFootY = (r + 1) * FISHING_TILE_SCREEN;

		if (drawBelow) {
			if (treeFootY <= playerFootY) continue;
		}
		else {
			if (treeFootY > playerFootY) continue;
		}

		// 화면 밖 행 컬링
		int screenFootY = treeFootY - cameraY;
		if (screenFootY < -200 || screenFootY > CLIENT_H + 200) continue;

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
				// 중앙 정렬 + 카메라 오프셋 적용
				int treeAreaW = FISHING_TILE_SCREEN * 2;
				int treeX = c * FISHING_TILE_SCREEN + (treeAreaW - g_treeInfos[t].dstW) / 2 - cameraX;
				int treeY = treeFootY - g_treeInfos[t].dstH - cameraY;

				// 흔들림 오프셋
				int shakeOff = 0;
				for (size_t si = 0; si < g_fishingTreeStates.size(); si++) {
					if (g_fishingTreeStates[si].row == r && g_fishingTreeStates[si].col == c) {
						if (g_fishingTreeStates[si].shakeTimer > 0) {
							if (g_fishingTreeStates[si].shakeTimer % 2 == 1) shakeOff = TREE_SHAKE_OFFX;
							else                                              shakeOff = -TREE_SHAKE_OFFX;
						}
						break;
					}
				}
				treeX += shakeOff;

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
// ---------- farming 도구 애니메이션 ----------
// farming_arm: 0~4 앞 호미, 5~9 옆 호미, 10~14 앞 도끼, 15~19 옆 도끼, 20~21 앞 물뿌리개, 22~23 옆 물뿌리개
// 시퀀스: 01234 (호미/도끼), 20 21 21 / 22 23 23 (물뿌리개)
// 뒷면은 앞면 소스를 플레이어 몸 뒤에 그림
static bool  g_isFarmingAnim = false; // 도구 애니메이션 진행 중
static int   g_farmArmIdx = 0;     // 현재 farming_arm 인덱스
static int   g_farmArmTimer = 0;     // 프레임 전환 카운터
static int   g_farmArmSeqPos = 0;     // 시퀀스 내 현재 위치
#define FARM_ARM_TICKS_PER_FRAME  5     // 프레임 하나당 유지 틱 수

// 도구별 시퀀스 (앞면/옆면 각각)
static int g_farmSeqHoeFront[5] = { 0,  1,  2,  3,  4 };
static int g_farmSeqHoeSide[5] = { 5,  6,  7,  8,  9 };
static int g_farmSeqAxeFront[5] = { 10, 11, 12, 13, 14 };
static int g_farmSeqAxeSide[5] = { 15, 16, 17, 18, 19 };
static int g_farmSeqWaterFront[3] = { 20, 21, 21 };
static int g_farmSeqWaterSide[3] = { 22, 23, 23 };
static int  g_farmSeqLen = 0;    // 현재 시퀀스 길이
static int* g_farmSeqPtr = NULL; // 현재 시퀀스 포인터

void UpdatePlayer() {
	// farming 애니메이션 중에는 이동 불가
	if (g_isFarmingAnim) return;

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

			// 안전장치: 이미 절벽/집 안에 끼어있으면 충돌 무시 (탈출 허용)
			bool alreadyInside = IsBlockedByCliff(
				g_player.x + footOffX, g_player.y + footOffY, footW, footH);

			int nextX = g_player.x + dx;
			if (!alreadyInside && (
				IsBlockedByCliff(nextX + footOffX, g_player.y + footOffY, footW, footH) ||
				IsBlockedByHouse(nextX + footOffX, g_player.y + footOffY, footW, footH))) {
				nextX = g_player.x;
				dx = 0;
			}
			int nextY = g_player.y + dy;
			if (!alreadyInside && (
				IsBlockedByCliff(nextX + footOffX, nextY + footOffY, footW, footH) ||
				IsBlockedByHouse(nextX + footOffX, nextY + footOffY, footW, footH))) {
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

	// 맵 경계 클램프 (농장/낚시 모두 1280x1280 월드 좌표 기준)
	{
		if (g_player.x < 0) g_player.x = 0;
		if (g_player.y < 0) g_player.y = 0;
		if (g_player.x + g_player.w > MAP_PIXEL_W) g_player.x = MAP_PIXEL_W - g_player.w;
		if (g_player.y + g_player.h > MAP_PIXEL_H) g_player.y = MAP_PIXEL_H - g_player.h;
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
		// 낚시터 출입구(타일 7, 행4~6 열0) 바로 오른쪽 — 월드 좌표 기준
		g_player.x = 1 * FISHING_TILE_SCREEN;
		g_player.y = 5 * FISHING_TILE_SCREEN - g_player.h / 2;
		g_player.dir = DIR_RIGHT;
		cameraX = 0;
		cameraY = 0;
		g_sceneCooldown = 30;
	}

	// [확장] 친구 맵 포탈 트리거: 우측 상단 흙길 끝 (worldX > 1200)
	// 메시지 박스 없이 즉시 SCENE_FISHING(친구 낚시 맵)으로 워프.
	if (g_currentScene == SCENE_FARM && g_sceneCooldown == 0) {
		int footCX = g_player.x + g_player.w / 2;
		int footCY = g_player.y + g_player.h / 2;
		if (footCX >= PORTAL_X_MIN && footCX <= PORTAL_X_MAX &&
			footCY >= PORTAL_Y_MIN && footCY <= PORTAL_Y_MAX) {
			OutputDebugString(TEXT("[PORTAL] 친구 맵으로 즉시 워프\n"));
			g_currentScene = SCENE_FISHING;   // = SCENE_FISHING_MAP (친구 낚시 맵)
			UpdateCamera();                    // 친구 맵 진입 즉시 플레이어 중앙으로 카메라 정렬
			g_player.x = 100;
			g_player.y = 600;
			g_player.dir = DIR_RIGHT;
			g_sceneCooldown = 30;
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
	// 배경을 마젠타로 채워야 TransparentBlt가 올바르게 투명 처리함
	HBRUSH magentaBrush = CreateSolidBrush(RGB(255, 0, 255));
	RECT fillRect = { 0, 0, srcW, srcH };
	FillRect(flipDC, &fillRect, magentaBrush);
	DeleteObject(magentaBrush);
	SelectObject(memDC, bmp);
	StretchBlt(flipDC, srcW - 1, 0, -srcW, srcH, memDC, 0, 0, srcW, srcH, SRCCOPY);
	TransparentBlt(hDC, dstX, dstY, dstW, dstH, flipDC, 0, 0, srcW, srcH, RGB(255, 0, 255));
	SelectObject(flipDC, flipOld);
	DeleteObject(flipBmp);
	DeleteDC(flipDC);
}

void DrawPlayer(HDC hDC) {
	// 월드 좌표 → 화면 좌표 변환
	int x = g_player.x - cameraX;
	int y = g_player.y - cameraY;

	// 방향별 걷기 시퀀스
	static int animSeqFront[4] = { 0, 1, 0, 2 };
	static int animSeqSide[4] = { 3, 4, 3, 5 };
	static int animSeqBack[4] = { 6, 8, 6, 7 };

	// 몸 인덱스 결정
	int baseIdx = 0;
	if (g_fishingPhase != FISHING_PHASE_NONE || g_isFarmingAnim) {
		// 낚시/farming 중: 정지 프레임 고정 (앞면=0, 옆면=3, 후면=6)
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
	int dispFArmFrontW = 16 * 2; int dispFArmFrontH = 41 * 2; // 앞면 낚시팔 
	int dispFArmSideW = 44 * 2; int dispFArmSideH = 33 * 2; // 옆면 낚시팔 

	bool isFishingState = (g_fishingPhase != FISHING_PHASE_NONE);

	// farming 팔 오프셋/크기 (참고함수 10배 기준 → 2배 환산)
	// 호미/도끼 앞: (-80,-60) → (-16,-12), 32x37 → 64x74
	// 호미/도끼 옆: (-70,-60) → (-14,-12), 32x37 → 64x74
	// 물뿌리개: (-20,-100) → (-4,-20), 20x27 → 40x54
	int farmArmHoeOffX_front = -16; int farmArmHoeOffY_front = -12;
	int farmArmHoeOffX_side = -14; int farmArmHoeOffY_side = -12;
	int farmArmWatOffX = -4; int farmArmWatOffY = -20;

	if (g_player.dir == DIR_LEFT) {
		// 몸 (좌우반전) 먼저
		DrawFlipped(hDC, memDC, g_player.base[baseIdx],
			x, y, dispBodyW, dispBodyH, 16, 28);

		if (isFishingState) {
			if (g_player.fishing_arm[g_fishingArmIdx] != NULL) {
				DrawFlipped(hDC, memDC, g_player.fishing_arm[g_fishingArmIdx],
					x - 30, y - 24, dispFArmSideW, dispFArmSideH, 44, 33);
			}
		}
		else if (g_isFarmingAnim) {
			// 호미/도끼/물뿌리개 옆면 (좌우반전) — 몸 다음에 그려서 몸 앞에 나오게
			if (g_currentTool == TOOL_HOE || g_currentTool == TOOL_AXE) {
				if (g_player.farming_arm[g_farmArmIdx] != NULL) {
					DrawFlipped(hDC, memDC, g_player.farming_arm[g_farmArmIdx],
						x + farmArmHoeOffX_side, y + farmArmHoeOffY_side, 64, 74, 32, 37);
				}
			}
			else if (g_currentTool == TOOL_WATER) {
				if (g_player.farming_arm[g_farmArmIdx] != NULL) {
					DrawFlipped(hDC, memDC, g_player.farming_arm[g_farmArmIdx],
						x + farmArmWatOffX, y + farmArmWatOffY, 40, 54, 20, 27);
				}
			}
		}
		else {
			// 일반 걷기 팔 (좌우반전)
			if (g_player.base_arm[baseIdx] != NULL) {
				DrawFlipped(hDC, memDC, g_player.base_arm[baseIdx],
					x, y + armOffY, dispArmW, dispArmH, 16, 12);
			}
		}
	}
	else {
		// 뒷면: 앞면 소스를 몸 뒤(먼저)에 그림
		if (g_player.dir == DIR_UP && g_isFarmingAnim) {
			if (g_currentTool == TOOL_HOE || g_currentTool == TOOL_AXE) {
				if (g_player.farming_arm[g_farmArmIdx] != NULL) {
					SelectObject(memDC, g_player.farming_arm[g_farmArmIdx]);
					TransparentBlt(hDC, x + farmArmHoeOffX_front, y + farmArmHoeOffY_front, 64, 74,
						memDC, 0, 0, 32, 37, RGB(255, 0, 255));
				}
			}
			else if (g_currentTool == TOOL_WATER) {
				if (g_player.farming_arm[g_farmArmIdx] != NULL) {
					SelectObject(memDC, g_player.farming_arm[g_farmArmIdx]);
					TransparentBlt(hDC, x + farmArmWatOffX, y + farmArmWatOffY, 40, 54,
						memDC, 0, 0, 20, 27, RGB(255, 0, 255));
				}
			}
		}

		// 몸
		SelectObject(memDC, g_player.base[baseIdx]);
		TransparentBlt(hDC, x, y, dispBodyW, dispBodyH,
			memDC, 0, 0, 16, 28, RGB(255, 0, 255));

		if (isFishingState) {
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
		else if (g_isFarmingAnim) {
			// farming 팔 — DIR_DOWN: 앞면(몸 앞), DIR_RIGHT: 옆면(몸 앞)
			// DIR_UP는 이미 몸 그리기 전에 처리했으므로 여기서 제외
			if (g_player.dir == DIR_DOWN) {
				if (g_player.farming_arm[g_farmArmIdx] != NULL) {
					SelectObject(memDC, g_player.farming_arm[g_farmArmIdx]);
					if (g_currentTool == TOOL_HOE || g_currentTool == TOOL_AXE) {
						TransparentBlt(hDC, x + farmArmHoeOffX_front, y + farmArmHoeOffY_front, 64, 74,
							memDC, 0, 0, 32, 37, RGB(255, 0, 255));
					}
					else if (g_currentTool == TOOL_WATER) {
						TransparentBlt(hDC, x + farmArmWatOffX, y + farmArmWatOffY, 40, 54,
							memDC, 0, 0, 20, 27, RGB(255, 0, 255));
					}
				}
			}
			else if (g_player.dir == DIR_RIGHT) {
				if (g_player.farming_arm[g_farmArmIdx] != NULL) {
					SelectObject(memDC, g_player.farming_arm[g_farmArmIdx]);
					if (g_currentTool == TOOL_HOE || g_currentTool == TOOL_AXE) {
						TransparentBlt(hDC, x + farmArmHoeOffX_side, y + farmArmHoeOffY_side, 64, 74,
							memDC, 0, 0, 32, 37, RGB(255, 0, 255));
					}
					else if (g_currentTool == TOOL_WATER) {
						TransparentBlt(hDC, x + farmArmWatOffX, y + farmArmWatOffY, 40, 54,
							memDC, 0, 0, 20, 27, RGB(255, 0, 255));
					}
				}
			}
		}
		else {
			// 일반 걷기 팔
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

// 낚시 바의 플레이어 기준 상대 조정값 — 외부에서 이 값만 수정하면 위치 조정 가능
// 플레이어 오른쪽 기준으로 추가 오프셋
static int g_fishBarAdjX = 10;  // 플레이어 오른쪽 끝에서 추가로 얼마나 (픽셀)
static int g_fishBarAdjY = -50;   // 플레이어 Y에서 추가로 얼마나 (픽셀, 음수면 위로)
// 매 프레임 WM_PAINT에서 갱신되는 실제 화면 오프셋 (직접 수정하지 말 것)
static int g_fishBarOffX = 0;
static int g_fishBarOffY = 0;

void FishingGameLogic(HDC hDC, HBRUSH hBrush, HBRUSH oldBrush, HPEN hPen, HPEN oldPen, HBITMAP hBitmap_fishing[4], struct GreenBar greenBar, struct TargetFish targetFish, struct FishingGage fishingGage) {
	// 낚시 게임의 핵심 로직을 구현하는 함수
	// 게이지 바 속에서 물고기 위 아래로 움직이는 것
	// 게이지 바 속 초록색 움직이는 것
	// 초록색 영역 안에 물고기 있으면 게이지 바 옆에 다른 게이지 채워지고 아니면 게이지 하락. 끝까지 하락하면 낚시 실패. 게이지 다 차면 낚시 성공.
	// 물고기 움직임 유형 : 기본 움직임 (랜덤하게 위아래로 움직인다.), 위로 올라갈 때 빨라짐, 아래로 내려갈 때 빨라짐, 움직임 크고 불규칙적.

	HDC hFishDC = CreateCompatibleDC(hDC);
	// 낚시 게이지 바 배경
	SelectObject(hFishDC, hBitmap_fishing[0]);
	TransparentBlt(hDC, 0 + g_fishBarOffX, 0 + g_fishBarOffY, 37, 149, hFishDC, 0, 0, 37, 149, RGB(255, 0, 255));

	// 물고기를 안에 둬야 하는 초록 게이지
	SelectObject(hFishDC, hBitmap_fishing[1]);
	TransparentBlt(hDC, greenBar.x + g_fishBarOffX, greenBar.y + g_fishBarOffY, greenBar.width, greenBar.height, hFishDC, 0, 0, 10, 10, RGB(255, 0, 255));

	if (targetFish.inGreenBar) {
		SelectObject(hFishDC, hBitmap_fishing[2]);
		TransparentBlt(hDC, targetFish.x + g_fishBarOffX, targetFish.y + g_fishBarOffY, targetFish.width, targetFish.height, hFishDC, 20, 0, 19, 20, RGB(255, 0, 255));
	}
	else {
		SelectObject(hFishDC, hBitmap_fishing[2]);
		TransparentBlt(hDC, targetFish.x + g_fishBarOffX, targetFish.y + g_fishBarOffY, targetFish.width, targetFish.height, hFishDC, 0, 0, 19, 20, RGB(255, 0, 255));
	}

	DeleteDC(hFishDC);

	hPen = CreatePen(PS_SOLID, 1, RGB(255, 0, 0));
	oldPen = (HPEN)SelectObject(hDC, hPen);
	hBrush = CreateSolidBrush(RGB(255, 0, 0));
	oldBrush = (HBRUSH)SelectObject(hDC, hBrush);

	int gageTop = fishingGage.height + g_fishBarOffY;
	int gageBottom = fishingGage.y + fishingGage.height + g_fishBarOffY;
	int gageTotalH = gageBottom - gageTop;
	int filledTop = gageBottom - (gageTotalH * fishingGage.current / fishingGage.maxVal);
	Rectangle(hDC, fishingGage.width + g_fishBarOffX, filledTop, fishingGage.x + fishingGage.width + g_fishBarOffX, gageBottom);
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

	// 아이템
	static struct Item fishItem[50];
	static struct Item woodItem[50];
	static struct Item fruitItem[50];

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

	// Shop
	static HBITMAP hBitmap_shopUi[2]; // 0 아이템 이름 가격 적는곳, 1 구매할 아이템 전시하는곳
	static HBITMAP hBitmap_test; // 테스트용 (나중에 제거) - 내 인벤토리 (판매할 것)
	static HBITMAP hBitmap_seedItem[4]; //0감자 1당근 2대황 3딸기

	switch (iMessage)
	{
	case WM_CREATE:
		// 농장 배경 로드
		// (파일명: 농장배경.bmp - 공백 없음. 위치: Project1\이미지소스\농장\)
		hBitmap_farm = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\농장배경.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);

		// ----- [확장 시스템] 추가 리소스 로드 + 초기화 -----
		g_hBitmap_map = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\background\\map.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_hBitmap_house = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\집\\house.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_hBitmap_tree1 = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\tree\\Tree1_118x140.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_hBitmap_tree2 = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\tree\\Tree2_56x72.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_hBitmap_tree3 = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\tree\\Tree3_50x62.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_hBitmap_cultivated = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\farming\\cultivated_land.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_hBitmap_wet = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\farming\\wet_ground.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_hBitmap_icon_hoe = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\icon\\icon_hoe.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_hBitmap_icon_water = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\icon\\icon_water.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_hBitmap_icon_pole = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\icon\\fishing pole.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_hBitmap_inv_quick = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\inventory\\inventory_1.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_hBitmap_inv_main = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\inventory\\inventory_4.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_hBitmap_inv_bag = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\inventory\\Inventory.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);

		// ----- [확장] 모든 아이템 아이콘 일괄 로드 (ItemType 인덱스 기준) -----
		g_hBitmap_item[ITEM_HOE] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\icon\\icon_hoe.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_hBitmap_item[ITEM_WATER] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\icon\\icon_water.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_hBitmap_item[ITEM_ROD] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\icon\\fishing pole.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_hBitmap_item[ITEM_AXE] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\icon\\axe_48x48.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_hBitmap_item[ITEM_SEED_CARROT] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\icon\\Carrot_Seeds_48x48.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_hBitmap_item[ITEM_SEED_POTATO] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\icon\\Potato_Seeds_48x48.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_hBitmap_item[ITEM_SEED_RHUBARB] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\icon\\Rhubarb_Seeds_48x48.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_hBitmap_item[ITEM_SEED_STRAWBERRY] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\icon\\Strawberry_Seeds_48x48.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_hBitmap_item[ITEM_FISH_CARP] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\icon\\Carp_48x48.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_hBitmap_item[ITEM_FISH_PERCH] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\icon\\Perch_48x48.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_hBitmap_item[ITEM_FISH_PUFFERFISH] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\icon\\Pufferfish_48x48.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_hBitmap_item[ITEM_FISH_SQUID] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\icon\\Squid_48x48.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_hBitmap_item[ITEM_LOG] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\icon\\wood_48x48.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);

		// ----- [작물 비트맵 로드] CropType × growStage 0~3 -----
		g_hBitmap_crop[CROP_CARROT][0] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\crops\\carrot\\carrot_seed.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_hBitmap_crop[CROP_CARROT][1] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\crops\\carrot\\carrot_grow1.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_hBitmap_crop[CROP_CARROT][2] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\crops\\carrot\\carrot_grow2.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_hBitmap_crop[CROP_CARROT][3] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\crops\\carrot\\carrot_ready.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_hBitmap_crop[CROP_POTATO][0] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\crops\\potato\\potato_seed.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_hBitmap_crop[CROP_POTATO][1] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\crops\\potato\\potato_grow1.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_hBitmap_crop[CROP_POTATO][2] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\crops\\potato\\potato_grow2.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_hBitmap_crop[CROP_POTATO][3] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\crops\\potato\\potato_ready.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_hBitmap_crop[CROP_RHUBARB][0] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\crops\\rhubarb\\rhubarb_seed.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_hBitmap_crop[CROP_RHUBARB][1] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\crops\\rhubarb\\rhubarb_grow1.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_hBitmap_crop[CROP_RHUBARB][2] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\crops\\rhubarb\\rhubarb_grow2.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_hBitmap_crop[CROP_RHUBARB][3] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\crops\\rhubarb\\rhubarb_ready.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_hBitmap_crop[CROP_STRAWBERRY][0] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\crops\\strawberry\\straw_seed.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_hBitmap_crop[CROP_STRAWBERRY][1] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\crops\\strawberry\\straw_grow1.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_hBitmap_crop[CROP_STRAWBERRY][2] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\crops\\strawberry\\straw_grow2.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_hBitmap_crop[CROP_STRAWBERRY][3] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\crops\\strawberry\\straw_ready.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);

		// 수확 아이템 아이콘
		g_hBitmap_item[ITEM_CARROT] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\icon\\Carrot_48x48.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_hBitmap_item[ITEM_POTATO] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\icon\\Potato_48x48.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_hBitmap_item[ITEM_RHUBARB] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\icon\\Rhubarb_48x48.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_hBitmap_item[ITEM_STRAWBERRY] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\icon\\Strawberry_48x48.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);

		InitTrees();
		g_house.x = 705; g_house.y = 470;   // 절대 좌표 고정 (흙길 끝 빨간 영역)
		g_house.currentFrame = 0; g_house.frameTimer = 0;
		g_house.isDoorOpening = false; g_house.doorAnimDone = false;
		// [중요] 게임 시작 시 카메라를 플레이어 중앙에 맞춰 1회 강제 계산
		// (이걸 안 하면 첫 프레임이 화면 좌상단을 비춰서 어색함)
		UpdateCamera();
		g_currentTool = TOOL_NONE;
		g_isInventoryOpen = false;
		g_selectedQuickSlot = -1;
		g_selectedInv4 = -1;
		// 퀵슬롯 초기 구성: 1=호미, 2=물뿌리개, 3=낚싯대, 4=도끼
		quickSlots[0] = ITEM_HOE;     g_quickCount[0] = 1;
		quickSlots[1] = ITEM_WATER;   g_quickCount[1] = 1;
		quickSlots[2] = ITEM_ROD;     g_quickCount[2] = 1;
		quickSlots[3] = ITEM_AXE;     g_quickCount[3] = 1;
		// 가방 초기화 (전부 비움)
		for (int kk = 0; kk < INV4_COUNT; kk++) {
			bagSlots[kk] = ITEM_NONE;
			g_bagCount[kk] = 0;
		}
		// 4가지 씨앗을 0~3번 슬롯에 1개씩 지급
		bagSlots[0] = ITEM_SEED_CARROT;     g_bagCount[0] = 1;
		bagSlots[1] = ITEM_SEED_POTATO;     g_bagCount[1] = 1;
		bagSlots[2] = ITEM_SEED_RHUBARB;    g_bagCount[2] = 1;
		bagSlots[3] = ITEM_SEED_STRAWBERRY; g_bagCount[3] = 1;
		// (가방 초기화는 위에서 완료, 도끼는 퀵슬롯 4번에 장착됨)
		for (int rr = 0; rr < MAP_TILE_H; rr++)
			for (int cc = 0; cc < MAP_TILE_W; cc++) farmState[rr][cc] = 0;

		// 나무 베면 나오는 아이템 로드
		//이미지소스\\농장\\icon\\wood_48x48.bmp

		// 호미로 작물 캐면 나오는 아이템 로드
		// 이미지소스\\농장\\icon\\Potato_48x48.bmp
		// 이미지소스\\농장\\icon\\Carrot_48x48.bmp
		// 이미지소스\\농장\\icon\\Rhubarb_48x48.bmp
		// 이미지소스\\농장\\icon\\Strawberry_48x48.bmp

		// 플레이어 이미지 로드 (아래 base/base_arm 로드에서 수행)
		g_player.farming_arm[0] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\사람\\farmer_호미_앞면1_32x37.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_player.farming_arm[1] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\사람\\farmer_호미_앞면2_32x37.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_player.farming_arm[2] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\사람\\farmer_호미_앞면3_32x37.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_player.farming_arm[3] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\사람\\farmer_호미_앞면4_32x37.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_player.farming_arm[4] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\사람\\farmer_호미_앞면5_32x37.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_player.farming_arm[5] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\사람\\farmer_호미_옆면1_32x37.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_player.farming_arm[6] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\사람\\farmer_호미_옆면2_32x37.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_player.farming_arm[7] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\사람\\farmer_호미_옆면3_32x37.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_player.farming_arm[8] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\사람\\farmer_호미_옆면4_32x37.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_player.farming_arm[9] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\사람\\farmer_호미_옆면5_32x37.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_player.farming_arm[10] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\사람\\farmer_도끼_앞면1_32x37.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_player.farming_arm[11] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\사람\\farmer_도끼_앞면2_32x37.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_player.farming_arm[12] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\사람\\farmer_도끼_앞면3_32x37.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_player.farming_arm[13] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\사람\\farmer_도끼_앞면4_32x37.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_player.farming_arm[14] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\사람\\farmer_도끼_앞면5_32x37.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_player.farming_arm[15] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\사람\\farmer_도끼_옆면1_32x37.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_player.farming_arm[16] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\사람\\farmer_도끼_옆면2_32x37.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_player.farming_arm[17] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\사람\\farmer_도끼_옆면3_32x37.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_player.farming_arm[18] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\사람\\farmer_도끼_옆면4_32x37.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_player.farming_arm[19] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\사람\\farmer_도끼_옆면5_32x37.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_player.farming_arm[20] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\사람\\farmer_물뿌리개_앞면1_20x27.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_player.farming_arm[21] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\사람\\farmer_물뿌리개_앞면2_20x27.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_player.farming_arm[22] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\사람\\farmer_물뿌리개_옆면1_20x27.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_player.farming_arm[23] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\사람\\farmer_물뿌리개_옆면2_20x27.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);

		//씬/플레이어 초기화
		g_currentScene = SCENE_SHOP;
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

		// 물고기 아이템 비트맵 로드
		g_hBitmap_fishItem[FISH_ITEM_CARP] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\icon\\Carp_48x48.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_hBitmap_fishItem[FISH_ITEM_PUFFER] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\icon\\Pufferfish_48x48.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_hBitmap_fishItem[FISH_ITEM_SQUID] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\icon\\Squid_48x48.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		g_hBitmap_fishItem[FISH_ITEM_BASS] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\icon\\Perch_48x48.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);

		hBitmap_fishingGround = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\낚시\\fishingmap_1280x1280.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		hBitmap_fishTree[0] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\낚시\\Tree1_118x140.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		hBitmap_fishTree[1] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\낚시\\Tree2_56x72.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		hBitmap_fishTree[2] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\낚시\\Tree3_50x62.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);

		// 씨앗 아이템 비트맵 로드
		hBitmap_seedItem[0] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\icon\\Potato_Seeds_48x48.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		hBitmap_seedItem[1] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\icon\\Carrot_Seeds_48x48.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		hBitmap_seedItem[2] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\icon\\Rhubarb_Seeds_48x48.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		hBitmap_seedItem[3] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\icon\\Strawberry_Seeds_48x48.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);

		// Shop 비트맵 로드
		hBitmap_shopUi[0] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\inventory\\emptyUi_300x300.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		hBitmap_shopUi[1] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\inventory\\inventory_1.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		hBitmap_test = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\농장\\inventory\\inventory_4.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);

		canFishing = false; // 낚시 가능 영역에 있을 때만 true로 설정됨

		// 낚시터 나무 HP 초기화 (타일맵 순회해서 8/9/10 위치 등록)
		g_fishingTreeStates.clear();
		{
			int r, c;
			for (r = 0; r < FISHING_MAP_ROWS; r++) {
				for (c = 0; c < FISHING_MAP_COLS - 1; c++) {
					int tile = g_fishingTileMap[r][c];
					if ((tile == 8 || tile == 9 || tile == 10) && g_fishingTileMap[r][c + 1] == tile) {
						FishingTreeState fs;
						fs.row = r;
						fs.col = c;
						fs.hp = TREE_HP_DEFAULT;
						fs.shakeTimer = 0;
						g_fishingTreeStates.push_back(fs);
						c++; // 오른쪽 칸 건너뜀
					}
				}
			}
		}
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
			// 작물 (씨앗 → grow1 → grow2 → ready) — 카메라 오프셋 적용
			DrawCrops(backDC);


			// 그리기 순서: 집 먼저 → 나무 나중 (나무가 집 앞에 그려져 자연스럽게)
			DrawHouseAnimated(backDC);
			DrawFarmTrees(backDC);
			DrawDroppedItem(backDC);

			DrawPlayer(backDC);

			// 안내
			SetBkMode(backDC, TRANSPARENT);
			SetTextColor(backDC, RGB(255, 255, 255));
			// 상단 안내 + 게임 시간/일차 표시
			wchar_t infoBuf[160];
			const wchar_t* dayPart;
			if (g_timeOfDay < 0.50f)      dayPart = L"낮";
			else if (g_timeOfDay < 0.65f) dayPart = L"저녁";
			else if (g_timeOfDay < 0.90f) dayPart = L"밤";
			else                          dayPart = L"새벽";
			wsprintfW(infoBuf, L"[농장] DAY %d / %s   |   방향키 이동, 1/2/3 도구, E 인벤토리",
				g_gameDay, dayPart);
			TextOut(backDC, 10, 10, infoBuf, (int)wcslen(infoBuf));

			// (밤 오버레이 + UI 는 switch 바깥 공통 영역에서 그려짐)
			break;
		}

		case SCENE_FISHING:
		{
			// 낚시 씬 배경 — 농장과 동일한 카메라 오프셋 1:1 BitBlt 방식 (200% 확대 비율 통일)
			// fishingmap_1280x1280.bmp 가 1280x1280 원본이므로 그대로 cameraX/Y 만큼 잘라 화면에 표시
			if (!hBitmap_fishingGround) {
				HBRUSH seaBrush = CreateSolidBrush(RGB(60, 110, 170));
				RECT seaRect = { 0, 0, CLIENT_W, CLIENT_H };
				FillRect(backDC, &seaRect, seaBrush);
				DeleteObject(seaBrush);
			}
			else {
				HDC hFishingDC = CreateCompatibleDC(backDC);
				HBITMAP hOldFishing = (HBITMAP)SelectObject(hFishingDC, hBitmap_fishingGround);
				BitBlt(backDC, 0, 0, CLIENT_W, CLIENT_H,
					hFishingDC, cameraX, cameraY,
					SRCCOPY);
				SelectObject(hFishingDC, hOldFishing);
				DeleteDC(hFishingDC);
			}

			// 낚시터플레이어이동못하는영역그리기(backDC);
			// 낚시가능영역그리기(backDC);
			// 농장으로이동영역그리기(backDC);

			// 플레이어 발 Y — 월드 좌표 기준 (DrawTrees 판정에 사용)
			int playerFootY = g_player.y + g_player.h * 3 / 4;

			// 플레이어 뒤에 있는 나무 먼저 (플레이어가 나무 앞에 서있는 경우)
			DrawFishingTrees(backDC, hBitmap_fishTree, playerFootY, false);

			// 바닥 물고기 아이템 (플레이어 뒤, 나무 앞)
			DrawDroppedFish(backDC);
			// 바닥 드롭 아이템 (통나무 등)
			DrawDroppedItem(backDC);

			// 플레이어 (스프라이트 or 도형)
			DrawPlayer(backDC);

			// 플레이어 앞에 있는 나무 나중에 (나무가 플레이어를 가리는 경우)
			DrawFishingTrees(backDC, hBitmap_fishTree, playerFootY, true);

			// (밤 오버레이 + UI 는 switch 바깥 공통 영역에서 그려짐)

			SetBkMode(backDC, TRANSPARENT);
			SetTextColor(backDC, RGB(255, 255, 255));
			const wchar_t* hint = L"[낚시터] 좌클릭으로 낚시 시작";
			TextOut(backDC, 10, 10, hint, (int)wcslen(hint));

			//낚시 그리기 코드
			SelectObject(hMemDC, hBitmap);
			if (g_fishingPhase == FISHING_PHASE_GAME) {
				// 낚시 바를 플레이어 화면 좌표 기준으로 배치
				// g_fishBarAdjX/Y 로 세부 위치 조정 가능
				g_fishBarOffX = (g_player.x - cameraX) + g_player.w + g_fishBarAdjX;
				g_fishBarOffY = (g_player.y - cameraY) + g_fishBarAdjY;
				FishingGameLogic(backDC, hBrush, oldBrush, hPen, oldPen, hBitmap_fishing, greenBar, targetFish, fishingGage);
			}
			break;
		}

		case SCENE_SHOP:
		{
			// 상점 씬: 임시 회색 배경 + 안내 텍스트
			HBRUSH shopBg = CreateSolidBrush(RGB(208, 176, 138));
			RECT r = { 0, 0, CLIENT_W, CLIENT_H };
			FillRect(backDC, &r, shopBg);
			DeleteObject(shopBg);

			HDC hShopDC = CreateCompatibleDC(backDC);

			// 아이템에 대한 설명이 있을 자리.
			// 판매할 아이템이나 인벤토리 속 아이템에 마우스를 올리면 이 부분에 아이템 설명이 뜬다.
			// 감자 씨앗 25골드 당근 씨앗 15골드 대황 씨앗 50골드 딸기 씨앗 100골드
			HBITMAP hOldShop = (HBITMAP)SelectObject(hShopDC, hBitmap_shopUi[0]);
			TransparentBlt(backDC, 550, 200, 200, 200,
				hShopDC, 0, 0, 300, 300, RGB(255, 0, 255));

			// 현재 보유 골드 상시 표시
			{
				HFONT hFontGold = CreateFont(
					20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
					DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
					DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, TEXT("맑은 고딕")
				);
				HFONT hOldFontGold = (HFONT)SelectObject(backDC, hFontGold);
				SetBkMode(backDC, TRANSPARENT);
				SetTextColor(backDC, RGB(0, 0, 0));
				wchar_t goldStr[64];
				wsprintfW(goldStr, L"보유 골드: %d G", g_gold);
				TextOut(backDC, 560, 360, goldStr, (int)wcslen(goldStr));
				SetTextColor(backDC, RGB(0, 0, 0));
				SelectObject(backDC, hOldFontGold);
				DeleteObject(hFontGold);
			}
			HFONT hFontBig = CreateFont(
				40, 0, 0, 0,
				FW_BOLD,
				FALSE, FALSE, FALSE,
				DEFAULT_CHARSET,
				OUT_DEFAULT_PRECIS,
				CLIP_DEFAULT_PRECIS,
				DEFAULT_QUALITY,
				DEFAULT_PITCH | FF_DONTCARE,
				TEXT("맑은 고딕")
			);

			HFONT hFontSmall = CreateFont(
				30, 0, 0, 0,
				FW_BOLD,
				FALSE, FALSE, FALSE,
				DEFAULT_CHARSET,
				OUT_DEFAULT_PRECIS,
				CLIP_DEFAULT_PRECIS,
				DEFAULT_QUALITY,
				DEFAULT_PITCH | FF_DONTCARE,
				TEXT("맑은 고딕")
			);

			HFONT hOldFont = (HFONT)SelectObject(backDC, hFontBig);
			SetBkMode(backDC, TRANSPARENT);
			SetTextColor(backDC, RGB(0, 0, 0));

			if (g_shopHoverIdx >= 0) {
				// 씨앗 아이템 호버: 이름 + 구매 안내
				TextOut(backDC, 560, 210, g_seedNames[g_shopHoverIdx], (int)wcslen(g_seedNames[g_shopHoverIdx]));
				SelectObject(backDC, hFontSmall);
				wchar_t priceStr[64];
				wsprintfW(priceStr, L"가격: %d골드", g_seedPrices[g_shopHoverIdx]);
				TextOut(backDC, 560, 280, priceStr, (int)wcslen(priceStr));
				if (g_gold < g_seedPrices[g_shopHoverIdx]) {
					SetTextColor(backDC, RGB(200, 0, 0));
					TextOut(backDC, 560, 310, L"돈이 부족합니다.", 8);
					SetTextColor(backDC, RGB(0, 0, 0));
				}
				else {
					TextOut(backDC, 560, 310, L"좌클릭으로 1개 구매.", 11);
				}
			}
			else if (g_shopInvHoverSlot >= 0 && bagSlots[g_shopInvHoverSlot] != ITEM_NONE) {
				// 인벤토리 아이템 호버
				ItemType hovItem = (ItemType)bagSlots[g_shopInvHoverSlot];
				// 아이템 이름 테이블
				static const wchar_t* itemNames[ITEM_COUNT] = {
					L"없음", L"호미", L"물뿌리개", L"낚싯대", L"도끼",
					L"당근 씨앗", L"감자 씨앗", L"대황 씨앗", L"딸기 씨앗",
					L"잉어", L"농어", L"복어", L"오징어",
					L"나무", L"당근", L"감자", L"대황", L"딸기"
				};
				const wchar_t* itemName = (hovItem >= 0 && hovItem < ITEM_COUNT) ? itemNames[hovItem] : L"알 수 없음";
				SelectObject(backDC, hFontBig);
				TextOut(backDC, 560, 210, itemName, (int)wcslen(itemName));
				SelectObject(backDC, hFontSmall);
				bool isTool = (hovItem >= TOOL_HOE && hovItem <= TOOL_AXE);
				if (isTool) {
					TextOut(backDC, 560, 280, L"판매할 수 없습니다.", 10);
				}
				else {
					int sellPrice = (hovItem >= 0 && hovItem < ITEM_COUNT) ? g_itemSellPrice[hovItem] : 0;
					wchar_t sellStr[64];
					wsprintfW(sellStr, L"판매가: %d골드", sellPrice);
					TextOut(backDC, 560, 280, sellStr, (int)wcslen(sellStr));
					TextOut(backDC, 560, 310, L"좌클릭으로 1개 판매.", 11);
				}
			}
			else {
				// 호버 없을 때 기본 안내
				SelectObject(backDC, hFontSmall);
				TextOut(backDC, 560, 240, L"아이템에 마우스를", 8);
				TextOut(backDC, 560, 270, L"올려보세요.", 5);
			}

			SelectObject(backDC, hOldFont);
			DeleteObject(hFontBig);
			DeleteObject(hFontSmall);

			// 판매할 아이템이 있을 자리.
			SelectObject(hShopDC, hBitmap_shopUi[1]);
			TransparentBlt(backDC, 100, 200, 300, 96,
				hShopDC, 0, 0, 300, 96, RGB(255, 0, 255));
			SelectObject(hShopDC, hBitmap_seedItem[0]);
			TransparentBlt(backDC, 100 + 25, 200 + 25, 48, 48,
				hShopDC, 0, 0, 48, 48, RGB(255, 0, 255));
			SelectObject(hShopDC, hBitmap_seedItem[1]);
			TransparentBlt(backDC, 100 + 95, 200 + 25, 48, 48,
				hShopDC, 0, 0, 48, 48, RGB(255, 0, 255));
			SelectObject(hShopDC, hBitmap_seedItem[2]);
			TransparentBlt(backDC, 100 + 95 + 70, 200 + 25, 48, 48,
				hShopDC, 0, 0, 48, 48, RGB(255, 0, 255));
			SelectObject(hShopDC, hBitmap_seedItem[3]);
			TransparentBlt(backDC, 100 + 90 + 140, 200 + 25, 48, 48,
				hShopDC, 0, 0, 48, 48, RGB(255, 0, 255));

			SelectObject(hShopDC, hOldShop);
			DeleteDC(hShopDC);

			// 플레이어 인벤토리 (상점 고정 위치에 항상 표시)
			DrawShopInventoryPanel(backDC);

			break;
		}
		}

		// ============================================================
		// [공통 렌더링 계층] switch 바깥 — 씬과 무관하게 항상 마지막에 그림.
		// 순서:
		//   1) 밤/낮 명암 (AlphaBlend) — 모든 게임 요소 위에 덮임
		//   2) 1줄 퀵슬롯, 4x4 인벤토리 — 화면 고정, 밤이라도 어두워지지 않음
		//   3) 드래그 중인 아이템 — 모든 UI 위에 표시
		// ============================================================
		// 상점/낚시미니게임 등 일부 씬은 시간 흐름 표현 안 해도 되면 여기 조건 추가 가능.
		DrawNightOverlay(backDC);
		DrawQuickSlot(backDC);
		DrawInventoryPanel(backDC);
		DrawDraggedItem(backDC);

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
		if (g_currentScene == SCENE_SHOP) {
			int mx = LOWORD(lParam);
			int my = HIWORD(lParam);

			// 씨앗 구매 클릭
			int i;
			for (i = 0; i < SHOP_SEED_COUNT; i++) {
				if (mx >= g_seedItemX[i] && mx < g_seedItemX[i] + SHOP_SEED_W &&
					my >= g_seedItemY[i] && my < g_seedItemY[i] + SHOP_SEED_H) {
					if (g_gold < g_seedPrices[i]) break;
					g_gold -= g_seedPrices[i];
					// 씨앗 종류: 0=감자씨앗 1=당근씨앗 2=대황씨앗 3=딸기씨앗 (g_seedNames 순)
					static const ItemType seedItemMap[SHOP_SEED_COUNT] = {
						ITEM_SEED_POTATO, ITEM_SEED_CARROT, ITEM_SEED_RHUBARB, ITEM_SEED_STRAWBERRY
					};
					AddItemToBag(seedItemMap[i]);
					InvalidateRect(hWnd, NULL, FALSE);
					break;
				}
			}

			// 인벤토리 슬롯 판매 클릭
			int cell = HitShopInvCell(mx, my);
			if (cell >= 0 && bagSlots[cell] != ITEM_NONE) {
				ItemType sellItem = (ItemType)bagSlots[cell];
				bool isTool = (sellItem >= TOOL_HOE && sellItem <= TOOL_AXE);
				if (!isTool && sellItem < ITEM_COUNT && g_itemSellPrice[sellItem] > 0) {
					g_gold += g_itemSellPrice[sellItem];
					g_bagCount[cell]--;
					if (g_bagCount[cell] <= 0) {
						bagSlots[cell] = ITEM_NONE;
						g_bagCount[cell] = 0;
					}
					InvalidateRect(hWnd, NULL, FALSE);
				}
			}
			break;
		}

		// ============================================================
		// [전역 인벤토리 마우스 처리] — 씬과 무관하게 모든 맵에서 작동
		// ============================================================
		{
			int mx = LOWORD(lParam);
			int my = HIWORD(lParam);

			// 1) 가방 아이콘 클릭 → 인벤토리 토글 (상점 제외 모든 씬에서)
			if (IsClickInBag(mx, my) && g_currentScene != SCENE_SHOP) {
				g_isInventoryOpen = !g_isInventoryOpen;
				g_selectedInv4 = -1;
				InvalidateRect(hWnd, NULL, FALSE);
				break;
			}

			// 2) 인벤토리 열려있으면 드래그 앤 드롭 (모든 씬에서)
			if (g_isInventoryOpen) {
				int cell = HitInv4Cell(mx, my);
				if (cell >= 0) {
					BeginDrag(cell, false);
					g_dragMouseX = mx; g_dragMouseY = my;
					InvalidateRect(hWnd, NULL, FALSE);
					break;
				}
				int qs = HitQuickSlot(mx, my);
				if (qs >= 0) {
					BeginDrag(qs, true);
					g_dragMouseX = mx; g_dragMouseY = my;
					InvalidateRect(hWnd, NULL, FALSE);
					break;
				}
				// 인벤토리 패널 안 클릭은 흡수 (맵으로 안 넘어가게)
				if (IsClickOnUI(mx, my)) {
					InvalidateRect(hWnd, NULL, FALSE);
					break;
				}
			}
			else {
				// 인벤토리 닫혀있으면 퀵슬롯 클릭 = 도구 선택 (모든 씬에서)
				int qs = HitQuickSlot(mx, my);
				if (qs >= 0) {
					SelectQuickSlot(qs);
					InvalidateRect(hWnd, NULL, FALSE);
					break;
				}
				// 퀵슬롯 패널 영역 클릭은 흡수
				if (IsClickOnUI(mx, my)) {
					InvalidateRect(hWnd, NULL, FALSE);
					break;
				}
			}
		}

		// ============================================================
		// [SCENE_FARM 전용 맵 상호작용] — 호미/물/씨앗/도끼/수확
		// ============================================================
		if (g_currentScene == SCENE_FARM) {
			int mx = LOWORD(lParam);
			int my = HIWORD(lParam);

			// 5) 맵 클릭 처리 (인벤토리 닫혀있을 때만)
			if (!g_isInventoryOpen) {
				// 도끼 들고 있으면 나무 벌목 시도
				if (g_currentTool == TOOL_AXE) {
					TryChopTree(mx, my);
				}
				else {
					HandleFarmClick(mx, my);
				}

				// 호미/도끼/물뿌리개 선택 시 farming 애니 시작 (애니 중 중복 시작 방지)
				if (!g_isFarmingAnim) {
					if (g_currentTool == TOOL_HOE || g_currentTool == TOOL_AXE) {
						g_isFarmingAnim = true;
						g_farmArmTimer = 0;
						g_farmArmSeqPos = 0;
						g_farmSeqLen = 5;
						if (g_player.dir == DIR_DOWN || g_player.dir == DIR_UP) {
							g_farmSeqPtr = (g_currentTool == TOOL_HOE) ? g_farmSeqHoeFront : g_farmSeqAxeFront;
							g_farmArmIdx = g_farmSeqHoeFront[0]; // 앞면 시작 인덱스
							if (g_currentTool == TOOL_AXE) g_farmArmIdx = g_farmSeqAxeFront[0];
						}
						else {
							g_farmSeqPtr = (g_currentTool == TOOL_HOE) ? g_farmSeqHoeSide : g_farmSeqAxeSide;
							g_farmArmIdx = g_farmSeqHoeSide[0];
							if (g_currentTool == TOOL_AXE) g_farmArmIdx = g_farmSeqAxeSide[0];
						}
					}
					else if (g_currentTool == TOOL_WATER) {
						g_isFarmingAnim = true;
						g_farmArmTimer = 0;
						g_farmArmSeqPos = 0;
						g_farmSeqLen = 3;
						if (g_player.dir == DIR_DOWN || g_player.dir == DIR_UP) {
							g_farmSeqPtr = g_farmSeqWaterFront;
						}
						else {
							g_farmSeqPtr = g_farmSeqWaterSide;
						}
						g_farmArmIdx = g_farmSeqPtr[0];
					}
				}

				InvalidateRect(hWnd, NULL, FALSE);
				break;
			}
		}

		if (g_fishingPhase == FISHING_PHASE_GAME) {
			floatingGreenBar = true; // 낚시 게임 중 마우스 누르면 초록 게이지 올라감
		}

		if (g_currentScene == SCENE_FISHING && !g_isInventoryOpen && g_currentTool == TOOL_AXE
			&& g_fishingPhase == FISHING_PHASE_NONE) {
			int mx = LOWORD(lParam);
			int my = HIWORD(lParam);
			TryChopFishingTree(mx, my);
			// 도끼 팔 애니메이션
			if (!g_isFarmingAnim) {
				g_isFarmingAnim = true;
				g_farmArmTimer = 0;
				g_farmArmSeqPos = 0;
				g_farmSeqLen = 5;
				if (g_player.dir == DIR_DOWN || g_player.dir == DIR_UP) {
					g_farmSeqPtr = g_farmSeqAxeFront;
					g_farmArmIdx = g_farmSeqAxeFront[0];
				}
				else {
					g_farmSeqPtr = g_farmSeqAxeSide;
					g_farmArmIdx = g_farmSeqAxeSide[0];
				}
			}
			InvalidateRect(hWnd, NULL, FALSE);
		}

		if (canFishing && g_fishingPhase == FISHING_PHASE_NONE && g_currentTool == TOOL_POLE) {
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

		// 드래그 종료: 놓은 위치 검사 후 swap 또는 취소
		if (g_draggedSlotIndex >= 0) {
			int mx = LOWORD(lParam);
			int my = HIWORD(lParam);
			// 어디에 놓았는지 확인 (퀵슬롯 우선, 그다음 가방)
			int dropQs = HitQuickSlot(mx, my);
			if (dropQs >= 0) {
				EndDrag(dropQs, true);
			}
			else {
				int dropCell = HitInv4Cell(mx, my);
				if (dropCell >= 0) {
					EndDrag(dropCell, false);
				}
				else {
					// 빈 공간 → 취소 (원위치)
					CancelDrag();
				}
			}
			InvalidateRect(hWnd, NULL, FALSE);
		}

		InvalidateRect(hWnd, NULL, FALSE);
		break;
	}

	case WM_MOUSEMOVE:
	{
		// 드래그 중이면 마우스 위치 갱신해 아이콘이 커서 따라오게
		if (g_draggedSlotIndex >= 0) {
			g_dragMouseX = LOWORD(lParam);
			g_dragMouseY = HIWORD(lParam);
			InvalidateRect(hWnd, NULL, FALSE);
		}
		if (g_currentScene == SCENE_SHOP) {
			int mx = LOWORD(lParam);
			int my = HIWORD(lParam);
			int prevHover = g_shopHoverIdx;
			int prevInvHover = g_shopInvHoverSlot;
			g_shopHoverIdx = -1;
			g_shopInvHoverSlot = -1;
			// 씨앗 호버
			int i;
			for (i = 0; i < SHOP_SEED_COUNT; i++) {
				if (mx >= g_seedItemX[i] && mx < g_seedItemX[i] + SHOP_SEED_W &&
					my >= g_seedItemY[i] && my < g_seedItemY[i] + SHOP_SEED_H) {
					g_shopHoverIdx = i;
					break;
				}
			}
			// 인벤토리 슬롯 호버
			if (g_shopHoverIdx < 0) {
				g_shopInvHoverSlot = HitShopInvCell(mx, my);
			}
			if (g_shopHoverIdx != prevHover || g_shopInvHoverSlot != prevInvHover)
				InvalidateRect(hWnd, NULL, FALSE);
		}
		break;
	}

	case WM_TIMER:
	{
		switch (wParam) { // 타이머 규칙 : 0XXX : 플레이어 관련 타이머, 1XXX : 농사 관련 타이머, 2XXX : 낚시 관련 타이머, 3XXX : 디펜스 관련 타이머
		case 0001: // 플레이어 이동 및 트리거 체크 타이머
			if (g_currentScene == SCENE_FARM || g_currentScene == SCENE_FISHING) {

				// farming 애니메이션 프레임 업데이트
				if (g_isFarmingAnim && g_farmSeqPtr != NULL) {
					g_farmArmTimer++;
					if (g_farmArmTimer >= FARM_ARM_TICKS_PER_FRAME) {
						g_farmArmTimer = 0;
						g_farmArmSeqPos++;
						if (g_farmArmSeqPos >= g_farmSeqLen) {
							// 시퀀스 끝 → 애니 종료
							g_isFarmingAnim = false;
							g_farmArmSeqPos = 0;
							g_farmArmIdx = 0;
						}
						else {
							g_farmArmIdx = g_farmSeqPtr[g_farmArmSeqPos];
						}
					}
				}

				UpdatePlayer();

				// 나무 흔들림 타이머 감소
				for (size_t ti = 0; ti < g_trees.size(); ti++) {
					if (g_trees[ti].shakeTimer > 0)
						g_trees[ti].shakeTimer--;
				}
				// 낚시터 나무 흔들림 타이머 감소
				for (size_t ti = 0; ti < g_fishingTreeStates.size(); ti++) {
					if (g_fishingTreeStates[ti].shakeTimer > 0)
						g_fishingTreeStates[ti].shakeTimer--;
				}

				// 바닥 드롭 아이템 (통나무/작물): 부유 애니 + 유예 + 플레이어 접촉 획득
				for (size_t di = 0; di < g_droppedItems.size(); ) {
					DroppedItem& item = g_droppedItems[di];
					if (!item.active) { g_droppedItems.erase(g_droppedItems.begin() + di); continue; }
					item.floatTimer = (item.floatTimer + 1) % DROPPED_ITEM_FLOAT_PERIOD;
					if (item.graceTick < g_itemGraceTicks) {
						item.graceTick++;
					}
					else {
						int itemLeft = item.worldX - DROPPED_ITEM_DRAW_W / 2;
						int itemTop = item.worldY - DROPPED_ITEM_DRAW_H / 2;
						int playerFX = g_player.x + g_player.w / 4;
						int playerFY = g_player.y + g_player.h * 3 / 4;
						int playerFW = g_player.w / 2;
						int playerFH = g_player.h / 4;
						if (RectOverlap(itemLeft, itemTop, DROPPED_ITEM_DRAW_W, DROPPED_ITEM_DRAW_H,
							playerFX, playerFY, playerFW, playerFH)) {
							AddItemToBag(item.type);
							g_droppedItems.erase(g_droppedItems.begin() + di);
							continue;
						}
					}
					di++;
				}

				// 바닥 물고기: 부유 애니 + 유예 + 플레이어 접촉 획득
				for (size_t fi = 0; fi < g_droppedFishes.size(); ) {
					DroppedFish& fish = g_droppedFishes[fi];
					if (!fish.active) { g_droppedFishes.erase(g_droppedFishes.begin() + fi); continue; }
					fish.floatTimer = (fish.floatTimer + 1) % DROPPED_FISH_FLOAT_PERIOD;
					if (fish.graceTick < g_fishGraceTicks) {
						fish.graceTick++;
					}
					else {
						int fishLeft = fish.worldX - FISH_ITEM_DRAW_W / 2;
						int fishTop = fish.worldY - FISH_ITEM_DRAW_H / 2;
						int playerFX = g_player.x + g_player.w / 4;
						int playerFY = g_player.y + g_player.h * 3 / 4;
						int playerFW = g_player.w / 2;
						int playerFH = g_player.h / 4;
						if (RectOverlap(fishLeft, fishTop, FISH_ITEM_DRAW_W, FISH_ITEM_DRAW_H,
							playerFX, playerFY, playerFW, playerFH)) {
							AddFishToInventory(fish.type);
							g_droppedFishes.erase(g_droppedFishes.begin() + fi);
							continue;
						}
					}
					fi++;
				}
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
				// [확장] 카메라는 농장/낚시 양쪽 모두 따라가도록 통일 (200% 동일 비율)
				if (g_currentScene == SCENE_FARM || g_currentScene == SCENE_FISHING) {
					UpdateCamera();
				}
				// [전역 유지] 게임 시간(낮/밤) 진행 — 씬과 무관하게 항상 계속 흐름
				UpdateGameTime();
				// 농장 씬: 집 애니메이션 + 문열림 완료 시 SCENE_SHOP
				if (g_currentScene == SCENE_FARM) {
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
						if (g_house.shopTransitionTimer >= 17) {
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

				// movementType → 물고기 종류 결정
				FishItemType droppedType = FISH_ITEM_CARP;
				if (targetFish.movementType == FISH_MOVE_FAST_UP)   droppedType = FISH_ITEM_PUFFER;
				if (targetFish.movementType == FISH_MOVE_FAST_DOWN)  droppedType = FISH_ITEM_SQUID;
				if (targetFish.movementType == FISH_MOVE_IRREGULAR)  droppedType = FISH_ITEM_BASS;

				// 플레이어 발 바로 아래에 드롭
				DroppedFish newFish;
				newFish.worldX = g_player.x + g_player.w / 2;
				newFish.worldY = g_player.y + g_player.h;
				newFish.type = droppedType;
				newFish.active = true;
				newFish.floatTimer = 0;
				newFish.graceTick = 0;
				g_droppedFishes.push_back(newFish);

				// [버그 수정] 인벤토리에 자동 적재 (스태킹 — 같은 물고기면 count++, 없으면 빈칸에)
				ItemType invItem = ITEM_FISH_CARP;
				if (droppedType == FISH_ITEM_PUFFER) invItem = ITEM_FISH_PUFFERFISH;
				else if (droppedType == FISH_ITEM_SQUID)  invItem = ITEM_FISH_SQUID;
				else if (droppedType == FISH_ITEM_BASS)   invItem = ITEM_FISH_PERCH;
				AddItemToBag(invItem);
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
		case 'E':
			if (g_currentScene != SCENE_SHOP)  // 상점에서는 인벤토리 토글 불가
				g_isInventoryOpen = !g_isInventoryOpen;
			g_selectedInv4 = -1;
			break;
		case 'Q':
			// 상점 씬에 있을 때 Q 누르면 집 밖(농장)으로 나가기
			if (g_currentScene == SCENE_SHOP) {
				g_currentScene = SCENE_FARM;
				// 집 문 바로 아래 (한 칸 아래) 위치로 스폰
				g_player.x = HOUSE_DOOR_X + (HOUSE_DOOR_W - g_player.w) / 2;
				g_player.y = HOUSE_DOOR_Y + HOUSE_DOOR_H + 8;
				g_player.dir = DIR_DOWN;
				// 집 애니/타이머 초기화 (다시 문 앞에 갔을 때 정상 작동)
				g_house.isDoorOpening = false;
				g_house.doorAnimDone = false;
				g_house.currentFrame = 0;
				g_house.frameTimer = 0;
				g_house.shopTransitionTimer = 0;
				g_sceneCooldown = 30;
			}
			break;
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