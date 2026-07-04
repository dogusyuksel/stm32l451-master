//
// Copyright (c) 2010-2023 Antmicro
//
//  This file is licensed under the MIT License.
//  Full license text is available in 'licenses/MIT.txt'.
//
using System;
using Antmicro.Renode.Core;
using Antmicro.Renode.Core.Structure.Registers;
using Antmicro.Renode.Logging;
using Antmicro.Renode.Peripherals.Bus;
using Antmicro.Renode.Peripherals.Timers;

namespace Antmicro.Renode.Peripherals.Miscellaneous
{
    [AllowedTranslations(AllowedTranslation.ByteToDoubleWord | AllowedTranslation.WordToDoubleWord)]
    public class STM32L4_RCC : BasicDoubleWordPeripheral, IKnownSize
    {
        public STM32L4_RCC(IMachine machine, IPeripheral rtc = null, ITimer lptimer = null, IHasDivisibleFrequency systick = null,
            long apbFrequency = DefaultApbFrequency, long lsiFrequency = DefaultLsiFrequency, long lseFrequency = DefaultLseFrequency,
            long hseFrequency = DefaultHseFrequency) : base(machine)
        {
            if(systick == null)
            {
                this.Log(LogLevel.Warning, "Systick not passed in the RCC constructor. Changes to the system clock will be ignored");
            }
            this.systick = systick;
            if(rtc == null)
            {
                this.Log(LogLevel.Warning, "RTC not passed in the RCC constructor. Changes to the real-time clock will be ignored");
            }
            this.rtc = rtc;
            if(lptimer == null)
            {
                this.Log(LogLevel.Warning, "Lptimer not passed in the RCC constructor. Changes to the low-power timer clock will be ignored");
            }
            this.lptimer = lptimer;

            this.apbFrequency = apbFrequency;
            this.lsiFrequency = lsiFrequency;
            this.lseFrequency = lseFrequency;
            this.hseFrequency = hseFrequency;

            DefineRegisters();
            Reset();
        }

        public override void Reset()
        {
            // Set PLL divisor to a default value of 1.
            // Despite it not being a valid value, this is what it resets to
            // and we don't want the error about invalid PLLDIV to show up after reset.
            PLLR = 2;
            PLLM = 1;
            PLLN = 16;
            msiMultiplier = 32;
            if(systick != null)
            {
                systick.Divider = 1;
            }
            base.Reset();
            UpdateClocks();
        }

        public long Size => 0x400;

        // Update frequencies and divisors of all clocks connected to the RCC.
        // Make sure it's called after any configuration register is touched
        private void UpdateClocks()
        {
            UpdateSystemClock();
            UpdateLpTimerClock();

        }

        private void UpdateSystemClock()
        {
            if(systick == null)
            {
                return;
            }

            var old = systick.Frequency;

            switch(systemClockSwitch.Value)
            {
                case SystemClockSourceSelection.Msi:
                    systick.Frequency = MsiFrequency;
                    this.Log(LogLevel.Debug, "Using MSI for sys clock");
                    break;
                case SystemClockSourceSelection.Hsi16:
                    systick.Frequency = Hsi16Frequency;
                    this.Log(LogLevel.Debug, "Using HSI16 for sys clock");
                    break;
                case SystemClockSourceSelection.Hse:
                    systick.Frequency = hseFrequency;
                    this.Log(LogLevel.Debug, "Using HSE for sys clock");
                    break;
                case SystemClockSourceSelection.Pll:
                    if(!pllOn.Value)
                    {
                        this.Log(LogLevel.Error, "Systick source set to PLL when PLL is disabled.");
                    }

                    this.Log(
                        LogLevel.Debug,
                        "PLLR {0} PLLN {1} PLLM {2}",
                        this.PLLR, this.PLLN, this.PLLM
                    );

                    this.Log(
                        LogLevel.Debug,
                        "PLL Freq is currently {0} pllSource.Value {1}",
                        this.PllFrequency, this.pllSource.Value
                    );

                    systick.Frequency = PllFrequency;
                    break;
                default:
                    throw new Exception("unreachable code");
            }
            if(old != systick.Frequency)
            {
                this.Log(
                    LogLevel.Debug,
                    "systick clock frequency changed to {0}. Current effective frequency: {1}",
                    systick.Frequency,
                    systick.Frequency / systick.Divider
                );
            }
        }

