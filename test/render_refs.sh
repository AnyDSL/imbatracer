#!/bin/bash
echo "Rendering all reference images..."

echo "Rendering still life scene (4 hours, vcm)"
../build/src/imbatracer/imbatracer scenes/stilllife/still_life.scene -a vcm -r 0.02 -w 1280 -h 720 --max-path-len 22 -q -t 14400 references/ref_still_life.png

echo "Rendering sponza scene (4 hours, pt)"
../build/src/imbatracer/imbatracer scenes/sponza/sponza.scene -a pt -w 1024 -h 1024 -q -t 14400 references/ref_sponza_curtain.png

echo "Rendering original cornell box (30 min, pt)"
../build/src/imbatracer/imbatracer scenes/cornell/cornell_org.scene -a pt -w 1024 -h 1024 -q -t 1800 references/ref_cornell_org.png

echo "Rendering indirect illuminated cornell box (30 min, bpt)"
../build/src/imbatracer/imbatracer scenes/cornell/cornell_indirect.scene -a bpt -w 1024 -h 1024 -q -t 1800 references/ref_cornell_indirect.png

echo "Rendering specular cornell box (interior view) (1 hour, vcm)"
../build/src/imbatracer/imbatracer scenes/cornell/cornell_specular.scene -a vcm -r 0.003 -w 1024 -h 1024 -q -t 3600 references/ref_cornell_specular.png

echo "Rendering specular cornell box (front view) (1 hour, vcm)"
../build/src/imbatracer/imbatracer scenes/cornell/cornell_specular_front.scene -a vcm -r 0.003 -w 1024 -h 1024 -q -t 3600 references/ref_cornell_specular_front.png

echo "Rendering cornell box with water (1 hour vcm)"
../build/src/imbatracer/imbatracer scenes/cornell/cornell_water.scene -a vcm -r 0.003 -w 1024 -h 1024 -q -t 3600 references/ref_cornell_water.png

echo "Rendering car scene (4 hours, vcm)"
../build/src/imbatracer/imbatracer scenes/car/car.scene -a vcm -w 1280 -h 720 --max-path-len 22 -q -t 14400 references/ref_car.png

echo "DONE"
