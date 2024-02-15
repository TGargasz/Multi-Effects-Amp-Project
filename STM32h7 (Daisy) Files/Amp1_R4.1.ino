/*
Amp1_Rx.x.ino - Multiple audio effects for guitar amplifier
Copyright (C) 2020  By: Tony Gargasz

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation version 3.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
*/

// ******************************************************************
// * Daisy Seed effects With Rate and Depth Knobs                   *
// *    By: Tony Gargasz - tony.gargasz@gmail.com                   *
// *    R1 - Optimized control interaction                          *
// *    R2 - Added Distortion and Tremelo effects                   *
// *    R3 - Clean up for release                                   *
// *    R3.1 - Added license for release into the wild              *
// *    R3.2 - Add I2C to ESP WiFi AP to make adjs with cell phone  *
// *    R3.3 - Remove I2C and add SPI to ESP8266 for WiFi app       *
// *    R3.4 - Remove SPI and add SoftwareSerial for Wifi app       *
// *    R3.5 - Changed back to SPI to ESP8266 for WiFi app          *
// *    R3.6 - Add comm routines to send settings back and forth    *
// *    R3.7 - Removed duplicate code in adjustSettings()           *
// *    R4.0 - Lockout knobs while under control by Wifi app        *
// *    R4.1 - Add 3 footswitch code and Reverb, Flanger effects    *
// ******************************************************************

#include <StringSplitter.h>
#include <SPI.h>
#include <DaisyDSP.h>
#include <DaisyDuino.h>

// Use the daisy namespace to prevent having to type
// daisy:: before all libdaisy functions
using namespace daisy;

DaisyHardware hw;

size_t num_channels;
bool bypass_On = false;
bool BypassSwitch = false;
bool btnBypassEnabled = true;
bool espHasDataEnabled = true;
bool espHasData = false;
bool flngFltr_On = false;

bool prev_ftSwitchLeftState = false;
bool prev_ftSwitchMiddleState = false;
bool prev_ftSwitchRightState = false;

bool ProcTremLeft = false;
bool ProcEchoLeft = false;
bool ProcDistLeft = false;
bool ProcRvrbLeft = false;
bool ProcFlngLeft = false;

bool ProcTremMid = false;
bool ProcEchoMid = false;
bool ProcDistMid = false;
bool ProcRvrbMid = false;
bool ProcFlngMid = false;

bool ProcTremRight = false;
bool ProcEchoRight = false;
bool ProcDistRight = false;
bool ProcRvrbRight = false;
bool ProcFlngRight = false;


int RateKnob_Num, DepthKnob_Num; // Knob vars
int ftSwLeft_Num, ftSwMiddle_Num, ftSwRight_Num;  // Footswitch assignments
float sample_rate, out_buf, prev_knob1val, prev_knob2val, knob1val, knob2val; // General vars
float wet, delay_adj, feedback_adj; // Delay Line (echo) vars
float rate_adj, depth_adj; // Tremelo vars 

// float fuzz_buf, gain_adj, mix_adj, fuzz_quo; // Distortion vars

float fuzz_buf, gain_adj, mix_adj, timbreInverse; // Distortion vars
float verb_in, out1, out2, hifreq_adj, amount_adj; // Reverb vars
float regen, sweep, flng_buf, speed_adj, range_adj, color_adj; // Flanger vars
float minDelay, maxDelay, centerDelay, spanDelay; // Flanger vars
int flangeFilter_type; // Flange Filter: 0=Flanger 1=Filter

// For knob value averaging
float tot_knob1val = 0;
float tot_knob2val = 0;

// Footswitches
const int ftSwitchLeft = D0;
const int ftSwitchMiddle = D1;
const int ftSwitchRight = D2;

// LEDs 
const int ledByp = D3;  // Bypass Led (front panel, blue)
const int ledLeft = D6;
const int ledMiddle = D5;
const int ledRight = D4;

// Potentiometers
// inMode(A0, OUTPUT);
const int knob1Pin = A0;
const int knob2Pin = A1;

unsigned long currentMillis;        // To set period to poll knobs and switchs
unsigned long previousMillis = 0;   // Last time knobs and switches were polled
const unsigned long interval = 100; // Interval at which to poll (milliseconds)

