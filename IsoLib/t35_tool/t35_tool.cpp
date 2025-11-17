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
#include <cstdio>
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

constexpr bool STRING_TO_HANDLE_MODE = false;   // true = hex, false = text

const float  PI_CUSTOM = 3.14159265358979323846;

const int MAX_NB_ALTERNATE = 4;
const int MAX_NB_CONTROL_POINTS = 32;
const int MAX_NB_CHROMATICITIES = 8;
const int MAX_NB_MIX_PARAMS = 6;

// Compute quantization error of each float to uint16_t
const float Q_HDR_REFERENCE_WHITE = 50000.0 / 10000.0;
const float Q_HDR_HEADROOM   = 60000.0 / 6.0;
const float P_GAIN_APPLICATION_SPACE_CHROMATICITY = 3.0 / 30000.0 / 2.0;
const float Q_GAIN_APPLICATION_SPACE_CHROMATICITY = 30000.0 / 3.0; 
const float P_GAIN_APPLICATION_OFFSET = 4.0 / 40000.0 / 2.0; 
const float Q_GAIN_APPLICATION_OFFSET = 40000.0 / 4.0; 
const float P_MIX_PARAMS = 1.0 / 10000.0 / 2.0;
const float Q_MIX_PARAMS = 10000.0; 
const float Q_GAIN_CURVE_CONTROL_POINT_X = 64000.0 / 64.0; 
const float O_GAIN_CURVE_CONTROL_POINT_Y = 6.0; 
const float Q_GAIN_CURVE_CONTROL_POINT_Y = 48000.0 / 12.0; 
const float O_GAIN_CURVE_CONTROL_POINT_THETA = 90.0; 
const float Q_GAIN_CURVE_CONTROL_POINT_THETA = 36000.0 / 180.0; 

// Usage shortcuts:
// - mkdir mybuild && cd mybuild
// - cmake ..
// - make t35_tool -j
// - cp ~/CodeApple/DimitriPodborski/ISOBMMF/isobmff-internal/bin/t35_tool ~/CodeApple/DimitriPodborski/
// - cd ~/CodeApple/DimitriPodborski/
// -  ./t35_tool <movie_file> inject <Metadata_folder> mebx
// -  ./t35_tool 160299415_SMPTE2094-50_MetadataVideo.mov inject 2025-09-22_mebx_MetadataExample mebx
// -  ./t35_tool 160299415_SMPTE2094-50_MetadataVideo.mov_mebx.mp4 extract mebx

// ./t35_tool 160299415_SMPTE2094-50_MetadataVideo.mov inject TestVariousMetadataType mebx
// ./t35_tool 160299415_SMPTE2094-50_MetadataVideo.mov_mebx.mp4 extract mebx

// ./t35_tool 160299415_SMPTE2094-50_MetadataVideo_Duration100Percent_IMG522.mov inject ImageToneMapping mebx --t35-prefix 'B500900001:SMPTE-ST2094-50'
// ./t35_tool 160299415_SMPTE2094-50_MetadataVideo_Duration100Percent_IMG522.mov_mebx.mp4 extract mebx
// To verify presence of metatada track in file:
// moovscope -dumpmebx -dumpmebxdata <filename>

struct MetadataItem {
  uint32_t frame_start;
  uint32_t frame_duration;
  std::string bin_path;
};


struct MetadataItems {
  bool error_raised;
  // get mandatory metadata
  uint32_t windowNumber     ;
  float hdrReferenceWhite ;
  uint32_t toneMapMode    ;

  // pre-allocate data
  float baselineHdrHeadroom;
  uint32_t numAlternateImages ;
  std::vector<float> gainApplicationSpaceChromaticities;
  float gainApplicationOffset;
  std::vector<float> alternateHdrHeadroom;

  std::vector<float> componentMixRed;
  std::vector<float> componentMixGreen;
  std::vector<float> componentMixBlue;
  std::vector<float> componentMixMax;
  std::vector<float> componentMixMin;
  std::vector<float> componentMixComponent;

  std::vector<uint32_t> gainCurveInterpolation;
  std::vector<uint32_t> gainCurveNumControlPoints;

  std::vector<std::vector<float>> gainCurveControlPointX;
  std::vector<std::vector<float>> gainCurveControlPointY;
  std::vector<std::vector<float>> gainCurveControlPointTheta;
};

struct SyntaxElements {
  uint16_t application_major_version;
  uint16_t application_minor_version;
  uint16_t num_windows_minus_1;        
   
  // aom_smpte_st_2094_50_window_info
  bool has_hdr_reference_white_flag;        
  uint16_t tone_map_mode;                                           
  uint16_t hdr_reference_white ;                  
  uint16_t baseline_hdr_headroom ;                

  uint16_t num_alternate_images_minus_1;          
  uint16_t gain_application_space_primaries;      
  bool has_gain_application_offset_flag;      
  bool has_common_mix_params_flag;            
  bool has_common_curve_params_flag;  
  uint16_t gain_application_space_chromaticities[MAX_NB_CHROMATICITIES];
  uint16_t gain_application_offset;                

  // alternateRepresentation
  uint16_t  alternate_hdr_headrooms[MAX_NB_ALTERNATE];                

  // component_mix_parametrization
  uint16_t  mix_encoding[MAX_NB_ALTERNATE];                            
  uint16_t  mix_params[MAX_NB_ALTERNATE][MAX_NB_MIX_PARAMS];

  // Interpolation
  uint16_t  gain_curve_interpolation[MAX_NB_ALTERNATE];
  uint16_t  gain_curve_num_control_points_minus_1[MAX_NB_ALTERNATE];
  // Control points
  uint16_t  gain_curve_control_points_x[MAX_NB_ALTERNATE][MAX_NB_CONTROL_POINTS];         
  uint16_t  gain_curve_control_points_y[MAX_NB_ALTERNATE][MAX_NB_CONTROL_POINTS];            
  uint16_t  gain_curve_control_points_theta[MAX_NB_ALTERNATE][MAX_NB_CONTROL_POINTS];     
};

// key = starting frame number
using MetadataMap = std::map<uint32_t, MetadataItem>;

/* *********************************** UTILITY FUNCTIONS *******************************************************************************************/

// Convert uint8 to heaxadecimal value
std::string uint8_to_hex(uint8_t value) {
  std::stringstream ss;
  ss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(value);
  return ss.str();
}

// Debug print-out for specific milestone in code
void dbgPrintMilestones(uint16_t line, const std::string& msg, float value){
  std::cout <<  "[RB: " << line << " " << msg << "=" << value << "]\n";
}

// Formatted cout of a name and a value aligned for binary encoding and decoding debugging
void printDebug(const std::string& varName, uint16_t varValue, uint8_t nbBits) {
  std::cout.width(50); std::cout << varName << "=";
  std::cout.width(6); std::cout  << varValue << " | ";
  switch (nbBits) { // bitset need constant
    case 1:
      std::cout.width(16); std::cout <<  std::bitset<1>(varValue).to_string() << "\n";
      break;
    case 2:
      std::cout.width(16); std::cout <<  std::bitset<2>(varValue).to_string() << "\n";
      break;
    case 3:
      std::cout.width(16); std::cout <<  std::bitset<3>(varValue).to_string() << "\n";
      break;
    case 4:
      std::cout.width(16); std::cout <<  std::bitset<4>(varValue).to_string() << "\n";
      break;
    case 5:
      std::cout.width(16); std::cout <<  std::bitset<5>(varValue).to_string() << "\n";
      break;
    case 6:
      std::cout.width(16); std::cout <<  std::bitset<6>(varValue).to_string() << "\n";
      break;
    case 7:
      std::cout.width(16); std::cout <<  std::bitset<7>(varValue).to_string() << "\n";
      break;
    case 8:
      std::cout.width(16); std::cout <<  std::bitset<8>(varValue).to_string() << "\n";
      case 16:
      std::cout.width(16); std::cout <<  std::bitset<16>(varValue).to_string() << "\n";
      break;
  default:
      break;
  }
}