        private void UpdateLpTimerClock()
        {
            if(lptimer == null)
            {
                return;
            }

            var old = lptimer.Frequency;
            switch(lpTimer1Selection.Value)
            {
                case LpTimerClockSourceSelection.Apb:
                    lptimer.Frequency = apbFrequency;
                    break;
                case LpTimerClockSourceSelection.Lsi:
                    lptimer.Frequency = lsiFrequency;
                    break;
                case LpTimerClockSourceSelection.Hsi16:
                    lptimer.Frequency = Hsi16Frequency;
                    break;
                case LpTimerClockSourceSelection.Lse:
                    lptimer.Frequency = lseFrequency;
                    break;
                default:
                    throw new Exception("unreachable code");
            }
            if(old != lptimer.Frequency)
            {
                this.Log(LogLevel.Debug, "LpTimer clock frequency changed to {0}", lptimer.Frequency);
            }
        }

        private void UpdateMSIFrequency(ulong msi_range_value) {
            switch(msi_range_value) {
                case 0:
                    MsiFrequency = 100000;
                    break;
                case 1:
                    MsiFrequency = 200000;
                    break;
                case 2:
                    MsiFrequency = 400000;
                    break;
                case 3:
                    MsiFrequency = 800000;
                    break;
                case 4:
                    MsiFrequency = 1000000;
                    break;
                case 5:
                    MsiFrequency = 2000000;
                    break;
                case 6:
                    MsiFrequency = 4000000;
                    break;
                case 7:
                    MsiFrequency = 8000000;
                    break;
                case 8:
                    MsiFrequency = 16000000;
                    break;
                case 9:
                    MsiFrequency = 24000000;
                    break;
                case 10:
                    MsiFrequency = 32000000;
                    break;
                case 11:
                    MsiFrequency = 48000000;
                    break;
                default:
                    this.Log(LogLevel.Error, "Invalid MSI value {0}", msi_range_value);
                    break;
            }
        }

