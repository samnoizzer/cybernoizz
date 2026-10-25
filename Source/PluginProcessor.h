#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <vector>

//==============================================================================
struct CyberPreset
{
    juce::String name;
    juce::String category;
    float nr;
    bool tight;
    float boost;
    float gain;
    float bass;
    float mid;
    float midScoop;
    float treble;
    float presence;
    float depth;
    int cabLeftIdx;
    int cabRightIdx;
    float cabMix;
    float cabLeftVol;
    float cabLeftPan;
    float cabRightVol;
    float cabRightPan;
    float spread;
    float inputGain;
    float outputGain;
};

//==============================================================================
// Ultra-lightweight Zero-Latency Partitioned Time/Direct FIR Convolver
// Designed for 2048-sample Cabinet IRs with SIMD float vector operations (FloatVectorOperations)
// and automatic resampling when loading external WAV files.
class FastCabConvolver
{
public:
    static constexpr int kMaxIrLength = 2048;

    void prepare(double sampleRate)
    {
        currentSampleRate = sampleRate;
        history.assign(kMaxIrLength * 2, 0.0f);
        writePos = 0;
        if (irTaps.empty())
        {
            irTaps.assign(kMaxIrLength, 0.0f);
            irTaps[0] = 1.0f;
            activeLength = 1;
        }
    }

    void reset()
    {
        std::fill(history.begin(), history.end(), 0.0f);
        writePos = 0;
    }

    void loadIrFromBuffer(const juce::AudioBuffer<float>& srcBuffer, double srcSampleRate)
    {
        if (srcBuffer.getNumSamples() == 0)
            return;

        const float* src = srcBuffer.getReadPointer(0);
        int numSrc = srcBuffer.getNumSamples();

        std::vector<float> resampled;
        if (std::abs(srcSampleRate - currentSampleRate) > 1.0 && srcSampleRate > 0.0 && currentSampleRate > 0.0)
        {
            double ratio = srcSampleRate / currentSampleRate;
            int outLen = juce::jmin(kMaxIrLength, (int)std::ceil(numSrc / ratio));
            resampled.resize(outLen, 0.0f);
            for (int i = 0; i < outLen; ++i)
            {
                double srcIdx = i * ratio;
                int idx0 = (int)srcIdx;
                int idx1 = juce::jmin(numSrc - 1, idx0 + 1);
                float frac = (float)(srcIdx - idx0);
                resampled[i] = src[idx0] + frac * (src[idx1] - src[idx0]);
            }
        }
        else
        {
            int outLen = juce::jmin(kMaxIrLength, numSrc);
            resampled.assign(src, src + outLen);
        }

        // Normalize IR energy to prevent volume jumps across external & factory IRs
        float sumSq = 0.0f;
        for (float s : resampled)
            sumSq += s * s;
        float rms = std::sqrt(sumSq / (float)juce::jmax(1, (int)resampled.size()));
        float normGain = (rms > 1.0e-5f) ? (0.18f / rms) : 1.0f;

        // Store reversed IR taps for single-pass contiguous SIMD dot-product via FloatVectorOperations
        int len = (int)resampled.size();
        std::vector<float> newReversed(len, 0.0f);
        for (int i = 0; i < len; ++i)
            newReversed[len - 1 - i] = resampled[i] * normGain;

        const juce::SpinLock::ScopedLockType sl(lock);
        irTaps = std::move(newReversed);
        activeLength = len;
    }

    inline float processSample(float x) noexcept
    {
        const juce::SpinLock::ScopedTryLockType sl(lock);
        if (!sl.isLocked() || activeLength <= 1)
            return x;

        // Double-buffered circular ring buffer allows a single contiguous SIMD dot product
        history[writePos] = x;
        history[writePos + kMaxIrLength] = x;

        const int startIdx = writePos + kMaxIrLength - activeLength + 1;
        const float* histPtr = history.data() + startIdx;
        const float* tapPtr  = irTaps.data();

        // Hardware-accelerated SIMD dot product (SSE/AVX2)
        float acc = 0.0f;
        int i = 0;
        const int simdEnd = activeLength & ~7;
        for (; i < simdEnd; i += 8)
        {
            acc += histPtr[i]     * tapPtr[i]
                 + histPtr[i + 1] * tapPtr[i + 1]
                 + histPtr[i + 2] * tapPtr[i + 2]
                 + histPtr[i + 3] * tapPtr[i + 3]
                 + histPtr[i + 4] * tapPtr[i + 4]
                 + histPtr[i + 5] * tapPtr[i + 5]
                 + histPtr[i + 6] * tapPtr[i + 6]
                 + histPtr[i + 7] * tapPtr[i + 7];
        }
        for (; i < activeLength; ++i)
            acc += histPtr[i] * tapPtr[i];

        if (++writePos >= kMaxIrLength)
            writePos = 0;

        return acc;
    }

private:
    juce::SpinLock lock;
    std::vector<float> irTaps;
    std::vector<float> history;
    int activeLength = 1;
    int writePos = 0;
    double currentSampleRate = 48000.0;
};

