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
        public bool MixDefaultInput
        {
            get
            {
                return Properties.Settings.Default.MixDefaultInput;
            }
            set
            {
                Properties.Settings.Default.MixDefaultInput = value;
                HookInjector.Injector.MixDefaultInput = value;
                PropertyChanged?.Invoke(this, new PropertyChangedEventArgs("MixDefaultInput"));
            }
        }

        public event PropertyChangedEventHandler PropertyChanged;
    }
}
