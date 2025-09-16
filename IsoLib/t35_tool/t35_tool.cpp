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

// C++ standard library headers
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <map>

// 3rd party headers
#include <CLI/CLI.hpp>
#include <nlohmann/json.hpp>


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
                                    const std::vector<u32>& metadataSizes,
                                    u32 local_key_id)
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
      *((u32*)*sizesH) = metadataSizes[0] + 8; // +4 box_size +4 box_type
    } else {
      err = MP4NewHandle(sizeof(u32) * sampleCount, &sizesH);
      if (err) goto bail;
      for (u32 n = 0; n < sampleCount; ++n) {
        ((u32*)*sizesH)[n] = metadataSizes[n] + 8; // +4 box_size +4 box_type
      }
    }
  }

  // --- Sample data handle ---
  totalSize = 0;
  for (u32 n = 0; n < sampleCount; ++n) {
    totalSize += metadataSizes[n] + 8;
  }
  err = MP4NewHandle((u32)totalSize, &sampleDataH);
  if (err) goto bail;

  {
    char* dst = reinterpret_cast<char*>(*sampleDataH);
    for (u32 n = 0; n < sampleCount; ++n)
    {
      const MetadataItem& item = sortedItems[n];
      std::ifstream binFile(item.bin_path, std::ios::binary);
      if (!binFile) 
      {
        std::cerr << "Failed to open .bin file: " << item.bin_path << "\n";
        err = MP4IOErr;
        goto bail;
      }

      u32 boxSize = 8 + metadataSizes[n];
      // write size
      dst[0] = (boxSize >> 24) & 0xFF;
      dst[1] = (boxSize >> 16) & 0xFF;
      dst[2] = (boxSize >>  8) & 0xFF;
      dst[3] = (boxSize      ) & 0xFF;
      // write type (local_key_id)
      dst[4] = (local_key_id >> 24) & 0xFF;
      dst[5] = (local_key_id >> 16) & 0xFF;
      dst[6] = (local_key_id >>  8) & 0xFF;
      dst[7] = (local_key_id      ) & 0xFF;
      dst += 8;

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

    err = addAllMetadataSamples(mediaM, sortedItems, metadataDurations, metadataSizes, local_key_id);
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


static MP4Err getMebxAndVideoTrackReaders(MP4Movie moov,
                                          MP4TrackReader* outMebxReader,
                                          MP4TrackReader* outVideoReader)
{
  MP4Err err = MP4NoErr;
  if (outMebxReader) *outMebxReader = nullptr;
  if (outVideoReader) *outVideoReader = nullptr;

  u32 trackCount = 0;
  err = MP4GetMovieTrackCount(moov, &trackCount);
  if (err) return err;

  // --- Step 1: find mebx track ---
  MP4Track mebxTrack = nullptr;
  for (u32 i = 1; i <= trackCount; ++i) 
  {
    MP4Track trak = nullptr;
    MP4Media media = nullptr;
    u32 handlerType = 0;

    err = MP4GetMovieIndTrack(moov, i, &trak);
    if (err) continue;

    err = MP4GetTrackMedia(trak, &media);
    if (err) continue;

    err = MP4GetMediaHandlerDescription(media, &handlerType, nullptr);
    if (err) continue;

    if (handlerType != MP4MetaHandlerType) continue; // only metadata tracks

    // Create track reader
    MP4TrackReader reader = nullptr;
    err = MP4CreateTrackReader(trak, &reader);
    if (err) continue;

    MP4Handle sampleEntryH = nullptr;
    err = MP4NewHandle(0, &sampleEntryH);
    if (err) { MP4DisposeTrackReader(reader); continue; }

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
    MP4DisposeTrackReader(reader);
    if (err) continue;

    if (type == MP4BoxedMetadataSampleEntryType) {
      mebxTrack = trak;
      break;
    }
  }

  if (!mebxTrack) {
    std::cerr << "No 'mebx' metadata track found\n";
    return MP4NotFoundErr;
  }

  // --- Step 2: create mebx reader ---
  MP4TrackReader mebxReader = nullptr;
  err = MP4CreateTrackReader(mebxTrack, &mebxReader);
  if (err) return err;

  // --- Step 3: find associated video track ---
  MP4Track videoTrack = nullptr;
  err = MP4GetTrackReference(mebxTrack, MP4DescTrackReferenceType, 1, &videoTrack);
  if (err) {
    MP4DisposeTrackReader(mebxReader);
    return err;
  }

  // --- Step 4: create video reader ---
  MP4TrackReader videoReader = nullptr;
  if (outVideoReader) { // Caller wants a video reader
    err = MP4CreateTrackReader(videoTrack, &videoReader);
    if (err) {
      MP4DisposeTrackReader(mebxReader);
      return err;
    }
  }

  if (outMebxReader) *outMebxReader = mebxReader;
  if (outVideoReader) *outVideoReader = videoReader;

  return MP4NoErr;
}


// TODO: add parameter with T.35 prefix to look for e.g.: B500900001
static MP4Err extractMebxSamples(MP4Movie moov, const std::string& inputFile) 
{
  std::cout << "Extracting SMPTE 2094-50 metadata (mebx)...\n";

  MP4Err err = MP4NoErr;
  MP4TrackReader mebxReader = nullptr;

  // --- Step 1: get mebx track reader ---
  err = getMebxAndVideoTrackReaders(moov, &mebxReader, nullptr);
  if (err) return err;

  // --- Step 1.1: set key_namespace and key_value that we are looking for ---
  u32 key_namespace = MP4_FOUR_CHAR_CODE('i', 't', '3', '5');
  MP4Handle key_value = nullptr;
  err = MP4NewHandle(30, &key_value);
  if (err) return err;
  // First 5 bytes: binary prefix according to generic definition
  (*key_value)[0] = 0xB5;
  (*key_value)[1] = 0x00;
  (*key_value)[2] = 0x90;
  (*key_value)[3] = 0x00;
  (*key_value)[4] = 0x01;
  // Rest: ASCII string "smpte_st_2094_50_dmcvt_v1"
  const char dmcvStr[] = "smpte_st_2094_50_dmcvt_v1";
  std::memcpy(&(*key_value)[5], dmcvStr, sizeof(dmcvStr) - 1);

  // --- Step 1.2: select the key ---
  u32 local_key_id = 0;
  err = MP4SelectMebxTrackReaderKey(mebxReader, key_namespace, key_value, &local_key_id);
  if(err) 
  {
    std::cerr << "MP4SelectMebxTrackReaderKey failed (err=" << err << ")\n";
    MP4DisposeHandle(key_value);
    MP4DisposeTrackReader(mebxReader);
    return err;
  }
  MP4DisposeHandle(key_value);
  std::cout << "Selected local_key_id = " << local_key_id << "\n";

  // --- Step 2: create output folder ---
  namespace fs = std::filesystem;
  fs::path outDir = fs::path(inputFile).stem().string() + "_dump";
  if (!fs::exists(outDir)) {
    fs::create_directory(outDir);
  }
  std::cout << "Extracting mebx samples to " << outDir << "\n";

  // --- Step 3: dump each sample ---
  u32 mebxSampleCount = 0;
  for (u32 i = 1; ; ++i) 
  {
    MP4Handle sampleH = nullptr;
    u32 sampleSize = 0, sampleFlags = 0, sampleDuration = 0;
    s32 dts = 0, cts = 0;

    err = MP4NewHandle(0, &sampleH);
    if (err) return err;

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
        std::cout << "Reached end of mebx samples.\n";
        err = MP4NoErr;
        break;
      }
      return err;
    }
    mebxSampleCount++;

    // Write .bin file
    fs::path outFile = outDir / ("sample_" + std::to_string(i) + ".bin");
    std::ofstream out(outFile, std::ios::binary);
    if (!out) {
      std::cerr << "Failed to open " << outFile << " for writing\n";
      MP4DisposeHandle(sampleH);
      return MP4IOErr;
    }

    out.write((char*)*sampleH, sampleSize);
    out.close();

    std::cout << "  wrote " << outFile 
          << " (" << sampleSize << " bytes)"
          << " DTS=" << dts 
          << " Duration=" << sampleDuration 
          << "\n";

    MP4DisposeHandle(sampleH);
  }

  std::cout << "Extracted " << mebxSampleCount << " mebx samples.\n";

  MP4DisposeTrackReader(mebxReader);
  return MP4NoErr;
}

