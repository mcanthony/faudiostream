/************************************************************************
 
 IMPORTANT NOTE : this file contains two clearly delimited sections :
 the ARCHITECTURE section (in two parts) and the USER section. Each section
 is governed by its own copyright and license. Please check individually
 each section for license and copyright information.
 *************************************************************************/

/*******************BEGIN ARCHITECTURE SECTION (part 1/2)****************/

/************************************************************************
 FAUST Architecture File
 Copyright (C) 2013 Reza Payami
 All rights reserved.
 ----------------------------BSD License------------------------------
 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions
 are met:
 
 * Redistributions of source code must retain the above copyright
 notice, this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above
 copyright notice, this list of conditions and the following
 disclaimer in the documentation and/or other materials provided
 with the distribution.
 * Neither the name of Remy Muller nor the names of its
 contributors may be used to endorse or promote products derived
 from this software without specific prior written permission.
 
 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 OF THE POSSIBILITY OF SUCH DAMAGE.
 
 ----------------------------Audio Unit SDK----------------------------------
 In order to compile a AU (TM) Synth plugin with this architecture file
 you will need the proprietary AU SDK from Apple. Please check
 the corresponding license.
 
 ************************************************************************
 ************************************************************************/

#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include <string>
#include <vector>

#include "faust/misc.h"
#include "faust/audio/dsp.h"

#include "faust/au/AUUI.h"

#include "AUInstrumentBase.h"
#include "FaustAUSynthVersion.h"

using namespace std;

static const UInt32 kNumNotes = 32;
static const UInt32 kMaxActiveNotes = 32;

static const char* FREQ_PARAM_NAME = "freq";
static const char* ATTACK_PARAM_NAME = "attack";
static const char* RELEASE_PARAM_NAME = "release";

/******************************************************************************
 *******************************************************************************
 *
 *	VECTOR INTRINSICS
 *
 *******************************************************************************
 *******************************************************************************/

<<includeIntrinsic>>


/********************END ARCHITECTURE SECTION (part 1/2)****************/

/**************************BEGIN USER SECTION **************************/

<<includeclass>>

/***************************END USER SECTION ***************************/

/*******************BEGIN ARCHITECTURE SECTION (part 2/2)***************/
class FaustAUSynth;

struct FaustAUSynthNote: public SynthNote {
	FaustAUSynthNote();
    
	virtual ~FaustAUSynthNote();
    
	virtual OSStatus Initialize();
    
	virtual bool Attack(const MusicDeviceNoteParams &inParams) {
		double sampleRate = SampleRate();
        
		amp = inParams.mVelocity / 127.;
        
		return true;
	}
	virtual void Kill(UInt32 inFrame); // voice is being stolen.
	virtual void Release(UInt32 inFrame);
	virtual void FastRelease(UInt32 inFrame);
	virtual Float32 Amplitude() {
		return amp;
	} // used for finding quietest note for voice stealing.
	virtual OSStatus Render(UInt64 inAbsoluteSampleFrame, UInt32 inNumFrames,
                            AudioBufferList** inBufferList, UInt32 inOutBusCount);
    
	FaustAUSynth* synth;
    
	double amp, maxAmp, upSlope, dnSlope;

	
public:
	auUI* dspUI = NULL;
    
	mydsp* dsp = NULL;
};

class FaustAUSynth: public AUMonotimbralInstrumentBase {
public:
	FaustAUSynth(ComponentInstance inComponentInstance);
	virtual ~FaustAUSynth();
    
	virtual OSStatus Initialize();
	virtual void Cleanup();
	virtual OSStatus Version() {
		return kFaustAUSynthVersion;
	}
    
	virtual AUElement* CreateElement(AudioUnitScope scope,
                                     AudioUnitElement element);
    
	virtual OSStatus GetParameterInfo(AudioUnitScope inScope,
                                      AudioUnitParameterID inParameterID,
                                      AudioUnitParameterInfo & outParameterInfo);
    
	void SetParameter(AudioUnitParameterID paramID,
                      AudioUnitParameterValue value);
    
	virtual OSStatus SetParameter(AudioUnitParameterID inID,
                                  AudioUnitScope inScope, AudioUnitElement inElement,
                                  AudioUnitParameterValue inValue, UInt32);
    
	MidiControls* GetControls(MusicDeviceGroupID inChannel) {
		SynthGroupElement *group = GetElForGroupID(inChannel);
		return (MidiControls *) group->GetMIDIControlHandler();
	}
    
private:
    
	FaustAUSynthNote mNotes[kNumNotes];
    
private:
	auUI* dspUI = NULL;
    
public:
	mydsp* dsp = NULL;
    
	int frequencyParameterID = -1;
	int attackParameterID = -1;
	int releaseParameterID = -1;
	
    
};



/**********************************************************************************/

