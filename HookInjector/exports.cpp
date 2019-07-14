#define INJECTOR_API __cdecl
#define INJECTOR_EXPORTS __declspec(dllexport)

#include <Windows.h>
#include <Psapi.h>
#include <memory>
#include <string>
#include <vector>
#include "scoped.h"
#include "runtime_error.h"

namespace
{
  struct ProcessInfo
  {
    DWORD process_id;
    BSTR executable_name;   // 呼び出し側で解放する
  };
}

extern "C" {
  // 返り値が指すメモリ領域は呼び出し側で解放する
  INJECTOR_EXPORTS ProcessInfo* INJECTOR_API EnumerateProcessExecutables(uint32_t* process_count)
  {
    try
    {
      std::vector<DWORD> process_ids(1024);
      auto ids_needed_size = static_cast<DWORD>(process_ids.size() * sizeof(DWORD) / 2);

      do
      {
        process_ids.resize(ids_needed_size / sizeof(DWORD) * 2);
        auto succeed = EnumProcesses(process_ids.data(), static_cast<DWORD>(process_ids.size() * sizeof(DWORD)), &ids_needed_size);
        if (succeed == 0)
        {
          throw runtime_error(L"プロセス一覧の取得に失敗しました。");
        }
      } while (ids_needed_size / sizeof(DWORD) == process_ids.size());

      process_ids.resize(ids_needed_size / sizeof(DWORD));
      *process_count = static_cast<uint32_t>(process_ids.size());

      auto process_info_array = reinterpret_cast<ProcessInfo*>(CoTaskMemAlloc(sizeof(ProcessInfo) * process_ids.size()));

      for (auto process_index = 0; process_index < process_ids.size(); process_index++)
      {
        auto process_id = process_ids[process_index];
        auto& process_info = process_info_array[process_index];
        process_info.process_id = process_id;
        process_info.executable_name = nullptr;

        scoped_handle process(OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, process_id));
        if (!process)
        {
          process_info.executable_name = nullptr;
          continue;
        }

        std::vector<HMODULE> process_modules(128);
        auto modules_needed_size = static_cast<DWORD>(process_modules.size() * sizeof(HMODULE) / 2);
        do
        {
          process_modules.resize(modules_needed_size / sizeof(HMODULE) * 2);
          auto succeed = EnumProcessModules(
            process.get(),
            process_modules.data(),
            static_cast<DWORD>(process_modules.size() * sizeof(HMODULE)),
            &modules_needed_size
          );
          if (succeed == 0)
          {
            continue;
          }
        } while (modules_needed_size / sizeof(HMODULE) == process_modules.size());

        process_modules.resize(modules_needed_size / sizeof(HMODULE));

        for (auto module_index = 0; module_index < process_modules.size(); module_index++)
        {
          auto module_handle = process_modules[module_index];
          TCHAR module_name[MAX_PATH];
          auto succeed = GetModuleBaseName(process.get(), module_handle, module_name, sizeof(module_name) / sizeof(TCHAR));
          if (succeed == 0)
          {
            continue;
          }

          std::wstring module_name_string(module_name);
          if (module_name_string.find(L"exe") == std::wstring::npos)
          {
            continue;
          }

          process_info.executable_name = SysAllocStringLen(module_name, static_cast<UINT>(module_name_string.length()));
        }
      }

      return process_info_array;
    }
    catch (const runtime_error& e)
    {
      OutputDebugString(e.get_message().c_str());
      return nullptr;
    }
  }

  static DWORD target_thread_id;
  static HHOOK hook;

  BOOL window_enumerator(HWND window_handle, LPARAM lparam)
  {
    DWORD target_process_id = static_cast<DWORD>(lparam);
    DWORD thread_id, process_id;
    thread_id = GetWindowThreadProcessId(window_handle, &process_id);
    if (process_id == target_process_id)
    {
      target_thread_id = thread_id;
      return FALSE;
    }

    return TRUE;
  }

  INJECTOR_EXPORTS void INJECTOR_API Inject(int process_id, const wchar_t* dll_path_pointer)
  {
    try
    {
      EnumWindows(window_enumerator, process_id);
      auto wasapi_hook = LoadLibrary(dll_path_pointer);
      auto hook_procedure = reinterpret_cast<HOOKPROC>(GetProcAddress(wasapi_hook, "hook_procedure"));
      hook = SetWindowsHookEx(WH_GETMESSAGE, hook_procedure, wasapi_hook, target_thread_id);
      PostThreadMessage(target_thread_id, WM_APP, 0, (LPARAM)hook);
    }
    catch (const runtime_error& e)
    {
      OutputDebugString(e.get_message().c_str());
    }
  }

  INJECTOR_EXPORTS void INJECTOR_API Remove(int process_id, const wchar_t* dll_path)
  {
    try
    {
      PostThreadMessage(target_thread_id, WM_APP + 1, 0, (LPARAM)hook);
    }
    catch (const runtime_error& e)
    {
      OutputDebugString(e.get_message().c_str());
    }
  }

  INJECTOR_EXPORTS void INJECTOR_API SetInputMixVolume(int volume)
  {
    PostThreadMessage(target_thread_id, WM_APP + 2, 0, volume);
  }

  INJECTOR_EXPORTS void INJECTOR_API SetWhisperVolume(int volume)
  {
    PostThreadMessage(target_thread_id, WM_APP + 3, 0, volume);
  }

  INJECTOR_EXPORTS void INJECTOR_API SetNetduettoVolume(int volume)
  {
    PostThreadMessage(target_thread_id, WM_APP + 4, 0, volume);
  }
}
