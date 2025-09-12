/**
 * @file t35_tool.cpp
 * @brief ITU-T T.35 metadata tool to inject/extract from/to mp4 files
 * @version 0.1
 * @date 2025-09-10
 *
 * @copyright This software module was originally developed by Apple Computer, Inc. in the course of
 * development of MPEG-4. This software module is an implementation of a part of one or more MPEG-4
 * tools as specified by MPEG-4. ISO/IEC gives users of MPEG-4 free license to this software module
 * or modifications thereof for use in hardware or software products claiming conformance to MPEG-4
 * only for evaluation and testing purposes. Those intending to use this software module in hardware
 * or software products are advised that its use may infringe existing patents. The original
 * developer of this software module and his/her company, the subsequent editors and their
 * companies, and ISO/IEC have no liability for use of this software module or modifications thereof
 * in an implementation.
 *
 * Copyright is not released for non MPEG-4 conforming products. Apple Computer, Inc. retains full
 * right to use the code for its own purpose, assign or donate the code to a third party and to
 * inhibit third parties from using the code for non MPEG-4 conforming products. This copyright
 * notice must be included in all copies or derivative works.
 *
 */

// libisomediafile headers
extern "C" {
  #include "MP4Movies.h"
  #include "MP4Atoms.h"
}

// C++ headers
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <nlohmann/json.hpp>


static void fourccToStr(u32 fcc, char out[5]) {
  out[0] = char((fcc >> 24) & 0xFF);
  out[1] = char((fcc >> 16) & 0xFF);
  out[2] = char((fcc >>  8) & 0xFF);
  out[3] = char((fcc      ) & 0xFF);
  out[4] = 0;
}

struct MetadataItem {
  uint32_t frame_start;
  uint32_t frame_duration;
  std::string bin_path;
};

// key = starting frame number
using MetadataMap = std::map<uint32_t, MetadataItem>;

MetadataMap parseMetadataFolder(const std::string& metadataFolder) {
  MetadataMap items;

  namespace fs = std::filesystem;
  for (auto& entry : fs::directory_iterator(metadataFolder)) {
    if (!entry.is_regular_file()) continue;

    auto path = entry.path();
    if (path.extension() == ".json") {
      std::ifstream in(path);
      if (!in) {
        std::cerr << "Failed to open JSON: " << path << "\n";
        continue;
      }

      nlohmann::json j;
      in >> j;

      if (!j.contains("frame_start") || !j.contains("frame_duration")) {
        std::cerr << "Skipping " << path << " (missing keys)\n";
        continue;
      }

      uint32_t frame_start    = j["frame_start"].get<uint32_t>();
      uint32_t frame_duration = j["frame_duration"].get<uint32_t>();

      // find matching .bin file
      auto baseName = path.stem().string();  // e.g. ST2094-50_IMG_0564_metadataItems
      auto binPath  = path.parent_path() / (baseName.substr(0, baseName.find("_metadataItems")) + ".bin");

      if (!fs::exists(binPath)) {
        std::cerr << "No matching .bin file for " << path << "\n";
        continue;
      }

      MetadataItem item{frame_start, frame_duration, binPath.string()};
      items[frame_start] = item;

      std::cout << "Loaded metadata: " << binPath << " -> frames [" 
                << frame_start << " - " << (frame_start + frame_duration - 1) << "]\n";
    }
  }

  return items;
}

static MP4Err findFirstVideoTrack(MP4Movie moov, MP4Track* outTrack) {
  MP4Err err = MP4NoErr;
  u32 trackCount = 0;
  *outTrack = nullptr;

  err = MP4GetMovieTrackCount(moov, &trackCount);
  if (err) return err;

  MP4Track firstVideo = nullptr;
  u32 videoCount = 0;

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

    if (handlerType == MP4VisualHandlerType) {
      if (!firstVideo) {
        firstVideo = trak;
      }
      ++videoCount;
    }
  }

  if (!firstVideo) {
    std::cerr << "No video track found in movie\n";
    return MP4NotFoundErr;
  }

  if (videoCount > 1) {
    std::cerr << "Warning: found " << videoCount
              << " video tracks, using the first one.\n";
  }

  *outTrack = firstVideo;
  return MP4NoErr;
}


