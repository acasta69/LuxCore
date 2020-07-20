/***************************************************************************
 * Copyright 1998-2020 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxCoreRender.                                   *
 *                                                                         *
 * Licensed under the Apache License, Version 2.0 (the "License");         *
 * you may not use this file except in compliance with the License.        *
 * You may obtain a copy of the License at                                 *
 *                                                                         *
 *     http://www.apache.org/licenses/LICENSE-2.0                          *
 *                                                                         *
 * Unless required by applicable law or agreed to in writing, software     *
 * distributed under the License is distributed on an "AS IS" BASIS,       *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.*
 * See the License for the specific language governing permissions and     *
 * limitations under the License.                                          *
 ***************************************************************************/

#include "luxrays/core/color/spds/blackbodyspd.h"

#include "slg/textures/blackbody.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Black body texture
//------------------------------------------------------------------------------

BlackBodyTexture::BlackBodyTexture(const float temp, const bool norm) :
		temperature(temp), normalize(norm) {
	BlackbodySPD spd(temperature);

	ColorSystem colorSpace;
	rgb = colorSpace.ToRGBConstrained(spd.ToXYZ()).Clamp(0.f);

	/*float maxValue = 0.f;
	for (u_int i = 0; i < 13000; i += 1) {
		BlackbodySPD spd(i);

		ColorSystem colorSpace;
		Spectrum s = colorSpace.ToRGBConstrained(spd.ToXYZ()).Clamp(0.f);
		maxValue = Max(maxValue, s.Max());
	}
	cout << maxValue << "\n";*/
	
	// To normalize rgb, divide by maxValue
	if (normalize)
		rgb /= 89159.6f;
}

Properties BlackBodyTexture::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.textures." + name + ".type")("blackbody"));
	props.Set(Property("scene.textures." + name + ".temperature")(temperature));
	props.Set(Property("scene.textures." + name + ".normalize")(normalize));

	return props;
}
