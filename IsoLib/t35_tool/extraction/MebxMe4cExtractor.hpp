#pragma once

#include "ExtractionStrategy.hpp"

namespace t35 {

/**
 * MEBX track with me4c namespace extractor
 * Extracts from mebx tracks using me4c namespace with 'it35' key_value
 */
class MebxMe4cExtractor : public ExtractionStrategy {
public:
    MebxMe4cExtractor() = default;
    virtual ~MebxMe4cExtractor() = default;

    std::string getName() const override { return "mebx-me4c"; }

    std::string getDescription() const override {
        return "Extract from MEBX metadata track with me4c namespace";
    }

    bool canExtract(const ExtractionConfig& config,
                   std::string& reason) const override;

    MP4Err extract(const ExtractionConfig& config) override;
};

} // namespace t35
