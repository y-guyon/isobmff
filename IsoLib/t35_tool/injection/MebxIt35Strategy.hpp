#pragma once

#include "InjectionStrategy.hpp"

namespace t35 {

/**
 * MEBX track with it35 namespace injection strategy
 * This is the current/default implementation
 */
class MebxIt35Strategy : public InjectionStrategy {
public:
    MebxIt35Strategy() = default;
    virtual ~MebxIt35Strategy() = default;

    std::string getName() const override { return "mebx-it35"; }

    std::string getDescription() const override {
        return "MEBX metadata track with it35 namespace";
    }

    bool isApplicable(const MetadataMap& items,
                     const InjectionConfig& config,
                     std::string& reason) const override;

    MP4Err inject(const InjectionConfig& config,
                 const MetadataMap& items,
                 const T35Prefix& prefix) override;
};

} // namespace t35
