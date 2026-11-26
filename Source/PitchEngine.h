#pragma once
#include <vector>
#include <cmath>
#include <cstdint>
#include <atomic>
#include <algorithm>

// Mono real-time pitch corrector: YIN detection -> scale snapping -> PSOLA resynthesis.
class PitchEngine
{
public:
    struct Settings { int mask = 0xFFF; float speedMs = 20.f; float amount = 1.f; float refA = 440.f; float stabilizer = 0.3f; };

    std::atomic<float> dbgMidi{ -1.f }, dbgTarget{ -1.f };

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        minLag = std::max (8, (int) (sr / 1000.0));
        maxLag = (int) (sr / 80.0) + 2;
        W = maxLag;
        hop = std::max (64, (int) (sr * 0.006));
        D = maxLag + 64;
        Dout = maxLag + 8;
        in.assign (N, 0.f);
        out.assign (N, 0.f);
        tmp.assign ((size_t) (W + maxLag + 4), 0.f);
        cmnd.assign ((size_t) (maxLag + 4), 1.f);
        n = 0; hopCount = 0; nextGrain = 0; srcPos = 0;
        Tin = (double) maxLag / 3.0; curRatio = 1.0; corr = 0.f;
        currentNote = -1; voiced = false; unvoicedHops = 0;
    }

    int latency() const { return D + Dout; }

    float process (float x, const Settings& s)
    {
        in[(size_t) (n & M)] = x;
        if (++hopCount >= hop) { hopCount = 0; analyse (s); }
        while (nextGrain <= (double) n) spawn();
        const size_t r = (size_t) ((n - Dout) & M);
        const float y = out[r];
        out[r] = 0.f;
        ++n;
        return y;
    }

private:
    static constexpr int64_t N = 1 << 16, M = N - 1;
    double sr = 44100.0;
    int minLag = 44, maxLag = 600, W = 600, hop = 256, D = 700, Dout = 600;
    std::vector<float> in, out, tmp, cmnd;
    int64_t n = 0, srcPos = 0;
    int hopCount = 0, currentNote = -1, unvoicedHops = 0;
    double nextGrain = 0, Tin = 200, curRatio = 1.0;
    float corr = 0.f;
    bool voiced = false;

    bool allowed (int note, int mask) const { return (mask >> (((note % 12) + 12) % 12)) & 1; }

    int nearestAllowed (float m, int mask) const
    {
        int best = (int) std::lround (m); float bd = 1e9f;
        for (int k = (int) std::lround (m) - 7; k <= (int) std::lround (m) + 7; ++k)
            if (allowed (k, mask)) { float d = std::fabs ((float) k - m); if (d < bd) { bd = d; best = k; } }
        return best;
    }

    void analyse (const Settings& s)
    {
        const int total = W + maxLag;
        double energy = 0;
        for (int i = 0; i < total; ++i)
        {
            const float v = in[(size_t) ((n - total + 1 + i) & M)];
            tmp[(size_t) i] = v; energy += (double) v * v;
        }
        const bool loud = std::sqrt (energy / total) > 0.002; // ~ -54 dBFS gate

        // YIN cumulative mean normalised difference
        double running = 0; cmnd[0] = 1.f;
        for (int tau = 1; tau < maxLag; ++tau)
        {
            double sum = 0;
            for (int j = 0; j < W; ++j) { const float d = tmp[(size_t) j] - tmp[(size_t) (j + tau)]; sum += (double) d * d; }
            running += sum;
            cmnd[(size_t) tau] = running > 0 ? (float) (sum * tau / running) : 1.f;
        }
        int best = -1;
        if (loud)
            for (int tau = minLag; tau < maxLag - 1; ++tau)
                if (cmnd[(size_t) tau] < 0.15f)
                {
                    while (tau + 1 < maxLag - 1 && cmnd[(size_t) tau + 1] < cmnd[(size_t) tau]) ++tau;
                    best = tau; break;
                }

        const float dt = (float) hop / (float) sr;
        if (best > 0)
        {
            const float a = cmnd[(size_t) best - 1], b = cmnd[(size_t) best], c = cmnd[(size_t) best + 1];
            const float den = a - 2.f * b + c;
            const double p = best + (std::fabs (den) > 1e-9f ? 0.5f * (a - c) / den : 0.f);
            Tin = (voiced && s.speedMs >= 0.f && std::fabs (p / Tin - 1.0) < 0.2) ? 0.6 * Tin + 0.4 * p : p;
            voiced = true; unvoicedHops = 0;

            const float f0 = (float) (sr / Tin);
            const float midi = 69.f + 12.f * std::log2 (f0 / s.refA);
            const int cand = nearestAllowed (midi, s.mask);
            if (currentNote < 0 || ! allowed (currentNote, s.mask)
                || (cand != currentNote && std::fabs (midi - (float) currentNote) - std::fabs (midi - (float) cand) > 0.15f + 0.8f * s.stabilizer))
                currentNote = cand;

            const float targetCorr = ((float) currentNote - midi) * s.amount;
            const float alpha = s.speedMs < 0.5f ? 1.f : 1.f - std::exp (-dt * 1000.f / s.speedMs);
            corr += alpha * (targetCorr - corr);
            dbgMidi = midi; dbgTarget = (float) currentNote;
        }
        else
        {
            if (++unvoicedHops > 2) { voiced = false; currentNote = -1; corr *= 0.7f; dbgMidi = -1.f; dbgTarget = -1.f; }
            if (! voiced) Tin = 0.9 * Tin + 0.1 * ((double) maxLag / 3.0);
        }
        curRatio = std::pow (2.0, (double) corr / 12.0);
    }

    void spawn()
    {
        const int T = std::clamp ((int) std::lround (Tin), minLag, maxLag);
        const double ratio = std::clamp (curRatio, 0.5, 2.0);
        const int64_t target = n - D;
        srcPos += T;
        while (target - srcPos > T) srcPos += T;   // source fell behind: skip a period
        while (srcPos > target) srcPos -= T;       // source ran ahead: repeat a period

        const int len = 2 * T;
        const int64_t start = (int64_t) std::llround (nextGrain) - T;
        const float g = (float) (1.0 / ratio);
        for (int i = 0; i < len; ++i)
        {
            const int64_t idx = start + i;
            if (idx < n - Dout) continue;
            const float w = 0.5f - 0.5f * std::cos (6.2831853f * ((float) i + 0.5f) / (float) len);
            out[(size_t) (idx & M)] += w * g * in[(size_t) ((srcPos - T + i) & M)];
        }
        nextGrain += (double) T / ratio;
    }
};
