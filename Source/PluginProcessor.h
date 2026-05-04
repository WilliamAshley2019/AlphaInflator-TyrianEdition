//==============================================================================
// JS Inflator  v1.4 - PluginProcessor.h  (JUCE 8.0.12)
//==============================================================================
#pragma once
#include <JuceHeader.h>

//==============================================================================
// Parameter IDs as strings (JUCE 8 compatible)
//==============================================================================
namespace ParamID
{
    inline const juce::String INPUT{ "input" };
    inline const juce::String EFFECT{ "effect" };
    inline const juce::String CURVE{ "curve" };
    inline const juce::String OUTPUT{ "output" };
    inline const juce::String SPLIT{ "split" };
    inline const juce::String MS_MODE{ "msmode" };
    inline const juce::String IN{ "in" };
    inline const juce::String BYPASS{ "bypass" };
    inline const juce::String CLIP_MODE{ "clipMode" };
    inline const juce::String OS{ "os" };
    inline const juce::String OS_QUAL{ "osQual" };
    inline const juce::String SPLIT_TYPE{ "splitType" };
    inline const juce::String AGC_MODE{ "agcMode" };
    inline const juce::String DELTA{ "delta" };
    inline const juce::String TONE{ "tone" };
    inline const juce::String FOCUS{ "focus" };
    inline const juce::String DYN_MODE{ "dynMode" };
    inline const juce::String LIMITER{ "limiter" };
    inline const juce::String LIM_CEIL{ "limCeil" };
    inline const juce::String DC_BLOCK{ "dcBlock" };
}

//==============================================================================
// DSP Structs (header-only)
//==============================================================================
struct OriginalSVFBandSplit
{
    double LP_C = 0, LP_R = 0, LP_I = 0, HP_C = 0, HP_R = 0, HP_I = 0, G = 1.0, GR = 1.0;

    void setFrequencies(double fcL, double fcH, double fs) noexcept
    {
        constexpr double pi = juce::MathConstants<double>::pi;
        LP_C = 0.5 * std::tan(pi * (fcL / fs - 0.25)) + 0.5;
        HP_C = 0.5 * std::tan(pi * (fcH / fs - 0.25)) + 0.5;
        G = HP_C * (1.0 - LP_C) / std::max(1e-12, HP_C - LP_C);
        GR = (G > 1e-12) ? 1.0 / G : 1.0;
    }

    void process(double x, double& l, double& m, double& h) noexcept
    {
        LP_R = LP_I + LP_C * (x - LP_I); LP_I = 2.0 * LP_R - LP_I;
        HP_R = (1.0 - HP_C) * HP_I + HP_C * x; HP_I = 2.0 * HP_R - HP_I;
        l = LP_R; h = x - HP_R; m = HP_R - LP_R;
    }
    void reset() noexcept { LP_R = LP_I = HP_R = HP_I = 0.0; }
};

struct SimpleBandSplit
{
    double lpState = 0, hpState = 0, coLow = 0, coHigh = 0, G = 1.0, GR = 1.0;

    void setFrequencies(double fcL, double fcH, double fs) noexcept
    {
        constexpr double tp = 2.0 * juce::MathConstants<double>::pi;
        coLow = 1.0 - std::exp(-tp * fcL / fs);
        coHigh = 1.0 - std::exp(-tp * fcH / fs);
        G = coHigh * (1.0 - coLow) / std::max(1e-12, coHigh - coLow);
        GR = (G > 1e-12) ? 1.0 / G : 1.0;
    }

    void process(double x, double& l, double& m, double& h) noexcept
    {
        lpState += coLow * (x - lpState);
        hpState += coHigh * (x - hpState);
        l = lpState; h = x - hpState; m = hpState - lpState;
    }
    void reset() noexcept { lpState = hpState = 0.0; }
};

struct TiltEQ
{
    double lpState = 0, coeff = 0;
    void prepare(double sr) noexcept
    {
        coeff = 1.0 - std::exp(-2.0 * juce::MathConstants<double>::pi * 800.0 / sr);
        reset();
    }
    double process(double x, double gL, double gH) noexcept
    {
        lpState += coeff * (x - lpState);
        return lpState * gL + (x - lpState) * gH;
    }
    void reset() noexcept { lpState = 0.0; }
};

struct DCBlocker
{
    double x1 = 0, y1 = 0;
    double process(double x) noexcept
    {
        y1 = x - x1 + 0.99975 * y1;
        x1 = x;
        return y1;
    }
    void reset() noexcept { x1 = y1 = 0.0; }
};

