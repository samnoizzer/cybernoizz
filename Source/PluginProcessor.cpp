#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <BinaryData.h>

//==============================================================================
// Fast rational Padé [3/3] tanh approximation for low-CPU tube waveshaping
static inline float fastTanh(float x) noexcept
{
    x = juce::jlimit(-4.5f, 4.5f, x);
    float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

// Asymmetric modded JCM800 12AX7 + 6550 saturation stage
static inline float tubeStageClip(float x, float bias, float asymmetry) noexcept
{
    float shifted = x + bias;
    float pos = fastTanh(shifted);
    float neg = fastTanh(shifted * (1.0f - asymmetry));
    return (shifted >= 0.0f ? pos : neg) - fastTanh(bias);
}

//==============================================================================
CybernoizzAudioProcessor::CybernoizzAudioProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, &undoManager, "CYBERNOIZZ_PARAMS", createParameterLayout())
{
    formatManager.registerBasicFormats();

    oversampler = std::make_unique<juce::dsp::Oversampling<float>>(
        2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);

    initFactoryPresets();

    // Load default factory IRs (Left = Bearsville Recto 4x12 V30, Right = ZeroSignal 1960 G12T75)
    loadFactoryIr(0, 0);
    loadFactoryIr(1, 1);

    // Initialize A/B comparison slots with default preset state
    loadFactoryPreset(0);
    abStateSlot[0] = apvts.copyState();
    abStateSlot[1] = apvts.copyState();
}

CybernoizzAudioProcessor::~CybernoizzAudioProcessor() = default;

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout CybernoizzAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ "bypass", 1 }, "Bypass", false));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "routing", 1 }, "Routing Mode",
        juce::StringArray{ "Mono", "Mono -> Stereo", "Stereo" }, 1));

    // Gate & Pre-Conditioning
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "nr", 1 }, "NR (Noise Reduction)",
        juce::NormalisableRange<float>(0.0f, 10.0f, 0.01f), 4.5f));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ "tight", 1 }, "Tight (Gunshot Gate)", true));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "boost", 1 }, "Boost (Sub Kick & Bass)",
        juce::NormalisableRange<float>(0.0f, 10.0f, 0.01f), 4.2f));

    // Modded JCM800 2203 "Elan Metalhead" Preamp + 6550 Power Amp Controls
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "gain", 1 }, "Gain",
        juce::NormalisableRange<float>(0.0f, 10.0f, 0.01f), 7.8f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "bass", 1 }, "Bass",
        juce::NormalisableRange<float>(0.0f, 10.0f, 0.01f), 7.2f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "mid", 1 }, "Mid",
        juce::NormalisableRange<float>(0.0f, 10.0f, 0.01f), 2.8f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "midScoop", 1 }, "Industrial Scoop",
        juce::NormalisableRange<float>(0.0f, 10.0f, 0.01f), 7.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "treble", 1 }, "Treble",
        juce::NormalisableRange<float>(0.0f, 10.0f, 0.01f), 7.6f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "presence", 1 }, "Presence",
        juce::NormalisableRange<float>(0.0f, 10.0f, 0.01f), 8.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "depth", 1 }, "Depth",
        juce::NormalisableRange<float>(0.0f, 10.0f, 0.01f), 6.8f));

    // Dual Cabinet IR Controls
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "cabMix", 1 }, "Cab L/R Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "cabLeftVol", 1 }, "Left Cab Volume",
        juce::NormalisableRange<float>(-24.0f, 6.0f, 0.1f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "cabLeftPan", 1 }, "Left Cab Pan",
        juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), -0.85f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "cabRightVol", 1 }, "Right Cab Volume",
        juce::NormalisableRange<float>(-24.0f, 6.0f, 0.1f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "cabRightPan", 1 }, "Right Cab Pan",
        juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), 0.85f));

    // "Spread" Haas & 200% Stereo Widener
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "spread", 1 }, "Spread (Haas / 200% Width)",
        juce::NormalisableRange<float>(0.0f, 10.0f, 0.01f), 5.5f));

    // Master Input / Output Trim
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "inputGain", 1 }, "Input Level",
        juce::NormalisableRange<float>(-18.0f, 18.0f, 0.1f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "outputGain", 1 }, "Output Level",
        juce::NormalisableRange<float>(-24.0f, 12.0f, 0.1f), 0.0f));

    return { params.begin(), params.end() };
}

//==============================================================================
const juce::StringArray& CybernoizzAudioProcessor::getFactoryIrNames()
{
    static const juce::StringArray names = {
        "01 Bearsville Recto 4x12 V30 (SM57)",
        "02 ZeroSignal 1960 G12T75 (MD421)",
        "03 Replica Hybrid 4x12 (57+421)",
        "04 NewBreed Industrial Scoop 4x12",
        "05 Obsolete EdgeCrusher 7-String 4x12",
        "06 SoulOfANewMachine '92 LA 4x12",
        "07 Mechanize Laboga V30+K100 4x12",
        "08 CyberTight Studio 2x12 (57+R121)"
    };
    return names;
}