        private void DefineRegisters()
        {
            // Keep in mind that most of these registers do not affect other
            // peripherals or their clocks.
            // Checked
            Registers.ClockControl.Define(this, 0x00000063)
                .WithFlag(0, out var msion, name: "MSION")
                .WithFlag(1, FieldMode.Read, valueProviderCallback: _ => msion.Value, name: "MSIRDY")
                .WithFlag(2, out var msi_pll_en, name: "MSIPLLEN")
                .WithFlag(3, out var msi_clk_sel, name: "MSIRGSEL")
                .WithValueField(4, 3, name: "MSIRANGE",
                    writeCallback: (_, value) =>
                    {
                        // TODO updates value when msi_clk_sel is 1. Otherwise set through  BackupDomainControlRegister
                        this.UpdateMSIFrequency(value);
                    })
                .WithFlag(8, out var hsi_on, name: "HSION")
                .WithFlag(9, name: "HSIKERON")
                .WithFlag(10, FieldMode.Read, valueProviderCallback: _ => hsi_on.Value, name: "HSIRDY")
                .WithFlag(11, name: "HSIASFS")
                // .WithReservedBits(12,3)
                .WithFlag(16, out var hseon, name: "HSEON")
                .WithFlag(17, FieldMode.Read, valueProviderCallback: _ => hseon.Value, name: "HSERDY")
                .WithFlag(18, name: "HSEBYP")
                .WithTag("CSSHSEON", 19, 1)
                // .WithReservedBits(20, 3)
                .WithFlag(24, out pllOn, name: "PLLON",
                    writeCallback: (previous, value) =>
                    {
                    })
                .WithFlag(25, FieldMode.Read, valueProviderCallback: _ => pllOn.Value, name: "PLLRDY")
                .WithFlag(26, out var sai1_pll_on, name: "PLLSAI1ON")
                .WithFlag(27, FieldMode.Read, valueProviderCallback: _ => sai1_pll_on.Value, name: "PLLSAI1RDY")
                // .WithReservedBits(28, 3)
                .WithWriteCallback((_, __) => UpdateClocks())
                ;

            // Checked
            Registers.InternalClockSourcesCalibration.Define(this, 0x10000000)
                .WithValueField(0, 8, name: "MSICAL")
                .WithValueField(8, 8, name: "MSITRIM")
                .WithValueField(16, 8, name: "HSI16CAL")
                .WithValueField(24, 5, name: "HSI16TRIM")
                .WithWriteCallback((_, __) => UpdateClocks())
                ;

            // Checked
            Registers.ClockConfigurationCfgr.Define(this)
                .WithEnumField<DoubleWordRegister, SystemClockSourceSelection>(0, 2, out systemClockSwitch, name: "SW")
                .WithValueField(2, 2, FieldMode.Read, name: "SWS", valueProviderCallback: _ => (ulong)systemClockSwitch.Value)
                .WithValueField(4, 4, name: "HPRE",
                    writeCallback: (previous, value) =>
                    {
                        if(systick == null || previous == value)
                        {
                            return;
                        }

                        // SYSCLK is not divided unless HPRE is set to 0b1000 or higher,
                        // in which case it's divided by consecutive powers of 2.
                        if((0b1000 & value) == 0)
                        {
                            systick.Divider = 1;
                        }
                        else
                        {
                            var power = (0b111 & value) + 1;
                            systick.Divider = (int)Math.Pow(2, power);
                        }
                        this.Log(
                            LogLevel.Debug,
                            "systick clock divisor changed to {0}. Current effective frequency: {1}",
                            systick.Divider,
                            systick.Frequency / systick.Divider
                        );
                    })
                .WithValueField(8, 3, name: "PPRE1")
                .WithValueField(11, 3, name: "PPRE2")
                .WithReservedBits(14, 1)
                .WithFlag(15, name: "STOPWUCK")
                .WithReservedBits(16,8)
                // .WithReservedBits(17, 7)
                .WithValueField(24, 4, name: "MCOSEL")
                .WithValueField(28, 3, name: "MCOPRE")
                .WithWriteCallback((_, __) => UpdateClocks())
                ;

            //Implemented
            Registers.PLLConfiguration.Define(this, 0x00001000)
                .WithEnumField<DoubleWordRegister, PllSourceSelection>(0, 2, out pllSource, name: "PLLSRC")
                .WithReservedBits(2, 2)
                .WithValueField(4, 3, name: "PLLM",
                    writeCallback: (previous, value) =>
                    {
                        if(pllOn.Value && previous != value)
                        {
                            this.Log(LogLevel.Error, "PLLM modified while PLL is enabled");
                        }
                        PLLM = (long)(value + 1);

                    })
                .WithValueField(8, 7, name: "PLLN",
                    writeCallback: (previous, value) => {
                        if(pllOn.Value && previous != value)
                        {
                            this.Log(LogLevel.Error, "PLLN modified while PLL is enabled");
                        }
                        switch(value){
                            case 0:
                            case 1:
                            case 7:
                            case 87:
                            case 127:
                                this.Log(LogLevel.Error, "Invalid PLLM: {0}", value);
                                break;
                            default:
                                PLLN = (long)value;
                                break;
                        }
                    })
                .WithReservedBits(15, 1)
                .WithFlag(16, name: "PLLPEN")
                .WithValueField(17, 1, name: "PLLP",
                    writeCallback: (previous, value) =>
                    {
                        if (pllOn.Value && previous != value)
                        {
                            this.Log(LogLevel.Error, "PLLP modified while PLL is enabled");
                        }

                        switch (value){
                            case 0:
                                PLLP = 7;
                                break;
                            case 1:
                                PLLP = 17;
                                break;
                        }
                    })
                .WithReservedBits(18, 2)
                .WithFlag(20, name: "PLLQEN") // TODO PLL48M1CLK control
                .WithValueField(21, 2, name: "PPLQ") // TODO PLL48M1CLK control
                .WithFlag(24, name: "PLLREN")
                .WithValueField(25, 2, name: "PLLR",
                    writeCallback: (previous, value) =>
                    {
                        if(pllOn.Value && previous != value)
                        {
                            this.Log(LogLevel.Error, "PLLR modified while PLL is enabled");
                        }

                        switch(value)
                        {
                            case 0b00:
                                PLLR = 2;
                                break;
                            case 0b01:
                                PLLR = 4;
                                break;
                            case 0b10:
                                PLLR = 6;
                                break;
                            case 0b11:
                                PLLR = 8;
                                break;
                            default:
                                throw new Exception("unreachable code");
                        }
                    })
                .WithValueField(27, 5, name: "PLLPDIV") // TODO PLLSAI2CLK Div
                .WithWriteCallback((_, __) => {
                    this.Log(LogLevel.Debug, "PLL SRC {0}", pllSource.Value);
                    UpdateClocks();
                })
                ;

            // Offset 0x18
            Registers.ClockInterruptEnable.Define(this)
                .WithFlag(0, out var lsirdyie, name: "LSIRDYIE")
                .WithFlag(1, out var lserdyie, name: "LSERDYIE")
                .WithFlag(2, out var msirdyie, name: "MSIRDYIE")
                .WithFlag(3, out var hsi16rdyie, name: "HSIRDYIE")
                .WithFlag(4, out var hserdyie, name: "HSERDYIE")
                .WithFlag(5, out var pllrdyie, name: "PLLRDYIE")
                .WithFlag(6, out var pll16rdyie, name: "PLLSAI1RDYIE")
                .WithFlag(7, name: "CSSLSE")
                .WithFlag(9, name: "LSECSSIE")
                .WithReservedBits(10, 22)
                ;

            // Offset 0x4C
            Registers.Ahb2PeripheralClockEnable.Define(this, 0x00000000)
                .WithFlag(0, name: "GPIOAEN")
                .WithFlag(1, name: "GPIOBEN")
                .WithFlag(2, name: "GPIOCEN")
                .WithFlag(3, name: "GPIODEN")
                .WithFlag(4, name: "GPIOEEN")
                .WithReservedBits(5, 2)
                .WithFlag(7, name:  "GPIOHEN")
                .WithReservedBits(8, 4)
                .WithFlag(13, name: "ADCEN")
                .WithReservedBits(14, 2)
                .WithFlag(16, name: "AESEN")
                .WithReservedBits(17, 1)
                .WithFlag(18, name: "RNGEN")
                .WithReservedBits(19, 12)
                ;

            // Offset 0x58 APB1ENR1
            Registers.Apb1PeripheralClockEnable1.Define(this)
                .WithFlag(0, name: "TIM2EN")
                .WithFlag(1,name: "TIM3EN")
                .WithFlag(4, name: "TIM6EN")
                .WithFlag(5, name: "TIM7EN")
                .WithReservedBits(6, 3)
                .WithFlag(9, name: "LCDEN")
                .WithFlag(10, name: "RTCAPBEN")
                .WithFlag(11, name: "WWDGEN")
                .WithReservedBits(12, 2)
                .WithFlag(14, name: "SPI1EN")
                .WithFlag(15, name: "SPI3EN")
                .WithReservedBits(16, 1)
                .WithFlag(17, name: "USART2EN")
                .WithFlag(18, name: "USART1EN")
                .WithFlag(19, name: "USART4EN")
                .WithReservedBits(20, 1)
                .WithFlag(21, name: "I2C1EN")
                .WithFlag(22, name: "I2C2EN")
                .WithFlag(23, name: "I2C3EN")
                .WithFlag(24, name: "CRSEN")
                .WithFlag(25, name: "CAN1EN")
                .WithFlag(26, name: "USBEN")
                .WithReservedBits(27, 1)
                .WithFlag(28, name: "PWREN")
                .WithFlag(29, name: "DAC1EN")
                //
                .WithFlag(30, name: "OPAMPEN")
                .WithFlag(31, name: "LPTIM1EN")
                ;

            Registers.Apb2PeripheralClockEnable.Define(this)
                .WithFlag(0, name: "SYSCFGEN")
                .WithReservedBits(1, 6)
                .WithFlag(7, name: "FWEN")
                .WithReservedBits(8, 2)
                .WithFlag(10, name: "SDMMC1EN")
                .WithFlag(11, name: "TIM1EN")
                .WithFlag(12, name: "SPI1EN")
                .WithReservedBits(13, 1)
                .WithFlag(14, name: "USART1EN")
                .WithReservedBits(15, 1)
                .WithFlag(16, name: "TIM15EN")
                .WithFlag(17, name: "TIM16EN")
                .WithReservedBits(18, 3)
                .WithFlag(21, name: "SA1EN")
                .WithReservedBits(22, 2)
                .WithFlag(24, name: "DFSDM1EN")
                .WithReservedBits(25, 7)
                ;

            Registers.PeripheralsIndepedentClockConfig.Define(this, 0x00000000)
                .WithValueField(0, 2, name: "USART1SEL")
                .WithValueField(2, 2, name: "USART2SEL")
                .WithValueField(4, 2, name: "USART3SEL")
                .WithValueField(6, 2, name: "USART4SEL")
                .WithReservedBits(8, 2)
                .WithValueField(10, 2, name: "LPUART1SEL")
                .WithValueField(12, 2, name: "I2C1SEL")
                .WithValueField(14, 2, name: "I2C2SEL")
                .WithValueField(16, 2, name: "I2C3SEL")
                .WithEnumField<DoubleWordRegister, LpTimerClockSourceSelection>(18, 2, out lpTimer1Selection, name: "LPTIM1SEL")
                .WithEnumField<DoubleWordRegister, LpTimerClockSourceSelection>(20, 2, out lpTimer2Selection, name: "LPTIM2SEL")
                .WithEnumField<DoubleWordRegister, SaiTimerClockSourceSelection>(22, 2, out saiTimer1Selection, name: "SAI1SEL")
                .WithReservedBits(24, 2)
                .WithValueField(26, 2, name: "HSI48SEL")
                .WithReservedBits(28, 4)
                .WithWriteCallback((_, __) => UpdateClocks())
                ;
            // Backup Domain Control Register 0x90
            Registers.BackupDomainControlRegister.Define(this, 0x00000000)
                .WithFlag(0, out var lseon, name: "LSEON")
                .WithFlag(1, FieldMode.Read, valueProviderCallback: _ => lseon.Value, name: "LSERDY")
                .WithTaggedFlag("LSEBYP", 2)
                .WithValueField(3, 2, name: "LSEDRV")
                .WithTaggedFlag("LSECSSON", 5)
                .WithTaggedFlag("LSECSSD", 6)
                .WithReservedBits(7, 1)
                .WithValueField(8, 2, name: "RTCSEL")
                .WithReservedBits(10, 5)
                .WithFlag(15, writeCallback: (_, value) =>
                    {
                        if(rtc == null)
                        {
                            return;
                        }
                        sysbus.SetPeripheralEnabled(rtc, value);
                    },
                    valueProviderCallback: _ => rtc == null ? false : sysbus.IsPeripheralEnabled(rtc), name: "RTCEN")
                .WithTaggedFlag("BDRST", 16)
                .WithReservedBits(17, 7)
                .WithTaggedFlag("LSCOEN", 24)
                .WithTaggedFlag("LSCOSEL", 25)
                .WithReservedBits(26, 6)
                ;

            // Control Status Register 0x94
            Registers.ControlStatus.Define(this, 0x0C000600)
                .WithFlag(0, out var lsion, name: "LSION")
                .WithFlag(1, FieldMode.Read, valueProviderCallback: _ => lsion.Value, name: "LSIRDY")
                .WithReservedBits(2, 6)
                .WithValueField(8, 4, name: "MSIRANGE",
                    writeCallback: (_, value) => {
                        // TODO switch on MSI sel
                        this.UpdateMSIFrequency(value);
                    }) // Is this not duplicated
                .WithTaggedFlag("RMVF", 23)
                .WithTaggedFlag("FWRSTF", 24)
                .WithTaggedFlag("OBLRSTF", 25)
                .WithTaggedFlag("PINRSTF", 26)
                .WithTaggedFlag("BORRSTF", 27)
                .WithTaggedFlag("SFTRSTF", 28)
                .WithTaggedFlag("IWDGRSTF", 29)
                .WithTaggedFlag("WWDGRSTF", 30)
                .WithTaggedFlag("LPWRRSTF", 31)
                .WithWriteCallback((_, __) => UpdateClocks())
                ;
        }


