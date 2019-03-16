#include "../HookInjector/runtime_error.h"
#include <Windows.h>

#define NOMINMAX
#include <windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <wrl/client.h>
#include <functional>
#include <stdexcept>
#include <string>
#include <algorithm>
#include <cmath>

using Microsoft::WRL::ComPtr;

typedef HRESULT(STDMETHODCALLTYPE *get_buffer_pointer)(
  IAudioCaptureClient *this_pointer,
  BYTE **data,
  UINT32 *num_frames_to_read,
  DWORD *flags,
  UINT64 *device_position,
  UINT64 *QPC_position
  );
typedef HRESULT(STDMETHODCALLTYPE *get_next_packet_size_pointer)(
  IAudioCaptureClient *this_pointer,
  UINT32 *num_frames_in_next_packet
  );
typedef HRESULT(STDMETHODCALLTYPE *release_buffer_pointer)(
  IAudioCaptureClient *this_pointer,
  UINT32 num_frames_read
  );

enum class sample_type
{
  undefined,
  int16,
  float32,
};

static sample_type audio_sample_type = sample_type::int16;
static get_buffer_pointer get_buffer_original;
static get_next_packet_size_pointer get_next_packet_size_original;
static release_buffer_pointer release_buffer_original;
static ComPtr<IAudioCaptureClient> capture_client;
static int num_channels;
static int sampling_rate = -1;
static REFERENCE_TIME default_period;
static float phase = 0.0f;
static const float pi = 3.141592653589793f;
static const float magic_frequency = 20.0f;
static const float magic_amplitude = 0.2f;
HRESULT get_buffer_hook(
  IAudioCaptureClient *this_pointer,
  BYTE **data,
  UINT32 *num_frames_to_read,
  DWORD *flags,
  UINT64 *device_position,
  UINT64 *QPC_position
)
{
  auto hr = get_buffer_original(this_pointer, data, num_frames_to_read, flags, device_position, QPC_position);

  if (sampling_rate < 0)
  {
    sampling_rate = static_cast<int>((*num_frames_to_read) / (default_period / 10000000.0));
  }
  if (audio_sample_type == sample_type::undefined)
  {
    auto float_data = *(reinterpret_cast<float**>(data));
    for (unsigned int sample_index = 0; sample_index < (*num_frames_to_read) * num_channels / (sizeof(float) / sizeof(int16_t)); sample_index += num_channels)
    {
      auto float_sample = float_data[sample_index];
      if (std::abs(float_sample) > 1.1f)
      {
        audio_sample_type = sample_type::int16;
      }
    }
    if (audio_sample_type == sample_type::undefined)
    {
      audio_sample_type = sample_type::float32;
    }
  }

  if (audio_sample_type == sample_type::int16)
  {
    auto* int16_data = *(reinterpret_cast<int16_t**>(data));
    for (unsigned int sample_index = 0; sample_index < (*num_frames_to_read) * num_channels; sample_index += num_channels)
    {
      phase += magic_frequency * 2.0f * pi / sampling_rate;
      phase = std::fmod(phase, 2.0f * pi);
      for (int channel_index = 0; channel_index < num_channels; channel_index++)
      {
        int16_data[sample_index + channel_index] += static_cast<int16_t>((1 << 15) * magic_amplitude * std::sin(phase));
      }
    }
  }
  else if (audio_sample_type == sample_type::float32)
  {
    auto* float_data = *(reinterpret_cast<float**>(data));
    for (unsigned int sample_index = 0; sample_index < (*num_frames_to_read) * num_channels; sample_index += num_channels)
    {
      phase += magic_frequency * 2.0f * pi / sampling_rate;
      phase = std::fmod(phase, 2.0f * pi);
      for (int channel_index = 0; channel_index < num_channels; channel_index++)
      {
        float_data[sample_index + channel_index] += static_cast<float>(magic_amplitude * std::sin(phase));
      }
    }
  }

  return hr;
}

