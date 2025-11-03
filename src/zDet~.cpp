//(c) 2024 Brian Lindgren

#include "m_pd.h"
#include <math.h>
#include <string>

// ---- strict (±1 sample) helpers for sample-interval comparisons ----
static inline int iabs_int(int v) { return (v < 0) ? -v : v; }

static inline bool approxEqualSamples(int a, int b) {
    // treat as equal only if they differ by at most 1 sample
    return iabs_int(a - b) <= 1;
}
static inline bool clearlyDifferentSamples(int a, int b) {
    // different if they differ by more than 1 sample
    return iabs_int(a - b) > 1;
}

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

    // keep short history of interval lengths (in samples)
    int N_z0_ = 0, N_z1_ = 0, N_z2_ = 0;

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

    // positive-going zero crossing, after the 'high' threshold wait
    if (pitchDetectionFlag != prevPitchDetectionFlag_ && !prevPitchDetectionFlag_
        && pitchDetectionTimer_ >= pitchDecHi_) {

        // interval in samples since last positive-going crossing
        int N = pitchDetectionTimer_;
        float freqCandidate = (N > 0) ? (float)sampleRate_ / (float)N : 0.0f;

        // update interval history: Z (latest), Z-1, Z-2
        N_z2_ = N_z1_;
        N_z1_ = N_z0_;
        N_z0_ = N;

        // alternating test: Z ≈ Z-2 AND Z != Z-1 (with strict ±1-sample tolerance)
        bool z_eq_z2 = approxEqualSamples(N_z0_, N_z2_);
        bool z_ne_z1 = clearlyDifferentSamples(N_z0_, N_z1_);
        bool alternating = z_eq_z2 && z_ne_z1;

        float freqOut = 0.f;

        if (alternating) {
            // one fundamental period is the sum of two adjacent sub-intervals
            int N_sum = N_z0_ + N_z1_;
            if (N_sum > 0) {
                freqOut = (float)sampleRate_ / (float)N_sum; // = 1 / ((Z + Z-1) * sampLen)
            } else {
                freqOut = freqCandidate;
            }
        } else {
            // clean single-interval period — your original light smoothing
            frequencyPrev_ = frequency_;
            frequencyNew_  = freqCandidate;
            freqOut        = 0.5f * (frequencyPrev_ + frequencyNew_);
        }

        // range gate
        if (freqOut > pitchDecLo_) {
            frequency_    = freqOut;
            freqOutRange_ = true;
            freqOutDect_  = true;
        } else {
            frequency_    = 0.f;
            freqOutRange_ = false;
            freqOutDect_  = false;
        }

        pitchDetectionTimer_ = 0; // reset timer regardless
    } else {
        // between detections
        freqOutDect_ = false;
    }

    // run every sample
    pitchDetectionTimer_++;
    prevPitchDetectionFlag_ = pitchDetectionFlag;

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
    t_outlet *msg_outlet;   // outlet for frequency message
    t_outlet *range_outlet; // outlet for frequency in range
    t_outlet *dect_outlet;  // outlet for detection status
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
    x->msg_outlet   = outlet_new(&x->x_obj, &s_float);
    x->range_outlet = outlet_new(&x->x_obj, &s_float);
    x->dect_outlet  = outlet_new(&x->x_obj, &s_float);

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
            freqOutDect  = x->string->isFreqOutDect();
            out[i] = frequency;
        } else {
            out[i] = 0; // output 0 when detection is off
        }
    }

    if (x->detection_on) {
        // Send frequency and flags as messages (one per DSP block)
        outlet_float(x->msg_outlet,   frequency);
        outlet_float(x->range_outlet, freqOutRange);
        outlet_float(x->dect_outlet,  freqOutDect);
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