#define MAX_DELAY static_cast<size_t>(48000)        // Set Echo max delay time to 100% of samplerate (1 second)
#define MAX_FLANGEDELAY static_cast<size_t>(720)    // Set Flange max delay time to 1.5% of samplerate (15 milliseconds)

// Instantiate Daisy Seed Effects
static DelayLine<float, MAX_DELAY> del;         // Echo delay line
static DelayLine<float, MAX_FLANGEDELAY> dly;   // Flanger delay line
static ReverbSc DSY_SDRAM_BSS verb;             // Reverb tank
static Oscillator osc_trem;                     // Tremolo LFO
static Oscillator osc_flng;                     // Flanger LFO
static DcBlock dcblk;                           // DC Block

// SPI 
class ESPMaster
{
public:
    ESPMaster() {}
    void readData(uint8_t* data)
    {
        SPI.transfer(0x03);
        SPI.transfer(0x00);
        for (uint8_t i = 0; i < 32; i++)
        {
            data[i] = SPI.transfer(0);
        }
    }

    void writeData(uint8_t* data, size_t len)
    {
        uint8_t i = 0;
        SPI.transfer(0x02);
        SPI.transfer(0x00);
        while (len-- && i < 32)
        {
            SPI.transfer(data[i++]);
        }
        while (i++ < 32)
        {
            SPI.transfer(0);
        }
    }

    String readData()
    {
        char data[33]{};
        data[32] = 0;
        readData((uint8_t*)data);
        return String(data);
    }

    void writeData(const char* data)
    {
        writeData((uint8_t*)data, strlen(data));
    }
};
// Instantiate SPI class
ESPMaster esp;

// Send settings to ESP8266 (event driven)
void sendData(const char* msgType)
{
    // digitalWrite(SelectPin, LOW);
    char sndData[32] = { 0 };
    if (msgType == "R")
    {
        sprintf(sndData, "R %9.3f %6.3f %6.3f", delay_adj, rate_adj, gain_adj);
        esp.writeData(sndData);
    }
    else if (msgType == "D")
    {
        sprintf(sndData, "D %5.3f %5.3f %5.3f", feedback_adj, depth_adj, mix_adj);
        esp.writeData(sndData);
    }
    else if (msgType[0] == 'B')
    {
        sprintf(sndData, "B %5.3f %5.3f %5.3f", hifreq_adj, amount_adj, speed_adj);
        esp.writeData(sndData);
    }
    else if (msgType[0] == 'A')
    {
        sprintf(sndData, "A %5.3f %5.3f %1d", range_adj, color_adj, flangeFilter_type);
        esp.writeData(sndData);
    }
    else if (msgType[0] == 'M')
    {
        sprintf(sndData, "M %1d %1d %1d %1d %1d", ftSwLeft_Num, ftSwMiddle_Num, ftSwRight_Num, RateKnob_Num, DepthKnob_Num);
        esp.writeData(sndData);
    }
    else if (msgType == "a")
    {
        esp.writeData("a"); // Request next pack of data
    }
    else if (msgType == "b")
    {
        esp.writeData("b"); // Request next pack of data
    }
    else if (msgType == "c")
    {
        esp.writeData("c"); // Request next pack of data
    }
    else if (msgType == "d")
    {
        esp.writeData("d"); // Request next pack of data
    }
    else if (msgType == "K")
    {
        esp.writeData("Keep Alive"); // Only used during testing [keep active!]
    }
    // digitalWrite(SelectPin, HIGH);
}

