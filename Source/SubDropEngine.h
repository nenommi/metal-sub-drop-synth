#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>

// Monophonic sub-drop voice: sine oscillator with a pitch-drop envelope,
// AHDR amp envelope, drive, HPF/LPF and an output soft-clip ceiling.
// Plain C++ with no JUCE dependency so the editor can reuse pitchAt() for its display.

struct EngineParams
{
    float tuneSemis   = 0.0f;
    float fineCents   = 0.0f;
    bool  landOnNote  = false;   // false: start on note and fall. true: start above and land on note.
    float dropSemis   = 12.0f;
    float dropTimeSec = 1.5f;
    float dropCurve   = 0.3f;    // -1 slow start .. +1 fast start
    float floorHz     = 20.0f;
    float punch       = 0.0f;    // 0..1

    float attackSec   = 0.002f;
    float holdSec     = 0.3f;
    float decaySec    = 2.5f;
    float releaseSec  = 0.3f;
    bool  noteLength  = false;   // false: one-shot. true: note-off triggers release.
    float velSens     = 0.5f;

    float drive       = 0.0f;    // 0..1
    float hpfHz       = 25.0f;
    float lpfHz       = 4000.0f;
    float ceilingLin  = 1.0f;
    float gainLin     = 0.5f;
};

class SubDropEngine
{
public:
    static constexpr float kPunchSemis = 24.0f;
    static constexpr float kPunchTime  = 0.008f;

    // Oscillator frequency `t` seconds after note-on. Shared by the DSP and the UI display.
    static float pitchAt (float t, int note, const EngineParams& p)
    {
        const float pos = p.dropTimeSec > 0.0f ? std::clamp (t / p.dropTimeSec, 0.0f, 1.0f) : 1.0f;
        const float shaped = 1.0f - std::pow (1.0f - pos, std::pow (2.0f, 3.0f * p.dropCurve));

        float semis = (float) note + p.tuneSemis + p.fineCents * 0.01f;
        semis += p.landOnNote ? p.dropSemis * (1.0f - shaped) : -p.dropSemis * shaped;
        semis += p.punch * kPunchSemis * std::exp (-t / kPunchTime);

        const float hz = 440.0f * std::pow (2.0f, (semis - 69.0f) / 12.0f);
        return std::max (hz, p.floorHz);
    }

    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        reset();
    }

    void reset()
    {
        stage = Stage::Idle;
        level = 0.0f;
        phase = 0.0;
        for (auto& f : hpf) f.reset();
        lpf.reset();
        active.store (false);
    }

    void setParams (const EngineParams& newParams)
    {
        p = newParams;

        const float hpfHz = std::min (p.hpfHz, sr * 0.45f);
        const float lpfHz = std::min (p.lpfHz, sr * 0.45f);
        // 4th-order Butterworth high-pass (24 dB/oct) to strip infrasonic rumble.
        hpf[0].setCutoff (hpfHz, sr, 0.5412f);
        hpf[1].setCutoff (hpfHz, sr, 1.3066f);
        lpf.setCutoff (lpfHz, sr, 0.7071f);

        decayCoef   = timeToCoef (p.decaySec);
        releaseCoef = timeToCoef (p.releaseSec);
    }

    void noteOn (int note, float velocity)
    {
        currentNote = note;
        noteAmp = 1.0f - p.velSens * (1.0f - velocity);
        elapsed = 0.0f;
        if (stage == Stage::Idle)
            phase = 0.0; // start at a zero crossing; retriggers keep phase to avoid clicks
        stage = Stage::Attack;
        holdCounter = 0.0f;
        active.store (true);
        uiNote.store (note);
    }

    void noteOff (int note)
    {
        if (p.noteLength && note == currentNote && stage != Stage::Idle)
            stage = Stage::Release;
    }

    void allNotesOff()
    {
        if (stage != Stage::Idle)
            stage = Stage::Release;
    }

    void render (float* out, int numSamples)
    {
        const float invSr = 1.0f / sr;
        const float attackInc = 1.0f / std::max (p.attackSec * sr, 1.0f);
        const float driveGain = 1.0f + p.drive * 15.0f;
        const float driveNorm = 1.0f / std::tanh (driveGain);

        for (int i = 0; i < numSamples; ++i)
        {
            if (stage == Stage::Idle)
            {
                out[i] = 0.0f;
                continue;
            }

            switch (stage)
            {
                case Stage::Attack:
                    level += attackInc;
                    if (level >= 1.0f) { level = 1.0f; stage = Stage::Hold; }
                    break;
                case Stage::Hold:
                    holdCounter += invSr;
                    if (holdCounter >= p.holdSec) stage = Stage::Decay;
                    break;
                case Stage::Decay:   level *= decayCoef;   break;
                case Stage::Release: level *= releaseCoef; break;
                case Stage::Idle:    break;
            }

            if ((stage == Stage::Decay || stage == Stage::Release) && level < 1.0e-4f)
            {
                stage = Stage::Idle;
                level = 0.0f;
                active.store (false);
            }

            const float hz = pitchAt (elapsed, currentNote, p);
            phase += (double) (hz * invSr);
            if (phase >= 1.0) phase -= 1.0;

            float x = (float) std::sin (phase * 6.283185307179586) * level * noteAmp;

            if (p.drive > 0.0f)
                x = std::tanh (x * driveGain) * driveNorm;

            x = hpf[0].highpass (x);
            x = hpf[1].highpass (x);
            x = lpf.lowpass (x);

            x = softCeiling (x * smoothedGain(), p.ceilingLin);
            out[i] = x;

            elapsed += invSr;
        }

        uiElapsed.store (elapsed);
        uiLevel.store (level);
    }

    // Lock-free state for the editor.
    std::atomic<bool>  active { false };
    std::atomic<float> uiElapsed { 0.0f };
    std::atomic<float> uiLevel { 0.0f };
    std::atomic<int>   uiNote { 28 };

