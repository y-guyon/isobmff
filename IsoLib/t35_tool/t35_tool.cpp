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

constexpr bool STRING_TO_HANDLE_MODE = false;   // true = hex, false = text

/*
const float  PI_CUSTOM = 3.14159265358979323846;

const int MAX_NB_ALTERNATE = 4;
const int MAX_NB_CONTROL_POINTS = 32;
const int MAX_NB_CHROMATICITIES = 8;
const int MAX_NB_component_mixing_coefficient = 6;

// Compute quantization error of each float to uint16_t
const float Q_HDR_REFERENCE_WHITE = 50000.0 / 10000.0;
const float Q_HDR_HEADROOM   = 60000.0 / 6.0;
const float P_GAIN_APPLICATION_SPACE_CHROMATICITY = 3.0 / 30000.0 / 2.0;
const float Q_GAIN_APPLICATION_SPACE_CHROMATICITY = 30000.0 / 3.0; 
const float P_COMPONENT_MIXING_COEFFICIENT = 1.0 / 10000.0 / 2.0;
const float Q_COMPONENT_MIXING_COEFFICIENT = 10000.0; 
const float Q_GAIN_CURVE_CONTROL_POINT_X = 64000.0 / 64.0; 
const float O_GAIN_CURVE_CONTROL_POINT_Y = 6.0; 
const float Q_GAIN_CURVE_CONTROL_POINT_Y = 48000.0 / 12.0; 
const float O_GAIN_CURVE_CONTROL_POINT_THETA = 90.0; 
const float Q_GAIN_CURVE_CONTROL_POINT_THETA = 36000.0 / 180.0; 
*/
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
struct GainCurve{
  // std::vector<uint32_t> gainCurveInterpolation; -> not in the spec currently
  uint32_t gainCurveNumControlPoints;
  std::vector<float> gainCurveControlPointX;
  std::vector<float> gainCurveControlPointY;
  std::vector<float> gainCurveControlPointTheta;
};

struct ComponentMix{
  float componentMixRed;
  float componentMixGreen;
  float componentMixBlue;
  float componentMixMax;
  float componentMixMin;
  float componentMixComponent;
};

struct ColorGainFunction{
  ComponentMix cm;
  GainCurve gc;
};

struct HeadroomAdaptiveToneMap{
  float baselineHdrHeadroom;
  
  uint32_t numAlternateImages;
  float gainApplicationSpaceChromaticities[MAX_NB_CHROMATICITIES];
  std::vector<float> alternateHdrHeadroom;
  std::vector<ColorGainFunction> cgf;
};

struct ColorVolumeTransform{
  float hdrReferenceWhite;
  HeadroomAdaptiveToneMap hatm;
};

struct ProcessingWindow{
  uint32_t upperLeftCorner;
  uint32_t lowerRightCorner;
  uint32_t windowNumber;
};

struct TimeInterval{
  uint32_t timeIntervalStart;
  uint32_t timeintervalDuration;
};
struct MetadataItems {
  uint8_t applicationIdentifier;
  uint8_t applicationVersion;
  TimeInterval timeI;
  ProcessingWindow pWin;
  ColorVolumeTransform cvt;

  // not in specification, convenience flag for implementation
  bool isHeadroomAdaptiveToneMap;   
  bool isReferenceWhiteToneMapping;
  bool isLinearInterpolation[MAX_NB_ALTERNATE];
  bool hasSlopeParameter[MAX_NB_ALTERNATE];
};

struct MetadataItem {
  uint32_t frame_start;
  uint32_t frame_duration;
  std::string bin_path;
};

struct SyntaxElements {
  // smpte_st_2094_50_application_info
  uint16_t application_version;     
   
  // smpte_st_2094_50_color_volume_transform
  bool has_custom_hdr_reference_white_flag;   
  bool has_adaptive_tone_map_flag;                                               
  uint16_t hdr_reference_white ;  
  
  // smpte_st_2094_50_adaptive_tone_map
  uint16_t baseline_hdr_headroom ;   
  bool use_reference_white_tone_mapping_flag;
  uint16_t num_alternate_images;          
  uint16_t gain_application_space_chromaticities_mode;
  bool has_common_component_component_mixing_coefficient_flag;
  bool has_common_curve_params_flag;
  uint16_t gain_application_space_chromaticities[MAX_NB_CHROMATICITIES];
  uint16_t  alternate_hdr_headrooms[MAX_NB_ALTERNATE];                

  // smpte_st_2094_50_component_mixing
  uint16_t  component_mixing_type[MAX_NB_ALTERNATE];
  bool has_component_mixing_coefficient_flag[MAX_NB_ALTERNATE][MAX_NB_component_mixing_coefficient];
  uint16_t  component_mixing_coefficient[MAX_NB_ALTERNATE][MAX_NB_component_mixing_coefficient];

  // smpte_st_2094_50_gain_curve
  uint16_t  gain_curve_num_control_points_minus_1[MAX_NB_ALTERNATE];
  bool  gain_curve_use_pchip_slope_flag[MAX_NB_ALTERNATE];
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
  std::cout <<"windowNumber=" << itm.pWin.windowNumber << "\n";
  std::cout <<"hdrReferenceWhite=" << itm.cvt.hdrReferenceWhite << "\n";
  std::cout <<"baselineHdrHeadroom=" << itm.cvt.hatm.baselineHdrHeadroom << "\n";
  if ( itm.isHeadroomAdaptiveToneMap || decode) {
    std::cout <<"numAlternateImages=" << itm.cvt.hatm.numAlternateImages << "\n";

    std::cout << "gainApplicationSpaceChromaticities=[" ;
    for (float val : itm.cvt.hatm.gainApplicationSpaceChromaticities) {
      std::cout << val << ", ";
    }
    std::cout << "]" << std::endl;

    for (uint32_t iAlt = 0; iAlt < itm.cvt.hatm.numAlternateImages; iAlt++) {
      std::cout <<"alternateHdrHeadroom=" << itm.cvt.hatm.alternateHdrHeadroom[iAlt] << "\n";
      std::cout <<"componentMixRed=" << itm.cvt.hatm.cgf[iAlt].cm.componentMixRed << "\n";
      std::cout <<"componentMixGreen=" << itm.cvt.hatm.cgf[iAlt].cm.componentMixGreen << "\n";
      std::cout <<"componentMixBlue=" << itm.cvt.hatm.cgf[iAlt].cm.componentMixBlue << "\n";
      std::cout <<"componentMixMax=" << itm.cvt.hatm.cgf[iAlt].cm.componentMixMax << "\n";
      std::cout <<"componentMixMin=" << itm.cvt.hatm.cgf[iAlt].cm.componentMixMin << "\n";
      std::cout <<"componentMixComponent=" << itm.cvt.hatm.cgf[iAlt].cm.componentMixComponent << "\n";

      std::cout <<"gainCurveNumControlPoints=" << itm.cvt.hatm.cgf[iAlt].gc.gainCurveNumControlPoints << "\n";

      std::cout << "gainCurveControlPointX=[" << std::endl;
      for (float val : itm.cvt.hatm.cgf[iAlt].gc.gainCurveControlPointX) {
          std::cout << val << ", ";
      }
      std::cout << "]" << std::endl;

      std::cout << "gainCurveControlPointY=[" << std::endl;
      for (float val : itm.cvt.hatm.cgf[iAlt].gc.gainCurveControlPointY) {
          std::cout << val << ", ";
      }
      std::cout << "]" << std::endl;

      std::cout << "gainCurveControlPointTheta=[" << std::endl;
      for (float val : itm.cvt.hatm.cgf[iAlt].gc.gainCurveControlPointTheta) {
          std::cout << val << ", ";
      }
      std::cout << "]" << std::endl;
    }
  }
  std::cout << "===================================================================================================================================]\n";
}

/* *********************************** ENCODING SECTION ********************************************************************************************/

