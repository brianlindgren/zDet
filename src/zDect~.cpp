#include "m_pd.h"
#include <math.h>
#include <string>

// Define the evString class for zero-crossing detection
class evString {
public:
    evString(int sampleRate, float stringHz, float stringHzHi);
    ~evString();

    float process(float in); // main loop

protected:
private:
    int sampleRate_;

    // pitch detection
    float stringHz_;
    float stringHzHi_;
    bool prevPitchDetectionFlag_;
    float sampLen_;
    float frequency_;
    float prevFrequency_; // only needed for printing purposes now
    float pitchDecLo_;
    float pitchDecHi_;
    int pitchDetectionTimer_;

    void firstSetup(int rate);
    void recalculateSettings();
};

// Implementation of evString methods
evString::evString(int sampleRate, float stringHz, float stringHzHi) : sampleRate_(sampleRate), stringHz_(stringHz), stringHzHi_(stringHzHi),
                                                     prevPitchDetectionFlag_(true), sampLen_(1.0 / sampleRate),
                                                     frequency_(0.0), prevFrequency_(0.0), pitchDetectionTimer_(0) {
    firstSetup(sampleRate);
}

evString::~evString() {
}

void evString::firstSetup(int rate) {
    sampleRate_ = rate;
    sampLen_ = 1.0 / rate;
    recalculateSettings();
}

void evString::recalculateSettings() {
    // pitch detection limits
    pitchDecLo_ = (stringHz_); // 5 Hz below
    pitchDecHi_ = 1 / ((stringHzHi_) / sampleRate_);
}

float evString::process(float in) {
    bool pitchDetectionFlag = false; // declare outside of if statements

    // 1 is (+) wavelength, 0 is (-)
    if (in > 0) { // 'in' is the current sample
        pitchDetectionFlag = true; // 1 = positive sample value
    } else if (in < 0) { // 0 = positive sample value
        pitchDetectionFlag = false;
    }

    // if we have a zero crossing and the wave is coming up from below the zero line
    if (pitchDetectionFlag != prevPitchDetectionFlag_ && prevPitchDetectionFlag_ == false
        && pitchDetectionTimer_ >= pitchDecHi_) {
        float freq = 1 / (pitchDetectionTimer_ * sampLen_);

        // if the calculated frequency is above the string's tuned pitch, let's use it!
        if (freq > pitchDecLo_) {
            frequency_ = freq;

            // only for printing purposes. print if new freq is more than 2 Hz different than old
            if (fabsf(prevFrequency_ - frequency_) > 2) {
                // Remove the printf call since it might not be suitable for Pure Data
                // printf("Pitch is %f:\n", frequency_);
            }

            prevFrequency_ = frequency_;
        }
        pitchDetectionTimer_ = 0; // reset timer regardless if the frequency was too low
    }

    // needs to be outside of an if/else. need to run every sample
    pitchDetectionTimer_++;
    prevPitchDetectionFlag_ = pitchDetectionFlag; // keep track of zero crossings

    return frequency_;
}

// Pure Data external definition
static t_class *zDect_class;

typedef struct _zDect {
    t_object x_obj;
    t_sample f;
    evString *string;
    t_float detection_on; // 0 for off, 1 for on
} t_zDect;

// Constructor
void *zDect_new(t_floatarg hz, t_floatarg hzHi) {
    t_zDect *x = (t_zDect *)pd_new(zDect_class);

    // Create a control inlet for detection on/off
    floatinlet_new(&x->x_obj, &x->detection_on);

    // Create a signal outlet
    outlet_new(&x->x_obj, &s_signal);

    // Initialize evString with the provided frequencies or default to 130 Hz and 880 Hz
    x->string = new evString(sys_getsr(), hz != 0 ? hz : 130, hzHi != 0 ? hzHi : 880);
    x->detection_on = 1; // start with detection on

    return (void *)x;
}

// Destructor
void zDect_free(t_zDect *x) {
    delete x->string;
}

// DSP Routine
t_int *zDect_perform(t_int *w) {
    t_zDect *x = (t_zDect *)(w[1]);
    t_sample *in = (t_sample *)(w[2]);
    t_sample *out = (t_sample *)(w[3]);
    int n = (int)(w[4]);

    for (int i = 0; i < n; i++) {
        if (x->detection_on) {
            out[i] = x->string->process(in[i]);
        } else {
            out[i] = 0; // output 0 when detection is off
        }
    }

    return (w + 5);
}

// DSP Setup
void zDect_dsp(t_zDect *x, t_signal **sp) {
    dsp_add(zDect_perform, 4, x, sp[0]->s_vec, sp[1]->s_vec, sp[0]->s_n);
}

// Setup Function
extern "C" void zDect_tilde_setup(void) {
    zDect_class = class_new(gensym("zDect~"),
                            (t_newmethod)zDect_new,
                            (t_method)zDect_free,
                            sizeof(t_zDect),
                            CLASS_DEFAULT,
                            A_DEFFLOAT, A_DEFFLOAT, 0);

    CLASS_MAINSIGNALIN(zDect_class, t_zDect, f);
    class_addmethod(zDect_class, (t_method)zDect_dsp, gensym("dsp"), A_CANT, 0);
}