void dbgPrintMetadataItems(MetadataItems itm, bool decode) {
  std::cout << "============================================= METADATA ITEMS ========================================================================\n";
  std::cout <<"windowNumber=" << itm.windowNumber << "\n";
  std::cout <<"hdrReferenceWhite=" << itm.hdrReferenceWhite << "\n";
  std::cout <<"toneMapMode=" << itm.toneMapMode << "\n";
  std::cout <<"baselineHdrHeadroom=" << itm.baselineHdrHeadroom << "\n";
  if (itm.toneMapMode == 4 || decode) {
    std::cout <<"numAlternateImages=" << itm.numAlternateImages << "\n";

    std::cout << "gainApplicationSpaceChromaticities=[" ;
    for (float val : itm.gainApplicationSpaceChromaticities) {
      std::cout << val << ", ";
    }
    std::cout << "]" << std::endl;
    std::cout <<"gainApplicationOffset=" << itm.gainApplicationOffset << "\n";

    for (uint32_t iAlt = 0; iAlt < itm.numAlternateImages; iAlt++) {
      std::cout <<"alternateHdrHeadroom=" << itm.alternateHdrHeadroom[iAlt] << "\n";
      std::cout <<"componentMixRed=" << itm.componentMixRed[iAlt] << "\n";
      std::cout <<"componentMixGreen=" << itm.componentMixGreen[iAlt] << "\n";
      std::cout <<"componentMixBlue=" << itm.componentMixBlue[iAlt] << "\n";
      std::cout <<"componentMixMax=" << itm.componentMixMax[iAlt] << "\n";
      std::cout <<"componentMixMin=" << itm.componentMixMin[iAlt] << "\n";
      std::cout <<"componentMixComponent=" << itm.componentMixComponent[iAlt] << "\n";

      std::cout <<"gainCurveInterpolation=" << itm.gainCurveInterpolation[iAlt] << "\n";
      std::cout <<"gainCurveNumControlPoints=" << itm.gainCurveNumControlPoints[iAlt] << "\n";

      std::cout << "gainCurveControlPointX=[" << std::endl;
      for (float val : itm.gainCurveControlPointX[iAlt]) {
          std::cout << val << ", ";
      }
      std::cout << "]" << std::endl;

      std::cout << "gainCurveControlPointY=[" << std::endl;
      for (float val : itm.gainCurveControlPointY[iAlt]) {
          std::cout << val << ", ";
      }
      std::cout << "]" << std::endl;

      std::cout << "gainCurveControlPointTheta=[" << std::endl;
      for (float val : itm.gainCurveControlPointTheta[iAlt]) {
          std::cout << val << ", ";
      }
      std::cout << "]" << std::endl;
    }
  }
  std::cout << "===================================================================================================================================]\n";
}

/* *********************************** ENCODING SECTION ********************************************************************************************/

// Read from json file the metadata items
MetadataItems decodeJsonToMetadataItems(nlohmann::json j, const std::string& path) {
  MetadataItems itm;
  itm.error_raised = false;

  std::cout << "++++++++++++++++++++++ DECODE JSON TO METADATA ITEMS **++++++++++++++++++++++++++++++*********************************************]\n";

  // Checking mandatory metadata
  if (!j.contains("windowNumber") || !j.contains("hdrReferenceWhite") || !j.contains("toneMapMode")) {
    std::cerr << "Skipping " << path << " (missing mandatory keys: windowNumber | hdrReferenceWhite | toneMapMode)\n";
    itm.error_raised = false; return itm;
  }
  // ================================================= Decode Metadata Items ===================================
  // get mandatory metadata
  itm.windowNumber      = j["windowNumber"].get<uint32_t>();
  itm.hdrReferenceWhite = j["hdrReferenceWhite"].get<uint32_t>();
  itm.toneMapMode       = j["toneMapMode"].get<uint32_t>();


  if (itm.toneMapMode > 0 && j.contains("baselineHdrHeadroom") ) // baseline_hdr_headroom should exist
  {
    itm.baselineHdrHeadroom = j["baselineHdrHeadroom"].get<float>();
    if (itm.toneMapMode == 4) // Custom Headroom Adaptive Tone Mapping mode
    {
      // Do separate check for better error handling
      if (!j.contains("numAlternateImages")) {
        std::cerr << "Skipping " << path << " (numAlternateImages metadata item missing )\n";
        itm.error_raised = false; return itm;
      }
      if (!j.contains("gainApplicationSpaceChromaticities")) {
        std::cerr << "Skipping " << path << " (gainApplicationSpaceChromaticities metadata item missing )\n";
        itm.error_raised = false; return itm;
      }
      if (!j.contains("gainApplicationOffset")) {
        std::cerr << "Skipping " << path << " (gainApplicationOffset metadata item missing )\n";
        itm.error_raised = false; return itm;
      }
      if (!j.contains("alternateHdrHeadroom")) {
        std::cerr << "Skipping " << path << " (alternateHdrHeadroom metadata item missing )\n";
        itm.error_raised = false; return itm;
      }

      if (!j.contains("componentMixRed")) {
        std::cerr << "Skipping " << path << " (componentMixRed metadata item missing )\n";
        itm.error_raised = false; return itm;
      }
      if (!j.contains("componentMixGreen")) {
        std::cerr << "Skipping " << path << " (componentMixGreen metadata item missing )\n";
        itm.error_raised = false; return itm;
      }
      if (!j.contains("componentMixBlue")) {
        std::cerr << "Skipping " << path << " (componentMixBlue metadata item missing )\n";
        itm.error_raised = false; return itm;
      }
      if (!j.contains("componentMixMax")) {
        std::cerr << "Skipping " << path << " (componentMixMax metadata item missing )\n";
        itm.error_raised = false; return itm;
      }
      if (!j.contains("componentMixMin")) {
        std::cerr << "Skipping " << path << " (componentMixMin metadata item missing )\n";
        itm.error_raised = false; return itm;
      }
      if (!j.contains("componentMixComponent")) {
        std::cerr << "Skipping " << path << " (componentMixComponent metadata item missing )\n";
        itm.error_raised = false; return itm;
      }

      if (!j.contains("gainCurveInterpolation")) {
        std::cerr << "Skipping " << path << " (gainCurveInterpolation metadata item missing )\n";
        itm.error_raised = false; return itm;
      }
      if (!j.contains("gainCurveNumControlPoints")) {
        std::cerr << "Skipping " << path << " (gainCurveNumControlPoints metadata item missing )\n";
        itm.error_raised = false; return itm;
      }
      if (!j.contains("gainCurveControlPointX")) {
        std::cerr << "Skipping " << path << " (gainCurveControlPointX metadata item missing )\n";
        itm.error_raised = false; return itm;
      }
      if (!j.contains("gainCurveControlPointY")) {
        std::cerr << "Skipping " << path << " (gainCurveControlPointY metadata item missing )\n";
        itm.error_raised = false; return itm;
      }

      dbgPrintMilestones(338, "check json is good", 0);
      
      // Decode headroom adaptive metadata
      itm.numAlternateImages                 = j["numAlternateImages"].get<uint32_t>();
      itm.gainApplicationSpaceChromaticities = j["gainApplicationSpaceChromaticities"].get<std::vector<float>>();
      itm.gainApplicationOffset              = j["gainApplicationOffset"].get<float>();
      itm.alternateHdrHeadroom               = j["alternateHdrHeadroom"].get<std::vector<float>>();

      dbgPrintMilestones(359, "check decoding is good", 4);

      itm.componentMixRed       = j["componentMixRed"].get<std::vector<float>>();
      itm.componentMixGreen     = j["componentMixGreen"].get<std::vector<float>>();
      itm.componentMixBlue      = j["componentMixBlue"].get<std::vector<float>>();
      itm.componentMixMax       = j["componentMixMax"].get<std::vector<float>>();
      itm.componentMixMin       = j["componentMixMin"].get<std::vector<float>>();
      itm.componentMixComponent = j["componentMixComponent"].get<std::vector<float>>();

      itm.gainCurveInterpolation       = j["gainCurveInterpolation"].get<std::vector<uint32_t>>();
      itm.gainCurveNumControlPoints     = j["gainCurveNumControlPoints"].get<std::vector<uint32_t>>();

      itm.gainCurveControlPointX = j["gainCurveControlPointX"].get<std::vector<std::vector<float>>>();
      itm.gainCurveControlPointY = j["gainCurveControlPointY"].get<std::vector<std::vector<float>>>();

      dbgPrintMilestones(359, "check decoding is good", 0);
      // Check if there is theta
      bool gainCurveControlPointTheta_is_present = false;
      for (uint32_t iAlt = 0; iAlt < itm.numAlternateImages; iAlt++) {
        if (itm.gainCurveInterpolation[iAlt] == 2){
          gainCurveControlPointTheta_is_present = true;
          break;
        }
      }
      if (gainCurveControlPointTheta_is_present)
      {
        if (!j.contains("gainCurveControlPointTheta")) {
          std::cerr << "File " << path << " (gainCurveInterpolation = 2 but gainCurveControlPointTheta is missing)\n";
          itm.error_raised = false; return itm;
        }
        itm.gainCurveControlPointTheta = j["gainCurveControlPointTheta"].get<std::vector<std::vector<float>>>();
      }
    }
  }
  else
  {
    std::cerr << "Skipping " << path << " (toneMapMode="  << itm.toneMapMode << " but baselineHdrHeadroom missing.)\n";
      itm.error_raised = false; return itm;
  }
  return itm;

}