static MP4Err dumpHevcWithMebxSei(MP4Movie moov, const std::string& inputFile)
{
  MP4Err err = MP4NoErr;
  MP4TrackReader mebxReader = nullptr;
  MP4TrackReader videoReader = nullptr;

  // --- Step 1: get track readers ---
  err = getMebxAndVideoTrackReaders(moov, &mebxReader, &videoReader);
  if (err) return err;

  // --- Step 2: get HEVC NALUs and legth_size_minus1+1 from sample entry ---
  MP4Handle videoSampleEntryH;
  err = MP4NewHandle(0, &videoSampleEntryH);
  if (err) return err;
  err = MP4TrackReaderGetCurrentSampleDescription(videoReader, videoSampleEntryH);
  if (err) return err;
  MP4Handle sampleEntryNALs = nullptr;
  MP4NewHandle(0, &sampleEntryNALs);
  err = ISOGetHEVCNALUs(videoSampleEntryH, sampleEntryNALs, 0);
  if(err)
  {
    std::cerr << "Failed to extract NAL units from sample entry (err=" << err << ")\n";
    return err;
  }
  u32 length_size = 0;
  err = ISOGetNALUnitLength(videoSampleEntryH, &length_size);
  if (err) {
    std::cerr << "Failed to get NAL unit length size (err=" << err << ")\n";
    return err;
  }
  std::cout << "HEVC NAL unit length size = " << length_size << "\n";

  // --- Step 3: prepare output file and dump NALs from sample entry ---
  namespace fs = std::filesystem;
  fs::path outFile = fs::path(inputFile).stem().string() + "_sei.hevc";
  std::ofstream out(outFile, std::ios::binary);
  if (!out) {
    std::cerr << "Failed to open " << outFile << " for writing\n";
    return MP4IOErr;
  }
  u32 sampleEntryNalSize = 0;
  MP4GetHandleSize(sampleEntryNALs, &sampleEntryNalSize);
  out.write((char*)*sampleEntryNALs, sampleEntryNalSize);
  std::cout << "Wrote " << sampleEntryNalSize << " bytes decoder configuration data into " << outFile << "\n";


  // --- Step X: iterate video samples ---
  
  // TODO: get video sample and associated mebx sample, write SEI NAL unit before video sample, do emulation prevention
  // Write raw video sample to output stream
  // replace length prefixes with start codes
    

  out.close();
  std::cout << "Finished writing " << outFile << "\n";

  MP4DisposeTrackReader(mebxReader);
  MP4DisposeTrackReader(videoReader);
  return MP4NoErr;
}

