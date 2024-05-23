#include "m_pd.h"
#include <math.h>
#include <string>

// Define the evString class for zero-crossing detection
class evString {
public:
    evString(int sampleRate, float stringHz, float stringHzHi);
    ~evString();

    float process(float in); // main loop
    void setLowRange(float lowRange) { stringHz_ = lowRange; recalculateSettings(); }
    void setHighRange(float highRange) { stringHzHi_ = highRange; recalculateSettings(); }
    void setSampleRate(int sampleRate) { sampleRate_ = sampleRate; firstSetup(sampleRate_); }
    bool isFreqOutRange() const { return freqOutRange_; }
    bool isFreqOutDect() const { return freqOutDect_; }

protected:
private:
    int sampleRate_;

    // pitch detection
    float stringHz_;
    float stringHzHi_;
    bool prevPitchDetectionFlag_;
    bool freqOutRange_;
    bool freqOutDect_;
    double sampLen_;
    float frequency_;
    float frequencyNew_;
    float frequencyPrev_ = 0;
    float pitchDecLo_;
    float pitchDecHi_;
    int pitchDetectionTimer_;

    void firstSetup(int rate);
    void recalculateSettings();
};

// Implementation of evString methods
evString::evString(int sampleRate, float stringHz, float stringHzHi)
    : sampleRate_(sampleRate), stringHz_(stringHz), stringHzHi_(stringHzHi),
      prevPitchDetectionFlag_(true), freqOutRange_(false), freqOutDect_(false),
      sampLen_(1.0 / sampleRate), frequency_(0.0), pitchDetectionTimer_(0) {
    firstSetup(sampleRate);
}

evString::~evString() {}

void evString::firstSetup(int rate) {
    sampleRate_ = rate;
    sampLen_ = 1.0 / rate;
    recalculateSettings();
}

void evString::recalculateSettings() {
    // pitch detection limits
    pitchDecLo_ = stringHz_;
    pitchDecHi_ = 1 / (stringHzHi_ / sampleRate_);
}

float evString::process(float in) {
    bool pitchDetectionFlag = false; // declare outside of if statements

    // true is (+) sample, false is (-)
    if (in > 0) { // 'in' is the current sample
        pitchDetectionFlag = true; // true = positive sample value
    } else if (in < 0) { // false = positive sample value
        pitchDetectionFlag = false;
    }

    // if we have a zero crossing and the wave is coming up from below the zero line and we've waited beyond the 'high' threshold time
    if (pitchDetectionFlag != prevPitchDetectionFlag_ && !prevPitchDetectionFlag_
        && pitchDetectionTimer_ >= pitchDecHi_) {

        float freq = 1 / (pitchDetectionTimer_ * sampLen_);

        // if the calculated frequency is above the string's tuned pitch, let's use it!
        if (freq > pitchDecLo_) {
           	frequencyPrev_ = frequency_;
           	frequencyNew_ = freq;
            frequency_ = (frequencyPrev_ + frequencyNew_) / 2;
            
            freqOutRange_ = true;
            freqOutDect_ = true;
        } else {
        	frequency_ = 0;
            freqOutRange_ = false;
            freqOutDect_ = false;
        }

        pitchDetectionTimer_ = 0; // reset timer regardless if the frequency was too low
    } else {
        freqOutDect_ = false;
    }

    // needs to be outside of an if/else. need to run every sample
    pitchDetectionTimer_++;
    prevPitchDetectionFlag_ = pitchDetectionFlag; // keep track of zero crossings

    return frequency_;
}

// Pure Data external definition
static t_class *zDet_class;

typedef struct _zDet {
    t_object x_obj;
    t_sample f;
    evString *string;
    t_float detection_on; // 0 for off, 1 for on
    t_float low_range;
    t_float high_range;
    t_outlet *msg_outlet; // outlet for frequency message
    t_outlet *range_outlet; // outlet for frequency in range
    t_outlet *dect_outlet; // outlet for detection status
} t_zDet;

// Constructor
void *zDet_new(t_floatarg hz, t_floatarg hzHi) {
    t_zDet *x = (t_zDet *)pd_new(zDet_class);

    // Create inlets for low and high range, and detection on/off
    floatinlet_new(&x->x_obj, &x->low_range);
    floatinlet_new(&x->x_obj, &x->high_range);
    floatinlet_new(&x->x_obj, &x->detection_on);

    // Create signal and message outlets
    outlet_new(&x->x_obj, &s_signal);
    x->msg_outlet = outlet_new(&x->x_obj, &s_float);
    x->range_outlet = outlet_new(&x->x_obj, &s_float);
    x->dect_outlet = outlet_new(&x->x_obj, &s_float);

    // Initialize evString with the provided frequencies or default to 0 Hz and 20000 Hz
    x->string = new evString(sys_getsr(), hz != 0 ? hz : 0, hzHi != 0 ? hzHi : 20000);
    x->detection_on = 1; // start with detection on
    x->low_range = hz != 0 ? hz : 0;
    x->high_range = hzHi != 0 ? hzHi : 20000;

    return (void *)x;
}

// Destructor
void zDet_free(t_zDet *x) {
    delete x->string;
}

// DSP Routine
t_int *zDet_perform(t_int *w) {
    t_zDet *x = (t_zDet *)(w[1]);
    t_sample *in = (t_sample *)(w[2]);
    t_sample *out = (t_sample *)(w[3]);
    int n = (int)(w[4]);

    float frequency = 0.0;
    bool freqOutRange = false;
    bool freqOutDect = false;

    // Update low and high range settings
    x->string->setLowRange(x->low_range);
    x->string->setHighRange(x->high_range);

    for (int i = 0; i < n; i++) {
        if (x->detection_on) {
            frequency = x->string->process(in[i]);
            freqOutRange = x->string->isFreqOutRange();
            freqOutDect = x->string->isFreqOutDect();
            out[i] = frequency;
        } else {
            out[i] = 0; // output 0 when detection is off
        }
    }

    if (x->detection_on) {
        // Send frequency as a message
        outlet_float(x->msg_outlet, frequency);
        outlet_float(x->range_outlet, freqOutRange);
        outlet_float(x->dect_outlet, freqOutDect);
    }

    return (w + 5);
}

// DSP Setup
void zDet_dsp(t_zDet *x, t_signal **sp) {
    x->string->setSampleRate(sp[0]->s_sr); // Update sample rate
    dsp_add(zDet_perform, 4, x, sp[0]->s_vec, sp[1]->s_vec, sp[0]->s_n);
}

// Setup Function
extern "C" void zDet_tilde_setup(void) {
    zDet_class = class_new(gensym("zDet~"),
                            (t_newmethod)zDet_new,
                            (t_method)zDet_free,
                            sizeof(t_zDet),
                            CLASS_DEFAULT,
                            A_DEFFLOAT, A_DEFFLOAT, 0);

    CLASS_MAINSIGNALIN(zDet_class, t_zDet, f);
    class_addmethod(zDet_class, (t_method)zDet_dsp, gensym("dsp"), A_CANT, 0);
}
