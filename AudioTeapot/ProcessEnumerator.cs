using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Threading;

namespace AudioTeapot
{
    class ProcessEnumerator : INotifyPropertyChanged
    {
        public string[] ProcessExecutableNames { get; private set; }

        private int selectedIndex = -1;
        public int SelectedIndex
        {
            get
            {
                if (selectedIndex < 0 && ProcessExecutableNames != null)
                {
                    selectedIndex = Array.IndexOf(ProcessExecutableNames, "Unity.exe");
                }
                return selectedIndex;
            }
            set
            {
                selectedIndex = value;
                PropertyChanged(this, new PropertyChangedEventArgs("SelectedIndex"));
            }
        }

        public event PropertyChangedEventHandler PropertyChanged;

        DispatcherTimer timer;

        public ProcessEnumerator()
        {
            timer = new DispatcherTimer(
                TimeSpan.FromSeconds(5),
                DispatcherPriority.Normal,
                async (sender, e) =>
                {
                    if (selectedIndex < 0)
                    {
                        await Refresh();
                    }
                },
                Dispatcher.CurrentDispatcher
                );
            Task.Run(Refresh);
        }

        public async Task Refresh()
        {
            await Task.Run(() =>
            {
                var nameList = new List<string>();
                var processes = HookInjector.Injector.Processes;
                foreach (var executableName in processes.Keys)
                {
                    nameList.Add(executableName);
                }
                ProcessExecutableNames = nameList.ToArray();
            });

            PropertyChanged(this, new PropertyChangedEventArgs("ProcessExecutableNames"));
            PropertyChanged(this, new PropertyChangedEventArgs("SelectedIndex"));
        }
    }
}
