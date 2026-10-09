#ifndef RUNNER_CURRENT_WEBSITE_H_
#define RUNNER_CURRENT_WEBSITE_H_

#include <windows.h>
#include <flutter/encodable_value.h>
#include <flutter/method_channel.h>

#include <memory>
#include <mutex>
#include <string>
#include <thread>

// Reads only the captured browser's chrome, never page content or the clipboard.
class CurrentWebsiteReader {
 public:
  CurrentWebsiteReader(flutter::BinaryMessenger* messenger, HWND window);
  ~CurrentWebsiteReader();
  bool HandleMessage(UINT message);
  CurrentWebsiteReader(const CurrentWebsiteReader&) = delete;
  CurrentWebsiteReader& operator=(const CurrentWebsiteReader&) = delete;

 private:
  static void CALLBACK ForegroundChanged(HWINEVENTHOOK hook, DWORD event,
                                         HWND window, LONG object, LONG child,
                                         DWORD thread, DWORD time);
  static CurrentWebsiteReader* instance_;
  void CaptureBrowser();
  void ReadURL(std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result);

  HWND window_;
  HWND last_foreground_ = nullptr;
  HWND captured_browser_ = nullptr;
  DWORD captured_process_ = 0;
  HWINEVENTHOOK foreground_hook_ = nullptr;
  std::unique_ptr<flutter::MethodChannel<flutter::EncodableValue>> channel_;
  std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> pending_result_;
  std::thread worker_;
  std::mutex mutex_;
  std::string url_;
};

#endif  // RUNNER_CURRENT_WEBSITE_H_
