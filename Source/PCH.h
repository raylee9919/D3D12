// Copyright (c) Seong Woo Lee
// Licensed under the MIT license (https://opensource.org/license/mit/)

#include "ThirdParty/DirectX/d3dx12.h"
#define NOMINMAX
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <dxgi.h>
#include <dxgi1_4.h>
#include <d3dcompiler.h>
#if _DEBUG
#  include <dxgidebug.h>
#endif


#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <immintrin.h>


#define STB_SPRINTF_IMPLEMENTATION
#include "ThirdParty/STB/stb_sprintf.h"

#define STB_IMAGE_IMPLEMENTATION
#include "ThirdParty/STB/stb_image.h"
