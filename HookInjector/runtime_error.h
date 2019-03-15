#pragma once

#include <stdexcept>

#define THROW_ON_FAILURE(hr, message) \
  if (FAILED(hr)) { \
    throw runtime_error(message); \
  }

class runtime_error : public std::runtime_error
{
public:
  runtime_error(const std::wstring& message);
  std::wstring get_message() const;

private:
  std::wstring message;
};
