// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2026 Try Edge Tech / tryedgetech
// [PicoVoxel] features_no_macrocell.h - default preset with macrocell skip disabled (CI: check for macrocell)

#ifndef PICOVOXEL_FEATURES_NO_MACROCELL_H
#define PICOVOXEL_FEATURES_NO_MACROCELL_H

#define DO_WATCHDOG
//#define DO_MACROCELL_SKIP
//#define DO_MACROCELL_OPTIMIZE
#define DO_DEPTH_QUANTUM
//#define DO_PROFILING
//#define DO_DDA_STATISTICS
#define DO_OPTIMIZE_UPSCALE_AA
#define DO_REDUCE_STORECMD
#define DO_RACING_THE_BEAM
#define DO_TRANSPARENT
//#define DO_JOYSTICK_CONTROL
#define DO_TEXTURE_SAMPLING
#define DO_TEXTURE_ANIMATION
#define DO_TEXSAMPLING_INTERP
//#define DO_PASSTHROUGH_BLOCK
#define DO_BLOCK_SHADOW

#endif // PICOVOXEL_FEATURES_NO_MACROCELL_H