AUDIOCOMPONENT_ENTRY(AUMusicDeviceFactory, FaustAUSynth)

//TODO set the output channels
FaustAUSynth::FaustAUSynth(ComponentInstance inComponentInstance)
: AUMonotimbralInstrumentBase(inComponentInstance, 0, 1)
{
	CreateElements();
    
	dspUI = new auUI();
    
	dsp = new mydsp();
    
	//int inChannels = dsp->getNumInputs();
	//int outChannels = dsp->getNumOutputs();
    
	//SetParamHasSampleRateDependency(true);
    
	dsp->buildUserInterface(dspUI);
    
	Globals()->UseIndexedParameters(dspUI->fUITable.size());
    
	if (dspUI)
        for (int i = 0; i < dspUI->fUITable.size(); i++)
            if (dspUI->fUITable[i] && dspUI->fUITable[i]->fZone)
            {
                if (dynamic_cast<auButton*>(dspUI->fUITable[i])) {
                    Globals()->SetParameter(i, 0);
                }
                else if (dynamic_cast<auToggleButton*>(dspUI->fUITable[i])) {
                    Globals()->SetParameter(i, 0);
                }
                else if (dynamic_cast<auCheckButton*>(dspUI->fUITable[i])) {
                    Globals()->SetParameter(i, 0);
                }
                else {
                    auSlider* slider = (auSlider*)dspUI->fUITable[i];
                    Globals()->SetParameter(i, slider->fInit );
                }
            }
}

FaustAUSynth::~FaustAUSynth() {
	if (dsp)
		delete dsp;
    
	if (dspUI)
		delete dspUI;
}

void FaustAUSynth::Cleanup() {
    
}

OSStatus FaustAUSynth::Initialize() {
	OSStatus result = AUMonotimbralInstrumentBase::Initialize();
    
	for (int i = 0; i < kNumNotes; i++) {
		mNotes[i].Initialize();
		mNotes[i].synth = this;
	}
    
	SetNotes(kNumNotes, kMaxActiveNotes, mNotes, sizeof(FaustAUSynthNote));
    
	return result;
}

AUElement* FaustAUSynth::CreateElement(AudioUnitScope scope,
                                       AudioUnitElement element) {
	switch (scope) {
        case kAudioUnitScope_Group:
            return new SynthGroupElement(this, element, new MidiControls);
        case kAudioUnitScope_Part:
            return new SynthPartElement(this, element);
        default:
            return AUBase::CreateElement(scope, element);
	}
}

OSStatus FaustAUSynth::GetParameterInfo(AudioUnitScope inScope,
                                        AudioUnitParameterID inParameterID,
                                        AudioUnitParameterInfo & outParameterInfo) {
    
	OSStatus result = noErr;
    
	char name[100];
	CFStringRef str;
    
	outParameterInfo.flags = kAudioUnitParameterFlag_IsWritable
    + kAudioUnitParameterFlag_IsReadable;
    
	if (inScope == kAudioUnitScope_Global) {
        
		if (dspUI && dspUI->fUITable[inParameterID]
            && dspUI->fUITable[inParameterID]->fZone) {
            
			if (dynamic_cast<auButton*>(dspUI->fUITable[inParameterID])) {
				auToggleButton* toggle =
                (auToggleButton*) dspUI->fUITable[inParameterID];
				toggle->GetName(name);
				str = CFStringCreateWithCString(kCFAllocatorDefault, name, 0);
                
				AUBase::FillInParameterName(outParameterInfo, str, false);
				outParameterInfo.unit = kAudioUnitParameterUnit_Boolean;
				outParameterInfo.minValue = 0;
				outParameterInfo.maxValue = 1;
				outParameterInfo.defaultValue = 0;
			} else if (dynamic_cast<auToggleButton*>(dspUI->fUITable[inParameterID])) {
				auToggleButton* toggle =
                (auToggleButton*) dspUI->fUITable[inParameterID];
				toggle->GetName(name);
                
				str = CFStringCreateWithCString(kCFAllocatorDefault, name, 0);
                
				AUBase::FillInParameterName(outParameterInfo, str, false);
				outParameterInfo.unit = kAudioUnitParameterUnit_Boolean;
				outParameterInfo.minValue = 0;
				outParameterInfo.maxValue = 1;
				outParameterInfo.defaultValue = 0;
                
			} else if (dynamic_cast<auCheckButton*>(dspUI->fUITable[inParameterID])) {
				auCheckButton* check =
                (auCheckButton*) dspUI->fUITable[inParameterID];
				check->GetName(name);
                
				str = CFStringCreateWithCString(kCFAllocatorDefault, name, 0);
                
				AUBase::FillInParameterName(outParameterInfo, str, false);
				outParameterInfo.unit = kAudioUnitParameterUnit_Boolean;
				outParameterInfo.minValue = 0;
				outParameterInfo.maxValue = 1;
				outParameterInfo.defaultValue = 0;
                
			} else {
                
				auSlider* slider = (auSlider*) dspUI->fUITable[inParameterID];
				slider->GetName(name);
                
				//TODO the default parameter name which is set by MIDI note in
				if (!strcmp(name, "freq")) {
					frequencyParameterID = inParameterID;
				}
                
				str = CFStringCreateWithCString(kCFAllocatorDefault, name, 0);
                
				AUBase::FillInParameterName(outParameterInfo, str, false);
				outParameterInfo.unit = kAudioUnitParameterUnit_Generic;
				outParameterInfo.minValue = slider->fMin;
				outParameterInfo.maxValue = slider->fMax;
				outParameterInfo.defaultValue = slider->fInit;
			}
            
		}
	} else {
		result = kAudioUnitErr_InvalidParameter;
	}
    
	return result;
    
}

