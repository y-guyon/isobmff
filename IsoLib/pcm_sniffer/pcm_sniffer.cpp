/**
 * @file pcm_sniffer.cpp
 * @brief Implementation of a simple PCM sniffer tool for HEVC
 * @version 0.1
 * @date 2025-08-26
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

extern "C" {
  #include "MP4Movies.h"
  #include "MP4Atoms.h"
}

#include <iostream>
#include <fstream>
#include <string>

class PcmSniffer {
public:
    int run(const std::string& fileName) {
        ISOErr err;
        ISOMovie moov = nullptr;

        // Open movie
        err = ISOOpenMovieFile(&moov, fileName.c_str(), MP4OpenMovieNormal);
        if (err) {
            std::cerr << "Failed to open file: " << fileName << " (err=" << err << ")\n";
            return err;
        }

        u32 trackCount = 0;
        err = ISOGetMovieTrackCount(moov, &trackCount);
        if (err) return err;

        std::cout << "Movie has " << trackCount << " tracks\n";

        for (u32 trackNumber = 1; trackNumber <= trackCount; ++trackNumber) {
            ISOTrack trak = nullptr;
            err = ISOGetMovieIndTrack(moov, trackNumber, &trak);
            if (err) continue;

            // Create track reader
            ISOTrackReader reader = nullptr;
            err = ISOCreateTrackReader(trak, &reader);
            if (err) continue;

            // Get sample entry
            ISOHandle sampleEntryH;
            ISONewHandle(0, &sampleEntryH);
            err = MP4TrackReaderGetCurrentSampleDescription(reader, sampleEntryH);
            if (err) { ISODisposeHandle(sampleEntryH); continue; }

            u32 sampleEntryType = 0;
            ISOGetSampleDescriptionType(sampleEntryH, &sampleEntryType);

            char typeStr[5] = {0};
            MP4TypeToString(sampleEntryType, typeStr);
            std::cout << "Track " << trackNumber << " sample entry type: " << typeStr << "\n";

            if (sampleEntryType == ISOHEVCSampleEntryAtomType) {
                std::cout << " -> HEVC track, extracting parameter sets\n";

                ISOHandle nalusH;
                ISONewHandle(0, &nalusH);

                // mode=0 → get VPS, SPS, PPS all together
                err = ISOGetHEVCNALUs(sampleEntryH, nalusH, 0);
                if (!err) {
                    u32 size = 0;
                    MP4GetHandleSize(nalusH, &size);
                    std::cout << "Extracted " << size << " bytes of NALUs in AnnexB\n";

                    if (size > 0) {
                        // Dump to file
                        std::string outName = "track_" + std::to_string(trackNumber) + "_ps.bin";
                        std::ofstream out(outName, std::ios::binary);
                        if (out) {
                            out.write(*nalusH, size);
                            std::cout << " -> Wrote parameter sets to " << outName << "\n";
                        } else {
                            std::cerr << " -> Failed to open " << outName << " for writing\n";
                        }
                    }
                } else {
                    std::cerr << "ISOGetHEVCNALUs failed with " << err << "\n";
                }

                ISODisposeHandle(nalusH);
            }

            ISODisposeHandle(sampleEntryH);
            MP4DisposeTrackReader(reader);
        }

        ISODisposeMovie(moov);
        return 0;
    }
};

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: pcm_sniffer <file.mp4>\n";
        return 1;
    }
    PcmSniffer sniffer;
    return sniffer.run(argv[1]);
}
