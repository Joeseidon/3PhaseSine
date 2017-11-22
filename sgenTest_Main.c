//###################################################################################
//Created using buffered sine dac example (buffdac_sine_cpu01.c) from TI as reference
//
//###################################################################################

//
// Included Files
//
#include "F28x_Project.h"
#include "sgen.h"         // Signal Generation Headerfile


//
// Defines
//
#define REFERENCE_VDAC        0
#define REFERENCE_VREF        1
#define DACA                  1
#define DACB                  2
#define DACC                  3

#define REFERENCE             REFERENCE_VDAC
#define CPUFREQ_MHZ           200

//use linear interpolation
#define LINEAR_INT				1
#define NORMAL_GEN				0
#define PHASE_GEN_TYPE			LINEAR_INT

//DAC Registers
volatile struct DAC_REGS* DAC_PTR[4] = {0x0,&DacaRegs,&DacbRegs,&DaccRegs};

//Waveform settings
Uint32 samplingFreq_hz = 200000;
Uint32 outputFreq_hz = 1000;
Uint32 maxOutputFreq_hz = 5000;
float waveformGain = 0.8003; // Range 0.0 -> 1.0
float waveformOffset = 0;    // Range -1.0 -> 1.0

//Define Signal Generator
#if PHASE_GEN_TYPE==LINEAR_INT
SGENTI_3 sgen = SGENTI_3_DEFAULTS;
#elif PHASE_GEN_TYPE==NORMAL_GEN
SGENT_3 sgen = SGENT_3_DEFAULTS;
#endif


//Sgen Channel Value Storage
Uint16 sgen_out1 = 0;
Uint16 sgen_out2 = 0;
Uint16 sgen_out3 = 0;


//Function Prototypes
static inline void setFreq(void);
static inline void setGain(void);
static inline void setOffset(void);
void configureDAC(void);
void configureWaveform(void);
interrupt void cpu_timer0_isr(void);

//
// Main
//
void main(void)
{
//
// Initialize System Control:
// PLL, WatchDog, enable Peripheral Clocks
// This example function is found in the F2837xS_SysCtrl.c file.
//
    InitSysCtrl();

//
// Disable CPU interrupts
//
    DINT;

//
// Initialize the PIE control registers to their default state.
// The default state is all PIE interrupts disabled and flags are cleared.
// This function is found in the F2837xS_PieCtrl.c file.
//
    InitPieCtrl();

//
// Clear all interrupts and initialize PIE vector table:
//
    IER = 0x0000;
    IFR = 0x0000;
    InitPieVectTable();

//
// Map Cpu Timer0 interrupt function to the PIE vector table
//
    EALLOW;
    PieVectTable.TIMER0_INT = &cpu_timer0_isr;
    EDIS;
	
//
// Configure DAC
//
    configureDAC();

//
// Configure Waveform
//
    configureWaveform();

//
// Initialize Cpu Timers
//
    InitCpuTimers();

//
// Configure Cpu Timer0 to interrupt at specified sampling frequency
//
    ConfigCpuTimer(&CpuTimer0, CPUFREQ_MHZ, 1000000.0/samplingFreq_hz);

//
// Start Cpu Timer0
//
    CpuTimer0Regs.TCR.all = 0x4000;

//
// Enable interrupt
//
    IER |= M_INT1;
    PieCtrlRegs.PIEIER1.bit.INTx7 = 1;
    EINT;
    ERTM;

    while(1)
    {
        setFreq();   // Set Output Frequency and Max Output Frequency
        setGain();   // Set Magnitude of Waveform
        setOffset(); // Set Offset of Waveform
	}
}//End Main


//
// setFreq - Set the SINE frequency in SGEN
//
static inline void setFreq(void)
{
	//
    // Range(Q0) = 0x0000 -> 0x7FFF, step_max(Q0) =
    // (Max_Freq_hz*0x10000)/Sampling_Freq_hz
    //
    sgen.step_max = (maxOutputFreq_hz*0x10000)/samplingFreq_hz;

    //
    // Range(Q15) = 0x0000 -> 0x7FFF, freq(Q15) =
    // (Required_Freq_hz/Max_Freq_hz)*0x8000
    //
    sgen.freq = ((float)outputFreq_hz/maxOutputFreq_hz)*0x8000;
}


//
// setGain - Set the gain in SGEN
//
static inline void setGain(void)
{
	sgen.gain = waveformGain * 0x7FFF;   // Range(Q15) = 0x0000 -> 0x7FFF
}


//
// setOffset - Set the offset in SGEN
//
static inline void setOffset(void)
{
	sgen.offset = waveformOffset * 0x7FFF; // Range(Q15) = 0x8000 -> 0x7FFF
}

//
// configureDAC - Enable and configure DAC modules
//
void configureDAC(void)
{
	EALLOW;
	
	//
	//Configure DACA
	//
	DAC_PTR[DACA]->DACCTL.bit.DACREFSEL = REFERENCE;
	DAC_PTR[DACA]->DACOUTEN.bit.DACOUTEN = 1;
	DAC_PTR[DACA]->DACVALS.all = 0;

	//
	//Configure DACB
	//
	DAC_PTR[DACB]->DACCTL.bit.DACREFSEL = REFERENCE;
	DAC_PTR[DACB]->DACOUTEN.bit.DACOUTEN = 1;
	DAC_PTR[DACB]->DACVALS.all = 0;

	//
	//Configure DACC
	//
	DAC_PTR[DACC]->DACCTL.bit.DACREFSEL = REFERENCE;
	DAC_PTR[DACC]->DACOUTEN.bit.DACOUTEN = 1;
	DAC_PTR[DACC]->DACVALS.all = 0;
	
	DELAY_US(10); //Allow for buffered DAC to power up
	
	EDIS;
}

//
// configureWaveform - Configure the SINE waveform
//
void configureWaveform(void)
{
	sgen.alpha = 0; // Range(16) = 0x0000 -> 0xFFFF
    setFreq();
    setGain();
    setOffset();
}

//
// cpu_timer0_isr - Timer ISR that writes the sine value to DAC, log the sine
//                  value, compute the next sine value, and calculate interrupt
//                  duration
//
interrupt void cpu_timer0_isr(void)
{
	//
	// Write current sine value to buffered DACs
	//
	DAC_PTR[DACA]->DACVALS.all = sgen_out1;
	DAC_PTR[DACB]->DACVALS.all = sgen_out2;
	DAC_PTR[DACC]->DACVALS.all = sgen_out3;
	
	//
    // Scale next sine value
    //
    sgen_out1 = (sgen.out1 + 32768) >> 4;
    sgen_out2 = (sgen.out2 + 32768) >> 4;
    sgen_out3 = (sgen.out3 + 32768) >> 4;
	
	//
    // Compute next sine value
    //
    sgen.calc(&sgen);
	
	//
    // Acknowledge this interrupt to receive more interrupts from group 1
    //
    PieCtrlRegs.PIEACK.all = PIEACK_GROUP1;
}

//
// End of file
//
