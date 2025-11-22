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
#include "SMPTE_ST2094_50.hpp" 

// Usage shortcuts:
// - mkdir mybuild && cd mybuild
// - cmake ..
// - make t35_tool -j
// - cp ~/CodeApple/DimitriPodborski/ISOBMMF/isobmff-internal/bin/t35_tool ~/CodeApple/DimitriPodborski/
// - cd ~/CodeApple/DimitriPodborski/
// -  ./t35_tool <movie_file> inject <Metadata_folder> mebx
// -  ./t35_tool 160299415_SMPTE2094-50_MetadataVideo_Duration100Percent_IMG522.mov inject 2025-09-22_mebx_MetadataExample mebx
// -  ./t35_tool 160299415_SMPTE2094-50_MetadataVideo_Duration100Percent_IMG522.mov_mebx.mp4 extract mebx

// ./t35_tool 160299415_SMPTE2094-50_MetadataVideo_Duration100Percent_IMG522.mov inject TestVariousMetadataType mebx
// ./t35_tool 160299415_SMPTE2094-50_MetadataVideo_Duration100Percent_IMG522.mov_mebx.mp4 extract mebx

// ./t35_tool 160299415_SMPTE2094-50_MetadataVideo_Duration100Percent_IMG522.mov inject SingleImage mebx --t35-prefix 'B500900001:SMPTE-ST2094-50'
// ./t35_tool 160299415_SMPTE2094-50_MetadataVideo_Duration100Percent_IMG522.mov_mebx.mp4 extract mebx
// To verify presence of metatada track in file:
// moovscope -dumpmebx -dumpmebxdata <filename>



constexpr bool STRING_TO_HANDLE_MODE = false;   // true = hex, false = text
struct MetadataItem {
  uint32_t frame_start;
  uint32_t frame_duration;
  std::string bin_path;
};



// key = starting frame number
using MetadataMap = std::map<uint32_t, MetadataItem>;

/* *********************************** ENCODING SECTION ********************************************************************************************/

// Parse JSON Folder, convert metadata item to syntax element and write to file binary data
MetadataMap parseMetadataFolder(const std::string& metadataFolder) {
  MetadataMap items;
  std::vector<uint8_t> payloadBinaryData;
  uint32_t frame_start;
  uint32_t frame_duration;
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

      // Decode SMPTE ST 2094-50 Metadata Items from json
      bool error_raised = false;
      SMPTE_ST2094_50 st2094_50; // Create an object of MyClass
      error_raised = st2094_50.decodeJsonToMetadataItems(j, path); // decode the json to metadata items
      st2094_50.dbgPrintMetadataItems(false);  // print up to what was decoded from the json
      if (error_raised) {
        std::cerr << "Skipping " << path << " error decoding json file\n";
        continue;
      }
      st2094_50.convertMetadataItemsToSyntaxElements();  // print up to what was decoded from the json
      st2094_50.writeSyntaxElementsToBinaryData();    
      payloadBinaryData = st2094_50.getPayloadData();
      frame_start       = st2094_50.getTimeIntervalStart();
      frame_duration    = st2094_50.getTimeintervalDuration();

      // write the payload data to a binary file
      auto baseName = path.stem().string();  // e.g. ST2094-50_IMG_0564_metadataItems
      std::string to_find = "_metadataItems";
      auto binPathGen  = path.parent_path() / (baseName.replace(baseName.find("_metadataItems"), to_find.length(), "_gen") + ".bin");
      // Write the binary data into a file
      std::ofstream outFile(binPathGen, std::ios::out | std::ios::binary);
      if (outFile.is_open()) {
        for (size_t i = 0; i < st2094_50.payloadBinaryData.size(); ++i) {
          outFile.write(reinterpret_cast<const char*>(&st2094_50.payloadBinaryData[i]), sizeof(st2094_50.payloadBinaryData[i]));
        }
        outFile.close(); // Close the file
        std::cout <<"Binary data successfuylly written to " << binPathGen << "\n";
        std::cout << "+++++++++++++++++++++++++++++++ Binary data successfully written to intermediate file +++++++++++++++++++++++++++++++++]\n";
      } else {
          std::cerr << "Error opening file: " << binPathGen << std::endl;
      } 
      
      std::cout << "Loaded metadata: " << baseName << " -> metadata size=" << st2094_50.payloadBinaryData.size() << " bytes || timimg frames [" 
                << frame_start << " - " << (frame_start + frame_duration - 1) << "]\n";

      // Write using the binary data generated by this tool from the json
      MetadataItem item{frame_start, frame_duration, binPathGen.string()};
      items[frame_start] = item;
    }
  }
  return items;
}

