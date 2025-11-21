#include "SMPTE_ST2094_50.hpp" 

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

// Constructors
SMPTE_ST2094_50::SMPTE_ST2094_50(){
    keyValue = "B500900001:SMPTE-ST2094-50";
}

// Getters
std::vector<uint8_t>    SMPTE_ST2094_50::getPayloadData(){return payloadBinaryData;}
uint32_t                SMPTE_ST2094_50::getTimeIntervalStart(){return timeI.timeIntervalStart;}
uint32_t                SMPTE_ST2094_50::getTimeintervalDuration(){return timeI.timeintervalDuration;}

// Methods 

/* *********************************** ENCODING SECTION ********************************************************************************************/
// Read from json file the metadata items
bool SMPTE_ST2094_50::decodeJsonToMetadataItems(nlohmann::json j, const std::string& path) {
bool error_raised = false;
  std::cout << "++++++++++++++++++++++ DECODE JSON TO METADATA ITEMS **++++++++++++++++++++++++++++++*********************************************]\n";
  // MetadataItems
 applicationIdentifier = 5;
 applicationVersion    = 255;

  // TimeInterval
  if (j.contains("frame_start") && j.contains("frame_duration")) {
   timeI.timeIntervalStart     = j["frame_start"].get<uint32_t>();
   timeI.timeintervalDuration  =  j["frame_duration"].get<uint32_t>();
  } else{
    // If not present attach it to only the first frame
    std::cerr << "JSON Decode: missing optional keys: frame_start | frame_duration\n";
   timeI.timeIntervalStart     = 0;
   timeI.timeintervalDuration  = 1;
  }

  // ProcessingWindow - fixed
 pWin.upperLeftCorner  = 0;
 pWin.lowerRightCorner = 0;
 pWin.windowNumber     = 1;

  // Checking mandatory metadata
  if (!j.contains("hdrReferenceWhite") ) {
    std::cerr <<  "missing mandatory keys: hdrReferenceWhite\n";
     return error_raised;
  }

  // ================================================= Decode Metadata Items ===================================
  // get mandatory metadata
 cvt.hdrReferenceWhite       = j["hdrReferenceWhite"].get<float>();
  // Initialize mode detection -> not metadata, just implementation convenience to set presence of various metadata
 isHeadroomAdaptiveToneMap   = false;
 isReferenceWhiteToneMapping = false;

  if (j.contains("baselineHdrHeadroom") )
  {
   isHeadroomAdaptiveToneMap    = true; // if there is a baselineHdrHeadroom then it is a headroom adaptive tone mapping
   cvt.hatm.baselineHdrHeadroom = j["baselineHdrHeadroom"].get<float>();

    if (!j.contains("numAlternateImages") || !j.contains("gainApplicationSpaceChromaticities") ){ // Derived Headroom Adaptive Tone Mapping parameters based on baselineHdrHeadroom
     isReferenceWhiteToneMapping  = true;
    }
    else // Custom Headroom Adaptive Tone Mapping mode
    {
     isReferenceWhiteToneMapping                 = false;
     cvt.hatm.numAlternateImages                 = j["numAlternateImages"].get<uint32_t>();

      std::vector<float> gainApplicationSpaceChromaticities = j["gainApplicationSpaceChromaticities"].get<std::vector<float>>();
      if (gainApplicationSpaceChromaticities.size() != MAX_NB_CHROMATICITIES){ error_raised = true; std::cerr <<  "JSON Decode: size of gainApplicationSpaceChromaticities != 8\n"; }
      for (int iCh = 0; iCh < MAX_NB_CHROMATICITIES; iCh++) {
       cvt.hatm.gainApplicationSpaceChromaticities[iCh] =  gainApplicationSpaceChromaticities[iCh];
      }
      
      if (cvt.hatm.numAlternateImages > 0) { // if no alternat then there is no more netadata
        if (!j.contains("alternateHdrHeadroom"))     { error_raised = true; std::cerr << "JSON Decode: alternateHdrHeadroom metadata item missing\n"     ; return error_raised;}

        if (!j.contains("componentMixRed"))          { error_raised = true; std::cerr << "JSON Decode: componentMixRed metadata item missing\n"          ; return error_raised;}
        if (!j.contains("componentMixGreen"))        { error_raised = true; std::cerr << "JSON Decode: componentMixGreen metadata item missing\n"        ; return error_raised;}
        if (!j.contains("componentMixBlue"))         { error_raised = true; std::cerr << "JSON Decode: componentMixBlue metadata item missing\n"         ; return error_raised;}
        if (!j.contains("componentMixMax"))          { error_raised = true; std::cerr << "JSON Decode: componentMixMax metadata item missing\n"          ; return error_raised;}
        if (!j.contains("componentMixMin"))          { error_raised = true; std::cerr << "JSON Decode: componentMixMin metadata item missing\n"          ; return error_raised;}
        if (!j.contains("componentMixComponent"))    { error_raised = true; std::cerr << "JSON Decode: componentMixComponent metadata item missing\n"    ; return error_raised;}

        if (!j.contains("gainCurveNumControlPoints")){ error_raised = true; std::cerr << "JSON Decode: gainCurveNumControlPoints metadata item missing\n"; return error_raised;}
        if (!j.contains("gainCurveControlPointX"))   { error_raised = true; std::cerr << "JSON Decode: gainCurveControlPointX metadata item missing\n"   ; return error_raised;}
        if (!j.contains("gainCurveControlPointY"))   { error_raised = true; std::cerr << "JSON Decode: gainCurveControlPointY metadata item missing\n"   ; return error_raised;}
        
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
        if (alternateHdrHeadroom.size()      <cvt.hatm.numAlternateImages){ error_raised = true; std::cerr << "JSON Decode: size of alternateHdrHeadroom < numAlternateImages\n";  return error_raised;}
        if (componentMixRed.size()           <cvt.hatm.numAlternateImages){ error_raised = true; std::cerr << "JSON Decode: size of componentMixRed < numAlternateImages\n"; return error_raised;}
        if (componentMixGreen.size()         <cvt.hatm.numAlternateImages){ error_raised = true; std::cerr << "JSON Decode: size of componentMixGreen < numAlternateImages\n"; return error_raised;}
        if (componentMixBlue.size()          <cvt.hatm.numAlternateImages){ error_raised = true; std::cerr << "JSON Decode: size of componentMixBlue < numAlternateImages\n"; return error_raised;}
        if (componentMixMax.size()           <cvt.hatm.numAlternateImages){ error_raised = true; std::cerr << "JSON Decode: size of componentMixMax < numAlternateImages\n"; return error_raised;}
        if (componentMixMin.size()           <cvt.hatm.numAlternateImages){ error_raised = true; std::cerr << "JSON Decode: size of componentMixMin < numAlternateImages\n"; return error_raised;}
        if (componentMixComponent.size()     <cvt.hatm.numAlternateImages){ error_raised = true; std::cerr << "JSON Decode: size of componentMixComponent < numAlternateImages\n"; return error_raised;}
        if (gainCurveNumControlPoints.size() <cvt.hatm.numAlternateImages){ error_raised = true; std::cerr << "JSON Decode: size of gainCurveNumControlPoints < numAlternateImages\n"; return error_raised;}
        if (gainCurveControlPointX.size()    <cvt.hatm.numAlternateImages){ error_raised = true; std::cerr << "JSON Decode: size of gainCurveControlPointX < numAlternateImages\n"; return error_raised;}
        if (gainCurveControlPointY.size()    <cvt.hatm.numAlternateImages){ error_raised = true; std::cerr << "JSON Decode: size of gainCurveControlPointY < numAlternateImages\n"; return error_raised;}

        std::vector<std::vector<float>> gainCurveControlPointTheta;
        if (j.contains("gainCurveControlPointTheta")) {
          for (uint16_t iAlt = 0; iAlt <cvt.hatm.numAlternateImages; iAlt++) {hasSlopeParameter[iAlt] = true; } // [Todo: within the json there could be only some alternates that have the slope. How to manage this case. For now assume they all have it or not]
          gainCurveControlPointTheta = j["gainCurveControlPointTheta"].get<std::vector<std::vector<float>>>();
          if (gainCurveControlPointTheta.size()    <cvt.hatm.numAlternateImages){ error_raised = true; std::cerr << "JSON Decode: size of gainCurveControlPointTheta < numAlternateImages\n"; return error_raised;}
        } else {
          for (uint16_t iAlt = 0; iAlt <cvt.hatm.numAlternateImages; iAlt++) {hasSlopeParameter[iAlt] = false;}
        }

        // Color Gain Function 
        for (uint16_t iAlt = 0; iAlt <cvt.hatm.numAlternateImages; iAlt++) {
         isLinearInterpolation[iAlt]  = false; // [Todo: not supported currently]
         cvt.hatm.alternateHdrHeadroom.push_back(alternateHdrHeadroom[iAlt]);

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
          if (gainCurveControlPointX[iAlt].size() < gainCurveNumControlPoints[iAlt]){ error_raised = true; std::cerr << "JSON Decode: size of gainCurveControlPointX[a] < gainCurveNumControlPoints\n"; return error_raised;}
          if (gainCurveControlPointY[iAlt].size() < gainCurveNumControlPoints[iAlt]){ error_raised = true; std::cerr << "JSON Decode: size of gainCurveControlPointX[a] < gainCurveNumControlPoints\n"; return error_raised;}
          for (uint16_t iCp = 0; iCp < gainCurveNumControlPoints[iAlt]; iCp++) {
            cgf.gc.gainCurveControlPointX.push_back(gainCurveControlPointX[iAlt][iCp]);
            cgf.gc.gainCurveControlPointY.push_back(gainCurveControlPointY[iAlt][iCp]);
          }
          if (hasSlopeParameter[iAlt]) {
            if (gainCurveControlPointTheta[iAlt].size() < gainCurveNumControlPoints[iAlt]){ error_raised = true; std::cerr << "JSON Decode: size of gainCurveControlPointTheta[a] < gainCurveNumControlPoints\n"; return error_raised;}
            for (uint16_t iCp = 0; iCp < gainCurveNumControlPoints[iAlt]; iCp++) {
              cgf.gc.gainCurveControlPointTheta.push_back(gainCurveControlPointTheta[iAlt][iCp]);
            }
          }
         cvt.hatm.cgf.push_back(cgf);
        }
      }
    }
  }
return error_raised;
}