static MP4Err getVideoSampleDurations(MP4Media mediaV, std::vector<u32>& durations)
{
  MP4Err err = MP4NoErr;
  u32 sampleCount = 0;

  durations.clear();

  // Get number of samples in this media
  err = MP4GetMediaSampleCount(mediaV, &sampleCount);
  if (err) return err;

  durations.reserve(sampleCount);

  for (u32 i = 1; i <= sampleCount; ++i) {
    MP4Handle sampleH = nullptr;
    u32 outSize, outSampleFlags, outSampleDescIndex;
    u64 outDTS, outDuration;
    s32 outCTSOffset;

  MP4NewHandle(0, &sampleH);
  err = MP4GetIndMediaSample(mediaV, i, sampleH, &outSize, &outDTS, &outCTSOffset,
                             &outDuration, &outSampleFlags, &outSampleDescIndex);
    if (err) {
      if (sampleH) MP4DisposeHandle(sampleH);
      return err;
    }

    durations.push_back(static_cast<u32>(outDuration));

    if (sampleH) MP4DisposeHandle(sampleH);
  }

  std::cout << "Collected " << durations.size() << " video sample durations\n";
  return MP4NoErr;
}



static MP4Err buildMetadataDurationsAndSizes( const MetadataMap& items,
                                              const std::vector<u32>& videoDurations,
                                              std::vector<u32>& metadataDurations,
                                              std::vector<u32>& metadataSizes,
                                              std::vector<MetadataItem>& sortedItems)
{
  MP4Err err = MP4NoErr;
  metadataDurations.clear();
  metadataSizes.clear();
  sortedItems.clear();

  if (items.empty()) {
    std::cerr << "No metadata items provided\n";
    return MP4BadParamErr;
  }

  // Sort items by frame_start
  std::vector<std::pair<u32, MetadataItem>> sorted;
  for (auto& kv : items) {
    sorted.push_back({kv.first, kv.second});
  }
  std::sort(sorted.begin(), sorted.end(),
            [](auto& a, auto& b) { return a.first < b.first; });

  // Validate no overlaps
  for (size_t i = 1; i < sorted.size(); ++i) {
    u32 prevEnd = sorted[i - 1].first + sorted[i - 1].second.frame_duration;
    if (sorted[i].first < prevEnd) {
      std::cerr << "Error: overlapping metadata entries detected. "
                << "Entry starting at " << sorted[i].first
                << " overlaps previous ending at " << prevEnd << "\n";
      return MP4BadParamErr;
    }
  }

  // Check coverage using last entry only (since no overlaps)
  auto& last = sorted.back().second;
  u32 maxFrame = sorted.back().first + last.frame_duration;
  if (maxFrame > videoDurations.size()) {
    std::cerr << "Metadata covers up to frame " << maxFrame
              << " but video only has " << videoDurations.size() << " samples\n";
    return MP4BadParamErr;
  }

  // Compute metadata sample durations and sizes
  for (auto& [start, item] : sorted) {
    u32 endFrame = start + item.frame_duration;
    u32 totalDur = 0;
    for (u32 f = start; f < endFrame; ++f) {
      totalDur += videoDurations[f];
    }
    metadataDurations.push_back(totalDur);
    sortedItems.push_back(item);

    // File size from .bin path
    namespace fs = std::filesystem;
    if (!fs::exists(item.bin_path)) {
      std::cerr << "Missing .bin file: " << item.bin_path << "\n";
      return MP4FileNotFoundErr;
    }
    auto fileSize = fs::file_size(item.bin_path);
    metadataSizes.push_back(static_cast<u32>(fileSize));

    std::cout << "Metadata " << item.bin_path
              << " covers frames [" << start << "-" << (endFrame - 1) << "]"
              << " totalDur=" << totalDur
              << " size=" << fileSize << " bytes\n";
  }

  return err;
}