// Convert metadata items to syntax elements 
SyntaxElements convertMetadataItemsToSyntaxElements(MetadataItems itm){

  SyntaxElements elm;

  // Init the application version:
  elm.application_major_version = 15;
  elm.application_minor_version = 15;
  elm.num_windows_minus_1       = 0;


  if (abs(itm.hdrReferenceWhite - 203.0) > Q_HDR_REFERENCE_WHITE / 2) {
    elm.has_hdr_reference_white_flag = true;
    elm.hdr_reference_white = uint16_t(itm.hdrReferenceWhite * Q_HDR_REFERENCE_WHITE);
  }
  elm.tone_map_mode = itm.toneMapMode;  
  if (itm.toneMapMode != 0){
    elm.baseline_hdr_headroom = uint16_t(itm.baselineHdrHeadroom * Q_HDR_HEADROOM);
  }
  if (itm.toneMapMode == 4){
    elm.num_alternate_images_minus_1 = uint16_t(itm.numAlternateImages - 1);
    // Check if the primary combination is known
    if (
      (itm.gainApplicationSpaceChromaticities[0] - 0.64) <  P_GAIN_APPLICATION_SPACE_CHROMATICITY &&
      (itm.gainApplicationSpaceChromaticities[1] - 0.33 ) <  P_GAIN_APPLICATION_SPACE_CHROMATICITY &&
      (itm.gainApplicationSpaceChromaticities[2] - 0.3 ) <  P_GAIN_APPLICATION_SPACE_CHROMATICITY &&
      (itm.gainApplicationSpaceChromaticities[3] - 0.6) <  P_GAIN_APPLICATION_SPACE_CHROMATICITY &&
      (itm.gainApplicationSpaceChromaticities[4] - 0.15 ) <  P_GAIN_APPLICATION_SPACE_CHROMATICITY &&
      (itm.gainApplicationSpaceChromaticities[5] - 0.06 ) <  P_GAIN_APPLICATION_SPACE_CHROMATICITY &&
      (itm.gainApplicationSpaceChromaticities[6] - 0.3127) <  P_GAIN_APPLICATION_SPACE_CHROMATICITY &&
      (itm.gainApplicationSpaceChromaticities[7] - 0.329) < P_GAIN_APPLICATION_SPACE_CHROMATICITY){ 
        elm.gain_application_space_primaries = 0;
      } 
      else if (
        (itm.gainApplicationSpaceChromaticities[0] - 0.68 ) <  P_GAIN_APPLICATION_SPACE_CHROMATICITY &&
        (itm.gainApplicationSpaceChromaticities[1] - 0.32 ) <  P_GAIN_APPLICATION_SPACE_CHROMATICITY &&
        (itm.gainApplicationSpaceChromaticities[2] - 0.265 ) <  P_GAIN_APPLICATION_SPACE_CHROMATICITY &&
        (itm.gainApplicationSpaceChromaticities[3] - 0.69 ) <  P_GAIN_APPLICATION_SPACE_CHROMATICITY &&
        (itm.gainApplicationSpaceChromaticities[4] - 0.15 ) <  P_GAIN_APPLICATION_SPACE_CHROMATICITY &&
        (itm.gainApplicationSpaceChromaticities[5] - 0.06 ) <  P_GAIN_APPLICATION_SPACE_CHROMATICITY &&
        (itm.gainApplicationSpaceChromaticities[6] - 0.3127 ) <  P_GAIN_APPLICATION_SPACE_CHROMATICITY &&
        (itm.gainApplicationSpaceChromaticities[7] - 0.329) < Q_GAIN_APPLICATION_SPACE_CHROMATICITY ) //[Todo: verify chromaticities value]
      { 
        elm.gain_application_space_primaries = 1;
      } else if (
        (itm.gainApplicationSpaceChromaticities[0] - 0.708) <  P_GAIN_APPLICATION_SPACE_CHROMATICITY &&
        (itm.gainApplicationSpaceChromaticities[1] - 0.292) <  P_GAIN_APPLICATION_SPACE_CHROMATICITY &&
        (itm.gainApplicationSpaceChromaticities[2] - 0.17) <  P_GAIN_APPLICATION_SPACE_CHROMATICITY &&
        (itm.gainApplicationSpaceChromaticities[3] - 0.797) <  P_GAIN_APPLICATION_SPACE_CHROMATICITY &&
        (itm.gainApplicationSpaceChromaticities[4] - 0.131) <  P_GAIN_APPLICATION_SPACE_CHROMATICITY &&
        (itm.gainApplicationSpaceChromaticities[5] - 0.046) <  P_GAIN_APPLICATION_SPACE_CHROMATICITY &&
        (itm.gainApplicationSpaceChromaticities[6] - 0.3127) <  P_GAIN_APPLICATION_SPACE_CHROMATICITY &&
        (itm.gainApplicationSpaceChromaticities[7] - 0.329) < P_GAIN_APPLICATION_SPACE_CHROMATICITY){ 
          elm.gain_application_space_primaries = 2;
      } else {
        elm.gain_application_space_primaries = 3;
        for (uint16_t iCh = 0; iCh < 8; iCh++) {
          elm.gain_application_space_chromaticities[iCh] = uint16_t(itm.gainApplicationSpaceChromaticities[iCh]* Q_GAIN_APPLICATION_SPACE_CHROMATICITY);
        }
      }
      if (abs(itm.gainApplicationOffset) > P_GAIN_APPLICATION_OFFSET) {
        elm.has_gain_application_offset_flag = true;
        elm.gain_application_offset = uint16_t(itm.gainApplicationOffset * Q_GAIN_APPLICATION_OFFSET);
      }

      // Check if all component mixing uses the same parameters 
      elm.has_common_mix_params_flag = true; // Init at true, any mismatch makes it false
      for (uint16_t iAlt = 1; iAlt < itm.numAlternateImages; iAlt++) {
        // Check if any coefficient is different across alterante images
        if (abs(itm.componentMixRed[0]       - itm.componentMixRed[iAlt])       > Q_MIX_PARAMS || 
            abs(itm.componentMixGreen[0]     - itm.componentMixGreen[iAlt])     > Q_MIX_PARAMS || 
            abs(itm.componentMixBlue[0]      - itm.componentMixBlue[iAlt])      > Q_MIX_PARAMS || 
            abs(itm.componentMixMax[0]       - itm.componentMixMax[iAlt])       > Q_MIX_PARAMS || 
            abs(itm.componentMixMin[0]       - itm.componentMixMin[iAlt])       > Q_MIX_PARAMS || 
            abs(itm.componentMixComponent[0] - itm.componentMixComponent[iAlt]) > Q_MIX_PARAMS) {
          elm.has_common_mix_params_flag = false;
          break;
        }
      } 
      // Check if all alternate have the same number of control points and interpolation
      elm.has_common_curve_params_flag = true;  // Init at true, any mismatch makes it false
      for (uint16_t iAlt = 1; iAlt < itm.numAlternateImages; iAlt++) {
        if (itm.gainCurveInterpolation[0] != itm.gainCurveInterpolation[iAlt] || itm.gainCurveNumControlPoints[0] != itm.gainCurveNumControlPoints[iAlt]){
          elm.has_common_mix_params_flag = false;
          break;
        }
      }

      // If they do, then check if all alternate have the same x position
      for (uint16_t pIdx = 0; pIdx < itm.gainCurveNumControlPoints[0]; pIdx++){
        for (uint16_t iAlt = 1; iAlt < itm.numAlternateImages; iAlt++) {
          if (itm.gainCurveControlPointX[0][pIdx] - itm.gainCurveControlPointX[iAlt][pIdx] > Q_GAIN_CURVE_CONTROL_POINT_X){
            elm.has_common_mix_params_flag = false;
            break;
          }
        }
      } 
      
      // Loop over alternate images
      for (uint16_t iAlt = 0; iAlt < itm.numAlternateImages; iAlt++) {
        elm.alternate_hdr_headrooms[iAlt] = uint16_t(itm.alternateHdrHeadroom[iAlt] * Q_HDR_HEADROOM);

        // init coefficient to 0
        for (uint16_t iCmf = 0; iCmf < MAX_NB_MIX_PARAMS; iCmf++){
          elm.mix_params[iAlt][iCmf] = uint16_t(0);
        }
        // Component mixing
        if (
          abs(itm.componentMixRed[iAlt]      ) < P_MIX_PARAMS &&
          abs(itm.componentMixGreen[iAlt]    ) < P_MIX_PARAMS &&
          abs(itm.componentMixBlue[iAlt]     ) < P_MIX_PARAMS &&
          abs(itm.componentMixMax[iAlt]- 1.0 ) < P_MIX_PARAMS &&
          abs(itm.componentMixMin[iAlt]      ) < P_MIX_PARAMS &&
          abs(itm.componentMixComponent[iAlt]) < P_MIX_PARAMS){ 
            elm.mix_encoding[iAlt] = 0;
        } else if (
          abs(itm.componentMixRed[iAlt]      ) < P_MIX_PARAMS &&
          abs(itm.componentMixGreen[iAlt]    ) < P_MIX_PARAMS &&
          abs(itm.componentMixBlue[iAlt]     ) < P_MIX_PARAMS &&
          abs(itm.componentMixMax[iAlt]      ) < P_MIX_PARAMS &&
          abs(itm.componentMixMin[iAlt]      ) < P_MIX_PARAMS &&
          abs(itm.componentMixComponent[iAlt] - 1.0 ) < P_MIX_PARAMS){ 
            elm.mix_encoding[iAlt] = 1;
        }  else if (
          abs(itm.componentMixRed[iAlt]   - (1.0 / 6.0)) < P_MIX_PARAMS &&
          abs(itm.componentMixGreen[iAlt] - (1.0 / 6.0)) < P_MIX_PARAMS &&
          abs(itm.componentMixBlue[iAlt]  - (1.0 / 6.0)) < P_MIX_PARAMS &&
          abs(itm.componentMixMax[iAlt]   - (1.0 / 2.0)) < P_MIX_PARAMS &&
          abs(itm.componentMixMin[iAlt]      ) < P_MIX_PARAMS &&
          abs(itm.componentMixComponent[iAlt]) < P_MIX_PARAMS){ 
            elm.mix_encoding[iAlt] = 2;
        }  else if (
          abs(itm.componentMixGreen[iAlt] -  itm.componentMixRed[iAlt] ) < P_MIX_PARAMS && //[Todo: is that enough or should we quantize red and then compare?]
          abs(itm.componentMixBlue[iAlt]  -  itm.componentMixRed[iAlt] ) < P_MIX_PARAMS &&
          abs(itm.componentMixMax[iAlt]   + 3.0 *  itm.componentMixBlue[iAlt] - 1.0) < P_MIX_PARAMS &&
          abs(itm.componentMixMin[iAlt]      ) < P_MIX_PARAMS &&
          abs(itm.componentMixComponent[iAlt]) < P_MIX_PARAMS){ 
            elm.mix_encoding[iAlt] = 3;
            elm.mix_params[iAlt][0] = uint16_t(itm.componentMixRed[iAlt] * 3.0 * P_MIX_PARAMS);
        }  else if (
          abs(itm.componentMixRed[iAlt]      ) < P_MIX_PARAMS &&
          abs(itm.componentMixGreen[iAlt]    ) < P_MIX_PARAMS &&
          abs(itm.componentMixBlue[iAlt]     ) < P_MIX_PARAMS &&
             (itm.componentMixMax[iAlt]      ) > P_MIX_PARAMS &&
          abs(itm.componentMixMin[iAlt]      ) < P_MIX_PARAMS &&
          abs(itm.componentMixComponent[iAlt] + itm.componentMixMax[iAlt] - 1.0) < P_MIX_PARAMS){ 
            elm.mix_encoding[iAlt] = 4;
            elm.mix_params[iAlt][0] = uint16_t(itm.componentMixMax[iAlt] * 3.0 * Q_MIX_PARAMS);
        }  else if ( //[Todo: do we allow 0 in cases they are signaled?]
          (itm.componentMixRed[iAlt]      ) < P_MIX_PARAMS &&     
          (itm.componentMixGreen[iAlt]    ) < P_MIX_PARAMS &&
          (itm.componentMixBlue[iAlt]  + itm.componentMixGreen[iAlt] + itm.componentMixRed[iAlt] - 1.0) < P_MIX_PARAMS &&
          abs(itm.componentMixMax[iAlt]      ) < P_MIX_PARAMS &&
          abs(itm.componentMixMin[iAlt]      ) < P_MIX_PARAMS &&
          abs(itm.componentMixComponent[iAlt]) < P_MIX_PARAMS){ 
            elm.mix_encoding[iAlt] = 5;
            elm.mix_params[iAlt][0] = uint16_t(itm.componentMixRed[iAlt]   * Q_MIX_PARAMS);
            elm.mix_params[iAlt][1] = uint16_t(itm.componentMixGreen[iAlt] * Q_MIX_PARAMS);
        }  else if (
          abs(itm.componentMixGreen[iAlt] -  itm.componentMixRed[iAlt] ) < P_MIX_PARAMS && 
          abs(itm.componentMixBlue[iAlt]  -  itm.componentMixRed[iAlt] ) < P_MIX_PARAMS &&
          (itm.componentMixComponent[iAlt] + itm.componentMixRed[iAlt] * 3.0 + itm.componentMixMax[iAlt] + itm.componentMixMin[iAlt] - 1.0 ) < P_MIX_PARAMS){ 
            elm.mix_encoding[iAlt] = 6;
            elm.mix_params[iAlt][0] = uint16_t(itm.componentMixRed[iAlt] * 3.0 * Q_MIX_PARAMS);
            elm.mix_params[iAlt][1] = uint16_t(itm.componentMixMax[iAlt] * Q_MIX_PARAMS);
            elm.mix_params[iAlt][2] = uint16_t(itm.componentMixMin[iAlt] * Q_MIX_PARAMS);
        }   else if (
          (itm.componentMixMin[iAlt] + itm.componentMixRed[iAlt] + itm.componentMixGreen[iAlt] + itm.componentMixBlue[iAlt] + itm.componentMixMax[iAlt] - 1.0 ) < P_MIX_PARAMS &&
          abs(itm.componentMixComponent[iAlt]) < P_MIX_PARAMS){ 
            elm.mix_encoding[iAlt] = 7;
            elm.mix_params[iAlt][0] = uint16_t(itm.componentMixRed[iAlt]   * Q_MIX_PARAMS);
            elm.mix_params[iAlt][1] = uint16_t(itm.componentMixGreen[iAlt] * Q_MIX_PARAMS);
            elm.mix_params[iAlt][2] = uint16_t(itm.componentMixBlue[iAlt]  * Q_MIX_PARAMS);
            elm.mix_params[iAlt][3] = uint16_t(itm.componentMixMax[iAlt]   * Q_MIX_PARAMS);
        } else {
          std::cerr << "Component mixing not recognized for alternate[" << iAlt << "] with coefficients: [" <<
          itm.componentMixRed[iAlt] << ", " <<
          itm.componentMixGreen[iAlt] << ", " <<
          itm.componentMixBlue[iAlt] << ", " <<
          itm.componentMixMax[iAlt] << ", " <<
          itm.componentMixMin[iAlt] << ", " <<
          itm.componentMixComponent[iAlt] << "]\n";
          continue;
        }

        // Create syntax elements for the gain curve function
        elm.gain_curve_interpolation[iAlt] = itm.gainCurveInterpolation[iAlt];
        elm.gain_curve_num_control_points_minus_1[iAlt] = itm.gainCurveNumControlPoints[iAlt] - 1;
        for (uint16_t iCps = 0; iCps < itm.gainCurveNumControlPoints[iAlt]; iCps++){
          elm.gain_curve_control_points_x[iAlt][iCps] = uint16_t(itm.gainCurveControlPointX[iAlt][iCps] * Q_GAIN_CURVE_CONTROL_POINT_X );
        }
        for (uint16_t iCps = 0; iCps < itm.gainCurveNumControlPoints[iAlt]; iCps++) {
          elm.gain_curve_control_points_y[iAlt][iCps] = uint16_t((itm.gainCurveControlPointY[iAlt][iCps]  + O_GAIN_CURVE_CONTROL_POINT_Y ) * Q_GAIN_CURVE_CONTROL_POINT_Y );
        }
        if (itm.gainCurveInterpolation[iAlt] == 2) {
          for (uint16_t iCps = 0; iCps < itm.gainCurveNumControlPoints[iAlt]; iCps++) {
            elm.gain_curve_control_points_theta[iAlt][iCps] = uint16_t((itm.gainCurveControlPointTheta[iAlt][iCps] + O_GAIN_CURVE_CONTROL_POINT_THETA) * Q_GAIN_CURVE_CONTROL_POINT_THETA );
          }
        }
      }
    }
    return elm;
  }

