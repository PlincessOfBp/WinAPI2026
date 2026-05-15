#include <Windows.h>
#include <tchar.h>
#include <stdlib.h>
#include <time.h>
#include <string>
#include <queue>
#include <math.h>
#include <wingdi.h> // 255 0 255 마젠타색을 투명색으로 사용하기 위해 필요
#pragma comment(lib, "msimg32.lib")

// 이미지소스\\Cursors.ko-KR << 필요한 부분만 잘라서 가공 필요!! 
// 이미지소스\\낚시\\570x540-Beach_Overview

HINSTANCE g_hInst;
LPCTSTR lpszClass = L"Window Class";
LPCTSTR lpszWindowName = L"windows program 1";

// 물고기 움직임 유형
// 각 유형마다 드랍되는 아이템이 다름 (아이템 드랍 구현 시 fishMovementType 변수를 활용할 것)
enum FishMovementType {
	FISH_MOVE_RANDOM = 0,      // 기본: 랜덤하게 위아래로 움직임
	FISH_MOVE_FAST_UP = 1,     // 위로 올라갈 때 빠르게, 내려올 때 느리게
	FISH_MOVE_FAST_DOWN = 2,   // 아래로 내려갈 때 빠르게, 올라갈 때 느리게
	FISH_MOVE_IRREGULAR = 3    // 움직임이 크고 불규칙적
};

// 각 움직임 유형별 등장 비율 (합산이 100이 되도록 설정, 기본값 1:1:1:1)
static int fishMovementRatio[4] = { 25, 25, 25, 25 };

struct GreenBar {
	int x, y; // 게이지 바의 위치
	int width, height; // 게이지 바의 크기
};

struct TargetFish {
	int x, y; // 물고기의 위치
	int width, height; // 물고기의 크기
	bool inGreenBar; // 물고기가 초록색 영역 안에 있는지 여부.
	int moveDirY;    // 물고기 이동 방향 (1: 아래, -1: 위)
	int moveTimer;   // 방향 전환 타이머 카운터
	int moveInterval;// 방향 유지 프레임 수 (불규칙 움직임에 활용)
	FishMovementType movementType; // 이 물고기의 움직임 유형
};