// Receive settings from ESP8266 (polled in Loop() )
void receiveData(String message)
{
    String tmp = "";
    StringSplitter* split = new StringSplitter(message, ' ', 24);
    int itemCount = split->getItemCount();

    if (message.startsWith("R"))
    {
        if (itemCount >= 4)
        {
            tmp = split->getItemAtIndex(1);
            delay_adj = strtod(tmp.c_str(), NULL);
            tmp = split->getItemAtIndex(2);
            rate_adj = strtod(tmp.c_str(), NULL);
            tmp = split->getItemAtIndex(3);
            gain_adj = strtod(tmp.c_str(), NULL);
        }
    }
    else if (message.startsWith("D"))
    {
        if (itemCount >= 4)
        {
            tmp = split->getItemAtIndex(1);
            feedback_adj = strtod(tmp.c_str(), NULL);
            tmp = split->getItemAtIndex(2);
            depth_adj = strtod(tmp.c_str(), NULL);
            tmp = split->getItemAtIndex(3);
            mix_adj = strtod(tmp.c_str(), NULL);
        }
    }
    else if (message.startsWith("B"))
    {
        if (itemCount >= 4)
        {
            tmp = split->getItemAtIndex(1);
            hifreq_adj = strtod(tmp.c_str(), NULL);
            tmp = split->getItemAtIndex(2);
            amount_adj = strtod(tmp.c_str(), NULL);
            tmp = split->getItemAtIndex(3);
            speed_adj = strtod(tmp.c_str(), NULL);
        }
    }
    else if (message.startsWith("A"))
    {
        if (itemCount >= 4)
        {
            tmp = split->getItemAtIndex(1);
            range_adj = strtof(tmp.c_str(), NULL);
            tmp = split->getItemAtIndex(2);
            color_adj = strtof(tmp.c_str(), NULL);
            tmp = split->getItemAtIndex(3);
            flangeFilter_type = strtof(tmp.c_str(), NULL);
            flngFltr_On = (flangeFilter_type == 1) ? true : false;
        }
    }
    else if (message.startsWith("M"))
    {
        if (itemCount >= 6)
        {
            tmp = split->getItemAtIndex(1);
            ftSwLeft_Num = strtod(tmp.c_str(), NULL);
            tmp = split->getItemAtIndex(2);
            ftSwMiddle_Num = strtod(tmp.c_str(), NULL);
            tmp = split->getItemAtIndex(3);
            ftSwRight_Num = strtod(tmp.c_str(), NULL);
            tmp = split->getItemAtIndex(4);
            RateKnob_Num = strtof(tmp.c_str(), NULL);
            tmp = split->getItemAtIndex(5);
            DepthKnob_Num = strtof(tmp.c_str(), NULL);

            // Set Foot Switch States to process the switches in loop()
            prev_ftSwitchLeftState = !digitalRead(ftSwitchLeft);
            prev_ftSwitchMiddleState = !digitalRead(ftSwitchMiddle);
            prev_ftSwitchRightState = !digitalRead(ftSwitchRight);
        }
    }
}

// Apply wireless settings
void adjustSettings()
{
    // Apply settings
    del.Reset(); // Reset Delayline
    del.SetDelay(delay_adj);
    dly.Reset(); // Reset Flanger Delayline
    dly.SetDelay(sweep);
    osc_trem.Reset(); // Reset Tremolo LFO
    osc_trem.SetFreq(rate_adj);
    osc_trem.SetAmp(depth_adj);
    osc_flng.Reset(); // Reset Flanger LFO
    osc_flng.SetFreq(speed_adj);
    osc_flng.SetAmp(range_adj);
    verb.Init(sample_rate); // Reset Reverb
    verb.SetLpFreq(hifreq_adj);
    verb.SetFeedback(amount_adj);
}

