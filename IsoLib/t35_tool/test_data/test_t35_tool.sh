#!/bin/bash
#

echo "*********************************************  ME4C *********************************************"
./t35_tool inject ST2094-50_LightDetector.mov ST2094-50_LightDetector_me4c.mov --source smpte-folder:./ExampleJson 
./t35_tool extract ST2094-50_LightDetector_me4c.mov ./output_folder --method auto --t35-prefix B500900001:SMPTE-ST2094-50

echo ""
echo ""
echo "*********************************************  DEDICATED TRACK *********************************************"
./t35_tool inject ST2094-50_LightDetector.mov ST2094-50_LightDetector_it35Track.mov --source smpte-folder:./ExampleJson --method dedicated-it35 --t35-prefix B500900001:SMPTE-ST2094-50
./t35_tool extract ST2094-50_LightDetector_it35Track.mov ./output_folder --method auto --t35-prefix B500900001:SMPTE-ST2094-50

echo ""
echo ""
echo "*********************************************  SAMPLE GROUP *********************************************"
./t35_tool inject ST2094-50_LightDetector.mov ST2094-50_LightDetector_sampleGroup.mov --source smpte-folder:./ExampleJson --method sample-group
./t35_tool extract ST2094-50_LightDetector_sampleGroup.mov ./output_folder --method auto --t35-prefix B500900001:SMPTE-ST2094-50