// Convert metadata items to syntax elements 
void SMPTE_ST2094_50::convertMetadataItemsToSyntaxElements(){

  // Init the application version:
  elm.application_version = 15;


  if (abs(cvt.hdrReferenceWhite - 203.0) > Q_HDR_REFERENCE_WHITE / 2) {
    elm.has_custom_hdr_reference_white_flag = true;
    elm.hdr_reference_white = uint16_t(cvt.hdrReferenceWhite * Q_HDR_REFERENCE_WHITE);
  }
  elm.has_adaptive_tone_map_flag = isHeadroomAdaptiveToneMap;
  if (elm.has_adaptive_tone_map_flag){
    elm.baseline_hdr_headroom = uint16_t(cvt.hatm.baselineHdrHeadroom * Q_HDR_HEADROOM);
  }
  elm.use_reference_white_tone_mapping_flag = isReferenceWhiteToneMapping;
  if (!elm.use_reference_white_tone_mapping_flag){
    elm.num_alternate_images = uint16_t(cvt.hatm.numAlternateImages - 1);
    // Check if the primary combination is known
    if (
      (cvt.hatm.gainApplicationSpaceChromaticities[0] - 0.64  ) <  P_GAIN_APPLICATION_SPACE_CHROMATICITY &&
      (cvt.hatm.gainApplicationSpaceChromaticities[1] - 0.33  ) <  P_GAIN_APPLICATION_SPACE_CHROMATICITY &&
      (cvt.hatm.gainApplicationSpaceChromaticities[2] - 0.30  ) <  P_GAIN_APPLICATION_SPACE_CHROMATICITY &&
      (cvt.hatm.gainApplicationSpaceChromaticities[3] - 0.60  ) <  P_GAIN_APPLICATION_SPACE_CHROMATICITY &&
      (cvt.hatm.gainApplicationSpaceChromaticities[4] - 0.15  ) <  P_GAIN_APPLICATION_SPACE_CHROMATICITY &&
      (cvt.hatm.gainApplicationSpaceChromaticities[5] - 0.06  ) <  P_GAIN_APPLICATION_SPACE_CHROMATICITY &&
      (cvt.hatm.gainApplicationSpaceChromaticities[6] - 0.3127) <  P_GAIN_APPLICATION_SPACE_CHROMATICITY &&
      (cvt.hatm.gainApplicationSpaceChromaticities[7] - 0.3290) < P_GAIN_APPLICATION_SPACE_CHROMATICITY){ 
        elm.gain_application_space_chromaticities_mode = 0;
      } 
      else if (
        (cvt.hatm.gainApplicationSpaceChromaticities[0] - 0.68  ) <  P_GAIN_APPLICATION_SPACE_CHROMATICITY &&
        (cvt.hatm.gainApplicationSpaceChromaticities[1] - 0.32  ) <  P_GAIN_APPLICATION_SPACE_CHROMATICITY &&
        (cvt.hatm.gainApplicationSpaceChromaticities[2] - 0.265 ) <  P_GAIN_APPLICATION_SPACE_CHROMATICITY &&
        (cvt.hatm.gainApplicationSpaceChromaticities[3] - 0.69  ) <  P_GAIN_APPLICATION_SPACE_CHROMATICITY &&
        (cvt.hatm.gainApplicationSpaceChromaticities[4] - 0.15  ) <  P_GAIN_APPLICATION_SPACE_CHROMATICITY &&
        (cvt.hatm.gainApplicationSpaceChromaticities[5] - 0.06  ) <  P_GAIN_APPLICATION_SPACE_CHROMATICITY &&
        (cvt.hatm.gainApplicationSpaceChromaticities[6] - 0.3127) <  P_GAIN_APPLICATION_SPACE_CHROMATICITY &&
        (cvt.hatm.gainApplicationSpaceChromaticities[7] - 0.329 ) < Q_GAIN_APPLICATION_SPACE_CHROMATICITY ) { 
        elm.gain_application_space_chromaticities_mode = 1;
      } else if (
        (cvt.hatm.gainApplicationSpaceChromaticities[0] - 0.708 ) <  P_GAIN_APPLICATION_SPACE_CHROMATICITY &&
        (cvt.hatm.gainApplicationSpaceChromaticities[1] - 0.292 ) <  P_GAIN_APPLICATION_SPACE_CHROMATICITY &&
        (cvt.hatm.gainApplicationSpaceChromaticities[2] - 0.17  ) <  P_GAIN_APPLICATION_SPACE_CHROMATICITY &&
        (cvt.hatm.gainApplicationSpaceChromaticities[3] - 0.797 ) <  P_GAIN_APPLICATION_SPACE_CHROMATICITY &&
        (cvt.hatm.gainApplicationSpaceChromaticities[4] - 0.131 ) <  P_GAIN_APPLICATION_SPACE_CHROMATICITY &&
        (cvt.hatm.gainApplicationSpaceChromaticities[5] - 0.046 ) <  P_GAIN_APPLICATION_SPACE_CHROMATICITY &&
        (cvt.hatm.gainApplicationSpaceChromaticities[6] - 0.3127) <  P_GAIN_APPLICATION_SPACE_CHROMATICITY &&
        (cvt.hatm.gainApplicationSpaceChromaticities[7] - 0.329 ) < P_GAIN_APPLICATION_SPACE_CHROMATICITY){ 
          elm.gain_application_space_chromaticities_mode = 2;
      } else {
        elm.gain_application_space_chromaticities_mode = 3;
        for (uint16_t iCh = 0; iCh < 8; iCh++) {
          elm.gain_application_space_chromaticities[iCh] = uint16_t(cvt.hatm.gainApplicationSpaceChromaticities[iCh]* Q_GAIN_APPLICATION_SPACE_CHROMATICITY);
        }
      }

      // Check if all component mixing uses the same parameters 
      elm.has_common_component_component_mixing_coefficient_flag = true; // Init at true, any mismatch makes it false
      for (uint16_t iAlt = 1; iAlt < cvt.hatm.numAlternateImages; iAlt++) {
        // Check if any coefficient is different across alterante images
        if (abs(cvt.hatm.cgf[0].cm.componentMixRed       - cvt.hatm.cgf[iAlt].cm.componentMixRed)       > Q_COMPONENT_MIXING_COEFFICIENT || 
            abs(cvt.hatm.cgf[0].cm.componentMixGreen     - cvt.hatm.cgf[iAlt].cm.componentMixGreen)     > Q_COMPONENT_MIXING_COEFFICIENT || 
            abs(cvt.hatm.cgf[0].cm.componentMixBlue      - cvt.hatm.cgf[iAlt].cm.componentMixBlue)      > Q_COMPONENT_MIXING_COEFFICIENT || 
            abs(cvt.hatm.cgf[0].cm.componentMixMax       - cvt.hatm.cgf[iAlt].cm.componentMixMax)       > Q_COMPONENT_MIXING_COEFFICIENT || 
            abs(cvt.hatm.cgf[0].cm.componentMixMin       - cvt.hatm.cgf[iAlt].cm.componentMixMin)       > Q_COMPONENT_MIXING_COEFFICIENT || 
            abs(cvt.hatm.cgf[0].cm.componentMixComponent - cvt.hatm.cgf[iAlt].cm.componentMixComponent) > Q_COMPONENT_MIXING_COEFFICIENT) {
          elm.has_common_component_component_mixing_coefficient_flag = false;
          break;
        }
      } 
      // Check if all alternate have the same number of control points and interpolation
      elm.has_common_curve_params_flag = true;  // Init at true, any mismatch makes it false
      for (uint16_t iAlt = 1; iAlt < cvt.hatm.numAlternateImages; iAlt++) {
        if (cvt.hatm.cgf[0].gc.gainCurveNumControlPoints != cvt.hatm.cgf[iAlt].gc.gainCurveNumControlPoints){
          elm.has_common_component_component_mixing_coefficient_flag = false;
          break;
        }
      }

      // If they do, then check if all alternate have the same x position
      for (uint16_t pIdx = 0; pIdx < cvt.hatm.cgf[0].gc.gainCurveNumControlPoints; pIdx++){
        for (uint16_t iAlt = 1; iAlt < cvt.hatm.numAlternateImages; iAlt++) {
          if (cvt.hatm.cgf[0].gc.gainCurveControlPointX[pIdx] - cvt.hatm.cgf[iAlt].gc.gainCurveControlPointX[pIdx] > Q_GAIN_CURVE_CONTROL_POINT_X){
            elm.has_common_component_component_mixing_coefficient_flag = false;
            break;
          }
        }
      } 
      
      // Loop over alternate images
      for (uint16_t iAlt = 0; iAlt < cvt.hatm.numAlternateImages; iAlt++) {
        elm.alternate_hdr_headrooms[iAlt] = uint16_t(cvt.hatm.alternateHdrHeadroom[iAlt] * Q_HDR_HEADROOM);

        // init coefficient to 0
        for (uint16_t iCmf = 0; iCmf < MAX_NB_component_mixing_coefficient; iCmf++){
          elm.component_mixing_coefficient[iAlt][iCmf] = uint16_t(0);
          elm.has_component_mixing_coefficient_flag[iAlt][iCmf] = false;
        }
        // Component mixing
        if (
          abs(cvt.hatm.cgf[iAlt].cm.componentMixRed      ) < P_COMPONENT_MIXING_COEFFICIENT &&
          abs(cvt.hatm.cgf[iAlt].cm.componentMixGreen    ) < P_COMPONENT_MIXING_COEFFICIENT &&
          abs(cvt.hatm.cgf[iAlt].cm.componentMixBlue     ) < P_COMPONENT_MIXING_COEFFICIENT &&
          abs(cvt.hatm.cgf[iAlt].cm.componentMixMax- 1.0 ) < P_COMPONENT_MIXING_COEFFICIENT &&
          abs(cvt.hatm.cgf[iAlt].cm.componentMixMin      ) < P_COMPONENT_MIXING_COEFFICIENT &&
          abs(cvt.hatm.cgf[iAlt].cm.componentMixComponent) < P_COMPONENT_MIXING_COEFFICIENT){ 
            elm.component_mixing_type[iAlt] = 0;
        } else if (
          abs(cvt.hatm.cgf[iAlt].cm.componentMixRed      ) < P_COMPONENT_MIXING_COEFFICIENT &&
          abs(cvt.hatm.cgf[iAlt].cm.componentMixGreen    ) < P_COMPONENT_MIXING_COEFFICIENT &&
          abs(cvt.hatm.cgf[iAlt].cm.componentMixBlue     ) < P_COMPONENT_MIXING_COEFFICIENT &&
          abs(cvt.hatm.cgf[iAlt].cm.componentMixMax      ) < P_COMPONENT_MIXING_COEFFICIENT &&
          abs(cvt.hatm.cgf[iAlt].cm.componentMixMin      ) < P_COMPONENT_MIXING_COEFFICIENT &&
          abs(cvt.hatm.cgf[iAlt].cm.componentMixComponent - 1.0 ) < P_COMPONENT_MIXING_COEFFICIENT){ 
            elm.component_mixing_type[iAlt] = 1;
        }  else if (
          abs(cvt.hatm.cgf[iAlt].cm.componentMixRed   - (1.0 / 6.0)) < P_COMPONENT_MIXING_COEFFICIENT &&
          abs(cvt.hatm.cgf[iAlt].cm.componentMixGreen - (1.0 / 6.0)) < P_COMPONENT_MIXING_COEFFICIENT &&
          abs(cvt.hatm.cgf[iAlt].cm.componentMixBlue  - (1.0 / 6.0)) < P_COMPONENT_MIXING_COEFFICIENT &&
          abs(cvt.hatm.cgf[iAlt].cm.componentMixMax   - (1.0 / 2.0)) < P_COMPONENT_MIXING_COEFFICIENT &&
          abs(cvt.hatm.cgf[iAlt].cm.componentMixMin      ) < P_COMPONENT_MIXING_COEFFICIENT &&
          abs(cvt.hatm.cgf[iAlt].cm.componentMixComponent) < P_COMPONENT_MIXING_COEFFICIENT){ 
            elm.component_mixing_type[iAlt] = 2;
        } else { // Send flag to true for each non-zero coefficient
          uint16_t iCmf = 0;
          // 
          elm.component_mixing_coefficient[iAlt][iCmf] = uint16_t(cvt.hatm.cgf[iAlt].cm.componentMixRed       * Q_COMPONENT_MIXING_COEFFICIENT); iCmf++;
          elm.component_mixing_coefficient[iAlt][iCmf] = uint16_t(cvt.hatm.cgf[iAlt].cm.componentMixGreen     * Q_COMPONENT_MIXING_COEFFICIENT); iCmf++;
          elm.component_mixing_coefficient[iAlt][iCmf] = uint16_t(cvt.hatm.cgf[iAlt].cm.componentMixBlue      * Q_COMPONENT_MIXING_COEFFICIENT); iCmf++;
          elm.component_mixing_coefficient[iAlt][iCmf] = uint16_t(cvt.hatm.cgf[iAlt].cm.componentMixMax       * Q_COMPONENT_MIXING_COEFFICIENT); iCmf++;
          elm.component_mixing_coefficient[iAlt][iCmf] = uint16_t(cvt.hatm.cgf[iAlt].cm.componentMixMin       * Q_COMPONENT_MIXING_COEFFICIENT); iCmf++;
          elm.component_mixing_coefficient[iAlt][iCmf] = uint16_t(cvt.hatm.cgf[iAlt].cm.componentMixComponent * Q_COMPONENT_MIXING_COEFFICIENT); iCmf++;
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
        // elm.gain_curve_interpolation[iAlt] = cvt.hatm.cgf[iAlt].gc.gainCurveInterpolation;
        elm.gain_curve_num_control_points_minus_1[iAlt] = cvt.hatm.cgf[iAlt].gc.gainCurveNumControlPoints - 1;
        for (uint16_t iCps = 0; iCps < cvt.hatm.cgf[iAlt].gc.gainCurveNumControlPoints; iCps++){
          elm.gain_curve_control_points_x[iAlt][iCps] = uint16_t(cvt.hatm.cgf[iAlt].gc.gainCurveControlPointX[iCps] * Q_GAIN_CURVE_CONTROL_POINT_X );
          elm.gain_curve_control_points_y[iAlt][iCps] = uint16_t((cvt.hatm.cgf[iAlt].gc.gainCurveControlPointY[iCps]  + O_GAIN_CURVE_CONTROL_POINT_Y ) * Q_GAIN_CURVE_CONTROL_POINT_Y );
        }
        elm.gain_curve_use_pchip_slope_flag[iAlt] = !hasSlopeParameter[iAlt];
        if (!elm.gain_curve_use_pchip_slope_flag[iAlt]) {
          for (uint16_t iCps = 0; iCps < cvt.hatm.cgf[iAlt].gc.gainCurveNumControlPoints; iCps++) {
            elm.gain_curve_control_points_theta[iAlt][iCps] = uint16_t((cvt.hatm.cgf[iAlt].gc.gainCurveControlPointTheta[iCps] + O_GAIN_CURVE_CONTROL_POINT_THETA) * Q_GAIN_CURVE_CONTROL_POINT_THETA );
          }
        }
      }
    }
  }

// Convert syntax element to finary data and write to file
void SMPTE_ST2094_50::writeSyntaxElementsToBinaryData(){
// ================================================= Convert binary data from Syntax Elements ===================================
uint8_t value_8; 
std::cout << "++++++++++++++++++++++ start SMPTE_ST2094_50:: writeSyntaxElementsToBinaryData ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n";  
printDebug("application_version", elm.application_version, 8);
value_8 = elm.application_version; 
payloadBinaryData.push_back(value_8);

printDebug("has_custom_hdr_reference_white_flag",elm.has_custom_hdr_reference_white_flag, 1);
printDebug("has_adaptive_tone_map_flag",elm.has_adaptive_tone_map_flag, 1);
value_8 = (elm.has_custom_hdr_reference_white_flag << 7) + (elm.has_adaptive_tone_map_flag << 6);
payloadBinaryData.push_back(value_8);

if (elm.has_custom_hdr_reference_white_flag){
printDebug("hdr_reference_white", elm.hdr_reference_white, 16);
value_8 = uint8_t((elm.hdr_reference_white >> 8) & 0x00FF); payloadBinaryData.push_back(value_8);
value_8 = uint8_t((elm.hdr_reference_white     ) & 0x00FF); payloadBinaryData.push_back(value_8);
}

if (elm.has_adaptive_tone_map_flag) {
printDebug("baseline_hdr_headroom", elm.baseline_hdr_headroom, 16);
value_8 = uint8_t((elm.baseline_hdr_headroom >> 8) & 0x00FF); payloadBinaryData.push_back(value_8);
value_8 = uint8_t((elm.baseline_hdr_headroom     ) & 0x00FF); payloadBinaryData.push_back(value_8);

printDebug("use_reference_white_tone_mapping_flag",elm.use_reference_white_tone_mapping_flag, 1);
if (!elm.use_reference_white_tone_mapping_flag) {
    printDebug("num_alternate_images",elm.num_alternate_images, 3);
    printDebug("gain_application_space_chromaticities_mode",elm.gain_application_space_chromaticities_mode, 2);
    printDebug("has_common_component_component_mixing_coefficient_flag",elm.has_common_component_component_mixing_coefficient_flag, 1);
    printDebug("has_common_curve_params_flag",elm.has_common_curve_params_flag, 1);
    value_8 = (elm.use_reference_white_tone_mapping_flag << 7) + (elm.num_alternate_images << 4) + (elm.gain_application_space_chromaticities_mode << 2) + 
    (elm.has_common_component_component_mixing_coefficient_flag << 1) + (elm.has_common_curve_params_flag );
    payloadBinaryData.push_back(value_8);

    if (elm.gain_application_space_chromaticities_mode == 3) {
    for (uint16_t iCh = 0; iCh < 8; iCh++) {
        printDebug("gain_application_space_chromaticities", elm.gain_application_space_chromaticities[iCh], 16);
        value_8 = uint8_t((elm.gain_application_space_chromaticities[iCh] >> 8) & 0x00FF); payloadBinaryData.push_back(value_8);
        value_8 = uint8_t((elm.gain_application_space_chromaticities[iCh]     ) & 0x00FF); payloadBinaryData.push_back(value_8);
    }
    }

    for (uint16_t iAlt = 0; iAlt < elm.num_alternate_images + 1; iAlt++) {
    printDebug("alternate_hdr_headrooms[iAlt]", elm.alternate_hdr_headrooms[iAlt], 16);
    value_8 = uint8_t((elm.alternate_hdr_headrooms[iAlt] >> 8) & 0x00FF); payloadBinaryData.push_back(value_8);
    value_8 = uint8_t((elm.alternate_hdr_headrooms[iAlt]     ) & 0x00FF); payloadBinaryData.push_back(value_8);

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
        payloadBinaryData.push_back(value_8); 
        // Write the coefficients 
        for (uint8_t iCm = 0; iCm < MAX_NB_component_mixing_coefficient; iCm++) {
            if (elm.has_component_mixing_coefficient_flag[iAlt][iCm]) {
            printDebug("component_mixing_coefficient", elm.component_mixing_coefficient[iAlt][iCm], 16);
            value_8 = uint8_t((elm.component_mixing_coefficient[iAlt][iCm] >> 8) & 0x00FF); payloadBinaryData.push_back(value_8);
            value_8 = uint8_t((elm.component_mixing_coefficient[iAlt][iCm]     ) & 0x00FF); payloadBinaryData.push_back(value_8);
            }
        }                
        } else {
        payloadBinaryData.push_back(value_8); 
        }
    }

    /// Write gain curve function parameters
    if ( iAlt == 0 || elm.has_common_curve_params_flag){
        //printDebug("gain_curve_interpolation[iAlt]",elm.gain_curve_interpolation[iAlt], 2);
        printDebug("gain_curve_num_control_points_minus_1[iAlt]",elm.gain_curve_num_control_points_minus_1[iAlt], 5);
        printDebug("gain_curve_use_pchip_slope_flag",elm.gain_curve_use_pchip_slope_flag[iAlt], 1);
        value_8 = (elm.gain_curve_num_control_points_minus_1[iAlt] << 3) + (elm.gain_curve_use_pchip_slope_flag[iAlt] << 2) ;
        payloadBinaryData.push_back(value_8);
        
        for (uint16_t iCps = 0; iCps < elm.gain_curve_num_control_points_minus_1[iAlt] + 1; iCps++){
        printDebug("gain_curve_control_points_x[iAlt][iCps]",elm.gain_curve_control_points_x[iAlt][iCps], 16);
        value_8 = uint8_t((elm.gain_curve_control_points_x[iAlt][iCps] >> 8) & 0x00FF); payloadBinaryData.push_back(value_8);
        value_8 = uint8_t((elm.gain_curve_control_points_x[iAlt][iCps]     ) & 0x00FF); payloadBinaryData.push_back(value_8);
        }
    }
    for (uint16_t iCps = 0; iCps < elm.gain_curve_num_control_points_minus_1[iAlt] + 1; iCps++) {
        printDebug("gain_curve_control_points_y[iAlt][iCps]",elm.gain_curve_control_points_y[iAlt][iCps], 16);
        value_8 = uint8_t((elm.gain_curve_control_points_y[iAlt][iCps] >> 8) & 0x00FF); payloadBinaryData.push_back(value_8);
        value_8 = uint8_t((elm.gain_curve_control_points_y[iAlt][iCps]     ) & 0x00FF); payloadBinaryData.push_back(value_8);
    }
    if (!elm.gain_curve_use_pchip_slope_flag[iAlt]) {
        for (uint16_t iCps = 0; iCps < elm.gain_curve_num_control_points_minus_1[iAlt] + 1; iCps++) {
        printDebug("gain_curve_control_points_theta[iAlt][iCps]",elm.gain_curve_control_points_theta[iAlt][iCps], 16);
        value_8 = uint8_t((elm.gain_curve_control_points_theta[iAlt][iCps] >> 8) & 0x00FF); payloadBinaryData.push_back(value_8);
        value_8 = uint8_t((elm.gain_curve_control_points_theta[iAlt][iCps]     ) & 0x00FF); payloadBinaryData.push_back(value_8);
        }
    }
    }
}
}
else{ // No more information need to be signaled when using Reference White Tone Mapping Operator
value_8 = (elm.use_reference_white_tone_mapping_flag << 7);
payloadBinaryData.push_back(value_8);
}

