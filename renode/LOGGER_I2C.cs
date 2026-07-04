//
// Copyright (c) 2010-2020 Antmicro
// Copyright (c) 2011-2015 Realtime Embedded
//
// This file is licensed under the MIT License.
// Full license text is available in 'licenses/MIT.txt'.
//
using System;
using System.Linq;

using Antmicro.Renode.Core;
using Antmicro.Renode.Logging;

namespace Antmicro.Renode.Peripherals.I2C
{
    public class LOGGER_I2C : II2CPeripheral
    {
        public LOGGER_I2C()
        {
            Reset();
        }

        public void Write(byte[] data)
        {
            this.NoisyLog("Write {0}", data.Select(x => x.ToString("X")).Aggregate((x, y) => x + " " + y));
        }

        public void FinishTransmission()
        {
        }

        public byte[] Read(int count = 0)
        {
            this.NoisyLog("Read {0} number of bytes", count);
            return new byte[count];
        }

        public void Reset()
        {
            
        }
    }
}