#!/bin/bash

f=0
while true; do
    if [ ! -f "technique_0_frame_"$f"_sample_0.png" ]; then
        break
    fi

    s=0

    while true; do
        if [ ! -f "technique_0_frame_"$f"_sample_"$s".png" ]; then
            break
        fi

        # Generate color images of the unweighted contributions
        convert "technique_0_frame_"$f"_sample_"$s".png" -alpha off "merging_cf"$f"s"$s.png
        convert "technique_1_frame_"$f"_sample_"$s".png" -alpha off "connecting_cf"$f"s"$s.png
        convert "technique_2_frame_"$f"_sample_"$s".png" -alpha off "next_event_cf"$f"s"$s.png
        convert "technique_3_frame_"$f"_sample_"$s".png" -alpha off "cam_connect_cf"$f"s"$s.png
        convert "technique_4_frame_"$f"_sample_"$s".png" -alpha off "light_hit_cf"$f"s"$s.png

        # Generate grayscale images for the weights
        convert "technique_0_frame_"$f"_sample_"$s".png" -alpha extract "merging_weightsf"$f"s"$s.png
        convert "technique_1_frame_"$f"_sample_"$s".png" -alpha extract "connecting_weightsf"$f"s"$s.png
        convert "technique_2_frame_"$f"_sample_"$s".png" -alpha extract "next_event_weightsf"$f"s"$s.png
        convert "technique_3_frame_"$f"_sample_"$s".png" -alpha extract "cam_connect_weightsf"$f"s"$s.png
        convert "technique_4_frame_"$f"_sample_"$s".png" -alpha extract "light_hit_weightsf"$f"s"$s.png

        # Delete the original files
        rm "technique_0_frame_"$f"_sample_"$s".png"
        rm "technique_1_frame_"$f"_sample_"$s".png"
        rm "technique_2_frame_"$f"_sample_"$s".png"
        rm "technique_3_frame_"$f"_sample_"$s".png"
        rm "technique_4_frame_"$f"_sample_"$s".png"

        s=$((s + 1))
    done

    f=$((f + 1))
done