// outFile.close(); // Close the file
std::cout << "+++++++++++++++++++++++++++++++ End SMPTE_ST2094_50:: writeSyntaxElementsToBinaryData +++++++++++++++++++++++++++++++++]\n";
}

/* *********************************** DECODING SECTION ********************************************************************************************/

/*
// Decode binary data into syntax elements
void SMPTE_ST2094_50::decodeBinaryToSyntaxElements(std::vector<uint8_t> binary_data) {

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
void SMPTE_ST2094_50::convertSyntaxElementsToMetadataItems(){
  bool error_raised = false;
  // get mandatory metadata
  windowNumber     = 1;
  if (elm.has_custom_hdr_reference_white_flag) {
    cvt.hdrReferenceWhite = float(elm.hdr_reference_white) / Q_HDR_REFERENCE_WHITE;
  } else {
    cvt.hdrReferenceWhite = 203.0;
  }
  toneMapMode = elm.tone_map_mode;

  // Get Optional metadata items
  if (toneMapMode != 0) {
    cvt.hatm.baselineHdrHeadroom = float(elm.baseline_hdr_headroom) / Q_HDR_HEADROOM;;
  }
  if (toneMapMode == 3) {
    cvt.hatm.numAlternateImages = 2;
    // BT.2020 primaries
    cvt.hatm.gainApplicationSpaceChromaticities.push_back(0.708);
    cvt.hatm.gainApplicationSpaceChromaticities.push_back(0.292);
    cvt.hatm.gainApplicationSpaceChromaticities.push_back(0.17);
    cvt.hatm.gainApplicationSpaceChromaticities.push_back(0.797);
    cvt.hatm.gainApplicationSpaceChromaticities.push_back(0.131);
    cvt.hatm.gainApplicationSpaceChromaticities.push_back(0.046);
    cvt.hatm.gainApplicationSpaceChromaticities.push_back(0.3127);
    cvt.hatm.gainApplicationSpaceChromaticities.push_back(0.329);
    // Compute alternate headroom
    cvt.hatm.alternateHdrHeadroom.push_back(0.0);
    float headroom_to_anchor_ratio = std::min(cvt.hatm.baselineHdrHeadroom / log2(1000.0/203.0), 1.0);
    float h_alt_1 = log2(8.0/3.0) * headroom_to_anchor_ratio;
    cvt.hatm.alternateHdrHeadroom.push_back(h_alt_1);

    // Constant parameter across alternate images
    float kappa  = 0.65;
    float x_knee = 1;
    float x_max  = pow(2.0, cvt.hatm.baselineHdrHeadroom);
    for (uint16_t iAlt = 0;  iAlt  < cvt.hatm.numAlternateImages; iAlt++){
      // Component mixing is maxRGB
      cvt.hatm.cgf[iAlt].cm.componentMixRed.push_back(0.0);
      cvt.hatm.cgf[iAlt].cm.componentMixGreen.push_back(0.0);
      cvt.hatm.cgf[iAlt].cm.componentMixBlue.push_back(0.0);
      cvt.hatm.cgf[iAlt].cm.componentMixMax.push_back(1.0);
      cvt.hatm.cgf[iAlt].cm.componentMixMin.push_back(0.0);
      cvt.hatm.cgf[iAlt].cm.componentMixComponent.push_back(0.0); 
      cvt.hatm.cgf[iAlt].gc.gainCurveInterpolation.push_back(2);
      cvt.hatm.cgf[iAlt].gc.gainCurveNumControlPoints.push_back(8);

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
      float y_max  = pow(2.0, cvt.hatm.alternateHdrHeadroom[iAlt]);
      float x_mid = (1.0 - kappa) * x_knee + kappa * (x_knee * y_max / y_knee);
      float y_mid = (1.0 - kappa) * y_knee + kappa * y_max;
      // Compute Quadratic Beziers coefficients
      float a_x = x_knee - 2 * x_mid + x_max;
      float a_y = y_knee - 2 * y_mid + y_max;
      float b_x = 2 * x_mid - 2 * x_knee;
      float b_y = 2 * y_mid - 2 * y_knee;
      float c_x = x_knee;
      float c_y = y_knee;

      for (uint16_t iCps = 0; iCps < cvt.hatm.cgf[iAlt].gc.gainCurveNumControlPoints; iCps++) {
        // Compute the control points
        float t = float(iCps) / (float(gainCurveNumControlPoints[iAlt]) - 1.0);
        float t_square = t * t;
        float x = a_x * t_square + b_x * t + c_x;
        float y = a_y * t_square + b_y * t + c_y;
        float m = (2.0 * a_y * t + b_y) / (2 * a_x * t + b_x);
        float slope = atan( (x * m - y) / (log(2) * x * y) );
        inner_gainCurveControlPointX.push_back(x);
        inner_gainCurveControlPointY.push_back(log2(y / x));
        inner_gainCurveControlPointTheta.push_back(slope / PI_CUSTOM * 180.0);
      }
      cvt.hatm.cgf[iAlt].gc.gainCurveControlPointX.push_back(inner_gainCurveControlPointX);
      cvt.hatm.cgf[iAlt].gc.gainCurveControlPointY.push_back(inner_gainCurveControlPointY);
      cvt.hatm.cgf[iAlt].gc.gainCurveControlPointTheta.push_back(inner_gainCurveControlPointTheta);
    }
  }
  if (toneMapMode == 4){
    cvt.hatm.numAlternateImages = elm.num_alternate_images + 1;
    if (elm.gain_application_space_chromaticities_mode == 0){
      cvt.hatm.gainApplicationSpaceChromaticities.push_back(0.64);
      cvt.hatm.gainApplicationSpaceChromaticities.push_back(0.33); 
      cvt.hatm.gainApplicationSpaceChromaticities.push_back(0.3); 
      cvt.hatm.gainApplicationSpaceChromaticities.push_back(0.6);
      cvt.hatm.gainApplicationSpaceChromaticities.push_back(0.15); 
      cvt.hatm.gainApplicationSpaceChromaticities.push_back(0.06);
      cvt.hatm.gainApplicationSpaceChromaticities.push_back(0.3127);
      cvt.hatm.gainApplicationSpaceChromaticities.push_back(0.329);
    } else if (elm.gain_application_space_chromaticities_mode == 1){
      cvt.hatm.gainApplicationSpaceChromaticities.push_back(0.68); 
      cvt.hatm.gainApplicationSpaceChromaticities.push_back(0.32); 
      cvt.hatm.gainApplicationSpaceChromaticities.push_back(0.265); 
      cvt.hatm.gainApplicationSpaceChromaticities.push_back(0.69); 
      cvt.hatm.gainApplicationSpaceChromaticities.push_back(0.15); 
      cvt.hatm.gainApplicationSpaceChromaticities.push_back(0.06); 
      cvt.hatm.gainApplicationSpaceChromaticities.push_back(0.3127); 
      cvt.hatm.gainApplicationSpaceChromaticities.push_back(0.329); 
    } else if (elm.gain_application_space_chromaticities_mode == 2){
      cvt.hatm.gainApplicationSpaceChromaticities.push_back(0.708);
      cvt.hatm.gainApplicationSpaceChromaticities.push_back(0.292);
      cvt.hatm.gainApplicationSpaceChromaticities.push_back(0.17);
      cvt.hatm.gainApplicationSpaceChromaticities.push_back(0.797);
      cvt.hatm.gainApplicationSpaceChromaticities.push_back(0.131);
      cvt.hatm.gainApplicationSpaceChromaticities.push_back(0.046);
      cvt.hatm.gainApplicationSpaceChromaticities.push_back(0.3127);
      cvt.hatm.gainApplicationSpaceChromaticities.push_back(0.329);
    } else if (elm.gain_application_space_chromaticities_mode == 3){
        for (uint16_t iCh = 0; iCh < 8; iCh++) {
          cvt.hatm.gainApplicationSpaceChromaticities.push_back(float(elm.gain_application_space_chromaticities[iCh]) / Q_GAIN_APPLICATION_SPACE_CHROMATICITY);
        }
    } else {
      std::cerr << "gain_application_space_primaries=" << elm.gain_application_space_chromaticities_mode << "  not defined.\n";
      error_raised = true;
    }
    for (uint16_t iAlt = 0;  iAlt  < cvt.hatm.numAlternateImages; iAlt++){

      cvt.hatm.alternateHdrHeadroom.push_back(float(elm.alternate_hdr_headrooms[iAlt]) / Q_HDR_HEADROOM);
      // init k_params to zero and replace the one that are not
      cvt.hatm.cgf[iAlt].cm.componentMixRed.push_back(0.0);
      cvt.hatm.cgf[iAlt].cm.componentMixGreen.push_back(0.0);
      cvt.hatm.cgf[iAlt].cm.componentMixBlue.push_back(0.0);
      cvt.hatm.cgf[iAlt].cm.componentMixMax.push_back(0.0);
      cvt.hatm.cgf[iAlt].cm.componentMixMin.push_back(0.0);
      cvt.hatm.cgf[iAlt].cm.componentMixComponent.push_back(0.0); 
      if (elm.component_mixing_type[iAlt] == 0){
        cvt.hatm.cgf[iAlt].cm.componentMixMax   = 1.0;
      } else if (elm.component_mixing_type[iAlt] == 1){
        cvt.hatm.cgf[iAlt].cm.componentMixComponent   = 1.0;
      } else if (elm.component_mixing_type[iAlt] == 2){
        cvt.hatm.cgf[iAlt].cm.componentMixRed   = 1.0 / 6.0;
        cvt.hatm.cgf[iAlt].cm.componentMixGreen = 1.0 / 6.0;
        cvt.hatm.cgf[iAlt].cm.componentMixBlue  = 1.0 / 6.0;
        cvt.hatm.cgf[iAlt].cm.componentMixMax   = 1.0 / 2.0;
      } else if (elm.component_mixing_type[iAlt] == 3){
        cvt.hatm.cgf[iAlt].cm.componentMixRed   = float(elm.component_mixing_type[iAlt][0]) / Q_COMPONENT_MIXING_COEFFICIENT / 3.0;
        cvt.hatm.cgf[iAlt].cm.componentMixGreen = float(elm.component_mixing_type[iAlt][0]) / Q_COMPONENT_MIXING_COEFFICIENT / 3.0;
        cvt.hatm.cgf[iAlt].cm.componentMixBlue  = float(elm.component_mixing_type[iAlt][0]) / Q_COMPONENT_MIXING_COEFFICIENT / 3.0;
        cvt.hatm.cgf[iAlt].cm.componentMixMax   = 1.0 - float(elm.component_mixing_type[iAlt][0]) / Q_COMPONENT_MIXING_COEFFICIENT;
      }  else if (elm.component_mixing_type[iAlt] == 4){
        cvt.hatm.cgf[iAlt].cm.componentMixMax   = float(elm.component_mixing_type[iAlt][0]) / Q_COMPONENT_MIXING_COEFFICIENT;
        cvt.hatm.cgf[iAlt].cm.componentMixComponent   = 1.0 - float(elm.component_mixing_type[iAlt][0]) / Q_COMPONENT_MIXING_COEFFICIENT;
      } else if (elm.component_mixing_type[iAlt] == 5){
        cvt.hatm.cgf[iAlt].cm.componentMixRed   = float(elm.component_mixing_type[iAlt][0]) / Q_COMPONENT_MIXING_COEFFICIENT;
        cvt.hatm.cgf[iAlt].cm.componentMixGreen = float(elm.component_mixing_type[iAlt][1]) / Q_COMPONENT_MIXING_COEFFICIENT;
        cvt.hatm.cgf[iAlt].cm.componentMixBlue  = 1.0 - (float(elm.component_mixing_type[iAlt][1]) + float((elm.component_mixing_type[iAlt][0]))) / Q_COMPONENT_MIXING_COEFFICIENT;
      } else if (elm.component_mixing_type[iAlt] == 6){
        cvt.hatm.cgf[iAlt].cm.componentMixRed   = float(elm.component_mixing_type[iAlt][0]) / Q_COMPONENT_MIXING_COEFFICIENT / 3.0;
        cvt.hatm.cgf[iAlt].cm.componentMixGreen = float(elm.component_mixing_type[iAlt][0]) / Q_COMPONENT_MIXING_COEFFICIENT / 3.0;
        cvt.hatm.cgf[iAlt].cm.componentMixBlue  = float(elm.component_mixing_type[iAlt][0]) / Q_COMPONENT_MIXING_COEFFICIENT / 3.0;
        cvt.hatm.cgf[iAlt].cm.componentMixMax   = float(elm.component_mixing_type[iAlt][1]) / Q_COMPONENT_MIXING_COEFFICIENT;
        cvt.hatm.cgf[iAlt].cm.componentMixMin   = float(elm.component_mixing_type[iAlt][2]) / Q_COMPONENT_MIXING_COEFFICIENT;
        cvt.hatm.cgf[iAlt].cm.componentMixComponent   = 1.0 - (float(elm.component_mixing_type[iAlt][0]) + float(elm.component_mixing_type[iAlt][1]) + float(elm.component_mixing_type[iAlt][2])) / Q_COMPONENT_MIXING_COEFFICIENT;
      } else if (elm.component_mixing_type[iAlt] == 7){
        cvt.hatm.cgf[iAlt].cm.componentMixRed   = float(elm.component_mixing_type[iAlt][0]) / Q_COMPONENT_MIXING_COEFFICIENT;
        cvt.hatm.cgf[iAlt].cm.componentMixGreen = float(elm.component_mixing_type[iAlt][1]) / Q_COMPONENT_MIXING_COEFFICIENT;
        cvt.hatm.cgf[iAlt].cm.componentMixBlue  = float(elm.component_mixing_type[iAlt][2]) / Q_COMPONENT_MIXING_COEFFICIENT;
        cvt.hatm.cgf[iAlt].cm.componentMixMax   = float(elm.component_mixing_type[iAlt][3]) / Q_COMPONENT_MIXING_COEFFICIENT;
        cvt.hatm.cgf[iAlt].cm.componentMixComponent   = 1.0 - (float(elm.component_mixing_type[iAlt][0]) + float(elm.component_mixing_type[iAlt][1]) + 
                                            float(elm.component_mixing_type[iAlt][2]) + float(elm.component_mixing_type[iAlt][3])) / Q_COMPONENT_MIXING_COEFFICIENT;
      } else {
        std::cerr << "mix_encoding[" << iAlt << "]=" << elm.component_mixing_type[iAlt] << "  not defined.\n";
        error_raised = true;
      }
      cvt.hatm.cgf[iAlt].gc.gainCurveInterpolation.push_back(elm.gain_curve_interpolation[iAlt]);
      cvt.hatm.cgf[iAlt].gc.gainCurveNumControlPoints.push_back(elm.gain_curve_num_control_points_minus_1[iAlt] + 1);

      // Inner vector for push_back
      std::vector<float> inner_gainCurveControlPointX;
      std::vector<float> inner_gainCurveControlPointY;
      std::vector<float> inner_gainCurveControlPointTheta;
      for (uint16_t iCps = 0; iCps < cvt.hatm.cgf[iAlt].gc.gainCurveNumControlPoints; iCps++) {
        inner_gainCurveControlPointX.push_back(float(elm.gain_curve_control_points_x[iAlt][iCps]) / Q_GAIN_CURVE_CONTROL_POINT_X);
        inner_gainCurveControlPointY.push_back(float(elm.gain_curve_control_points_y[iAlt][iCps]) / Q_GAIN_CURVE_CONTROL_POINT_Y - O_GAIN_CURVE_CONTROL_POINT_Y);
        if (gainCurveInterpolation == 2) {
          inner_gainCurveControlPointTheta.push_back(float(elm.gain_curve_control_points_theta[iAlt][iCps]) / Q_GAIN_CURVE_CONTROL_POINT_THETA - O_GAIN_CURVE_CONTROL_POINT_THETA);
        }
      }
      cvt.hatm.cgf[iAlt].gc.gainCurveControlPointX.push_back(inner_gainCurveControlPointX);
      cvt.hatm.cgf[iAlt].gc.gainCurveControlPointY.push_back(inner_gainCurveControlPointY);
      cvt.hatm.cgf[iAlt].gc.gainCurveControlPointTheta.push_back(inner_gainCurveControlPointTheta);
    }
  }
  dbgPrintMetadataItems(itm, true);
  return itm;
}
*/

