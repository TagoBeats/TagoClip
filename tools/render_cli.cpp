// Offline render for the golden comparison against the tagodsp references.
// Streams the exact ClipEngine signal path in fixed-size blocks and trims the
// constant 20 sample latency. Not shipped with the plugin; built via the
// TagoClipRender target (-DTAGOCLIP_BUILD_TOOLS=ON).
//
// Usage: TagoClipRender in.wav out.wav <fl|hard|tanh> <threshold 1-127>
//        <drive_db> <os 1|4|8> <output_db> <mono_low 0..1> <delta 0|1>
//        <mix 0..1> [block]

#include <cstdlib>
#include <iostream>

#include <juce_audio_formats/juce_audio_formats.h>

#include "../plugin/ClipEngine.h"

int main (int argc, char* argv[])
{
    if (argc < 11)
    {
        std::cerr << "usage: TagoClipRender in.wav out.wav <fl|hard|tanh> <threshold 1-127> "
                     "<drive_db> <os 1|4|8> <output_db> <mono_low 0..1> <delta 0|1> "
                     "<mix 0..1> [block]\n";
        return 1;
    }

    const juce::File inFile (argv[1]);
    const juce::File outFile (argv[2]);
    const juce::String curveName (argv[3]);

    tagoclip::Parameters params;
    params.curve = curveName == "hard" ? tagoclip::curves::Type::hard
                 : curveName == "tanh" ? tagoclip::curves::Type::tanh
                                       : tagoclip::curves::Type::fl;
    params.thresholdSteps = atoi (argv[4]);
    params.driveDb = (float) atof (argv[5]);
    params.oversample = atoi (argv[6]);
    params.outputDb = (float) atof (argv[7]);
    params.monoLow = (float) atof (argv[8]);
    params.delta = atoi (argv[9]) != 0;
    params.mix = (float) atof (argv[10]);
    const int blockSize = argc > 11 ? atoi (argv[11]) : 512;

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (inFile));
    if (reader == nullptr)
    {
        std::cerr << "cannot read " << inFile.getFullPathName() << "\n";
        return 1;
    }

    const int channels = (int) reader->numChannels;
    const int n = (int) reader->lengthInSamples;
    const double sr = reader->sampleRate;

    juce::AudioBuffer<float> input (channels, n);
    reader->read (&input, 0, n, 0, true, true);

    tagoclip::ClipEngine engine;
    engine.prepare (sr, params);
    const int latency = engine.latencySamples();

    // Match the plugin's FP environment (processBlock runs with FTZ/DAZ on),
    // so the golden render can't drift in the last bits via denormal tails.
    juce::ScopedNoDenormals noDenormals;

    // Stream input plus latency tail in fixed blocks, trim the latency after.
    const int total = n + latency;
    juce::AudioBuffer<float> out (channels, total);
    out.clear();

    juce::AudioBuffer<float> block (channels, blockSize);
    for (int pos = 0; pos < total; pos += blockSize)
    {
        const int len = std::min (blockSize, total - pos);
        block.clear();
        if (pos < n)
            for (int c = 0; c < channels; ++c)
                block.copyFrom (c, 0, input, c, pos, std::min (len, n - pos));

        juce::AudioBuffer<float> view (block.getArrayOfWritePointers(), channels, 0, len);
        engine.process (view);

        for (int c = 0; c < channels; ++c)
            out.copyFrom (c, pos, block, c, 0, len);
    }

    juce::AudioBuffer<float> trimmed (channels, n);
    for (int c = 0; c < channels; ++c)
        trimmed.copyFrom (c, 0, out, c, latency, n);

    std::cout << "latency=" << latency << " samples block=" << blockSize << "\n";

    outFile.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> stream (new juce::FileOutputStream (outFile));
    auto writer = wav.createWriterFor (stream,
                                       juce::AudioFormatWriterOptions {}
                                           .withSampleRate (sr)
                                           .withNumChannels (channels)
                                           .withBitsPerSample (32));
    if (writer == nullptr || ! writer->writeFromAudioSampleBuffer (trimmed, 0, n))
    {
        std::cerr << "cannot write " << outFile.getFullPathName() << "\n";
        return 1;
    }
    writer->flush();
    return 0;
}
