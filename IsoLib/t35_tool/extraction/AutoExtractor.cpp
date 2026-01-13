#include "AutoExtractor.hpp"
#include "../common/Logger.hpp"

namespace t35 {

bool AutoExtractor::canExtract(const ExtractionConfig& config,
                               std::string& reason) const {
    reason = "AutoExtractor not yet implemented";
    return false;
}

MP4Err AutoExtractor::extract(const ExtractionConfig& config) {
    LOG_ERROR("AutoExtractor::extract() - NOT IMPLEMENTED YET");
    throw T35Exception(T35Error::NotImplemented,
                       "AutoExtractor is not yet implemented");
}

} // namespace t35
