// WeaselServer.cpp : main source file for WeaselServer.exe
//
//	WTL MessageLoop 封装了消息循环. 实现了 getmessage/dispatchmessage....

#include "stdafx.h"
#include "resource.h"
#include "WeaselService.h"
#include <WeaselIPC.h>
#include <WeaselUI.h>
#include <RimeWithWeasel.h>
#include <WeaselUtility.h>
#include <winsparkle.h>
#include <functional>
#include <ShellScalingApi.h>
#include <WinUser.h>
#include <memory>
#include <atlstr.h>
#pragma comment(lib, "Shcore.lib")
CAppModule _Module;

int WINAPI _tWinMain(HINSTANCE hInstance,
                     HINSTANCE /*hPrevInstance*/,
                     LPTSTR lpstrCmdLine,
                     int nCmdShow) {
  DEBUG << "启动了: " << GetCurrentProcessId();
  LANGID langId = get_language_id();
  SetThreadUILanguage(langId);
  SetThreadLocale(langId);

  if (!IsWindowsBlueOrLaterEx()) {
    CString info, cap;
    info.LoadStringW(IDS_STR_SYSTEM_VERSION_WARNING);
    cap.LoadStringW(IDS_STR_SYSTEM_VERSION_WARNING_CAPTION);
    MessageBoxExW(NULL, info, cap, MB_ICONERROR, langId);
    return 0;
  }
  SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);

  // 防止服务进程开启输入法
  ImmDisableIME(-1);

  WCHAR user_name[20] = {0};
  DWORD size = _countof(user_name);
  GetUserName(user_name, &size);
  if (!_wcsicmp(user_name, L"SYSTEM")) {
    return 1;
  }

  HRESULT hRes = ::CoInitialize(NULL);
  // If you are running on NT 4.0 or higher you can use the following call
  // instead to make the EXE free threaded. This means that calls come in on a
  // random RPC thread.
  // HRESULT hRes = ::CoInitializeEx(NULL, COINIT_MULTITHREADED);
  ATLASSERT(SUCCEEDED(hRes));

  // this resolves ATL window thunking problem when Microsoft Layer for Unicode
  // (MSLU) is used
  ::DefWindowProc(NULL, 0, 0, 0L);

  AtlInitCommonControls(
      ICC_BAR_CLASSES);  // add flags to support other controls

  hRes = _Module.Init(NULL, hInstance);
  ATLASSERT(SUCCEEDED(hRes));

  if (!wcscmp(L"/userdir", lpstrCmdLine)) {
    CreateDirectory(WeaselUserDataPath().c_str(), NULL);
    WeaselServerApp::explore(WeaselUserDataPath());
    return 0;
  }
  if (!wcscmp(L"/weaseldir", lpstrCmdLine)) {
    WeaselServerApp::explore(WeaselServerApp::install_dir());
    return 0;
  }
  if (!wcscmp(L"/ascii", lpstrCmdLine) || !wcscmp(L"/nascii", lpstrCmdLine)) {
    weasel::Client client;
    bool ascii = !wcscmp(L"/ascii", lpstrCmdLine);
    if (client.Connect())  // try to connect to running server
    {
      if (ascii)
        client.TrayCommand(ID_WEASELTRAY_ENABLE_ASCII);
      else
        client.TrayCommand(ID_WEASELTRAY_DISABLE_ASCII);
    }
    return 0;
  }

  // command line option /q stops the running server
  bool quit = !wcscmp(L"/q", lpstrCmdLine) || !wcscmp(L"/quit", lpstrCmdLine);
  // restart if already running
  {
    DEBUG << "检查是否有运行中的服务器 PID: " << GetCurrentProcessId();
    weasel::Client client;
    if (client.Connect())  // try to connect to running server
    {
      DEBUG << "连接到现有服务器成功，正在关闭 PID: " << GetCurrentProcessId();
      client.ShutdownServer();
      if (quit) {
        DEBUG << "退出命令，直接返回 PID: " << GetCurrentProcessId();
        return 0;
      }
      int retry = 0;
      while (client.Connect() && retry < 10) {
        DEBUG << "重试关闭服务器，第 " << retry + 1
              << " 次 PID: " << GetCurrentProcessId();
        client.ShutdownServer();
        retry++;
        Sleep(50);
      }
      if (retry >= 10) {
        DEBUG << "重试次数过多，放弃启动 PID: " << GetCurrentProcessId();
        return 0;
      }
      DEBUG << "成功关闭现有服务器，继续启动新实例 PID: "
            << GetCurrentProcessId();
    } else if (quit) {
      DEBUG << "没有运行中的服务器且为退出命令 PID: " << GetCurrentProcessId();
      return 0;
    } else {
      DEBUG << "没有运行中的服务器，继续启动 PID: " << GetCurrentProcessId();
    }
  }

  bool check_updates = !wcscmp(L"/update", lpstrCmdLine);
  if (check_updates) {
    WeaselServerApp::check_update();
  }

  CreateDirectory(WeaselUserDataPath().c_str(), NULL);

  int nRet = 0;
  try {
    DEBUG << "创建 WeaselServerApp 实例 PID: " << GetCurrentProcessId();
    WeaselServerApp app;
    RegisterApplicationRestart(NULL, 0);
    DEBUG << "开始运行 WeaselServerApp PID: " << GetCurrentProcessId();
    nRet = app.Run();
    DEBUG << "WeaselServerApp 运行结束，返回值: " << nRet
          << " PID: " << GetCurrentProcessId();
  } catch (...) {
    // bad luck...
    DEBUG << "WeaselServerApp 运行异常 PID: " << GetCurrentProcessId();
    nRet = -1;
  }

  _Module.Term();
  ::CoUninitialize();

  DEBUG << "退出了: " << GetCurrentProcessId();
  return nRet;
}