// Read from json file the metadata items
MetadataItems decodeJsonToMetadataItems(nlohmann::json j, const std::string& path, bool *error_raised) {
  MetadataItems itm;

  std::cout << "++++++++++++++++++++++ DECODE JSON TO METADATA ITEMS **++++++++++++++++++++++++++++++*********************************************]\n";

  // MetadataItems
  itm.applicationIdentifier = 5;
  itm.applicationVersion    = 255;

  // TimeInterval
  if (j.contains("frame_start") && j.contains("frame_duration")) {
    itm.timeI.timeIntervalStart     = j["frame_start"].get<uint32_t>();
    itm.timeI.timeintervalDuration  =  j["frame_duration"].get<uint32_t>();
  } else{
    // If not present attach it to only the first frame
    std::cerr << "JSON Decode: missing optional keys: frame_start | frame_duration\n";
    itm.timeI.timeIntervalStart     = 0;
    itm.timeI.timeintervalDuration  = 1;
  }

  // ProcessingWindow - fixed
  itm.pWin.upperLeftCorner  = 0;
  itm.pWin.lowerRightCorner = 0;
  itm.pWin.windowNumber     = 1;

  // Checking mandatory metadata
  if (!j.contains("hdrReferenceWhite") ) {
    std::cerr <<  "missing mandatory keys: hdrReferenceWhite\n";
     return itm;
  }

  // ================================================= Decode Metadata Items ===================================
  // get mandatory metadata
  itm.cvt.hdrReferenceWhite       = j["hdrReferenceWhite"].get<float>();
  // Initialize mode detection -> not metadata, just implementation convenience to set presence of various metadata
  itm.isHeadroomAdaptiveToneMap   = false;
  itm.isReferenceWhiteToneMapping = false;

  if (j.contains("baselineHdrHeadroom") )
  {
    itm.isHeadroomAdaptiveToneMap    = true; // if there is a baselineHdrHeadroom then it is a headroom adaptive tone mapping
    itm.cvt.hatm.baselineHdrHeadroom = j["baselineHdrHeadroom"].get<float>();

    if (!j.contains("numAlternateImages") || !j.contains("gainApplicationSpaceChromaticities") ){ // Derived Headroom Adaptive Tone Mapping parameters based on baselineHdrHeadroom
      itm.isReferenceWhiteToneMapping  = true;
    }
    else // Custom Headroom Adaptive Tone Mapping mode
    {
      itm.isReferenceWhiteToneMapping                 = false;
      itm.cvt.hatm.numAlternateImages                 = j["numAlternateImages"].get<uint32_t>();

      std::vector<float> gainApplicationSpaceChromaticities = j["gainApplicationSpaceChromaticities"].get<std::vector<float>>();
      if (gainApplicationSpaceChromaticities.size() != MAX_NB_CHROMATICITIES){ *error_raised = true; std::cerr <<  "JSON Decode: size of gainApplicationSpaceChromaticities != 8\n"; }
      for (int iCh = 0; iCh < MAX_NB_CHROMATICITIES; iCh++) {
        itm.cvt.hatm.gainApplicationSpaceChromaticities[iCh] =  gainApplicationSpaceChromaticities[iCh];
      }
      
      if (itm.cvt.hatm.numAlternateImages > 0) { // if no alternat then there is no more netadata
        if (!j.contains("alternateHdrHeadroom"))     { *error_raised = true; std::cerr << "JSON Decode: alternateHdrHeadroom metadata item missing\n"     ; return itm;}

        if (!j.contains("componentMixRed"))          { *error_raised = true; std::cerr << "JSON Decode: componentMixRed metadata item missing\n"          ; return itm;}
        if (!j.contains("componentMixGreen"))        { *error_raised = true; std::cerr << "JSON Decode: componentMixGreen metadata item missing\n"        ; return itm;}
        if (!j.contains("componentMixBlue"))         { *error_raised = true; std::cerr << "JSON Decode: componentMixBlue metadata item missing\n"         ; return itm;}
        if (!j.contains("componentMixMax"))          { *error_raised = true; std::cerr << "JSON Decode: componentMixMax metadata item missing\n"          ; return itm;}
        if (!j.contains("componentMixMin"))          { *error_raised = true; std::cerr << "JSON Decode: componentMixMin metadata item missing\n"          ; return itm;}
        if (!j.contains("componentMixComponent"))    { *error_raised = true; std::cerr << "JSON Decode: componentMixComponent metadata item missing\n"    ; return itm;}

        if (!j.contains("gainCurveNumControlPoints")){ *error_raised = true; std::cerr << "JSON Decode: gainCurveNumControlPoints metadata item missing\n"; return itm;}
        if (!j.contains("gainCurveControlPointX"))   { *error_raised = true; std::cerr << "JSON Decode: gainCurveControlPointX metadata item missing\n"   ; return itm;}
        if (!j.contains("gainCurveControlPointY"))   { *error_raised = true; std::cerr << "JSON Decode: gainCurveControlPointY metadata item missing\n"   ; return itm;}
        
        // Decode headroom adaptive metadata
        std::vector<float> alternateHdrHeadroom  = j["alternateHdrHeadroom"].get<std::vector<float>>();
        std::vector<float> componentMixRed       = j["componentMixRed"].get<std::vector<float>>();
        std::vector<float> componentMixGreen     = j["componentMixGreen"].get<std::vector<float>>();
        std::vector<float> componentMixBlue      = j["componentMixBlue"].get<std::vector<float>>();
        std::vector<float> componentMixMax       = j["componentMixMax"].get<std::vector<float>>();
        std::vector<float> componentMixMin       = j["componentMixMin"].get<std::vector<float>>();
        std::vector<float> componentMixComponent = j["componentMixComponent"].get<std::vector<float>>();

        std::vector<uint32_t>           gainCurveNumControlPoints = j["gainCurveNumControlPoints"].get<std::vector<uint32_t>>();
        std::vector<std::vector<float>> gainCurveControlPointX    = j["gainCurveControlPointX"].get<std::vector<std::vector<float>>>();
        std::vector<std::vector<float>> gainCurveControlPointY    = j["gainCurveControlPointY"].get<std::vector<std::vector<float>>>();

        // Check the size of outter element
        if (alternateHdrHeadroom.size()      < itm.cvt.hatm.numAlternateImages){ *error_raised = true; std::cerr << "JSON Decode: size of alternateHdrHeadroom < numAlternateImages\n";  return itm;}
        if (componentMixRed.size()           < itm.cvt.hatm.numAlternateImages){ *error_raised = true; std::cerr << "JSON Decode: size of componentMixRed < numAlternateImages\n"; return itm;}
        if (componentMixGreen.size()         < itm.cvt.hatm.numAlternateImages){ *error_raised = true; std::cerr << "JSON Decode: size of componentMixGreen < numAlternateImages\n"; return itm;}
        if (componentMixBlue.size()          < itm.cvt.hatm.numAlternateImages){ *error_raised = true; std::cerr << "JSON Decode: size of componentMixBlue < numAlternateImages\n"; return itm;}
        if (componentMixMax.size()           < itm.cvt.hatm.numAlternateImages){ *error_raised = true; std::cerr << "JSON Decode: size of componentMixMax < numAlternateImages\n"; return itm;}
        if (componentMixMin.size()           < itm.cvt.hatm.numAlternateImages){ *error_raised = true; std::cerr << "JSON Decode: size of componentMixMin < numAlternateImages\n"; return itm;}
        if (componentMixComponent.size()     < itm.cvt.hatm.numAlternateImages){ *error_raised = true; std::cerr << "JSON Decode: size of componentMixComponent < numAlternateImages\n"; return itm;}
        if (gainCurveNumControlPoints.size() < itm.cvt.hatm.numAlternateImages){ *error_raised = true; std::cerr << "JSON Decode: size of gainCurveNumControlPoints < numAlternateImages\n"; return itm;}
        if (gainCurveControlPointX.size()    < itm.cvt.hatm.numAlternateImages){ *error_raised = true; std::cerr << "JSON Decode: size of gainCurveControlPointX < numAlternateImages\n"; return itm;}
        if (gainCurveControlPointY.size()    < itm.cvt.hatm.numAlternateImages){ *error_raised = true; std::cerr << "JSON Decode: size of gainCurveControlPointY < numAlternateImages\n"; return itm;}

        std::vector<std::vector<float>> gainCurveControlPointTheta;
        if (j.contains("gainCurveControlPointTheta")) {
          for (uint16_t iAlt = 0; iAlt < itm.cvt.hatm.numAlternateImages; iAlt++) { itm.hasSlopeParameter[iAlt] = true; } // [Todo: within the json there could be only some alternates that have the slope. How to manage this case. For now assume they all have it or not]
          gainCurveControlPointTheta = j["gainCurveControlPointTheta"].get<std::vector<std::vector<float>>>();
          if (gainCurveControlPointTheta.size()    < itm.cvt.hatm.numAlternateImages){ *error_raised = true; std::cerr << "JSON Decode: size of gainCurveControlPointTheta < numAlternateImages\n"; return itm;}
        } else {
          for (uint16_t iAlt = 0; iAlt < itm.cvt.hatm.numAlternateImages; iAlt++) { itm.hasSlopeParameter[iAlt] = false;}
        }

        // Color Gain Function 
        for (uint16_t iAlt = 0; iAlt < itm.cvt.hatm.numAlternateImages; iAlt++) {
          itm.isLinearInterpolation[iAlt]  = false; // [Todo: not supported currently]
          itm.cvt.hatm.alternateHdrHeadroom.push_back(alternateHdrHeadroom[iAlt]);

          ColorGainFunction cgf; 
          cgf.cm.componentMixRed       = componentMixRed[iAlt];
          cgf.cm.componentMixGreen     = componentMixGreen[iAlt];
          cgf.cm.componentMixBlue      = componentMixBlue[iAlt];
          cgf.cm.componentMixMax       = componentMixMax[iAlt];
          cgf.cm.componentMixMin       = componentMixMin[iAlt];
          cgf.cm.componentMixComponent = componentMixComponent[iAlt];

          // Gain Curve metadata
          cgf.gc.gainCurveNumControlPoints = gainCurveNumControlPoints[iAlt];
          
          // Check the size of outter element
          if (gainCurveControlPointX[iAlt].size() < gainCurveNumControlPoints[iAlt]){ *error_raised = true; std::cerr << "JSON Decode: size of gainCurveControlPointX[a] < gainCurveNumControlPoints\n"; return itm;}
          if (gainCurveControlPointY[iAlt].size() < gainCurveNumControlPoints[iAlt]){ *error_raised = true; std::cerr << "JSON Decode: size of gainCurveControlPointX[a] < gainCurveNumControlPoints\n"; return itm;}
          for (uint16_t iCp = 0; iCp < gainCurveNumControlPoints[iAlt]; iCp++) {
            cgf.gc.gainCurveControlPointX.push_back(gainCurveControlPointX[iAlt][iCp]);
            cgf.gc.gainCurveControlPointY.push_back(gainCurveControlPointY[iAlt][iCp]);
          }
          if (itm.hasSlopeParameter[iAlt]) {
            if (gainCurveControlPointTheta[iAlt].size() < gainCurveNumControlPoints[iAlt]){ *error_raised = true; std::cerr << "JSON Decode: size of gainCurveControlPointTheta[a] < gainCurveNumControlPoints\n"; return itm;}
            for (uint16_t iCp = 0; iCp < gainCurveNumControlPoints[iAlt]; iCp++) {
              cgf.gc.gainCurveControlPointTheta.push_back(gainCurveControlPointTheta[iAlt][iCp]);
            }
          }
          itm.cvt.hatm.cgf.push_back(cgf);
        }
      }
    }
  }
return itm;
}