// Convert syntax element to finary data and write to file
void writeSyntaxElementsToBinaryData(SyntaxElements elm, const std::string& binPathGen){
  // ================================================= Convert binary data from Syntax Elements ===================================
  //const char* filename = "output.bin"; // The name of the binary file
  // Open the file in binary output mode
  std::ofstream outFile(binPathGen, std::ios::out | std::ios::binary);
  
  if (outFile.is_open()) {
    uint8_t value_8; 
    const int kMixEncodingNumParams[8] = {0, 0, 0, 1, 1 ,2, 3, 4};
    // Write the uint16_t value directly to the file
    // Reinterpret_cast is used to treat the memory location of 'value' as a sequence of bytes
    
    //[Todo: for some reason I have less bytes for the test json (58 versus 60)]
    //[Todo: binary from matlab seems to be different endian than what this tool does. To Verify]
    std::cout << "++++++++++++++++++++++ Write binary data to intermediate file ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n";  
    printDebug("application_major_version",elm.application_major_version, 4);
    printDebug("application_minor_version",elm.application_minor_version, 4);
    value_8 = (elm.application_major_version << 4) + elm.application_minor_version; 
    outFile.write(reinterpret_cast<const char*>(&value_8), sizeof(value_8));
    
    printDebug("num_windows_minus_1=",elm.num_windows_minus_1, 4);
    value_8 = (elm.num_windows_minus_1 << 4);
    outFile.write(reinterpret_cast<const char*>(&value_8), sizeof(value_8));

    printDebug("has_hdr_reference_white_flag",elm.has_hdr_reference_white_flag, 1);
    printDebug("tone_map_mode",elm.tone_map_mode, 3);
    value_8 = (elm.has_hdr_reference_white_flag << 7) + (elm.tone_map_mode << 4);
    outFile.write(reinterpret_cast<const char*>(&value_8), sizeof(value_8));

    if (elm.has_hdr_reference_white_flag){
      printDebug("hdr_reference_white", elm.hdr_reference_white, 16);
      value_8 = uint8_t((elm.hdr_reference_white >> 8) & 0x00FF); outFile.write(reinterpret_cast<const char*>(&value_8), sizeof(value_8));
      value_8 = uint8_t((elm.hdr_reference_white     ) & 0x00FF); outFile.write(reinterpret_cast<const char*>(&value_8), sizeof(value_8));
    }
    if (elm.tone_map_mode != 0) {
      printDebug("baseline_hdr_headroom", elm.baseline_hdr_headroom, 16);
      value_8 = uint8_t((elm.baseline_hdr_headroom >> 8) & 0x00FF); outFile.write(reinterpret_cast<const char*>(&value_8), sizeof(value_8));
      value_8 = uint8_t((elm.baseline_hdr_headroom     ) & 0x00FF); outFile.write(reinterpret_cast<const char*>(&value_8), sizeof(value_8));
    }
    if (elm.tone_map_mode == 4) {
      printDebug("num_alternate_images_minus_1",elm.num_alternate_images_minus_1, 2);
      printDebug("gain_application_space_primaries",elm.gain_application_space_primaries, 2);
      printDebug("has_gain_application_offset_flag",elm.has_gain_application_offset_flag, 1);
      printDebug("has_common_mix_params_flag",elm.has_common_mix_params_flag, 1);
      printDebug("has_common_curve_params_flag",elm.has_common_curve_params_flag, 1);
      value_8 = (elm.num_alternate_images_minus_1 << 6) + (elm.gain_application_space_primaries << 4) + (elm.has_gain_application_offset_flag << 3) + 
      (elm.has_common_mix_params_flag << 2) + (elm.has_common_curve_params_flag << 1);
      outFile.write(reinterpret_cast<const char*>(&value_8), sizeof(value_8));

      if (elm.gain_application_space_primaries == 3) {
        for (uint16_t iCh = 0; iCh < 8; iCh++) {
          printDebug("gain_application_space_chromaticities", elm.gain_application_space_chromaticities[iCh], 16);
          value_8 = uint8_t((elm.gain_application_space_chromaticities[iCh] >> 8) & 0x00FF); outFile.write(reinterpret_cast<const char*>(&value_8), sizeof(value_8));
          value_8 = uint8_t((elm.gain_application_space_chromaticities[iCh]     ) & 0x00FF); outFile.write(reinterpret_cast<const char*>(&value_8), sizeof(value_8));
        }
      }
      if (elm.has_gain_application_offset_flag) {
        printDebug("gain_application_offset", elm.gain_application_offset, 16);
        value_8 = uint8_t((elm.gain_application_offset >> 8) & 0x00FF); outFile.write(reinterpret_cast<const char*>(&value_8), sizeof(value_8));
        value_8 = uint8_t((elm.gain_application_offset     ) & 0x00FF); outFile.write(reinterpret_cast<const char*>(&value_8), sizeof(value_8));
      }
      for (uint16_t iAlt = 0; iAlt < elm.num_alternate_images_minus_1 + 1; iAlt++) {
        printDebug("alternate_hdr_headrooms[iAlt]", elm.alternate_hdr_headrooms[iAlt], 16);
        value_8 = uint8_t((elm.alternate_hdr_headrooms[iAlt] >> 8) & 0x00FF); outFile.write(reinterpret_cast<const char*>(&value_8), sizeof(value_8));
        value_8 = uint8_t((elm.alternate_hdr_headrooms[iAlt]     ) & 0x00FF); outFile.write(reinterpret_cast<const char*>(&value_8), sizeof(value_8));

        // Write component mixing function parameters
        if ( iAlt == 0 || !elm.has_common_mix_params_flag){
          printDebug("mix_encoding[iAlt]",elm.mix_encoding[iAlt], 3);
          value_8 = (elm.mix_encoding[iAlt] << 5);
          outFile.write(reinterpret_cast<const char*>(&value_8), sizeof(value_8));                  
          for (uint16_t iPar = 0; iPar < kMixEncodingNumParams[elm.mix_encoding[iAlt]]; iPar++){
            printDebug("mix_params[iAlt][iPar]",elm.mix_params[iAlt][iPar], 16);
            value_8 = uint8_t((elm.mix_params[iAlt][iPar] >> 8) & 0x00FF); outFile.write(reinterpret_cast<const char*>(&value_8), sizeof(value_8));
            value_8 = uint8_t((elm.mix_params[iAlt][iPar]     ) & 0x00FF); outFile.write(reinterpret_cast<const char*>(&value_8), sizeof(value_8));
          }
        }

        /// Write gain curve function parameters
        if ( iAlt == 0 || elm.has_common_curve_params_flag){
          printDebug("gain_curve_interpolation[iAlt]",elm.gain_curve_interpolation[iAlt], 2);
          printDebug("gain_curve_num_control_points_minus_1[iAlt]",elm.gain_curve_num_control_points_minus_1[iAlt], 5);

          value_8 = (elm.gain_curve_interpolation[iAlt] << 6) + (elm.gain_curve_num_control_points_minus_1[iAlt] << 1);
          outFile.write(reinterpret_cast<const char*>(&value_8), sizeof(value_8));
          
          for (uint16_t iCps = 0; iCps < elm.gain_curve_num_control_points_minus_1[iAlt] + 1; iCps++){
            printDebug("gain_curve_control_points_x[iAlt][iCps]",elm.gain_curve_control_points_x[iAlt][iCps], 16);
            value_8 = uint8_t((elm.gain_curve_control_points_x[iAlt][iCps] >> 8) & 0x00FF); outFile.write(reinterpret_cast<const char*>(&value_8), sizeof(value_8));
            value_8 = uint8_t((elm.gain_curve_control_points_x[iAlt][iCps]     ) & 0x00FF); outFile.write(reinterpret_cast<const char*>(&value_8), sizeof(value_8));
          }
        }
        for (uint16_t iCps = 0; iCps < elm.gain_curve_num_control_points_minus_1[iAlt] + 1; iCps++) {
          printDebug("gain_curve_control_points_y[iAlt][iCps]",elm.gain_curve_control_points_y[iAlt][iCps], 16);
          value_8 = uint8_t((elm.gain_curve_control_points_y[iAlt][iCps] >> 8) & 0x00FF); outFile.write(reinterpret_cast<const char*>(&value_8), sizeof(value_8));
          value_8 = uint8_t((elm.gain_curve_control_points_y[iAlt][iCps]     ) & 0x00FF); outFile.write(reinterpret_cast<const char*>(&value_8), sizeof(value_8));
        }
        if (elm.gain_curve_interpolation[iAlt] == 2) {
          for (uint16_t iCps = 0; iCps < elm.gain_curve_num_control_points_minus_1[iAlt] + 1; iCps++) {
            printDebug("gain_curve_control_points_theta[iAlt][iCps]",elm.gain_curve_control_points_theta[iAlt][iCps], 16);
            value_8 = uint8_t((elm.gain_curve_control_points_theta[iAlt][iCps] >> 8) & 0x00FF); outFile.write(reinterpret_cast<const char*>(&value_8), sizeof(value_8));
            value_8 = uint8_t((elm.gain_curve_control_points_theta[iAlt][iCps]     ) & 0x00FF); outFile.write(reinterpret_cast<const char*>(&value_8), sizeof(value_8));
          }
        }
      }
    }

    outFile.close(); // Close the file
    std::cout <<"Binary data successfuylly written to " << binPathGen << "\n";
    std::cout << "+++++++++++++++++++++++++++++++ Binary data successfully written to intermediate file +++++++++++++++++++++++++++++++++]\n";
  } else {
      std::cerr << "Error opening file: " << binPathGen << std::endl;
  } 
}