// Callback to process audio
void MyCallback(float** in, float** out, size_t size)
{
    // Process the effects 
    for (size_t i = 0; i < size; i++)
    {
        // Read from I/O
        out_buf = in[0][i];

        // Single momentary footswitch with TRS connection
        if (bypass_On != true)
        {
            if (ProcTremLeft || ProcTremMid || ProcTremRight)
            {   // ==================== Tremelo ==========================

                // Apply modulation to the dry signal
                out_buf = dcblk.Process(out_buf + (out_buf * osc_trem.Process()));
            }
                        
            if (ProcDistLeft || ProcDistMid || ProcDistRight)
            {   // =================== Distortion ========================

                timbreInverse = (1 - (gain_adj * 0.099)) * 10; //inverse scaling of timbre
                fuzz_buf = out_buf * mix_adj;
                fuzz_buf = tanh((fuzz_buf * (gain_adj + 1)));
                fuzz_buf = (fuzz_buf * ((0.1 + gain_adj) * timbreInverse));
                fuzz_buf = cos((fuzz_buf + (gain_adj + 0.25)));
                fuzz_buf = tanh(fuzz_buf * (gain_adj + 1));
                fuzz_buf = fuzz_buf * 0.125;
                out_buf = dcblk.Process(out_buf + fuzz_buf);
            }

            if (ProcFlngLeft || ProcFlngMid || ProcFlngRight)
            {   // ==================== Flanger ==========================

                /* My attempt to emulate an Electric Mistress circa 1981
                *  Flange Mode - Delay time is swept by LFO freq * LFO amplitude
                *  Filter Mode - LFO is ingnored and Delay time is set manually by Range control
                *  Rate - LFO freq adjust (speed_adj) (0 to 2 cps)
                *  Range - LFO amplitude adjust (0 to 1.0)
                *  Color - Delay regen (wet signal) feedback level (0 to 0.95)
                */

                if (flngFltr_On)
                {   // ~~~~~~~~~~ Filter Mode ~~~~~~~~~~
                    sweep = ((range_adj + 0.25f) * 0.75f) * 230.0f;
                }
                else
                {   // ~~~~~~~~~~ Flange Mode ~~~~~~~~~~
                    minDelay = 0.16f;
                    maxDelay = 225.0f;
                    spanDelay = (maxDelay - minDelay) / 2.0f;
                    centerDelay = minDelay + spanDelay;
                    sweep = centerDelay + (osc_flng.Process() * spanDelay);
                }

                // Adjust the Delayline 
                dly.SetDelay(sweep);

                // Read regen (wet signal) from Delay Line
                regen = dly.Read();

                // Write to Delay
                dly.Write((regen * color_adj) + out_buf);

                // Send to I/O
                out_buf = dcblk.Process((regen * color_adj) + out_buf);
            }

            if (ProcEchoLeft || ProcEchoMid || ProcEchoRight)
            {   // ====================== Echo ===========================

                // Read Wet from Delay Line
                wet = del.Read();

                // Write to Delay
                del.Write((wet * feedback_adj) + out_buf);

                // Send to I/O
                out_buf = dcblk.Process((wet * feedback_adj) + out_buf);
            }

            if (ProcRvrbLeft || ProcRvrbMid || ProcRvrbRight)
            {   // ===================== Reverb ==========================
                
                verb_in = out_buf;

                // Process the reverb
                verb.Process(verb_in, verb_in, &out1, &out2);

                // Send to I/O 
                out_buf = dcblk.Process(out_buf + out1 + out2);
            }
        }
        // Note: Line level audio is mono and tied to both audio inputs
        // Only Audio Out 2 is being used and Audio Out 1 is not connected       
        for (size_t chn = 0; chn < num_channels; chn++)
        {
            out[chn][i] = out_buf;
        }
    }
}