/* *********************************** DECODING SECTION ********************************************************************************************/

void decodeBinaryData(const std::string& inputFile) {

  // Open the binary file for reading
  std::ifstream inFile(inputFile, std::ios::in | std::ios::binary);

  if (!inFile.is_open()) {
      std::cerr << "Error opening file: " << inputFile << std::endl;
  }

  // Read uint16_t values one by one
  uint16_t data;
  //uint16_t total_sample = 0;
  std::vector<uint8_t> binary_data;
  while (inFile.read(reinterpret_cast<char*>(&data), sizeof(uint8_t))) {
    binary_data.push_back(data);
  }

  std::cout << "++++++++++++++++++++++Start processing : " <<  inputFile << "+++++++++++++++++++++++\n"; 
  SMPTE_ST2094_50 st2094_50; // Create an object of MyClass
  st2094_50.decodeBinaryToSyntaxElements(binary_data);
  st2094_50.convertSyntaxElementsToMetadataItems();
  st2094_50.dbgPrintMetadataItems(true);  // print decoded metadata from bitstream
  return;

}


/* *********************************** MOVIE FILE SECTION ******************************************************************************************/
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

static MP4Err stringToHandle(const std::string& input, MP4Handle* outHandle, bool asHex)
{
  MP4Err err = MP4NoErr;
  *outHandle = nullptr;

  if (asHex) {
    /* No need to be a multiple of 2 anymore since we have descriptive label
    if (input.size() % 2 != 0) {
      std::cerr << "Invalid hex string length: " << input << "\n";
      return MP4BadParamErr;
    }
      */
    u32 byteCount = static_cast<u32>(input.size() / 2);
    err = MP4NewHandle(byteCount, outHandle);
    if (err) return err;

    for (u32 i = 0; i < byteCount; i++) {
      unsigned int byteVal = 0;
      std::string byteStr = input.substr(i * 2, 2);
      /* Do not restrict value - Maybe to characters?
      if (sscanf(byteStr.c_str(), "%02x", &byteVal) != 1) {
        std::cerr << "Invalid hex substring: " << byteStr << "\n";
        MP4DisposeHandle(*outHandle);
        *outHandle = nullptr;
        return MP4BadParamErr;
      }
        */
      (**outHandle)[i] = static_cast<u8>(byteVal);
    }
  } else {
    u32 byteCount = static_cast<u32>(input.size());
    err = MP4NewHandle(byteCount, outHandle);
    if (err) return err;

    memcpy(**outHandle, input.data(), byteCount);
  }

  return MP4NoErr;
}

