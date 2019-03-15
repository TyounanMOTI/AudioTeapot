using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;
using System.Reactive.Linq;
using System.Reactive.Disposables;
using System.Diagnostics;
using AudioTeapot.HookInjector;

namespace AudioTeapot {
    struct Progress {
        public string description;
        public int percentage;

        public Progress(string description, int percentage)
        {
            this.description = description;
            this.percentage = percentage;
        }
    }

    public partial class MainWindow : Window {
        uint injectedProcessId;

        public MainWindow() {
            InitializeComponent();
            Inject();
            Closing += MainWindow_Closing;
        }

        private void Inject()
        {
            Observable.Create<Progress>(async o =>
            {
                o.OnNext(new Progress("プロセス検索中", 10));
                injectedProcessId = await Task.Run(async () =>
                {
                    while (!Injector.Processes.ContainsKey("VRChat.exe"))
                    {
                        await Task.Delay(5000);
                    }
                    return Injector.Processes["VRChat.exe"];
                });
                o.OnNext(new Progress("接続中", 40));
                await Task.Run(() =>
                {
                    Injector.Inject(injectedProcessId);
                });
                o.OnNext(new Progress("接続完了", 100));
                await CheckTargetProcessAlive((int)injectedProcessId);
                return Disposable.Empty;
            }).Subscribe(x =>
            {
                InjectorProgressText.Text = x.description;
                InjectorProgress.Value = x.percentage;
            });
        }

        private async Task CheckTargetProcessAlive(int processId)
        {
            await Task.Run(() =>
            {
                var process = Process.GetProcessById(processId);
                while (!process.HasExited)
                {
                    Task.Delay(5 * 1000);
                }
            });
            Inject();
        }

        private void MainWindow_Closing(object sender, System.ComponentModel.CancelEventArgs e)
        {
            if (Injector.Processes.ContainsKey("VRChat.exe"))
            {
                Injector.Remove(injectedProcessId);
            }
        }
    }
}
