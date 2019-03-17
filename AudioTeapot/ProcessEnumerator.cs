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

        private string selectedValue = Properties.Settings.Default.AutoConnectExecutableName;
        public string SelectedValue
        {
            get
            {
                return selectedValue;
            }
            set
            {
                Properties.Settings.Default.AutoConnectExecutableName = value;
                selectedValue = value;
                PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(SelectedValue));
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
                    await Refresh();
                },
                Dispatcher.CurrentDispatcher
                );
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
        }
    }
}
