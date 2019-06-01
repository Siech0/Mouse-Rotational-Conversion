/*
Author: Gavin Dunlap [Siech0]
Purpose: Translate A/D keyboard input into mouse input, tracing a circular pattern. (For a game)
Usage: 
	ALT+Q quits
	ALT+R initiates circle specification phase
	After ALT+R the next two left-mouse button down-presses will do the following
		1) Determine the center point of the circle
		2) Determine the radius of the circle
	After the circle information has been input, the A and D keys will now cause the mouse to trace the circle.
*/

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <vector>
#include <cstdlib>
#include <cmath>

enum MousePrepStatus
{
	PrepareCenter,
	PrepareRadius,
	BoundsDefined,
	Invalid
};

enum RotationDirection
{
	DirectionLeft,
	DirectionRight
};

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
DWORD WINAPI RotateFunction(void *lpParameter);
void Quit(HWND hWnd);

RAWINPUTDEVICE inputDevices[2];
HANDLE rotateThread = nullptr;

POINT center = {};
unsigned int radius = 0;
MousePrepStatus mousePrep = MousePrepStatus::Invalid;
constexpr int HOTKEY_REGISTER = 6030, HOTKEY_QUIT = 6031;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	//Prepare and register Window Class
	WNDCLASSEX wc = {};
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APPLICATION));
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = "hiddenWindowClass";
	wc.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APPLICATION));

	if (!RegisterClassEx(&wc))
	{
		MessageBox(nullptr, "ERROR: Window registration failed!", "ERROR!", 0);
		return EXIT_FAILURE;
	}

	//Create Window
	HWND hWnd = CreateWindow(
		"hiddenWindowClass",
		"hiddenWindow",
		0,
		CW_USEDEFAULT, CW_USEDEFAULT,
		0, 0,
		HWND_MESSAGE,
		nullptr,
		hInstance,
		nullptr
	);

	if (!hWnd)
	{
		MessageBox(nullptr, "ERROR: Window creation failed!", "ERROR!", 0);
		return EXIT_FAILURE;
	}

	//Register Control Hotkeys
	if (!RegisterHotKey(hWnd, HOTKEY_QUIT, MOD_ALT | MOD_NOREPEAT, 0x51))
	{ //Register ALT+Q
		MessageBox(nullptr, "Error: ALT+Q Hotkey registration failed!", "ERROR", 0);
		return EXIT_FAILURE;
	}
	if (!RegisterHotKey(hWnd, HOTKEY_REGISTER, MOD_ALT | MOD_NOREPEAT, 0x52))
	{ //Register ALT+R
		MessageBox(nullptr, "Error: ALT+R Hotkey registration failed!", "ERROR", 0);
		return EXIT_FAILURE;
	}

	//Prepare input controls
	inputDevices[0].usUsagePage = 0x01;
	inputDevices[0].usUsage = 0x02;
	inputDevices[0].dwFlags = RIDEV_INPUTSINK | RIDEV_NOLEGACY;
	inputDevices[0].hwndTarget = hWnd;

	inputDevices[1].usUsagePage = 0x01;
	inputDevices[1].usUsage = 0x06;
	inputDevices[1].dwFlags = RIDEV_INPUTSINK | RIDEV_NOLEGACY;
	inputDevices[1].hwndTarget = hWnd;

	if (RegisterRawInputDevices(inputDevices, 2, sizeof(inputDevices[0])) == false)
	{
		MessageBox(nullptr, "Error: Unable to register raw input devices!", "ERROR", 0);
		return EXIT_FAILURE;
	}

	//Message loop
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return (int)msg.wParam;
}

