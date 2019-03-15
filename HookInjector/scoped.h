#pragma once
#include <Windows.h>

class scoped_handle
{
public:
  scoped_handle() {}

  explicit scoped_handle(const HANDLE& handle)
    : handle(handle)
  {}

  ~scoped_handle()
  {
    CloseHandle(handle);
  }

  HANDLE get()
  {
    return handle;
  }

  void operator =(const HANDLE& rhs)
  {
    handle = rhs;
  }

  bool operator !()
  {
    return handle == INVALID_HANDLE_VALUE || handle == NULL;
  }

private:
  HANDLE handle = INVALID_HANDLE_VALUE;
};

class virtual_free
{
public:
  constexpr virtual_free() noexcept = default;

  virtual_free(HANDLE process, SIZE_T size)
    : process(process), size(size)
  {}

  void operator ()(void* ptr)
  {
    VirtualFreeEx(process, ptr, size, MEM_RELEASE);
  }

private:
  HANDLE process = INVALID_HANDLE_VALUE;
  SIZE_T size = 0;
};
