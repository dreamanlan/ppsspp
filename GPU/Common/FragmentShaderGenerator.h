// Copyright (c) 2012- PPSSPP Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0 or later versions.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official git repository and contact information can be found at
// https://github.com/hrydgard/ppsspp and http://www.ppsspp.org/.

#pragma once

#include "Common/Common.h"
#include "Common/GPU/Shader.h"
#include "Common/GPU/thin3d.h"

struct FShaderID;

// Can technically be deduced from the fragment shader ID, but this is safer.
enum class FragmentShaderFlags : u32 {
	USES_DISCARD = 2,
	USES_FLAT_SHADING = 4,
};
ENUM_CLASS_BITOPS(FragmentShaderFlags);

bool GenerateFragmentShader(const FShaderID &id, char *buffer, const ShaderLanguageDesc &compat, Draw::Bugs bugs, uint64_t *uniformMask, FragmentShaderFlags *fragmentShaderFlags, std::string *errorString);