OSStatus FaustAUSynth::SetParameter(AudioUnitParameterID inID,
                                    AudioUnitScope inScope, AudioUnitElement inElement,
                                    AudioUnitParameterValue inValue, UInt32 inBufferOffsetInFrames) {
	if (inScope == kAudioUnitScope_Global) {
        
		if (dspUI) {
			if (dspUI->fUITable[inID] && dspUI->fUITable[inID]->fZone)
                
				*(dspUI->fUITable[inID]->fZone) = (FAUSTFLOAT) inValue;
            
			for (int i = 0; i < kNumNotes; i++) {
				if (mNotes[i].dspUI)
					*(mNotes[i].dspUI->fUITable[inID]->fZone) =
                    (FAUSTFLOAT) inValue;
			}
		}
	}
    
	return AUMonotimbralInstrumentBase::SetParameter(inID, inScope, inElement,
                                                     inValue, inBufferOffsetInFrames);
}

/**********************************************************************************/

FaustAUSynthNote::FaustAUSynthNote() {
    
}

OSStatus FaustAUSynthNote::Initialize() {
	dspUI = new auUI();
	dsp = new mydsp();
    
	dsp->buildUserInterface(dspUI);
    
	//TODO find a way to call GetSampleRate(), there will be a NullPointerException if it is called
	if (dsp)
		dsp->init(44100);
    
	return noErr;
}

FaustAUSynthNote::~FaustAUSynthNote() {
	if (dsp)
		delete dsp;
    
	if (dspUI)
		delete dspUI;
}

void FaustAUSynthNote::Release(UInt32 inFrame) {
	SynthNote::Release(inFrame);
}

void FaustAUSynthNote::FastRelease(UInt32 inFrame) // voice is being stolen.
{
	SynthNote::Release(inFrame);
}

void FaustAUSynthNote::Kill(UInt32 inFrame) // voice is being stolen.
{
	SynthNote::Kill(inFrame);
}

OSStatus FaustAUSynthNote::Render(UInt64 inAbsoluteSampleFrame,
                                  UInt32 inNumFrames, AudioBufferList** inBufferList,
                                  UInt32 inOutBusCount) {
	int MAX_OUT_CHANNELS = 1000;
    
	float* outBuffer[MAX_OUT_CHANNELS];
	float* audioData[MAX_OUT_CHANNELS];
    
	int inChannels = dsp->getNumInputs();
	int outChannels = dsp->getNumOutputs();
    
    // bus number is assumed to be zero
    for (int i = 0; i < outChannels; i++) {
		outBuffer[i] = new float[inNumFrames];
        
		audioData[i] = (float*) inBufferList[0]->mBuffers[i].mData;
	}
    
	if (synth) {
		auSlider* frequencySlider = NULL;
		if (synth->frequencyParameterID != -1) {
			if (dspUI)
				frequencySlider =
                (auSlider*) dspUI->fUITable[synth->frequencyParameterID];
			if (frequencySlider)
				//TODO change the SetValue function call accordingly
                frequencySlider->SetValue((Frequency() - 20 )/ ((float)(SampleRate() / 2)));
			//frequencySlider->SetValue((float) GetMidiKey() / 88.0);
		}
        
		dsp->compute(inNumFrames, audioData, outBuffer);
	}
	switch (GetState()) {
        case kNoteState_Attacked:
        case kNoteState_Sostenutoed:
        case kNoteState_ReleasedButSostenutoed:
        case kNoteState_ReleasedButSustained: {
            for (int i = 0; i < outChannels; i++) {
                for (UInt32 frame = 0; frame < inNumFrames; ++frame) {
                    audioData[i][frame] += outBuffer[i][frame] * amp;
                }
            }
            break;
            
        }
            
        case kNoteState_Released:
        case kNoteState_FastReleased: {
            
            NoteEnded(0xFFFFFFFF);
            
            break;
        }
        default:
            break;
	}
	return noErr;
    
}

/********************END ARCHITECTURE SECTION (part 2/2)****************/