// Convert metadata items to syntax elements 
SyntaxElements convertMetadataItemsToSyntaxElements(MetadataItems itm){

  SyntaxElements elm;

  // Init the application version:
  elm.application_version = 15;


  if (abs(itm.cvt.hdrReferenceWhite - 203.0) > Q_HDR_REFERENCE_WHITE / 2) {
    elm.has_custom_hdr_reference_white_flag = true;
    elm.hdr_reference_white = uint16_t(itm.cvt.hdrReferenceWhite * Q_HDR_REFERENCE_WHITE);
  }
  elm.has_adaptive_tone_map_flag = itm.isHeadroomAdaptiveToneMap;
  if (elm.has_adaptive_tone_map_flag){
    elm.baseline_hdr_headroom = uint16_t(itm.cvt.hatm.baselineHdrHeadroom * Q_HDR_HEADROOM);
  }
  elm.use_reference_white_tone_mapping_flag = itm.isReferenceWhiteToneMapping;
  if (!elm.use_reference_white_tone_mapping_flag){
    elm.num_alternate_images = uint16_t(itm.cvt.hatm.numAlternateImages - 1);
    // Check if the primary combination is known
    if (
      (itm.cvt.hatm.gainApplicationSpaceChromaticities[0] - 0.64  ) <  P_GAIN_APPLICATION_SPACE_CHROMATICITY &&
      (itm.cvt.hatm.gainApplicationSpaceChromaticities[1] - 0.33  ) <  P_GAIN_APPLICATION_SPACE_CHROMATICITY &&
      (itm.cvt.hatm.gainApplicationSpaceChromaticities[2] - 0.30  ) <  P_GAIN_APPLICATION_SPACE_CHROMATICITY &&
      (itm.cvt.hatm.gainApplicationSpaceChromaticities[3] - 0.60  ) <  P_GAIN_APPLICATION_SPACE_CHROMATICITY &&
      (itm.cvt.hatm.gainApplicationSpaceChromaticities[4] - 0.15  ) <  P_GAIN_APPLICATION_SPACE_CHROMATICITY &&
      (itm.cvt.hatm.gainApplicationSpaceChromaticities[5] - 0.06  ) <  P_GAIN_APPLICATION_SPACE_CHROMATICITY &&
      (itm.cvt.hatm.gainApplicationSpaceChromaticities[6] - 0.3127) <  P_GAIN_APPLICATION_SPACE_CHROMATICITY &&
      (itm.cvt.hatm.gainApplicationSpaceChromaticities[7] - 0.3290) < P_GAIN_APPLICATION_SPACE_CHROMATICITY){ 
        elm.gain_application_space_chromaticities_mode = 0;
      } 
      else if (
        (itm.cvt.hatm.gainApplicationSpaceChromaticities[0] - 0.68  ) <  P_GAIN_APPLICATION_SPACE_CHROMATICITY &&
        (itm.cvt.hatm.gainApplicationSpaceChromaticities[1] - 0.32  ) <  P_GAIN_APPLICATION_SPACE_CHROMATICITY &&
        (itm.cvt.hatm.gainApplicationSpaceChromaticities[2] - 0.265 ) <  P_GAIN_APPLICATION_SPACE_CHROMATICITY &&
        (itm.cvt.hatm.gainApplicationSpaceChromaticities[3] - 0.69  ) <  P_GAIN_APPLICATION_SPACE_CHROMATICITY &&
        (itm.cvt.hatm.gainApplicationSpaceChromaticities[4] - 0.15  ) <  P_GAIN_APPLICATION_SPACE_CHROMATICITY &&
        (itm.cvt.hatm.gainApplicationSpaceChromaticities[5] - 0.06  ) <  P_GAIN_APPLICATION_SPACE_CHROMATICITY &&
        (itm.cvt.hatm.gainApplicationSpaceChromaticities[6] - 0.3127) <  P_GAIN_APPLICATION_SPACE_CHROMATICITY &&
        (itm.cvt.hatm.gainApplicationSpaceChromaticities[7] - 0.329 ) < Q_GAIN_APPLICATION_SPACE_CHROMATICITY ) { 
        elm.gain_application_space_chromaticities_mode = 1;
      } else if (
        (itm.cvt.hatm.gainApplicationSpaceChromaticities[0] - 0.708 ) <  P_GAIN_APPLICATION_SPACE_CHROMATICITY &&
        (itm.cvt.hatm.gainApplicationSpaceChromaticities[1] - 0.292 ) <  P_GAIN_APPLICATION_SPACE_CHROMATICITY &&
        (itm.cvt.hatm.gainApplicationSpaceChromaticities[2] - 0.17  ) <  P_GAIN_APPLICATION_SPACE_CHROMATICITY &&
        (itm.cvt.hatm.gainApplicationSpaceChromaticities[3] - 0.797 ) <  P_GAIN_APPLICATION_SPACE_CHROMATICITY &&
        (itm.cvt.hatm.gainApplicationSpaceChromaticities[4] - 0.131 ) <  P_GAIN_APPLICATION_SPACE_CHROMATICITY &&
        (itm.cvt.hatm.gainApplicationSpaceChromaticities[5] - 0.046 ) <  P_GAIN_APPLICATION_SPACE_CHROMATICITY &&
        (itm.cvt.hatm.gainApplicationSpaceChromaticities[6] - 0.3127) <  P_GAIN_APPLICATION_SPACE_CHROMATICITY &&
        (itm.cvt.hatm.gainApplicationSpaceChromaticities[7] - 0.329 ) < P_GAIN_APPLICATION_SPACE_CHROMATICITY){ 
          elm.gain_application_space_chromaticities_mode = 2;
      } else {
        elm.gain_application_space_chromaticities_mode = 3;
        for (uint16_t iCh = 0; iCh < 8; iCh++) {
          elm.gain_application_space_chromaticities[iCh] = uint16_t(itm.cvt.hatm.gainApplicationSpaceChromaticities[iCh]* Q_GAIN_APPLICATION_SPACE_CHROMATICITY);
        }
      }

      // Check if all component mixing uses the same parameters 
      elm.has_common_component_component_mixing_coefficient_flag = true; // Init at true, any mismatch makes it false
      for (uint16_t iAlt = 1; iAlt < itm.cvt.hatm.numAlternateImages; iAlt++) {
        // Check if any coefficient is different across alterante images
        if (abs(itm.cvt.hatm.cgf[0].cm.componentMixRed       - itm.cvt.hatm.cgf[iAlt].cm.componentMixRed)       > Q_COMPONENT_MIXING_COEFFICIENT || 
            abs(itm.cvt.hatm.cgf[0].cm.componentMixGreen     - itm.cvt.hatm.cgf[iAlt].cm.componentMixGreen)     > Q_COMPONENT_MIXING_COEFFICIENT || 
            abs(itm.cvt.hatm.cgf[0].cm.componentMixBlue      - itm.cvt.hatm.cgf[iAlt].cm.componentMixBlue)      > Q_COMPONENT_MIXING_COEFFICIENT || 
            abs(itm.cvt.hatm.cgf[0].cm.componentMixMax       - itm.cvt.hatm.cgf[iAlt].cm.componentMixMax)       > Q_COMPONENT_MIXING_COEFFICIENT || 
            abs(itm.cvt.hatm.cgf[0].cm.componentMixMin       - itm.cvt.hatm.cgf[iAlt].cm.componentMixMin)       > Q_COMPONENT_MIXING_COEFFICIENT || 
            abs(itm.cvt.hatm.cgf[0].cm.componentMixComponent - itm.cvt.hatm.cgf[iAlt].cm.componentMixComponent) > Q_COMPONENT_MIXING_COEFFICIENT) {
          elm.has_common_component_component_mixing_coefficient_flag = false;
          break;
        }
      } 
      // Check if all alternate have the same number of control points and interpolation
      elm.has_common_curve_params_flag = true;  // Init at true, any mismatch makes it false
      for (uint16_t iAlt = 1; iAlt < itm.cvt.hatm.numAlternateImages; iAlt++) {
        if (itm.cvt.hatm.cgf[0].gc.gainCurveNumControlPoints != itm.cvt.hatm.cgf[iAlt].gc.gainCurveNumControlPoints){
          elm.has_common_component_component_mixing_coefficient_flag = false;
          break;
        }
      }

      // If they do, then check if all alternate have the same x position
      for (uint16_t pIdx = 0; pIdx < itm.cvt.hatm.cgf[0].gc.gainCurveNumControlPoints; pIdx++){
        for (uint16_t iAlt = 1; iAlt < itm.cvt.hatm.numAlternateImages; iAlt++) {
          if (itm.cvt.hatm.cgf[0].gc.gainCurveControlPointX[pIdx] - itm.cvt.hatm.cgf[iAlt].gc.gainCurveControlPointX[pIdx] > Q_GAIN_CURVE_CONTROL_POINT_X){
            elm.has_common_component_component_mixing_coefficient_flag = false;
            break;
          }
        }
      } 
      
      // Loop over alternate images
      for (uint16_t iAlt = 0; iAlt < itm.cvt.hatm.numAlternateImages; iAlt++) {
        elm.alternate_hdr_headrooms[iAlt] = uint16_t(itm.cvt.hatm.alternateHdrHeadroom[iAlt] * Q_HDR_HEADROOM);

        // init coefficient to 0
        for (uint16_t iCmf = 0; iCmf < MAX_NB_component_mixing_coefficient; iCmf++){
          elm.component_mixing_coefficient[iAlt][iCmf] = uint16_t(0);
          elm.has_component_mixing_coefficient_flag[iAlt][iCmf] = false;
        }
        // Component mixing
        if (
          abs(itm.cvt.hatm.cgf[iAlt].cm.componentMixRed      ) < P_COMPONENT_MIXING_COEFFICIENT &&
          abs(itm.cvt.hatm.cgf[iAlt].cm.componentMixGreen    ) < P_COMPONENT_MIXING_COEFFICIENT &&
          abs(itm.cvt.hatm.cgf[iAlt].cm.componentMixBlue     ) < P_COMPONENT_MIXING_COEFFICIENT &&
          abs(itm.cvt.hatm.cgf[iAlt].cm.componentMixMax- 1.0 ) < P_COMPONENT_MIXING_COEFFICIENT &&
          abs(itm.cvt.hatm.cgf[iAlt].cm.componentMixMin      ) < P_COMPONENT_MIXING_COEFFICIENT &&
          abs(itm.cvt.hatm.cgf[iAlt].cm.componentMixComponent) < P_COMPONENT_MIXING_COEFFICIENT){ 
            elm.component_mixing_type[iAlt] = 0;
        } else if (
          abs(itm.cvt.hatm.cgf[iAlt].cm.componentMixRed      ) < P_COMPONENT_MIXING_COEFFICIENT &&
          abs(itm.cvt.hatm.cgf[iAlt].cm.componentMixGreen    ) < P_COMPONENT_MIXING_COEFFICIENT &&
          abs(itm.cvt.hatm.cgf[iAlt].cm.componentMixBlue     ) < P_COMPONENT_MIXING_COEFFICIENT &&
          abs(itm.cvt.hatm.cgf[iAlt].cm.componentMixMax      ) < P_COMPONENT_MIXING_COEFFICIENT &&
          abs(itm.cvt.hatm.cgf[iAlt].cm.componentMixMin      ) < P_COMPONENT_MIXING_COEFFICIENT &&
          abs(itm.cvt.hatm.cgf[iAlt].cm.componentMixComponent - 1.0 ) < P_COMPONENT_MIXING_COEFFICIENT){ 
            elm.component_mixing_type[iAlt] = 1;
        }  else if (
          abs(itm.cvt.hatm.cgf[iAlt].cm.componentMixRed   - (1.0 / 6.0)) < P_COMPONENT_MIXING_COEFFICIENT &&
          abs(itm.cvt.hatm.cgf[iAlt].cm.componentMixGreen - (1.0 / 6.0)) < P_COMPONENT_MIXING_COEFFICIENT &&
          abs(itm.cvt.hatm.cgf[iAlt].cm.componentMixBlue  - (1.0 / 6.0)) < P_COMPONENT_MIXING_COEFFICIENT &&
          abs(itm.cvt.hatm.cgf[iAlt].cm.componentMixMax   - (1.0 / 2.0)) < P_COMPONENT_MIXING_COEFFICIENT &&
          abs(itm.cvt.hatm.cgf[iAlt].cm.componentMixMin      ) < P_COMPONENT_MIXING_COEFFICIENT &&
          abs(itm.cvt.hatm.cgf[iAlt].cm.componentMixComponent) < P_COMPONENT_MIXING_COEFFICIENT){ 
            elm.component_mixing_type[iAlt] = 2;
        } else { // Send flag to true for each non-zero coefficient
          uint16_t iCmf = 0;
          // 
          elm.component_mixing_coefficient[iAlt][iCmf] = uint16_t(itm.cvt.hatm.cgf[iAlt].cm.componentMixRed       * Q_COMPONENT_MIXING_COEFFICIENT); iCmf++;
          elm.component_mixing_coefficient[iAlt][iCmf] = uint16_t(itm.cvt.hatm.cgf[iAlt].cm.componentMixGreen     * Q_COMPONENT_MIXING_COEFFICIENT); iCmf++;
          elm.component_mixing_coefficient[iAlt][iCmf] = uint16_t(itm.cvt.hatm.cgf[iAlt].cm.componentMixBlue      * Q_COMPONENT_MIXING_COEFFICIENT); iCmf++;
          elm.component_mixing_coefficient[iAlt][iCmf] = uint16_t(itm.cvt.hatm.cgf[iAlt].cm.componentMixMax       * Q_COMPONENT_MIXING_COEFFICIENT); iCmf++;
          elm.component_mixing_coefficient[iAlt][iCmf] = uint16_t(itm.cvt.hatm.cgf[iAlt].cm.componentMixMin       * Q_COMPONENT_MIXING_COEFFICIENT); iCmf++;
          elm.component_mixing_coefficient[iAlt][iCmf] = uint16_t(itm.cvt.hatm.cgf[iAlt].cm.componentMixComponent * Q_COMPONENT_MIXING_COEFFICIENT); iCmf++;
          uint16_t sumCoefficients = 0;
          for (iCmf = 0; iCmf < MAX_NB_component_mixing_coefficient; iCmf++){
            if (elm.component_mixing_coefficient[iAlt][iCmf] > 0 && elm.component_mixing_coefficient[iAlt][iCmf] <= 10000){
              elm.has_component_mixing_coefficient_flag[iAlt][iCmf] = true;
            } else if (elm.component_mixing_coefficient[iAlt][iCmf] == 0) {
              elm.has_component_mixing_coefficient_flag[iAlt][iCmf] = false;
            } else {
              std::cerr << "Error: component mixing coefficient for alternate " << iAlt << " color " << iCmf << "is greater than 1.0\n";
            }
            sumCoefficients = sumCoefficients + elm.component_mixing_coefficient[iAlt][iCmf];
          }
          if (sumCoefficients != 60000) { std::cerr << "Error: sum component mixing coefficient for alternate " << iAlt << " color " << iCmf << "is greater than 1.0\n"; }
        }

        // Create syntax elements for the gain curve function
        // elm.gain_curve_interpolation[iAlt] = itm.cvt.hatm.cgf[iAlt].gc.gainCurveInterpolation;
        elm.gain_curve_num_control_points_minus_1[iAlt] = itm.cvt.hatm.cgf[iAlt].gc.gainCurveNumControlPoints - 1;
        for (uint16_t iCps = 0; iCps < itm.cvt.hatm.cgf[iAlt].gc.gainCurveNumControlPoints; iCps++){
          elm.gain_curve_control_points_x[iAlt][iCps] = uint16_t(itm.cvt.hatm.cgf[iAlt].gc.gainCurveControlPointX[iCps] * Q_GAIN_CURVE_CONTROL_POINT_X );
          elm.gain_curve_control_points_y[iAlt][iCps] = uint16_t((itm.cvt.hatm.cgf[iAlt].gc.gainCurveControlPointY[iCps]  + O_GAIN_CURVE_CONTROL_POINT_Y ) * Q_GAIN_CURVE_CONTROL_POINT_Y );
        }
        elm.gain_curve_use_pchip_slope_flag[iAlt] = !itm.hasSlopeParameter[iAlt];
        if (!elm.gain_curve_use_pchip_slope_flag[iAlt]) {
          for (uint16_t iCps = 0; iCps < itm.cvt.hatm.cgf[iAlt].gc.gainCurveNumControlPoints; iCps++) {
            elm.gain_curve_control_points_theta[iAlt][iCps] = uint16_t((itm.cvt.hatm.cgf[iAlt].gc.gainCurveControlPointTheta[iCps] + O_GAIN_CURVE_CONTROL_POINT_THETA) * Q_GAIN_CURVE_CONTROL_POINT_THETA );
          }
        }
      }
    }
    dbgPrintMilestones(588, "Finished convertMetadataItemsToSyntaxElements", 0);
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
    // Write the uint16_t value directly to the file
    // Reinterpret_cast is used to treat the memory location of 'value' as a sequence of bytes
    
    //[Todo: for some reason I have less bytes for the test json (58 versus 60)]
    //[Todo: binary from matlab seems to be different endian than what this tool does. To Verify]
    std::cout << "++++++++++++++++++++++ Write binary data to intermediate file ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n";  
    printDebug("application_version", elm.application_version, 8);
    value_8 = elm.application_version; 
    outFile.write(reinterpret_cast<const char*>(&value_8), sizeof(value_8));

    printDebug("has_custom_hdr_reference_white_flag",elm.has_custom_hdr_reference_white_flag, 1);
    printDebug("has_adaptive_tone_map_flag",elm.has_adaptive_tone_map_flag, 1);
    value_8 = (elm.has_custom_hdr_reference_white_flag << 7) + (elm.has_adaptive_tone_map_flag << 6);
    outFile.write(reinterpret_cast<const char*>(&value_8), sizeof(value_8));

    if (elm.has_custom_hdr_reference_white_flag){
      printDebug("hdr_reference_white", elm.hdr_reference_white, 16);
      value_8 = uint8_t((elm.hdr_reference_white >> 8) & 0x00FF); outFile.write(reinterpret_cast<const char*>(&value_8), sizeof(value_8));
      value_8 = uint8_t((elm.hdr_reference_white     ) & 0x00FF); outFile.write(reinterpret_cast<const char*>(&value_8), sizeof(value_8));
    }
    
    if (elm.has_adaptive_tone_map_flag) {
      printDebug("baseline_hdr_headroom", elm.baseline_hdr_headroom, 16);
      value_8 = uint8_t((elm.baseline_hdr_headroom >> 8) & 0x00FF); outFile.write(reinterpret_cast<const char*>(&value_8), sizeof(value_8));
      value_8 = uint8_t((elm.baseline_hdr_headroom     ) & 0x00FF); outFile.write(reinterpret_cast<const char*>(&value_8), sizeof(value_8));

      printDebug("use_reference_white_tone_mapping_flag",elm.use_reference_white_tone_mapping_flag, 1);
      if (!elm.use_reference_white_tone_mapping_flag) {
        printDebug("num_alternate_images",elm.num_alternate_images, 3);
        printDebug("gain_application_space_chromaticities_mode",elm.gain_application_space_chromaticities_mode, 2);
        printDebug("has_common_component_component_mixing_coefficient_flag",elm.has_common_component_component_mixing_coefficient_flag, 1);
        printDebug("has_common_curve_params_flag",elm.has_common_curve_params_flag, 1);
        value_8 = (elm.use_reference_white_tone_mapping_flag << 7) + (elm.num_alternate_images << 4) + (elm.gain_application_space_chromaticities_mode << 2) + 
        (elm.has_common_component_component_mixing_coefficient_flag << 1) + (elm.has_common_curve_params_flag );
        outFile.write(reinterpret_cast<const char*>(&value_8), sizeof(value_8));
  
        if (elm.gain_application_space_chromaticities_mode == 3) {
          for (uint16_t iCh = 0; iCh < 8; iCh++) {
            printDebug("gain_application_space_chromaticities", elm.gain_application_space_chromaticities[iCh], 16);
            value_8 = uint8_t((elm.gain_application_space_chromaticities[iCh] >> 8) & 0x00FF); outFile.write(reinterpret_cast<const char*>(&value_8), sizeof(value_8));
            value_8 = uint8_t((elm.gain_application_space_chromaticities[iCh]     ) & 0x00FF); outFile.write(reinterpret_cast<const char*>(&value_8), sizeof(value_8));
          }
        }

        for (uint16_t iAlt = 0; iAlt < elm.num_alternate_images + 1; iAlt++) {
          printDebug("alternate_hdr_headrooms[iAlt]", elm.alternate_hdr_headrooms[iAlt], 16);
          value_8 = uint8_t((elm.alternate_hdr_headrooms[iAlt] >> 8) & 0x00FF); outFile.write(reinterpret_cast<const char*>(&value_8), sizeof(value_8));
          value_8 = uint8_t((elm.alternate_hdr_headrooms[iAlt]     ) & 0x00FF); outFile.write(reinterpret_cast<const char*>(&value_8), sizeof(value_8));
  
          // Write component mixing function parameters
          if ( iAlt == 0 || !elm.has_common_component_component_mixing_coefficient_flag){
            printDebug("component_mixing_type[iAlt]", elm.component_mixing_type[iAlt], 2);
            value_8 = (elm.component_mixing_type[iAlt] << 6);
            if (elm.component_mixing_type[iAlt] == 3) {
              // Write the flag to indicate which coefficients are signaled 
              for (uint8_t iCm = 0; iCm < MAX_NB_component_mixing_coefficient; iCm++) {
                uint8_t flagValue = static_cast<uint8_t>(elm.has_component_mixing_coefficient_flag[iAlt][iCm]);
                value_8 = value_8 + (flagValue << (5 - iCm) );
              }   
              outFile.write(reinterpret_cast<const char*>(&value_8), sizeof(value_8)); 
              // Write the coefficients 
              for (uint8_t iCm = 0; iCm < MAX_NB_component_mixing_coefficient; iCm++) {
                if (elm.has_component_mixing_coefficient_flag[iAlt][iCm]) {
                  printDebug("component_mixing_coefficient", elm.component_mixing_coefficient[iAlt][iCm], 16);
                  value_8 = uint8_t((elm.component_mixing_coefficient[iAlt][iCm] >> 8) & 0x00FF); outFile.write(reinterpret_cast<const char*>(&value_8), sizeof(value_8));
                  value_8 = uint8_t((elm.component_mixing_coefficient[iAlt][iCm]     ) & 0x00FF); outFile.write(reinterpret_cast<const char*>(&value_8), sizeof(value_8));
                }
              }                
            } else {
              outFile.write(reinterpret_cast<const char*>(&value_8), sizeof(value_8)); 
            }
          }
  
          /// Write gain curve function parameters
          if ( iAlt == 0 || elm.has_common_curve_params_flag){
            //printDebug("gain_curve_interpolation[iAlt]",elm.gain_curve_interpolation[iAlt], 2);
            printDebug("gain_curve_num_control_points_minus_1[iAlt]",elm.gain_curve_num_control_points_minus_1[iAlt], 5);
            printDebug("gain_curve_use_pchip_slope_flag",elm.gain_curve_use_pchip_slope_flag[iAlt], 1);
            value_8 = (elm.gain_curve_num_control_points_minus_1[iAlt] << 3) + (elm.gain_curve_use_pchip_slope_flag[iAlt] << 2) ;
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
          if (!elm.gain_curve_use_pchip_slope_flag[iAlt]) {
            for (uint16_t iCps = 0; iCps < elm.gain_curve_num_control_points_minus_1[iAlt] + 1; iCps++) {
              printDebug("gain_curve_control_points_theta[iAlt][iCps]",elm.gain_curve_control_points_theta[iAlt][iCps], 16);
              value_8 = uint8_t((elm.gain_curve_control_points_theta[iAlt][iCps] >> 8) & 0x00FF); outFile.write(reinterpret_cast<const char*>(&value_8), sizeof(value_8));
              value_8 = uint8_t((elm.gain_curve_control_points_theta[iAlt][iCps]     ) & 0x00FF); outFile.write(reinterpret_cast<const char*>(&value_8), sizeof(value_8));
            }
          }
        }
      }
    }
    else{ // No more information need to be signaled when using Reference White Tone Mapping Operator
      value_8 = (elm.use_reference_white_tone_mapping_flag << 7);
      outFile.write(reinterpret_cast<const char*>(&value_8), sizeof(value_8));
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
        std::cerr << "Skipping " << path << " (missing frame_start || frame_duration)\n";
        continue;
      }

      frame_start    = j["frame_start"].get<uint32_t>();
      frame_duration = j["frame_duration"].get<uint32_t>();

      std::cout << "++++++++++++++++++++++Start processing : " << path << "+++++++++++++++++++++++\n"; 

      // Decode Metadata Items from json
      bool error_raised = false;
      bool *error_raised_ptr = &error_raised;




      MetadataItems itm = decodeJsonToMetadataItems(j, path, error_raised_ptr);
      dbgPrintMetadataItems(itm, false); // print up to what was decoded 
      if (error_raised) {
        std::cerr << "Skipping " << path << " error decoding json file\n";
        continue;
      }

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
      MetadataItem item{itm.timeI.timeIntervalStart, itm.timeI.timeintervalDuration, binPathGen.string()};
      items[frame_start] = item;
    }
  }
  return items;
}