void CybernoizzAudioProcessor::loadFactoryIr(int side, int factoryIndex)
{
    side = juce::jlimit(0, 1, side);
    factoryIndex = juce::jlimit(0, 7, factoryIndex);

    const char* dataPtr = nullptr;
    int dataSize = 0;

    switch (factoryIndex)
    {
        case 0: dataPtr = BinaryData::_01_Demanufacture_Bearsville_Recto412_V30_SM57_wav; dataSize = BinaryData::_01_Demanufacture_Bearsville_Recto412_V30_SM57_wavSize; break;
        case 1: dataPtr = BinaryData::_02_ZeroSignal_Marshall1960_G12T75_MD421_wav;      dataSize = BinaryData::_02_ZeroSignal_Marshall1960_G12T75_MD421_wavSize;      break;
        case 2: dataPtr = BinaryData::_03_Replica_Hybrid_412_V30_57_421_Blend_wav;        dataSize = BinaryData::_03_Replica_Hybrid_412_V30_57_421_Blend_wavSize;        break;
        case 3: dataPtr = BinaryData::_04_NewBreed_Industrial_Scoop_412_SM57_OffAxis_wav; dataSize = BinaryData::_04_NewBreed_Industrial_Scoop_412_SM57_OffAxis_wavSize; break;
        case 4: dataPtr = BinaryData::_05_Obsolete_EdgeCrusher_Oversized412_7String_wav;  dataSize = BinaryData::_05_Obsolete_EdgeCrusher_Oversized412_7String_wavSize;  break;
        case 5: dataPtr = BinaryData::_06_SoulOfANewMachine_1992_LA_DeathThrash_412_wav;  dataSize = BinaryData::_06_SoulOfANewMachine_1992_LA_DeathThrash_412_wavSize;  break;
        case 6: dataPtr = BinaryData::_07_Mechanize_Laboga_Custom412_V30_K100_wav;        dataSize = BinaryData::_07_Mechanize_Laboga_Custom412_V30_K100_wavSize;        break;
        case 7: dataPtr = BinaryData::_08_Cyber_Tight_2x12_Studio_Precision_SM57_R121_wav;dataSize = BinaryData::_08_Cyber_Tight_2x12_Studio_Precision_SM57_R121_wavSize;break;
        default: break;
    }

    if (dataPtr != nullptr && dataSize > 0)
    {
        auto memStream = std::make_unique<juce::MemoryInputStream>(dataPtr, (size_t)dataSize, false);
        std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(std::move(memStream)));
        if (reader != nullptr)
        {
            juce::AudioBuffer<float> tempBuf((int)reader->numChannels, (int)reader->lengthInSamples);
            reader->read(&tempBuf, 0, (int)reader->lengthInSamples, 0, true, true);
            if (side == 0)
                cabConvolverL.loadIrFromBuffer(tempBuf, reader->sampleRate);
            else
                cabConvolverR.loadIrFromBuffer(tempBuf, reader->sampleRate);

            loadedFactoryIrIndex[side] = factoryIndex;
            customIrPath[side].clear();
            loadedIrDisplay[side] = getFactoryIrNames()[factoryIndex];
        }
    }
}

bool CybernoizzAudioProcessor::loadExternalIrFromFile(int side, const juce::File& wavFile)
{
    side = juce::jlimit(0, 1, side);
    if (!wavFile.existsAsFile())
        return false;

    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(wavFile));
    if (reader == nullptr || reader->lengthInSamples <= 0)
        return false;

    int numSamples = (int)juce::jmin<juce::int64>(reader->lengthInSamples, 16384);
    juce::AudioBuffer<float> tempBuf((int)reader->numChannels, numSamples);
    reader->read(&tempBuf, 0, numSamples, 0, true, true);

    if (side == 0)
        cabConvolverL.loadIrFromBuffer(tempBuf, reader->sampleRate);
    else
        cabConvolverR.loadIrFromBuffer(tempBuf, reader->sampleRate);

    loadedFactoryIrIndex[side] = -1;
    customIrPath[side] = wavFile.getFullPathName();
    loadedIrDisplay[side] = "EXT: " + wavFile.getFileNameWithoutExtension();
    return true;
}

juce::String CybernoizzAudioProcessor::getLoadedIrDisplayName(int side) const
{
    return loadedIrDisplay[juce::jlimit(0, 1, side)];
}

//==============================================================================
void CybernoizzAudioProcessor::initFactoryPresets()
{
    // 14 Authentic Fear Factory song tones spanning Demanufacture, Obsolete, Soul of a New Machine & Mechanize
    // Columns: Name, Category, NR, Tight, Boost, Gain, Bass, Mid, Scoop, Treble, Presence, Depth, CabL, CabR, Mix, VolL, PanL, VolR, PanR, Spread, In, Out
    factoryPresets = {
        { "01 Demanufacture (Album Rhythm)", "Demanufacture (1995)", 4.8f, true,  4.5f, 8.0f, 7.4f, 2.6f, 7.8f, 7.8f, 8.2f, 7.0f, 0, 1, 0.50f,  0.0f, -0.90f,  0.0f,  0.90f, 5.8f,  0.0f,  0.0f },
        { "02 Self Bias Resistor (Quad Pick)", "Demanufacture (1995)", 5.2f, true,  5.0f, 8.2f, 7.0f, 3.0f, 7.4f, 8.1f, 8.4f, 6.8f, 0, 2, 0.45f,  0.0f, -0.85f,  0.0f,  0.85f, 5.2f,  0.5f,  0.0f },
        { "03 Zero Signal (Machine Gun Staccato)", "Demanufacture (1995)", 6.5f, true,  6.2f, 8.6f, 7.2f, 2.2f, 8.6f, 8.4f, 8.8f, 7.4f, 1, 3, 0.55f,  0.5f, -1.00f,  0.5f,  1.00f, 6.8f,  1.0f, -0.5f },
        { "04 Replica (Radio Hook Chug)", "Demanufacture (1995)", 4.2f, true,  3.8f, 7.4f, 7.5f, 3.6f, 6.4f, 7.4f, 7.6f, 6.8f, 2, 0, 0.50f,  0.0f, -0.80f,  0.0f,  0.80f, 4.8f,  0.0f,  0.0f },
        { "05 New Breed (Cyber Industrial Grind)", "Demanufacture (1995)", 6.0f, true,  5.8f, 8.8f, 6.8f, 1.8f, 9.2f, 8.7f, 9.0f, 6.5f, 3, 1, 0.40f,  0.0f, -0.95f,  0.0f,  0.95f, 7.5f,  1.0f, -0.5f },
        { "06 Dog Day Sunrise (Industrial Sustained)", "Demanufacture (1995)", 3.0f, false, 2.5f, 6.5f, 7.2f, 4.8f, 4.5f, 6.9f, 7.0f, 6.4f, 2, 7, 0.50f,  0.0f, -0.75f,  0.0f,  0.75f, 6.2f,  0.0f,  0.5f },
        { "07 Body Hammer (Crushing Low B Groove)", "Demanufacture (1995)", 5.5f, true,  7.2f, 8.3f, 8.2f, 2.5f, 8.0f, 7.7f, 8.1f, 8.4f, 0, 4, 0.50f,  0.5f, -0.90f,  0.5f,  0.90f, 5.5f,  0.0f, -0.5f },
        { "08 Flashpoint (Hyper-Pick Thrash Razor)", "Demanufacture (1995)", 5.8f, true,  4.8f, 8.5f, 6.6f, 2.9f, 7.9f, 8.5f, 8.7f, 6.2f, 1, 7, 0.50f,  0.0f, -0.88f,  0.0f,  0.88f, 5.0f,  1.5f,  0.0f },
        { "09 H-K (Hunter-Killer Cyber Wall)", "Demanufacture (1995)", 5.4f, true,  6.5f, 8.4f, 7.8f, 2.4f, 8.3f, 8.0f, 8.5f, 7.8f, 0, 3, 0.52f,  0.0f, -1.00f,  0.0f,  1.00f, 8.0f,  0.5f,  0.0f },
        { "10 Pisschrist (Mechanical Thrash Assault)", "Demanufacture (1995)", 5.0f, true,  5.2f, 8.1f, 7.1f, 2.8f, 7.6f, 8.2f, 8.3f, 7.0f, 0, 1, 0.48f,  0.0f, -0.85f,  0.0f,  0.85f, 5.6f,  0.5f,  0.0f },
        { "11 Shock (Obsolete 7-String Drop-A)", "Obsolete (1998)", 5.6f, true,  7.5f, 8.4f, 7.9f, 2.7f, 7.7f, 7.9f, 8.3f, 8.5f, 4, 0, 0.50f,  0.0f, -0.92f,  0.0f,  0.92f, 6.4f,  0.0f, -0.5f },
        { "12 Edgecrusher (Sub-Drop Behemoth)", "Obsolete (1998)", 6.2f, true,  8.6f, 8.7f, 8.4f, 2.4f, 8.2f, 7.8f, 8.4f, 8.8f, 4, 6, 0.50f,  0.5f, -0.95f,  0.5f,  0.95f, 7.0f,  0.5f, -1.0f },
        { "13 Martyr ('92 Soul Of A New Machine)", "Early Era (1992)", 4.0f, true,  3.5f, 7.6f, 6.8f, 3.8f, 5.8f, 7.6f, 7.9f, 6.2f, 5, 1, 0.45f,  0.0f, -0.80f,  0.0f,  0.80f, 4.2f,  0.0f,  0.0f },
        { "14 Powershifter (Mechanize Precision)", "Modern Era (2010)", 6.0f, true,  6.0f, 8.5f, 7.5f, 3.1f, 7.5f, 8.3f, 8.6f, 7.6f, 6, 7, 0.50f,  0.0f, -0.95f,  0.0f,  0.95f, 6.5f,  1.0f, -0.5f }
    };
}