// Setup
void setup()
{
    // Foot Switches
    pinMode(BypassSwitch, INPUT_PULLUP);
    pinMode(ftSwitchLeft, INPUT_PULLUP);
    pinMode(ftSwitchMiddle, INPUT_PULLUP);
    pinMode(ftSwitchRight, INPUT_PULLUP);

    // LEDs
    pinMode(ledByp, OUTPUT);
    pinMode(ledLeft, OUTPUT);
    pinMode(ledMiddle, OUTPUT);
    pinMode(ledRight, OUTPUT);
    pinMode(LED_BUILTIN, OUTPUT);

    // Special Pins
    pinMode(espHasData, INPUT_PULLUP);
    // pinMode(SelectPin, OUTPUT);

    // Initialize Daisy Seed at 48kHz
    hw = DAISY.init(DAISY_SEED, AUDIO_SR_48K);
    num_channels = hw.num_channels;
    sample_rate = DAISY.get_samplerate();

    // Initialize Effects
    del.Init();
    dly.Init();
    verb.Init(sample_rate);
    osc_trem.Init(sample_rate);
    osc_flng.Init(sample_rate);
    dcblk.Init(sample_rate);

    // Initial Effects Mode
    ftSwLeft_Num = 1;   // Default: Tremolo
    ftSwMiddle_Num = 2; // Default: Distortion
    ftSwRight_Num = 4;  // Default: Flanger

    // Initiaize Knob Assignments
    RateKnob_Num = 1;  // Default: Tremolo:Rate
    DepthKnob_Num = 2; // Default: Tremolo:Depth

    // Set Foot Switch States to process the switches in loop()
    prev_ftSwitchLeftState = !digitalRead(ftSwitchLeft);
    prev_ftSwitchMiddleState = !digitalRead(ftSwitchMiddle);
    prev_ftSwitchRightState = !digitalRead(ftSwitchRight);

    // Used to indicate SPI data transfers
    digitalWrite(LED_BUILTIN, LOW); // Turn led off

    // Initially set all controls to midpoint or whatever
    delay_adj = 24000.00;
    rate_adj = 5.00;
    gain_adj = 0.50;
    feedback_adj = 0.50;
    depth_adj = 0.50;
    mix_adj = 0.50;
    hifreq_adj = 10000.00;
    amount_adj = 0.50;
    speed_adj = 0.20;
    range_adj = 0.75;
    color_adj = 0.60;
    sweep = 360.00;
    flangeFilter_type = 0;

    // Set Echo and Flanger delay times
    del.SetDelay(delay_adj);
    dly.SetDelay(sweep);

    // Set parameters for tremelo LFO 
    osc_trem.SetWaveform(Oscillator::WAVE_SIN);
    osc_trem.SetFreq(rate_adj);
    osc_trem.SetAmp(depth_adj);

    // Set parameters for flanger LFO 
    osc_flng.SetWaveform(Oscillator::WAVE_TRI);
    osc_flng.SetFreq(speed_adj);
    osc_flng.SetAmp(range_adj);

    // Set parameters for reverb
    verb.SetLpFreq(hifreq_adj);
    verb.SetFeedback(amount_adj);

    // Start SPI 
    SPI.begin();

    // Initialize Bypass
    delay(100); // Give Bypass footswitch pushbutton capacitor time to charge
    bypass_On = false; // Led is OFF when effects are bypassed
    digitalWrite(ledByp, HIGH);  // Turn on front panel led (Led ON = Not Bypassed)
    delay(20);

    // Send data to ESP8266
    digitalWrite(LED_BUILTIN, HIGH); // Turn led on
    sendData("R");
    delay(10);
    sendData("D");
    delay(10);
    sendData("B");
    delay(10);
    sendData("A");
    delay(10);
    sendData("M");
    delay(10);
    digitalWrite(LED_BUILTIN, LOW); // Turn led off

    // Start Audio
    DAISY.begin(MyCallback);
}

