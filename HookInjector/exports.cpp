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

  INJECTOR_EXPORTS void INJECTOR_API Inject(int process_id, const wchar_t* dll_path_pointer)
  {
    try
    {
      const std::wstring dll_path(dll_path_pointer);
      scoped_handle process(OpenProcess(PROCESS_ALL_ACCESS, FALSE, process_id));
      if (!process)
      {
        throw runtime_error(L"プロセスを開くことができませんでした。");
      }

      auto kernel32 = GetModuleHandle(L"kernel32");
      if (kernel32 == INVALID_HANDLE_VALUE)
      {
        throw runtime_error(L"kernel32.dllを開けませんでした。");
      }

      auto load_library = GetProcAddress(kernel32, "LoadLibraryW");
      if (load_library == INVALID_HANDLE_VALUE)
      {
        throw runtime_error(L"LoadLibraryW関数のアドレス取得に失敗しました。");
      }

      auto free_library(GetProcAddress(kernel32, "FreeLibrary"));
      if (free_library == INVALID_HANDLE_VALUE)
      {
        throw runtime_error(L"FreeLibrary関数のアドレス取得に失敗しました。");
      }

      std::unique_ptr<void, virtual_free> dll_path_memory(
        VirtualAllocEx(process.get(), NULL, dll_path.size() * sizeof(wchar_t), MEM_COMMIT, PAGE_READWRITE),
        virtual_free(process.get(), dll_path.size() * sizeof(wchar_t))
      );
      if (!dll_path_memory)
      {
        throw runtime_error(L"対象プロセス上にDLLパス文字列領域を確保できませんでした。");
      }

      auto succeed = WriteProcessMemory(process.get(), dll_path_memory.get(), dll_path.c_str(), dll_path.size() * sizeof(wchar_t), NULL);
      if (succeed == FALSE)
      {
        throw runtime_error(L"対象プロセスのDLLパス文字列領域に書き込めませんでした。");
      }

      scoped_handle remote_thread(
        CreateRemoteThread(
          process.get(),
          NULL,
          0,
          (LPTHREAD_START_ROUTINE)load_library,
          dll_path_memory.get(),
          0,
          NULL
        )
      );
      if (!remote_thread)
      {
        throw runtime_error(L"対象プロセス上でのLoadLibrary実行に失敗しました。");
      }

      WaitForSingleObject(remote_thread.get(), INFINITE);
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
      scoped_handle process(OpenProcess(PROCESS_ALL_ACCESS, FALSE, process_id));
      if (!process)
      {
        throw runtime_error(L"プロセスを開くことができませんでした。");
      }

      auto kernel32 = GetModuleHandle(L"kernel32");
      if (kernel32 == NULL)
      {
        throw runtime_error(L"kernel32.dllを開けませんでした。");
      }

      auto free_library = GetProcAddress(kernel32, "FreeLibrary");
      if (!free_library)
      {
        throw runtime_error(L"FreeLibrary関数のアドレス取得に失敗しました。");
      }

      scoped_handle mapping_handle(
        OpenFileMapping(
          FILE_MAP_READ,
          TRUE,
          (L"WasapiHook_dll_handle_"
            + std::to_wstring(process_id)
            ).c_str()
        )
      );
      if (!mapping_handle)
      {
        throw runtime_error(L"InjectしたDLLのハンドルを渡すメモリマップへの接続に失敗しました。");
      }

      auto dll_handle_pointer = reinterpret_cast<HANDLE*>(
        MapViewOfFile(
          mapping_handle.get(),
          FILE_MAP_READ, 0, 0, sizeof(HANDLE)
        )
        );
      if (dll_handle_pointer == NULL)
      {
        throw runtime_error(L"InjectしたDLLのハンドルを取得できませんでした。" + std::to_wstring(GetLastError()));
      }

      auto dll_handle = *dll_handle_pointer;

      auto succeed = UnmapViewOfFile(dll_handle_pointer);
      if (succeed == FALSE)
      {
        throw runtime_error(L"メモリマップを閉じることができませんでした。");
      }

      scoped_handle remote_thread_free_library(
        CreateRemoteThread(
          process.get(),
          NULL,
          0,
          (LPTHREAD_START_ROUTINE)free_library,
          dll_handle,
          0,
          NULL
        )
      );
      WaitForSingleObject(remote_thread_free_library.get(), INFINITE);
    }
    catch (const runtime_error& e)
    {
      OutputDebugString(e.get_message().c_str());
    }
  }
}