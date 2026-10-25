#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace CyberColors
{
    const juce::Colour bgDeepBlack    { 0xff07090b };
    const juce::Colour chassisDark    { 0xff101418 };
    const juce::Colour moduleSurface  { 0xff161c22 };
    const juce::Colour metalBevelLight{ 0xff2c3742 };
    const juce::Colour metalBevelDark { 0xff050708 };
    const juce::Colour pcbTraceDim    { 0xff0b3b24 };
    const juce::Colour pcbTraceGlow   { 0xff00b84c };
    const juce::Colour neonGreen      { 0xff00ff66 };
    const juce::Colour neonGreenBright{ 0xff7affb2 };
    const juce::Colour textSilver     { 0xffd0dce5 };
    const juce::Colour textMuted      { 0xff7c8b99 };
}

//==============================================================================
CyberPcbLookAndFeel::CyberPcbLookAndFeel()
{
    setColour(juce::Slider::textBoxTextColourId,       CyberColors::neonGreen);
    setColour(juce::Slider::textBoxBackgroundColourId, CyberColors::bgDeepBlack);
    setColour(juce::Slider::textBoxOutlineColourId,    CyberColors::pcbTraceDim);
    setColour(juce::ComboBox::backgroundColourId,      CyberColors::bgDeepBlack);
    setColour(juce::ComboBox::textColourId,            CyberColors::neonGreen);
    setColour(juce::ComboBox::outlineColourId,         CyberColors::pcbTraceGlow);
    setColour(juce::PopupMenu::backgroundColourId,     CyberColors::chassisDark);
    setColour(juce::PopupMenu::textColourId,           CyberColors::textSilver);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, CyberColors::pcbTraceDim);
    setColour(juce::PopupMenu::highlightedTextColourId,       CyberColors::neonGreen);
}

void CyberPcbLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                           float sliderPos, float rotaryStartAngle,
                                           float rotaryEndAngle, juce::Slider& slider)
{
    auto bounds = juce::Rectangle<float>((float)x, (float)y, (float)width, (float)height).reduced(4.0f);
    float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    auto centre = bounds.getCentre();
    float rx = centre.x - radius;
    float ry = centre.y - radius;
    float rw = radius * 2.0f;
    float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    // 1. Outer PCB LED Arc Track (Dark background groove + Neon Green active glow)
    juce::Path bgArc;
    bgArc.addCentredArc(centre.x, centre.y, radius * 0.92f, radius * 0.92f, 0.0f,
                        rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(CyberColors::bgDeepBlack);
    g.strokePath(bgArc, juce::PathStrokeType(4.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path valArc;
    valArc.addCentredArc(centre.x, centre.y, radius * 0.92f, radius * 0.92f, 0.0f,
                         rotaryStartAngle, angle, true);
    g.setColour(CyberColors::neonGreen.withAlpha(0.28f));
    g.strokePath(valArc, juce::PathStrokeType(7.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(CyberColors::neonGreen);
    g.strokePath(valArc, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // 2. 3D Drop Shadow under the metallic turret knob
    float knobRadius = radius * 0.74f;
    g.setColour(juce::Colours::black.withAlpha(0.75f));
    g.fillEllipse(centre.x - knobRadius + 2.0f, centre.y - knobRadius + 4.0f, knobRadius * 2.0f, knobRadius * 2.0f);

    // 3. Outer Beveled Anodized Dark Gray Cylinder
    juce::ColourGradient outerGrad(CyberColors::metalBevelLight, centre.x - knobRadius * 0.6f, centre.y - knobRadius * 0.6f,
                                   CyberColors::metalBevelDark,  centre.x + knobRadius * 0.7f, centre.y + knobRadius * 0.7f, false);
    g.setGradientFill(outerGrad);
    g.fillEllipse(centre.x - knobRadius, centre.y - knobRadius, knobRadius * 2.0f, knobRadius * 2.0f);

    // Knurled turbine notches around perimeter for 3D hardware depth
    g.setColour(CyberColors::bgDeepBlack.withAlpha(0.7f));
    const int numNotches = 16;
    for (int i = 0; i < numNotches; ++i)
    {
        float a = i * (juce::MathConstants<float>::twoPi / (float)numNotches) + angle * 0.25f;
        float x1 = centre.x + std::sin(a) * (knobRadius * 0.84f);
        float y1 = centre.y - std::cos(a) * (knobRadius * 0.84f);
        float x2 = centre.x + std::sin(a) * (knobRadius * 0.99f);
        float y2 = centre.y - std::cos(a) * (knobRadius * 0.99f);
        g.drawLine(x1, y1, x2, y2, 1.6f);
    }

    // 4. Inner Concave Cap with Specular Highlight & Neon Ring
    float innerRadius = knobRadius * 0.78f;
    juce::ColourGradient capGrad(juce::Colour(0xff232b33), centre.x, centre.y - innerRadius,
                                 juce::Colour(0xff0b0e12), centre.x, centre.y + innerRadius, false);
    g.setGradientFill(capGrad);
    g.fillEllipse(centre.x - innerRadius, centre.y - innerRadius, innerRadius * 2.0f, innerRadius * 2.0f);

    g.setColour(CyberColors::pcbTraceGlow.withAlpha(0.55f));
    g.drawEllipse(centre.x - innerRadius, centre.y - innerRadius, innerRadius * 2.0f, innerRadius * 2.0f, 1.2f);

    // 5. Laser-Etched Neon Green Indicator Line + Tip Via Dot
    juce::Path pointer;
    float pLen = innerRadius * 0.88f;
    pointer.addRoundedRectangle(-1.8f, -pLen, 3.6f, pLen * 0.62f, 1.5f);
    pointer.applyTransform(juce::AffineTransform::rotation(angle).translated(centre.x, centre.y));

    g.setColour(CyberColors::neonGreen.withAlpha(0.4f));
    g.strokePath(pointer, juce::PathStrokeType(3.5f));
    g.setColour(CyberColors::neonGreenBright);
    g.fillPath(pointer);

    juce::ignoreUnused(rx, ry, rw, slider);
}

void CyberPcbLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                                           float sliderPos, float, float,
                                           const juce::Slider::SliderStyle, juce::Slider&)
{
    auto trackBounds = juce::Rectangle<float>((float)x, (float)y + (float)height * 0.5f - 4.0f, (float)width, 8.0f);
    g.setColour(CyberColors::bgDeepBlack);
    g.fillRoundedRectangle(trackBounds, 4.0f);
    g.setColour(CyberColors::pcbTraceDim);
    g.drawRoundedRectangle(trackBounds, 4.0f, 1.2f);

    // Center tick mark
    float midX = (float)x + (float)width * 0.5f;
    g.setColour(CyberColors::neonGreen.withAlpha(0.5f));
    g.drawLine(midX, (float)y + 2.0f, midX, (float)(y + height - 2), 1.5f);

    // Glowing active trace from center to thumb
    float startX = juce::jmin(midX, sliderPos);
    float spanW  = std::abs(sliderPos - midX);
    g.setColour(CyberColors::neonGreen.withAlpha(0.65f));
    g.fillRoundedRectangle(startX, trackBounds.getY() + 1.5f, spanW, trackBounds.getHeight() - 3.0f, 2.0f);

    // 3D Fader Cap
    juce::Rectangle<float> thumb(sliderPos - 9.0f, (float)y + 3.0f, 18.0f, (float)height - 6.0f);
    g.setColour(juce::Colours::black.withAlpha(0.7f));
    g.fillRoundedRectangle(thumb.translated(1.5f, 2.0f), 3.0f);

    juce::ColourGradient thumbGrad(CyberColors::metalBevelLight, thumb.getX(), thumb.getY(),
                                   CyberColors::chassisDark,     thumb.getRight(), thumb.getBottom(), false);
    g.setGradientFill(thumbGrad);
    g.fillRoundedRectangle(thumb, 3.0f);

    g.setColour(CyberColors::neonGreen);
    g.drawRoundedRectangle(thumb, 3.0f, 1.4f);
    g.drawLine(sliderPos, thumb.getY() + 3.0f, sliderPos, thumb.getBottom() - 3.0f, 2.2f);
}

void CyberPcbLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                               const juce::Colour&,
                                               bool isHighlighted, bool isDown)
{
    auto b = button.getLocalBounds().toFloat().reduced(1.5f);
    bool toggled = button.getToggleState();

    // 3D recessed/extruded bevel
    g.setColour(juce::Colours::black.withAlpha(0.65f));
    g.fillRoundedRectangle(b.translated(1.0f, 2.0f), 4.0f);

    juce::Colour topCol = toggled ? juce::Colour(0xff0b3d22) : juce::Colour(0xff1e262e);
    juce::Colour botCol = toggled ? juce::Colour(0xff062012) : juce::Colour(0xff0d1115);
    if (isHighlighted)
        topCol = topCol.brighter(0.15f);
    if (isDown)
        std::swap(topCol, botCol);

    juce::ColourGradient grad(topCol, b.getX(), b.getY(), botCol, b.getX(), b.getBottom(), false);
    g.setGradientFill(grad);
    g.fillRoundedRectangle(b, 4.0f);

    juce::Colour borderCol = toggled ? CyberColors::neonGreen : (isHighlighted ? CyberColors::pcbTraceGlow : CyberColors::metalBevelLight);
    g.setColour(borderCol);
    g.drawRoundedRectangle(b, 4.0f, toggled ? 1.8f : 1.1f);

    // Subtle LED status bar on toggled buttons
    if (toggled)
    {
        g.setColour(CyberColors::neonGreen.withAlpha(0.25f));
        g.fillRoundedRectangle(b.reduced(2.0f), 3.0f);
    }
}

void CyberPcbLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool,
                                       int, int, int, int, juce::ComboBox& box)
{
    auto b = juce::Rectangle<float>(0.0f, 0.0f, (float)width, (float)height).reduced(1.0f);
    g.setColour(CyberColors::bgDeepBlack);
    g.fillRoundedRectangle(b, 4.0f);

    g.setColour(CyberColors::pcbTraceGlow.withAlpha(0.8f));
    g.drawRoundedRectangle(b, 4.0f, 1.3f);

    // Neon green PCB chevron arrow
    juce::Path arrow;
    float ax = (float)width - 18.0f;
    float ay = (float)height * 0.5f - 2.5f;
    arrow.startNewSubPath(ax, ay);
    arrow.lineTo(ax + 5.0f, ay + 5.5f);
    arrow.lineTo(ax + 10.0f, ay);
    g.setColour(CyberColors::neonGreen);
    g.strokePath(arrow, juce::PathStrokeType(2.0f));
    juce::ignoreUnused(box);
}

//==============================================================================
void CyberStereoMeter::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour(CyberColors::bgDeepBlack);
    g.fillRoundedRectangle(b, 4.0f);
    g.setColour(CyberColors::pcbTraceDim);
    g.drawRoundedRectangle(b, 4.0f, 1.2f);

    auto titleArea = b.removeFromBottom(16.0f);
    g.setColour(CyberColors::neonGreen);
    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    g.drawText(title + (isStereo ? " [ST]" : " [M]"), titleArea, juce::Justification::centred, false);

    auto barsArea = b.reduced(5.0f, 5.0f);
    float barW = (barsArea.getWidth() - 4.0f) * 0.5f;

    auto drawBar = [&](juce::Rectangle<float> rect, float level)
    {
        const int numSegments = 22;
        float segGap = 1.8f;
        float segH = (rect.getHeight() - segGap * (numSegments - 1)) / (float)numSegments;

        // Convert linear peak to normalized dB scale (-48dB .. +3dB)
        float db = juce::Decibels::gainToDecibels(level, -48.0f);
        float norm = juce::jlimit(0.0f, 1.0f, (db + 48.0f) / 51.0f);
        int litSegments = (int)std::round(norm * numSegments);

        for (int s = 0; s < numSegments; ++s)
        {
            float segY = rect.getBottom() - (s + 1) * segH - s * segGap;
            juce::Rectangle<float> segRect(rect.getX(), segY, rect.getWidth(), segH);

            bool lit = (s < litSegments);
            juce::Colour c = CyberColors::neonGreen;
            if (s >= numSegments - 2)
                c = juce::Colour(0xffff3355); // Clip warning LED
            else if (s >= numSegments - 5)
                c = juce::Colour(0xffb8ff00); // Hot signal LED

            g.setColour(lit ? c : c.withAlpha(0.10f));
            g.fillRoundedRectangle(segRect, 1.2f);
        }
    };

    drawBar(juce::Rectangle<float>(barsArea.getX(), barsArea.getY(), barW, barsArea.getHeight()), levelL);
    drawBar(juce::Rectangle<float>(barsArea.getX() + barW + 4.0f, barsArea.getY(), barW, barsArea.getHeight()), isStereo ? levelR : levelL);
}

//==============================================================================
CybernoizzAudioProcessorEditor::CybernoizzAudioProcessorEditor(CybernoizzAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setLookAndFeel(&pcbLookAndFeel);

    // Make GUI Resizable with aspect ratio constraints (Futuristic 3D PCB Chassis)
    setResizable(true, true);
    setResizeLimits(880, 560, 1760, 1120);
    getConstrainer()->setFixedAspectRatio(1100.0 / 700.0);
    setSize(1100, 700);

    // --- 1. Top Toolbar: Bypass, Routing, Presets, Save/Load, Undo/Redo, A/B ---
    addAndMakeVisible(bypassButton);
    bypassAttach = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "bypass", bypassButton);

    routingBox.addItemList({ "MONO", "MONO -> STEREO", "STEREO" }, 1);
    addAndMakeVisible(routingBox);
    routingAttach = std::make_unique<ComboAttachment>(audioProcessor.apvts, "routing", routingBox);

    const auto& presets = audioProcessor.getFactoryPresets();
    for (int i = 0; i < (int)presets.size(); ++i)
        presetBox.addItem(presets[(size_t)i].name, i + 1);
    presetBox.setSelectedId(1, juce::dontSendNotification);
    presetBox.onChange = [this]()
    {
        int idx = presetBox.getSelectedId() - 1;
        if (idx >= 0)
        {
            audioProcessor.loadFactoryPreset(idx);
            refreshPresetAndIrDisplays();
        }
    };
    addAndMakeVisible(presetBox);

    savePresetBtn.onClick = [this]()
    {
        activeFileChooser = std::make_unique<juce::FileChooser>(
            "Save CYBERNOIZZ Preset", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.cynz");
        activeFileChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc)
            {
                auto f = fc.getResult();
                if (f != juce::File{})
                    audioProcessor.saveUserPresetToFile(f.withFileExtension(".cynz"));
            });
    };
    addAndMakeVisible(savePresetBtn);

    loadPresetBtn.onClick = [this]()
    {
        activeFileChooser = std::make_unique<juce::FileChooser>(
            "Load CYBERNOIZZ Preset", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.cynz");
        activeFileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc)
            {
                auto f = fc.getResult();
                if (f.existsAsFile())
                {
                    audioProcessor.loadUserPresetFromFile(f);
                    refreshPresetAndIrDisplays();
                }
            });
    };
    addAndMakeVisible(loadPresetBtn);

    undoBtn.onClick = [this]() { audioProcessor.undoManager.undo(); };
    redoBtn.onClick = [this]() { audioProcessor.undoManager.redo(); };
    addAndMakeVisible(undoBtn);
    addAndMakeVisible(redoBtn);

    slotABtn.setClickingTogglesState(false);
    slotBBtn.setClickingTogglesState(false);
    slotABtn.setToggleState(true, juce::dontSendNotification);
    slotABtn.onClick = [this]()
    {
        audioProcessor.switchABState(0);
        slotABtn.setToggleState(true, juce::dontSendNotification);
        slotBBtn.setToggleState(false, juce::dontSendNotification);
        refreshPresetAndIrDisplays();
    };
    slotBBtn.onClick = [this]()
    {
        audioProcessor.switchABState(1);
        slotABtn.setToggleState(false, juce::dontSendNotification);
        slotBBtn.setToggleState(true, juce::dontSendNotification);
        refreshPresetAndIrDisplays();
    };
    copyABBtn.onClick = [this]() { audioProcessor.copyCurrentToOtherABSlot(); };
    addAndMakeVisible(slotABtn);
    addAndMakeVisible(slotBBtn);
    addAndMakeVisible(copyABBtn);

    // --- 2. Pre-Conditioning & Dynamics Module (NR, Tight Gate Button, Sub Boost) ---
    setupKnob(nrSlider,    nrLabel,    "NR [NOISE RED.]");
    setupKnob(boostSlider, boostLabel, "BOOST [SUB/KICK]");
    addAndMakeVisible(tightButton);

    nrAttach    = std::make_unique<SliderAttachment>(audioProcessor.apvts, "nr",    nrSlider);
    tightAttach = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "tight", tightButton);
    boostAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "boost", boostSlider);

    // --- 3. Modded JCM800 2203 "Elan Metalhead" Preamp + 6550 Power Amp Knobs ---
    setupKnob(gainSlider,     gainLabel,     "GAIN");
    setupKnob(bassSlider,     bassLabel,     "BASS");
    setupKnob(midSlider,      midLabel,      "MID");
    setupKnob(scoopSlider,    scoopLabel,    "IND. SCOOP");
    setupKnob(trebleSlider,   trebleLabel,   "TREBLE");
    setupKnob(presenceSlider, presenceLabel, "PRESENCE");
    setupKnob(depthSlider,    depthLabel,    "DEPTH");

    gainAttach     = std::make_unique<SliderAttachment>(audioProcessor.apvts, "gain",     gainSlider);
    bassAttach     = std::make_unique<SliderAttachment>(audioProcessor.apvts, "bass",     bassSlider);
    midAttach      = std::make_unique<SliderAttachment>(audioProcessor.apvts, "mid",      midSlider);
    scoopAttach    = std::make_unique<SliderAttachment>(audioProcessor.apvts, "midScoop", scoopSlider);
    trebleAttach   = std::make_unique<SliderAttachment>(audioProcessor.apvts, "treble",   trebleSlider);
    presenceAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "presence", presenceSlider);
    depthAttach    = std::make_unique<SliderAttachment>(audioProcessor.apvts, "depth",    depthSlider);

    // --- 4. Master I/O & "Spread" (Haas / 200% Stereo Widener) ---
    setupKnob(inputSlider,  inputLabel,  "INPUT",  " dB");
    setupKnob(spreadSlider, spreadLabel, "SPREAD [HAAS/200%]");
    setupKnob(outputSlider, outputLabel, "OUTPUT", " dB");

    inputAttach  = std::make_unique<SliderAttachment>(audioProcessor.apvts, "inputGain",  inputSlider);
    spreadAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "spread",     spreadSlider);
    outputAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "outputGain", outputSlider);

    // --- 5. Dual Cabinet IR Loading Bars (Left & Right Cabs + Mix + Vol + Pan) ---
    const auto& irNames = CybernoizzAudioProcessor::getFactoryIrNames();
    leftCabCombo.addItemList(irNames, 1);
    rightCabCombo.addItemList(irNames, 1);
    leftCabCombo.setSelectedId(1, juce::dontSendNotification);
    rightCabCombo.setSelectedId(2, juce::dontSendNotification);

    leftCabCombo.onChange = [this]()
    {
        if (leftCabCombo.getSelectedId() > 0)
        {
            audioProcessor.loadFactoryIr(0, leftCabCombo.getSelectedId() - 1);
            refreshPresetAndIrDisplays();
        }
    };
    rightCabCombo.onChange = [this]()
    {
        if (rightCabCombo.getSelectedId() > 0)
        {
            audioProcessor.loadFactoryIr(1, rightCabCombo.getSelectedId() - 1);
            refreshPresetAndIrDisplays();
        }
    };
    addAndMakeVisible(leftCabCombo);
    addAndMakeVisible(rightCabCombo);

    leftLoadIrBtn.onClick = [this]()
    {
        activeFileChooser = std::make_unique<juce::FileChooser>(
            "Load External Left Cabinet IR (.wav)", juce::File::getSpecialLocation(juce::File::userHomeDirectory), "*.wav;*.aif;*.flac");
        activeFileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc)
            {
                if (audioProcessor.loadExternalIrFromFile(0, fc.getResult()))
                    refreshPresetAndIrDisplays();
            });
    };
    rightLoadIrBtn.onClick = [this]()
    {
        activeFileChooser = std::make_unique<juce::FileChooser>(
            "Load External Right Cabinet IR (.wav)", juce::File::getSpecialLocation(juce::File::userHomeDirectory), "*.wav;*.aif;*.flac");
        activeFileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc)
            {
                if (audioProcessor.loadExternalIrFromFile(1, fc.getResult()))
                    refreshPresetAndIrDisplays();
            });
    };
    addAndMakeVisible(leftLoadIrBtn);
    addAndMakeVisible(rightLoadIrBtn);

    leftIrStatusLabel.setColour(juce::Label::textColourId, CyberColors::neonGreenBright);
    rightIrStatusLabel.setColour(juce::Label::textColourId, CyberColors::neonGreenBright);
    leftIrStatusLabel.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    rightIrStatusLabel.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    addAndMakeVisible(leftIrStatusLabel);
    addAndMakeVisible(rightIrStatusLabel);

    cabMixSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    cabMixSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 68, 16);
    addAndMakeVisible(cabMixSlider);
    cabMixLabel.setText("CAB L/R MIX", juce::dontSendNotification);
    cabMixLabel.setJustificationType(juce::Justification::centred);
    cabMixLabel.setColour(juce::Label::textColourId, CyberColors::neonGreen);
    cabMixLabel.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    addAndMakeVisible(cabMixLabel);
    cabMixAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "cabMix", cabMixSlider);

    setupKnob(cabLeftVolSlider,  cabLeftVolLabel,  "L VOL", " dB");
    setupKnob(cabLeftPanSlider,  cabLeftPanLabel,  "L PAN");
    setupKnob(cabRightVolSlider, cabRightVolLabel, "R VOL", " dB");
    setupKnob(cabRightPanSlider, cabRightPanLabel, "R PAN");

    cabLeftVolAttach  = std::make_unique<SliderAttachment>(audioProcessor.apvts, "cabLeftVol",  cabLeftVolSlider);
    cabLeftPanAttach  = std::make_unique<SliderAttachment>(audioProcessor.apvts, "cabLeftPan",  cabLeftPanSlider);
    cabRightVolAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "cabRightVol", cabRightVolSlider);
    cabRightPanAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "cabRightPan", cabRightPanSlider);

    // --- 6. Input & Output Meters ---
    addAndMakeVisible(inputMeter);
    addAndMakeVisible(outputMeter);

    refreshPresetAndIrDisplays();
    startTimerHz(30);
}

