using System;
using Antmicro.Renode.Core;
using Antmicro.Renode.Core.Structure.Registers;
using Antmicro.Renode.Exceptions;
using Antmicro.Renode.Logging;
using Antmicro.Renode.Peripherals.Memory;
using Antmicro.Renode.Peripherals.SPI;
using Antmicro.Renode.Utilities;

namespace Antmicro.Renode.Peripherals.SPI
{
    public class LOGGER_SPI : ISPIPeripheral, IGPIOReceiver
    {
        public LOGGER_SPI(MappedMemory memory)
        {
            if(!Misc.IsPowerOfTwo((ulong)memory.Size))
            {
                throw new ConstructionException("Size of the underlying memory must be a power of 2");
            }

            this.memory = memory;

            Reset();
        }

        public void Reset()
        {
            currentCommand = Commands.None;
            bytesReceived = 0;
            address = 0;
        }

        public void OnGPIO(int number, bool value)
        {
            // CS rising edge -> transmission done
            if(number == 0 && value)
            {
                FinishTransmission();
            }
        }

        public void FinishTransmission()
        {
            currentCommand = Commands.None;
            bytesReceived = 0;
            address = 0;
        }

        public byte Transmit(byte data)
        {
            this.Log(LogLevel.Info, $"SPI byte received: 0x{data:X2}");

            return 0;
        }

        private readonly MappedMemory memory;
        private Commands currentCommand;
        private int bytesReceived;
        private int address;

        private enum Commands : byte
        {
            None = 0x00,
        }
    }
}