class LookaheadLimiter
{
public:
    static constexpr int MAX_BUF = 128;
    double release_s = 0.050;

    void prepare(double fs) noexcept
    {
        lookaheadN = std::min(MAX_BUF, std::max(1, (int)(fs * 0.001)));
        updateCoeff(fs);
        reset();
    }
    void updateCoeff(double fs) noexcept
    {
        releaseCoeff = std::exp(-1.0 / (fs * release_s));
    }
    void reset() noexcept
    {
        std::fill(bufL, bufL + MAX_BUF, 0.0);
        std::fill(bufR, bufR + MAX_BUF, 0.0);
        gainRed = 1.0; writePos = 0;
    }
    void processFrame(double& L, double& R, double ceiling) noexcept
    {
        bufL[writePos] = L; bufR[writePos] = R;
        const double pk = std::max(std::fabs(L), std::fabs(R));
        if (pk > ceiling && pk > 1e-12)
            gainRed = std::min(gainRed, ceiling / pk);
        gainRed += (1.0 - gainRed) * (1.0 - releaseCoeff);
        gainRed = std::min(gainRed, 1.0);
        const int rp = (writePos - lookaheadN + MAX_BUF) % MAX_BUF;
        L = bufL[rp] * gainRed; R = bufR[rp] * gainRed;
        writePos = (writePos + 1) % MAX_BUF;
    }
    int getLatency() const noexcept { return lookaheadN; }
    double getGR() const noexcept { return juce::Decibels::gainToDecibels(gainRed); }

private:
    double bufL[MAX_BUF] = {}, bufR[MAX_BUF] = {}, gainRed = 1.0, releaseCoeff = 0.999;
    int writePos = 0, lookaheadN = 48;
};

struct TransientDetector
{
    double fast = 0, slow = 0, fc = 0, sc = 0;
    void prepare(double fs) noexcept
    {
        fc = std::exp(-1.0 / (fs * 0.005));
        sc = std::exp(-1.0 / (fs * 0.100));
        reset();
    }
    double process(double x) noexcept
    {
        const double lv = std::fabs(x);
        fast = fc * fast + (1.0 - fc) * lv;
        slow = sc * slow + (1.0 - sc) * lv;
        const double d = fast - slow;
        return (slow > 1e-10) ? juce::jlimit(0.0, 1.0, d / slow) : 0.0;
    }
    void reset() noexcept { fast = slow = 0.0; }
};

struct LevelFollower
{
    double state = 0, atk = 0, rel = 0;
    void prepare(double fs) noexcept
    {
        atk = std::exp(-1.0 / (fs * 0.001));
        rel = std::exp(-1.0 / (fs * 0.300));
        reset();
    }
    void update(double x) noexcept
    {
        const double lv = std::fabs(x);
        state = ((lv > state) ? atk : rel) * state + (1.0 - ((lv > state) ? atk : rel)) * lv;
    }
    float getValue() const noexcept { return static_cast<float>(state); }
    void reset() noexcept { state = 0.0; }
};

//==============================================================================
class JSInflatorProcessor final : public juce::AudioProcessor
{
public:
    JSInflatorProcessor();
    ~JSInflatorProcessor() override;

