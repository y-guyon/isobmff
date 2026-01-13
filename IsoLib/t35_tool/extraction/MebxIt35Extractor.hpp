#pragma once

#include "ExtractionStrategy.hpp"

namespace t35 {

/**
 * MEBX track with it35 namespace extractor
 * This is the current/default extractor
 */
class MebxIt35Extractor : public ExtractionStrategy {
public:
    MebxIt35Extractor() = default;
    virtual ~MebxIt35Extractor() = default;

    std::string getName() const override { return "mebx-it35"; }

    std::string getDescription() const override {
        return "Extract from MEBX metadata track with it35 namespace";
    }

    bool canExtract(const ExtractionConfig& config,
                   std::string& reason) const override;

    MP4Err extract(const ExtractionConfig& config) override;
};

} // namespace t35
