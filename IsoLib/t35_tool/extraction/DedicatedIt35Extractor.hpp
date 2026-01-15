#pragma once

#include "ExtractionStrategy.hpp"

namespace t35 {

/**
 * Dedicated IT35 metadata track extractor
 * Extracts from tracks with 'it35' sample entry (T35MetadataSampleEntry)
 */
class DedicatedIt35Extractor : public ExtractionStrategy {
public:
    DedicatedIt35Extractor() = default;
    virtual ~DedicatedIt35Extractor() = default;

    std::string getName() const override { return "dedicated-it35"; }

    std::string getDescription() const override {
        return "Extract from dedicated IT35 metadata track";
    }

    bool canExtract(const ExtractionConfig& config,
                   std::string& reason) const override;

    MP4Err extract(const ExtractionConfig& config) override;
};

} // namespace t35
