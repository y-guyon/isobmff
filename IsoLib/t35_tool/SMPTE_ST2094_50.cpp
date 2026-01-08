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
      break;
    case 16:
      std::cout.width(16); std::cout <<  std::bitset<16>(varValue).to_string() << "\n";
      break;
  default:
      break;
  }
}


void push_boolean(struct BinaryData *payloadBinaryData, bool boolValue, const std::string& varName){
    uint8_t decValue = static_cast<uint8_t>(boolValue) ;
    payloadBinaryData->payload[payloadBinaryData->byteIdx] = payloadBinaryData->payload[payloadBinaryData->byteIdx] + (decValue << (7 - payloadBinaryData->bitIdx));
    printDebug(varName, decValue,  1);
    //printDebug("Bool--" + varName, (decValue << (7 - payloadBinaryData->bitIdx)),  8);

    payloadBinaryData->bitIdx++;
    if (payloadBinaryData->bitIdx == uint8_t(8)){
        payloadBinaryData->bitIdx = 0;
        payloadBinaryData->payload.push_back(0); payloadBinaryData->byteIdx++;
    } else if (payloadBinaryData->bitIdx > 8) {
        // Error to handle
        dbgPrintMilestones(1, "************************************ Error To Check *************************************************************************************", decValue);
    }
}

void push_bits(struct BinaryData *payloadBinaryData, uint8_t value, uint8_t nbBits, const std::string& varName){
    payloadBinaryData->payload[payloadBinaryData->byteIdx] = payloadBinaryData->payload[payloadBinaryData->byteIdx] + (value << ( 8 - nbBits - payloadBinaryData->bitIdx));
    printDebug(varName, value,  nbBits);
    //printDebug("Bits--" + varName, (value << ( 8 - nbBits - payloadBinaryData->bitIdx)),  8);
    payloadBinaryData->bitIdx += nbBits;
    if ( payloadBinaryData->bitIdx == 8){
        payloadBinaryData->byteIdx++;
        payloadBinaryData->bitIdx = 0;
        payloadBinaryData->payload.push_back(0);
    } else if ( payloadBinaryData->bitIdx == 8) {
        // Error to handle
        dbgPrintMilestones(1, "************************************ Error To Check *************************************************************************************", value);
        dbgPrintMilestones(nbBits, varName, value);
    }
}

void push_8bits(struct BinaryData *payloadBinaryData, uint16_t value, const std::string& varName){
    // Verify that we are at the start of a byte
    if (payloadBinaryData->bitIdx != 0){
        dbgPrintMilestones(89, "************************************ Error To Check *************************************************************************************", value);
    } else {
        payloadBinaryData->payload[payloadBinaryData->byteIdx] = uint8_t(value & 0x00FF);
        payloadBinaryData->payload.push_back(0); payloadBinaryData->byteIdx++; 
        printDebug(varName, value,  8);
    }
}

void push_16bits(struct BinaryData *payloadBinaryData, uint16_t value, const std::string& varName){
    // Verify that we are at the start of a byte
    if (payloadBinaryData->bitIdx != 0){
        dbgPrintMilestones(89, "************************************ Error To Check *************************************************************************************", value);
    } else {
        payloadBinaryData->payload[payloadBinaryData->byteIdx] = uint8_t((value >> 8) & 0x00FF);
        payloadBinaryData->payload.push_back(0); payloadBinaryData->byteIdx++; 
        payloadBinaryData->payload[payloadBinaryData->byteIdx] = uint8_t((value     ) & 0x00FF);
        payloadBinaryData->payload.push_back(0); payloadBinaryData->byteIdx++; 
        printDebug(varName, value,  16);
    }
}

bool pull_boolean(struct BinaryData *payloadBinaryData, const std::string& varName){
    uint8_t decValue = (payloadBinaryData->payload[payloadBinaryData->byteIdx] >> (7 - payloadBinaryData->bitIdx)) & 0x01 ;
    bool result = static_cast<bool>(decValue) ;
    payloadBinaryData->bitIdx++;
    if (payloadBinaryData->bitIdx == uint8_t(8)){        
        payloadBinaryData->byteIdx++;
        payloadBinaryData->bitIdx = 0;
    } else if (payloadBinaryData->bitIdx > 8) {
        // Error to handle
        dbgPrintMilestones(1, "************************************ Error To Check *************************************************************************************", decValue);
    }
    printDebug(varName, decValue,  1);
    return result;
}

uint16_t pull_bits(struct BinaryData *payloadBinaryData, uint8_t nbBits, const std::string& varName){
    uint8_t decValue = uint8_t(payloadBinaryData->payload[payloadBinaryData->byteIdx] << payloadBinaryData->bitIdx) >> ((8 - nbBits)); 
    payloadBinaryData->bitIdx += nbBits;
    if ( payloadBinaryData->bitIdx == 8){
        payloadBinaryData->byteIdx++;
        payloadBinaryData->bitIdx = 0;
    } else if ( payloadBinaryData->bitIdx == 8) {
        // Error to handle
        dbgPrintMilestones(1, "************************************ Error To Check *************************************************************************************", decValue);
        dbgPrintMilestones(nbBits, varName, decValue);
    }
    printDebug(varName, decValue,  nbBits);
    return uint16_t(decValue);
}