void CybernoizzAudioProcessor::loadFactoryPreset(int index)
{
    if (index < 0 || index >= (int)factoryPresets.size())
        return;

    currentPresetIdx = index;
    const auto& p = factoryPresets[(size_t)index];
    currentPresetName = p.name;

    auto setParam = [this](const juce::String& id, float plainVal)
    {
        if (auto* param = apvts.getParameter(id))
            param->setValueNotifyingHost(param->convertTo0to1(plainVal));
    };

    setParam("nr",          p.nr);
    setParam("tight",       p.tight ? 1.0f : 0.0f);
    setParam("boost",       p.boost);
    setParam("gain",        p.gain);
    setParam("bass",        p.bass);
    setParam("mid",         p.mid);
    setParam("midScoop",    p.midScoop);
    setParam("treble",      p.treble);
    setParam("presence",    p.presence);
    setParam("depth",       p.depth);
    setParam("cabMix",      p.cabMix);
    setParam("cabLeftVol",  p.cabLeftVol);
    setParam("cabLeftPan",  p.cabLeftPan);
    setParam("cabRightVol", p.cabRightVol);
    setParam("cabRightPan", p.cabRightPan);
    setParam("spread",      p.spread);
    setParam("inputGain",   p.inputGain);
    setParam("outputGain",  p.outputGain);

    loadFactoryIr(0, p.cabLeftIdx);
    loadFactoryIr(1, p.cabRightIdx);
}

bool CybernoizzAudioProcessor::saveUserPresetToFile(const juce::File& file)
{
    auto state = apvts.copyState();
    state.setProperty("presetName", file.getFileNameWithoutExtension(), nullptr);
    state.setProperty("cabLeftFactoryIdx", loadedFactoryIrIndex[0], nullptr);
    state.setProperty("cabRightFactoryIdx", loadedFactoryIrIndex[1], nullptr);
    state.setProperty("cabLeftCustomPath", customIrPath[0], nullptr);
    state.setProperty("cabRightCustomPath", customIrPath[1], nullptr);

    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    if (xml == nullptr)
        return false;

    currentPresetName = file.getFileNameWithoutExtension();
    return xml->writeTo(file);
}

bool CybernoizzAudioProcessor::loadUserPresetFromFile(const juce::File& file)
{
    if (!file.existsAsFile())
        return false;

    std::unique_ptr<juce::XmlElement> xml = juce::XmlDocument::parse(file);
    if (xml == nullptr || !xml->hasTagName(apvts.state.getType()))
        return false;

    juce::ValueTree tree = juce::ValueTree::fromXml(*xml);
    apvts.replaceState(tree);
    currentPresetName = tree.getProperty("presetName", file.getFileNameWithoutExtension()).toString();

    for (int side = 0; side < 2; ++side)
    {
        juce::String pathProp = (side == 0) ? "cabLeftCustomPath" : "cabRightCustomPath";
        juce::String idxProp  = (side == 0) ? "cabLeftFactoryIdx" : "cabRightFactoryIdx";
        juce::String extPath  = tree.getProperty(pathProp, "").toString();
        int facIdx            = (int)tree.getProperty(idxProp, side);

        if (extPath.isNotEmpty() && juce::File(extPath).existsAsFile())
            loadExternalIrFromFile(side, juce::File(extPath));
        else
            loadFactoryIr(side, juce::jlimit(0, 7, facIdx));
    }
    return true;
}