HRESULT get_next_packet_size_hook(
  IAudioCaptureClient *this_pointer,
  UINT32 *num_frames_in_next_packet
)
{
  return get_next_packet_size_original(this_pointer, num_frames_in_next_packet);
}

HRESULT release_buffer_hook(
  IAudioCaptureClient *this_pointer,
  UINT32 num_frames_read
)
{
  return release_buffer_original(this_pointer, num_frames_read);
}

void on_attach(HINSTANCE hinstance)
{
  auto current_process_id = GetCurrentProcessId();

  // Get audio render client
  ComPtr<IMMDeviceEnumerator> enumerator;
  auto hr = CoCreateInstance(
    __uuidof(MMDeviceEnumerator),
    NULL,
    CLSCTX_ALL,
    IID_PPV_ARGS(&enumerator)
  );
  ComPtr<IMMDevice> device;
  hr = enumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &device);
  ComPtr<IAudioClient> audio_client;
  hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, &audio_client);
  WAVEFORMATEX *format;
  hr = audio_client->GetMixFormat(&format);
  REFERENCE_TIME minimum_period;
  hr = audio_client->GetDevicePeriod(&default_period, &minimum_period);
  hr = audio_client->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, default_period, 0, format, NULL);
  hr = audio_client->GetService(IID_PPV_ARGS(&capture_client));

  num_channels = format->nChannels;

  // replace audio render client vtable
  {
    DWORD old_protect;
    auto virtual_protect_retval = VirtualProtect(*reinterpret_cast<PVOID**>(capture_client.Get()), sizeof(LONG_PTR), PAGE_EXECUTE_READWRITE, &old_protect);
    if (virtual_protect_retval == FALSE)
    {
      return;
    }

    auto vtable = *reinterpret_cast<PVOID**>(capture_client.Get());
    get_buffer_original = reinterpret_cast<get_buffer_pointer>(vtable[3]);
    release_buffer_original = reinterpret_cast<release_buffer_pointer>(vtable[4]);
    get_next_packet_size_original = reinterpret_cast<get_next_packet_size_pointer>(vtable[5]);
    vtable[3] = get_buffer_hook;
    vtable[4] = release_buffer_hook;
    vtable[5] = get_next_packet_size_hook;
    VirtualProtect(*reinterpret_cast<PVOID**>(capture_client.Get()), sizeof(LONG_PTR), old_protect, &old_protect);
  }
}

void on_detach()
{
  DWORD old_protect;
  auto virtual_protect_retval = VirtualProtect(*reinterpret_cast<PVOID**>(capture_client.Get()), sizeof(LONG_PTR), PAGE_EXECUTE_READWRITE, &old_protect);
  if (virtual_protect_retval == FALSE)
  {
    return;
  }

  auto vtable = *reinterpret_cast<PVOID**>(capture_client.Get());
  vtable[3] = get_buffer_original;
  vtable[4] = release_buffer_original;
  vtable[5] = get_next_packet_size_original;
  VirtualProtect(*reinterpret_cast<PVOID**>(capture_client.Get()), sizeof(LONG_PTR), old_protect, &old_protect);
}

BOOL WINAPI DllMain(
  HINSTANCE hinstance,
  DWORD reason,
  LPVOID reserved
)
{
  try
  {
    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
      on_attach(hinstance);
      break;
    case DLL_THREAD_ATTACH:
      break;
    case DLL_THREAD_DETACH:
      break;
    case DLL_PROCESS_DETACH:
      on_detach();
      break;
    }
  }
  catch (const runtime_error&)
  {
    return FALSE;
  }

  return TRUE;
}

extern "C"
{
  __declspec(dllexport) LRESULT CALLBACK hook_procedure(int code, WPARAM wparam, LPARAM lparam)
  {
    auto message = reinterpret_cast<MSG*>(lparam);
    if (message->message == (WM_APP + 1))
    {
      UnhookWindowsHookEx(reinterpret_cast<HHOOK>(lparam));
    }
    return CallNextHookEx(0, code, wparam, lparam);
  }
}