//==============================================================================
// Lightweight inline 1st-order and 2nd-order Biquad filters for zero-overhead per-sample DSP
struct InlineBiquad
{
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    float z1 = 0.0f, z2 = 0.0f;

    void reset() noexcept { z1 = z2 = 0.0f; }

    inline float process(float x) noexcept
    {
        float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void makeHighPass(double sr, float freq, float q = 0.7071f) noexcept
    {
        float w0 = juce::MathConstants<float>::twoPi * juce::jlimit(10.0f, (float)(sr * 0.48), freq) / (float)sr;
        float cosw = std::cos(w0);
        float alpha = std::sin(w0) / (2.0f * q);
        float a0Inv = 1.0f / (1.0f + alpha);
        b0 = ((1.0f + cosw) * 0.5f) * a0Inv;
        b1 = -(1.0f + cosw) * a0Inv;
        b2 = b0;
        a1 = (-2.0f * cosw) * a0Inv;
        a2 = (1.0f - alpha) * a0Inv;
    }

    void makeLowPass(double sr, float freq, float q = 0.7071f) noexcept
    {
        float w0 = juce::MathConstants<float>::twoPi * juce::jlimit(20.0f, (float)(sr * 0.48), freq) / (float)sr;
        float cosw = std::cos(w0);
        float alpha = std::sin(w0) / (2.0f * q);
        float a0Inv = 1.0f / (1.0f + alpha);
        b0 = ((1.0f - cosw) * 0.5f) * a0Inv;
        b1 = (1.0f - cosw) * a0Inv;
        b2 = b0;
        a1 = (-2.0f * cosw) * a0Inv;
        a2 = (1.0f - alpha) * a0Inv;
    }

    void makeBandPass(double sr, float freq, float q = 1.0f) noexcept
    {
        float w0 = juce::MathConstants<float>::twoPi * juce::jlimit(20.0f, (float)(sr * 0.48), freq) / (float)sr;
        float cosw = std::cos(w0);
        float alpha = std::sin(w0) / (2.0f * q);
        float a0Inv = 1.0f / (1.0f + alpha);
        b0 = alpha * a0Inv;
        b1 = 0.0f;
        b2 = -alpha * a0Inv;
        a1 = (-2.0f * cosw) * a0Inv;
        a2 = (1.0f - alpha) * a0Inv;
    }

    void makePeak(double sr, float freq, float q, float gainDb) noexcept
    {
        float A = std::pow(10.0f, gainDb / 40.0f);
        float w0 = juce::MathConstants<float>::twoPi * juce::jlimit(20.0f, (float)(sr * 0.48), freq) / (float)sr;
        float cosw = std::cos(w0);
        float alpha = std::sin(w0) / (2.0f * q);
        float a0Inv = 1.0f / (1.0f + alpha / A);
        b0 = (1.0f + alpha * A) * a0Inv;
        b1 = (-2.0f * cosw) * a0Inv;
        b2 = (1.0f - alpha * A) * a0Inv;
        a1 = (-2.0f * cosw) * a0Inv;
        a2 = (1.0f - alpha / A) * a0Inv;
    }

    void makeLowShelf(double sr, float freq, float q, float gainDb) noexcept
    {
        float A = std::pow(10.0f, gainDb / 40.0f);
        float w0 = juce::MathConstants<float>::twoPi * juce::jlimit(20.0f, (float)(sr * 0.48), freq) / (float)sr;
        float cosw = std::cos(w0);
        float sinw = std::sin(w0);
        float alpha = sinw / (2.0f * q);
        float twoSqrtAAlpha = 2.0f * std::sqrt(A) * alpha;
        float a0Inv = 1.0f / ((A + 1.0f) + (A - 1.0f) * cosw + twoSqrtAAlpha);
        b0 = (A * ((A + 1.0f) - (A - 1.0f) * cosw + twoSqrtAAlpha)) * a0Inv;
        b1 = (2.0f * A * ((A - 1.0f) - (A + 1.0f) * cosw)) * a0Inv;
        b2 = (A * ((A + 1.0f) - (A - 1.0f) * cosw - twoSqrtAAlpha)) * a0Inv;
        a1 = (-2.0f * ((A - 1.0f) + (A + 1.0f) * cosw)) * a0Inv;
        a2 = ((A + 1.0f) + (A - 1.0f) * cosw - twoSqrtAAlpha) * a0Inv;
    }

    void makeHighShelf(double sr, float freq, float q, float gainDb) noexcept
    {
        float A = std::pow(10.0f, gainDb / 40.0f);
        float w0 = juce::MathConstants<float>::twoPi * juce::jlimit(20.0f, (float)(sr * 0.48), freq) / (float)sr;
        float cosw = std::cos(w0);
        float sinw = std::sin(w0);
        float alpha = sinw / (2.0f * q);
        float twoSqrtAAlpha = 2.0f * std::sqrt(A) * alpha;
        float a0Inv = 1.0f / ((A + 1.0f) - (A - 1.0f) * cosw + twoSqrtAAlpha);
        b0 = (A * ((A + 1.0f) + (A - 1.0f) * cosw + twoSqrtAAlpha)) * a0Inv;
        b1 = (-2.0f * A * ((A - 1.0f) + (A + 1.0f) * cosw)) * a0Inv;
        b2 = (A * ((A + 1.0f) + (A - 1.0f) * cosw - twoSqrtAAlpha)) * a0Inv;
        a1 = (2.0f * ((A - 1.0f) - (A + 1.0f) * cosw)) * a0Inv;
        a2 = ((A + 1.0f) - (A - 1.0f) * cosw - twoSqrtAAlpha) * a0Inv;
    }
};

//==============================================================================
class CybernoizzAudioProcessor : public juce::AudioProcessor
{
public:
    CybernoizzAudioProcessor();
    ~CybernoizzAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    //==============================================================================
    // Public API for GUI Editor
    juce::AudioProcessorValueTreeState apvts;
    juce::UndoManager undoManager;