void CybernoizzAudioProcessor::switchABState(int targetSlot)
{
    targetSlot = juce::jlimit(0, 1, targetSlot);
    if (targetSlot == activeABSlot)
        return;

    // Save current state into active slot before switching
    abStateSlot[activeABSlot] = apvts.copyState();
    abStateSlot[activeABSlot].setProperty("cabLeftFactoryIdx", loadedFactoryIrIndex[0], nullptr);
    abStateSlot[activeABSlot].setProperty("cabRightFactoryIdx", loadedFactoryIrIndex[1], nullptr);
    abStateSlot[activeABSlot].setProperty("cabLeftCustomPath", customIrPath[0], nullptr);
    abStateSlot[activeABSlot].setProperty("cabRightCustomPath", customIrPath[1], nullptr);

    activeABSlot = targetSlot;
    if (abStateSlot[activeABSlot].isValid())
    {
        apvts.replaceState(abStateSlot[activeABSlot].createCopy());
        for (int side = 0; side < 2; ++side)
        {
            juce::String pathProp = (side == 0) ? "cabLeftCustomPath" : "cabRightCustomPath";
            juce::String idxProp  = (side == 0) ? "cabLeftFactoryIdx" : "cabRightFactoryIdx";
            juce::String extPath  = abStateSlot[activeABSlot].getProperty(pathProp, "").toString();
            int facIdx            = (int)abStateSlot[activeABSlot].getProperty(idxProp, side);
            if (extPath.isNotEmpty() && juce::File(extPath).existsAsFile())
                loadExternalIrFromFile(side, juce::File(extPath));
            else
                loadFactoryIr(side, juce::jlimit(0, 7, facIdx));
        }
    }
}

void CybernoizzAudioProcessor::copyCurrentToOtherABSlot()
{
    int other = 1 - activeABSlot;
    abStateSlot[other] = apvts.copyState();
    abStateSlot[other].setProperty("cabLeftFactoryIdx", loadedFactoryIrIndex[0], nullptr);
    abStateSlot[other].setProperty("cabRightFactoryIdx", loadedFactoryIrIndex[1], nullptr);
    abStateSlot[other].setProperty("cabLeftCustomPath", customIrPath[0], nullptr);
    abStateSlot[other].setProperty("cabRightCustomPath", customIrPath[1], nullptr);
}

//==============================================================================
void CybernoizzAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 48000.0;

    oversampler->initProcessing((size_t)juce::jmax(32, samplesPerBlock));
    oversampler->reset();

    cabConvolverL.prepare(currentSampleRate);
    cabConvolverR.prepare(currentSampleRate);

    // Reload active IRs at the new sample rate
    for (int side = 0; side < 2; ++side)
    {
        if (customIrPath[side].isNotEmpty() && juce::File(customIrPath[side]).existsAsFile())
            loadExternalIrFromFile(side, juce::File(customIrPath[side]));
        else
            loadFactoryIr(side, juce::jlimit(0, 7, loadedFactoryIrIndex[side]));
    }

    for (auto& ch : channelState)
        ch.reset();

    // Prepare Haas / Stereo Widener delay buffers (~35ms max capacity)
    int maxHaasSamples = (int)std::ceil(currentSampleRate * 0.040) + 16;
    haasDelayLineL.assign((size_t)maxHaasSamples, 0.0f);
    haasDelayLineR.assign((size_t)maxHaasSamples, 0.0f);
    haasWritePos = 0;

    spreadLowPassL.reset();  spreadLowPassR.reset();
    spreadHighPassL.reset(); spreadHighPassR.reset();
    sideAirShelf.reset();

    updateFilterCoefficients();
}

void CybernoizzAudioProcessor::releaseResources() {}

bool CybernoizzAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& mainIn  = layouts.getMainInputChannelSet();
    const auto& mainOut = layouts.getMainOutputChannelSet();

    if (mainIn != juce::AudioChannelSet::mono() && mainIn != juce::AudioChannelSet::stereo())
        return false;
    if (mainOut != juce::AudioChannelSet::mono() && mainOut != juce::AudioChannelSet::stereo())
        return false;

    // Support Mono->Mono, Mono->Stereo, and Stereo->Stereo
    if (mainIn == juce::AudioChannelSet::stereo() && mainOut == juce::AudioChannelSet::mono())
        return false;

    return true;
}