    void prepareToPlay(double sr, int blks) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock(juce::AudioBuffer<double>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "JS Inflator"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    // Meter getters
    float getInLevelL() const noexcept { return inLevelL.load(); }
    float getInLevelR() const noexcept { return inLevelR.load(); }
    float getOutLevelL() const noexcept { return outLevelL.load(); }
    float getOutLevelR() const noexcept { return outLevelR.load(); }
    float getInPeakL() const noexcept { return inPeakL.load(); }
    float getInPeakR() const noexcept { return inPeakR.load(); }
    float getOutPeakL() const noexcept { return outPeakL.load(); }
    float getOutPeakR() const noexcept { return outPeakR.load(); }
    float getInRmsL() const noexcept { return inRmsL.load(); }
    float getInRmsR() const noexcept { return inRmsR.load(); }
    float getOutRmsL() const noexcept { return outRmsL.load(); }
    float getOutRmsR() const noexcept { return outRmsR.load(); }
    int getAndClearInOversL() noexcept { return inOversL.exchange(0); }
    int getAndClearInOversR() noexcept { return inOversR.exchange(0); }
    int getAndClearOutOversL() noexcept { return outOversL.exchange(0); }
    int getAndClearOutOversR() noexcept { return outOversR.exchange(0); }
    float getEffectMeter() const noexcept { return effectMeter.load(); }
    float getLimiterGR() const noexcept { return limiterGR.load(); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    double processInflatorSample(double x) const noexcept;
    void updateCurveCoefficients(double c) noexcept;
    double curveA = 1.5, curveB = 0.0, curveC = -0.5, curveD = 0.0625;

    static double softClip(double x) noexcept
    {
        if (x > 4.0) return 1.0; if (x < -4.0) return -1.0;
        const double x2 = x * x;
        return x * (27.0 + x2) / (27.0 + 9.0 * x2);
    }
    static double hardClip(double x) noexcept { return juce::jlimit(-1.0, 1.0, x); }

    static void encodeMS(float& L, float& R) noexcept { const float m = (L + R) * 0.5f, s = (L - R) * 0.5f; L = m; R = s; }
    static void decodeMS(float& M, float& S) noexcept { const float l = M + S, r = M - S; M = l; S = r; }
    static void encodeMS(double& L, double& R) noexcept { const double m = (L + R) * 0.5, s = (L - R) * 0.5; L = m; R = s; }
    static void decodeMS(double& M, double& S) noexcept { const double l = M + S, r = M - S; M = l; S = r; }

    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, 6> oversamplers;
    std::atomic<int> requestedOSIndex{ 0 }, requestedOSQual{ 0 };
    int currentOSIndex = -1, currentOSQual = -1;
    void onOSParamChanged(int i, int q);
    int getOversamplerArrayIndex(int i, int q) const noexcept { return (i - 1) + q * 3; }

    std::array<OriginalSVFBandSplit, 2> svfSplit, svfSplitOS;
    std::array<SimpleBandSplit, 2> simpleSplit, simpleSplitOS;
    void processBandSplit(int ch, bool useOS, bool useSVF, double x,
        double& l, double& m, double& h, double& G, double& GR) noexcept;

    std::array<DCBlocker, 2> dcBlocker;
    std::array<TiltEQ, 2> tiltEQ;
    std::array<TransientDetector, 2> transientDet;
    LookaheadLimiter lookaheadLim;

    static constexpr int DRY_DELAY_MAX = 16384;
    float dryDelayL[DRY_DELAY_MAX] = {}, dryDelayR[DRY_DELAY_MAX] = {};
    int dryDelayWrite = 0, dryDelayLen = 0;
    void pushDry(float l, float r) noexcept;
    void peekDry(float& l, float& r) const noexcept;

    double agcRmsIn = 0, agcRmsOut = 0, agcCoeffSlow = 0, agcCoeffFast = 0;
    juce::SmoothedValue<double, juce::ValueSmoothingTypes::Multiplicative> agcGainSmooth;
    juce::SmoothedValue<double, juce::ValueSmoothingTypes::Multiplicative> inputGain, outputGain;
    juce::SmoothedValue<double, juce::ValueSmoothingTypes::Linear> effectWet, curveSmoother, toneSmoother;
    std::array<LevelFollower, 2> inputFollower, outputFollower;
    std::array<double, 2> inRmsAcc = { 0, 0 }, outRmsAcc = { 0, 0 };
    double rmsCoeff = 0.0;
    float peakHoldInL = 0, peakHoldInR = 0, peakHoldOutL = 0, peakHoldOutR = 0;
    int peakHoldCounter = 0, peakHoldSamples = 0;
    juce::AudioBuffer<float> dryBuffer;

    std::atomic<float> inLevelL{ 0 }, inLevelR{ 0 }, outLevelL{ 0 }, outLevelR{ 0 };
    std::atomic<float> inPeakL{ 0 }, inPeakR{ 0 }, outPeakL{ 0 }, outPeakR{ 0 };
    std::atomic<float> inRmsL{ 0 }, inRmsR{ 0 }, outRmsL{ 0 }, outRmsR{ 0 };
    std::atomic<int> inOversL{ 0 }, inOversR{ 0 }, outOversL{ 0 }, outOversR{ 0 };
    std::atomic<float> effectMeter{ 0 }, limiterGR{ 0 };
    double currentSampleRate = 44100.0, currentBlockSize = 512;

    struct OSListener : public juce::AudioProcessorValueTreeState::Listener
    {
        explicit OSListener(JSInflatorProcessor& p) : proc(p) {}
        void parameterChanged(const juce::String&, float) override
        {
            const int idx = static_cast<int>(proc.apvts.getRawParameterValue(ParamID::OS)->load() + 0.5f);
            const int qual = static_cast<int>(proc.apvts.getRawParameterValue(ParamID::OS_QUAL)->load() + 0.5f);
            proc.onOSParamChanged(idx, qual);
        }
        JSInflatorProcessor& proc;
    } osListener{ *this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JSInflatorProcessor)
};