// Parse JSON Folder, convert metadata item to syntax element and write to file binary data
MetadataMap parseMetadataFolder(const std::string& metadataFolder) {
  MetadataMap items;
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

      if (!j.contains("frame_start") || !j.contains("frame_duration")) {
        std::cerr << "Skipping " << path << " (missing keys)\n";
        continue;
      }

      frame_start    = j["frame_start"].get<uint32_t>();
      frame_duration = j["frame_duration"].get<uint32_t>();

      std::cout << "++++++++++++++++++++++Start processing : " << path << "+++++++++++++++++++++++\n"; 

      // Decode Metadata Items from json
      MetadataItems itm = decodeJsonToMetadataItems(j, path);
      if (itm.error_raised) {
        std::cerr << "Skipping " << path << " error decoding json file\n";
        continue;
      }
      dbgPrintMetadataItems(itm, false);


      // find matching .bin file
      auto baseName = path.stem().string();  // e.g. ST2094-50_IMG_0564_metadataItems
      // auto binPath  = path.parent_path() / (baseName.substr(0, baseName.find("_metadataItems")) + ".bin");
      // if (!fs::exists(binPath)) {
      //   std::cerr << "No matching .bin file for " << path << "\n";
      //   continue;
      // }

      std::string to_find = "_metadataItems";
      auto binPathGen  = path.parent_path() / (baseName.replace(baseName.find("_metadataItems"), to_find.length(), "_gen") + ".bin");
      
      std::cout << "Loaded metadata: " << baseName << " -> frames [" 
                << frame_start << " - " << (frame_start + frame_duration - 1) << "]\n";

      // ================================================= Convert Metadata Items to Syntax Elements ===================================
  
      SyntaxElements elm = convertMetadataItemsToSyntaxElements(itm);

      writeSyntaxElementsToBinaryData(elm, binPathGen);      

      // Write using the binary data generated by this tool from the json
      MetadataItem item{frame_start, frame_duration, binPathGen.string()};

      //Write using the binary data generated by matlab
      //MetadataItem item{frame_start, frame_duration, binPath.string()};
      items[frame_start] = item;
    }
  }
  return items;
}

