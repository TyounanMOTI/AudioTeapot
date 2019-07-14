using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace AudioTeapot
{
    class HookConfiguration : INotifyPropertyChanged
    {
        public int WhisperVolume
        {
            get
            {
                return Properties.Settings.Default.WhisperVolume;
            }
            set
            {
                Properties.Settings.Default.WhisperVolume = value;
                HookInjector.Injector.WhisperVolume = value;
                PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(WhisperVolume)));
            }
        }

        public int InputMixVolume
        {
            get
            {
                return Properties.Settings.Default.InputMixVolume;
            }
            set
            {
                Properties.Settings.Default.InputMixVolume = value;
                HookInjector.Injector.InputMixVolume = value;
                PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(InputMixVolume)));
            }
        }

        public int NetduettoVolume
        {
            get
            {
                return Properties.Settings.Default.NetduettoVolume;
            }
            set
            {
                Properties.Settings.Default.NetduettoVolume = value;
                HookInjector.Injector.NetduettoVolume = value;
                PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(NetduettoVolume)));
            }
        }

        public event PropertyChangedEventHandler PropertyChanged;
    }
}
