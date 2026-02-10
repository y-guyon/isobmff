#!/bin/bash
#
folder="NoAdaptiveToneMap"
./t35_tool inject ST2094-50_HLG_LightDetector.mov ST2094-50_HLG_LightDetector_${folder}_mebx.mov --source smpte-folder:./${folder} --method mebx-it35 --t35-prefix B500900001:SMPTE-ST2094-50 > ${folder}_encode.log
./t35_tool extract ST2094-50_HLG_LightDetector_${folder}_mebx.mov ./output_folder --method auto --t35-prefix B500900001:SMPTE-ST2094-50 > ${folder}_decode.log


folder="DefaultToneMapRWTMO"
./t35_tool inject ST2094-50_HLG_LightDetector.mov ST2094-50_HLG_LightDetector_${folder}_mebx.mov --source smpte-folder:./${folder} --method mebx-it35 --t35-prefix B500900001:SMPTE-ST2094-50 > ${folder}_encode.log
./t35_tool extract ST2094-50_HLG_LightDetector_${folder}_mebx.mov ./output_folder --method auto --t35-prefix B500900001:SMPTE-ST2094-50 > ${folder}_decode.log

folder="CustomTMO"
./t35_tool inject ST2094-50_HLG_LightDetector.mov ST2094-50_HLG_LightDetector_${folder}_mebx.mov --source smpte-folder:./${folder} --method mebx-it35 --t35-prefix B500900001:SMPTE-ST2094-50 > ${folder}_encode.log
./t35_tool extract ST2094-50_HLG_LightDetector_${folder}_mebx.mov ./output_folder --method auto --t35-prefix B500900001:SMPTE-ST2094-50 > ${folder}_decode.log