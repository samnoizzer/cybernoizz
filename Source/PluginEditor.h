#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
// Custom Futuristic 3D PCB LookAndFeel (Neon Green #00FF66, Carbon Black, Dark Metallic Gray)
class CyberPcbLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CyberPcbLookAndFeel();

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider& slider) override;

    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float minSliderPos, float maxSliderPos,
                          const juce::Slider::SliderStyle style, juce::Slider& slider) override;

    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override;

    void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH,
                      juce::ComboBox& box) override;
};

//==============================================================================
// Dual-Channel (Mono / Stereo L+R) Neon LED Peak + RMS Bar Meter
class CyberStereoMeter : public juce::Component
{
public:
    CyberStereoMeter(const juce::String& titleText) : title(titleText) {}

    void setLevels(float leftPeak, float rightPeak, bool stereoActive)
    {
        levelL = juce::jlimit(0.0f, 1.25f, leftPeak);
        levelR = juce::jlimit(0.0f, 1.25f, rightPeak);
        isStereo = stereoActive;
        repaint();
    }

    void paint(juce::Graphics& g) override;

private:
    juce::String title;
    float levelL = 0.0f;
    float levelR = 0.0f;
    bool isStereo = true;
};

//==============================================================================
class CybernoizzAudioProcessorEditor : public juce::AudioProcessorEditor,
                                       private juce::Timer
{
public:
    explicit CybernoizzAudioProcessorEditor(CybernoizzAudioProcessor&);
    ~CybernoizzAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void setupKnob(juce::Slider& slider, juce::Label& label, const juce::String& text, const juce::String& suffix = "");
    void drawPcbTracesAndChassis(juce::Graphics& g, juce::Rectangle<float> bounds);
    void refreshPresetAndIrDisplays();

    CybernoizzAudioProcessor& audioProcessor;
    CyberPcbLookAndFeel pcbLookAndFeel;

    // --- Top Toolbar Controls ---
    juce::ToggleButton bypassButton   { "BYPASS" };
    juce::ComboBox     routingBox;
    juce::ComboBox     presetBox;
    juce::TextButton   savePresetBtn  { "SAVE" };
    juce::TextButton   loadPresetBtn  { "LOAD" };
    juce::TextButton   undoBtn        { "UNDO" };
    juce::TextButton   redoBtn        { "REDO" };
    juce::TextButton   slotABtn       { "A" };
    juce::TextButton   slotBBtn       { "B" };
    juce::TextButton   copyABBtn      { "COPY" };

    // --- Pre-Conditioning & Dynamics Module (NR, Tight, Boost) ---
    juce::Slider       nrSlider;
    juce::Label        nrLabel;
    juce::ToggleButton tightButton    { "TIGHT [GUNSHOT GATE]" };
    juce::Slider       boostSlider;
    juce::Label        boostLabel;

    // --- Modded JCM800 "Elan Metalhead" Preamp + 6550 Power Amp Knobs ---
    juce::Slider gainSlider, bassSlider, midSlider, scoopSlider, trebleSlider, presenceSlider, depthSlider;
    juce::Label  gainLabel,  bassLabel,  midLabel,  scoopLabel,  trebleLabel,  presenceLabel,  depthLabel;

    // --- Master I/O & "Spread" Widener Knobs ---
    juce::Slider inputSlider, outputSlider, spreadSlider;
    juce::Label  inputLabel,  outputLabel,  spreadLabel;

    // --- Dual Cabinet IR Loading Bars (Left & Right Cabs + Mix + Vol + Pan) ---
    juce::ComboBox   leftCabCombo, rightCabCombo;
    juce::TextButton leftLoadIrBtn  { "LOAD WAV..." };
    juce::TextButton rightLoadIrBtn { "LOAD WAV..." };
    juce::Label      leftIrStatusLabel, rightIrStatusLabel;

    juce::Slider cabMixSlider;
    juce::Label  cabMixLabel;
    juce::Slider cabLeftVolSlider,  cabLeftPanSlider;
    juce::Label  cabLeftVolLabel,   cabLeftPanLabel;
    juce::Slider cabRightVolSlider, cabRightPanSlider;
    juce::Label  cabRightVolLabel,  cabRightPanLabel;

    // --- Input & Output Mono/Stereo Meters ---
    CyberStereoMeter inputMeter  { "IN L/R" };
    CyberStereoMeter outputMeter { "OUT L/R" };

    // --- APVTS Attachments ---
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<ButtonAttachment> bypassAttach, tightAttach;
    std::unique_ptr<ComboAttachment>  routingAttach;
    std::unique_ptr<SliderAttachment> nrAttach, boostAttach, gainAttach, bassAttach, midAttach,
                                      scoopAttach, trebleAttach, presenceAttach, depthAttach,
                                      cabMixAttach, cabLeftVolAttach, cabLeftPanAttach,
                                      cabRightVolAttach, cabRightPanAttach, spreadAttach,
                                      inputAttach, outputAttach;

    std::unique_ptr<juce::FileChooser> activeFileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CybernoizzAudioProcessorEditor)
};