uint16_t pull_8bits(struct BinaryData *payloadBinaryData, const std::string& varName){
    // Verify that we are at the start of a byte
    uint16_t decValue = 404;
    if (payloadBinaryData->bitIdx != 0){
        dbgPrintMilestones(89, "************************************ Error To Check *************************************************************************************", 0);
    } else {
        decValue = uint16_t(payloadBinaryData->payload[payloadBinaryData->byteIdx]);
    }
    printDebug(varName, decValue,  8);
    payloadBinaryData->byteIdx++;
    return decValue;
}

uint16_t pull_16bits(struct BinaryData *payloadBinaryData, const std::string& varName){
    // Verify that we are at the start of a byte
    uint16_t decValue = 404;
    if (payloadBinaryData->bitIdx != 0){
        dbgPrintMilestones(89, "************************************ Error To Check *************************************************************************************", 0);
    } else {
        decValue = uint16_t(payloadBinaryData->payload[payloadBinaryData->byteIdx]) << 8; payloadBinaryData->byteIdx++;
        decValue = decValue + uint16_t(payloadBinaryData->payload[payloadBinaryData->byteIdx]); payloadBinaryData->byteIdx++;
        printDebug(varName, decValue,  16);
    }
    return decValue;
}

/* *********************************** SMPTE ST 2094-50 FUNCTIONS *******************************************************************************************/
// Constructors
SMPTE_ST2094_50::SMPTE_ST2094_50(){
    keyValue = "B500900001:SMPTE-ST2094-50";

    // Application - fixed
    applicationIdentifier = 5;
    applicationVersion    = 255;

    // ProcessingWindow - fixed
    pWin.upperLeftCorner  = 0;
    pWin.lowerRightCorner = 0;
    pWin.windowNumber     = 1;

    // Initialize convenience flags
    isHeadroomAdaptiveToneMap = false;
    isReferenceWhiteToneMapping = false;
    for (uint16_t iAlt = 0; iAlt < MAX_NB_ALTERNATE; iAlt++) {
        isLinearInterpolation[iAlt] = false;
        hasSlopeParameter[iAlt] = false;
    }
}

// Getters
std::vector<uint8_t>    SMPTE_ST2094_50::getPayloadData(){return payloadBinaryData.payload;}
uint32_t                SMPTE_ST2094_50::getTimeIntervalStart(){return timeI.timeIntervalStart;}
uint32_t                SMPTE_ST2094_50::getTimeintervalDuration(){return timeI.timeintervalDuration;}

// Setters
void                    SMPTE_ST2094_50::setTimeIntervalStart(uint32_t frame_start){timeI.timeIntervalStart = frame_start;}
void                    SMPTE_ST2094_50::setTimeintervalDuration(uint32_t frame_duration){timeI.timeintervalDuration = frame_duration;}

