using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Collections.ObjectModel;

namespace AudioTeapot
{
    class AudioSourceList
    {
        public ObservableCollection<AudioSourceListItem> Data { get; }

        public AudioSourceList()
        {
            Data = new ObservableCollection<AudioSourceListItem>();
            Data.Add(new AudioSourceListItem { Enabled = true, Type = AudioSourceType.GateBreaker });
            Data.Add(new AudioSourceListItem { Enabled = true, Type = AudioSourceType.AudioInput });
        }
    }
}
