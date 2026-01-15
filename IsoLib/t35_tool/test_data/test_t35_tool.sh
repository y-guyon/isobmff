#!/bin/bash
#

./t35_tool inject ST2094-50_LightDetector.mov ST2094-50_LightDetector_mebx.mov --source smpte-folder:./ExampleJson --method dedicated-it35

./t35_tool extract ST2094-50_LightDetector_mebx.mov ./output_folder --method auto --t35-prefix B500900001:SMPTE-ST2094-50