/* *********************************** DECODING SECTION ********************************************************************************************/

// Decode binary data into syntax elements
SyntaxElements decodeBinaryToSyntaxElements(std::vector<uint8_t> binary_data) {

  const int kMixEncodingNumParams[8] = {0, 0, 0, 1, 1 ,2, 3, 4};
  uint16_t decoded_sample = 0;
  SyntaxElements elements;

  // printDebug("(binary_data[decoded_sample] & 0xFF)",(binary_data[decoded_sample] & 0xFF), 8);
  std::cout << "++++++++++++++++++++++Syntax Elements Decoding ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n"; 
  elements.application_major_version = (binary_data[decoded_sample] & 0xF0) >> 4;
  elements.application_minor_version = (binary_data[decoded_sample] & 0x0F);
  printDebug("application_major_version",elements.application_major_version, 4);
  printDebug("application_minor_version",elements.application_minor_version, 4);
  decoded_sample++;

  elements.num_windows_minus_1 = (binary_data[decoded_sample] & 0xF0) >>  4;
  printDebug("num_windows_minus_1=",elements.num_windows_minus_1, 4);
  decoded_sample++;

  elements.has_hdr_reference_white_flag = (binary_data[decoded_sample] & 0x80) >> 7;
  elements.tone_map_mode = (binary_data[decoded_sample] & 0x70) >>  4;
  printDebug("has_hdr_reference_white_flag",elements.has_hdr_reference_white_flag, 1);
  printDebug("tone_map_mode",elements.tone_map_mode, 3);
  decoded_sample++;

  
  if (elements.has_hdr_reference_white_flag){
    elements.hdr_reference_white = (uint16_t(binary_data[decoded_sample]) << 8) + (uint16_t(binary_data[decoded_sample+1]) );
    printDebug("hdr_reference_white", elements.hdr_reference_white, 16);
    decoded_sample++;
    decoded_sample++;
  }
  if (elements.tone_map_mode != 0) {
    elements.baseline_hdr_headroom = (uint16_t(binary_data[decoded_sample]) << 8) + (uint16_t(binary_data[decoded_sample+1]) );
    printDebug("baseline_hdr_headroom", elements.baseline_hdr_headroom, 16);
    decoded_sample++;
    decoded_sample++;
  }
  
  if (elements.tone_map_mode == 4) {
    elements.num_alternate_images_minus_1     = (binary_data[decoded_sample] & 0xC0) >> 6;
    elements.gain_application_space_primaries = (binary_data[decoded_sample] & 0x30) >> 4;
    elements.has_gain_application_offset_flag = (binary_data[decoded_sample] & 0x08) >> 3;
    elements.has_common_mix_params_flag       = (binary_data[decoded_sample] & 0x04) >> 2;
    elements.has_common_curve_params_flag     = (binary_data[decoded_sample] & 0x02) >> 1;
    printDebug("num_alternate_images_minus_1",elements.num_alternate_images_minus_1, 2);
    printDebug("gain_application_space_primaries",elements.gain_application_space_primaries, 2);
    printDebug("has_gain_application_offset_flag",elements.has_gain_application_offset_flag, 1);
    printDebug("has_common_mix_params_flag",elements.has_common_mix_params_flag, 1);
    printDebug("has_common_curve_params_flag",elements.has_common_curve_params_flag, 1);
    decoded_sample++;

    if (elements.gain_application_space_primaries == 3) {
      for (uint16_t iCh = 0; iCh < 8; iCh++) {
        elements.gain_application_space_chromaticities[iCh] = (uint16_t(binary_data[decoded_sample]) << 8) + (uint16_t(binary_data[decoded_sample+1]) );
        printDebug("gain_application_space_chromaticities[iCh]",elements.gain_application_space_chromaticities[iCh], 16);
        decoded_sample++;
        decoded_sample++;
      }
    }
    
    if (elements.has_gain_application_offset_flag) {
      elements.gain_application_offset = (uint16_t(binary_data[decoded_sample]) << 8) + (uint16_t(binary_data[decoded_sample+1]) );
      printDebug("gain_application_offset",elements.gain_application_offset, 16);
      decoded_sample++;
      decoded_sample++;
    }
    for (uint16_t iAlt = 0; iAlt < elements.num_alternate_images_minus_1 + 1; iAlt++) {
      elements.alternate_hdr_headrooms[iAlt] = (uint16_t(binary_data[decoded_sample]) << 8) + (uint16_t(binary_data[decoded_sample+1]) );
      printDebug("alternate_hdr_headrooms[iAlt]",elements.alternate_hdr_headrooms[iAlt], 16);
      decoded_sample++;
      decoded_sample++;
      // Write component mixing function parameters
      if ( iAlt == 0 || !elements.has_common_mix_params_flag){
        elements.mix_encoding[iAlt] = uint16_t(binary_data[decoded_sample] & 0xE0) >> 5;
        printDebug("mix_encoding[iAlt]",elements.mix_encoding[iAlt], 3);
        decoded_sample++;           
        for (uint16_t iPar = 0; iPar < kMixEncodingNumParams[elements.mix_encoding[iAlt]]; iPar++){
          elements.mix_params[iAlt][iPar] = (uint16_t(binary_data[decoded_sample]) << 8) + (uint16_t(binary_data[decoded_sample+1]) );
          printDebug("mix_params[iAlt][iPar]",elements.mix_params[iAlt][iPar], 16);
          decoded_sample++;
          decoded_sample++;
        }
      } else {
        elements.mix_encoding[iAlt] = elements.mix_encoding[0];
        for (uint16_t iPar = 0; iPar < kMixEncodingNumParams[elements.mix_encoding[iAlt]]; iPar++){
          elements.mix_params[iAlt][iPar] = elements.mix_params[0][iPar];
        }
      }
      
      /// Write gain curve function parameters
      if ( iAlt == 0 || elements.has_common_curve_params_flag){
        elements.gain_curve_interpolation[iAlt] = uint16_t(binary_data[decoded_sample] & 0xC0) >> 6;
        elements.gain_curve_num_control_points_minus_1[iAlt] = uint16_t(binary_data[decoded_sample] & 0x3E) >> 1;
        printDebug("gain_curve_interpolation[iAlt]",elements.gain_curve_interpolation[iAlt], 2);
        printDebug("gain_curve_num_control_points_minus_1[iAlt]",elements.gain_curve_num_control_points_minus_1[iAlt], 5);
        decoded_sample++;
        
        for (uint16_t iCps = 0; iCps < elements.gain_curve_num_control_points_minus_1[iAlt] + 1; iCps++){
          elements.gain_curve_control_points_x[iAlt][iCps] = (uint16_t(binary_data[decoded_sample]) << 8) + (uint16_t(binary_data[decoded_sample+1]) );
          printDebug("gain_curve_control_points_x[iAlt][iCps]",elements.gain_curve_control_points_x[iAlt][iCps], 16);
          decoded_sample++;
          decoded_sample++;
        }
      } else {
        elements.gain_curve_interpolation[iAlt] = elements.gain_curve_interpolation[0];
        elements.gain_curve_num_control_points_minus_1[iAlt] = elements.gain_curve_num_control_points_minus_1[0];
        for (uint16_t iCps = 0; iCps < elements.gain_curve_num_control_points_minus_1[iAlt] + 1; iCps++){
          elements.gain_curve_control_points_x[iAlt][iCps] = elements.gain_curve_control_points_x[0][iCps];
        }
      }
      for (uint16_t iCps = 0; iCps < elements.gain_curve_num_control_points_minus_1[iAlt] + 1; iCps++) {
        elements.gain_curve_control_points_y[iAlt][iCps] = (uint16_t(binary_data[decoded_sample]) << 8) + (uint16_t(binary_data[decoded_sample+1]) );
        printDebug("gain_curve_control_points_y[iAlt][iCps]",elements.gain_curve_control_points_y[iAlt][iCps], 16);
        decoded_sample++;
        decoded_sample++;
      }
      if (elements.gain_curve_interpolation[iAlt] == 2) {
        for (uint16_t iCps = 0; iCps < elements.gain_curve_num_control_points_minus_1[iAlt] + 1; iCps++) {
          elements.gain_curve_control_points_theta[iAlt][iCps] = (uint16_t(binary_data[decoded_sample]) << 8) + (uint16_t(binary_data[decoded_sample+1]) );
          printDebug("gain_curve_control_points_theta[iAlt][iCps]",elements.gain_curve_control_points_theta[iAlt][iCps], 16);
          decoded_sample++;
          decoded_sample++;
        }
      }
    }
  }  
  std::cout << "++++++++++++++++++++++++++++++Syntax Elements Successfully Decoding ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++]\n";
  return elements;
}

