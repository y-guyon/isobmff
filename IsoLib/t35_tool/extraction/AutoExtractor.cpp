#include "AutoExtractor.hpp"
#include "MebxIt35Extractor.hpp"
#include "MebxMe4cExtractor.hpp"
#include "DedicatedIt35Extractor.hpp"
#include "../common/Logger.hpp"

namespace t35 {

bool AutoExtractor::canExtract(const ExtractionConfig& config,
                               std::string& reason) const {
    // Try extractors in priority order
    std::vector<std::unique_ptr<ExtractionStrategy>> extractors;
    extractors.push_back(std::make_unique<DedicatedIt35Extractor>());
    extractors.push_back(std::make_unique<MebxIt35Extractor>());
    extractors.push_back(std::make_unique<MebxMe4cExtractor>());

    for (const auto& extractor : extractors) {
        std::string extractorReason;
        if (extractor->canExtract(config, extractorReason)) {
            LOG_INFO("Auto-detected strategy: {}", extractor->getName());
            return true;
        }
    }

    reason = "No compatible metadata tracks found (tried: dedicated-it35, mebx-it35, mebx-me4c)";
    return false;
}

MP4Err AutoExtractor::extract(const ExtractionConfig& config) {
    LOG_INFO("Auto-detecting extraction strategy");

    // Try extractors in priority order
    std::vector<std::unique_ptr<ExtractionStrategy>> extractors;
    extractors.push_back(std::make_unique<DedicatedIt35Extractor>());
    extractors.push_back(std::make_unique<MebxIt35Extractor>());
    extractors.push_back(std::make_unique<MebxMe4cExtractor>());

    for (auto& extractor : extractors) {
        std::string reason;
        if (extractor->canExtract(config, reason)) {
            LOG_INFO("Using auto-detected strategy: {}", extractor->getName());
            return extractor->extract(config);
        } else {
            LOG_DEBUG("Strategy '{}' cannot extract: {}", extractor->getName(), reason);
        }
    }

    LOG_ERROR("No compatible extraction strategy found");
    throw T35Exception(T35Error::ExtractionFailed,
                       "No compatible metadata tracks found (tried: dedicated-it35, mebx-it35, mebx-me4c)");
}

} // namespace t35