void CybernoizzAudioProcessor::updateFilterCoefficients()
{
    const float nr       = apvts.getRawParameterValue("nr")->load();
    const float bass     = apvts.getRawParameterValue("bass")->load();
    const float mid      = apvts.getRawParameterValue("mid")->load();
    const float midScoop = apvts.getRawParameterValue("midScoop")->load();
    const float treble   = apvts.getRawParameterValue("treble")->load();
    const float presence = apvts.getRawParameterValue("presence")->load();
    const float depth    = apvts.getRawParameterValue("depth")->load();

    const double osRate = currentSampleRate * 2.0;

    for (auto& ch : channelState)
    {
        // 1. Cable Hum & Static RF Filters controlled by NR
        float notchAtten = -2.0f * nr; // up to -20dB hum notch at 60Hz & 120Hz
        ch.humNotch60.makePeak(currentSampleRate, 60.0f, 10.0f, notchAtten);
        ch.humNotch120.makePeak(currentSampleRate, 120.0f, 12.0f, notchAtten * 0.75f);
        float rfCutoff = juce::jmap(nr, 0.0f, 10.0f, 14000.0f, 7800.0f);
        ch.rfLowPass.makeLowPass(currentSampleRate, rfCutoff, 0.707f);

        // 2. Intelligent "Tight" Gate Detector Filters
        ch.gateLowDetectorBp.makeBandPass(currentSampleRate, 105.0f, 1.5f);   // Palm-mute chug resonance
        ch.gateHighDetectorHp.makeHighPass(currentSampleRate, 1050.0f, 0.8f); // Sustained notes & harmonics

        // 3. "Boost" Pick-Triggered Sub-Bass / Kick Drum Filters
        ch.subPitchBp.makeBandPass(currentSampleRate, 110.0f, 1.4f);
        ch.subCleanLp.makeLowPass(currentSampleRate, 108.0f, 0.85f);
        ch.kickThumpBp.makeBandPass(currentSampleRate, 56.0f, 3.4f);          // 56Hz Raymond Herrera-locked kick punch

        // 4. Modded JCM800 "Elan Metalhead" Preamp Conditioning (runs at 2x oversampled rate)
        ch.preTightHp.makeHighPass(osRate, 118.0f, 0.85f);                    // Keeps low B/A string razor-tight
        ch.preMidFocusPeak.makePeak(osRate, 1480.0f, 1.4f, 6.5f);             // EMG 81 + modded preamp mid push
        ch.interStageHp1.makeHighPass(osRate, 92.0f, 0.707f);
        ch.interStageLp1.makeLowPass(osRate, 8200.0f, 0.707f);
        ch.interStageHp2.makeHighPass(osRate, 130.0f, 0.8f);

        // 5. Post-Amp Tone Stack + 6550 Power Amp Section (runs at base sample rate)
        float bassDb     = (bass - 5.0f) * 2.1f;                              // +/- 10.5 dB @ 110 Hz
        float midDb      = (mid - 5.0f) * 2.0f;                               // +/- 10.0 dB @ 800 Hz
        float scoopDb    = -midScoop * 1.35f;                                 // Up to -13.5 dB surgical scoop @ 560 Hz
        float trebleDb   = (treble - 5.0f) * 2.1f;                            // +/- 10.5 dB @ 3.2 kHz
        float presenceDb = (presence - 5.0f) * 1.9f;                          // +/- 9.5 dB  @ 4.3 kHz (6550 bite)
        float depthDb    = (depth - 5.0f) * 1.85f;                            // +/- 9.25 dB @ 98 Hz (6550 negative feedback resonance)

        ch.eqBassShelf.makeLowShelf(currentSampleRate, 115.0f, 0.75f, bassDb);
        ch.eqMidPeak.makePeak(currentSampleRate, 820.0f, 1.25f, midDb);
        ch.eqIndustrialScoop.makePeak(currentSampleRate, 560.0f, 2.35f, scoopDb);
        ch.eqTrebleShelf.makeHighShelf(currentSampleRate, 3100.0f, 0.75f, trebleDb);
        ch.eqPresencePeak.makePeak(currentSampleRate, 4250.0f, 1.8f, presenceDb);
        ch.eqDepthResonance.makePeak(currentSampleRate, 96.0f, 2.15f, depthDb);
    }

    // 6. "Spread" Phase-Safe Crossover Filters (220 Hz Linkwitz-Riley-inspired split)
    // Everything below 220 Hz stays 100% phase-locked in the center!
    spreadLowPassL.makeLowPass(currentSampleRate, 220.0f, 0.7071f);
    spreadLowPassR.makeLowPass(currentSampleRate, 220.0f, 0.7071f);
    spreadHighPassL.makeHighPass(currentSampleRate, 220.0f, 0.7071f);
    spreadHighPassR.makeHighPass(currentSampleRate, 220.0f, 0.7071f);
    sideAirShelf.makeHighShelf(currentSampleRate, 2800.0f, 0.7071f, 2.5f);
}