CybernoizzAudioProcessorEditor::~CybernoizzAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void CybernoizzAudioProcessorEditor::setupKnob(juce::Slider& slider, juce::Label& label,
                                               const juce::String& text, const juce::String& suffix)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 16);
    if (suffix.isNotEmpty())
        slider.setTextValueSuffix(suffix);
    addAndMakeVisible(slider);

    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, CyberColors::textSilver);
    label.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    addAndMakeVisible(label);
}

void CybernoizzAudioProcessorEditor::refreshPresetAndIrDisplays()
{
    leftIrStatusLabel.setText("ACTIVE IR L: " + audioProcessor.getLoadedIrDisplayName(0), juce::dontSendNotification);
    rightIrStatusLabel.setText("ACTIVE IR R: " + audioProcessor.getLoadedIrDisplayName(1), juce::dontSendNotification);
}

void CybernoizzAudioProcessorEditor::timerCallback()
{
    int mode = audioProcessor.getCurrentRoutingMode();
    bool inStereo  = (mode == 2);
    bool outStereo = (mode != 0);

    inputMeter.setLevels(audioProcessor.getInputMeterLevel(0),
                         audioProcessor.getInputMeterLevel(1), inStereo);
    outputMeter.setLevels(audioProcessor.getOutputMeterLevel(0),
                          audioProcessor.getOutputMeterLevel(1), outStereo);
}