/* *********************************** DECODING SECTION ********************************************************************************************/

/*
// Decode binary data into syntax elements
SyntaxElements decodeBinaryToSyntaxElements(std::vector<uint8_t> binary_data) {

  const int kMixEncodingNumParams[8] = {0, 0, 0, 1, 1 ,2, 3, 4};
  uint16_t decoded_sample = 0;
  SyntaxElements elements;

  // printDebug("(binary_data[decoded_sample] & 0xFF)",(binary_data[decoded_sample] & 0xFF), 8);
  std::cout << "++++++++++++++++++++++Syntax Elements Decoding ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n"; 
  elements.application_version = (binary_data[decoded_sample] & 0xFF);
  printDebug("application_version",elements.application_version, 8);
  decoded_sample++;

  elements.has_custom_hdr_reference_white_flag = (binary_data[decoded_sample] & 0x80) >> 7;
  elements.tone_map_mode = (binary_data[decoded_sample] & 0x70) >>  4;
  printDebug("has_custom_hdr_reference_white_flag",elements.has_custom_hdr_reference_white_flag, 1);
  printDebug("tone_map_mode",elements.tone_map_mode, 3);
  decoded_sample++;

  
  if (elements.has_custom_hdr_reference_white_flag){
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
    elements.num_alternate_images     = (binary_data[decoded_sample] & 0xC0) >> 6;
    elements.gain_application_space_primaries = (binary_data[decoded_sample] & 0x30) >> 4;
    elements.has_gain_application_offset_flag = (binary_data[decoded_sample] & 0x08) >> 3;
    elements.has_common_component_component_mixing_coefficient_flag       = (binary_data[decoded_sample] & 0x04) >> 2;
    elements.has_common_curve_params_flag     = (binary_data[decoded_sample] & 0x02) >> 1;
    printDebug("num_alternate_images",elements.num_alternate_images, 2);
    printDebug("gain_application_space_primaries",elements.gain_application_space_primaries, 2);
    printDebug("has_gain_application_offset_flag",elements.has_gain_application_offset_flag, 1);
    printDebug("has_common_component_component_mixing_coefficient_flag",elements.has_common_component_component_mixing_coefficient_flag, 1);
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
    for (uint16_t iAlt = 0; iAlt < elements.num_alternate_images + 1; iAlt++) {
      elements.alternate_hdr_headrooms[iAlt] = (uint16_t(binary_data[decoded_sample]) << 8) + (uint16_t(binary_data[decoded_sample+1]) );
      printDebug("alternate_hdr_headrooms[iAlt]",elements.alternate_hdr_headrooms[iAlt], 16);
      decoded_sample++;
      decoded_sample++;
      // Write component mixing function parameters
      if ( iAlt == 0 || !elements.has_common_component_component_mixing_coefficient_flag){
        elements.mix_encoding[iAlt] = uint16_t(binary_data[decoded_sample] & 0xE0) >> 5;
        printDebug("mix_encoding[iAlt]",elements.mix_encoding[iAlt], 3);
        decoded_sample++;           
        for (uint16_t iPar = 0; iPar < kMixEncodingNumParams[elements.mix_encoding[iAlt]]; iPar++){
          elements.component_mixing_coefficient[iAlt][iPar] = (uint16_t(binary_data[decoded_sample]) << 8) + (uint16_t(binary_data[decoded_sample+1]) );
          printDebug("component_mixing_coefficient[iAlt][iPar]",elements.component_mixing_coefficient[iAlt][iPar], 16);
          decoded_sample++;
          decoded_sample++;
        }
      } else {
        elements.mix_encoding[iAlt] = elements.mix_encoding[0];
        for (uint16_t iPar = 0; iPar < kMixEncodingNumParams[elements.mix_encoding[iAlt]]; iPar++){
          elements.component_mixing_coefficient[iAlt][iPar] = elements.component_mixing_coefficient[0][iPar];
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
*/