//==============================================================================
void CybernoizzAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples             = buffer.getNumSamples();

    for (int i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, numSamples);

    if (numSamples == 0)
        return;

    // Fetch parameters once per block
    const bool  bypass      = apvts.getRawParameterValue("bypass")->load() > 0.5f;
    const int   routingMode = (int)apvts.getRawParameterValue("routing")->load(); // 0=Mono, 1=Mono->Stereo, 2=Stereo
    const float nrAmount    = apvts.getRawParameterValue("nr")->load();
    const bool  tightMode   = apvts.getRawParameterValue("tight")->load() > 0.5f;
    const float boostAmount = apvts.getRawParameterValue("boost")->load();
    const float gainParam   = apvts.getRawParameterValue("gain")->load();
    const float cabMix      = apvts.getRawParameterValue("cabMix")->load();
    const float cabLeftGain = juce::Decibels::decibelsToGain(apvts.getRawParameterValue("cabLeftVol")->load());
    const float cabLeftPan  = apvts.getRawParameterValue("cabLeftPan")->load();
    const float cabRightGain= juce::Decibels::decibelsToGain(apvts.getRawParameterValue("cabRightVol")->load());
    const float cabRightPan = apvts.getRawParameterValue("cabRightPan")->load();
    const float spreadParam = apvts.getRawParameterValue("spread")->load();
    const float inGain      = juce::Decibels::decibelsToGain(apvts.getRawParameterValue("inputGain")->load());
    const float outGain     = juce::Decibels::decibelsToGain(apvts.getRawParameterValue("outputGain")->load());

    currentRoutingMode.store(routingMode);
    updateFilterCoefficients();

    // Apply Input Gain & Measure Input Peaks (Mono / Stereo)
    buffer.applyGain(inGain);
    float inPeakL = buffer.getMagnitude(0, 0, numSamples);
    float inPeakR = (totalNumInputChannels > 1 && routingMode == 2)
                  ? buffer.getMagnitude(1, 0, numSamples)
                  : inPeakL;
    inputLevels[0].store(juce::jmax(inPeakL, inputLevels[0].load() * 0.82f));
    inputLevels[1].store(juce::jmax(inPeakR, inputLevels[1].load() * 0.82f));

    if (bypass)
    {
        if (routingMode != 2 && totalNumOutputChannels > 1)
            buffer.copyFrom(1, 0, buffer, 0, 0, numSamples);
        outputLevels[0].store(inPeakL);
        outputLevels[1].store(inPeakR);
        gateActivity.store(0.0f);
        subBoostActivity.store(0.0f);
        return;
    }

    // Determine how many amp channels need processing:
    // In Mono (0) or Mono->Stereo (1), we only run the Preamp/Gate/Sub-Boost on Channel 0 (50% CPU savings!)
    const int numAmpChannels = (routingMode == 2 && totalNumInputChannels > 1 && totalNumOutputChannels > 1) ? 2 : 1;

    //==========================================================================
    // STAGE 1: "NR" (Cable/Static Noise Reduction) + Intelligent "Tight" Gate + "Boost" (Pick Sub-Bass & Kick Thump)
    //==========================================================================
    const float sr = (float)currentSampleRate;
    const float nrThresh = juce::Decibels::decibelsToGain(juce::jmap(nrAmount, 0.0f, 10.0f, -85.0f, -38.0f));
    const float nrAttCoeff = std::exp(-1.0f / (0.0012f * sr));
    const float nrRelCoeff = std::exp(-1.0f / (0.0450f * sr));

    // Tight Gate envelope coefficients: ultra-fast 0.4ms attack, gunshot 9ms release on palm-mutes, 85ms on sustained notes
    const float fastEnvCoeff  = std::exp(-1.0f / (0.0015f * sr));
    const float slowEnvCoeff  = std::exp(-1.0f / (0.0350f * sr));
    const float gateOpenCoeff = std::exp(-1.0f / (0.0005f * sr));
    const float gatePalmClose = std::exp(-1.0f / (0.0085f * sr)); // Gunshot 8.5ms stop for Fear Factory staccato riffs
    const float gateSustClose = std::exp(-1.0f / (0.0950f * sr)); // Smooth 95ms decay for sustained chords/leads

    // Sub-Bass "Boost" envelope coefficients
    const float pickFastCoeff = std::exp(-1.0f / (0.0018f * sr));
    const float pickSlowCoeff = std::exp(-1.0f / (0.0280f * sr));
    const float kickDecayCoeff= std::exp(-1.0f / (0.0220f * sr));
    const float boostMix      = (boostAmount / 10.0f) * 0.42f;

    float blockGateSum = 0.0f;
    float blockBoostMax = 0.0f;

    for (int ch = 0; ch < numAmpChannels; ++ch)
    {
        float* data = buffer.getWritePointer(ch);
        auto& st = channelState[(size_t)ch];

        for (int i = 0; i < numSamples; ++i)
        {
            float x = data[i];

            // --- 1A. "NR" Cable Hum & Static Expander ---
            if (nrAmount > 0.05f)
            {
                x = st.humNotch60.process(x);
                x = st.humNotch120.process(x);
                x = st.rfLowPass.process(x);

                float absX = std::abs(x);
                st.nrEnv = (absX > st.nrEnv)
                         ? nrAttCoeff * st.nrEnv + (1.0f - nrAttCoeff) * absX
                         : nrRelCoeff * st.nrEnv + (1.0f - nrRelCoeff) * absX;

                float targetNrGain = 1.0f;
                if (st.nrEnv < nrThresh)
                {
                    float ratio = st.nrEnv / (nrThresh + 1.0e-7f);
                    targetNrGain = ratio * ratio * ratio; // Steep 3:1 downward expander kills static hiss
                }
                st.nrGain = 0.990f * st.nrGain + 0.010f * targetNrGain;
                x *= st.nrGain;
            }

            // --- 1B. Intelligent "Tight" Gate (Gunshot Palm-Mute vs Sustain Discrimination) ---
            float lowEnergy  = std::abs(st.gateLowDetectorBp.process(x));
            float highEnergy = std::abs(st.gateHighDetectorHp.process(x));

            st.lowBandEnv  = 0.992f * st.lowBandEnv  + 0.008f * lowEnergy;
            st.highBandEnv = 0.992f * st.highBandEnv + 0.008f * highEnergy;

            float fullAbs = std::abs(x);
            st.fastTransientEnv = fastEnvCoeff * st.fastTransientEnv + (1.0f - fastEnvCoeff) * fullAbs;
            st.slowTransientEnv = slowEnvCoeff * st.slowTransientEnv + (1.0f - slowEnvCoeff) * fullAbs;

            if (tightMode)
            {
                // Detect whether the player is chugging a palm mute (lowBandEnv dominates highBandEnv)
                // or letting a sustained note/harmonic ring out (highBandEnv stays strong relative to lowBandEnv)
                float palmMuteRatio = st.lowBandEnv / (st.highBandEnv * 2.6f + 1.0e-6f);
                bool isPalmMuted = (palmMuteRatio > 1.35f);

                // Base threshold scales slightly with NR knob, higher for palm-mute tail cutoff
                float baseThresh = juce::Decibels::decibelsToGain(-44.0f + nrAmount * 1.4f);
                float activeThresh = isPalmMuted ? (baseThresh * 2.4f) : (baseThresh * 0.55f);

                // Hysteresis: open immediately on pick transient or signal above activeThresh
                bool shouldOpen = (st.fastTransientEnv > activeThresh) || (st.fastTransientEnv > st.slowTransientEnv * 1.35f && st.fastTransientEnv > baseThresh * 0.5f);
                float targetGate = shouldOpen ? 1.0f : 0.0f;
                float closeCoeff = isPalmMuted ? gatePalmClose : gateSustClose;

                st.tightGateGain = (targetGate > st.tightGateGain)
                                 ? gateOpenCoeff * st.tightGateGain + (1.0f - gateOpenCoeff) * targetGate
                                 : closeCoeff    * st.tightGateGain + (1.0f - closeCoeff)    * targetGate;

                x *= st.tightGateGain;
                blockGateSum += (1.0f - st.tightGateGain);
            }
            else
            {
                st.tightGateGain = 1.0f;
            }

            // --- 1C. "Boost" Pick-Hit Sub-Bass & Kick Drum Generator ---
            if (boostAmount > 0.05f)
            {
                float absGated = std::abs(x);
                st.pickFastEnv = (absGated > st.pickFastEnv)
                               ? 0.6f * st.pickFastEnv + 0.4f * absGated
                               : pickFastCoeff * st.pickFastEnv + (1.0f - pickFastCoeff) * absGated;
                st.pickSlowEnv = pickSlowCoeff * st.pickSlowEnv + (1.0f - pickSlowCoeff) * absGated;

                // Pick transient crest detector (spikes right when pick strikes string)
                float pickCrest = juce::jmax(0.0f, st.pickFastEnv - st.pickSlowEnv * 0.85f);

                // Emphasize when palm muting (lowBandEnv > highBandEnv)
                float palmWeight = juce::jlimit(0.6f, 2.2f, st.lowBandEnv / (st.highBandEnv + 1.0e-5f));
                if (pickCrest > 0.008f)
                    st.kickEnv = juce::jmin(1.0f, st.kickEnv + pickCrest * palmWeight * 6.5f);
                st.kickEnv *= kickDecayCoeff;

                // Synthesize 1) Sub-octave backing bass note via zero-crossing flip divider on fundamental
                float fund = st.subPitchBp.process(x);
                if (fund > 0.0f && st.prevSubSample <= 0.0f)
                    st.subFlipSign = -st.subFlipSign;
                st.prevSubSample = fund;

                float rawSubBass = fund * st.subFlipSign;
                float cleanSubNote = st.subCleanLp.process(rawSubBass);

                // Synthesize 2) Kick drum 56Hz chest thump excited by pick impulse
                float kickThump = st.kickThumpBp.process(x * (1.0f + st.kickEnv * 8.0f));

                float subSignal = (cleanSubNote * 0.65f + kickThump * 0.85f) * st.tightGateGain;
                x += subSignal * boostMix;

                blockBoostMax = juce::jmax(blockBoostMax, std::abs(subSignal) * boostMix * 4.0f);
            }

            data[i] = x;
        }
    }

    gateActivity.store(tightMode ? juce::jlimit(0.0f, 1.0f, blockGateSum / (float)(numSamples * numAmpChannels)) : 0.0f);
    subBoostActivity.store(juce::jlimit(0.0f, 1.0f, blockBoostMax));

    //==========================================================================
    // STAGE 2: Modded Marshall JCM800 2203 "Elan Metalhead" 4-Stage Tube Preamp (2x Oversampled)
    //==========================================================================
    juce::dsp::AudioBlock<float> baseBlock(buffer.getArrayOfWritePointers(), (size_t)numAmpChannels, (size_t)numSamples);
    juce::dsp::AudioBlock<float> osBlock = oversampler->processSamplesUp(baseBlock);
    const int osNumSamples = (int)osBlock.getNumSamples();

    // Map Gain (0..10) to cascaded modded JCM800 drive gains
    const float driveStage1 = juce::jmap(gainParam, 0.0f, 10.0f, 2.2f, 14.5f);
    const float driveStage2 = juce::jmap(gainParam, 0.0f, 10.0f, 1.8f, 8.2f);
    const float driveStage3 = juce::jmap(gainParam, 0.0f, 10.0f, 1.5f, 5.6f);
    const float driveStage4 = juce::jmap(gainParam, 0.0f, 10.0f, 1.2f, 3.4f);

    for (int ch = 0; ch < numAmpChannels; ++ch)
    {
        float* osData = osBlock.getChannelPointer((size_t)ch);
        auto& st = channelState[(size_t)ch];

        for (int i = 0; i < osNumSamples; ++i)
        {
            float s = osData[i];

            // Pre-EQ: Elan Metalhead bass tightener + EMG 81 upper-mid attack focus
            s = st.preTightHp.process(s);
            s = st.preMidFocusPeak.process(s);

            // V1A + V1B: Cold-biased 12AX7 input + Elan extra 12AX7 gain boost stages
            s = tubeStageClip(s * driveStage1, 0.08f, 0.18f);
            s = st.interStageHp1.process(s);
            s = tubeStageClip(s * driveStage2, -0.06f, 0.14f);

            // V2A + V2B: Cathode-follower crunch + 6550 power tube driver saturation
            s = st.interStageLp1.process(s);
            s = tubeStageClip(s * driveStage3, 0.05f, 0.12f);
            s = st.interStageHp2.process(s);
            s = tubeStageClip(s * driveStage4, 0.0f, 0.06f);

            // DC blocker
            float dcOut = s - st.dcBlockerX1 + 0.996f * st.dcBlockerY1;
            st.dcBlockerX1 = s;
            st.dcBlockerY1 = dcOut;

            osData[i] = dcOut * 0.45f;
        }
    }

    oversampler->processSamplesDown(baseBlock);

    //==========================================================================
    // STAGE 3: Post-Amp Tone Stack, Demanufacture Surgical Mid Scoop & 6550 Power Amp EQ
    //==========================================================================
    for (int ch = 0; ch < numAmpChannels; ++ch)
    {
        float* data = buffer.getWritePointer(ch);
        auto& st = channelState[(size_t)ch];

        for (int i = 0; i < numSamples; ++i)
        {
            float s = data[i];
            s = st.eqBassShelf.process(s);
            s = st.eqMidPeak.process(s);
            s = st.eqIndustrialScoop.process(s);
            s = st.eqTrebleShelf.process(s);
            s = st.eqPresencePeak.process(s);
            s = st.eqDepthResonance.process(s);
            data[i] = s;
        }
    }

    //==========================================================================
    // STAGE 4: Dual Cabinet IR Convolver (Left & Right Cabs with Mix, Volume & Pan)
    //==========================================================================
    float* outL = buffer.getWritePointer(0);
    float* outR = (totalNumOutputChannels > 1) ? buffer.getWritePointer(1) : nullptr;

    // Equal-power pan weights for Left Cab and Right Cab
    const float panAngleL = (cabLeftPan + 1.0f) * (juce::MathConstants<float>::pi * 0.25f);
    const float leftCabToL = std::cos(panAngleL) * cabLeftGain * (1.0f - cabMix * 0.5f) * 1.414f;
    const float leftCabToR = std::sin(panAngleL) * cabLeftGain * (1.0f - cabMix * 0.5f) * 1.414f;

    const float panAngleR = (cabRightPan + 1.0f) * (juce::MathConstants<float>::pi * 0.25f);
    const float rightCabToL = std::cos(panAngleR) * cabRightGain * (cabMix * 0.5f + 0.25f) * 1.414f;
    const float rightCabToR = std::sin(panAngleR) * cabRightGain * (cabMix * 0.5f + 0.25f) * 1.414f;

    // Delay offset in samples for "Spread" Haas effect (7.5ms to 16ms above 220Hz)
    const int haasBufLen = (int)haasDelayLineL.size();
    const float spreadNorm = spreadParam / 10.0f;
    const int haasDelaySamples = juce::jlimit(1, haasBufLen - 4,
        (int)std::round((0.0065f + 0.0095f * spreadNorm) * currentSampleRate));

    for (int i = 0; i < numSamples; ++i)
    {
        const float ampInL = outL[i];
        const float ampInR = (numAmpChannels > 1 && outR != nullptr) ? outR[i] : ampInL;

        // Run Left and Right IR Convolvers
        const float cabOutL = cabConvolverL.processSample(ampInL);
        const float cabOutR = cabConvolverR.processSample(ampInR);

        // Combine cabs using their Volume, Pan, and Mix slider
        float stereoL = cabOutL * leftCabToL + cabOutR * rightCabToL;
        float stereoR = cabOutL * leftCabToR + cabOutR * rightCabToR;

        if (routingMode == 0 || outR == nullptr)
        {
            // Pure Mono Output mode: phase-aligned sum of both cabs
            float monoOut = (stereoL + stereoR) * 0.5f * outGain;
            outL[i] = monoOut;
            if (outR != nullptr)
                outR[i] = monoOut;
            continue;
        }

        //======================================================================
        // STAGE 5: "Spread" Knob — Phase-Coherent Low End (<220Hz) + Haas (Mono->Stereo) / 200% Width (Stereo)
        //======================================================================
        if (spreadNorm > 0.005f)
        {
            // Split L & R at 220 Hz so sub-bass & palm-mute thump (<220 Hz) stay 100% mono phase-locked!
            float lowL  = spreadLowPassL.process(stereoL);
            float lowR  = spreadLowPassR.process(stereoR);
            float lowMono = (lowL + lowR) * 0.5f;

            float highL = spreadHighPassL.process(stereoL);
            float highR = spreadHighPassR.process(stereoR);

            // Write upper-frequency signal into circular Haas delay line
            haasDelayLineL[(size_t)haasWritePos] = highL;
            haasDelayLineR[(size_t)haasWritePos] = highR;

            int readIdx = haasWritePos - haasDelaySamples;
            if (readIdx < 0)
                readIdx += haasBufLen;

            float delayedHighL = haasDelayLineL[(size_t)readIdx];
            float delayedHighR = haasDelayLineR[(size_t)readIdx];

            if (++haasWritePos >= haasBufLen)
                haasWritePos = 0;

            if (routingMode == 1)
            {
                // MONO -> STEREO MODE:
                // Create massive double-tracked Haas stereo width on >220Hz while keeping mono-sum cancellation zero
                // by blending orthogonalized delayed high-band with subtle air-shelf decorrelation
                float haasSide = sideAirShelf.process(delayedHighL) * spreadNorm * 0.85f;
                highL = highL + haasSide * 0.35f;
                highR = (highR * (1.0f - spreadNorm * 0.55f)) + delayedHighR * (spreadNorm * 0.88f) - haasSide * 0.25f;
            }
            else
            {
                // STEREO INPUT MODE (100% -> 200% Stereo Expansion):
                // Mid/Side matrix on >220Hz only, pushing Side gain from 1.0x (100%) up to 2.0x (200%)
                // plus subtle micro-Haas decorrelation so hard-panned rhythm guitars jump outside the speakers
                float mid  = (highL + highR) * 0.5f;
                float side = (highL - highR) * 0.5f;

                float sideBoost = 1.0f + spreadNorm; // 1.0 (100%) to 2.0 (200%)
                float decorrSide = (delayedHighL - delayedHighR) * 0.18f * spreadNorm;
                float wideSide = side * sideBoost + decorrSide;

                // Compensate Mid slightly at extreme 200% settings to keep constant perceived loudness
                float midComp = 1.0f - spreadNorm * 0.14f;
                highL = mid * midComp + wideSide;
                highR = mid * midComp - wideSide;
            }

            stereoL = lowMono + highL;
            stereoR = lowMono + highR;
        }

        outL[i] = stereoL * outGain;
        outR[i] = stereoR * outGain;
    }

    // Update Output Meters
    float outPeakL = buffer.getMagnitude(0, 0, numSamples);
    float outPeakR = (totalNumOutputChannels > 1) ? buffer.getMagnitude(1, 0, numSamples) : outPeakL;
    outputLevels[0].store(juce::jmax(outPeakL, outputLevels[0].load() * 0.82f));
    outputLevels[1].store(juce::jmax(outPeakR, outputLevels[1].load() * 0.82f));
}