private:
    enum class Stage { Idle, Attack, Hold, Decay, Release };

    // Topology-preserving-transform state variable filter (stable under modulation).
    struct Svf
    {
        float a1 = 0, a2 = 0, a3 = 0, k = 1.414f, ic1 = 0, ic2 = 0;

        void reset() { ic1 = ic2 = 0.0f; }

        void setCutoff (float hz, float sampleRate, float q)
        {
            const float g = std::tan (3.14159265f * hz / sampleRate);
            k = 1.0f / q;
            a1 = 1.0f / (1.0f + g * (g + k));
            a2 = g * a1;
            a3 = g * a2;
        }

        void tick (float v0, float& band, float& low)
        {
            const float v3 = v0 - ic2;
            band = a1 * ic1 + a2 * v3;
            low  = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0f * band - ic1;
            ic2 = 2.0f * low - ic2;
        }

        float lowpass (float x)  { float b, l; tick (x, b, l); return l; }
        float highpass (float x) { float b, l; tick (x, b, l); return x - k * b - l; }
    };

    float timeToCoef (float seconds) const
    {
        // Exponential fall reaching -80 dB after `seconds`.
        return std::exp (std::log (1.0e-4f) / std::max (seconds * sr, 1.0f));
    }

    float smoothedGain()
    {
        gainState += (p.gainLin - gainState) * 0.002f;
        return gainState;
    }

    static float softCeiling (float x, float ceiling)
    {
        const float knee = 0.6f * ceiling;
        const float ax = std::abs (x);
        if (ax <= knee)
            return x;
        const float span = ceiling - knee;
        return std::copysign (knee + span * std::tanh ((ax - knee) / span), x);
    }

    EngineParams p;
    float sr = 44100.0f;

    Stage stage = Stage::Idle;
    float level = 0.0f, noteAmp = 1.0f, holdCounter = 0.0f, elapsed = 0.0f;
    float decayCoef = 0.999f, releaseCoef = 0.999f, gainState = 0.5f;
    double phase = 0.0;
    int currentNote = 28;

    Svf hpf[2], lpf;
};
