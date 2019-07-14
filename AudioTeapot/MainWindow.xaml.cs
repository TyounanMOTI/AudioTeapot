using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading;
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
        public InjectionStatus.State status;

        public Progress(string description, int percentage, InjectionStatus.State status)
        {
            this.description = description;
            this.percentage = percentage;
            this.status = status;
        }
    }

    public partial class MainWindow : Window {
        uint injectedProcessId;
        ProcessEnumerator enumerator = new ProcessEnumerator();
        CancellationTokenSource injectCancel = new CancellationTokenSource();
        InjectionStatus injectionStatus = new InjectionStatus();
        HookConfiguration hookConfiguration = new HookConfiguration();

        public MainWindow() {
            InitializeComponent();

            ProcessComboBox.DataContext = enumerator;
            DisconnectButton.DataContext = injectionStatus;
            CancelButton.DataContext = injectionStatus;
            ConnectButton.DataContext = injectionStatus;

            WhisperVolumeSlider.DataContext = hookConfiguration;
            WhisperVolumeTextBox.DataContext = hookConfiguration;
            InputMixVolumeSlider.DataContext = hookConfiguration;
            InputMixVolumeTextBox.DataContext = hookConfiguration;
            NetduettoVolumeSlider.DataContext = hookConfiguration;
            NetduettoVlumeTextBox.DataContext = hookConfiguration;

            Closing += MainWindow_Closing;

            var autoConnectExecutableName = Properties.Settings.Default.AutoConnectExecutableName;
            if (AutoConnectCheckBox.IsChecked == true && !string.IsNullOrEmpty(autoConnectExecutableName))
            {
                Inject(autoConnectExecutableName);
            }
        }

        private void MainWindow_Closing(object sender, System.ComponentModel.CancelEventArgs e)
        {
            Properties.Settings.Default.Save();
            if (injectionStatus.Injected)
            {
                injectionStatus.CurrentStatus = InjectionStatus.State.Disconnecting;
                Injector.Remove(injectedProcessId);
                injectionStatus.CurrentStatus = InjectionStatus.State.Disconnected;
            }
        }

        private void ConnectButton_Click(object sender, RoutedEventArgs e)
        {
            var executableName = ProcessComboBox.Text as string;
            if (string.IsNullOrEmpty(executableName))
            {
                return;
            }

            Inject(executableName);
        }

        private async void DisconnectButton_Click(object sender, RoutedEventArgs e)
        {
            await RemoveInjection();
        }

        private async Task RemoveInjection()
        {
            if (injectionStatus.Injected)
            {
                injectionStatus.CurrentStatus = InjectionStatus.State.Disconnecting;
                await Task.Run(() =>
                {
                    Injector.Remove(injectedProcessId);
                });
                injectionStatus.CurrentStatus = InjectionStatus.State.Disconnected;
            }
        }

        private void Inject(string executableName)
        {
            injectCancel = new CancellationTokenSource();
            Observable.Create<Progress>(async o =>
            {
                try
                {
                    o.OnNext(new Progress("プロセス検索中", 10, InjectionStatus.State.Searching));
                    injectedProcessId = await Task.Run(async () =>
                    {
                        while (!Injector.Processes.ContainsKey(executableName))
                        {
                            injectCancel.Token.ThrowIfCancellationRequested();
                            await Task.Delay(5000, injectCancel.Token);
                        }
                        return Injector.Processes[executableName];
                    });

                    o.OnNext(new Progress("接続中", 40, InjectionStatus.State.Injecting));
                    await Task.Run(() =>
                    {
                        Injector.Inject(injectedProcessId);
                        Injector.InputMixVolume = Properties.Settings.Default.InputMixVolume;
                        Injector.NetduettoVolume = Properties.Settings.Default.NetduettoVolume;
                        Injector.WhisperVolume = Properties.Settings.Default.WhisperVolume;
                    });

                    await Task.Delay(1000, injectCancel.Token);
                    o.OnNext(new Progress("接続完了", 100, InjectionStatus.State.Injected));

                    await CheckTargetProcessAlive((int)injectedProcessId);
                    o.OnNext(new Progress("切断済み", 0, InjectionStatus.State.Disconnected));

                    await Task.Delay(1000, injectCancel.Token);
                    o.OnCompleted();
                }
                catch (OperationCanceledException)
                {
                    if (injectionStatus.Injected)
                    {
                        await RemoveInjection();
                    }
                    injectionStatus.CurrentStatus = InjectionStatus.State.Disconnected;
                }
                return Disposable.Empty;
            }).Subscribe(x =>
            {
                if (InjectorProgress != null && InjectorProgressText != null)
                {
                    InjectorProgressText.Text = x.description;
                    InjectorProgress.Value = x.percentage;
                }
                injectionStatus.CurrentStatus = x.status;
            },
            injectCancel.Token);
        }

        private async Task CheckTargetProcessAlive(int processId)
        {
            await Task.Run(async () =>
            {
                var process = Process.GetProcessById(processId);
                while (!process.HasExited)
                {
                    injectCancel.Token.ThrowIfCancellationRequested();
                    await Task.Delay(5 * 1000, injectCancel.Token);
                }
            }, injectCancel.Token);
        }

        private void CancelButton_Click(object sender, RoutedEventArgs e)
        {
            injectCancel.Cancel();
            injectionStatus.CurrentStatus = InjectionStatus.State.Disconnecting;
        }
    }
}