int main(int argc, char** argv) {
  CLI::App app{"ITU-T T.35 metadata tool"};

  std::string inputFile;
  std::string metadataFolder;
  std::string mode = "mebx"; // default mode

  app.add_option("input", inputFile, "Input file")->required();

  // Subcommand: inject
  auto inject = app.add_subcommand("inject", "Inject metadata into MP4");
  inject->add_option("metadata", metadataFolder, "Folder with metadata")->required();
  inject->add_option("mode", mode, "Injection mode: mebx or sei")
        ->default_val("mebx")
        ->check(CLI::IsMember({"mebx", "sei"}));

  // Subcommand: extract
  auto extract = app.add_subcommand("extract", "Extract metadata from MP4");
  extract->add_option("mode", mode, "Extraction mode: mebx or sei")
         ->default_val("mebx")
         ->check(CLI::IsMember({"mebx", "sei"}));

  CLI11_PARSE(app, argc, argv);

  MP4Err err = MP4NoErr;
  MP4Movie moov = nullptr;

  // Open MP4
  err = MP4OpenMovieFile(&moov, inputFile.c_str(), MP4OpenMovieNormal);
  if (err) {
    std::cerr << "Failed to open " << inputFile << " (err=" << err << ")\n";
    return err;
  }

  if (*inject) 
  {
    std::cout << "Input file      : " << inputFile << "\n";
    std::cout << "Metadata folder : " << metadataFolder << "\n";
    std::cout << "Action          : inject\n";
    std::cout << "Mode            : " << mode << "\n";

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
  } 
  else if (*extract) 
  {
    std::cout << "Input file      : " << inputFile << "\n";
    std::cout << "Action          : extract\n";
    std::cout << "Mode            : " << mode << "\n";

    if (mode == "mebx") {
      err = extractMebxSamples(moov, inputFile);
      if (err) {
        std::cerr << "Extraction failed with err=" << err << "\n";
      } else {
        std::cout << "Extraction completed successfully.\n";
      }
    } else if (mode == "sei") {
      err = dumpHevcWithMebxSei(moov, inputFile);
      if (err) {
        std::cerr << "Dumping HEVC with SEI failed with err=" << err << "\n";
      } else {
        std::cout << "Dumping HEVC with SEI completed successfully.\n";
      }
    }
  }

  MP4DisposeMovie(moov);
  return 0;
}
