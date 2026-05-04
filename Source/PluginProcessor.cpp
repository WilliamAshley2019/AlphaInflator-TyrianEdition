//==============================================================================
// JS Inflator  v1.4 - PluginProcessor.cpp
//==============================================================================
#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout
JSInflatorProcessor::createParameterLayout()
{
    using Range = juce::NormalisableRange<float>;
    using Param = juce::AudioParameterFloat;
    using Choice = juce::AudioParameterChoice;
    using Bool = juce::AudioParameterBool;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto dBStr = [](float v, int) -> juce::String { return juce::String(v, 1) + " dB"; };
    auto pctStr = [](float v, int) -> juce::String { return juce::String(v, 1) + "%"; };

    layout.add(std::make_unique<Param>(juce::ParameterID(ParamID::INPUT, 1), "Input Gain",
        Range(-12.0f, 12.0f, 0.01f), 0.0f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction(dBStr)));

    layout.add(std::make_unique<Param>(juce::ParameterID(ParamID::EFFECT, 1), "Effect",
        Range(0.0f, 100.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction(pctStr)));

    layout.add(std::make_unique<Param>(juce::ParameterID(ParamID::CURVE, 1), "Curve",
        Range(0.0f, 100.0f, 0.1f), 50.0f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction(
            [](float v, int) -> juce::String { return juce::String(v, 1); })));

    layout.add(std::make_unique<Param>(juce::ParameterID(ParamID::OUTPUT, 1), "Output Gain",
        Range(-12.0f, 0.0f, 0.01f), 0.0f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction(dBStr)));

    layout.add(std::make_unique<Choice>(juce::ParameterID(ParamID::OS, 1), "Oversampling",
        juce::StringArray{ "1x", "2x", "4x", "8x" }, 0));
    layout.add(std::make_unique<Choice>(juce::ParameterID(ParamID::OS_QUAL, 1), "OS Phase",
        juce::StringArray{ "Min Phase", "Linear Phase" }, 0));
    layout.add(std::make_unique<Choice>(juce::ParameterID(ParamID::CLIP_MODE, 1), "Clip Mode",
        juce::StringArray{ "Off", "Hard", "Soft", "Hard+Soft" }, 0));
    layout.add(std::make_unique<Choice>(juce::ParameterID(ParamID::SPLIT_TYPE, 1), "Split Type",
        juce::StringArray{ "Simple", "Original SVF" }, 0));

    layout.add(std::make_unique<Bool>(juce::ParameterID(ParamID::SPLIT, 1), "Band Split", false));
    layout.add(std::make_unique<Bool>(juce::ParameterID(ParamID::MS_MODE, 1), "Mid/Side", false));
    layout.add(std::make_unique<Bool>(juce::ParameterID(ParamID::IN, 1), "In", true));
    layout.add(std::make_unique<Bool>(juce::ParameterID(ParamID::BYPASS, 1), "Bypass", false));

    layout.add(std::make_unique<Choice>(juce::ParameterID(ParamID::AGC_MODE, 1), "Auto Gain",
        juce::StringArray{ "Off", "Static", "Dynamic" }, 0));
    layout.add(std::make_unique<Bool>(juce::ParameterID(ParamID::DELTA, 1), "Delta Monitor", false));

    layout.add(std::make_unique<Param>(juce::ParameterID(ParamID::TONE, 1), "Tone",
        Range(-50.0f, 50.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction(
            [](float v, int) -> juce::String
            {
                if (v < -5.0f) return juce::String("Warm ") + juce::String(std::fabs(v), 0);
                if (v > 5.0f) return juce::String("Bright ") + juce::String(v, 0);
                return juce::String("Neutral");
            })));

    layout.add(std::make_unique<Choice>(juce::ParameterID(ParamID::FOCUS, 1), "Frequency Focus",
        juce::StringArray{ "Full", "Low", "Mid", "High" }, 0));
    layout.add(std::make_unique<Choice>(juce::ParameterID(ParamID::DYN_MODE, 1), "Dynamics Sens",
        juce::StringArray{ "Smooth", "Neutral", "Punch", "Dense" }, 1));

    layout.add(std::make_unique<Bool>(juce::ParameterID(ParamID::LIMITER, 1), "Safety Limiter", false));
    layout.add(std::make_unique<Param>(juce::ParameterID(ParamID::LIM_CEIL, 1), "Limiter Ceiling",
        Range(-6.0f, -0.1f, 0.05f), -0.3f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction(dBStr)));
    layout.add(std::make_unique<Bool>(juce::ParameterID(ParamID::DC_BLOCK, 1), "DC Blocker", true));

    return layout;
}

JSInflatorProcessor::JSInflatorProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    apvts(*this, nullptr, "JSInflator", createParameterLayout())
{
    apvts.addParameterListener(ParamID::OS, &osListener);
    apvts.addParameterListener(ParamID::OS_QUAL, &osListener);
    updateCurveCoefficients(0.5);
}

JSInflatorProcessor::~JSInflatorProcessor()
{
    apvts.removeParameterListener(ParamID::OS, &osListener);
    apvts.removeParameterListener(ParamID::OS_QUAL, &osListener);
}

bool JSInflatorProcessor::isBusesLayoutSupported(const BusesLayout& l) const
{
    const auto& in = l.getMainInputChannelSet();
    return in == l.getMainOutputChannelSet() &&
        (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo());
}

void JSInflatorProcessor::prepareToPlay(double sr, int blks)
{
    currentSampleRate = sr; currentBlockSize = blks;
    const int numCh = getTotalNumInputChannels();

    for (int qi = 0; qi < 2; ++qi)
    {
        const auto ft = (qi == 0) ? juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR
            : juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple;
        for (int st = 1; st <= 3; ++st)
        {
            const int idx = (st - 1) + qi * 3;
            oversamplers[idx] = std::make_unique<juce::dsp::Oversampling<float>>(numCh, st, ft, true, false);
            oversamplers[idx]->initProcessing(static_cast<size_t>(blks));
        }
    }

    const int osIdx = static_cast<int>(apvts.getRawParameterValue(ParamID::OS)->load() + 0.5f);
    const int osQual = static_cast<int>(apvts.getRawParameterValue(ParamID::OS_QUAL)->load() + 0.5f);
    currentOSIndex = osIdx; currentOSQual = osQual;
    requestedOSIndex.store(osIdx); requestedOSQual.store(osQual);

    const int osLat = [this, osIdx, osQual]() -> int {
        if (osIdx == 0) return 0;
        const int ai = getOversamplerArrayIndex(osIdx, osQual);
        return (ai >= 0 && ai < 6 && oversamplers[ai]) ? (int)oversamplers[ai]->getLatencyInSamples() : 0;
        }();

    lookaheadLim.prepare(sr);
    setLatencySamples(osLat + lookaheadLim.getLatency());

    dryDelayLen = std::min(osLat + lookaheadLim.getLatency(), DRY_DELAY_MAX - 1);
    dryDelayWrite = 0;
    std::fill(dryDelayL, dryDelayL + DRY_DELAY_MAX, 0.0f);
    std::fill(dryDelayR, dryDelayR + DRY_DELAY_MAX, 0.0f);

    for (int ch = 0; ch < 2; ++ch)
    {
        svfSplit[ch].setFrequencies(240, 2400, sr); svfSplit[ch].reset();
        simpleSplit[ch].setFrequencies(240, 2400, sr); simpleSplit[ch].reset();
        svfSplitOS[ch].reset(); simpleSplitOS[ch].reset();
    }

    for (auto& d : dcBlocker) d.reset();
    for (auto& t : tiltEQ) t.prepare(sr);
    for (auto& td : transientDet) td.prepare(sr);

    const double ramp = 0.020;
    inputGain.reset(sr, ramp); outputGain.reset(sr, ramp);
    effectWet.reset(sr, ramp); curveSmoother.reset(sr, ramp); toneSmoother.reset(sr, ramp);
    agcGainSmooth.reset(sr, 0.500);

    auto dB2g = [](float dB) { return juce::Decibels::decibelsToGain<double>(dB); };
    inputGain.setCurrentAndTargetValue(dB2g(apvts.getRawParameterValue(ParamID::INPUT)->load()));
    outputGain.setCurrentAndTargetValue(dB2g(apvts.getRawParameterValue(ParamID::OUTPUT)->load()));
    effectWet.setCurrentAndTargetValue(apvts.getRawParameterValue(ParamID::EFFECT)->load() / 100.0);
    curveSmoother.setCurrentAndTargetValue(apvts.getRawParameterValue(ParamID::CURVE)->load() / 100.0);
    toneSmoother.setCurrentAndTargetValue(apvts.getRawParameterValue(ParamID::TONE)->load() / 50.0);
    agcGainSmooth.setCurrentAndTargetValue(1.0);

    agcCoeffSlow = std::exp(-1.0 / (sr * 5.0));
    agcCoeffFast = std::exp(-1.0 / (sr * 0.5));
    agcRmsIn = agcRmsOut = 0.0;

    rmsCoeff = std::exp(-1.0 / (sr * 0.300));
    inRmsAcc.fill(0.0); outRmsAcc.fill(0.0);

    for (auto& f : inputFollower) { f.prepare(sr); f.reset(); }
    for (auto& f : outputFollower) { f.prepare(sr); f.reset(); }

    peakHoldSamples = static_cast<int>(sr * 2.0);
    peakHoldCounter = 0;
    peakHoldInL = peakHoldInR = peakHoldOutL = peakHoldOutR = 0.0f;

    dryBuffer.setSize(numCh, blks);

    inLevelL = inLevelR = outLevelL = outLevelR = 0.0f;
    inPeakL = inPeakR = outPeakL = outPeakR = 0.0f;
    inRmsL = inRmsR = outRmsL = outRmsR = 0.0f;
    inOversL = inOversR = outOversL = outOversR = 0;
    effectMeter = 0.0f; limiterGR = 0.0f;
}

void JSInflatorProcessor::releaseResources()
{
    for (auto& os : oversamplers) os.reset();
}

void JSInflatorProcessor::onOSParamChanged(int nI, int nQ)
{
    requestedOSIndex.store(nI); requestedOSQual.store(nQ);
    int osLat = 0;
    if (nI > 0)
    {
        const int ai = getOversamplerArrayIndex(nI, nQ);
        if (ai >= 0 && ai < 6 && oversamplers[ai])
            osLat = (int)oversamplers[ai]->getLatencyInSamples();
    }
    setLatencySamples(osLat + lookaheadLim.getLatency());
    updateHostDisplay(juce::AudioProcessorListener::ChangeDetails().withLatencyChanged(true));
}

void JSInflatorProcessor::pushDry(float l, float r) noexcept
{
    dryDelayL[dryDelayWrite] = l; dryDelayR[dryDelayWrite] = r;
    dryDelayWrite = (dryDelayWrite + 1) % DRY_DELAY_MAX;
}

void JSInflatorProcessor::peekDry(float& l, float& r) const noexcept
{
    const int rp = (dryDelayWrite - dryDelayLen + DRY_DELAY_MAX) % DRY_DELAY_MAX;
    l = dryDelayL[rp]; r = dryDelayR[rp];
}

double JSInflatorProcessor::processInflatorSample(double x) const noexcept
{
    const double sign = (x >= 0.0) ? 1.0 : -1.0;
    const double s1 = std::fabs(x), s2 = s1 * s1, s3 = s2 * s1, s4 = s2 * s2;
    double out;
    if (s1 >= 2.0) out = 0.0;
    else if (s1 > 1.0) out = 2.0 * s1 - s2;
    else out = curveA * s1 + curveB * s2 + curveC * s3 - curveD * (s2 - 2.0 * s3 + s4);
    return out * sign;
}

void JSInflatorProcessor::updateCurveCoefficients(double cp_norm) noexcept
{
    const double cp = cp_norm - 0.5;
    curveA = 1.5 + cp;
    curveB = -(cp + cp);
    curveC = cp - 0.5;
    curveD = 0.0625 - cp * 0.25 + cp * cp * 0.25;
}

void JSInflatorProcessor::processBandSplit(int ch, bool useOS, bool useSVF, double x,
    double& l, double& m, double& h, double& G, double& GR) noexcept
{
    if (useSVF)
    {
        auto& bs = useOS ? svfSplitOS[ch] : svfSplit[ch];
        bs.process(x, l, m, h); G = bs.G; GR = bs.GR;
    }
    else
    {
        auto& bs = useOS ? simpleSplitOS[ch] : simpleSplit[ch];
        bs.process(x, l, m, h); G = bs.G; GR = bs.GR;
    }
}

void JSInflatorProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    auto raw = [this](juce::StringRef id) { return apvts.getRawParameterValue(id)->load(); };

    const bool bypass = raw(ParamID::BYPASS) > 0.5f, isIn = raw(ParamID::IN) > 0.5f;
    const bool doSplit = raw(ParamID::SPLIT) > 0.5f, doMS = raw(ParamID::MS_MODE) > 0.5f;
    const bool doDelta = raw(ParamID::DELTA) > 0.5f, doLim = raw(ParamID::LIMITER) > 0.5f;
    const bool doDC = raw(ParamID::DC_BLOCK) > 0.5f;
    const int clipMode = static_cast<int>(raw(ParamID::CLIP_MODE) + 0.5f);
    const int splitType = static_cast<int>(raw(ParamID::SPLIT_TYPE) + 0.5f);
    const int agcMode = static_cast<int>(raw(ParamID::AGC_MODE) + 0.5f);
    const int focus = static_cast<int>(raw(ParamID::FOCUS) + 0.5f);
    const int dynMode = static_cast<int>(raw(ParamID::DYN_MODE) + 0.5f);
    const double limCeil = juce::Decibels::decibelsToGain(raw(ParamID::LIM_CEIL));
    const bool useSVF = (splitType == 1);

    const int osIdx = requestedOSIndex.load(), osQual = requestedOSQual.load();
    currentOSIndex = osIdx; currentOSQual = osQual;
    const int numCh = buffer.getNumChannels(), numS = buffer.getNumSamples();

    auto dB2g = [](float dB) { return juce::Decibels::decibelsToGain<double>(dB); };
    inputGain.setTargetValue(dB2g(raw(ParamID::INPUT)));
    outputGain.setTargetValue(dB2g(raw(ParamID::OUTPUT)));
    effectWet.setTargetValue(raw(ParamID::EFFECT) / 100.0);
    curveSmoother.setTargetValue(raw(ParamID::CURVE) / 100.0);
    toneSmoother.setTargetValue(raw(ParamID::TONE) / 50.0);

    double dynTransBoost = 0.0, dynSustReduce = 0.0;
    if (dynMode == 0) dynTransBoost = -0.30;
    else if (dynMode == 2) dynTransBoost = 0.40;
    else if (dynMode == 3) dynSustReduce = 0.35;

    for (int ch = 0; ch < std::min(numCh, 2); ++ch)
    {
        const float* s = buffer.getReadPointer(ch);
        for (int i = 0; i < numS; ++i)
        {
            inputFollower[ch].update(static_cast<double>(s[i]));
            inRmsAcc[ch] = rmsCoeff * inRmsAcc[ch] + (1.0 - rmsCoeff) * (double)s[i] * s[i];
            if (s[i] >= 1.0f || s[i] <= -1.0f)
            {
                if (ch == 0) inOversL.fetch_add(1, std::memory_order_relaxed);
                else inOversR.fetch_add(1, std::memory_order_relaxed);
            }
        }
    }
    peakHoldInL = std::max(peakHoldInL, inputFollower[0].getValue());
    peakHoldInR = std::max(peakHoldInR, (numCh > 1) ? inputFollower[1].getValue() : peakHoldInL);

    if (numCh >= 2)
    {
        const float* sL = buffer.getReadPointer(0), * sR = buffer.getReadPointer(1);
        for (int i = 0; i < numS; ++i) pushDry(sL[i], sR[i]);
    }
    else if (numCh == 1)
    {
        const float* sL = buffer.getReadPointer(0);
        for (int i = 0; i < numS; ++i) pushDry(sL[i], sL[i]);
    }

    if (bypass)
    {
        for (int ch = 0; ch < std::min(numCh, 2); ++ch)
        {
            const float* s = buffer.getReadPointer(ch);
            for (int i = 0; i < numS; ++i) outputFollower[ch].update(static_cast<double>(s[i]));
        }
        outLevelL = outputFollower[0].getValue();
        outLevelR = (numCh > 1) ? outputFollower[1].getValue() : outLevelL.load();
        inLevelL = inputFollower[0].getValue();
        inLevelR = (numCh > 1) ? inputFollower[1].getValue() : inLevelL.load();
        effectMeter = 0.0f;
        return;
    }

    if (doMS && numCh == 2)
    {
        float* L = buffer.getWritePointer(0), * R = buffer.getWritePointer(1);
        for (int i = 0; i < numS; ++i) encodeMS(L[i], R[i]);
    }

    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::AudioBlock<float> osBlock;
    juce::dsp::Oversampling<float>* activeOS = nullptr;

    if (osIdx > 0)
    {
        const int ai = getOversamplerArrayIndex(osIdx, osQual);
        if (ai >= 0 && ai < 6 && oversamplers[ai])
        {
            activeOS = oversamplers[ai].get();
            osBlock = activeOS->processSamplesUp(block);
        }
    }
    if (!activeOS) osBlock = block;

    const int osN = static_cast<int>(osBlock.getNumSamples());
    const double osFS = currentSampleRate * (osIdx == 0 ? 1 : (1 << osIdx));

    for (int ch = 0; ch < std::min(numCh, 2); ++ch)
    {
        svfSplitOS[ch].setFrequencies(240, 2400, osFS); svfSplitOS[ch].reset();
        simpleSplitOS[ch].setFrequencies(240, 2400, osFS); simpleSplitOS[ch].reset();
    }
    for (auto& t : tiltEQ) t.prepare(osFS);

    updateCurveCoefficients(curveSmoother.getCurrentValue());
    const double toneNorm = toneSmoother.getCurrentValue();
    const double tiltLow = juce::Decibels::decibelsToGain(-toneNorm * 6.0);
    const double tiltHigh = juce::Decibels::decibelsToGain(toneNorm * 6.0);

    double effectAccum = 0.0;

    for (int ch = 0; ch < std::min(numCh, 2); ++ch)
    {
        float* samples = osBlock.getChannelPointer(ch);
        for (int i = 0; i < osN; ++i)
        {
            const double inG = inputGain.getNextValue(), outG = outputGain.getNextValue();
            const double wet = effectWet.getNextValue();
            double x = static_cast<double>(samples[i]) * inG;

            if (clipMode == 1 || clipMode == 3) x = hardClip(x);
            x = juce::jlimit(-2.0, 2.0, x);
            const double dry = x;

            const double transAmt = transientDet[ch].process(x);
            double dynWet = juce::jlimit(0.0, 1.0, wet + wet * dynTransBoost * transAmt - wet * dynSustReduce * (1.0 - transAmt));

            double processed = x;
            if (isIn)
            {
                if (doSplit)
                {
                    double l, m, h, G, GR;
                    processBandSplit(ch, true, useSVF, x, l, m, h, G, GR);
                    const double midNorm = m * G;
                    if (focus == 0) { l = processInflatorSample(l); m = processInflatorSample(midNorm) * GR; h = processInflatorSample(h); }
                    else if (focus == 1) { l = processInflatorSample(l); m = midNorm * GR; }
                    else if (focus == 2) { m = processInflatorSample(midNorm) * GR; }
                    else { h = processInflatorSample(h); m = midNorm * GR; }
                    processed = l + m + h;
                }
                else { processed = processInflatorSample(x); }
            }

            switch (clipMode)
            {
            case 1: processed = hardClip(processed); break;
            case 2: processed = softClip(processed); break;
            case 3: processed = hardClip(softClip(processed)); break;
            default: break;
            }

            processed = dry * (1.0 - dynWet) + processed * dynWet;
            effectAccum += std::fabs(processed) - std::fabs(dry);
            if (doDC) processed = dcBlocker[ch].process(processed);
            processed = tiltEQ[ch].process(processed, tiltLow, tiltHigh);
            processed *= outG;
            samples[i] = static_cast<float>(processed);
        }
    }

    if (activeOS) activeOS->processSamplesDown(block);

    if (doDelta && numCh >= 2)
    {
        float* outL = buffer.getWritePointer(0), * outR = buffer.getWritePointer(1);
        const int bs = (dryDelayWrite - numS + DRY_DELAY_MAX) % DRY_DELAY_MAX;
        for (int i = 0; i < numS; ++i)
        {
            const int rp = (bs + i) % DRY_DELAY_MAX;
            outL[i] -= dryDelayL[rp]; outR[i] -= dryDelayR[rp];
        }
    }

    if (doLim && numCh >= 2)
    {
        float* L = buffer.getWritePointer(0), * R = buffer.getWritePointer(1);
        for (int i = 0; i < numS; ++i)
        {
            double dL = L[i], dR = R[i];
            lookaheadLim.processFrame(dL, dR, limCeil);
            L[i] = static_cast<float>(dL); R[i] = static_cast<float>(dR);
        }
        limiterGR = static_cast<float>(lookaheadLim.getGR());
    }
    else { limiterGR = 0.0f; }

    if (doMS && numCh == 2)
    {
        float* L = buffer.getWritePointer(0), * R = buffer.getWritePointer(1);
        for (int i = 0; i < numS; ++i) decodeMS(L[i], R[i]);
    }

    if (agcMode > 0)
    {
        const double coeff = (agcMode == 2) ? agcCoeffFast : agcCoeffSlow;
        const double inPow = (inRmsAcc[0] + (numCh > 1 ? inRmsAcc[1] : inRmsAcc[0])) * 0.5;
        double outPow = 0.0;
        for (int ch = 0; ch < std::min(numCh, 2); ++ch)
        {
            const float* src = buffer.getReadPointer(ch);
            for (int i = 0; i < numS; ++i) outPow += (double)src[i] * src[i];
        }
        outPow /= static_cast<double>(numS * std::min(numCh, 2));
        agcRmsIn = coeff * agcRmsIn + (1.0 - coeff) * inPow;
        agcRmsOut = coeff * agcRmsOut + (1.0 - coeff) * outPow;
        if (agcRmsOut > 1e-12 && agcRmsIn > 1e-12)
            agcGainSmooth.setTargetValue(juce::jlimit(0.1, 4.0, std::sqrt(agcRmsIn / agcRmsOut)));
        buffer.applyGain(static_cast<float>(agcGainSmooth.getNextValue()));
    }

    for (int ch = 0; ch < std::min(numCh, 2); ++ch)
    {
        const float* s = buffer.getReadPointer(ch);
        for (int i = 0; i < numS; ++i)
        {
            double v = s[i];
            outputFollower[ch].update(v);
            outRmsAcc[ch] = rmsCoeff * outRmsAcc[ch] + (1.0 - rmsCoeff) * v * v;
            if (s[i] >= 1.0f || s[i] <= -1.0f)
            {
                if (ch == 0) outOversL.fetch_add(1, std::memory_order_relaxed);
                else outOversR.fetch_add(1, std::memory_order_relaxed);
            }
        }
    }

    peakHoldOutL = std::max(peakHoldOutL, outputFollower[0].getValue());
    peakHoldOutR = std::max(peakHoldOutR, (numCh > 1) ? outputFollower[1].getValue() : peakHoldOutL);
    peakHoldCounter += numS;
    if (peakHoldCounter >= peakHoldSamples)
    {
        peakHoldCounter = 0;
        peakHoldInL *= 0.95f; peakHoldInR *= 0.95f;
        peakHoldOutL *= 0.95f; peakHoldOutR *= 0.95f;
    }

    inLevelL = inputFollower[0].getValue();
    inLevelR = (numCh > 1) ? inputFollower[1].getValue() : inLevelL.load();
    outLevelL = outputFollower[0].getValue();
    outLevelR = (numCh > 1) ? outputFollower[1].getValue() : outLevelL.load();
    inPeakL = peakHoldInL; inPeakR = peakHoldInR;
    outPeakL = peakHoldOutL; outPeakR = peakHoldOutR;
    inRmsL = static_cast<float>(std::sqrt(inRmsAcc[0]));
    inRmsR = static_cast<float>(std::sqrt((numCh > 1) ? inRmsAcc[1] : inRmsAcc[0]));
    outRmsL = static_cast<float>(std::sqrt(outRmsAcc[0]));
    outRmsR = static_cast<float>(std::sqrt((numCh > 1) ? outRmsAcc[1] : outRmsAcc[0]));

    const double avgEff = effectAccum / std::max(1, osN * std::min(numCh, 2));
    effectMeter = static_cast<float>(juce::jlimit(0.0, 1.0, 0.5 + juce::Decibels::gainToDecibels(std::fabs(avgEff) + 1e-12) / 40.0));
}

void JSInflatorProcessor::processBlock(juce::AudioBuffer<double>& buffer, juce::MidiBuffer& midi)
{
    juce::AudioBuffer<float> fb(buffer.getNumChannels(), buffer.getNumSamples());
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        const double* s = buffer.getReadPointer(ch);
        float* d = fb.getWritePointer(ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i) d[i] = static_cast<float>(s[i]);
    }
    processBlock(fb, midi);
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        const float* s = fb.getReadPointer(ch);
        double* d = buffer.getWritePointer(ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i) d[i] = static_cast<double>(s[i]);
    }
}

juce::AudioProcessorEditor* JSInflatorProcessor::createEditor() { return new JSInflatorEditor(*this); }

void JSInflatorProcessor::getStateInformation(juce::MemoryBlock& d)
{
    auto xml = apvts.copyState().createXml();
    copyXmlToBinary(*xml, d);
}

void JSInflatorProcessor::setStateInformation(const void* d, int s)
{
    auto xml = getXmlFromBinary(d, s);
    if (xml && xml->hasTagName(apvts.state.getType()))
    {
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
        const int idx = static_cast<int>(apvts.getRawParameterValue(ParamID::OS)->load() + 0.5f);
        const int qual = static_cast<int>(apvts.getRawParameterValue(ParamID::OS_QUAL)->load() + 0.5f);
        onOSParamChanged(idx, qual);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new JSInflatorProcessor(); }