using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace AudioTeapot
{
    class InjectionStatus : INotifyPropertyChanged
    {
        public enum State {
            Undefined,
            Disconnecting,
            Disconnected,
            Searching,
            Injecting,
            Injected,
        }
        private State currentStatus = State.Disconnected;
        public State CurrentStatus
        {
            get
            {
                return currentStatus;
            }
            set
            {
                currentStatus = value;
                PropertyChanged?.Invoke(this, new PropertyChangedEventArgs("CurrentStatus"));
                PropertyChanged?.Invoke(this, new PropertyChangedEventArgs("Injected"));
                PropertyChanged?.Invoke(this, new PropertyChangedEventArgs("Cancellable"));
                PropertyChanged?.Invoke(this, new PropertyChangedEventArgs("Injectable"));
            }
        }

        public bool Injected
        {
            get
            {
                return currentStatus == State.Injected;
            }
        }

        public bool Cancellable
        {
            get
            {
                return currentStatus == State.Injecting || currentStatus == State.Searching;
            }
        }

        public bool Injectable
        {
            get
            {
                return currentStatus == State.Disconnected;
            }
        }

        public event PropertyChangedEventHandler PropertyChanged;
    }
}