/*
// Convert the syntax elements to Metadata Items as described in Clause C.3
MetadataItems convertSyntaxElementsToMetadataItems(SyntaxElements elm){
  MetadataItems itm;
  bool error_raised = false;
  // get mandatory metadata
  itm.windowNumber     = 1;
  if (elm.has_custom_hdr_reference_white_flag) {
    itm.cvt.hdrReferenceWhite = float(elm.hdr_reference_white) / Q_HDR_REFERENCE_WHITE;
  } else {
    itm.cvt.hdrReferenceWhite = 203.0;
  }
  itm.toneMapMode = elm.tone_map_mode;

  // Get Optional metadata items
  if (itm.toneMapMode != 0) {
    itm.cvt.hatm.baselineHdrHeadroom = float(elm.baseline_hdr_headroom) / Q_HDR_HEADROOM;;
  }
  if (itm.toneMapMode == 3) {
    itm.cvt.hatm.numAlternateImages = 2;
    // BT.2020 primaries
    itm.cvt.hatm.gainApplicationSpaceChromaticities.push_back(0.708);
    itm.cvt.hatm.gainApplicationSpaceChromaticities.push_back(0.292);
    itm.cvt.hatm.gainApplicationSpaceChromaticities.push_back(0.17);
    itm.cvt.hatm.gainApplicationSpaceChromaticities.push_back(0.797);
    itm.cvt.hatm.gainApplicationSpaceChromaticities.push_back(0.131);
    itm.cvt.hatm.gainApplicationSpaceChromaticities.push_back(0.046);
    itm.cvt.hatm.gainApplicationSpaceChromaticities.push_back(0.3127);
    itm.cvt.hatm.gainApplicationSpaceChromaticities.push_back(0.329);
    // Compute alternate headroom
    itm.cvt.hatm.alternateHdrHeadroom.push_back(0.0);
    float headroom_to_anchor_ratio = std::min(itm.cvt.hatm.baselineHdrHeadroom / log2(1000.0/203.0), 1.0);
    float h_alt_1 = log2(8.0/3.0) * headroom_to_anchor_ratio;
    itm.cvt.hatm.alternateHdrHeadroom.push_back(h_alt_1);

    // Constant parameter across alternate images
    float kappa  = 0.65;
    float x_knee = 1;
    float x_max  = pow(2.0, itm.cvt.hatm.baselineHdrHeadroom);
    for (uint16_t iAlt = 0;  iAlt  < itm.cvt.hatm.numAlternateImages; iAlt++){
      // Component mixing is maxRGB
      itm.cvt.hatm.cgf[iAlt].cm.componentMixRed.push_back(0.0);
      itm.cvt.hatm.cgf[iAlt].cm.componentMixGreen.push_back(0.0);
      itm.cvt.hatm.cgf[iAlt].cm.componentMixBlue.push_back(0.0);
      itm.cvt.hatm.cgf[iAlt].cm.componentMixMax.push_back(1.0);
      itm.cvt.hatm.cgf[iAlt].cm.componentMixMin.push_back(0.0);
      itm.cvt.hatm.cgf[iAlt].cm.componentMixComponent.push_back(0.0); 
      itm.cvt.hatm.cgf[iAlt].gc.gainCurveInterpolation.push_back(2);
      itm.cvt.hatm.cgf[iAlt].gc.gainCurveNumControlPoints.push_back(8);

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
      float y_max  = pow(2.0, itm.cvt.hatm.alternateHdrHeadroom[iAlt]);
      float x_mid = (1.0 - kappa) * x_knee + kappa * (x_knee * y_max / y_knee);
      float y_mid = (1.0 - kappa) * y_knee + kappa * y_max;
      // Compute Quadratic Beziers coefficients
      float a_x = x_knee - 2 * x_mid + x_max;
      float a_y = y_knee - 2 * y_mid + y_max;
      float b_x = 2 * x_mid - 2 * x_knee;
      float b_y = 2 * y_mid - 2 * y_knee;
      float c_x = x_knee;
      float c_y = y_knee;

      for (uint16_t iCps = 0; iCps < itm.cvt.hatm.cgf[iAlt].gc.gainCurveNumControlPoints; iCps++) {
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
      itm.cvt.hatm.cgf[iAlt].gc.gainCurveControlPointX.push_back(inner_gainCurveControlPointX);
      itm.cvt.hatm.cgf[iAlt].gc.gainCurveControlPointY.push_back(inner_gainCurveControlPointY);
      itm.cvt.hatm.cgf[iAlt].gc.gainCurveControlPointTheta.push_back(inner_gainCurveControlPointTheta);
    }
  }
  if (itm.toneMapMode == 4){
    itm.cvt.hatm.numAlternateImages = elm.num_alternate_images + 1;
    if (elm.gain_application_space_chromaticities_mode == 0){
      itm.cvt.hatm.gainApplicationSpaceChromaticities.push_back(0.64);
      itm.cvt.hatm.gainApplicationSpaceChromaticities.push_back(0.33); 
      itm.cvt.hatm.gainApplicationSpaceChromaticities.push_back(0.3); 
      itm.cvt.hatm.gainApplicationSpaceChromaticities.push_back(0.6);
      itm.cvt.hatm.gainApplicationSpaceChromaticities.push_back(0.15); 
      itm.cvt.hatm.gainApplicationSpaceChromaticities.push_back(0.06);
      itm.cvt.hatm.gainApplicationSpaceChromaticities.push_back(0.3127);
      itm.cvt.hatm.gainApplicationSpaceChromaticities.push_back(0.329);
    } else if (elm.gain_application_space_chromaticities_mode == 1){
      itm.cvt.hatm.gainApplicationSpaceChromaticities.push_back(0.68); 
      itm.cvt.hatm.gainApplicationSpaceChromaticities.push_back(0.32); 
      itm.cvt.hatm.gainApplicationSpaceChromaticities.push_back(0.265); 
      itm.cvt.hatm.gainApplicationSpaceChromaticities.push_back(0.69); 
      itm.cvt.hatm.gainApplicationSpaceChromaticities.push_back(0.15); 
      itm.cvt.hatm.gainApplicationSpaceChromaticities.push_back(0.06); 
      itm.cvt.hatm.gainApplicationSpaceChromaticities.push_back(0.3127); 
      itm.cvt.hatm.gainApplicationSpaceChromaticities.push_back(0.329); 
    } else if (elm.gain_application_space_chromaticities_mode == 2){
      itm.cvt.hatm.gainApplicationSpaceChromaticities.push_back(0.708);
      itm.cvt.hatm.gainApplicationSpaceChromaticities.push_back(0.292);
      itm.cvt.hatm.gainApplicationSpaceChromaticities.push_back(0.17);
      itm.cvt.hatm.gainApplicationSpaceChromaticities.push_back(0.797);
      itm.cvt.hatm.gainApplicationSpaceChromaticities.push_back(0.131);
      itm.cvt.hatm.gainApplicationSpaceChromaticities.push_back(0.046);
      itm.cvt.hatm.gainApplicationSpaceChromaticities.push_back(0.3127);
      itm.cvt.hatm.gainApplicationSpaceChromaticities.push_back(0.329);
    } else if (elm.gain_application_space_chromaticities_mode == 3){
        for (uint16_t iCh = 0; iCh < 8; iCh++) {
          itm.cvt.hatm.gainApplicationSpaceChromaticities.push_back(float(elm.gain_application_space_chromaticities[iCh]) / Q_GAIN_APPLICATION_SPACE_CHROMATICITY);
        }
    } else {
      std::cerr << "gain_application_space_primaries=" << elm.gain_application_space_chromaticities_mode << "  not defined.\n";
      error_raised = true;
    }
    for (uint16_t iAlt = 0;  iAlt  < itm.cvt.hatm.numAlternateImages; iAlt++){

      itm.cvt.hatm.alternateHdrHeadroom.push_back(float(elm.alternate_hdr_headrooms[iAlt]) / Q_HDR_HEADROOM);
      // init k_params to zero and replace the one that are not
      itm.cvt.hatm.cgf[iAlt].cm.componentMixRed.push_back(0.0);
      itm.cvt.hatm.cgf[iAlt].cm.componentMixGreen.push_back(0.0);
      itm.cvt.hatm.cgf[iAlt].cm.componentMixBlue.push_back(0.0);
      itm.cvt.hatm.cgf[iAlt].cm.componentMixMax.push_back(0.0);
      itm.cvt.hatm.cgf[iAlt].cm.componentMixMin.push_back(0.0);
      itm.cvt.hatm.cgf[iAlt].cm.componentMixComponent.push_back(0.0); 
      if (elm.component_mixing_type[iAlt] == 0){
        itm.cvt.hatm.cgf[iAlt].cm.componentMixMax   = 1.0;
      } else if (elm.component_mixing_type[iAlt] == 1){
        itm.cvt.hatm.cgf[iAlt].cm.componentMixComponent   = 1.0;
      } else if (elm.component_mixing_type[iAlt] == 2){
        itm.cvt.hatm.cgf[iAlt].cm.componentMixRed   = 1.0 / 6.0;
        itm.cvt.hatm.cgf[iAlt].cm.componentMixGreen = 1.0 / 6.0;
        itm.cvt.hatm.cgf[iAlt].cm.componentMixBlue  = 1.0 / 6.0;
        itm.cvt.hatm.cgf[iAlt].cm.componentMixMax   = 1.0 / 2.0;
      } else if (elm.component_mixing_type[iAlt] == 3){
        itm.cvt.hatm.cgf[iAlt].cm.componentMixRed   = float(elm.component_mixing_type[iAlt][0]) / Q_COMPONENT_MIXING_COEFFICIENT / 3.0;
        itm.cvt.hatm.cgf[iAlt].cm.componentMixGreen = float(elm.component_mixing_type[iAlt][0]) / Q_COMPONENT_MIXING_COEFFICIENT / 3.0;
        itm.cvt.hatm.cgf[iAlt].cm.componentMixBlue  = float(elm.component_mixing_type[iAlt][0]) / Q_COMPONENT_MIXING_COEFFICIENT / 3.0;
        itm.cvt.hatm.cgf[iAlt].cm.componentMixMax   = 1.0 - float(elm.component_mixing_type[iAlt][0]) / Q_COMPONENT_MIXING_COEFFICIENT;
      }  else if (elm.component_mixing_type[iAlt] == 4){
        itm.cvt.hatm.cgf[iAlt].cm.componentMixMax   = float(elm.component_mixing_type[iAlt][0]) / Q_COMPONENT_MIXING_COEFFICIENT;
        itm.cvt.hatm.cgf[iAlt].cm.componentMixComponent   = 1.0 - float(elm.component_mixing_type[iAlt][0]) / Q_COMPONENT_MIXING_COEFFICIENT;
      } else if (elm.component_mixing_type[iAlt] == 5){
        itm.cvt.hatm.cgf[iAlt].cm.componentMixRed   = float(elm.component_mixing_type[iAlt][0]) / Q_COMPONENT_MIXING_COEFFICIENT;
        itm.cvt.hatm.cgf[iAlt].cm.componentMixGreen = float(elm.component_mixing_type[iAlt][1]) / Q_COMPONENT_MIXING_COEFFICIENT;
        itm.cvt.hatm.cgf[iAlt].cm.componentMixBlue  = 1.0 - (float(elm.component_mixing_type[iAlt][1]) + float((elm.component_mixing_type[iAlt][0]))) / Q_COMPONENT_MIXING_COEFFICIENT;
      } else if (elm.component_mixing_type[iAlt] == 6){
        itm.cvt.hatm.cgf[iAlt].cm.componentMixRed   = float(elm.component_mixing_type[iAlt][0]) / Q_COMPONENT_MIXING_COEFFICIENT / 3.0;
        itm.cvt.hatm.cgf[iAlt].cm.componentMixGreen = float(elm.component_mixing_type[iAlt][0]) / Q_COMPONENT_MIXING_COEFFICIENT / 3.0;
        itm.cvt.hatm.cgf[iAlt].cm.componentMixBlue  = float(elm.component_mixing_type[iAlt][0]) / Q_COMPONENT_MIXING_COEFFICIENT / 3.0;
        itm.cvt.hatm.cgf[iAlt].cm.componentMixMax   = float(elm.component_mixing_type[iAlt][1]) / Q_COMPONENT_MIXING_COEFFICIENT;
        itm.cvt.hatm.cgf[iAlt].cm.componentMixMin   = float(elm.component_mixing_type[iAlt][2]) / Q_COMPONENT_MIXING_COEFFICIENT;
        itm.cvt.hatm.cgf[iAlt].cm.componentMixComponent   = 1.0 - (float(elm.component_mixing_type[iAlt][0]) + float(elm.component_mixing_type[iAlt][1]) + float(elm.component_mixing_type[iAlt][2])) / Q_COMPONENT_MIXING_COEFFICIENT;
      } else if (elm.component_mixing_type[iAlt] == 7){
        itm.cvt.hatm.cgf[iAlt].cm.componentMixRed   = float(elm.component_mixing_type[iAlt][0]) / Q_COMPONENT_MIXING_COEFFICIENT;
        itm.cvt.hatm.cgf[iAlt].cm.componentMixGreen = float(elm.component_mixing_type[iAlt][1]) / Q_COMPONENT_MIXING_COEFFICIENT;
        itm.cvt.hatm.cgf[iAlt].cm.componentMixBlue  = float(elm.component_mixing_type[iAlt][2]) / Q_COMPONENT_MIXING_COEFFICIENT;
        itm.cvt.hatm.cgf[iAlt].cm.componentMixMax   = float(elm.component_mixing_type[iAlt][3]) / Q_COMPONENT_MIXING_COEFFICIENT;
        itm.cvt.hatm.cgf[iAlt].cm.componentMixComponent   = 1.0 - (float(elm.component_mixing_type[iAlt][0]) + float(elm.component_mixing_type[iAlt][1]) + 
                                            float(elm.component_mixing_type[iAlt][2]) + float(elm.component_mixing_type[iAlt][3])) / Q_COMPONENT_MIXING_COEFFICIENT;
      } else {
        std::cerr << "mix_encoding[" << iAlt << "]=" << elm.component_mixing_type[iAlt] << "  not defined.\n";
        error_raised = true;
      }
      itm.cvt.hatm.cgf[iAlt].gc.gainCurveInterpolation.push_back(elm.gain_curve_interpolation[iAlt]);
      itm.cvt.hatm.cgf[iAlt].gc.gainCurveNumControlPoints.push_back(elm.gain_curve_num_control_points_minus_1[iAlt] + 1);

      // Inner vector for push_back
      std::vector<float> inner_gainCurveControlPointX;
      std::vector<float> inner_gainCurveControlPointY;
      std::vector<float> inner_gainCurveControlPointTheta;
      for (uint16_t iCps = 0; iCps < itm.cvt.hatm.cgf[iAlt].gc.gainCurveNumControlPoints; iCps++) {
        inner_gainCurveControlPointX.push_back(float(elm.gain_curve_control_points_x[iAlt][iCps]) / Q_GAIN_CURVE_CONTROL_POINT_X);
        inner_gainCurveControlPointY.push_back(float(elm.gain_curve_control_points_y[iAlt][iCps]) / Q_GAIN_CURVE_CONTROL_POINT_Y - O_GAIN_CURVE_CONTROL_POINT_Y);
        if (itm.gainCurveInterpolation == 2) {
          inner_gainCurveControlPointTheta.push_back(float(elm.gain_curve_control_points_theta[iAlt][iCps]) / Q_GAIN_CURVE_CONTROL_POINT_THETA - O_GAIN_CURVE_CONTROL_POINT_THETA);
        }
      }
      itm.cvt.hatm.cgf[iAlt].gc.gainCurveControlPointX.push_back(inner_gainCurveControlPointX);
      itm.cvt.hatm.cgf[iAlt].gc.gainCurveControlPointY.push_back(inner_gainCurveControlPointY);
      itm.cvt.hatm.cgf[iAlt].gc.gainCurveControlPointTheta.push_back(inner_gainCurveControlPointTheta);
    }
  }
  dbgPrintMetadataItems(itm, true);
  return itm;
}
*/

/*
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
*/

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
    // decodeBinaryData(outFile); $RB: remove decoding entry point
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