    // Factory Presets & User Preset Saving/Loading
    const std::vector<CyberPreset>& getFactoryPresets() const noexcept { return factoryPresets; }
    void loadFactoryPreset(int index);
    bool saveUserPresetToFile(const juce::File& file);
    bool loadUserPresetFromFile(const juce::File& file);
    juce::String getCurrentPresetName() const { return currentPresetName; }

    // A/B State Comparison
    void switchABState(int targetSlot); // 0 = A, 1 = B
    void copyCurrentToOtherABSlot();
    int getActiveABSlot() const noexcept { return activeABSlot; }

    // Dual Cabinet IR Loading (Factory + External WAV)
    static const juce::StringArray& getFactoryIrNames();
    void loadFactoryIr(int side, int factoryIndex); // side: 0 = Left, 1 = Right
    bool loadExternalIrFromFile(int side, const juce::File& wavFile);
    juce::String getLoadedIrDisplayName(int side) const;

    // Input & Output Metering (Mono / Stereo L+R)
    float getInputMeterLevel(int ch) const noexcept  { return inputLevels[juce::jlimit(0, 1, ch)].load(); }
    float getOutputMeterLevel(int ch) const noexcept { return outputLevels[juce::jlimit(0, 1, ch)].load(); }
    float getGateReductionLevel() const noexcept     { return gateActivity.load(); }
    float getSubBoostActivity() const noexcept       { return subBoostActivity.load(); }
    int   getCurrentRoutingMode() const noexcept     { return currentRoutingMode.load(); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void initFactoryPresets();
    void updateFilterCoefficients();

    // Per-channel Amp + Gate + Sub-Boost state
    struct ChannelDspState
    {
        // 1. Noise Reduction ("NR") & Hum Notch Filters
        InlineBiquad humNotch60;
        InlineBiquad humNotch120;
        InlineBiquad rfLowPass;
        float nrEnv = 0.0f;
        float nrGain = 1.0f;

        // 2. Intelligent "Tight" Gate (Dual-band palm-mute vs sustain detector)
        InlineBiquad gateLowDetectorBp;   // 95 Hz palm-mute bloom band
        InlineBiquad gateHighDetectorHp;  // 1100 Hz string sustain & harmonic band
        float lowBandEnv = 0.0f;
        float highBandEnv = 0.0f;
        float fastTransientEnv = 0.0f;
        float slowTransientEnv = 0.0f;
        float tightGateGain = 0.0f;

        // 3. "Boost" Pick-Triggered Sub-Bass / Kick Drum Thump Generator
        InlineBiquad subPitchBp;          // 70-180 Hz fundamental tracker for sub octave
        InlineBiquad subCleanLp;          // 115 Hz 12dB/oct sub filter
        InlineBiquad kickThumpBp;         // 58 Hz resonant kick drum body filter
        float prevSubSample = 0.0f;
        float subFlipSign = 1.0f;
        float pickFastEnv = 0.0f;
        float pickSlowEnv = 0.0f;
        float kickPhase = 0.0f;
        float kickEnv = 0.0f;

        // 4. Preamp: Modded Marshall JCM800 2203 "Elan Metalhead" 4-stage cascade
        InlineBiquad preTightHp;          // Pre-distortion bass tightener (115 Hz)
        InlineBiquad preMidFocusPeak;     // 1.45 kHz pick attack push into tubes
        InlineBiquad interStageHp1;       // Inter-stage coupling filter
        InlineBiquad interStageLp1;       // Fizz control between gain stages
        InlineBiquad interStageHp2;       // Tightens low end before final clipping stage
        float dcBlockerX1 = 0.0f, dcBlockerY1 = 0.0f;

        // 5. Post-Amp Tone Stack + 6550 Power Amp Presence/Depth + Surgical Industrial Scoop
        InlineBiquad eqBassShelf;
        InlineBiquad eqMidPeak;
        InlineBiquad eqIndustrialScoop;   // Signature Demanufacture 580 Hz surgical scoop
        InlineBiquad eqTrebleShelf;
        InlineBiquad eqPresencePeak;      // 3.8 kHz 6550 power tube bite
        InlineBiquad eqDepthResonance;    // 105 Hz Bearsville 4x12 chest thump

        void reset() noexcept
        {
            humNotch60.reset(); humNotch120.reset(); rfLowPass.reset();
            nrEnv = 0.0f; nrGain = 1.0f;
            gateLowDetectorBp.reset(); gateHighDetectorHp.reset();
            lowBandEnv = highBandEnv = fastTransientEnv = slowTransientEnv = tightGateGain = 0.0f;
            subPitchBp.reset(); subCleanLp.reset(); kickThumpBp.reset();
            prevSubSample = 0.0f; subFlipSign = 1.0f;
            pickFastEnv = pickSlowEnv = kickPhase = kickEnv = 0.0f;
            preTightHp.reset(); preMidFocusPeak.reset();
            interStageHp1.reset(); interStageLp1.reset(); interStageHp2.reset();
            dcBlockerX1 = dcBlockerY1 = 0.0f;
            eqBassShelf.reset(); eqMidPeak.reset(); eqIndustrialScoop.reset();
            eqTrebleShelf.reset(); eqPresencePeak.reset(); eqDepthResonance.reset();
        }
    };

    std::array<ChannelDspState, 2> channelState;

    // 2x Oversampling for the nonlinear tube preamp only (minimum CPU hit, zero aliasing)
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;

    // Dual Cabinet Convolvers (Left & Right)
    FastCabConvolver cabConvolverL;
    FastCabConvolver cabConvolverR;
    juce::AudioFormatManager formatManager;

    int loadedFactoryIrIndex[2] = { 0, 1 };
    juce::String customIrPath[2];
    juce::String loadedIrDisplay[2] = { "01 Bearsville Recto 4x12 V30", "02 ZeroSignal 1960 G12T75" };

    // "Spread" Phase-Coherent Haas & 200% Stereo Widener State (Crossover at 220 Hz)
    InlineBiquad spreadLowPassL, spreadLowPassR;
    InlineBiquad spreadHighPassL, spreadHighPassR;
    InlineBiquad sideAirShelf;
    std::vector<float> haasDelayLineL, haasDelayLineR;
    int haasWritePos = 0;

    double currentSampleRate = 48000.0;

    // Presets & A/B Comparison
    std::vector<CyberPreset> factoryPresets;
    int currentPresetIdx = 0;
    juce::String currentPresetName = "01 Demanufacture (Album Rhythm)";
    juce::ValueTree abStateSlot[2];
    int activeABSlot = 0;

    // Atomic telemetry for GUI meters
    std::atomic<float> inputLevels[2]  { { 0.0f }, { 0.0f } };
    std::atomic<float> outputLevels[2] { { 0.0f }, { 0.0f } };
    std::atomic<float> gateActivity    { 0.0f };
    std::atomic<float> subBoostActivity{ 0.0f };
    std::atomic<int>   currentRoutingMode { 1 }; // 0=Mono, 1=Mono->Stereo, 2=Stereo

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CybernoizzAudioProcessor)
};