        private long GetPllInputFrequency() {
            long source_frequency = 0;

            switch (pllSource.Value)
            {
                case PllSourceSelection.Hse:
                    source_frequency = hseFrequency;
                    break;
                case PllSourceSelection.Hsi16:
                    source_frequency = Hsi16Frequency;
                    break;
                case PllSourceSelection.MSI:
                    source_frequency = MsiFrequency;
                    break;

                default:
                    this.Log(LogLevel.Error, "Calculating PLL frequency from invalid clock {0}", pllSource.Value);
                    break;
            }

            this.Log(LogLevel.Noisy, "Source frequency {0} (source {1})",
                source_frequency, pllSource.Value);
            return source_frequency;
        }

        private long PllInputFrequency {
            get => GetPllInputFrequency();
        }

        private long VcoFrequency {
            get => (PllInputFrequency * PLLN) / PLLM;
        }
        private long PllFrequency
        {
            get => VcoFrequency / PLLR;
        }
        private long MsiFrequency = 4000000;

        private long Hsi16Frequency { get => 16000000 / (1); }
        private IFlagRegisterField hsi16diven;
        private IEnumRegisterField<PllSourceSelection> pllSource;
        private IEnumRegisterField<SystemClockSourceSelection> systemClockSwitch;
        private IEnumRegisterField<LpTimerClockSourceSelection> lpTimer1Selection;
        private IEnumRegisterField<LpTimerClockSourceSelection> lpTimer2Selection;

