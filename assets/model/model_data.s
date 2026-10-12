# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: Copyright (c) 2026 Try Edge Tech / tryedgetech
# [PicoVoxel] model_data.s - World data file inclusion

.section .rodata
.align 4

# world for title
.global model_world_title_data
.global model_world_title_data_end
model_world_title_data:
    .incbin "world_title.bin"
model_world_title_data_end:

# world for japanese countryside
.global model_world_japanese_data
.global model_world_japanese_data_end
model_world_japanese_data:
    .incbin "world_japanese.bin"
model_world_japanese_data_end:
