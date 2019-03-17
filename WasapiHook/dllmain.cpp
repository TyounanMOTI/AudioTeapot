#include "../HookInjector/runtime_error.h"
#include <Windows.h>

#define NOMINMAX
#include <windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <wrl/client.h>
#include <functiondiscoverykeys_devpkey.h>
#include <functional>
#include <stdexcept>
#include <string>
#include <algorithm>
#include <cmath>
#include <deque>
#include <atomic>
#include <thread>
#include <mutex>
#include <Wmcodecdsp.h>
#include <mftransform.h>
#include <mfapi.h>

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

static get_buffer_pointer get_buffer_original;
static ComPtr<IAudioCaptureClient> capture_client;
static int num_channels;
static std::atomic<int> sampling_rate = -1;
static REFERENCE_TIME default_period;
static float phase = 0.0f;
static const float pi = 3.141592653589793f;
static const float magic_frequency = 20.0f;
static const float magic_amplitude = 0.2f;
static bool netduetto_buffer_prepared = false;
static HANDLE capture_event;
static ComPtr<IAudioClient> audio_client;
static std::thread capture_thread;
static std::atomic<bool> capture_stop_requested = false;
static std::mutex capture_buffer_mutex;
static std::deque<int16_t> capture_buffer;
static ComPtr<IMMDeviceEnumerator> enumerator;

static void capture()
{
  while (!capture_stop_requested)
  {
    auto wait_result = WaitForSingleObject(capture_event, 1000);
    switch (wait_result)
    {
    case WAIT_FAILED:
    case WAIT_TIMEOUT:
    case WAIT_ABANDONED:
      return;
    case WAIT_OBJECT_0:
      break;
    }

    UINT32 next_packet_size;
    auto hr = capture_client->GetNextPacketSize(&next_packet_size);
    while (next_packet_size != 0)
    {
      BYTE* data;
      UINT32 num_frames_to_read;
      DWORD flags;
      hr = get_buffer_original(capture_client.Get(), &data, &num_frames_to_read, &flags, nullptr, nullptr);

      auto int16_buffer = reinterpret_cast<int16_t*>(data);
      {
        std::scoped_lock<std::mutex> lock(capture_buffer_mutex);
        std::copy(int16_buffer, int16_buffer + num_frames_to_read * num_channels, std::back_inserter(capture_buffer));
      }

      hr = capture_client->ReleaseBuffer(num_frames_to_read);
      hr = capture_client->GetNextPacketSize(&next_packet_size);
    }
  }
}

HRESULT get_buffer_hook(
  IAudioCaptureClient *this_pointer,
  BYTE **data,
  UINT32 *num_frames_to_read,
  DWORD *flags,
  UINT64 *device_position,
  UINT64 *QPC_position
)
{
  auto original_hr = get_buffer_original(this_pointer, data, num_frames_to_read, flags, device_position, QPC_position);

  if (sampling_rate < 0)
  {
    sampling_rate = static_cast<int>((*num_frames_to_read) / (default_period / 10000000.0));

    ComPtr<IMMDeviceCollection> device_collection;
    ComPtr<IMMDevice> device;
    auto hr = enumerator->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &device_collection);
    UINT device_count;
    hr = device_collection->GetCount(&device_count);
    for (UINT device_index = 0; device_index < device_count; device_index++)
    {
      hr = device_collection->Item(device_index, &device);
      ComPtr<IPropertyStore> properties;
      hr = device->OpenPropertyStore(STGM_READ, &properties);
      PROPVARIANT variant;
      PropVariantInit(&variant);
      properties->GetValue(PKEY_DeviceInterface_FriendlyName, &variant);
      if (std::wstring(variant.pwszVal) == L"Yamaha NETDUETTO Driver (WDM)")
      {
        break;
      }
    }

    hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, &audio_client);
    WAVEFORMATEX netduetto_format;
    netduetto_format.wFormatTag = WAVE_FORMAT_PCM;
    netduetto_format.nChannels = 2;
    netduetto_format.nSamplesPerSec = sampling_rate;
    netduetto_format.wBitsPerSample = 16;
    netduetto_format.nBlockAlign = netduetto_format.nChannels * netduetto_format.wBitsPerSample / 8;
    netduetto_format.nAvgBytesPerSec = netduetto_format.nSamplesPerSec * netduetto_format.nBlockAlign;
    netduetto_format.cbSize = 0;

    REFERENCE_TIME netduetto_default_period, netduetto_minimum_period;
    hr = audio_client->GetDevicePeriod(&netduetto_default_period, &netduetto_minimum_period);
    hr = audio_client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_RATEADJUST, netduetto_default_period, 0, &netduetto_format, NULL);
    hr = audio_client->GetService(IID_PPV_ARGS(&capture_client));
    capture_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    audio_client->SetEventHandle(capture_event);
    audio_client->Start();
    capture_thread = std::thread(capture);
  }

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

  if (!netduetto_buffer_prepared && capture_buffer.size() > (*num_frames_to_read) * num_channels * 2)
  {
    netduetto_buffer_prepared = true;
  }
  if (netduetto_buffer_prepared && capture_buffer.size() >= (*num_frames_to_read) * num_channels)
  {
    std::scoped_lock<std::mutex> lock(capture_buffer_mutex);
    for (unsigned int sample_index = 0; sample_index < (*num_frames_to_read) * num_channels; sample_index++)
    {
      int16_data[sample_index] += capture_buffer.front();
      capture_buffer.pop_front();
    }
  }

  return original_hr;
}

static void on_attach(HINSTANCE hinstance)
{
  auto current_process_id = GetCurrentProcessId();

  // Get audio render client
  auto hr = CoCreateInstance(
    __uuidof(MMDeviceEnumerator),
    NULL,
    CLSCTX_ALL,
    IID_PPV_ARGS(&enumerator)
  );
  ComPtr<IMMDevice> device;
  hr = enumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &device);
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
    vtable[3] = get_buffer_hook;
    VirtualProtect(*reinterpret_cast<PVOID**>(capture_client.Get()), sizeof(LONG_PTR), old_protect, &old_protect);
  }
}

static void on_detach()
{
  capture_stop_requested = true;
  capture_thread.join();
  audio_client->Stop();
  CloseHandle(capture_event);

  DWORD old_protect;
  auto virtual_protect_retval = VirtualProtect(*reinterpret_cast<PVOID**>(capture_client.Get()), sizeof(LONG_PTR), PAGE_EXECUTE_READWRITE, &old_protect);
  if (virtual_protect_retval == FALSE)
  {
    return;
  }

  auto vtable = *reinterpret_cast<PVOID**>(capture_client.Get());
  vtable[3] = get_buffer_original;
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
    static HHOOK hook = 0;
    if (code < 0)
    {
      return CallNextHookEx(hook, code, wparam, lparam);
    }

    auto message = reinterpret_cast<MSG*>(lparam);
    if (message->message == (WM_APP))
    {
      hook = (HHOOK)(message->lParam);
    }
    if (message->message == (WM_APP + 1))
    {
      on_detach();
      UnhookWindowsHookEx((HHOOK)(message->lParam));
    }

    return CallNextHookEx(hook, code, wparam, lparam);
  }
}