/* *********************************** DEBUGGING SECTION *******************************************************************************************/
// Print the metadata item 
void SMPTE_ST2094_50::dbgPrintMetadataItems(bool decode) {

    std::cout << "============================================= METADATA ITEMS ========================================================================\n";
    std::cout <<"windowNumber=" << pWin.windowNumber << "\n";
    std::cout <<"hdrReferenceWhite=" << cvt.hdrReferenceWhite << "\n";
    std::cout <<"baselineHdrHeadroom=" << cvt.hatm.baselineHdrHeadroom << "\n";
    if ( isHeadroomAdaptiveToneMap || decode) {
      std::cout <<"numAlternateImages=" << cvt.hatm.numAlternateImages << "\n";
  
      std::cout << "gainApplicationSpaceChromaticities=[" ;
      for (float val : cvt.hatm.gainApplicationSpaceChromaticities) {
        std::cout << val << ", ";
      }
      std::cout << "]" << std::endl;
  
      for (uint32_t iAlt = 0; iAlt < cvt.hatm.numAlternateImages; iAlt++) {
        std::cout <<"alternateHdrHeadroom=" << cvt.hatm.alternateHdrHeadroom[iAlt] << "\n";
        std::cout <<"componentMixRed=" << cvt.hatm.cgf[iAlt].cm.componentMixRed << "\n";
        std::cout <<"componentMixGreen=" << cvt.hatm.cgf[iAlt].cm.componentMixGreen << "\n";
        std::cout <<"componentMixBlue=" << cvt.hatm.cgf[iAlt].cm.componentMixBlue << "\n";
        std::cout <<"componentMixMax=" << cvt.hatm.cgf[iAlt].cm.componentMixMax << "\n";
        std::cout <<"componentMixMin=" << cvt.hatm.cgf[iAlt].cm.componentMixMin << "\n";
        std::cout <<"componentMixComponent=" << cvt.hatm.cgf[iAlt].cm.componentMixComponent << "\n";
  
        std::cout <<"gainCurveNumControlPoints=" << cvt.hatm.cgf[iAlt].gc.gainCurveNumControlPoints << "\n";
  
        std::cout << "gainCurveControlPointX=[" << std::endl;
        for (float val : cvt.hatm.cgf[iAlt].gc.gainCurveControlPointX) {
            std::cout << val << ", ";
        }
        std::cout << "]" << std::endl;
  
        std::cout << "gainCurveControlPointY=[" << std::endl;
        for (float val : cvt.hatm.cgf[iAlt].gc.gainCurveControlPointY) {
            std::cout << val << ", ";
        }
        std::cout << "]" << std::endl;
  
        std::cout << "gainCurveControlPointTheta=[" << std::endl;
        for (float val : cvt.hatm.cgf[iAlt].gc.gainCurveControlPointTheta) {
            std::cout << val << ", ";
        }
        std::cout << "]" << std::endl;
      }
    }
    std::cout << "===================================================================================================================================]\n";
  }