//==============================================================================
void CybernoizzAudioProcessorEditor::drawPcbTracesAndChassis(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    // 1. Deep Carbon-Black & Brushed Dark Gray Metallic Background
    juce::ColourGradient bgGrad(CyberColors::chassisDark, bounds.getX(), bounds.getY(),
                                CyberColors::bgDeepBlack, bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill(bgGrad);
    g.fillRect(bounds);

    // 2. Intricate 45-Degree Procedural PCB Copper/Neon-Green Traces & Solder Vias
    const float w = bounds.getWidth();
    const float h = bounds.getHeight();

    juce::Random rng(19950613); // Seeded with Demanufacture release date (June 13, 1995)
    for (int i = 0; i < 48; ++i)
    {
        float x0 = rng.nextFloat() * w;
        float y0 = rng.nextFloat() * h;
        float seg1 = 25.0f + rng.nextFloat() * 90.0f;
        float seg2 = 20.0f + rng.nextFloat() * 65.0f;
        float seg3 = 25.0f + rng.nextFloat() * 80.0f;
        float dirX = (rng.nextBool() ? 1.0f : -1.0f);
        float dirY = (rng.nextBool() ? 1.0f : -1.0f);

        juce::Path trace;
        trace.startNewSubPath(x0, y0);
        float x1 = x0 + seg1 * dirX;
        float y1 = y0;
        trace.lineTo(x1, y1);
        float x2 = x1 + seg2 * dirX;
        float y2 = y1 + seg2 * dirY; // 45-degree PCB bend
        trace.lineTo(x2, y2);
        float x3 = x2;
        float y3 = y2 + seg3 * dirY;
        trace.lineTo(x3, y3);

        bool brightTrace = (i % 6 == 0);
        g.setColour(brightTrace ? CyberColors::neonGreen.withAlpha(0.22f)
                                : CyberColors::pcbTraceGlow.withAlpha(0.11f));
        g.strokePath(trace, juce::PathStrokeType(brightTrace ? 1.8f : 1.1f));

        // Draw PCB gold/neon plated via holes at endpoints
        auto drawVia = [&](float vx, float vy)
        {
            g.setColour(CyberColors::neonGreen.withAlpha(brightTrace ? 0.45f : 0.22f));
            g.fillEllipse(vx - 3.2f, vy - 3.2f, 6.4f, 6.4f);
            g.setColour(CyberColors::bgDeepBlack);
            g.fillEllipse(vx - 1.4f, vy - 1.4f, 2.8f, 2.8f);
        };
        drawVia(x0, y0);
        drawVia(x3, y3);
    }

    // 3. 3D Beveled Sub-Panels for Header, Pre-Conditioning, Modded JCM800 Amp Head, Dual Cab IRs & Master
    auto drawPanel = [&](juce::Rectangle<float> r, const juce::String& titleText)
    {
        // Outer recessed shadow
        g.setColour(juce::Colours::black.withAlpha(0.75f));
        g.fillRoundedRectangle(r.translated(2.0f, 3.0f), 7.0f);

        // Metallic module body gradient
        juce::ColourGradient pGrad(CyberColors::moduleSurface.brighter(0.08f), r.getX(), r.getY(),
                                   CyberColors::bgDeepBlack.brighter(0.03f),   r.getX(), r.getBottom(), false);
        g.setGradientFill(pGrad);
        g.fillRoundedRectangle(r, 7.0f);

        // 3D chamfer border + neon PCB trace accent
        g.setColour(CyberColors::metalBevelLight);
        g.drawRoundedRectangle(r, 7.0f, 1.5f);
        g.setColour(CyberColors::neonGreen.withAlpha(0.42f));
        g.drawRoundedRectangle(r.reduced(2.0f), 5.5f, 1.0f);

        // Corner hex bolts
        for (auto pt : { r.getTopLeft().translated(9, 9), r.getTopRight().translated(-9, 9),
                         r.getBottomLeft().translated(9, -9), r.getBottomRight().translated(-9, -9) })
        {
            g.setColour(CyberColors::metalBevelDark);
            g.fillEllipse(pt.x - 3.5f, pt.y - 3.5f, 7.0f, 7.0f);
            g.setColour(CyberColors::neonGreen.withAlpha(0.5f));
            g.drawEllipse(pt.x - 3.5f, pt.y - 3.5f, 7.0f, 7.0f, 0.9f);
        }

        if (titleText.isNotEmpty())
        {
            auto headerRect = r.removeFromTop(24.0f).reduced(14.0f, 2.0f);
            g.setColour(CyberColors::neonGreen);
            g.setFont(juce::FontOptions(12.5f, juce::Font::bold));
            g.drawText(titleText, headerRect, juce::Justification::centredLeft, false);

            // Decorative PCB bus line next to header title
            float lineStart = headerRect.getX() + 195.0f;
            if (lineStart < headerRect.getRight() - 10.0f)
            {
                g.setColour(CyberColors::neonGreen.withAlpha(0.35f));
                g.drawLine(lineStart, headerRect.getCentreY(), headerRect.getRight(), headerRect.getCentreY(), 1.2f);
            }
        }
    };

    float pad = w * 0.012f;
    float meterW = w * 0.062f;
    float headerH = h * 0.105f;
    float mainW = w - (meterW * 2.0f) - (pad * 4.0f);
    float mainX = pad * 2.0f + meterW;

    // Header Bar Panel
    drawPanel(juce::Rectangle<float>(pad, pad, w - pad * 2.0f, headerH), "");

    // Brand Title inside Header
    g.setColour(CyberColors::neonGreen);
    g.setFont(juce::FontOptions(25.0f, juce::Font::bold));
    g.drawText("CYBERNOIZZ", (int)(pad + 16), (int)(pad + 6), 220, (int)(headerH - 22), juce::Justification::centredLeft);
    g.setColour(CyberColors::textSilver);
    g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
    g.drawText("MODDED JCM800 METALHEAD // DEMANUFACTURE ENGINE", (int)(pad + 18), (int)(pad + headerH - 22), 290, 16, juce::Justification::centredLeft);

    float row1Y = pad * 2.0f + headerH;
    float row1H = h * 0.255f;
    float preW  = mainW * 0.56f;
    float mstrW = mainW - preW - pad;

    drawPanel(juce::Rectangle<float>(mainX, row1Y, preW, row1H),  "PRE-CONDITIONING // GATE & SUB-SYNTH");
    drawPanel(juce::Rectangle<float>(mainX + preW + pad, row1Y, mstrW, row1H), "SPREAD [HAAS / 200%] & MASTER I/O");

    float row2Y = row1Y + row1H + pad;
    float row2H = h * 0.305f;
    drawPanel(juce::Rectangle<float>(mainX, row2Y, mainW, row2H), "ELAN METALHEAD 2203 PREAMP + 6550 POWER AMP");

    float row3Y = row2Y + row2H + pad;
    float row3H = h - row3Y - pad;
    float cabPanelW = (mainW - pad * 2.0f) * 0.42f;
    float mixPanelW = mainW - (cabPanelW * 2.0f) - (pad * 2.0f);

    drawPanel(juce::Rectangle<float>(mainX, row3Y, cabPanelW, row3H), "LEFT CABINET IR LOADER");
    drawPanel(juce::Rectangle<float>(mainX + cabPanelW + pad, row3Y, mixPanelW, row3H), "CAB BLEND");
    drawPanel(juce::Rectangle<float>(mainX + cabPanelW + mixPanelW + pad * 2.0f, row3Y, cabPanelW, row3H), "RIGHT CABINET IR LOADER");
}

void CybernoizzAudioProcessorEditor::paint(juce::Graphics& g)
{
    drawPcbTracesAndChassis(g, getLocalBounds().toFloat());
}

void CybernoizzAudioProcessorEditor::resized()
{
    auto b = getLocalBounds().toFloat();
    const float w = b.getWidth();
    const float h = b.getHeight();
    const float pad = w * 0.012f;
    const float meterW = w * 0.062f;
    const float headerH = h * 0.105f;

    // --- 1. Top Toolbar Layout ---
    float topY = pad + (headerH - 30.0f) * 0.5f;
    float cursorX = pad + 265.0f;

    bypassButton.setBounds((int)cursorX, (int)topY, 76, 30);
    cursorX += 82.0f;

    routingBox.setBounds((int)cursorX, (int)topY, 132, 30);
    cursorX += 138.0f;

    presetBox.setBounds((int)cursorX, (int)topY, 235, 30);
    cursorX += 241.0f;

    savePresetBtn.setBounds((int)cursorX, (int)topY, 50, 30);
    cursorX += 54.0f;
    loadPresetBtn.setBounds((int)cursorX, (int)topY, 50, 30);
    cursorX += 60.0f;

    undoBtn.setBounds((int)cursorX, (int)topY, 50, 30);
    cursorX += 54.0f;
    redoBtn.setBounds((int)cursorX, (int)topY, 50, 30);
    cursorX += 60.0f;

    slotABtn.setBounds((int)cursorX, (int)topY, 32, 30);
    cursorX += 35.0f;
    slotBBtn.setBounds((int)cursorX, (int)topY, 32, 30);
    cursorX += 35.0f;
    copyABBtn.setBounds((int)cursorX, (int)topY, 48, 30);

    // --- 2. Left & Right Input/Output Meters ---
    float bodyY = pad * 2.0f + headerH;
    float bodyH = h - bodyY - pad;
    inputMeter.setBounds((int)pad, (int)bodyY, (int)meterW, (int)bodyH);
    outputMeter.setBounds((int)(w - pad - meterW), (int)bodyY, (int)meterW, (int)bodyH);

    // --- 3. Row 1: Pre-Conditioning (NR, Tight, Boost) + Spread & Master I/O ---
    float mainX = pad * 2.0f + meterW;
    float mainW = w - (meterW * 2.0f) - (pad * 4.0f);
    float row1Y = bodyY;
    float row1H = h * 0.255f;
    float preW  = mainW * 0.56f;
    float mstrW = mainW - preW - pad;

    auto layoutKnobWithLabel = [](juce::Slider& s, juce::Label& l, juce::Rectangle<float> cell)
    {
        auto labelArea = cell.removeFromTop(18.0f);
        l.setBounds(labelArea.toNearestInt());
        s.setBounds(cell.reduced(4.0f, 2.0f).toNearestInt());
    };

    juce::Rectangle<float> preContent(mainX + 12.0f, row1Y + 26.0f, preW - 24.0f, row1H - 32.0f);
    float preColW = preContent.getWidth() / 3.0f;
    layoutKnobWithLabel(nrSlider, nrLabel, preContent.removeFromLeft(preColW));
    auto tightCell = preContent.removeFromLeft(preColW).reduced(10.0f, 28.0f);
    tightButton.setBounds(tightCell.toNearestInt());
    layoutKnobWithLabel(boostSlider, boostLabel, preContent);

    juce::Rectangle<float> mstrContent(mainX + preW + pad + 10.0f, row1Y + 26.0f, mstrW - 20.0f, row1H - 32.0f);
    float mstrColW = mstrContent.getWidth() / 3.0f;
    layoutKnobWithLabel(inputSlider,  inputLabel,  mstrContent.removeFromLeft(mstrColW));
    layoutKnobWithLabel(spreadSlider, spreadLabel, mstrContent.removeFromLeft(mstrColW));
    layoutKnobWithLabel(outputSlider, outputLabel, mstrContent);

    // --- 4. Row 2: 7 Amp Knobs (Gain, Bass, Mid, Industrial Scoop, Treble, Presence, Depth) ---
    float row2Y = row1Y + row1H + pad;
    float row2H = h * 0.305f;
    juce::Rectangle<float> ampContent(mainX + 12.0f, row2Y + 26.0f, mainW - 24.0f, row2H - 32.0f);
    float ampColW = ampContent.getWidth() / 7.0f;

    layoutKnobWithLabel(gainSlider,     gainLabel,     ampContent.removeFromLeft(ampColW));
    layoutKnobWithLabel(bassSlider,     bassLabel,     ampContent.removeFromLeft(ampColW));
    layoutKnobWithLabel(midSlider,      midLabel,      ampContent.removeFromLeft(ampColW));
    layoutKnobWithLabel(scoopSlider,    scoopLabel,    ampContent.removeFromLeft(ampColW));
    layoutKnobWithLabel(trebleSlider,   trebleLabel,   ampContent.removeFromLeft(ampColW));
    layoutKnobWithLabel(presenceSlider, presenceLabel, ampContent.removeFromLeft(ampColW));
    layoutKnobWithLabel(depthSlider,    depthLabel,    ampContent);

    // --- 5. Row 3: Left Cab IR Loader + Mix Slider + Right Cab IR Loader ---
    float row3Y = row2Y + row2H + pad;
    float row3H = h - row3Y - pad;
    float cabPanelW = (mainW - pad * 2.0f) * 0.42f;
    float mixPanelW = mainW - (cabPanelW * 2.0f) - (pad * 2.0f);

    // Left Cab Panel
    juce::Rectangle<float> leftCabArea(mainX + 12.0f, row3Y + 26.0f, cabPanelW - 24.0f, row3H - 32.0f);
    auto leftBarRow = leftCabArea.removeFromTop(26.0f);
    leftLoadIrBtn.setBounds(leftBarRow.removeFromRight(88.0f).toNearestInt());
    leftBarRow.removeFromRight(6.0f);
    leftCabCombo.setBounds(leftBarRow.toNearestInt());
    leftIrStatusLabel.setBounds(leftCabArea.removeFromTop(18.0f).toNearestInt());

    float lKnobW = leftCabArea.getWidth() * 0.5f;
    layoutKnobWithLabel(cabLeftVolSlider, cabLeftVolLabel, leftCabArea.removeFromLeft(lKnobW));
    layoutKnobWithLabel(cabLeftPanSlider, cabLeftPanLabel, leftCabArea);

    // Center Cab Mix Panel
    juce::Rectangle<float> mixArea(mainX + cabPanelW + pad + 10.0f, row3Y + 34.0f, mixPanelW - 20.0f, row3H - 44.0f);
    cabMixLabel.setBounds(mixArea.removeFromTop(20.0f).toNearestInt());
    cabMixSlider.setBounds(mixArea.reduced(2.0f, 16.0f).toNearestInt());

    // Right Cab Panel
    juce::Rectangle<float> rightCabArea(mainX + cabPanelW + mixPanelW + pad * 2.0f + 12.0f, row3Y + 26.0f, cabPanelW - 24.0f, row3H - 32.0f);
    auto rightBarRow = rightCabArea.removeFromTop(26.0f);
    rightLoadIrBtn.setBounds(rightBarRow.removeFromRight(88.0f).toNearestInt());
    rightBarRow.removeFromRight(6.0f);
    rightCabCombo.setBounds(rightBarRow.toNearestInt());
    rightIrStatusLabel.setBounds(rightCabArea.removeFromTop(18.0f).toNearestInt());

    float rKnobW = rightCabArea.getWidth() * 0.5f;
    layoutKnobWithLabel(cabRightVolSlider, cabRightVolLabel, rightCabArea.removeFromLeft(rKnobW));
    layoutKnobWithLabel(cabRightPanSlider, cabRightPanLabel, rightCabArea);
}
