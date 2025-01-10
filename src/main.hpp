#include <windows.h>
#include <shellapi.h>
#include "../resource.h"
#include "unify_status.hpp"

#define WM_USER_SHELLICON WM_USER + 1

HMENU hPopMenu;

HINSTANCE instance;
NOTIFYICONDATAA nid{};

UnifyStatus* driver;

bool stop_thread = false;

HANDLE driver_win_thread;
