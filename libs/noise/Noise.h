#pragma once

#include "FastNoiseLite.h"

namespace noise
{

enum class Algorithm
{
    Perlin,
    Simplex,
    FBm,
    RidgedMultifractal,
    Billowy,
    RockyPlateaus,
    CrackedGround,
    Craters,
    ErodedHills,
    Dunes
};

constexpr int AlgorithmCount = 10;

constexpr double FeatureSizePerUnitFrequency = 2.0;

struct NoiseParameters
{
    Algorithm algorithm = Algorithm::Perlin;
    unsigned int seed = 0;
    double frequency = 1.0;
    double amplitude = 1.0;
    int octaves = 4;
    double persistence = 0.5;
    double lacunarity = 2.0;
};

namespace detail
{

struct Preset
{
    const char* name;
    FastNoiseLite::NoiseType noiseType;
    FastNoiseLite::FractalType fractalType;
    FastNoiseLite::CellularReturnType cellularReturn;
    float warpAmount;
    FastNoiseLite::DomainWarpType warpType;
    FastNoiseLite::FractalType warpFractalType;
    float frequencyScale;
    float scale;
    float bias;
};

inline const Preset& getPreset(Algorithm algorithm)
{
    static const Preset presets[] = {
        {"Perlin",
         FastNoiseLite::NoiseType_Perlin, FastNoiseLite::FractalType_None,
         FastNoiseLite::CellularReturnType_CellValue,
         0.0f, FastNoiseLite::DomainWarpType_OpenSimplex2, FastNoiseLite::FractalType_None,
         0.9995f,
         1.0190f, -0.0000f},

        {"Simplex",
         FastNoiseLite::NoiseType_OpenSimplex2, FastNoiseLite::FractalType_None,
         FastNoiseLite::CellularReturnType_CellValue,
         0.0f, FastNoiseLite::DomainWarpType_OpenSimplex2, FastNoiseLite::FractalType_None,
         0.6890f,
         0.9993f, 0.0009f},

        {"fBm",
         FastNoiseLite::NoiseType_OpenSimplex2, FastNoiseLite::FractalType_FBm,
         FastNoiseLite::CellularReturnType_CellValue,
         0.0f, FastNoiseLite::DomainWarpType_OpenSimplex2, FastNoiseLite::FractalType_None,
         0.4195f,
         1.0000f, 0.0000f},

        {"Ridged Multifractal",
         FastNoiseLite::NoiseType_OpenSimplex2, FastNoiseLite::FractalType_Ridged,
         FastNoiseLite::CellularReturnType_CellValue,
         0.0f, FastNoiseLite::DomainWarpType_OpenSimplex2, FastNoiseLite::FractalType_None,
         0.2220f,
         0.9444f, -0.0561f},

        {"Billowy",
         FastNoiseLite::NoiseType_OpenSimplex2, FastNoiseLite::FractalType_PingPong,
         FastNoiseLite::CellularReturnType_CellValue,
         0.0f, FastNoiseLite::DomainWarpType_OpenSimplex2, FastNoiseLite::FractalType_None,
         0.1190f,
         0.9427f, -0.0573f},

        {"Rocky Plateaus",
         FastNoiseLite::NoiseType_Cellular, FastNoiseLite::FractalType_None,
         FastNoiseLite::CellularReturnType_CellValue,
         0.0f, FastNoiseLite::DomainWarpType_OpenSimplex2, FastNoiseLite::FractalType_None,
         1.5620f,
         0.9970f, 0.0030f},

        {"Cracked Ground",
         FastNoiseLite::NoiseType_Cellular, FastNoiseLite::FractalType_None,
         FastNoiseLite::CellularReturnType_Distance2Div,
         0.0f, FastNoiseLite::DomainWarpType_OpenSimplex2, FastNoiseLite::FractalType_None,
         0.6485f,
         1.5784f, 0.5774f},

        {"Craters",
         FastNoiseLite::NoiseType_Cellular, FastNoiseLite::FractalType_None,
         FastNoiseLite::CellularReturnType_Distance,
         0.0f, FastNoiseLite::DomainWarpType_OpenSimplex2, FastNoiseLite::FractalType_None,
         0.8285f,
         1.5374f, 0.8664f},

        {"Eroded Hills",
         FastNoiseLite::NoiseType_OpenSimplex2, FastNoiseLite::FractalType_FBm,
         FastNoiseLite::CellularReturnType_CellValue,
         3.0f, FastNoiseLite::DomainWarpType_OpenSimplex2, FastNoiseLite::FractalType_DomainWarpIndependent,
         0.2520f,
         1.0001f, -0.0008f},

        {"Dunes",
         FastNoiseLite::NoiseType_OpenSimplex2S, FastNoiseLite::FractalType_FBm,
         FastNoiseLite::CellularReturnType_CellValue,
         2.0f, FastNoiseLite::DomainWarpType_OpenSimplex2Reduced, FastNoiseLite::FractalType_DomainWarpProgressive,
         0.2070f,
         0.9966f, -0.0040f},
    };

    static_assert(sizeof(presets) / sizeof(presets[0]) == AlgorithmCount,
        "The preset table must hold one entry per Algorithm value");

    int index = static_cast<int>(algorithm);

    if (index < 0 || index >= AlgorithmCount)
    {
        index = 0;
    }

    return presets[index];
}

} // namespace detail

inline const char* getAlgorithmName(Algorithm algorithm)
{
    return detail::getPreset(algorithm).name;
}

inline bool algorithmUsesFractalParameters(Algorithm algorithm)
{
    return detail::getPreset(algorithm).fractalType != FastNoiseLite::FractalType_None;
}

class NoiseGenerator
{
private:
    FastNoiseLite _noise;
    FastNoiseLite _warp;
    NoiseParameters _params;
    float _scale;
    float _bias;
    bool _warping;