static MP4Err addAllMetadataSamples(MP4Media mediaM,
                                    const std::vector<MetadataItem>& sortedItems,
                                    const std::vector<u32>& metadataDurations,
                                    const std::vector<u32>& metadataSizes)
{
  MP4Err err = MP4NoErr;
  u32 sampleCount = static_cast<u32>(sortedItems.size());

  MP4Handle durationsH = nullptr;
  MP4Handle sizesH     = nullptr;
  MP4Handle sampleDataH = nullptr;
  u64 totalSize = 0;

  if (sampleCount == 0) {
    std::cerr << "No metadata samples to add\n";
    return MP4BadParamErr;
  }

  // --- Durations handle ---
  {
    bool allSame = std::all_of(metadataDurations.begin(), metadataDurations.end(),
                               [&](u32 d) { return d == metadataDurations[0]; });
    if (allSame) {
      err = MP4NewHandle(sizeof(u32), &durationsH);
      if (err) goto bail;
      *((u32*)*durationsH) = metadataDurations[0];
    } else {
      err = MP4NewHandle(sizeof(u32) * sampleCount, &durationsH);
      if (err) goto bail;
      for (u32 n = 0; n < sampleCount; ++n) {
        ((u32*)*durationsH)[n] = metadataDurations[n];
      }
    }
  }

  // --- Sizes handle ---
  {
    bool allSame = std::all_of(metadataSizes.begin(), metadataSizes.end(),
                               [&](u32 s) { return s == metadataSizes[0]; });
    if (allSame) {
      err = MP4NewHandle(sizeof(u32), &sizesH);
      if (err) goto bail;
      *((u32*)*sizesH) = metadataSizes[0];
    } else {
      err = MP4NewHandle(sizeof(u32) * sampleCount, &sizesH);
      if (err) goto bail;
      for (u32 n = 0; n < sampleCount; ++n) {
        ((u32*)*sizesH)[n] = metadataSizes[n];
      }
    }
  }

  // --- Sample data handle ---
  totalSize = 0;
  for (u32 n = 0; n < sampleCount; ++n) {
    totalSize += metadataSizes[n];
  }
  err = MP4NewHandle((u32)totalSize, &sampleDataH);
  if (err) goto bail;

  {
    char* dst = reinterpret_cast<char*>(*sampleDataH);
    for (u32 n = 0; n < sampleCount; ++n) {
      const MetadataItem& item = sortedItems[n];
      std::ifstream binFile(item.bin_path, std::ios::binary);
      if (!binFile) {
        std::cerr << "Failed to open .bin file: " << item.bin_path << "\n";
        err = MP4IOErr;
        goto bail;
      }
      binFile.read(dst, metadataSizes[n]);
      dst += metadataSizes[n];
    }
  }

  // --- Add all samples in one call ---
  err = MP4AddMediaSamples(mediaM,
                           sampleDataH,
                           sampleCount,
                           durationsH,
                           sizesH,
                           0,   // reuse sample entry
                           0,   // no decoding offsets
                           0);  // all sync samples
  if (err) {
    std::cerr << "MP4AddMediaSamples failed (err=" << err << ")\n";
    goto bail;
  }

  std::cout << "Added " << sampleCount << " metadata samples\n";

bail:
  if (sampleDataH) MP4DisposeHandle(sampleDataH);
  if (durationsH)  MP4DisposeHandle(durationsH);
  if (sizesH)      MP4DisposeHandle(sizesH);

  return err;
}


static MP4Err injectMetadata(MP4Movie moov,
                             const std::string& mode,
                             const MetadataMap& items)
{
  std::cout << "Injecting SMPTE 2094-50 metadata (" << mode << ")...\n";

  if (mode == "mebx") {
    std::cout << "Creating 'mebx' track...\n";

    MP4Err err = MP4NoErr;
    MP4Track trakM = nullptr;   // metadata track
    MP4Track trakV = nullptr;   // reference to video track
    MP4Media mediaM = nullptr;

    err = findFirstVideoTrack(moov, &trakV);
    if (err) return err;

    // Create mebx track
    err = MP4NewMovieTrack(moov, MP4NewTrackIsMebx, &trakM);
    if (err) return err;

    // Create mebx media, using the same timescale as the video track
    MP4Media videoMedia = nullptr;
    u32 videoTimescale = 0;
    err = MP4GetTrackMedia(trakV, &videoMedia);
    if (err) return err;
    err = MP4GetMediaTimeScale(videoMedia, &videoTimescale);
    if (err) {
      videoTimescale = 1000; // default to 1000 if not available
      std::cerr << "Warning: failed to get video timescale, defaulting to 1000\n";
    }
    std::vector<u32> videoDurations;
    err = getVideoSampleDurations(videoMedia, videoDurations);
    if(err) {
      std::cerr << "Failed to get video sample durations (err=" << err << ")\n";
      return err;
    }

    err = MP4NewTrackMedia(trakM, &mediaM, MP4MetaHandlerType, videoTimescale, NULL);
    if (err) return err;

    // Link metadata track to video track
    err = MP4AddTrackReference(trakM, trakV, MP4DescTrackReferenceType, 0);
    if (err) return err;

    // Create mebx sample entry
    MP4BoxedMetadataSampleEntryPtr mebx = nullptr;
    err = ISONewMebxSampleDescription(&mebx, 1);
    if (err) return err;

    // Build dmcvt handle
    MP4Handle dmcvtH = nullptr;
    err = MP4NewHandle(30, &dmcvtH);
    if (err) return err;

    // First 5 bytes: binary prefix according to generic definition
    (*dmcvtH)[0] = 0xB5;
    (*dmcvtH)[1] = 0x00;
    (*dmcvtH)[2] = 0x90;
    (*dmcvtH)[3] = 0x00;
    (*dmcvtH)[4] = 0x01;
    // Rest: ASCII string "smpte_st_2094_50_dmcvt_v1"
    const char dmcvStr[] = "smpte_st_2094_50_dmcvt_v1";
    std::memcpy(&(*dmcvtH)[5], dmcvStr, sizeof(dmcvStr) - 1);

    // Add sample entry
    u32 local_key_id = 0;
    err = ISOAddMebxMetadataToSampleEntry(
              mebx,
              1,
              &local_key_id,
              MP4_FOUR_CHAR_CODE('i', 't', '3', '5'),
              dmcvtH,
              0,
              0);
    if (err) return err;
    MP4Handle sampleEntryMH = nullptr;
    err = MP4NewHandle(0, &sampleEntryMH);
    if (err) return err;
    err = ISOGetMebxHandle(mebx, sampleEntryMH);
    if (err) return err;
    err = MP4AddMediaSamples(mediaM, 0, 0, 0, 0, sampleEntryMH, 0, 0); // no sample yet, just the sample entry
    if (err) return err;

    std::cout << "MEBX track and sample entry created successfully.\n";
    std::cout << "Local key ID = " << local_key_id << "\n";

    // Prepare metadata sample durations and sizes
    std::vector<u32> metadataDurations;
    std::vector<u32> metadataSizes;
    std::vector<MetadataItem> sortedItems;
    err = buildMetadataDurationsAndSizes(items, videoDurations, metadataDurations, metadataSizes, sortedItems);
    if (err) return err;

    err = addAllMetadataSamples(mediaM, sortedItems, metadataDurations, metadataSizes);
    if (err) return err;

    err = MP4EndMediaEdits(mediaM);
    if (err) return err;
  } else if (mode == "sei") {
    // insert SEI messages into video samples
    for (auto& [start, item] : items) {
      std::cout << "Would inject (SEI) " << item.bin_path
                << " into frames [" << start
                << " - " << (start + item.frame_duration - 1) << "]\n";
    }
    return MP4NotImplementedErr;
  }

  return MP4NoErr;
}