// Convert the syntax elements to Metadata Items as described in Clause C.3
MetadataItems convertSyntaxElementsToMetadataItems(SyntaxElements elm){
  MetadataItems itm;
  
  itm.error_raised = false;
  // get mandatory metadata
  itm.windowNumber     = 1;
  if (elm.has_hdr_reference_white_flag) {
    itm.hdrReferenceWhite = float(elm.hdr_reference_white) / Q_HDR_REFERENCE_WHITE;
  } else {
    itm.hdrReferenceWhite = 203.0;
  }
  itm.toneMapMode = elm.tone_map_mode;

  // Get Optional metadata items
  if (itm.toneMapMode != 0) {
    itm.baselineHdrHeadroom = float(elm.baseline_hdr_headroom) / Q_HDR_HEADROOM;;
  }
  if (itm.toneMapMode == 3) {
    itm.numAlternateImages = 2;
    // BT.2020 primaries
    itm.gainApplicationSpaceChromaticities.push_back(0.708);
    itm.gainApplicationSpaceChromaticities.push_back(0.292);
    itm.gainApplicationSpaceChromaticities.push_back(0.17);
    itm.gainApplicationSpaceChromaticities.push_back(0.797);
    itm.gainApplicationSpaceChromaticities.push_back(0.131);
    itm.gainApplicationSpaceChromaticities.push_back(0.046);
    itm.gainApplicationSpaceChromaticities.push_back(0.3127);
    itm.gainApplicationSpaceChromaticities.push_back(0.329);
    // 
    itm.gainApplicationOffset = 0.0;
    // Compute alternate headroom
    itm.alternateHdrHeadroom.push_back(0.0);
    float headroom_to_anchor_ratio = std::min(itm.baselineHdrHeadroom / log2(1000.0/203.0), 1.0);
    float h_alt_1 = log2(8.0/3.0) * headroom_to_anchor_ratio;
    itm.alternateHdrHeadroom.push_back(h_alt_1);

    // Constant parameter across alternate images
    float kappa  = 0.65;
    float x_knee = 1;
    float x_max  = pow(2.0, itm.baselineHdrHeadroom);
    for (uint16_t iAlt = 0;  iAlt  < itm.numAlternateImages; iAlt++){
      // Component mixing is maxRGB
      itm.componentMixRed.push_back(0.0);
      itm.componentMixGreen.push_back(0.0);
      itm.componentMixBlue.push_back(0.0);
      itm.componentMixMax.push_back(1.0);
      itm.componentMixMin.push_back(0.0);
      itm.componentMixComponent.push_back(0.0); 
      itm.gainCurveInterpolation.push_back(2);
      itm.gainCurveNumControlPoints.push_back(8);

      // Inner vector for push_back
      std::vector<float> inner_gainCurveControlPointX;
      std::vector<float> inner_gainCurveControlPointY;
      std::vector<float> inner_gainCurveControlPointTheta;

      // Compute the control points parameter depending on the alternate headroom
      float  y_white = 1.0;
      if (iAlt == 0) {
        y_white = 1 - (0.5 * headroom_to_anchor_ratio );
      }
      float y_knee = y_white;
      float y_max  = pow(2.0, itm.alternateHdrHeadroom[iAlt]);
      float x_mid = (1.0 - kappa) * x_knee + kappa * (x_knee * y_max / y_knee);
      float y_mid = (1.0 - kappa) * y_knee + kappa * y_max;
      // Compute Quadratic Beziers coefficients
      float a_x = x_knee - 2 * x_mid + x_max;
      float a_y = y_knee - 2 * y_mid + y_max;
      float b_x = 2 * x_mid - 2 * x_knee;
      float b_y = 2 * y_mid - 2 * y_knee;
      float c_x = x_knee;
      float c_y = y_knee;

      for (uint16_t iCps = 0; iCps < itm.gainCurveNumControlPoints[iAlt]; iCps++) {
        // Compute the control points
        float t = float(iCps) / (float(itm.gainCurveNumControlPoints[iAlt]) - 1.0);
        float t_square = t * t;
        float x = a_x * t_square + b_x * t + c_x;
        float y = a_y * t_square + b_y * t + c_y;
        float m = (2.0 * a_y * t + b_y) / (2 * a_x * t + b_x);
        float slope = atan( (x * m - y) / (log(2) * x * y) );
        inner_gainCurveControlPointX.push_back(x);
        inner_gainCurveControlPointY.push_back(log2(y / x));
        inner_gainCurveControlPointTheta.push_back(slope / PI_CUSTOM * 180.0);
      }
      itm.gainCurveControlPointX.push_back(inner_gainCurveControlPointX);
      itm.gainCurveControlPointY.push_back(inner_gainCurveControlPointY);
      itm.gainCurveControlPointTheta.push_back(inner_gainCurveControlPointTheta);
    }
  }
  if (itm.toneMapMode == 4){
    itm.numAlternateImages = elm.num_alternate_images_minus_1 + 1;
    if (elm.gain_application_space_primaries == 0){
      itm.gainApplicationSpaceChromaticities.push_back(0.64);
      itm.gainApplicationSpaceChromaticities.push_back(0.33); 
      itm.gainApplicationSpaceChromaticities.push_back(0.3); 
      itm.gainApplicationSpaceChromaticities.push_back(0.6);
      itm.gainApplicationSpaceChromaticities.push_back(0.15); 
      itm.gainApplicationSpaceChromaticities.push_back(0.06);
      itm.gainApplicationSpaceChromaticities.push_back(0.3127);
      itm.gainApplicationSpaceChromaticities.push_back(0.329);
    } else if (elm.gain_application_space_primaries == 1){
      itm.gainApplicationSpaceChromaticities.push_back(0.68); 
      itm.gainApplicationSpaceChromaticities.push_back(0.32); 
      itm.gainApplicationSpaceChromaticities.push_back(0.265); 
      itm.gainApplicationSpaceChromaticities.push_back(0.69); 
      itm.gainApplicationSpaceChromaticities.push_back(0.15); 
      itm.gainApplicationSpaceChromaticities.push_back(0.06); 
      itm.gainApplicationSpaceChromaticities.push_back(0.3127); 
      itm.gainApplicationSpaceChromaticities.push_back(0.329); 
    } else if (elm.gain_application_space_primaries == 2){
      itm.gainApplicationSpaceChromaticities.push_back(0.708);
      itm.gainApplicationSpaceChromaticities.push_back(0.292);
      itm.gainApplicationSpaceChromaticities.push_back(0.17);
      itm.gainApplicationSpaceChromaticities.push_back(0.797);
      itm.gainApplicationSpaceChromaticities.push_back(0.131);
      itm.gainApplicationSpaceChromaticities.push_back(0.046);
      itm.gainApplicationSpaceChromaticities.push_back(0.3127);
      itm.gainApplicationSpaceChromaticities.push_back(0.329);
    } else if (elm.gain_application_space_primaries == 3){
        for (uint16_t iCh = 0; iCh < 8; iCh++) {
          itm.gainApplicationSpaceChromaticities.push_back(float(elm.gain_application_space_chromaticities[iCh]) / Q_GAIN_APPLICATION_SPACE_CHROMATICITY);
        }
    } else {
      std::cerr << "gain_application_space_primaries=" << elm.gain_application_space_primaries << "  not defined.\n";
      itm.error_raised = true;
    }
    if (elm.has_gain_application_offset_flag) {
      itm.gainApplicationOffset = float(elm.gain_application_offset) / Q_GAIN_APPLICATION_OFFSET;
    } else {
      itm.gainApplicationOffset = 0.0;
    }
    for (uint16_t iAlt = 0;  iAlt  < itm.numAlternateImages; iAlt++){

      itm.alternateHdrHeadroom.push_back(float(elm.alternate_hdr_headrooms[iAlt]) / Q_HDR_HEADROOM);
      // init k_params to zero and replace the one that are not
      itm.componentMixRed.push_back(0.0);
      itm.componentMixGreen.push_back(0.0);
      itm.componentMixBlue.push_back(0.0);
      itm.componentMixMax.push_back(0.0);
      itm.componentMixMin.push_back(0.0);
      itm.componentMixComponent.push_back(0.0); 
      if (elm.mix_encoding[iAlt] == 0){
        itm.componentMixMax[iAlt]   = 1.0;
      } else if (elm.mix_encoding[iAlt] == 1){
        itm.componentMixComponent[iAlt]   = 1.0;
      } else if (elm.mix_encoding[iAlt] == 2){
        itm.componentMixRed[iAlt]   = 1.0 / 6.0;
        itm.componentMixGreen[iAlt] = 1.0 / 6.0;
        itm.componentMixBlue[iAlt]  = 1.0 / 6.0;
        itm.componentMixMax[iAlt]   = 1.0 / 2.0;
      } else if (elm.mix_encoding[iAlt] == 3){
        itm.componentMixRed[iAlt]   = float(elm.mix_params[iAlt][0]) / Q_MIX_PARAMS / 3.0;
        itm.componentMixGreen[iAlt] = float(elm.mix_params[iAlt][0]) / Q_MIX_PARAMS / 3.0;
        itm.componentMixBlue[iAlt]  = float(elm.mix_params[iAlt][0]) / Q_MIX_PARAMS / 3.0;
        itm.componentMixMax[iAlt]   = 1.0 - float(elm.mix_params[iAlt][0]) / Q_MIX_PARAMS;
      }  else if (elm.mix_encoding[iAlt] == 4){
        itm.componentMixMax[iAlt]   = float(elm.mix_params[iAlt][0]) / Q_MIX_PARAMS;
        itm.componentMixComponent[iAlt]   = 1.0 - float(elm.mix_params[iAlt][0]) / Q_MIX_PARAMS;
      } else if (elm.mix_encoding[iAlt] == 5){
        itm.componentMixRed[iAlt]   = float(elm.mix_params[iAlt][0]) / Q_MIX_PARAMS;
        itm.componentMixGreen[iAlt] = float(elm.mix_params[iAlt][1]) / Q_MIX_PARAMS;
        itm.componentMixBlue[iAlt]  = 1.0 - (float(elm.mix_params[iAlt][1]) + float((elm.mix_params[iAlt][0]))) / Q_MIX_PARAMS;
      } else if (elm.mix_encoding[iAlt] == 6){
        itm.componentMixRed[iAlt]   = float(elm.mix_params[iAlt][0]) / Q_MIX_PARAMS / 3.0;
        itm.componentMixGreen[iAlt] = float(elm.mix_params[iAlt][0]) / Q_MIX_PARAMS / 3.0;
        itm.componentMixBlue[iAlt]  = float(elm.mix_params[iAlt][0]) / Q_MIX_PARAMS / 3.0;
        itm.componentMixMax[iAlt]   = float(elm.mix_params[iAlt][1]) / Q_MIX_PARAMS;
        itm.componentMixMin[iAlt]   = float(elm.mix_params[iAlt][2]) / Q_MIX_PARAMS;
        itm.componentMixComponent[iAlt]   = 1.0 - (float(elm.mix_params[iAlt][0]) + float(elm.mix_params[iAlt][1]) + float(elm.mix_params[iAlt][2])) / Q_MIX_PARAMS;
      } else if (elm.mix_encoding[iAlt] == 7){
        itm.componentMixRed[iAlt]   = float(elm.mix_params[iAlt][0]) / Q_MIX_PARAMS;
        itm.componentMixGreen[iAlt] = float(elm.mix_params[iAlt][1]) / Q_MIX_PARAMS;
        itm.componentMixBlue[iAlt]  = float(elm.mix_params[iAlt][2]) / Q_MIX_PARAMS;
        itm.componentMixMax[iAlt]   = float(elm.mix_params[iAlt][3]) / Q_MIX_PARAMS;
        itm.componentMixComponent[iAlt]   = 1.0 - (float(elm.mix_params[iAlt][0]) + float(elm.mix_params[iAlt][1]) + 
                                            float(elm.mix_params[iAlt][2]) + float(elm.mix_params[iAlt][3])) / Q_MIX_PARAMS;
      } else {
        std::cerr << "mix_encoding[" << iAlt << "]=" << elm.mix_encoding[iAlt] << "  not defined.\n";
        itm.error_raised = true;
      }
      itm.gainCurveInterpolation.push_back(elm.gain_curve_interpolation[iAlt]);
      itm.gainCurveNumControlPoints.push_back(elm.gain_curve_num_control_points_minus_1[iAlt] + 1);

      // Inner vector for push_back
      std::vector<float> inner_gainCurveControlPointX;
      std::vector<float> inner_gainCurveControlPointY;
      std::vector<float> inner_gainCurveControlPointTheta;
      for (uint16_t iCps = 0; iCps < itm.gainCurveNumControlPoints[iAlt]; iCps++) {
        inner_gainCurveControlPointX.push_back(float(elm.gain_curve_control_points_x[iAlt][iCps]) / Q_GAIN_CURVE_CONTROL_POINT_X);
        inner_gainCurveControlPointY.push_back(float(elm.gain_curve_control_points_y[iAlt][iCps]) / Q_GAIN_CURVE_CONTROL_POINT_Y - O_GAIN_CURVE_CONTROL_POINT_Y);
        if (itm.gainCurveInterpolation[iAlt] == 2) {
          inner_gainCurveControlPointTheta.push_back(float(elm.gain_curve_control_points_theta[iAlt][iCps]) / Q_GAIN_CURVE_CONTROL_POINT_THETA - O_GAIN_CURVE_CONTROL_POINT_THETA);
        }
      }
      itm.gainCurveControlPointX.push_back(inner_gainCurveControlPointX);
      itm.gainCurveControlPointY.push_back(inner_gainCurveControlPointY);
      itm.gainCurveControlPointTheta.push_back(inner_gainCurveControlPointTheta);
    }
  }
  dbgPrintMetadataItems(itm, true);
  return itm;
}