//==============================================================================
const juce::String CybernoizzAudioProcessor::getName() const { return JucePlugin_Name; }
bool CybernoizzAudioProcessor::acceptsMidi() const { return false; }
bool CybernoizzAudioProcessor::producesMidi() const { return false; }
bool CybernoizzAudioProcessor::isMidiEffect() const { return false; }
double CybernoizzAudioProcessor::getTailLengthSeconds() const { return 0.05; }

int CybernoizzAudioProcessor::getNumPrograms() { return (int)factoryPresets.size(); }
int CybernoizzAudioProcessor::getCurrentProgram() { return currentPresetIdx; }
void CybernoizzAudioProcessor::setCurrentProgram(int index) { loadFactoryPreset(index); }
const juce::String CybernoizzAudioProcessor::getProgramName(int index)
{
    if (index >= 0 && index < (int)factoryPresets.size())
        return factoryPresets[(size_t)index].name;
    return {};
}
void CybernoizzAudioProcessor::changeProgramName(int, const juce::String&) {}

//==============================================================================
void CybernoizzAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty("presetName", currentPresetName, nullptr);
    state.setProperty("cabLeftFactoryIdx", loadedFactoryIrIndex[0], nullptr);
    state.setProperty("cabRightFactoryIdx", loadedFactoryIrIndex[1], nullptr);
    state.setProperty("cabLeftCustomPath", customIrPath[0], nullptr);
    state.setProperty("cabRightCustomPath", customIrPath[1], nullptr);

    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void CybernoizzAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
    {
        juce::ValueTree tree = juce::ValueTree::fromXml(*xmlState);
        apvts.replaceState(tree);
        currentPresetName = tree.getProperty("presetName", currentPresetName).toString();

        for (int side = 0; side < 2; ++side)
        {
            juce::String pathProp = (side == 0) ? "cabLeftCustomPath" : "cabRightCustomPath";
            juce::String idxProp  = (side == 0) ? "cabLeftFactoryIdx" : "cabRightFactoryIdx";
            juce::String extPath  = tree.getProperty(pathProp, "").toString();
            int facIdx            = (int)tree.getProperty(idxProp, side);

            if (extPath.isNotEmpty() && juce::File(extPath).existsAsFile())
                loadExternalIrFromFile(side, juce::File(extPath));
            else
                loadFactoryIr(side, juce::jlimit(0, 7, facIdx));
        }
    }
}

bool CybernoizzAudioProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* CybernoizzAudioProcessor::createEditor()
{
    return new CybernoizzAudioProcessorEditor(*this);
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CybernoizzAudioProcessor();
}
