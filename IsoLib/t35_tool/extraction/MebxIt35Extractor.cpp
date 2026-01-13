#include "MebxIt35Extractor.hpp"
#include "../common/Logger.hpp"

extern "C" {
    #include "MP4Movies.h"
    #include "MP4Atoms.h"
}

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace t35 {

// Helper: Find mebx track with it35 namespace and matching T.35 prefix
static MP4Err findMebxTrackReader(MP4Movie moov,
                                   const std::string& t35PrefixStr,
                                   MP4TrackReader* outReader,
                                   MP4Track* outTrack) {
    MP4Err err = MP4NoErr;
    *outReader = nullptr;
    if (outTrack) *outTrack = nullptr;

    u32 trackCount = 0;
    err = MP4GetMovieTrackCount(moov, &trackCount);
    if (err) return err;

    LOG_DEBUG("Searching for mebx track in {} tracks", trackCount);

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

        MP4Handle sampleEntryH = nullptr;
        err = MP4NewHandle(0, &sampleEntryH);
        if (err) {
            MP4DisposeTrackReader(reader);
            continue;
        }

        // Get current sample description
        err = MP4TrackReaderGetCurrentSampleDescription(reader, sampleEntryH);
        if (err) {
            MP4DisposeHandle(sampleEntryH);
            MP4DisposeTrackReader(reader);
            continue;
        }

        // Check if it's 'mebx'
        u32 type = 0;
        err = ISOGetSampleDescriptionType(sampleEntryH, &type);

        MP4DisposeHandle(sampleEntryH);
        MP4DisposeTrackReader(reader); // Dispose the temporary reader

        if (err || type != MP4BoxedMetadataSampleEntryType) {
            continue;
        }

        LOG_INFO("Found mebx track with ID {}", trackID);

        // Check for 'rndr' track reference
        MP4Track videoTrack = nullptr;
        err = MP4GetTrackReference(trak, MP4_FOUR_CHAR_CODE('r', 'n', 'd', 'r'), 1, &videoTrack);
        if (err) {
            LOG_WARN("Mebx track ID {} has no 'rndr' track reference, skipping", trackID);
            continue;
        }

        u32 videoTrackID = 0;
        MP4GetTrackID(videoTrack, &videoTrackID);
        LOG_DEBUG("Mebx track references video track ID {}", videoTrackID);

        // Create a fresh reader for key selection
        err = MP4CreateTrackReader(trak, &reader);
        if (err) return err;

        // Try to select the it35 namespace key with matching T.35 prefix
        u32 key_namespace = MP4_FOUR_CHAR_CODE('i', 't', '3', '5');
        MP4Handle key_value = nullptr;
        err = MP4NewHandle((u32)t35PrefixStr.size(), &key_value);
        if (err) {
            MP4DisposeTrackReader(reader);
            return err;
        }

        // Copy T.35 prefix string to handle (text mode)
        std::memcpy((void*)*key_value, t35PrefixStr.data(), t35PrefixStr.size());

        LOG_DEBUG("Searching for key_namespace='it35', key_value='{}' ({}bytes)",
                 t35PrefixStr, t35PrefixStr.size());

        u32 local_key_id = 0;
        err = MP4SelectMebxTrackReaderKey(reader, key_namespace, key_value, &local_key_id);
        MP4DisposeHandle(key_value);

        if (err) {
            LOG_DEBUG("MP4SelectMebxTrackReaderKey failed for track {} (err={})", trackID, err);
            MP4DisposeTrackReader(reader);
            continue;
        }

        LOG_INFO("Selected mebx track ID {} with local_key_id = {}", trackID, local_key_id);

        // Success!
        *outReader = reader;
        if (outTrack) *outTrack = trak;
        return MP4NoErr;
    }

    LOG_ERROR("No mebx track found with it35 namespace and matching T.35 prefix");
    return MP4NotFoundErr;
}

bool MebxIt35Extractor::canExtract(const ExtractionConfig& config,
                                   std::string& reason) const {
    if (!config.movie) {
        reason = "No movie provided";
        return false;
    }

    // Try to find mebx track
    MP4TrackReader reader = nullptr;
    MP4Err err = findMebxTrackReader(config.movie, config.t35Prefix, &reader, nullptr);

    if (reader) {
        MP4DisposeTrackReader(reader);
    }

    if (err) {
        reason = "No mebx track with it35 namespace found";
        return false;
    }

    return true;
}

MP4Err MebxIt35Extractor::extract(const ExtractionConfig& config) {
    LOG_INFO("Extracting metadata using mebx-it35 extractor");
    LOG_INFO("T.35 prefix: {}", config.t35Prefix);
    LOG_INFO("Output path: {}", config.outputPath);

    MP4Err err = MP4NoErr;
    MP4TrackReader mebxReader = nullptr;
    MP4Track mebxTrack = nullptr;

    // Find mebx track
    LOG_DEBUG("Finding mebx track");
    err = findMebxTrackReader(config.movie, config.t35Prefix, &mebxReader, &mebxTrack);
    if (err) {
        LOG_ERROR("Failed to find mebx track (err={})", err);
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
        // For now, we'll use 1 frame per timescale unit, can be refined
        u32 frameDuration = 1;
        if (sampleDuration > 0) {
            // This is a simplification - in reality we'd need video track info
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

    LOG_INFO("✓ Extraction complete");
    return MP4NoErr;
}

} // namespace t35