static MP4Err injectMetadata(MP4Movie moov,
                             const std::string& mode,
                             const MetadataMap& items,
                             const std::string& t35PrefixHex)
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

    // Link metadata track to video track using 'rndr' track reference
    err = MP4AddTrackReference(trakM, trakV, MP4_FOUR_CHAR_CODE('r', 'n', 'd', 'r'), 0);
    if (err) return err;

    // Create mebx sample entry
    MP4BoxedMetadataSampleEntryPtr mebx = nullptr;
    err = ISONewMebxSampleDescription(&mebx, 1);
    if (err) return err;

    // Build T.35 Prefix handle
    MP4Handle key_value = nullptr;
    err = stringToHandle(t35PrefixHex, &key_value, STRING_TO_HANDLE_MODE);
    if (err) return err;

    // Add sample entry
    u32 local_key_id = 0;
    err = ISOAddMebxMetadataToSampleEntry(
              mebx,
              1,
              &local_key_id,
              MP4_FOUR_CHAR_CODE('i', 't', '3', '5'),
              key_value,
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
                                          MP4TrackReader* outVideoReader,
                                          const std::string& t35PrefixHex)
{
  MP4Err err = MP4NoErr;
  if (outMebxReader) *outMebxReader = nullptr;
  if (outVideoReader) *outVideoReader = nullptr;

  u32 trackCount = 0;
  err = MP4GetMovieTrackCount(moov, &trackCount);
  if (err) return err;

  // --- Step 1: find mebx track ---
  MP4Track mebxTrack = nullptr;
  MP4Track videoTrack = nullptr;
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

    u32 currentMebxTrackID = 0;
    MP4GetTrackID(trak, &currentMebxTrackID);

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

    if (type != MP4BoxedMetadataSampleEntryType) continue;

    std::cout << "Found mebx track with trackID = " << currentMebxTrackID << "\n";

    // --- Step 2: create mebx reader ---
    MP4TrackReader mebxReader = nullptr;
    err = MP4CreateTrackReader(trak, &mebxReader);
    if (err) return err;

    // --- Step 3: find associated video track with rndr track reference type ---
    err = MP4GetTrackReference(trak, MP4_FOUR_CHAR_CODE('r', 'n', 'd', 'r'), 1, &videoTrack);
    if (err) {
      std::cerr << "Mebx track ID " << currentMebxTrackID << " has no 'rndr' track reference. Skip it...\n";
      MP4DisposeTrackReader(mebxReader);
      continue;
    }

    // --- Step 3.1: set key_namespace and key_value that we are looking for ---
    u32 key_namespace = MP4_FOUR_CHAR_CODE('i', 't', '3', '5');
    MP4Handle key_value = nullptr;
    err = stringToHandle(t35PrefixHex, &key_value, STRING_TO_HANDLE_MODE);
    if (err) return err;

    // --- Step 3.2: select the key ---
    u32 local_key_id = 0;
    err = MP4SelectMebxTrackReaderKey(mebxReader, key_namespace, key_value, &local_key_id);
    if(err) 
    {
      std::cerr << "MP4SelectMebxTrackReaderKey failed (err=" << err << ")\n";
      MP4DisposeHandle(key_value);
      MP4DisposeTrackReader(mebxReader);
      continue;
    }
    MP4DisposeHandle(key_value);
    std::cout << "Selected local_key_id = " << local_key_id << "\n";

    // --- Step 4: create video reader if needed ---
    u32 currentVideoTrackID = 0;
    MP4GetTrackID(videoTrack, &currentVideoTrackID);
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

    mebxTrack = trak; // It's 'mebx' with a 'rndr' reference to video
    std::cout << "Mebx track ID = " << currentMebxTrackID
              << " references video track ID = " << currentVideoTrackID << "\n";
    break;
  } // for all tracks

  if (!mebxTrack) {
    std::cerr << "No 'mebx' metadata track found\n";
    return MP4NotFoundErr;
  }

  return MP4NoErr;
}


static MP4Err extractMebxSamples(MP4Movie moov, const std::string& inputFile, const std::string& t35PrefixHex) 
{
  std::cout << "Extracting SMPTE 2094-50 metadata with prefix '" << t35PrefixHex << "'\n";

  MP4Err err = MP4NoErr;
  MP4TrackReader mebxReader = nullptr;

  // --- Step 1: get mebx track reader ---
  err = getMebxAndVideoTrackReaders(moov, &mebxReader, nullptr, t35PrefixHex);
  if (err) return err;

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
    decodeBinaryData(outFile);
  }

  std::cout << "Extracted " << mebxSampleCount << " mebx samples.\n";

  MP4DisposeTrackReader(mebxReader);
  return MP4NoErr;
}

static void writeAnnexBNAL(std::ofstream& out, const uint8_t* data, u32 size) {
  // std::cout << "Writing NALU of size " << size << " bytes\n";
  static const uint8_t startCode[4] = {0x00, 0x00, 0x00, 0x01};
  out.write((const char*)startCode, 4);
  out.write((const char*)data, size);
}

