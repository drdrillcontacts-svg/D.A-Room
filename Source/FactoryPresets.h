#pragma once

namespace DARoomPresets
{
struct FactoryPreset
{
    const char* name;
    float mix;
    float predelay;
    float decay;
    float size;
    int mode;
    float diffusion;
    float modulation;
    float damping;
    float width;
    float lowCut;
    float highCut;
};

inline constexpr const char* modeDisplayNames[5] = { "ROOM", "HALL", "PLATE", "CHAMBER", "AMBIENCE" };
inline constexpr const char* modeJsonNames[5]    = { "Room", "Hall", "Plate", "Chamber", "Ambience" };
inline constexpr const char* parameterIDs[10]    = {
    "mix", "predelay", "decay", "size", "diffusion",
    "modulation", "damping", "width", "lowcut", "highcut"
};
inline constexpr const char* allParameterIDs[11] = {
    "mix", "predelay", "decay", "size", "mode", "diffusion",
    "modulation", "damping", "width", "lowcut", "highcut"
};

inline constexpr FactoryPreset factoryPresets[] = {
    // 0: Room (10)
    { "Studio Tight",      12.0f,  5.0f,  0.45f,  28.0f, 0, 65.0f,  8.0f, 62.0f,  55.0f, 120.0f, 15000.0f },
    { "Vocal Booth",       14.0f,  8.0f,  0.55f,  32.0f, 0, 70.0f, 12.0f, 58.0f,  65.0f, 150.0f, 14500.0f },
    { "Small Studio",      18.0f, 10.0f,  0.70f,  38.0f, 0, 72.0f, 15.0f, 55.0f,  72.0f, 100.0f, 16000.0f },
    { "Wood Room",         20.0f, 12.0f,  0.90f,  42.0f, 0, 78.0f, 18.0f, 60.0f,  68.0f,  90.0f, 14000.0f },
    { "Drum Room",         22.0f,  4.0f,  0.85f,  52.0f, 0, 82.0f, 22.0f, 48.0f,  80.0f,  70.0f, 17000.0f },
    { "Bright Room",       18.0f,  7.0f,  1.10f,  48.0f, 0, 75.0f, 26.0f, 32.0f,  78.0f, 130.0f, 18000.0f },
    { "Living Room",       20.0f, 14.0f,  1.20f,  50.0f, 0, 80.0f, 10.0f, 55.0f,  72.0f, 100.0f, 13500.0f },
    { "Tracking Room",     24.0f, 16.0f,  1.45f,  58.0f, 0, 84.0f, 18.0f, 50.0f,  76.0f,  80.0f, 16000.0f },
    { "Wide Room",         25.0f, 12.0f,  1.35f,  62.0f, 0, 88.0f, 30.0f, 45.0f, 100.0f,  90.0f, 16500.0f },
    { "Vintage Room",      22.0f, 18.0f,  1.60f,  55.0f, 0, 76.0f,  6.0f, 72.0f,  65.0f, 100.0f,  9500.0f },

    // 1: Hall (10)
    { "Small Hall",        20.0f, 18.0f,  1.80f,  55.0f, 1, 72.0f, 12.0f, 48.0f,  72.0f, 100.0f, 15000.0f },
    { "Concert Hall",      25.0f, 24.0f,  3.20f,  75.0f, 1, 84.0f, 18.0f, 42.0f,  88.0f,  80.0f, 15500.0f },
    { "Dark Hall",         22.0f, 20.0f,  4.20f,  78.0f, 1, 82.0f, 12.0f, 72.0f,  85.0f, 120.0f,  8000.0f },
    { "Bright Hall",       24.0f, 18.0f,  2.80f,  68.0f, 1, 80.0f, 22.0f, 30.0f,  90.0f, 100.0f, 18000.0f },
    { "Cathedral",         24.0f, 35.0f,  7.50f,  92.0f, 1, 90.0f, 16.0f, 68.0f,  92.0f,  90.0f, 10000.0f },
    { "Arena",             28.0f, 32.0f,  5.80f,  95.0f, 1, 86.0f, 28.0f, 48.0f, 100.0f, 100.0f, 14000.0f },
    { "Orchestral Depth",  22.0f, 26.0f,  4.60f,  84.0f, 1, 88.0f, 20.0f, 52.0f,  92.0f,  70.0f, 14500.0f },
    { "Piano Hall",        18.0f, 22.0f,  3.10f,  70.0f, 1, 82.0f, 10.0f, 38.0f,  82.0f,  80.0f, 16000.0f },
    { "Grand Hall",        27.0f, 28.0f,  5.20f,  88.0f, 1, 86.0f, 24.0f, 46.0f,  96.0f,  90.0f, 15000.0f },
    { "Soft Hall",         20.0f, 25.0f,  3.80f,  76.0f, 1, 90.0f,  8.0f, 70.0f,  80.0f, 110.0f, 10500.0f },

    // 2: Plate (10)
    { "Vocal Plate",       22.0f, 18.0f,  1.80f,  58.0f, 2, 78.0f, 25.0f, 32.0f,  82.0f, 120.0f, 16000.0f },
    { "Vintage Plate",     25.0f, 12.0f,  2.20f,  62.0f, 2, 70.0f, 10.0f, 65.0f,  75.0f, 100.0f, 11000.0f },
    { "Bright Plate",      22.0f,  8.0f,  2.60f,  65.0f, 2, 76.0f, 30.0f, 25.0f,  82.0f, 150.0f, 18000.0f },
    { "Drum Plate",        18.0f,  4.0f,  1.35f,  50.0f, 2, 80.0f, 18.0f, 40.0f,  78.0f,  70.0f, 17000.0f },
    { "Snare Bloom",       20.0f,  6.0f,  1.70f,  54.0f, 2, 82.0f, 24.0f, 38.0f,  84.0f,  80.0f, 16000.0f },
    { "Smooth Plate",      24.0f, 16.0f,  2.40f,  60.0f, 2, 88.0f, 12.0f, 58.0f,  86.0f, 130.0f, 12500.0f },
    { "Guitar Plate",      18.0f, 10.0f,  1.60f,  48.0f, 2, 75.0f, 20.0f, 45.0f,  74.0f,  90.0f, 14500.0f },
    { "Pop Plate",         20.0f, 14.0f,  2.10f,  57.0f, 2, 80.0f, 28.0f, 30.0f,  82.0f, 120.0f, 17000.0f },
    { "Dark Plate",        24.0f, 20.0f,  2.80f,  68.0f, 2, 82.0f, 10.0f, 75.0f,  88.0f, 120.0f,  8500.0f },
    { "Classic Plate",     23.0f, 11.0f,  2.00f,  60.0f, 2, 72.0f, 16.0f, 52.0f,  78.0f, 100.0f, 14000.0f },

    // 3: Chamber (10)
    { "Deep Chamber",      26.0f, 25.0f,  4.50f,  78.0f, 3, 82.0f, 18.0f, 50.0f,  78.0f, 100.0f, 14000.0f },
    { "Stone Chamber",     24.0f, 20.0f,  3.60f,  72.0f, 3, 78.0f, 14.0f, 60.0f,  80.0f, 120.0f, 11500.0f },
    { "Warm Chamber",      28.0f, 20.0f,  2.40f,  68.0f, 3, 80.0f, 12.0f, 68.0f,  76.0f, 100.0f, 12000.0f },
    { "Dark Chamber",      20.0f, 16.0f,  3.20f,  64.0f, 3, 84.0f,  8.0f, 78.0f,  82.0f, 140.0f,  9000.0f },
    { "Gold Chamber",      26.0f, 14.0f,  2.80f,  70.0f, 3, 86.0f, 22.0f, 42.0f,  88.0f,  90.0f, 15000.0f },
    { "Vocal Chamber",     24.0f, 22.0f,  2.70f,  66.0f, 3, 80.0f, 20.0f, 48.0f,  82.0f, 130.0f, 15000.0f },
    { "Cinematic Chamber", 25.0f, 28.0f,  5.60f,  88.0f, 3, 88.0f, 26.0f, 50.0f,  96.0f,  80.0f, 13500.0f },
    { "Tight Chamber",     16.0f, 10.0f,  1.50f,  45.0f, 3, 74.0f, 12.0f, 58.0f,  68.0f, 100.0f, 14000.0f },
    { "Velvet Chamber",    24.0f, 18.0f,  3.40f,  74.0f, 3, 90.0f, 14.0f, 72.0f,  86.0f, 120.0f, 11000.0f },
    { "Wide Chamber",      28.0f, 24.0f,  4.20f,  82.0f, 3, 88.0f, 32.0f, 45.0f, 100.0f, 100.0f, 14500.0f },

    // 4: Ambience (10)
    { "Ambient Cloud",     30.0f, 35.0f,  8.50f,  95.0f, 4, 88.0f, 22.0f, 65.0f,  96.0f, 100.0f, 12000.0f },
    { "Dream Space",       32.0f, 30.0f,  7.20f,  90.0f, 4, 92.0f, 28.0f, 60.0f, 100.0f, 100.0f, 14000.0f },
    { "Cosmic Drift",      28.0f, 42.0f, 10.00f, 100.0f, 4, 92.0f, 36.0f, 52.0f, 100.0f,  80.0f, 13000.0f },
    { "Frozen Air",        22.0f, 28.0f,  5.80f,  86.0f, 4, 94.0f, 20.0f, 72.0f,  90.0f, 180.0f, 10500.0f },
    { "Soft Focus",        18.0f, 24.0f,  4.20f,  78.0f, 4, 90.0f, 10.0f, 76.0f,  86.0f, 130.0f, 10000.0f },
    { "Wide Synth",        30.0f, 18.0f,  5.20f,  88.0f, 4, 88.0f, 32.0f, 48.0f, 100.0f, 100.0f, 15000.0f },
    { "Endless Horizon",   30.0f, 50.0f, 12.00f, 100.0f, 4, 94.0f, 26.0f, 68.0f, 100.0f,  70.0f, 11500.0f },
    { "Airy Vocal",        24.0f, 28.0f,  5.00f,  84.0f, 4, 92.0f, 24.0f, 42.0f,  90.0f, 150.0f, 16000.0f },
    { "Shimmering Tail",   26.0f, 22.0f,  6.40f,  92.0f, 4, 90.0f, 38.0f, 28.0f,  96.0f, 120.0f, 18000.0f },
    { "Afterglow",         25.0f, 32.0f,  7.80f,  94.0f, 4, 96.0f, 18.0f, 62.0f,  98.0f,  90.0f, 12500.0f }
};

inline constexpr int numFactoryPresets = static_cast<int> (sizeof (factoryPresets) / sizeof (factoryPresets[0]));
inline constexpr int defaultPresetIndex = 30; // Deep Chamber
} // namespace DARoomPresets
