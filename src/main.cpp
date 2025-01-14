#include "main.hpp"
#include <string>
#include <thread>

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam){
	int wmId, wmEvent;
    POINT lpClickPoint;

	switch (uMsg) {
		case WM_USER_SHELLICON:
			switch (LOWORD(lParam)) {
				case WM_RBUTTONDOWN:
				GetCursorPos(&lpClickPoint);
				SetForegroundWindow(hWnd);
				hPopMenu = LoadMenuA(instance, (LPCSTR)MAKEINTRESOURCEA(IDR_MENU1));
				HMENU popup = GetSubMenu(hPopMenu, 0);
				MENUITEMINFOA info{};
				info.cbSize = sizeof(info);
				info.fMask = MIIM_ID | MIIM_TYPE | MIIM_DATA;
				std::string title;
				for (int i = 0; i < driver->devices_info.size(); ++i) {
					if (driver->devices_info[i].name != "") {
						info.wID = 40000 + i;
						title = driver->devices_info[i].name + " - " + driver->status_to_string.at(driver->devices_info[i].status);
						info.dwTypeData = (LPSTR)title.c_str();
						info.cch = title.length();
						InsertMenuItemA(popup, i, TRUE, &info);
					}
				}
				TrackPopupMenu(popup,TPM_LEFTALIGN|TPM_LEFTBUTTON|TPM_BOTTOMALIGN,lpClickPoint.x, lpClickPoint.y,0,hWnd,NULL);
				DestroyMenu(hPopMenu);
				return true; 
			}
			break;
		case WM_COMMAND:
			wmId    = LOWORD(wParam);
			wmEvent = HIWORD(wParam);
			switch (wmId) {
				case ID_RELOAD:
					// driver will exit,
					// then the thread loop will set quit to false,
					// then run the driver again
					driver->quit = true;
					CancelSynchronousIo(driver_win_thread);
					break;
				case ID_EXIT:
					Shell_NotifyIconA(NIM_DELETE, &nid);
					DestroyWindow(hWnd);
					break;
				default:
					return DefWindowProc(hWnd, uMsg, wParam, lParam);
			}
			break;
		case WM_DESTROY:
			PostQuitMessage(0);
			break;
		default:
			return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}
	return false;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
	instance = hInstance;
	std::thread driver_thread([&]() {
		// Get the windows handle of the driver thread,
		// this allows the main thread to cancel pending io operations
		DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &driver_win_thread, NULL, FALSE, DUPLICATE_CLOSE_SOURCE | DUPLICATE_SAME_ACCESS);
		while (!stop_thread) {
			driver = new UnifyStatus();
			driver->run();
			delete driver;
		}
		});

	const std::string tooltip = "Logitech Unify MQTT";
	WNDCLASSA wc{};
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = tooltip.c_str();
	RegisterClassA(&wc);

	HWND hWnd;
	hWnd = CreateWindowA(tooltip.c_str(), tooltip.c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 300, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);
	HICON icon = LoadIconA(hInstance, (LPCSTR)MAKEINTRESOURCEA(IDI_ICON1));

	nid.cbSize = sizeof(NOTIFYICONDATAA);
	nid.hWnd = hWnd;
	nid.uID = 0;
	nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	nid.hIcon = icon;
	nid.uCallbackMessage = WM_USER_SHELLICON;
	std::memcpy(nid.szTip, tooltip.c_str(), tooltip.length());
	Shell_NotifyIconA(NIM_ADD, &nid);

	MSG msg{};
	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);

		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
	
	stop_thread = true;
	driver->quit = true;
	// cancel any pending reads
	CancelSynchronousIo(driver_win_thread);
	driver_thread.join();
	DestroyIcon(icon);
	DestroyWindow(hWnd);
	UnregisterClassA(tooltip.c_str(), hInstance);

}