void Quit(HWND hWnd)
{
	UnregisterHotKey(hWnd, HOTKEY_REGISTER);
	UnregisterHotKey(hWnd, HOTKEY_QUIT);
	UnregisterClass("hiddenWindowClass", GetModuleHandle(nullptr));
	PostQuitMessage(0);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{

	case WM_DESTROY:
	{
		Quit(hWnd);
	}break;

	case WM_HOTKEY:
	{
		if (wParam == HOTKEY_REGISTER)
		{
			OutputDebugString("ALT+R\n");
			mousePrep = MousePrepStatus::PrepareCenter;
		}
		else if (wParam == HOTKEY_QUIT)
		{
			OutputDebugString("ALT+Q\n");
			Quit(hWnd);
		}
	}break;

	case WM_INPUT:
	{
		unsigned int dwSize;
		GetRawInputData((HRAWINPUT)lParam, RID_INPUT, nullptr, &dwSize, sizeof(RAWINPUTHEADER));
		std::vector<BYTE> lpb(dwSize);
		
		if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb.data(), &dwSize, sizeof(RAWINPUTHEADER)) != dwSize)
			OutputDebugString("GetRawInputData did not return correct size! \n");
		RAWINPUT* raw = (RAWINPUT *)lpb.data();

		if (raw->header.dwType == RIM_TYPEKEYBOARD)
		{
			const auto& kb = raw->data.keyboard; //Cleans up code a bit, should be optimized out.
			if (mousePrep == MousePrepStatus::BoundsDefined) //Center and radius are initialized
			{
				if (kb.Message == WM_KEYDOWN) //key down
				{
					if (kb.VKey == 0x41) //A
					{
						if (rotateThread == nullptr) //Thread is running, possible 2 keys down
							rotateThread = CreateThread(nullptr, 0, RotateFunction, (void *)RotationDirection::DirectionLeft, 0, nullptr);
					}
					else if (kb.VKey == 0x44) //D
					{
						if (rotateThread == nullptr)
							rotateThread = CreateThread(nullptr, 0, RotateFunction, (void *)RotationDirection::DirectionRight , 0, nullptr);
					}
				}
				else if (kb.Message == WM_KEYUP && (kb.VKey == 0x41 || kb.VKey == 0x44)) //One of our keys is now up
				{
					TerminateThread(rotateThread, 0);
					rotateThread = nullptr;
				}
			}
		}
		else if (raw->header.dwType == RIM_TYPEMOUSE)
		{
			const auto& mouse = raw->data.mouse; //Alias in order to clean up co
			if (mouse.usButtonFlags == RI_MOUSE_LEFT_BUTTON_DOWN) //Left click down
			{
				POINT pos;
				GetCursorPos(&pos);
				if (mousePrep == MousePrepStatus::PrepareCenter)
				{
					POINT pos;
					GetCursorPos(&pos);

					center.x = pos.x;
					center.y = pos.y;
					mousePrep = MousePrepStatus::PrepareRadius;
				}
				else if (mousePrep == MousePrepStatus::PrepareRadius)
				{
					POINT pos;
					GetCursorPos(&pos);

					const LONG xDistRaw = center.x - pos.x; //Simply to get rid of code-soup
					const LONG yDistRaw = center.y - pos.y;
					radius = sqrt(xDistRaw*xDistRaw + yDistRaw*yDistRaw);
					mousePrep = MousePrepStatus::BoundsDefined;
				}
			}
		}
	}break;

	default:
	{
		return DefWindowProc(hWnd, message, wParam, lParam);
	}break;

	}
	return 0;
}



DWORD WINAPI RotateFunction(void *lpParameter)
{
	POINT sPos; //Starting position
	if (!GetCursorPos(&sPos))
		return 0;

	static constexpr double degree = 0.1745; //1 Degree in rads
	double currentAngle = std::atan2((sPos.y - center.y), (sPos.x - center.x));
	long long dir = ((long long)lpParameter);

	while (true)
	{
		if (dir == RotationDirection::DirectionRight)
			currentAngle += degree / 2;
		else if (dir == RotationDirection::DirectionLeft)
			currentAngle -= degree / 2;

		SetCursorPos(center.x + radius*std::cos(currentAngle), center.y + radius*std::sin(currentAngle));
		Sleep(20);
	}

}