static std::vector<uint8_t> buildSeiNalu(const uint8_t* payload, u32 size, const std::string& t35PrefixHex) {
    std::vector<uint8_t> sei;

    // std::cout << "Building SEI NALU with payload size " << size << " bytes\n";

    // NAL header: forbidden_zero_bit=0, nal_unit_type=39 (prefix SEI), nuh_layer_id=0, nuh_temporal_id_plus1=1
    sei.push_back(0x00 | (39 << 1) | 0); 
    sei.push_back(0x01);

    // Build full T.35 payload = [prefix][metadata]
    MP4Handle prefixH = nullptr;
    if (stringToHandle(t35PrefixHex, &prefixH, STRING_TO_HANDLE_MODE) != MP4NoErr) {
        std::cerr << "Failed to parse T.35 prefix\n";
        return sei; // return incomplete NAL
    }
    u32 prefixSize = 0;
    MP4GetHandleSize(prefixH, &prefixSize);

    std::vector<uint8_t> fullPayload(prefixSize + size);
    memcpy(fullPayload.data(), *prefixH, prefixSize);
    memcpy(fullPayload.data() + prefixSize, payload, size);
    MP4DisposeHandle(prefixH);

    // payloadType = 4 (user_data_registered_itu_t_t35)
    sei.push_back(4);

    // payloadSize (in one byte for simplicity, assumes < 255)
    sei.push_back((uint8_t)fullPayload.size());

    // payload with emulation prevention
    for (size_t i = 0; i < fullPayload.size(); i++) {
        uint8_t b = fullPayload[i];
        sei.push_back(b);
        size_t n = sei.size();
        if (n >= 3 && sei[n - 1] <= 0x03 && sei[n - 2] == 0x00 && sei[n - 3] == 0x00) {
            sei.push_back(0x03);
        }
    }

    // rbsp_trailing_bits (10000000)
    sei.push_back(0x80);

    return sei;
}