MetadataItems decodeBinaryData(const std::string& inputFile) {

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
  SyntaxElements elm = decodeBinaryToSyntaxElements(binary_data);

  MetadataItems itm = convertSyntaxElementsToMetadataItems(elm);
  return itm;

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

    // Extract hex portion of t35PrefixHex (strip description after ':' if present)
    std::string hexOnly = t35PrefixHex;
    size_t colonPos = t35PrefixHex.find(':');
    if (colonPos != std::string::npos) {
        hexOnly = t35PrefixHex.substr(0, colonPos);
    }

    // Build full T.35 payload = [prefix][metadata]
    // Convert hex string to binary data
    std::vector<uint8_t> prefixBytes;
    if (hexOnly.size() % 2 != 0) {
        std::cerr << "Invalid hex string length in T.35 prefix (must be even): " << hexOnly << "\n";
        return sei; // return incomplete NAL
    }
    for (size_t i = 0; i < hexOnly.size(); i += 2) {
        unsigned int byteVal = 0;
        std::string byteStr = hexOnly.substr(i, 2);
        if (sscanf(byteStr.c_str(), "%02x", &byteVal) != 1) {
            std::cerr << "Invalid hex substring in T.35 prefix: " << byteStr << "\n";
            return sei; // return incomplete NAL
        }
        prefixBytes.push_back(static_cast<uint8_t>(byteVal));
    }
    u32 prefixSize = static_cast<u32>(prefixBytes.size());

    std::vector<uint8_t> fullPayload(prefixSize + size);
    memcpy(fullPayload.data(), prefixBytes.data(), prefixSize);
    memcpy(fullPayload.data() + prefixSize, payload, size);

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
