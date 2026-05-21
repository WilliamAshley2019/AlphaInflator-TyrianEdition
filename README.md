--------------------------------------------------------------------------------------------------
Copyright (c) 2026 William Ashley d/b/a William Ashley Music ( http://WilliamAshley.music )
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License  (v3) 

This program is distributed in the hope that it will be useful to other audio programmers and music makers in their own plugin designs.
There is no WARRANTY expressed or implied including for MERCHANTABILITY or FITNESS FOR ANY PURPOSE. 
See the GNU General Public License for more details.

Attributtion is requested where possible if you use or modify any of the source,
Notice of use is requested so I can familiarize myself with how the code has been adapted for personal interest.
contact@WilliamAshley.music   
-----------------------------------------------------------------------------------------------------
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![JUCE](https://img.shields.io/badge/Built%20with-JUCE%208.0.12-blue)](https://juce.com)
[![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20-lightgrey)]()
[![Format](https://img.shields.io/badge/Format-VST3%20%7C%20-orange)]()

TO DO - merge all inflator plugins to a common repository folder


Still testing this.. GUI still a little buggy but the colour scheme is a little closer to what I was going for.  Some overlap still. Sound still has limits but its sort of predictable,
I should probably extend the range of +/- on this especially for input / output and the limiter itself. 



Currently it applies those concept like 

1. Harmonic Excitation which is known as the curve curveA * s1 + curveB * s2 + curveC * s3 - curveD * (s2 - 2*s3 + s4) 
The curve is asymmetrical and generates even and odd harmonics that make the ear perceive greater density and "presence" without just turning up the volume.

The curveD term specifically creates a soft-knee saturation that rounds off peaks while filling in the waveform valleys. That is cutting off the peaks and filling out the floor - since peak measurement can tell some analysis how loud it gets squashing / softclipping  etc.. those peaks may bring down your peaks or let you raise your floor dynamically relative to the peaks increasing overall loudness at cost of dynamic range. 

2. Band-Split Processing with Frequency Focus. Frequency Focus (focusCombo) concentrates the excitation on specific ranges — this is critical because boosting mid-range harmonics (2-5kHz) maximizes perceived loudness via the Fletcher-Munson curves. This goes back to EQ in that humans hear and process different ranges of audio frequency with different brain processes. The way and characteristics of those audio bands differs so where you place focus in those bands will either be non audible (wasted technical levels) or not heavily related to loudness itself as some bands may be more critically noticed based upon spatial recognition, timbre, or other aspects.

3. Transient Shaping + Dynamics Sensitivity. Transient density may allow sounds to be noticed more. Smoothing increasing RMS without hitting peaks. This is also acheived through compression methods such as parallel compression on drum buses. 

Clip modes (Hard/Soft/Hard+Soft) provide a final ceiling.

Spectral Balance Targeting	Human loudness perception is dominated by 1-4kHz "presence" region	Add a tilt EQ post-processor with adjustable slope (already partially present via TiltEQ, but make it parametric with Q)
Bass Enhancement via Sub-Harmonic Generation	Low-end perceived loudness requires fundamental + first harmonic	Add a sub-octave synthesizer (track pitch, generate -1 octave, blend 5-15%)
Stereo Width Control with Mono Compatibility	Wide stereo = perceived spaciousness, but collapsed mono must retain energy.


To add in next version???

K-Weighting filter + LUFS metering (short-term, momentary, integrated) - This is genuinely useful and the code is well-defined by ITU-R BS.1770-4
Adaptive release on the limiter - Good idea, relatively simple
Stereo correlation meter - Simple and useful for mono compatibility checking
Multi-stage saturation with character modes - Good audio engineering
True peak detection improvement - Worth doing
Spectral tilt EQ improvement (already have it, make slope adjustable)
