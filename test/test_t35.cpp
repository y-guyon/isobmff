/**
 * @file test_t35.cpp
 * @author Dimitri Podborski
 * @brief Perform checks on T.35
 * @version 0.1
 * @date 2025-04-18
 *
 * @copyright This software module was originally developed by Apple Computer, Inc. in the course of
 * development of MPEG-4. This software module is an implementation of a part of one or more MPEG-4
 * tools as specified by MPEG-4. ISO/IEC gives users of MPEG-4 free license to this software module
 * or modifications thereof for use in hardware or software products claiming conformance to MPEG-4.
 * Those intending to use this software module in hardware or software products are advised that its
 * use may infringe existing patents. The original developer of this software module and his/her
 * company, the subsequent editors and their companies, and ISO/IEC have no liability for use of
 * this software module or modifications thereof in an implementation. Copyright is not released for
 * non MPEG-4 conforming products. Apple Computer, Inc. retains full right to use the code for its
 * own purpose, assign or donate the code to a third party and to inhibit third parties from using
 * the code for non MPEG-4 conforming products. This copyright notice must be included in all copies
 * or derivative works. Copyright (c) 1999.
 *
 */

//  fix
// mdat size correction
// 5796 - 85 = 5711 (0x164F)

//  first sample size correction
//  1839 - 85 = 1754
//  0x07 0x2F -> 0x06 0xDA

#include <catch.hpp>
#include "test_helpers.h"
#include "testdataPath.h"
#include <MP4Atoms.h>

const std::string strDataPath = TESTDATA_PATH;
const std::string strTestFile = strDataPath + "/isobmff/hvc1_hdr10plus_original.mp4";

/**
 * @brief Starting point for this testing case
 *
 */
TEST_CASE("T35")
{
  std::string strT35marking = "test_t35_marking.mp4";

  /**
   * @brief Read a file with T.35 NAL Units and create a sample group that is used for marking purposes only
   *
   * create Defragmented file with NORMAL SampleToGroupBox
   * create Defragmented file with COMPACT SampleToGroupBox
   * create Defragmented file with AUTO SampleToGroupBox (smaller size is used automatically)
   *
   */
  SECTION("Check defragmentation of sample groups")
  {
    MP4Err err;
    MP4Movie moov;
    MP4Track trak;
    MP4Media media;

    err = MP4OpenMovieFile(&moov, strTestFile.c_str(), MP4OpenMovieDebug);
    err = MP4GetMovieIndTrack(moov, 1, &trak);
    err = MP4GetTrackMedia(trak, &media);

    u32 codecType = 0;
    u32 nalUnitLength = 0;
    err = MP4GetMovieIndTrackSampleEntryType(moov, 1, &codecType);
    err = MP4GetMovieIndTrackNALUnitLength(moov, 1, &nalUnitLength);
    REQUIRE(codecType == ISOHEVCSampleEntryAtomType);
    CHECK(nalUnitLength == 4);
    
    u32 sampleCnt = 0;
    err = MP4GetMediaSampleCount(media, &sampleCnt);
    CHECK(30 == sampleCnt);

    // TBD iterate through samples and look through T.35 SEI NAL Units
    // we need to build up a table that will contain
    // T.35 data and the number of sample the same data was detected in. For example data_blob1 in sample 1,2,5. data_blob2 in samples 3,4,5,6 etc.

  }
}
