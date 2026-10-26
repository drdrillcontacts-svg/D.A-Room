#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <iostream>

int main (int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cerr << "Usage: VerifyPluginGUI <output_png_path> [hwnd_png_path]\n";
        return 2;
    }

    const juce::ScopedJuceInitialiser_GUI guiInit;

    DARoomAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    if (editor == nullptr)
    {
        std::cerr << "FAIL: createEditor() returned nullptr\n";
        return 1;
    }

    if (editor->getWidth() != 1015 || editor->getHeight() != 388)
    {
        std::cerr << "FAIL: unexpected editor dimensions "
                  << editor->getWidth() << "x" << editor->getHeight() << "\n";
        return 1;
    }

   #if JUCE_WINDOWS
    editor->addToDesktop (0);
    editor->setTopLeftPosition (40, 40);
    editor->setVisible (true);

    if (auto* peer = editor->getPeer())
    {
        peer->performAnyPendingRepaintsNow();
        std::cout << "Active rendering engine index: " << peer->getCurrentRenderingEngine()
                  << " (" << peer->getAvailableRenderingEngines()[peer->getCurrentRenderingEngine()] << ")\n";
        if (peer->getCurrentRenderingEngine() != 0)
        {
            std::cerr << "FAIL: Windows rendering engine is not 0 (Software Renderer)\n";
            return 1;
        }
    }
   #endif

    const juce::Image snapshot = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
    if (! snapshot.isValid())
    {
        std::cerr << "FAIL: createComponentSnapshot returned invalid image\n";
        return 1;
    }

    const juce::Colour archTop = snapshot.getPixelAt (507, 88);
    const juce::Colour archGlow = snapshot.getPixelAt (452, 145);
    const juce::Colour waterRefl = snapshot.getPixelAt (480, 210);

    std::cout << "Sampled arch_top (507,88) RGB=("
              << static_cast<int> (archTop.getRed()) << ","
              << static_cast<int> (archTop.getGreen()) << ","
              << static_cast<int> (archTop.getBlue()) << ")\n";
    std::cout << "Sampled arch_glow (452,145) RGB=("
              << static_cast<int> (archGlow.getRed()) << ","
              << static_cast<int> (archGlow.getGreen()) << ","
              << static_cast<int> (archGlow.getBlue()) << ")\n";
    std::cout << "Sampled water_refl (480,210) RGB=("
              << static_cast<int> (waterRefl.getRed()) << ","
              << static_cast<int> (waterRefl.getBlue()) << ")\n";

    if (archTop.getBlue() < 150 || archTop.getGreen() < 140 || archGlow.getBlue() < 55)
    {
        std::cerr << "FAIL: Background GUI artwork did not render (pixels are dark/black)!\n";
        return 1;
    }

    const juce::File outFile (juce::String::fromUTF8 (argv[1]));
    outFile.deleteFile();
    {
        juce::FileOutputStream fos (outFile);
        if (! fos.openedOk())
        {
            std::cerr << "FAIL: Could not open output file " << outFile.getFullPathName() << "\n";
            return 1;
        }
        juce::PNGImageFormat png;
        if (! png.writeImageToStream (snapshot, fos))
        {
            std::cerr << "FAIL: Could not write PNG snapshot\n";
            return 1;
        }
    }

    editor->removeFromDesktop();
    editor.reset();
    processor.releaseResources();

    std::cout << "SUCCESS: Verified D.A Room GUI rendered cleanly ("
              << snapshot.getWidth() << "x" << snapshot.getHeight() << ") -> "
              << outFile.getFullPathName() << "\n";
    return 0;
}