int main(int argc, char** argv) {
  if (argc < 4 || argc > 5) {
    std::cerr << "Usage: t35_tool input.mp4 metadata_folder action [mode]\n";
    std::cerr << "  action: 'inject' or 'extract'\n";
    std::cerr << "  mode  : 'mebx' or 'sei' (required only for 'inject')\n";
    return 1;
  }

  std::string inputFile      = argv[1];
  std::string metadataFolder = argv[2];
  std::string action         = argv[3];
  std::string mode;

  if (action != "inject" && action != "extract") {
    std::cerr << "Invalid action: " << action << "\n";
    std::cerr << "Must be either 'inject' or 'extract'\n";
    return 1;
  }

  if (action == "inject") {
    if (argc != 5) {
      std::cerr << "Inject mode requires a 'mode' argument (mebx or sei)\n";
      return 1;
    }
    mode = argv[4];
    if (mode != "mebx" && mode != "sei") {
      std::cerr << "Invalid mode: " << mode << "\n";
      std::cerr << "Must be either 'mebx' or 'sei'\n";
      return 1;
    }
  }

  std::cout << "Input file      : " << inputFile << "\n";
  std::cout << "Metadata folder : " << metadataFolder << "\n";
  std::cout << "Action          : " << action << "\n";
  if (action == "inject") {
    std::cout << "Mode            : " << mode << "\n";
  }

  MP4Err err = MP4NoErr;
  MP4Movie moov = nullptr;

  // Open MP4
  err = MP4OpenMovieFile(&moov, inputFile.c_str(), MP4OpenMovieDebug);
  // return MP4NoErr; // debug

  if (err) {
    std::cerr << "Failed to open " << inputFile << " (err=" << err << ")\n";
    return err;
  }

  if (action == "inject") {
    // Step 1: parse metadata folder
    auto items = parseMetadataFolder(metadataFolder);

    if (items.empty()) {
      std::cerr << "No metadata found in folder " << metadataFolder << "\n";
    } else {
      std::cout << "Parsed " << items.size() << " metadata items\n";
    }

    err = injectMetadata(moov, mode, items);
    if (err) {
      std::cerr << "Injection failed with err=" << err << "\n";
    } else {
      std::cout << "Injection completed successfully.\n";

      std::string outFile;
      if (mode == "mebx") {
        outFile = inputFile + "_mebx.mp4";
      } else {
        outFile = inputFile + "_sei.mp4";
      }

      std::cout << "Writing output file: " << outFile << "\n";
      err = MP4WriteMovieToFile(moov, outFile.c_str());
      if (err) {
        std::cerr << "Failed to write output file (err=" << err << ")\n";
      }
    }
  } else if (action == "extract") {
    // TODO: extraction logic
    std::cout << "Extraction not yet implemented.\n";
  }

  MP4DisposeMovie(moov);
  return 0;
}

