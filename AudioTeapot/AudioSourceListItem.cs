using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace AudioTeapot
{
    enum AudioSourceType : int
    {
        GateBreaker = 0,
        AudioInput = 1,
    }

    class AudioSourceListItem
    {
        readonly static string[] audioSourceTypeNames = new string[] { "囁きアビリティ（音量ゲート無効化）", "音声入力デバイス" };

        public bool Enabled { get; set; }
        public AudioSourceType Type { get; set; }

        public int TypeIndex => (int)Type;
    }
}
