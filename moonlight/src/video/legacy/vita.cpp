/*
    Copyright 2025 AorsiniYT

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#include "vita.hpp"
#include "ConfigManager.hpp"
#include "debug.hpp"
#include "gamestream/sps.h" // clase SpsContext + flags
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/display.h>
#include <psp2/videodec.h>
#include <stdlib.h>
#include <memory> // std::unique_ptr, std::make_unique
#include <string.h>
#include <stdio.h>
#include <memory>

// Refactored modules
#include "modules/vita_globals.hpp"
// The .cpp files are compiled separately via CMake

// Callbacks para Limelight
DECODER_RENDERER_CALLBACKS decoder_callbacks_vita_new = {
    .setup = vitavideo_setup,
    .start = vitavideo_start,
    .stop = vitavideo_stop,
    .cleanup = vita_cleanup,
    .submitDecodeUnit = vitavideo_submit_decode_unit,
    .capabilities = CAPABILITY_DIRECT_SUBMIT | CAPABILITY_SLICES_PER_FRAME(2)
};

static_assert(sizeof(DECODER_RENDERER_CALLBACKS) == (sizeof(void*)*5 + sizeof(int)), "Tamaño inesperado de DECODER_RENDERER_CALLBACKS (posible padding diferente)");