static MP4Err dumpHevcWithMebxSei(MP4Movie moov, const std::string& inputFile, const std::string& t35PrefixHex)
{
  MP4Err err = MP4NoErr;
  MP4TrackReader mebxReader = nullptr;
  MP4TrackReader videoReader = nullptr;

  // --- Step 1: get track readers ---
  err = getMebxAndVideoTrackReaders(moov, &mebxReader, &videoReader, t35PrefixHex);
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

  // --- Step 4: setup mebx track reader ---
  u32 key_namespace = MP4_FOUR_CHAR_CODE('i', 't', '3', '5');
  MP4Handle key_value = nullptr;
  err = stringToHandle(t35PrefixHex, &key_value, STRING_TO_HANDLE_MODE);
  if (err) return err;
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

  // --- Step 5: iterate video samples and inject SEIs ---
  u64 mebxRemain = 0;
  MP4Handle mebxSampleH = nullptr;
  u32 mebxSize = 0, mebxDuration = 0;

  while (true) {
    // Get next video sample
    MP4Handle videoSampleH = nullptr;
    u32 videoSize = 0, videoFlags = 0, videoDuration = 0;
    s32 videoCTS = 0, videoDTS = 0;
    err = MP4NewHandle(0, &videoSampleH);
    if (err) break;

    err = MP4TrackReaderGetNextAccessUnitWithDuration(
              videoReader,
              videoSampleH,
              &videoSize,
              &videoFlags,
              &videoDTS,
              &videoCTS,
              &videoDuration);

    // std::cout << "Video sample: size=" << videoSize
    //           << " DTS=" << videoDTS
    //           << " CTS=" << videoCTS
    //           << " duration=" << videoDuration
    //           << "\n";
    
    if (err == MP4EOF) {
      MP4DisposeHandle(videoSampleH);
      break; // end
    }
    if (err) {
      MP4DisposeHandle(videoSampleH);
      return err;
    }

    // If no active mebx sample, fetch next one
    if (mebxRemain == 0) {
      if (mebxSampleH) {
        MP4DisposeHandle(mebxSampleH);
        mebxSampleH = nullptr;
      }
      err = MP4NewHandle(0, &mebxSampleH);
      if (err) return err;

      u32 mebxFlags = 0;
      s32 mebxCTS = 0, mebxDTS = 0;
      err = MP4TrackReaderGetNextAccessUnitWithDuration(
                mebxReader,
                mebxSampleH,
                &mebxSize,
                &mebxFlags,
                &mebxDTS,
                &mebxCTS,
                &mebxDuration);
      std::cout << "MEBX sample: size=" << mebxSize
                << " DTS=" << mebxDTS
                << " CTS=" << mebxCTS
                << " duration=" << mebxDuration
                << "\n";

      if (err == MP4EOF) {
        MP4DisposeHandle(mebxSampleH);
        mebxSampleH = nullptr;
        mebxSize = 0;
      } else if (err) {
        return err;
      } else {
        mebxRemain = mebxDuration;
      }
    }

    // If active MEBX, inject SEI before video sample
    if (mebxRemain > 0 && mebxSampleH) {
      std::vector<uint8_t> sei = buildSeiNalu((uint8_t*)*mebxSampleH, mebxSize, t35PrefixHex);
      writeAnnexBNAL(out, sei.data(), (u32)sei.size());
      mebxRemain -= videoDuration;
    }

    // Write video sample (convert length-prefix -> Annex-B)
    {
      uint8_t* src = (uint8_t*)*videoSampleH;
      uint8_t* end = src + videoSize;
      while (src + length_size <= end) {
        u32 nalLen = 0;
        for (u32 i = 0; i < length_size; i++) {
          nalLen = (nalLen << 8) | src[i];
        }
        src += length_size;
        writeAnnexBNAL(out, src, nalLen);
        src += nalLen;
      }
    }

    MP4DisposeHandle(videoSampleH);
  }

  if (mebxSampleH) MP4DisposeHandle(mebxSampleH);

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
  std::string t35PrefixHex;

  app.add_option("input", inputFile, "Input file")->required();

  // Subcommand: inject
  auto inject = app.add_subcommand("inject", "Inject metadata into MP4");
  inject->add_option("metadata", metadataFolder, "Folder with metadata")->required();
  inject->add_option("mode", mode, "Injection mode: mebx or sei")
        ->default_val("mebx")
        ->check(CLI::IsMember({"mebx", "sei"}));
  inject->add_option("--t35-prefix", t35PrefixHex, "T.35 prefix as hex string")
        ->default_val("B500900001:SMPTE-ST2094-50");

  // Subcommand: extract
  auto extract = app.add_subcommand("extract", "Extract metadata from MP4");
  extract->add_option("mode", mode, "Extraction mode: mebx or sei")
         ->default_val("mebx")
         ->check(CLI::IsMember({"mebx", "sei"}));
  extract->add_option("--t35-prefix", t35PrefixHex, "T.35 prefix as hex string")
         ->default_val("B500900001:SMPTE-ST2094-50");

  CLI11_PARSE(app, argc, argv);

  MP4Err err = MP4NoErr;
  MP4Movie moov = nullptr;

  // Open MP4
  err = MP4OpenMovieFile(&moov, inputFile.c_str(), MP4OpenMovieDebug);
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
    std::cout << "T.35 prefix     : " << t35PrefixHex << "\n";

    // Step 1: parse metadata folder
    auto items = parseMetadataFolder(metadataFolder);

    if (items.empty()) {
      std::cerr << "No metadata found in folder " << metadataFolder << "\n";
    } else {
      std::cout << "Parsed " << items.size() << " metadata items\n";
    }

    err = injectMetadata(moov, mode, items, t35PrefixHex);
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
    std::cout << "T.35 prefix     : " << t35PrefixHex << "\n";

    if (mode == "mebx") {
      err = extractMebxSamples(moov, inputFile, t35PrefixHex);
      if (err) {
        std::cerr << "Extraction failed with err=" << err << "\n";
      } else {
        std::cout << "Extraction completed successfully.\n";
      }
    } else if (mode == "sei") {
      err = dumpHevcWithMebxSei(moov, inputFile, t35PrefixHex);
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
