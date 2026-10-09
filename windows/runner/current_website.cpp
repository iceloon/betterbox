#include "current_website.h"

#include <UIAutomation.h>
#include <wrl/client.h>
#include <flutter/standard_method_codec.h>

#include <cwchar>
#include <deque>
#include <utility>

namespace {
using Microsoft::WRL::ComPtr;
constexpr UINT kWebsiteReadComplete = WM_APP + 173;

bool IsTrayWindow(HWND window) {
  wchar_t name[128] = {};
  GetClassNameW(window, name, 128);
  return wcscmp(name, L"Shell_TrayWnd") == 0 ||
         wcscmp(name, L"Shell_SecondaryTrayWnd") == 0 ||
         wcscmp(name, L"NotifyIconOverflowWindow") == 0 ||
         wcscmp(name, L"TopLevelWindowForOverflowXamlIsland") == 0;
}

bool IsSupportedBrowser(HWND window, DWORD* process_id) {
  if (!IsWindow(window)) return false;
  wchar_t class_name[128] = {};
  GetClassNameW(window, class_name, 128);
  if (wcscmp(class_name, L"Chrome_WidgetWin_1") != 0) return false;
  GetWindowThreadProcessId(window, process_id);
  HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, *process_id);
  if (!process) return false;
  wchar_t path[32768] = {};
  DWORD size = 32768;
  const BOOL found = QueryFullProcessImageNameW(process, 0, path, &size);
  CloseHandle(process);
  if (!found) return false;
  const wchar_t* name = wcsrchr(path, L'\\');
  if (!name) return false;
  return _wcsicmp(name + 1, L"chrome.exe") == 0 ||
         _wcsicmp(name + 1, L"msedge.exe") == 0;
}

std::wstring GetPropertyString(IUIAutomationElement* element, PROPERTYID id) {
  VARIANT value;
  VariantInit(&value);
  std::wstring text;
  if (SUCCEEDED(element->GetCurrentPropertyValue(id, &value)) &&
      value.vt == VT_BSTR && value.bstrVal) {
    text.assign(value.bstrVal, SysStringLen(value.bstrVal));
  }
  VariantClear(&value);
  return text;
}

bool IsAddressBar(IUIAutomationElement* element) {
  const auto id = GetPropertyString(element, UIA_AutomationIdPropertyId);
  if (id == L"addressEditBox" || id == L"view_omnibox") return true;
  const auto name = GetPropertyString(element, UIA_NamePropertyId);
  // Do not guess from arbitrary editable fields. Unknown localizations fall
  // back to manual URL input. Unicode escapes keep the native source ASCII.
  return name == L"Address and search bar" || name == L"Address bar" ||
         name == L"Search or enter web address" ||
         name == L"\u5730\u5740\u548c\u641c\u7d22\u680f" ||
         name == L"\u5730\u5740\u680f" ||
         name == L"\u5730\u5740\u8207\u641c\u5c0b\u5217" ||
         name == L"\u5730\u5740\u548c\u641c\u5c0b\u5217";
}

std::string ReadAddressBar(HWND window) {
  ComPtr<IUIAutomation> automation;
  if (FAILED(CoCreateInstance(CLSID_CUIAutomation8, nullptr, CLSCTX_INPROC_SERVER,
                              IID_PPV_ARGS(&automation))) &&
      FAILED(CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER,
                              IID_PPV_ARGS(&automation)))) return {};
  ComPtr<IUIAutomation2> automation2;
  if (SUCCEEDED(automation.As(&automation2))) {
    automation2->put_ConnectionTimeout(1000);
    automation2->put_TransactionTimeout(1000);
  }
  ComPtr<IUIAutomationElement> root;
  ComPtr<IUIAutomationTreeWalker> walker;
  if (FAILED(automation->ElementFromHandle(window, &root)) ||
      FAILED(automation->get_ControlViewWalker(&walker))) return {};

  std::deque<std::pair<ComPtr<IUIAutomationElement>, int>> queue;
  queue.emplace_back(root, 0);
  int visited = 0;
  const ULONGLONG deadline = GetTickCount64() + 5000;
  while (!queue.empty() && visited++ < 256 && GetTickCount64() < deadline) {
    auto entry = std::move(queue.front());
    queue.pop_front();
    CONTROLTYPEID type = 0;
    if (FAILED(entry.first->get_CurrentControlType(&type))) continue;
    // Never enter the web document, DevTools contents or page form fields.
    if (type == UIA_DocumentControlTypeId) continue;
    if (type == UIA_EditControlTypeId && IsAddressBar(entry.first.Get())) {
      BOOL offscreen = TRUE;
      if (FAILED(entry.first->get_CurrentIsOffscreen(&offscreen)) || offscreen) continue;
      ComPtr<IUIAutomationValuePattern> pattern;
      if (FAILED(entry.first->GetCurrentPatternAs(UIA_ValuePatternId,
                                                  IID_PPV_ARGS(&pattern)))) continue;
      BSTR value = nullptr;
      if (FAILED(pattern->get_CurrentValue(&value)) || !value) continue;
      const int length = static_cast<int>(SysStringLen(value));
      const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                                           value, length, nullptr, 0, nullptr, nullptr);
      std::string url(size > 0 ? size : 0, '\0');
      if (size > 0) WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                                        value, length, url.data(), size, nullptr, nullptr);
      SysFreeString(value);
      return url;
    }
    if (entry.second >= 12) continue;
    ComPtr<IUIAutomationElement> child;
    if (FAILED(walker->GetFirstChildElement(entry.first.Get(), &child))) continue;
    while (child && queue.size() < 256 && GetTickCount64() < deadline) {
      queue.emplace_back(child, entry.second + 1);
      ComPtr<IUIAutomationElement> sibling;
      if (FAILED(walker->GetNextSiblingElement(child.Get(), &sibling))) break;
      child = std::move(sibling);
    }
  }
  return {};
}
}  // namespace