// Loop
void loop()
{
    // Poll knobs and wireless settings. Poll period is 100 ms and can be adjusted with var: interval 
    currentMillis = millis();
    if (currentMillis - previousMillis >= interval)
    {
        // save the last poll time
        previousMillis = currentMillis;

        // ===================== Check for wireless Settings change =====================
        if (digitalRead(espHasData) == HIGH)
        {
            if (espHasDataEnabled == true)
            {
                digitalWrite(LED_BUILTIN, HIGH); // Turn led on
                delay(10);
                receiveData(esp.readData());
                sendData("a");
                delay(10);
                receiveData(esp.readData());
                sendData("b");
                delay(10);
                receiveData(esp.readData());
                sendData("c");
                delay(10);
                receiveData(esp.readData());
                sendData("d");
                delay(10);
                receiveData(esp.readData());
                adjustSettings();
                espHasDataEnabled = false;
                digitalWrite(LED_BUILTIN, LOW); // Turn led off
            }
        }
        else
        {
            if (espHasDataEnabled == false)
            {
                espHasDataEnabled = true;
            }
        }

        // ================== Check for Bypass Footswitch activity =====================
        // Bypass Footswitch is momentary (effects are bypassed the led is Off)
        if (digitalRead(BypassSwitch))
        {
            if (btnBypassEnabled == true)
            {
                digitalWrite(ledByp, !(digitalRead(ledByp)));
                btnBypassEnabled = false;
                if (digitalRead(ledByp) == LOW) // Led is Off
                {
                    del.Reset(); // Reset Delayline
                    del.SetDelay(delay_adj);
                    dly.Reset(); // Reset Flanger Delayline
                    dly.SetDelay(sweep);
                    osc_flng.Reset(); // Reset Flanger LFO
                    osc_trem.Reset(); // Reset Tremolo LFO
                    verb.Init(sample_rate); // Reset Reverb
                    verb.SetLpFreq(hifreq_adj);
                    verb.SetFeedback(amount_adj);
                }
            }
        }
        else
        {
            if (btnBypassEnabled == false)
            {
                btnBypassEnabled = true;
            }
        }
        if (digitalRead(ledByp) == HIGH)
        {
            bypass_On = false; // Led is On
        }
        else
        {
            bypass_On = true; // Led is Off
        }

        // ================ Set Footswitch/LED states (push-on/push-off) ================
        if (digitalRead(ftSwitchLeft) != prev_ftSwitchLeftState)
        {
            if (prev_ftSwitchLeftState)
            {
                prev_ftSwitchLeftState = false;
                digitalWrite(ledLeft, LOW);
                if (bitRead(ftSwLeft_Num, 0))
                {
                    ProcTremLeft = false;
                    osc_trem.Reset(); // Reset Tremolo LFO
                    osc_trem.SetFreq(rate_adj);
                    osc_trem.SetAmp(depth_adj);
                }
                if (bitRead(ftSwLeft_Num, 1))
                {
                    ProcDistLeft = false;
                }
                if (bitRead(ftSwLeft_Num, 2))
                {
                    ProcFlngLeft = false;
                    dly.Reset(); // Reset Flanger Delayline
                    dly.SetDelay(sweep);
                    osc_flng.Reset(); // Reset Flanger LFO
                    osc_flng.SetFreq(speed_adj);
                    osc_flng.SetAmp(range_adj);
                }
                if (bitRead(ftSwLeft_Num, 3))
                {
                    ProcEchoLeft = false;
                    del.Reset(); // Reset Delayline
                    del.SetDelay(delay_adj);
                }
                if (bitRead(ftSwLeft_Num, 4))
                {
                    ProcRvrbLeft = false;
                    verb.Init(sample_rate);
                    verb.SetLpFreq(hifreq_adj);
                    verb.SetFeedback(amount_adj);
                }
            }
            else
            {
                prev_ftSwitchLeftState = true;
                digitalWrite(ledLeft, HIGH);
                ProcTremLeft = (bitRead(ftSwLeft_Num, 0)) ? true : false;
                ProcDistLeft = (bitRead(ftSwLeft_Num, 1)) ? true : false;
                ProcFlngLeft = (bitRead(ftSwLeft_Num, 2)) ? true : false;
                ProcEchoLeft = (bitRead(ftSwLeft_Num, 3)) ? true : false;
                ProcRvrbLeft = (bitRead(ftSwLeft_Num, 4)) ? true : false;
            }
        }
        if (digitalRead(ftSwitchMiddle) != prev_ftSwitchMiddleState)
        {
            if (digitalRead(ftSwitchMiddle) == LOW)
            {
                prev_ftSwitchMiddleState = false;
                digitalWrite(ledMiddle, LOW);
                if (bitRead(ftSwMiddle_Num, 0))
                {
                    ProcTremMid = false;
                    osc_trem.Reset(); // Reset Tremolo LFO
                    osc_trem.SetFreq(rate_adj);
                    osc_trem.SetAmp(depth_adj);
                }
                if (bitRead(ftSwMiddle_Num, 1))
                {
                    ProcDistMid = false;
                }
                if (bitRead(ftSwMiddle_Num, 2))
                {
                    ProcFlngMid = false;
                    dly.Reset(); // Reset Flanger Delayline
                    dly.SetDelay(sweep);
                    osc_flng.Reset(); // Reset Flanger LFO
                    osc_flng.SetFreq(speed_adj);
                    osc_flng.SetAmp(range_adj);
                }
                if (bitRead(ftSwMiddle_Num, 3))
                {
                    ProcEchoMid = false;
                    del.Reset(); // Reset Delayline
                    del.SetDelay(delay_adj);
                }
                if (bitRead(ftSwMiddle_Num, 4))
                {
                    ProcRvrbMid = false;
                    verb.Init(sample_rate);
                    verb.SetLpFreq(hifreq_adj);
                    verb.SetFeedback(amount_adj);
                }
            }
            else
            {
                prev_ftSwitchMiddleState = true;
                digitalWrite(ledMiddle, HIGH);
                ProcTremMid = (bitRead(ftSwMiddle_Num, 0)) ? true : false;
                ProcDistMid = (bitRead(ftSwMiddle_Num, 1)) ? true : false;
                ProcFlngMid = (bitRead(ftSwMiddle_Num, 2)) ? true : false;
                ProcEchoMid = (bitRead(ftSwMiddle_Num, 3)) ? true : false;
                ProcRvrbMid = (bitRead(ftSwMiddle_Num, 4)) ? true : false;
            }
        }
        if (digitalRead(ftSwitchRight) != prev_ftSwitchRightState)
        {
            if (digitalRead(ftSwitchRight) == LOW)
            {
                prev_ftSwitchRightState = false;
                digitalWrite(ledRight, LOW);
                if (bitRead(ftSwRight_Num, 0))
                {
                    ProcTremRight = false;
                    osc_trem.Reset(); // Reset Tremolo LFO
                    osc_trem.SetFreq(rate_adj);
                    osc_trem.SetAmp(depth_adj);
                }
                if (bitRead(ftSwRight_Num, 1))
                {
                    ProcDistRight = false;
                }
                if (bitRead(ftSwRight_Num, 2))
                {
                    ProcFlngRight = false;
                    dly.Reset(); // Reset Flanger Delayline
                    dly.SetDelay(sweep);
                    osc_flng.Reset(0.5f); // Reset Flanger LFO
                    osc_flng.SetFreq(speed_adj);
                    osc_flng.SetAmp(range_adj);
                }
                if (bitRead(ftSwRight_Num, 3))
                {
                    ProcEchoRight = false;
                    del.Reset(); // Reset Delayline
                    del.SetDelay(delay_adj);
                }
                if (bitRead(ftSwRight_Num, 4))
                {
                    ProcRvrbRight = false;
                    verb.Init(sample_rate);
                    verb.SetLpFreq(hifreq_adj);
                    verb.SetFeedback(amount_adj);
                }
            }
            else
            {
                prev_ftSwitchRightState = true;
                digitalWrite(ledRight, HIGH);
                ProcTremRight = (bitRead(ftSwRight_Num, 0)) ? true : false;
                ProcDistRight = (bitRead(ftSwRight_Num, 1)) ? true : false;
                ProcFlngRight = (bitRead(ftSwRight_Num, 2)) ? true : false;
                ProcEchoRight = (bitRead(ftSwRight_Num, 3)) ? true : false;
                ProcRvrbRight = (bitRead(ftSwRight_Num, 4)) ? true : false;
            }
        }

        // ================= Read Knobs (return range is 0 to 1023) =====================
        
        // ============ Set only when knob 1 (Rate) position has changed ================

        // Average 10 reads
        tot_knob1val = 0;
        for (int i = 1; i < 11; i++)
        {
            tot_knob1val = tot_knob1val + analogRead(knob1Pin); // Read Knob
            delay(1);
        }
        knob1val = tot_knob1val / 10;

        // Only when knob value has changed
        if (abs(prev_knob1val - knob1val) > 2)
        {
            prev_knob1val = knob1val;

            switch (RateKnob_Num)
            {
            case 0:
                // NA: (Not Assigned)
                break;
            case 1:
                // Tremelo (sweep) Rate Scale: 0.00 to 10.0
                rate_adj = knob1val / 102.30f;
                osc_trem.SetFreq(rate_adj);
                sendData("R");
                break;
            case 2:
                // Tremelo Depth Scale: 0.00 to 1.00
                depth_adj = knob1val / 1023.00f;
                osc_trem.SetAmp(depth_adj);
                sendData("D");
                break;
            case 3:
                // Distortion Drive   Knob Scale: 0.00 to 1.00
                gain_adj = knob1val / 1023.00f;
                sendData("R");
                break;
            case 4:
                // Distortion Mix    Knob Scale: 0.00 to 1.00
                mix_adj = knob1val / 1023.00f;
                sendData("D");
                break;
            case 5:
                // Flanger: Rate       Knob Scale: 0.00 to 2.00
                speed_adj = knob1val / 511.50f;
                osc_flng.SetFreq(speed_adj);
                sendData("B");
                break;
            case 6:
                // Flanger: Range      Knob Scale: 0.05 to 0.90
                range_adj = (knob2val + 5) / 1140.00f;
                osc_flng.SetAmp(range_adj);
                sendData("A");
                break;
            case 7:
                // Flanger: Color      Knob Scale: 0.00 to 0.95
                color_adj = knob1val / 1075.00f;
                sendData("A");
                break;
            case 8:
                // Echo Delay time        Knob Scale: 48 to 48000
                delay_adj = map(knob1val, 0, 1023, (sample_rate / 1000), sample_rate);
                del.Reset();
                del.SetDelay(delay_adj);
                sendData("R");
                break;
            case 9:
                // Echo Feedback          Knob Scale: 0.00 to 0.95
                feedback_adj = knob1val / 1075.00f;
                sendData("D");
                break;
            case 10:
                // Reverb: Hi Freq Cut Off   Knob Scale: 100 to 20000
                hifreq_adj = map(knob1val, 0, 1023, 100, 20000);
                verb.SetLpFreq(hifreq_adj);
                sendData("B");
                break;
            case 11:
                // Reverb: Depth        Knob Scale: 0.00 to 0.95
                amount_adj = knob1val / 1075.00f;
                verb.SetFeedback(amount_adj);
                sendData("B");
                break;
            }
            delay(10);
        }

        // ============ Set only when knob 2 (Depth) position has changed ===============
        
        // Average 10 reads
        tot_knob2val = 0;
        for (int i = 1; i < 11; i++)
        {
            tot_knob2val = tot_knob2val + analogRead(knob2Pin); // Read Knob
            delay(1);
        }
        knob2val = tot_knob2val / 10;

        // Only when knob value has changed
        if (abs(prev_knob2val - knob2val) > 2)
        {
            prev_knob2val = knob2val;

            switch (DepthKnob_Num)
            {
            case 0:
                // NA: (Not Assigned)
                break;
            case 1:
                // Tremelo (sweep) Rate Scale: 0.00 to 10.0
                rate_adj = knob2val / 102.30f;
                osc_trem.SetFreq(rate_adj);
                sendData("R");
                break;
            case 2:
                // Tremelo Depth Scale: 0.00 to 1.00
                depth_adj = knob2val / 1023.00f;
                osc_trem.SetAmp(depth_adj);
                sendData("D");
                break;
            case 3:
                // Distortion Drive   Knob Scale: 0.00 to 1.00
                gain_adj = knob2val / 1023.00f;
                sendData("R");
                break;
            case 4:
                // Distortion Mix    Knob Scale: 0.00 to 1.00
                mix_adj = knob2val / 1023.00f;
                sendData("D");
                break;
            case 5:
                // Flanger: Rate       Knob Scale: 0.00 to 2.00
                speed_adj = knob2val / 511.50f;
                osc_flng.SetFreq(speed_adj);
                sendData("B");
                break;
            case 6:
                // Flanger: Range      Knob Scale: 0.05 to 0.90
                range_adj = (knob2val + 5) / 1140.00f;
                osc_flng.SetAmp(range_adj);
                sendData("A");
                break;
            case 7:
                // Flanger: Color      Knob Scale: 0.00 to 0.95
                color_adj = knob2val / 1075.00f;
                sendData("A");
                break;
            case 8:
                // Echo Delay time        Knob Scale: 48 to 48000
                delay_adj = map(knob2val, 0, 1023, (sample_rate / 1000), sample_rate);
                del.Reset();
                del.SetDelay(delay_adj);
                sendData("R");
                break;
                
            case 9:
                // Echo Feedback          Knob Scale: 0.00 to 0.95
                feedback_adj = knob2val / 1075.00f;
                sendData("D");
                break;
            case 10:
                // Reverb: Hi Freq Cut Off   Knob Scale: 100 to 20000
                hifreq_adj = map(knob2val, 0, 1023, 100, 20000);
                verb.SetLpFreq(hifreq_adj);
                sendData("B");
                break;
            case 11:
                // Reverb: Depth        Knob Scale: 0.00 to 0.95
                amount_adj = knob2val / 1075.00f;
                verb.SetFeedback(amount_adj);
                sendData("B");
                break;
            }
            delay(10);
        }
    }
}