/* *********************************** ENCODING SECTION ********************************************************************************************/
// Read from json file the metadata items
bool SMPTE_ST2094_50::decodeJsonToMetadataItems(nlohmann::json j) {
bool error_raised = false;
std::cout << "++++++++++++++++++++++ DECODE JSON TO METADATA ITEMS **++++++++++++++++++++++++++++++*********************************************]\n";

// TimeInterval
if (j.contains("frame_start") && j.contains("frame_duration")) {
  setTimeIntervalStart(j["frame_start"].get<uint32_t>());
  setTimeintervalDuration(j["frame_duration"].get<uint32_t>());
} else{
  // If not present attach it to only the first frame
  std::cerr << "JSON Decode: missing optional keys: frame_start | frame_duration\n";
}

// Checking mandatory metadata
if (!j.contains("hdrReferenceWhite") ) {
  std::cerr <<  "missing mandatory keys: hdrReferenceWhite\n";
    return error_raised;
}

// ================================================= Decode Metadata Items ===================================
// get mandatory metadata
cvt.hdrReferenceWhite       = j["hdrReferenceWhite"].get<float>();
if (j.contains("baselineHdrHeadroom") )
{
  isHeadroomAdaptiveToneMap    = true; // if there is a baselineHdrHeadroom then it is a headroom adaptive tone mapping
  cvt.hatm.baselineHdrHeadroom = j["baselineHdrHeadroom"].get<float>();

  if (!j.contains("numAlternateImages") || !j.contains("gainApplicationSpaceChromaticities") ){ // Derived Headroom Adaptive Tone Mapping parameters based on baselineHdrHeadroom
    isReferenceWhiteToneMapping  = true;
  }
  else // Custom Headroom Adaptive Tone Mapping mode
  {
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
      }

      // Color Gain Function 
      for (uint16_t iAlt = 0; iAlt <cvt.hatm.numAlternateImages; iAlt++) {
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
  elm.has_custom_hdr_reference_white_flag = false;
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
    elm.num_alternate_images = uint16_t(cvt.hatm.numAlternateImages);
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
      elm.has_common_component_mix_params_flag = true; // Init at true, any mismatch makes it false
      for (uint16_t iAlt = 1; iAlt < cvt.hatm.numAlternateImages; iAlt++) {
        // Check if any coefficient is different across alterante images
        if (abs(cvt.hatm.cgf[0].cm.componentMixRed       - cvt.hatm.cgf[iAlt].cm.componentMixRed)       > Q_COMPONENT_MIXING_COEFFICIENT || 
            abs(cvt.hatm.cgf[0].cm.componentMixGreen     - cvt.hatm.cgf[iAlt].cm.componentMixGreen)     > Q_COMPONENT_MIXING_COEFFICIENT || 
            abs(cvt.hatm.cgf[0].cm.componentMixBlue      - cvt.hatm.cgf[iAlt].cm.componentMixBlue)      > Q_COMPONENT_MIXING_COEFFICIENT || 
            abs(cvt.hatm.cgf[0].cm.componentMixMax       - cvt.hatm.cgf[iAlt].cm.componentMixMax)       > Q_COMPONENT_MIXING_COEFFICIENT || 
            abs(cvt.hatm.cgf[0].cm.componentMixMin       - cvt.hatm.cgf[iAlt].cm.componentMixMin)       > Q_COMPONENT_MIXING_COEFFICIENT || 
            abs(cvt.hatm.cgf[0].cm.componentMixComponent - cvt.hatm.cgf[iAlt].cm.componentMixComponent) > Q_COMPONENT_MIXING_COEFFICIENT) {
          elm.has_common_component_mix_params_flag = false;
          break;
        }
      } 

      // Check if all alternate have the same number of control points and interpolation
      elm.has_common_curve_params_flag = true;  // Init at true, any mismatch makes it false
      for (uint16_t iAlt = 1; iAlt < cvt.hatm.numAlternateImages; iAlt++) {
        if (cvt.hatm.cgf[0].gc.gainCurveNumControlPoints != cvt.hatm.cgf[iAlt].gc.gainCurveNumControlPoints){
          elm.has_common_component_mix_params_flag = false;
          break;
        }
      }

      // If they do, then check if all alternate have the same x position
      for (uint16_t pIdx = 0; pIdx < cvt.hatm.cgf[0].gc.gainCurveNumControlPoints; pIdx++){
        for (uint16_t iAlt = 1; iAlt < cvt.hatm.numAlternateImages; iAlt++) {
          if (cvt.hatm.cgf[0].gc.gainCurveControlPointX[pIdx] - cvt.hatm.cgf[iAlt].gc.gainCurveControlPointX[pIdx] > Q_GAIN_CURVE_CONTROL_POINT_X){
            elm.has_common_component_mix_params_flag = false;
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
          elm.component_mixing_type[iAlt] = 0; dbgPrintMilestones(426, "MaxRGB", cvt.hatm.cgf[iAlt].cm.componentMixMax);
        } else if (
          abs(cvt.hatm.cgf[iAlt].cm.componentMixRed      ) < P_COMPONENT_MIXING_COEFFICIENT &&
          abs(cvt.hatm.cgf[iAlt].cm.componentMixGreen    ) < P_COMPONENT_MIXING_COEFFICIENT &&
          abs(cvt.hatm.cgf[iAlt].cm.componentMixBlue     ) < P_COMPONENT_MIXING_COEFFICIENT &&
          abs(cvt.hatm.cgf[iAlt].cm.componentMixMax      ) < P_COMPONENT_MIXING_COEFFICIENT &&
          abs(cvt.hatm.cgf[iAlt].cm.componentMixMin      ) < P_COMPONENT_MIXING_COEFFICIENT &&
          abs(cvt.hatm.cgf[iAlt].cm.componentMixComponent - 1.0 ) < P_COMPONENT_MIXING_COEFFICIENT){ 
          elm.component_mixing_type[iAlt] = 1; dbgPrintMilestones(434, "Cmp", cvt.hatm.cgf[iAlt].cm.componentMixComponent);
        }  else if (
          abs(cvt.hatm.cgf[iAlt].cm.componentMixRed   - (1.0 / 6.0)) < P_COMPONENT_MIXING_COEFFICIENT &&
          abs(cvt.hatm.cgf[iAlt].cm.componentMixGreen - (1.0 / 6.0)) < P_COMPONENT_MIXING_COEFFICIENT &&
          abs(cvt.hatm.cgf[iAlt].cm.componentMixBlue  - (1.0 / 6.0)) < P_COMPONENT_MIXING_COEFFICIENT &&
          abs(cvt.hatm.cgf[iAlt].cm.componentMixMax   - (1.0 / 2.0)) < P_COMPONENT_MIXING_COEFFICIENT &&
          abs(cvt.hatm.cgf[iAlt].cm.componentMixMin      ) < P_COMPONENT_MIXING_COEFFICIENT &&
          abs(cvt.hatm.cgf[iAlt].cm.componentMixComponent) < P_COMPONENT_MIXING_COEFFICIENT){ 
          elm.component_mixing_type[iAlt] = 2; dbgPrintMilestones(442, "LumaA", cvt.hatm.cgf[iAlt].cm.componentMixRed);
        } else { // Send flag to true for each non-zero coefficient
          elm.component_mixing_type[iAlt] = 3; dbgPrintMilestones(444, "Custom", cvt.hatm.cgf[iAlt].cm.componentMixMax);
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
// Initialize the binary payload structure
payloadBinaryData.byteIdx = 0;
payloadBinaryData.bitIdx  = 0;
payloadBinaryData.payload.push_back(0);

std::cout << "++++++++++++++++++++++ start SMPTE_ST2094_50:: writeSyntaxElementsToBinaryData ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n";  
push_8bits(&payloadBinaryData, elm.application_version, "application_version");

push_boolean(&payloadBinaryData, elm.has_custom_hdr_reference_white_flag, "has_custom_hdr_reference_white_flag");
push_boolean(&payloadBinaryData, elm.has_adaptive_tone_map_flag, "has_adaptive_tone_map_flag");
push_bits(&payloadBinaryData, 0, 6, "zero_6");

if (elm.has_custom_hdr_reference_white_flag){
    push_16bits(&payloadBinaryData, elm.hdr_reference_white, "hdr_reference_white");
}

if (elm.has_adaptive_tone_map_flag) {
    push_16bits(&payloadBinaryData, elm.baseline_hdr_headroom, "baseline_hdr_headroom");
    push_boolean(&payloadBinaryData, elm.use_reference_white_tone_mapping_flag, "use_reference_white_tone_mapping_flag");
    if (!elm.use_reference_white_tone_mapping_flag) {
        push_bits(&payloadBinaryData, uint8_t(elm.num_alternate_images)                      , 3, "num_alternate_images");
        push_bits(&payloadBinaryData, uint8_t(elm.gain_application_space_chromaticities_mode), 2, "gain_application_space_chromaticities_mode");
        push_boolean(&payloadBinaryData, elm.has_common_component_mix_params_flag               , "has_common_component_mix_params_flag");
        push_boolean(&payloadBinaryData, elm.has_common_curve_params_flag                       , "has_common_curve_params_flag");
        if (elm.gain_application_space_chromaticities_mode == 3) {
            for (uint16_t iCh = 0; iCh < 8; iCh++) {
                push_16bits(&payloadBinaryData, elm.gain_application_space_chromaticities[iCh], "gain_application_space_chromaticities[iCh]");
            }
        }

        for (uint16_t iAlt = 0; iAlt < elm.num_alternate_images; iAlt++) {
            push_16bits(&payloadBinaryData, elm.alternate_hdr_headrooms[iAlt], "alternate_hdr_headrooms[iAlt]");
            // Write component mixing function parameters
            if ( iAlt == 0 || !elm.has_common_component_mix_params_flag){
                push_bits(&payloadBinaryData, uint8_t(elm.component_mixing_type[iAlt]), 2, "component_mixing_type[iAlt]");
                if (elm.component_mixing_type[iAlt] == 3) {
                    // Write the flag to indicate which coefficients are signaled 
                    uint8_t value_8 = 0;
                    for (uint8_t iCm = 0; iCm < MAX_NB_component_mixing_coefficient; iCm++) {
                        uint8_t flagValue = static_cast<uint8_t>(elm.has_component_mixing_coefficient_flag[iAlt][iCm]);
                        value_8 = value_8 + (flagValue << (5 - iCm) );
                    }   
                    push_bits(&payloadBinaryData, value_8, 6, "has_component_mixing_coefficient_flag[iAlt]");
                    // Write the coefficients 
                    for (uint8_t iCm = 0; iCm < MAX_NB_component_mixing_coefficient; iCm++) {
                        if (elm.has_component_mixing_coefficient_flag[iAlt][iCm]) {
                            push_16bits(&payloadBinaryData, elm.component_mixing_coefficient[iAlt][iCm], "component_mixing_coefficient[iAlt][iCm]");
                        }
                    }                
                } else {
                    push_bits(&payloadBinaryData, 0, 6, "zero_6bits[iAlt][iCm]");
                }
            }
            /// Write gain curve function parameters
            if ( iAlt == 0 || elm.has_common_curve_params_flag){
                push_bits(&payloadBinaryData,    elm.gain_curve_num_control_points_minus_1[iAlt], 5, "gain_curve_num_control_points_minus_1[iAlt]");
                push_boolean(&payloadBinaryData, elm.gain_curve_use_pchip_slope_flag[iAlt]      ,    "gain_curve_use_pchip_slope_flag[iAlt]"); 
                push_bits(&payloadBinaryData,     0                                             , 2, "zero_2bits[iAlt]");                
                for (uint16_t iCps = 0; iCps < elm.gain_curve_num_control_points_minus_1[iAlt] + 1; iCps++){
                    push_16bits(&payloadBinaryData, elm.gain_curve_control_points_x[iAlt][iCps], "gain_curve_control_points_x[iAlt][iCps]");
                }
            }
            for (uint16_t iCps = 0; iCps < elm.gain_curve_num_control_points_minus_1[iAlt] + 1; iCps++) {
                push_16bits(&payloadBinaryData, elm.gain_curve_control_points_y[iAlt][iCps], "gain_curve_control_points_y[iAlt][iCps]");
            }
            if (!elm.gain_curve_use_pchip_slope_flag[iAlt]) {
                for (uint16_t iCps = 0; iCps < elm.gain_curve_num_control_points_minus_1[iAlt] + 1; iCps++) {
                    push_16bits(&payloadBinaryData, elm.gain_curve_control_points_theta[iAlt][iCps], "gain_curve_control_points_theta[iAlt][iCps]");
                }
            }
        }
    }
    else{ // No more information need to be signaled when using Reference White Tone Mapping Operator
        push_bits(&payloadBinaryData, 0, 7, "zero_7bits");
    }
    // reomve the latest elements created but not used
    payloadBinaryData.payload.pop_back();
}

// outFile.close(); // Close the file
std::cout << "+++++++++++++++++++++++++++++++ End SMPTE_ST2094_50:: writeSyntaxElementsToBinaryData +++++++++++++++++++++++++++++++++]\n";
}

/* *********************************** DECODING SECTION ********************************************************************************************/

// Decode binary data into syntax elements
void SMPTE_ST2094_50::decodeBinaryToSyntaxElements(std::vector<uint8_t> binary_data) {

  // Adapt binary data
  // Initialize the binary payload structure
  payloadBinaryData.byteIdx = 0;
  payloadBinaryData.bitIdx  = 0;
  for (int i = 0; i < int(binary_data.size()); i++) {
      payloadBinaryData.payload.push_back(binary_data[i]);
      printDebug("Byte[" + std::to_string(i) + "]= ", uint16_t(binary_data[i]), 8);
  }

  std::cout << "++++++++++++++++++++++Syntax Elements Decoding ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n"; 
  elm.application_version = pull_8bits(&payloadBinaryData, "application_version");

  elm.has_custom_hdr_reference_white_flag = pull_boolean(&payloadBinaryData, "has_custom_hdr_reference_white_flag");
  elm.has_adaptive_tone_map_flag = pull_boolean(&payloadBinaryData, "has_adaptive_tone_map_flag");
  pull_bits(&payloadBinaryData, 6, "zero_6bits");

  if (elm.has_custom_hdr_reference_white_flag){
    elm.hdr_reference_white = pull_16bits(&payloadBinaryData, "hdr_reference_white");
  }

  if (elm.has_adaptive_tone_map_flag) {
    elm.baseline_hdr_headroom = pull_16bits(&payloadBinaryData, "baseline_hdr_headroom");  
    
    elm.use_reference_white_tone_mapping_flag = pull_boolean(&payloadBinaryData, "use_reference_white_tone_mapping_flag");
    if (!elm.use_reference_white_tone_mapping_flag){
        elm.num_alternate_images = pull_bits(&payloadBinaryData, 3, "num_alternate_images");
        elm.gain_application_space_chromaticities_mode = pull_bits(&payloadBinaryData, 2, "gain_application_space_chromaticities_mode");
        elm.has_common_component_mix_params_flag = pull_boolean(&payloadBinaryData, "has_common_component_mix_params_flag");
        elm.has_common_curve_params_flag = pull_boolean(&payloadBinaryData, "has_common_curve_params_flag");  

        if (elm.gain_application_space_chromaticities_mode == 3) {
            for (uint16_t iCh = 0; iCh < 8; iCh++) {
                elm.gain_application_space_chromaticities[iCh] = pull_16bits(&payloadBinaryData, "gain_application_space_chromaticities[iCh]");                
              }
        }

        for (uint16_t iAlt = 0; iAlt < elm.num_alternate_images; iAlt++) {
            elm.alternate_hdr_headrooms[iAlt] = pull_16bits(&payloadBinaryData, "elm.alternate_hdr_headrooms[iAlt]"); 

            // Read component mixing function parameters - Table C.4
            if ( iAlt == 0 || !elm.has_common_component_mix_params_flag){
                elm.component_mixing_type[iAlt] = pull_bits(&payloadBinaryData, 2, "elm.component_mixing_type[iAlt]");
              if (elm.component_mixing_type[iAlt] == 3) {
                uint8_t has_component_mixing_coefficient_flag = pull_bits(&payloadBinaryData, 6, "elm.component_mixing_type[iAlt]"); 
                // Decode the flags and the associated values
                for (uint8_t iCm = 0; iCm < MAX_NB_component_mixing_coefficient; iCm++) {
                    elm.has_component_mixing_coefficient_flag[iAlt][iCm] = bool( has_component_mixing_coefficient_flag & (0x01 << (5 - iCm) ) );
                    if (elm.has_component_mixing_coefficient_flag[iAlt][iCm]) {
                        elm.component_mixing_coefficient[iAlt][iCm] = pull_16bits(&payloadBinaryData, "component_mixing_coefficient[iAlt][iCm]"); 
                    } else {elm.component_mixing_coefficient[iAlt][iCm] = 0;}
                }                
              } else {
                pull_bits(&payloadBinaryData, 6, "zero_6bits"); 
              }
            } else {
                elm.component_mixing_type[iAlt] = elm.component_mixing_type[0];
                if (elm.component_mixing_type[0] == 3) {
                    elm.component_mixing_type[iAlt] = elm.component_mixing_type[0];
                    for (uint8_t iCm = 0; iCm < MAX_NB_component_mixing_coefficient; iCm++) {
                        elm.has_component_mixing_coefficient_flag[iAlt][iCm] = elm.has_component_mixing_coefficient_flag[iAlt][iCm];
                        elm.component_mixing_coefficient[iAlt][iCm] = elm.component_mixing_coefficient[0][iCm];
                    }
                }
            }

            /// Read gain curve function parameters - table C.5
            if ( iAlt == 0 || elm.has_common_curve_params_flag){
              elm.gain_curve_num_control_points_minus_1[iAlt] = pull_bits(&payloadBinaryData, 5, "gain_curve_num_control_points_minus_1[iAlt]");
                elm.gain_curve_use_pchip_slope_flag[iAlt] = pull_boolean(&payloadBinaryData, "gain_curve_use_pchip_slope_flag[iAlt]");
                pull_bits(&payloadBinaryData, 2, "zero_2bits");           
              
              for (uint16_t iCps = 0; iCps < elm.gain_curve_num_control_points_minus_1[iAlt] + 1; iCps++){
                elm.gain_curve_control_points_x[iAlt][iCps] = pull_16bits(&payloadBinaryData, "gain_curve_control_points_x[iAlt][iCps]");
              }
            } else {
              //elm.gain_curve_interpolation[iAlt] = elm.gain_curve_interpolation[0];
              elm.gain_curve_num_control_points_minus_1[iAlt] = elm.gain_curve_num_control_points_minus_1[0];
              elm.gain_curve_use_pchip_slope_flag[iAlt]       = elm.gain_curve_use_pchip_slope_flag[0];
              for (uint16_t iCps = 0; iCps < elm.gain_curve_num_control_points_minus_1[iAlt] + 1; iCps++){
                elm.gain_curve_control_points_x[iAlt][iCps] = elm.gain_curve_control_points_x[0][iCps];
              }
            }
            for (uint16_t iCps = 0; iCps < elm.gain_curve_num_control_points_minus_1[iAlt] + 1; iCps++) {
                elm.gain_curve_control_points_y[iAlt][iCps] = pull_16bits(&payloadBinaryData, "gain_curve_control_points_y[iAlt][iCps]");
            }
            if (!elm.gain_curve_use_pchip_slope_flag[iAlt]) {
              for (uint16_t iCps = 0; iCps < elm.gain_curve_num_control_points_minus_1[iAlt] + 1; iCps++) {
                elm.gain_curve_control_points_theta[iAlt][iCps] = pull_16bits(&payloadBinaryData, "gain_curve_control_points_theta[iAlt][iCps]");
              }
            }
        }

    }

}
std::cout << "++++++++++++++++++++++++++++++Syntax Elements Successfully Decoding ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++]\n";
}

// Convert the syntax elements to Metadata Items as described in Clause C.3
void SMPTE_ST2094_50::convertSyntaxElementsToMetadataItems(){

  // get mandatory metadata
  if (elm.has_custom_hdr_reference_white_flag) {
    cvt.hdrReferenceWhite = float(elm.hdr_reference_white) / Q_HDR_REFERENCE_WHITE;
  } else {
    cvt.hdrReferenceWhite = 203.0;
  }

  // Get Optional metadata items
  if (elm.has_adaptive_tone_map_flag) {
    isHeadroomAdaptiveToneMap = true;
    HeadroomAdaptiveToneMap hatm;
    cvt.hatm.baselineHdrHeadroom = float(elm.baseline_hdr_headroom) / Q_HDR_HEADROOM;
    if (elm.use_reference_white_tone_mapping_flag) {
      isReferenceWhiteToneMapping = true;
      cvt.hatm.numAlternateImages = 2;
      // BT.2020 primaries
      cvt.hatm.gainApplicationSpaceChromaticities[0] = 0.708;
      cvt.hatm.gainApplicationSpaceChromaticities[1] = 0.292;
      cvt.hatm.gainApplicationSpaceChromaticities[2] = 0.17 ;
      cvt.hatm.gainApplicationSpaceChromaticities[3] = 0.797;
      cvt.hatm.gainApplicationSpaceChromaticities[4] = 0.131;
      cvt.hatm.gainApplicationSpaceChromaticities[5] = 0.046;
      cvt.hatm.gainApplicationSpaceChromaticities[6] = 0.3127;
      cvt.hatm.gainApplicationSpaceChromaticities[7] = 0.3290;
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
        isLinearInterpolation[iAlt] = false;
        hasSlopeParameter[iAlt]     = true;
        // Component mixing is maxRGB
        ColorGainFunction cgf;
        cgf.cm.componentMixRed = 0.0;
        cgf.cm.componentMixGreen = 0.0;
        cgf.cm.componentMixBlue = 0.0;
        cgf.cm.componentMixMax = 1.0;
        cgf.cm.componentMixMin = 0.0;
        cgf.cm.componentMixComponent = 0.0; 
        cgf.gc.gainCurveNumControlPoints = 8;
  
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
  
        for (uint16_t iCps = 0; iCps < cgf.gc.gainCurveNumControlPoints; iCps++) {
          // Compute the control points
          float t = float(iCps) / (float(cgf.gc.gainCurveNumControlPoints) - 1.0);
          float t_square = t * t;
          float x = a_x * t_square + b_x * t + c_x;
          float y = a_y * t_square + b_y * t + c_y;
          float m = (2.0 * a_y * t + b_y) / (2 * a_x * t + b_x);
          float slope = atan( (x * m - y) / (log(2) * x * y) );
          cgf.gc.gainCurveControlPointX.push_back(x);
          cgf.gc.gainCurveControlPointY.push_back(log2(y / x));
          cgf.gc.gainCurveControlPointTheta.push_back(slope / PI_CUSTOM * 180.0);
        }
        cvt.hatm.cgf.push_back(cgf);
      }
    } else {
      cvt.hatm.numAlternateImages = elm.num_alternate_images;
      if (elm.gain_application_space_chromaticities_mode == 0){
        cvt.hatm.gainApplicationSpaceChromaticities[0] = 0.64;
        cvt.hatm.gainApplicationSpaceChromaticities[1] = 0.33; 
        cvt.hatm.gainApplicationSpaceChromaticities[2] = 0.3; 
        cvt.hatm.gainApplicationSpaceChromaticities[3] = 0.6;
        cvt.hatm.gainApplicationSpaceChromaticities[4] = 0.15; 
        cvt.hatm.gainApplicationSpaceChromaticities[5] = 0.06;
        cvt.hatm.gainApplicationSpaceChromaticities[6] = 0.3127;
        cvt.hatm.gainApplicationSpaceChromaticities[7] = 0.329;
      } else if (elm.gain_application_space_chromaticities_mode == 1){
        cvt.hatm.gainApplicationSpaceChromaticities[0] = 0.68; 
        cvt.hatm.gainApplicationSpaceChromaticities[1] = 0.32; 
        cvt.hatm.gainApplicationSpaceChromaticities[2] = 0.265; 
        cvt.hatm.gainApplicationSpaceChromaticities[3] = 0.69; 
        cvt.hatm.gainApplicationSpaceChromaticities[4] = 0.15; 
        cvt.hatm.gainApplicationSpaceChromaticities[5] = 0.06; 
        cvt.hatm.gainApplicationSpaceChromaticities[6] = 0.3127; 
        cvt.hatm.gainApplicationSpaceChromaticities[7] = 0.329; 
      } else if (elm.gain_application_space_chromaticities_mode == 2){
        cvt.hatm.gainApplicationSpaceChromaticities[0] = 0.708;
        cvt.hatm.gainApplicationSpaceChromaticities[1] = 0.292;
        cvt.hatm.gainApplicationSpaceChromaticities[2] = 0.17;
        cvt.hatm.gainApplicationSpaceChromaticities[3] = 0.797;
        cvt.hatm.gainApplicationSpaceChromaticities[4] = 0.131;
        cvt.hatm.gainApplicationSpaceChromaticities[5] = 0.046;
        cvt.hatm.gainApplicationSpaceChromaticities[6] = 0.3127;
        cvt.hatm.gainApplicationSpaceChromaticities[7] = 0.329;
      } else if (elm.gain_application_space_chromaticities_mode == 3){
          for (uint16_t iCh = 0; iCh < 8; iCh++) {
            cvt.hatm.gainApplicationSpaceChromaticities[iCh] = float(elm.gain_application_space_chromaticities[iCh]) / Q_GAIN_APPLICATION_SPACE_CHROMATICITY;
          }
      } else {
        std::cerr << "gain_application_space_primaries=" << elm.gain_application_space_chromaticities_mode << "  not defined.\n";
      }
      for (uint16_t iAlt = 0;  iAlt  < cvt.hatm.numAlternateImages; iAlt++){
        isLinearInterpolation[iAlt] = false;
        cvt.hatm.alternateHdrHeadroom.push_back(float(elm.alternate_hdr_headrooms[iAlt]) / Q_HDR_HEADROOM);
        // init k_params to zero and replace the one that are not
        ColorGainFunction cgf;
        cgf.cm.componentMixRed = 0.0;
        cgf.cm.componentMixGreen = 0.0;
        cgf.cm.componentMixBlue = 0.0;
        cgf.cm.componentMixMax= 0.0;
        cgf.cm.componentMixMin = 0.0;
        cgf.cm.componentMixComponent = 0.0;
        if (elm.component_mixing_type[iAlt] == 0){
          cgf.cm.componentMixMax   = 1.0;
        } else if (elm.component_mixing_type[iAlt] == 1){
          cgf.cm.componentMixComponent   = 1.0;
        } else if (elm.component_mixing_type[iAlt] == 2){
          cgf.cm.componentMixRed   = 1.0 / 6.0;
          cgf.cm.componentMixGreen = 1.0 / 6.0;
          cgf.cm.componentMixBlue  = 1.0 / 6.0;
          cgf.cm.componentMixMax   = 1.0 / 2.0;
        } else if (elm.component_mixing_type[iAlt] == 3){
          cgf.cm.componentMixRed       = float(elm.component_mixing_coefficient[iAlt][0]) / Q_COMPONENT_MIXING_COEFFICIENT;
          cgf.cm.componentMixGreen     = float(elm.component_mixing_coefficient[iAlt][0]) / Q_COMPONENT_MIXING_COEFFICIENT;
          cgf.cm.componentMixBlue      = float(elm.component_mixing_coefficient[iAlt][0]) / Q_COMPONENT_MIXING_COEFFICIENT;
          cgf.cm.componentMixMax       = float(elm.component_mixing_coefficient[iAlt][0]) / Q_COMPONENT_MIXING_COEFFICIENT;
          cgf.cm.componentMixMin       = float(elm.component_mixing_coefficient[iAlt][0]) / Q_COMPONENT_MIXING_COEFFICIENT;
          cgf.cm.componentMixComponent = float(elm.component_mixing_coefficient[iAlt][0]) / Q_COMPONENT_MIXING_COEFFICIENT;
        } else {
          std::cerr << "mix_encoding[" << iAlt << "]=" << elm.component_mixing_type[iAlt] << "  not defined.\n";
        }
        cgf.gc.gainCurveNumControlPoints = elm.gain_curve_num_control_points_minus_1[iAlt] + 1;
        for (uint16_t iCps = 0; iCps < cgf.gc.gainCurveNumControlPoints; iCps++) {
          cgf.gc.gainCurveControlPointX.push_back(float(elm.gain_curve_control_points_x[iAlt][iCps]) / Q_GAIN_CURVE_CONTROL_POINT_X);
          cgf.gc.gainCurveControlPointY.push_back(float(elm.gain_curve_control_points_y[iAlt][iCps]) / Q_GAIN_CURVE_CONTROL_POINT_Y - O_GAIN_CURVE_CONTROL_POINT_Y);
          if (!elm.gain_curve_use_pchip_slope_flag[iAlt]) {
            hasSlopeParameter[iAlt]     = true;
            cgf.gc.gainCurveControlPointTheta.push_back(float(elm.gain_curve_control_points_theta[iAlt][iCps]) / Q_GAIN_CURVE_CONTROL_POINT_THETA - O_GAIN_CURVE_CONTROL_POINT_THETA);
          }
        }
        cvt.hatm.cgf.push_back(cgf);
        }
    }
  }
}

nlohmann::json SMPTE_ST2094_50::encodeMetadataItemsToJson() {
  nlohmann::json j;

  j["frame_start"] = timeI.timeIntervalStart;
  j["frame_duration"] = timeI.timeintervalDuration;

  j["hdrReferenceWhite"] = cvt.hdrReferenceWhite;
  if (isHeadroomAdaptiveToneMap) {
    j["baselineHdrHeadroom"] = cvt.hatm.baselineHdrHeadroom;
    if (!isReferenceWhiteToneMapping) {
      j["numAlternateImages"] = cvt.hatm.numAlternateImages;
      j["gainApplicationSpaceChromaticities"] = cvt.hatm.gainApplicationSpaceChromaticities;

      if (cvt.hatm.numAlternateImages > 0) {
        j["alternateHdrHeadroom"] = cvt.hatm.alternateHdrHeadroom;

        std::vector<float> componentMixRed;
        std::vector<float> componentMixGreen;
        std::vector<float> componentMixBlue;
        std::vector<float> componentMixMax;
        std::vector<float> componentMixMin;
        std::vector<float> componentMixComponent;

        std::vector<uint32_t> gainCurveNumControlPoints;
        std::vector<std::vector<float>> gainCurveControlPointX_ptr;
        std::vector<std::vector<float>> gainCurveControlPointY_ptr;
        std::vector<std::vector<float>> gainCurveControlPointT_ptr;
        // "Unnesting" the variables: Accessing individual members of each struct
        for (size_t i = 0; i < cvt.hatm.cgf.size(); i++) { // Range-based for loop for convenience
          componentMixRed.push_back(cvt.hatm.cgf[i].cm.componentMixRed);
          componentMixGreen.push_back(cvt.hatm.cgf[i].cm.componentMixGreen);
          componentMixBlue.push_back(cvt.hatm.cgf[i].cm.componentMixBlue);
          componentMixMax.push_back(cvt.hatm.cgf[i].cm.componentMixMax);
          componentMixMin.push_back(cvt.hatm.cgf[i].cm.componentMixMin);
          componentMixComponent.push_back(cvt.hatm.cgf[i].cm.componentMixComponent);

          gainCurveNumControlPoints.push_back(cvt.hatm.cgf[i].gc.gainCurveNumControlPoints);
          std::vector<float> gainCurveControlPointX;
          std::vector<float> gainCurveControlPointY;
          std::vector<float> gainCurveControlPointT;
          for (uint32_t p = 0; p < cvt.hatm.cgf[i].gc.gainCurveNumControlPoints; p++){
            gainCurveControlPointX.push_back(cvt.hatm.cgf[i].gc.gainCurveControlPointX[p]);
            gainCurveControlPointY.push_back(cvt.hatm.cgf[i].gc.gainCurveControlPointY[p]);
          }
          for (size_t p = 0; p < cvt.hatm.cgf[i].gc.gainCurveControlPointTheta.size(); p++){
            gainCurveControlPointT.push_back(cvt.hatm.cgf[i].gc.gainCurveControlPointTheta[p]);
          }
          gainCurveControlPointX_ptr.push_back(gainCurveControlPointX);
          gainCurveControlPointY_ptr.push_back(gainCurveControlPointY);
          gainCurveControlPointT_ptr.push_back(gainCurveControlPointT);

        }

        j["componentMixRed"]        = componentMixRed;
        j["componentMixGreen"]      = componentMixGreen;
        j["componentMixBlue"]       = componentMixBlue;
        j["componentMixMax"]        = componentMixMax;
        j["componentMixMin"]        = componentMixMin;
        j["componentMixComponent"]  = componentMixComponent;

        j["gainCurveNumControlPoints"] = gainCurveNumControlPoints;
        j["gainCurveControlPointX"]    = gainCurveControlPointX_ptr;
        j["gainCurveControlPointY"]    = gainCurveControlPointY_ptr;
        j["gainCurveControlPointX"]    = gainCurveControlPointT_ptr;
      }
    }
  }
return j;
}

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