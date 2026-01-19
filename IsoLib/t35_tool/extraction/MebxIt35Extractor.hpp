#pragma once

#include "ExtractionStrategy.hpp"

// Forward declarations from libisomediafile
extern "C" {
struct MP4TrackReaderRecord;
typedef struct MP4TrackReaderRecord* MP4TrackReader;
struct MP4TrackRecord;
typedef struct MP4TrackRecord* MP4Track;
}

namespace t35 {

/**
 * MEBX track with it35 namespace extractor
 * This is the current/default extractor
 */
class MebxIt35Extractor : public ExtractionStrategy {
public:
    MebxIt35Extractor() = default;
    virtual ~MebxIt35Extractor();

    std::string getName() const override { return "mebx-it35"; }

    std::string getDescription() const override {
        return "Extract from MEBX metadata track with it35 namespace";
    }

    bool canExtract(const ExtractionConfig& config,
                   std::string& reason) override;

    MP4Err extract(const ExtractionConfig& config, MetadataMap* outItems = nullptr) override;

private:
    // Cache the reader and track found in canExtract() for use in extract()
    MP4TrackReader m_cachedReader = nullptr;
    MP4Track m_cachedTrack = nullptr;

    void clearCache();
};

} // namespace t35
