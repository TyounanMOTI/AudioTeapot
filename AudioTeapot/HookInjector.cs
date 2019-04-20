using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Runtime.InteropServices;

namespace AudioTeapot.HookInjector
{
    public class Injector
    {
        public static Dictionary<string, uint> Processes
        {
            get
            {
                uint processCount;
                var processInfoPointer = NativePlugin.EnumerateProcessExecutables(out processCount);
                var processes = new Dictionary<string, uint>();
                var iterator = processInfoPointer;
                for (var infoIndex = 0; infoIndex < processCount; infoIndex++)
                {
                    var info = Marshal.PtrToStructure<NativePlugin.ProcessInfo>(iterator);
                    iterator = IntPtr.Add(iterator, Marshal.SizeOf<NativePlugin.ProcessInfo>());

                    if (string.IsNullOrEmpty(info.executableName))
                    {
                        continue;
                    }
                    if (processes.ContainsKey(info.executableName))
                    {
                        continue;
                    }
                    processes.Add(info.executableName, info.processId);
                }

                Marshal.FreeCoTaskMem(processInfoPointer);

                return processes;
            }
        }

        public static void Inject(uint processId)
        {
            NativePlugin.Inject(processId, "WasapiHook.dll");
        }

        public static void Remove(uint processId)
        {
            NativePlugin.Remove(processId, "WasapiHook.dll");
        }

        public static bool MixDefaultInput
        {
            set
            {
                NativePlugin.SetMixDefaultInput(value);
            }
        }
    }

    internal class NativePlugin
    {
        public struct ProcessInfo {
            public uint processId;
            [MarshalAs(UnmanagedType.BStr)] public string executableName;

            public ProcessInfo(uint processId, string executableName)
            {
                this.processId = processId;
                this.executableName = executableName;
            }
        }

        [DllImport("HookInjector.dll")]
        public static extern IntPtr EnumerateProcessExecutables(out uint processCount);

        [DllImport("HookInjector.dll", CharSet = CharSet.Unicode)]
        public static extern void Inject(uint processId, string dllPath);

        [DllImport("HookInjector.dll", CharSet = CharSet.Unicode)]
        public static extern void Remove(uint processId, string dllPath);

        [DllImport("HookInjector.dll")]
        public static extern void SetMixDefaultInput(bool enable);
    }
}
