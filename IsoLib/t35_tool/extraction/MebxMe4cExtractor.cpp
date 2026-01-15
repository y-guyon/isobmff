#include "MebxMe4cExtractor.hpp"
#include "../common/Logger.hpp"
#include "../common/T35Prefix.hpp"

extern "C" {
    #include "MP4Movies.h"
    #include "MP4Atoms.h"
}

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace t35 {

// Helper: Convert u32 4CC to string representation
static std::string fourCCToString(u32 fourcc) {
    char buf[5] = {0};
    buf[0] = (fourcc >> 24) & 0xFF;
    buf[1] = (fourcc >> 16) & 0xFF;
    buf[2] = (fourcc >> 8) & 0xFF;
    buf[3] = fourcc & 0xFF;
    return std::string(buf);
}

// Helper: Find mebx track with me4c namespace and it35 key_value
static MP4Err findMebxMe4cTrackReader(MP4Movie moov,
                                       const std::string& t35PrefixStr,
                                       MP4TrackReader* outReader,
                                       MP4Track* outTrack) {
    MP4Err err = MP4NoErr;
    *outReader = nullptr;
    if (outTrack) *outTrack = nullptr;

    u32 trackCount = 0;
    err = MP4GetMovieTrackCount(moov, &trackCount);
    if (err) return err;

    LOG_DEBUG("Searching for mebx me4c track in {} tracks", trackCount);

    // Search for mebx track
    for (u32 i = 1; i <= trackCount; ++i) {
        MP4Track trak = nullptr;
        MP4Media media = nullptr;
        u32 handlerType = 0;

        err = MP4GetMovieIndTrack(moov, i, &trak);
        if (err) continue;

        err = MP4GetTrackMedia(trak, &media);
        if (err) continue;

        err = MP4GetMediaHandlerDescription(media, &handlerType, nullptr);
        if (err) continue;

        // Only metadata tracks
        if (handlerType != MP4MetaHandlerType) continue;

        u32 trackID = 0;
        MP4GetTrackID(trak, &trackID);
        LOG_DEBUG("Found metadata track with ID {}", trackID);

        // Create track reader
        MP4TrackReader reader = nullptr;
        err = MP4CreateTrackReader(trak, &reader);
        if (err) continue;

        // Get sample description (we'll reuse this for both type checking and metadata config)
        MP4Handle sampleEntryH = nullptr;
        err = MP4NewHandle(0, &sampleEntryH);
        if (err) {
            MP4DisposeTrackReader(reader);
            continue;
        }

        err = MP4TrackReaderGetCurrentSampleDescription(reader, sampleEntryH);
        if (err) {
            MP4DisposeHandle(sampleEntryH);
            MP4DisposeTrackReader(reader);
            continue;
        }

        // Check if it's 'mebx'
        u32 type = 0;
        err = ISOGetSampleDescriptionType(sampleEntryH, &type);

        if (err || type != MP4BoxedMetadataSampleEntryType) {
            MP4DisposeHandle(sampleEntryH);
            MP4DisposeTrackReader(reader);
            continue;
        }

        LOG_INFO("Found mebx track with ID {}", trackID);

        // Check for 'rndr' track reference
        MP4Track videoTrack = nullptr;
        err = MP4GetTrackReference(trak, MP4_FOUR_CHAR_CODE('r', 'n', 'd', 'r'), 1, &videoTrack);
        if (err) {
            LOG_WARN("Mebx track ID {} has no 'rndr' track reference, skipping", trackID);
            MP4DisposeHandle(sampleEntryH);
            MP4DisposeTrackReader(reader);
            continue;
        }

        u32 videoTrackID = 0;
        MP4GetTrackID(videoTrack, &videoTrackID);
        LOG_DEBUG("Mebx track references video track ID {}", videoTrackID);

        // Try to select the me4c namespace key with 'it35' key_value
        u32 key_namespace = MP4_FOUR_CHAR_CODE('m', 'e', '4', 'c');
        MP4Handle key_value = nullptr;
        err = MP4NewHandle(4, &key_value);
        if (err) {
            MP4DisposeHandle(sampleEntryH);
            MP4DisposeTrackReader(reader);
            return err;
        }

        // Key_value = 'it35' (4CC)
        u32 it35_fourcc = MP4_FOUR_CHAR_CODE('i', 't', '3', '5');
        char* keyPtr = (char*)*key_value;
        keyPtr[0] = (it35_fourcc >> 24) & 0xFF;
        keyPtr[1] = (it35_fourcc >> 16) & 0xFF;
        keyPtr[2] = (it35_fourcc >> 8) & 0xFF;
        keyPtr[3] = it35_fourcc & 0xFF;

        LOG_DEBUG("Searching for key_namespace='me4c', key_value='it35' (4CC)");

        u32 local_key_id = 0;
        err = MP4SelectMebxTrackReaderKey(reader, key_namespace, key_value, &local_key_id);
        MP4DisposeHandle(key_value);

        if (err) {
            LOG_DEBUG("MP4SelectMebxTrackReaderKey failed for track {} (err={})", trackID, err);
            MP4DisposeHandle(sampleEntryH);
            MP4DisposeTrackReader(reader);
            continue;
        }

        LOG_INFO("Selected mebx me4c track ID {} with local_key_id = '{}' (0x{:08X})",
                 trackID, fourCCToString(local_key_id), local_key_id);

        // Verify the T.35 prefix stored in setupInfo parameter
        // Reuse the sampleEntryH we already got above

        // Get metadata configuration for the selected key (idx=0 for first key)
        u32 read_local_key_id = 0;
        u32 read_key_namespace = 0;
        MP4Handle read_key_value = nullptr;
        MP4Handle setupInfoH = nullptr;
        char* locale_string = nullptr;

        err = MP4NewHandle(0, &read_key_value);
        if (err) {
            LOG_ERROR("Failed to create read_key_value handle (err={})", err);
            MP4DisposeHandle(sampleEntryH);
            MP4DisposeTrackReader(reader);
            continue;
        }

        err = MP4NewHandle(0, &setupInfoH);
        if (err) {
            LOG_ERROR("Failed to create setupInfo handle (err={})", err);
            MP4DisposeHandle(read_key_value);
            MP4DisposeHandle(sampleEntryH);
            MP4DisposeTrackReader(reader);
            continue;
        }

        LOG_DEBUG("Calling ISOGetMebxMetadataConfig with idx=0");
        err = ISOGetMebxMetadataConfig(sampleEntryH, 0, &read_local_key_id, &read_key_namespace,
                                       read_key_value, &locale_string, setupInfoH);

        if (err) {
            LOG_WARN("ISOGetMebxMetadataConfig failed (err={})", err);
            LOG_DEBUG("  read_local_key_id = 0x{:08X}", read_local_key_id);
            LOG_DEBUG("  read_key_namespace = 0x{:08X} ('{}')", read_key_namespace,
                     fourCCToString(read_key_namespace));
            MP4DisposeHandle(setupInfoH);
            MP4DisposeHandle(read_key_value);
            MP4DisposeHandle(sampleEntryH);
            // Continue anyway - rely on successful key selection
        } else {
            LOG_DEBUG("ISOGetMebxMetadataConfig succeeded");
            LOG_DEBUG("  read_local_key_id = 0x{:08X} ('{}')", read_local_key_id,
                     fourCCToString(read_local_key_id));
            LOG_DEBUG("  read_key_namespace = 0x{:08X} ('{}')", read_key_namespace,
                     fourCCToString(read_key_namespace));

            // Check setupInfo contains the T.35 prefix
            u32 setupInfoSize = 0;
            MP4GetHandleSize(setupInfoH, &setupInfoSize);
            LOG_DEBUG("  setupInfo size = {} bytes", setupInfoSize);

            if (setupInfoSize > 0) {
                // setupInfo should contain the T.35 prefix as a null-terminated string
                std::string setupInfoStr((char*)*setupInfoH, setupInfoSize);
                LOG_DEBUG("  setupInfo = '{}'", setupInfoStr);

                // Parse both prefixes to compare hex part only
                T35Prefix requestedPrefix(t35PrefixStr);
                T35Prefix filePrefix(setupInfoStr);

                // Verify hex prefix matches (ignore description)
                if (requestedPrefix.hex() != filePrefix.hex()) {
                    LOG_WARN("T.35 hex in setupInfo '{}' does not match requested hex '{}'",
                            filePrefix.hex(), requestedPrefix.hex());
                    MP4DisposeHandle(setupInfoH);
                    MP4DisposeHandle(read_key_value);
                    MP4DisposeHandle(sampleEntryH);
                    MP4DisposeTrackReader(reader);
                    continue;  // Try next track
                }
                LOG_DEBUG("T.35 hex matches requested hex");

                // Warn if descriptions differ (informative only)
                if (!requestedPrefix.description().empty() &&
                    !filePrefix.description().empty() &&
                    requestedPrefix.description() != filePrefix.description()) {
                    LOG_WARN("T.35 description mismatch: requested '{}' but file has '{}'",
                            requestedPrefix.description(), filePrefix.description());
                }
            } else {
                LOG_WARN("setupInfo is empty, cannot verify T.35 prefix");
            }

            MP4DisposeHandle(setupInfoH);
            MP4DisposeHandle(read_key_value);
            MP4DisposeHandle(sampleEntryH);
        }

        // Success!
        *outReader = reader;
        if (outTrack) *outTrack = trak;
        return MP4NoErr;
    }

    LOG_ERROR("No mebx track found with me4c namespace and it35 key_value");
    return MP4NotFoundErr;
}

bool MebxMe4cExtractor::canExtract(const ExtractionConfig& config,
                                    std::string& reason) const {
    if (!config.movie) {
        reason = "No movie provided";
        return false;
    }

    // Try to find mebx me4c track
    MP4TrackReader reader = nullptr;
    MP4Err err = findMebxMe4cTrackReader(config.movie, config.t35Prefix, &reader, nullptr);

    if (reader) {
        MP4DisposeTrackReader(reader);
    }

    if (err) {
        reason = "No mebx track with me4c namespace found";
        return false;
    }

    return true;
}

MP4Err MebxMe4cExtractor::extract(const ExtractionConfig& config) {
    LOG_INFO("Extracting metadata using mebx-me4c extractor");
    LOG_INFO("T.35 prefix: {}", config.t35Prefix);
    LOG_INFO("Output path: {}", config.outputPath);

    MP4Err err = MP4NoErr;
    MP4TrackReader mebxReader = nullptr;
    MP4Track mebxTrack = nullptr;

    // Find mebx me4c track
    LOG_DEBUG("Finding mebx me4c track");
    err = findMebxMe4cTrackReader(config.movie, config.t35Prefix, &mebxReader, &mebxTrack);
    if (err) {
        LOG_ERROR("Failed to find mebx me4c track (err={})", err);
        return err;
    }

    // Get timescale
    MP4Media mebxMedia = nullptr;
    u32 timescale = 1000; // default
    err = MP4GetTrackMedia(mebxTrack, &mebxMedia);
    if (err == MP4NoErr) {
        MP4GetMediaTimeScale(mebxMedia, &timescale);
    }
    LOG_DEBUG("Mebx track timescale: {}", timescale);

    // Create output directory
    namespace fs = std::filesystem;
    fs::path outDir(config.outputPath);

    if (!fs::exists(outDir)) {
        if (!fs::create_directories(outDir)) {
            LOG_ERROR("Failed to create output directory: {}", config.outputPath);
            MP4DisposeTrackReader(mebxReader);
            return MP4IOErr;
        }
    }

    LOG_INFO("Extracting samples to {}", outDir.string());

    // Extract all samples
    std::vector<nlohmann::json> manifestItems;
    u32 sampleCount = 0;
    u32 currentFrame = 0;

    for (u32 i = 1; ; ++i) {
        MP4Handle sampleH = nullptr;
        u32 sampleSize = 0, sampleFlags = 0, sampleDuration = 0;
        s32 dts = 0, cts = 0;

        err = MP4NewHandle(0, &sampleH);
        if (err) {
            MP4DisposeTrackReader(mebxReader);
            return err;
        }

        err = MP4TrackReaderGetNextAccessUnitWithDuration(
                mebxReader,
                sampleH,
                &sampleSize,
                &sampleFlags,
                &dts,
                &cts,
                &sampleDuration);

        if (err) {
            MP4DisposeHandle(sampleH);
            if (err == MP4EOF) {
                LOG_DEBUG("Reached end of mebx samples");
                err = MP4NoErr;
                break;
            }
            LOG_ERROR("Failed to read sample (err={})", err);
            MP4DisposeTrackReader(mebxReader);
            return err;
        }

        sampleCount++;

        // Calculate frame duration (assuming constant frame rate)
        u32 frameDuration = 1;
        if (sampleDuration > 0) {
            frameDuration = sampleDuration / 512; // rough estimate
            if (frameDuration == 0) frameDuration = 1;
        }

        // Write binary file
        fs::path binFile = outDir / ("metadata_" + std::to_string(i) + ".bin");
        std::ofstream out(binFile, std::ios::binary);
        if (!out) {
            LOG_ERROR("Failed to open {} for writing", binFile.string());
            MP4DisposeHandle(sampleH);
            MP4DisposeTrackReader(mebxReader);
            return MP4IOErr;
        }

        out.write((char*)*sampleH, sampleSize);
        out.close();

        LOG_INFO("Extracted sample {}: {} bytes, DTS={}, duration={} (frame {})",
                i, sampleSize, dts, sampleDuration, currentFrame);

        // Add to manifest
        nlohmann::json item;
        item["frame_start"] = currentFrame;
        item["frame_duration"] = frameDuration;
        item["binary_file"] = binFile.filename().string();
        item["sample_size"] = sampleSize;
        item["dts"] = dts;
        item["duration_timescale"] = sampleDuration;
        manifestItems.push_back(item);

        currentFrame += frameDuration;

        MP4DisposeHandle(sampleH);
    }

    MP4DisposeTrackReader(mebxReader);

    LOG_INFO("Extracted {} metadata samples", sampleCount);

    // Write manifest JSON
    if (!manifestItems.empty()) {
        nlohmann::json manifest;
        manifest["t35_prefix"] = config.t35Prefix;
        manifest["timescale"] = timescale;
        manifest["sample_count"] = sampleCount;
        manifest["items"] = manifestItems;

        fs::path manifestFile = outDir / "manifest.json";
        std::ofstream manifestOut(manifestFile);
        if (manifestOut) {
            manifestOut << manifest.dump(2);
            manifestOut.close();
            LOG_INFO("Wrote manifest to {}", manifestFile.string());
        } else {
            LOG_WARN("Failed to write manifest file");
        }
    }

    LOG_INFO("Extraction complete");
    return MP4NoErr;
}

} // namespace t35
