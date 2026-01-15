#include "ExtractionStrategy.hpp"
#include "MebxIt35Extractor.hpp"
#include "MebxMe4cExtractor.hpp"
#include "DedicatedIt35Extractor.hpp"
#include "AutoExtractor.hpp"
#include "../common/Logger.hpp"

namespace t35 {

std::unique_ptr<ExtractionStrategy> createExtractionStrategy(const std::string& strategyName) {
    LOG_DEBUG("Creating extraction strategy: '{}'", strategyName);

    if (strategyName == "auto") {
        return std::make_unique<AutoExtractor>();
    } else if (strategyName == "mebx-it35") {
        return std::make_unique<MebxIt35Extractor>();
    } else if (strategyName == "mebx-me4c") {
        return std::make_unique<MebxMe4cExtractor>();
    } else if (strategyName == "dedicated-it35") {
        return std::make_unique<DedicatedIt35Extractor>();
    } else if (strategyName == "sample-group" ||
               strategyName == "sample-entry-box" ||
               strategyName == "sei") {
        throw T35Exception(T35Error::NotImplemented,
                          "Extraction strategy '" + strategyName + "' is not yet implemented");
    } else {
        throw T35Exception(T35Error::ExtractionFailed,
                          "Unknown extraction strategy: " + strategyName);
    }
}

} // namespace t35
