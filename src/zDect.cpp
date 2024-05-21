#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <math.h>
#include <sstream>

#include "wavetable.h"
#include "Biquad.h"


class evString {
	public:
		evString(int sampleRate, std::string name);
		~evString();
		
		void firstSetup(float rate, std::string name);
		
		void msgIn(const std::string& module, const std::string& parameter, float value); //receive parameters from .txt or GUI
		void recalculateSettings();
		
		float process(float amp, float pitch); //main loop
		
		

	protected:
	private:
		int sampleRate_;
		std::string instanceName; //string name
		int counterUtil;
		
		//amplitude detection, envelope following & debounce
		Biquad highpassDC_;
		Biquad lop_;
		Biquad lop20_;
		
		const float ampThresholdOn = .01; //amplitude needed to trigger a note on
		const float ampThresholdOff = .01; //amplitude needed to trigger a note off
		const int ampDebounceOn = .1 * 44100; //number of samples to wait for debounce when turning on
		const int ampDebounceOff = .02 * 44100; //number of samples to wait for debounce when turning off
		int ampDebounceTimer;
		int ampState = kStateOff;
		bool ampOnOff = 0;
		
		enum {
			kStateJustOn = 0,
			kStateOn,
			kStateJustOff,
			kStateOff
		};
		
		//pitch detection
		float stringHz_ = 0;
		bool prevPitchDetectionFlag_ = 1;
		float sampLen_;
		float frequency_ = 0.0;
		float prevFrequency_; //only need for printing purposes now
		float pitchDecLo_;
		float pitchDecHi_;
		int pitchDetectionTimer_ = 0;
		
		//input
		bool stringSwitch_ = 0;
		float pitchAdj_ = 1.0;
		
		//synthesis
		int kWavetableSize_;
		int kNumOscillators_;
		std::vector<Wavetable> gOscillators_; //generic. # of wavetables is based on kNumOscillators_. uses sine waves.
		
		std::vector<float> gAmplitudesSq_; //specific to the square wave
		std::vector<float> gAmplitudesSaw_; //specific to the saw wave
	
		float squareLevel_;

};



//---------------------

bool pitchDectectionFlag = 0;	//declare outside of if statements

//1 is (+) wavelength, 0 is (-)
if(in > 0){
	pitchDectectionFlag = 1;
}
else if(in < 0) {
	pitchDectectionFlag = 0;
}

//if we have a zero crossing and the wave is coming up from below the zero line
if(pitchDectectionFlag != prevPitchDetectionFlag_ &&  prevPitchDetectionFlag_ == 0 
&& pitchDetectionTimer_ >= pitchDecHi_)
	{ 
	float freq = 1 / (pitchDetectionTimer_ * sampLen_);
	
	//if the calculated frequency is above the string's tuned pitch, let's use it!
	if(freq > pitchDecLo_){
		frequency_ = freq;
		
		//only for printing purposes. print if new freq is more than 2 hz different than old
		if(fabsf(prevFrequency_ - frequency_) > 2) { 
		rt_printf("'%s': pitch is %f:\n", instanceName.c_str(), frequency_);
		}

		prevFrequency_ = frequency_; 
	}
	pitchDetectionTimer_ = 0; //reset timer regardless if the frequency was too low

	}
	
//needs to be outside of an if/else. need to run every sample
pitchDetectionTimer_++; 
prevPitchDetectionFlag_ = pitchDectectionFlag; // keep track of zero crossings