        private IEnumRegisterField<SaiTimerClockSourceSelection> saiTimer1Selection;
        private IFlagRegisterField pllOn;

        private long PLLN;
        private long PLLM;
        private long PLLR;

        private long PLLP;
        private long msiMultiplier;

        private readonly long apbFrequency;
        private readonly long lsiFrequency;
        private readonly long lseFrequency;
        private readonly long hseFrequency;

        private readonly IHasDivisibleFrequency systick;
        private readonly IPeripheral rtc;
        private readonly ITimer lptimer;

        private const long DefaultApbFrequency = 32000000;
        private const long DefaultLsiFrequency = 32000;
        private const long DefaultLseFrequency = 32768;
        private const long DefaultHseFrequency = 16000000;
        private const long DefaultMsiFrequency = 2097000;
        private const long BaseMsiFrequency = 65536;

        // There can't be one common ClockSourceSelection enum because different peripherals
        // have different sets of possible values:
        // I2C has APB, system clock, HSI16, reserved;
        // UARTs have APB, system clock, HSI16, LSE.
        // The system clock can be HSI16, HSE, PLL, MSI (default)
        private enum LpTimerClockSourceSelection
        {
            Apb,
            Lsi,
            Hsi16,
            Lse,
        }

        private enum SaiTimerClockSourceSelection
        {
            PLLSAI_P,
            Hsi16,
            PPLL_P,
            External,
        }

        private enum SystemClockSourceSelection
        {
            Msi,
            Hsi16,
            Hse,
            Pll,
        }

        private enum PllSourceSelection
        {
            None,
            MSI,
            Hsi16,
            Hse,
        }

        private enum Registers
        {
            ClockControl = 0x0,
            InternalClockSourcesCalibration = 0x4,
            ClockConfigurationCfgr = 0x8,

            PLLConfiguration = 0xC,

            ClockInterruptEnable = 0x18,
            Ahb2PeripheralClockEnable = 0x4C,
            Apb1PeripheralClockEnable1 = 0x58,

            Apb2PeripheralClockEnable = 0x60,
            PeripheralsIndepedentClockConfig = 0x88,
            BackupDomainControlRegister = 0x90,
            ControlStatus = 0x94,
        }
    }
}