CurrentWebsiteReader* CurrentWebsiteReader::instance_ = nullptr;

CurrentWebsiteReader::CurrentWebsiteReader(flutter::BinaryMessenger* messenger, HWND window)
    : window_(window) {
  instance_ = this;
  last_foreground_ = GetForegroundWindow();
  // Clicking the taskbar can foreground Explorer before the Dart tray callback.
  // Retain the preceding non-tray window, not an unrelated background browser.
  foreground_hook_ = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND,
      nullptr, ForegroundChanged, 0, 0, WINEVENT_OUTOFCONTEXT);
  channel_ = std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
      messenger, "betterbox/current_website", &flutter::StandardMethodCodec::GetInstance());
  channel_->SetMethodCallHandler([this](const auto& call, auto result) {
    if (call.method_name() == "captureBrowser") {
      CaptureBrowser();
      result->Success();
    } else if (call.method_name() == "readURL") {
      ReadURL(std::move(result));
    } else {
      result->NotImplemented();
    }
  });
}

CurrentWebsiteReader::~CurrentWebsiteReader() {
  if (foreground_hook_) UnhookWinEvent(foreground_hook_);
  instance_ = nullptr;
  if (worker_.joinable()) worker_.join();
  if (pending_result_) pending_result_->Error("browser_read_failed", "Reader closed");
}

void CALLBACK CurrentWebsiteReader::ForegroundChanged(HWINEVENTHOOK, DWORD,
    HWND window, LONG, LONG, DWORD, DWORD) {
  if (instance_ && window && !IsTrayWindow(window)) {
    instance_->last_foreground_ = window;
  }
}

void CurrentWebsiteReader::CaptureBrowser() {
  captured_browser_ = nullptr;
  captured_process_ = 0;
  HWND target = GetForegroundWindow();
  if (IsTrayWindow(target)) target = last_foreground_;
  DWORD process = 0;
  if (IsSupportedBrowser(target, &process)) {
    captured_browser_ = target;
    captured_process_ = process;
  }
}

void CurrentWebsiteReader::ReadURL(
    std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
  if (pending_result_) {
    result->Error("browser_read_failed", "Reader busy");
    return;
  }
  DWORD process = 0;
  if (!IsSupportedBrowser(captured_browser_, &process) || process != captured_process_) {
    result->Error("unsupported_browser", "Bring a Chrome or Edge window to the front");
    return;
  }
  const HWND browser = captured_browser_;
  captured_browser_ = nullptr;
  if (worker_.joinable()) worker_.join();
  pending_result_ = std::move(result);
  worker_ = std::thread([this, browser]() {
    const HRESULT initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    std::string url;
    if (SUCCEEDED(initialized)) {
      url = ReadAddressBar(browser);
      CoUninitialize();
    }
    {
      std::lock_guard<std::mutex> lock(mutex_);
      url_ = std::move(url);
    }
    PostMessageW(window_, kWebsiteReadComplete, 0, 0);
  });
}

bool CurrentWebsiteReader::HandleMessage(UINT message) {
  if (message != kWebsiteReadComplete) return false;
  if (!pending_result_) return true;
  if (worker_.joinable()) worker_.join();
  std::string url;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    url = std::move(url_);
  }
  if (url.empty()) {
    pending_result_->Error("browser_read_failed", "Could not read the browser address bar");
  } else {
    pending_result_->Success(flutter::EncodableValue(url));
  }
  pending_result_.reset();
  return true;
}