struct FishingGage { // 낚시 외부 게이지 정보
	int x, y; // 게이지의 위치
	int width, height; // 게이지의 크기
	int current;     // 현재 게이지 값 (0 ~ max)
	int maxVal;      // 게이지 최대값
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

// inGreenBar 체크: 물고기가 초록 게이지 영역 안에 있는지 판단
bool CheckFishInGreenBar(const struct TargetFish& fish, const struct GreenBar& bar) {
	// 물고기 중심 Y가 초록 바 영역 안에 있으면 true
	int fishCenterY = fish.y + fish.height / 2;
	return (fishCenterY >= bar.y && fishCenterY <= bar.y + bar.height);
}

// 물고기 움직임 처리 (타이머마다 호출)
void UpdateFishMovement(struct TargetFish& fish) {
	const int FISH_MIN_Y = 5;
	const int FISH_MAX_Y = 125; // 5 ~ 125 사이에서 움직임

	int speedNormal = 3;
	int speedFast = 7;
	int speedSlow = 2;

	fish.moveTimer++;

	switch (fish.movementType) {
	case FISH_MOVE_RANDOM:
	{
		// 일정 간격마다 방향을 랜덤하게 바꿈
		if (fish.moveTimer >= fish.moveInterval) {
			fish.moveDirY = (rand() % 2 == 0) ? 1 : -1;
			fish.moveInterval = 5 + rand() % 8; // 5~12 프레임마다 방향 전환
			fish.moveTimer = 0;
		}
		fish.y += fish.moveDirY * speedNormal;
		break;
	}
	case FISH_MOVE_FAST_UP:
	{
		// 위로 올라갈 때 빠름, 내려올 때 느림
		if (fish.moveTimer >= fish.moveInterval) {
			fish.moveDirY = (rand() % 2 == 0) ? 1 : -1;
			fish.moveInterval = 6 + rand() % 6;
			fish.moveTimer = 0;
		}
		int spd = (fish.moveDirY == -1) ? speedFast : speedSlow;
		fish.y += fish.moveDirY * spd;
		break;
	}
	case FISH_MOVE_FAST_DOWN:
	{
		// 아래로 내려갈 때 빠름, 올라갈 때 느림
		if (fish.moveTimer >= fish.moveInterval) {
			fish.moveDirY = (rand() % 2 == 0) ? 1 : -1;
			fish.moveInterval = 6 + rand() % 6;
			fish.moveTimer = 0;
		}
		int spd = (fish.moveDirY == 1) ? speedFast : speedSlow;
		fish.y += fish.moveDirY * spd;
		break;
	}
	case FISH_MOVE_IRREGULAR:
	{
		// 불규칙적으로 큰 폭으로 움직임: 매 프레임마다 방향이 바뀔 수도 있음
		if (fish.moveTimer >= fish.moveInterval) {
			fish.moveDirY = (rand() % 2 == 0) ? 1 : -1;
			fish.moveInterval = 1 + rand() % 4; // 1~4 프레임마다 방향 전환 (매우 불규칙)
			fish.moveTimer = 0;
		}
		int irregularSpeed = 4 + rand() % 5; // 4~8 사이 랜덤 속도
		fish.y += fish.moveDirY * irregularSpeed;
		break;
	}
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

	HDC hFishDC = CreateCompatibleDC(hDC);
	// 낚시 게이지 바 배경
	SelectObject(hFishDC, hBitmap_fishing[0]);
	TransparentBlt(hDC, 0, 0, 37, 149, hFishDC, 0, 0, 37, 149, RGB(255, 0, 255));

	// 물고기를 안에 둬야 하는 초록 게이지
	SelectObject(hFishDC, hBitmap_fishing[1]);
	TransparentBlt(hDC, greenBar.x, greenBar.y, greenBar.width, greenBar.height, hFishDC, 0, 0, 10, 10, RGB(255, 0, 255));

	if (targetFish.inGreenBar) {
		SelectObject(hFishDC, hBitmap_fishing[2]);
		TransparentBlt(hDC,
			targetFish.x, targetFish.y,
			targetFish.width, targetFish.height,
			hFishDC,
			20, 0, 19, 20,
			RGB(255, 0, 255));
	}
	else {
		SelectObject(hFishDC, hBitmap_fishing[2]);
		TransparentBlt(hDC, targetFish.x, targetFish.y, targetFish.width, targetFish.height, hFishDC, 0, 0, 19, 20, RGB(255, 0, 255));
	}

	DeleteDC(hFishDC);

	// 외부 낚시 게이지 바 그리기 (위에서 아래로 채워짐 / 위에서 줄어듦)
	// 게이지 바의 전체 영역: (fishingGage.x, fishingGage.y) ~ (fishingGage.x + fishingGage.width, fishingGage.y + fishingGage.height)
	// current/maxVal 비율만큼 위에서 아래로 채워지는 구조
	{
		int gageLeft = fishingGage.x;
		int gageTop = fishingGage.y;
		int gageRight = fishingGage.x + fishingGage.width;
		int gageBottom = fishingGage.y + fishingGage.height;
		int gageFullH = fishingGage.height;

		// 배경 (빈 게이지)
		hPen = CreatePen(PS_SOLID, 1, RGB(80, 80, 80));
		oldPen = (HPEN)SelectObject(hDC, hPen);
		hBrush = CreateSolidBrush(RGB(50, 50, 50));
		oldBrush = (HBRUSH)SelectObject(hDC, hBrush);
		Rectangle(hDC, gageLeft, gageTop, gageRight, gageBottom);
		SelectObject(hDC, oldPen);
		SelectObject(hDC, oldBrush);
		DeleteObject(hPen);
		DeleteObject(hBrush);

		// 채워진 게이지 (아래에서 위로 상승, 위에서 아래로 줄어듦)
		// current가 maxVal에 가까울수록 gageTop에서부터 많이 채워짐
		int filledH = (gageFullH * fishingGage.current) / fishingGage.maxVal;
		int filledTop = gageBottom - filledH; // 아래에서 위로 채움

		if (filledH > 0) {
			hPen = CreatePen(PS_SOLID, 1, RGB(255, 180, 0));
			oldPen = (HPEN)SelectObject(hDC, hPen);
			hBrush = CreateSolidBrush(RGB(255, 200, 0));
			oldBrush = (HBRUSH)SelectObject(hDC, hBrush);
			Rectangle(hDC, gageLeft, filledTop, gageRight, gageBottom);
			SelectObject(hDC, oldPen);
			SelectObject(hDC, oldBrush);
			DeleteObject(hPen);
			DeleteObject(hBrush);
		}
	}
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

	static bool canFishing; // 특정 영역에서 낚시 가능 여부
	static bool isFishing; // 낚시 중인지 여부
	static bool floatingGreenBar; // 초록 게이지가 위로 올라가는지. true면 올라가는 것.
	static HBITMAP hBitmap_fishing[4]; //0 게이지 바탕 //1 초록 게이지 //2 게이지 안에서 움직이는 물고기 //3 물었다! 표시
	static struct GreenBar greenBar; // 초록 게이지 바 정보
	static struct TargetFish targetFish; // 게이지 안 속을 움직이는 물고기
	static struct FishingGage fishingGage; // 낚시 외부 게이지 정보

	switch (iMessage)
	{
	case WM_CREATE:
		hBitmap = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\낚시\\570x540-Beach_Overview.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);

		hBitmap_fishing[0] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\낚시\\fishing_37x149.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
		hBitmap_fishing[1] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\낚시\\green_10x10.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION); // 3배정도 길게 늘려서 사용. 10x50 정도의 크기.
		hBitmap_fishing[2] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\낚시\\fish_39x20.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION); // 19x20 씩 잘라서 사용해야 함. 19x20, 1픽셀 띄우고 다시 19x20 이렇게.
		hBitmap_fishing[3] = (HBITMAP)LoadImage(g_hInst, TEXT("이미지소스\\낚시\\물었다_74x28.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);

		canFishing = true; // 개발용, 실제로는 특정 영역에서만 true로 설정해야 함
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
		targetFish.movementType = FISH_MOVE_RANDOM; // 낚시 시작 시 SelectFishMovementType()으로 결정

		fishingGage.x = 42;  // 게이지 바 오른쪽 옆에 위치
		fishingGage.y = 4;
		fishingGage.width = 8;
		fishingGage.height = 141;
		fishingGage.current = 50;  // 초기값: 중간에서 시작
		fishingGage.maxVal = 100;

		break;

	case WM_PAINT:
		hDC = BeginPaint(hWnd, &ps);
		hMemDC = CreateCompatibleDC(hDC);
		SelectObject(hMemDC, hBitmap);

		if (isFishing)
			FishingGameLogic(hDC, hBrush, oldBrush, hPen, oldPen, hBitmap_fishing, greenBar, targetFish, fishingGage);

		DeleteDC(hMemDC);
		EndPaint(hWnd, &ps);
		break;

	case WM_LBUTTONDOWN:
	{
		if (isFishing) {
			floatingGreenBar = true; //마우스 버튼을 누르면 초록 게이지가 위로 올라가기 시작.
		}

		if (canFishing && !isFishing) {
			isFishing = true;

			// 낚시 시작 시 물고기 움직임 유형 결정
			targetFish.movementType = SelectFishMovementType();
			targetFish.moveTimer = 0;
			targetFish.moveInterval = 5 + rand() % 8;
			targetFish.moveDirY = (rand() % 2 == 0) ? 1 : -1;

			// 외부 게이지 초기화
			fishingGage.current = 50;

			SetTimer(hWnd, 2001, 100, NULL); // 낚시 전용 타이머 시작. 0.1초마다 WM_TIMER 메시지 발생
		}

		InvalidateRect(hWnd, NULL, TRUE);
		break;
	}

	case WM_LBUTTONUP:
	{
		floatingGreenBar = false; // 마우스 버튼을 떼면 초록 게이지가 더 이상 올라가지 않도록 설정
		InvalidateRect(hWnd, NULL, TRUE);
		break;
	}

	case WM_MOUSEMOVE:
	{

		break;
	}

	case WM_TIMER:
	{
		switch (wParam) { // 타이머 규칙 : 0XXX : 플레이어 관련 타이머, 1XXX : 농사 관련 타이머, 2XXX : 낚시 관련 타이머, 3XXX : 디펜스 관련 타이머
		case 2001: // 낚시 전용 타이머

			// 초록 게이지 위아래 이동
			if (floatingGreenBar) {
				greenBar.y -= 5; // 초록 게이지가 위로 올라감
				if (greenBar.y < 5) // 게이지가 너무 위로 올라가지 않도록 제한
					greenBar.y = 5;
			}
			else {
				greenBar.y += 5; // 초록 게이지가 아래로 내려감
				if (greenBar.y > 95) // 게이지가 너무 아래로 내려가지 않도록 제한
					greenBar.y = 95;
			}

			// 물고기 움직임 업데이트
			UpdateFishMovement(targetFish);

			// 물고기가 초록색 영역 안에 있는지 여부 업데이트
			targetFish.inGreenBar = CheckFishInGreenBar(targetFish, greenBar);

			// 외부 게이지 업데이트
			// 물고기가 초록 바 안: 게이지 상승 / 밖: 게이지 하락
			if (targetFish.inGreenBar) {
				fishingGage.current += 3;
				if (fishingGage.current > fishingGage.maxVal)
					fishingGage.current = fishingGage.maxVal;
			}
			else {
				fishingGage.current -= 2;
				if (fishingGage.current < 0)
					fishingGage.current = 0;
			}

			// 낚시 성공: 게이지가 꽉 찬 경우
			if (fishingGage.current >= fishingGage.maxVal) {
				KillTimer(hWnd, 2001);
				isFishing = false;
				floatingGreenBar = false;
				// TODO: 낚시 성공 처리
				// targetFish.movementType 에 따라 다른 아이템 드랍 처리 예정
				// ex) if (targetFish.movementType == FISH_MOVE_RANDOM) { /* 일반 물고기 아이템 */ }
				//     else if (targetFish.movementType == FISH_MOVE_FAST_UP) { /* 희귀 아이템 */ } 등
				MessageBox(hWnd, TEXT("낚시 성공!"), TEXT("낚시"), MB_OK);
			}

			// 낚시 실패: 게이지가 다 닳은 경우
			if (fishingGage.current <= 0) {
				KillTimer(hWnd, 2001);
				isFishing = false;
				floatingGreenBar = false;
				// TODO: 낚시 실패 처리
				MessageBox(hWnd, TEXT("낚시 실패..."), TEXT("낚시"), MB_OK);
			}

			break;
		}
		InvalidateRect(hWnd, NULL, TRUE);
		break;
	}

	case WM_RBUTTONDOWN:
	{

		InvalidateRect(hWnd, NULL, TRUE);
		break;
	}

	case WM_KEYDOWN:
		switch (wParam) {
		}
		InvalidateRect(hWnd, NULL, TRUE);
		break;
	case WM_DESTROY:
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