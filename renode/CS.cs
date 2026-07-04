//coppied from renode-infrastructure/src/Emulator/Peripherals/Peripherals/Miscellaneous/LED.cs
using System;

using Antmicro.Migrant;
using Antmicro.Renode.Core;
using Antmicro.Renode.Logging;

namespace Antmicro.Renode.Peripherals.Miscellaneous
{
    public class CS : IGPIOReceiver, ILed
    {
        public CS(bool invert = false)
        {
            inverted = invert;
            state = invert;
            sync = new object();
        }

        public void OnGPIO(int number, bool value)
        {
            if(number != 0)
            {
                throw new ArgumentOutOfRangeException();
            }

            State = inverted ? !value : value;
        }

        public void Reset()
        {
            state = inverted;
        }

        public bool State
        {
            get => state;

            private set
            {
                lock(sync)
                {
                    if(value == state)
                    {
                        return;
                    }

                    state = value;
                    StateChanged?.Invoke(this, state);
                    if(state)
                        this.Log(LogLevel.Noisy, "Device Dectivated (CS High)");
                    else
                        this.Log(LogLevel.Noisy, "Device Activated (CS Low)");
                }
            }
        }

        [field: Transient]
        public event Action<ILed, bool> StateChanged;

        private bool state;

        private readonly bool inverted;
        private readonly object sync;
    }
}