    void configure()
    {
        const detail::Preset& preset = detail::getPreset(_params.algorithm);

        float frequency = static_cast<float>(_params.frequency) * preset.frequencyScale;

        _noise.SetSeed(static_cast<int>(_params.seed));
        _noise.SetNoiseType(preset.noiseType);
        _noise.SetFractalType(preset.fractalType);
        _noise.SetFrequency(frequency);
        _noise.SetFractalOctaves(_params.octaves);
        _noise.SetFractalGain(static_cast<float>(_params.persistence));
        _noise.SetFractalLacunarity(static_cast<float>(_params.lacunarity));
        _noise.SetCellularDistanceFunction(FastNoiseLite::CellularDistanceFunction_Euclidean);
        _noise.SetCellularReturnType(preset.cellularReturn);

        _scale = preset.scale;
        _bias = preset.bias;
        _warping = preset.warpAmount > 0.0f && frequency > 0.0f;

        if (_warping)
        {
            _warp.SetSeed(static_cast<int>(_params.seed) + 1);
            _warp.SetDomainWarpType(preset.warpType);
            _warp.SetFractalType(preset.warpFractalType);
            _warp.SetFrequency(frequency * 0.5f);
            _warp.SetFractalOctaves(_params.octaves);
            _warp.SetFractalGain(static_cast<float>(_params.persistence));
            _warp.SetFractalLacunarity(static_cast<float>(_params.lacunarity));
            _warp.SetDomainWarpAmp(preset.warpAmount / frequency);
        }
    }

public:
    explicit NoiseGenerator(const NoiseParameters& params = NoiseParameters())
        : _params(params)
    {
        configure();
    }

    void setParameters(const NoiseParameters& params)
    {
        _params = params;
        configure();
    }

    const NoiseParameters& getParameters() const
    {
        return _params;
    }

    double sample(double x, double y) const
    {
        double sx = x;
        double sy = y;

        if (_warping)
        {
            _warp.DomainWarp(sx, sy);
        }

        double value = _noise.GetNoise(sx, sy) * _scale + _bias;

        return value * _params.amplitude;
    }
};

} // namespace noise
