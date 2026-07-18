#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// PlatformModule.h  —  Pulverine Audio Platform Interface
// Identica a Hypnos e PaperCUT. Zero dipendenze JUCE.
// ─────────────────────────────────────────────────────────────────────────────
#include <cstdint>
#include <vector>

namespace Platform {

struct ModuleDescriptor {
    const char* id;
    const char* name;
    const char* version;
    const char* category;
    const char* company;
};

class IModule {
public:
    virtual ~IModule() = default;

    virtual ModuleDescriptor getDescriptor() const = 0;

    virtual void initialize() = 0;
    virtual void prepare(double sampleRate, int maxBlockSize) = 0;
    virtual void reset() = 0;
    virtual void process(const float* const* inputs,
                         float* const*       outputs,
                         int numInputChannels,
                         int numOutputChannels,
                         int numSamples) = 0;
    virtual void suspend() = 0;
    virtual void resume() = 0;
    virtual void shutdown() = 0;

    virtual void saveState(std::vector<uint8_t>& dest) = 0;
    virtual void loadState(const std::vector<uint8_t>& source) = 0;

    virtual void  setParameter(int paramIndex, float normalizedValue) = 0;
    virtual float getParameter(int paramIndex) const = 0;
};

} // namespace Platform
