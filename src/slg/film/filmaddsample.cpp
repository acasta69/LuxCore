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

#include <limits>
#include <algorithm>
#include <exception>

#include <boost/lexical_cast.hpp>
#include <boost/foreach.hpp>

#include "slg/film/film.h"
#include "slg/film/sampleresult.h"
#include "slg/utils/varianceclamping.h"
#include "slg/film/denoiser/filmdenoiser.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Film add sample related methods
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Normal method versions
//------------------------------------------------------------------------------

void Film::AddSampleResultColor(const u_int x, const u_int y,
		const SampleResult &sampleResult, const float weight)  {
	filmDenoiser.AddSample(x, y, sampleResult, weight);

	if ((channel_RADIANCE_PER_PIXEL_NORMALIZEDs.size() > 0) && sampleResult.HasChannel(RADIANCE_PER_PIXEL_NORMALIZED)) {
		for (u_int i = 0; i < Min<u_int>(sampleResult.radiance.Size(), channel_RADIANCE_PER_PIXEL_NORMALIZEDs.size()); ++i) {
			if (sampleResult.radiance[i].IsNaN() || sampleResult.radiance[i].IsInf())
				continue;

			channel_RADIANCE_PER_PIXEL_NORMALIZEDs[i]->AddWeightedPixel(x, y, sampleResult.radiance[i].c, weight);
		}
	}

	// Faster than HasChannel(channel_RADIANCE_PER_SCREEN_NORMALIZED)
	if ((channel_RADIANCE_PER_SCREEN_NORMALIZEDs.size() > 0) && sampleResult.HasChannel(RADIANCE_PER_SCREEN_NORMALIZED)) {
		for (u_int i = 0; i < Min<u_int>(sampleResult.radiance.Size(), channel_RADIANCE_PER_SCREEN_NORMALIZEDs.size()); ++i) {
			if (sampleResult.radiance[i].IsNaN() || sampleResult.radiance[i].IsInf())
				continue;

			channel_RADIANCE_PER_SCREEN_NORMALIZEDs[i]->AddWeightedPixel(x, y, sampleResult.radiance[i].c, weight);
		}
	}

	// Faster than HasChannel(ALPHA)
	if (channel_ALPHA && sampleResult.HasChannel(ALPHA))
		channel_ALPHA->AddWeightedPixel(x, y, &sampleResult.alpha, weight);

	if (hasComposingChannel) {
		// Faster than HasChannel(DIRECT_DIFFUSE)
		if (channel_DIRECT_DIFFUSE && sampleResult.HasChannel(DIRECT_DIFFUSE))
			channel_DIRECT_DIFFUSE->AddWeightedPixel(x, y, sampleResult.directDiffuse.c, weight);

		// Faster than HasChannel(DIRECT_GLOSSY)
		if (channel_DIRECT_GLOSSY && sampleResult.HasChannel(DIRECT_GLOSSY))
			channel_DIRECT_GLOSSY->AddWeightedPixel(x, y, sampleResult.directGlossy.c, weight);

		// Faster than HasChannel(EMISSION)
		if (channel_EMISSION && sampleResult.HasChannel(EMISSION))
			channel_EMISSION->AddWeightedPixel(x, y, sampleResult.emission.c, weight);

		// Faster than HasChannel(INDIRECT_DIFFUSE)
		if (channel_INDIRECT_DIFFUSE && sampleResult.HasChannel(INDIRECT_DIFFUSE))
			channel_INDIRECT_DIFFUSE->AddWeightedPixel(x, y, sampleResult.indirectDiffuse.c, weight);

		// Faster than HasChannel(INDIRECT_GLOSSY)
		if (channel_INDIRECT_GLOSSY && sampleResult.HasChannel(INDIRECT_GLOSSY))
			channel_INDIRECT_GLOSSY->AddWeightedPixel(x, y, sampleResult.indirectGlossy.c, weight);

		// Faster than HasChannel(INDIRECT_SPECULAR)
		if (channel_INDIRECT_SPECULAR && sampleResult.HasChannel(INDIRECT_SPECULAR))
			channel_INDIRECT_SPECULAR->AddWeightedPixel(x, y, sampleResult.indirectSpecular.c, weight);

		// This is MATERIAL_ID_MASK and BY_MATERIAL_ID
		if (sampleResult.HasChannel(MATERIAL_ID)) {
			// MATERIAL_ID_MASK
			for (u_int i = 0; i < maskMaterialIDs.size(); ++i) {
				float pixel[2];
				pixel[0] = (sampleResult.materialID == maskMaterialIDs[i]) ? weight : 0.f;
				pixel[1] = weight;
				channel_MATERIAL_ID_MASKs[i]->AddPixel(x, y, pixel);
			}

			// BY_MATERIAL_ID
			if ((channel_RADIANCE_PER_PIXEL_NORMALIZEDs.size() > 0) && sampleResult.HasChannel(RADIANCE_PER_PIXEL_NORMALIZED)) {
				for (u_int index = 0; index < byMaterialIDs.size(); ++index) {
					Spectrum c;

					if (sampleResult.materialID == byMaterialIDs[index]) {
						// Merge all radiance groups
						for (u_int i = 0; i < Min<u_int>(sampleResult.radiance.Size(), channel_RADIANCE_PER_PIXEL_NORMALIZEDs.size()); ++i) {
							if (sampleResult.radiance[i].IsNaN() || sampleResult.radiance[i].IsInf())
								continue;

							c += sampleResult.radiance[i];
						}
					}

					channel_BY_MATERIAL_IDs[index]->AddWeightedPixel(x, y, c.c, weight);
				}
			}
		}

		// Faster than HasChannel(DIRECT_SHADOW)
		if (channel_DIRECT_SHADOW_MASK && sampleResult.HasChannel(DIRECT_SHADOW_MASK))
			channel_DIRECT_SHADOW_MASK->AddWeightedPixel(x, y, &sampleResult.directShadowMask, weight);

		// Faster than HasChannel(INDIRECT_SHADOW_MASK)
		if (channel_INDIRECT_SHADOW_MASK && sampleResult.HasChannel(INDIRECT_SHADOW_MASK))
			channel_INDIRECT_SHADOW_MASK->AddWeightedPixel(x, y, &sampleResult.indirectShadowMask, weight);

		// Faster than HasChannel(IRRADIANCE)
		if (channel_IRRADIANCE && sampleResult.HasChannel(IRRADIANCE))
			channel_IRRADIANCE->AddWeightedPixel(x, y, sampleResult.irradiance.c, weight);

		// This is OBJECT_ID_MASK and BY_OBJECT_ID
		if (sampleResult.HasChannel(OBJECT_ID)) {
			// OBJECT_ID_MASK
			for (u_int i = 0; i < maskObjectIDs.size(); ++i) {
				float pixel[2];
				pixel[0] = (sampleResult.objectID == maskObjectIDs[i]) ? weight : 0.f;
				pixel[1] = weight;
				channel_OBJECT_ID_MASKs[i]->AddPixel(x, y, pixel);
			}

			// BY_OBJECT_ID
			if ((channel_RADIANCE_PER_PIXEL_NORMALIZEDs.size() > 0) && sampleResult.HasChannel(RADIANCE_PER_PIXEL_NORMALIZED)) {
				for (u_int index = 0; index < byObjectIDs.size(); ++index) {
					Spectrum c;

					if (sampleResult.objectID == byObjectIDs[index]) {
						// Merge all radiance groups
						for (u_int i = 0; i < Min<u_int>(sampleResult.radiance.Size(), channel_RADIANCE_PER_PIXEL_NORMALIZEDs.size()); ++i) {
							if (sampleResult.radiance[i].IsNaN() || sampleResult.radiance[i].IsInf())
								continue;

							c += sampleResult.radiance[i];
						}
					}

					channel_BY_OBJECT_IDs[index]->AddWeightedPixel(x, y, c.c, weight);
				}
			}
		}

		// Faster than HasChannel(MATERIAL_ID_COLOR)
		if (channel_MATERIAL_ID_COLOR && sampleResult.HasChannel(MATERIAL_ID_COLOR)) {
			const u_int matID = sampleResult.materialID;
			const Spectrum matColID(
					(matID & 0x0000ffu) * (1.f / 255.f),
					((matID & 0x00ff00u) >> 8) * (1.f / 255.f),
					((matID & 0xff0000u) >> 16) * (1.f / 255.f));

			channel_MATERIAL_ID_COLOR->AddWeightedPixel(x, y, matColID.c, weight);
		}

		// Faster than HasChannel(ALBEDO)
		if (channel_ALBEDO && sampleResult.HasChannel(ALBEDO))
			channel_ALBEDO->AddWeightedPixel(x, y, sampleResult.albedo.c, weight);
		
		// Faster than HasChannel(AVG_SHADING_NORMAL)
		if (channel_AVG_SHADING_NORMAL && sampleResult.HasChannel(AVG_SHADING_NORMAL))
			channel_AVG_SHADING_NORMAL->AddWeightedPixel(x, y, &sampleResult.shadingNormal.x, weight);
	}
}

void Film::AddSampleResultData(const u_int x, const u_int y,
		const SampleResult &sampleResult)  {
	bool depthWrite = true;

	// Faster than HasChannel(DEPTH)
	if (channel_DEPTH && sampleResult.HasChannel(DEPTH))
		depthWrite = channel_DEPTH->MinPixel(x, y, &sampleResult.depth);

	if (depthWrite) {
	// Faster than HasChannel(POSITION)
	if (channel_POSITION && sampleResult.HasChannel(POSITION))
		channel_POSITION->SetPixel(x, y, &sampleResult.position.x);

	// Faster than HasChannel(GEOMETRY_NORMAL)
	if (channel_GEOMETRY_NORMAL && sampleResult.HasChannel(GEOMETRY_NORMAL))
		channel_GEOMETRY_NORMAL->SetPixel(x, y, &sampleResult.geometryNormal.x);

	// Faster than HasChannel(SHADING_NORMAL)
	if (channel_SHADING_NORMAL && sampleResult.HasChannel(SHADING_NORMAL))
		channel_SHADING_NORMAL->SetPixel(x, y, &sampleResult.shadingNormal.x);

		// Faster than HasChannel(MATERIAL_ID)
		if (channel_MATERIAL_ID && sampleResult.HasChannel(MATERIAL_ID))
			channel_MATERIAL_ID->SetPixel(x, y, &sampleResult.materialID);

		// Faster than HasChannel(UV)
		if (channel_UV && sampleResult.HasChannel(UV))
			channel_UV->SetPixel(x, y, &sampleResult.uv.u);

		// Faster than HasChannel(OBJECT_ID)
		if (channel_OBJECT_ID && sampleResult.HasChannel(OBJECT_ID) &&
				(sampleResult.objectID != std::numeric_limits<u_int>::max()))
			channel_OBJECT_ID->SetPixel(x, y, &sampleResult.objectID);
	}

	if (channel_RAYCOUNT && sampleResult.HasChannel(RAYCOUNT))
		channel_RAYCOUNT->AddPixel(x, y, &sampleResult.rayCount);

	if (channel_SAMPLECOUNT && sampleResult.HasChannel(SAMPLECOUNT)) {
		static u_int one = 1;
		channel_SAMPLECOUNT->AddPixel(x, y, &one);
	}
	
	// Nothing to do for CONVERGENCE channel because it is updated
	// by the convergence test
}

void Film::AddSample(const u_int x, const u_int y,
		const SampleResult &sampleResult, const float weight) {
	AddSampleResultColor(x, y, sampleResult, weight);
	if (hasDataChannel)
		AddSampleResultData(x, y, sampleResult);
}

//------------------------------------------------------------------------------
// Atomic method versions
//------------------------------------------------------------------------------

void Film::AtomicAddSampleResultColor(const u_int x, const u_int y,
		const SampleResult &sampleResult, const float weight)  {
	filmDenoiser.AddSample(x, y, sampleResult, weight);

	if ((channel_RADIANCE_PER_PIXEL_NORMALIZEDs.size() > 0) && sampleResult.HasChannel(RADIANCE_PER_PIXEL_NORMALIZED)) {
		for (u_int i = 0; i < Min<u_int>(sampleResult.radiance.Size(), channel_RADIANCE_PER_PIXEL_NORMALIZEDs.size()); ++i) {
			if (sampleResult.radiance[i].IsNaN() || sampleResult.radiance[i].IsInf())
				continue;

			channel_RADIANCE_PER_PIXEL_NORMALIZEDs[i]->AtomicAddWeightedPixel(x, y, sampleResult.radiance[i].c, weight);
		}
	}

	// Faster than HasChannel(channel_RADIANCE_PER_SCREEN_NORMALIZED)
	if ((channel_RADIANCE_PER_SCREEN_NORMALIZEDs.size() > 0) && sampleResult.HasChannel(RADIANCE_PER_SCREEN_NORMALIZED)) {
		for (u_int i = 0; i < Min<u_int>(sampleResult.radiance.Size(), channel_RADIANCE_PER_SCREEN_NORMALIZEDs.size()); ++i) {
			if (sampleResult.radiance[i].IsNaN() || sampleResult.radiance[i].IsInf())
				continue;

			channel_RADIANCE_PER_SCREEN_NORMALIZEDs[i]->AtomicAddWeightedPixel(x, y, sampleResult.radiance[i].c, weight);
		}
	}

	// Faster than HasChannel(ALPHA)
	if (channel_ALPHA && sampleResult.HasChannel(ALPHA))
		channel_ALPHA->AtomicAddWeightedPixel(x, y, &sampleResult.alpha, weight);

	if (hasComposingChannel) {
		// Faster than HasChannel(DIRECT_DIFFUSE)
		if (channel_DIRECT_DIFFUSE && sampleResult.HasChannel(DIRECT_DIFFUSE))
			channel_DIRECT_DIFFUSE->AtomicAddWeightedPixel(x, y, sampleResult.directDiffuse.c, weight);

		// Faster than HasChannel(DIRECT_GLOSSY)
		if (channel_DIRECT_GLOSSY && sampleResult.HasChannel(DIRECT_GLOSSY))
			channel_DIRECT_GLOSSY->AtomicAddWeightedPixel(x, y, sampleResult.directGlossy.c, weight);

		// Faster than HasChannel(EMISSION)
		if (channel_EMISSION && sampleResult.HasChannel(EMISSION))
			channel_EMISSION->AtomicAddWeightedPixel(x, y, sampleResult.emission.c, weight);

		// Faster than HasChannel(INDIRECT_DIFFUSE)
		if (channel_INDIRECT_DIFFUSE && sampleResult.HasChannel(INDIRECT_DIFFUSE))
			channel_INDIRECT_DIFFUSE->AtomicAddWeightedPixel(x, y, sampleResult.indirectDiffuse.c, weight);

		// Faster than HasChannel(INDIRECT_GLOSSY)
		if (channel_INDIRECT_GLOSSY && sampleResult.HasChannel(INDIRECT_GLOSSY))
			channel_INDIRECT_GLOSSY->AtomicAddWeightedPixel(x, y, sampleResult.indirectGlossy.c, weight);

		// Faster than HasChannel(INDIRECT_SPECULAR)
		if (channel_INDIRECT_SPECULAR && sampleResult.HasChannel(INDIRECT_SPECULAR))
			channel_INDIRECT_SPECULAR->AtomicAddWeightedPixel(x, y, sampleResult.indirectSpecular.c, weight);

		// This is MATERIAL_ID_MASK and BY_MATERIAL_ID
		if (sampleResult.HasChannel(MATERIAL_ID)) {
			// MATERIAL_ID_MASK
			for (u_int i = 0; i < maskMaterialIDs.size(); ++i) {
				float pixel[2];
				pixel[0] = (sampleResult.materialID == maskMaterialIDs[i]) ? weight : 0.f;
				pixel[1] = weight;
				channel_MATERIAL_ID_MASKs[i]->AtomicAddPixel(x, y, pixel);
			}

			// BY_MATERIAL_ID
			if ((channel_RADIANCE_PER_PIXEL_NORMALIZEDs.size() > 0) && sampleResult.HasChannel(RADIANCE_PER_PIXEL_NORMALIZED)) {
				for (u_int index = 0; index < byMaterialIDs.size(); ++index) {
					Spectrum c;

					if (sampleResult.materialID == byMaterialIDs[index]) {
						// Merge all radiance groups
						for (u_int i = 0; i < Min<u_int>(sampleResult.radiance.Size(), channel_RADIANCE_PER_PIXEL_NORMALIZEDs.size()); ++i) {
							if (sampleResult.radiance[i].IsNaN() || sampleResult.radiance[i].IsInf())
								continue;

							c += sampleResult.radiance[i];
						}
					}

					channel_BY_MATERIAL_IDs[index]->AtomicAddWeightedPixel(x, y, c.c, weight);
				}
			}
		}

		// Faster than HasChannel(DIRECT_SHADOW)
		if (channel_DIRECT_SHADOW_MASK && sampleResult.HasChannel(DIRECT_SHADOW_MASK))
			channel_DIRECT_SHADOW_MASK->AtomicAddWeightedPixel(x, y, &sampleResult.directShadowMask, weight);

		// Faster than HasChannel(INDIRECT_SHADOW_MASK)
		if (channel_INDIRECT_SHADOW_MASK && sampleResult.HasChannel(INDIRECT_SHADOW_MASK))
			channel_INDIRECT_SHADOW_MASK->AtomicAddWeightedPixel(x, y, &sampleResult.indirectShadowMask, weight);

		// Faster than HasChannel(IRRADIANCE)
		if (channel_IRRADIANCE && sampleResult.HasChannel(IRRADIANCE))
			channel_IRRADIANCE->AtomicAddWeightedPixel(x, y, sampleResult.irradiance.c, weight);

		// This is OBJECT_ID_MASK and BY_OBJECT_ID
		if (sampleResult.HasChannel(OBJECT_ID)) {
			// OBJECT_ID_MASK
			for (u_int i = 0; i < maskObjectIDs.size(); ++i) {
				float pixel[2];
				pixel[0] = (sampleResult.objectID == maskObjectIDs[i]) ? weight : 0.f;
				pixel[1] = weight;
				channel_OBJECT_ID_MASKs[i]->AtomicAddPixel(x, y, pixel);
			}

			// BY_OBJECT_ID
			if ((channel_RADIANCE_PER_PIXEL_NORMALIZEDs.size() > 0) && sampleResult.HasChannel(RADIANCE_PER_PIXEL_NORMALIZED)) {
				for (u_int index = 0; index < byObjectIDs.size(); ++index) {
					Spectrum c;

					if (sampleResult.objectID == byObjectIDs[index]) {
						// Merge all radiance groups
						for (u_int i = 0; i < Min<u_int>(sampleResult.radiance.Size(), channel_RADIANCE_PER_PIXEL_NORMALIZEDs.size()); ++i) {
							if (sampleResult.radiance[i].IsNaN() || sampleResult.radiance[i].IsInf())
								continue;

							c += sampleResult.radiance[i];
						}
					}

					channel_BY_OBJECT_IDs[index]->AtomicAddWeightedPixel(x, y, c.c, weight);
				}
			}
		}

		// Faster than HasChannel(MATERIAL_ID_COLOR)
		if (channel_MATERIAL_ID_COLOR && sampleResult.HasChannel(MATERIAL_ID_COLOR)) {
			const u_int matID = sampleResult.materialID;
			const Spectrum matColID(
					(matID & 0x0000ffu) * ( 1.f / 255.f),
					((matID & 0x00ff00u) >> 8) * ( 1.f / 255.f),
					((matID & 0xff0000u) >> 16) * ( 1.f / 255.f));

			channel_MATERIAL_ID_COLOR->AtomicAddWeightedPixel(x, y, matColID.c, weight);
		}
		
		// Faster than HasChannel(ALBEDO)
		if (channel_ALBEDO && sampleResult.HasChannel(ALBEDO))
			channel_ALBEDO->AtomicAddWeightedPixel(x, y, sampleResult.albedo.c, weight);

		// Faster than HasChannel(AVG_SHADING_NORMAL)
		if (channel_AVG_SHADING_NORMAL && sampleResult.HasChannel(AVG_SHADING_NORMAL))
			channel_AVG_SHADING_NORMAL->AtomicAddWeightedPixel(x, y, &sampleResult.shadingNormal.x, weight);
	}
}

void Film::AtomicAddSampleResultData(const u_int x, const u_int y,
		const SampleResult &sampleResult)  {
	bool depthWrite = true;

	// Faster than HasChannel(DEPTH)
	if (channel_DEPTH && sampleResult.HasChannel(DEPTH))
		depthWrite = channel_DEPTH->AtomicMinPixel(x, y, &sampleResult.depth);

	if (depthWrite) {
		// Faster than HasChannel(POSITION)
		if (channel_POSITION && sampleResult.HasChannel(POSITION))
			channel_POSITION->SetPixel(x, y, &sampleResult.position.x);

		// Faster than HasChannel(GEOMETRY_NORMAL)
		if (channel_GEOMETRY_NORMAL && sampleResult.HasChannel(GEOMETRY_NORMAL))
			channel_GEOMETRY_NORMAL->SetPixel(x, y, &sampleResult.geometryNormal.x);

		// Faster than HasChannel(SHADING_NORMAL)
		if (channel_SHADING_NORMAL && sampleResult.HasChannel(SHADING_NORMAL))
			channel_SHADING_NORMAL->SetPixel(x, y, &sampleResult.shadingNormal.x);

		// Faster than HasChannel(MATERIAL_ID)
		if (channel_MATERIAL_ID && sampleResult.HasChannel(MATERIAL_ID))
			channel_MATERIAL_ID->SetPixel(x, y, &sampleResult.materialID);

		// Faster than HasChannel(UV)
		if (channel_UV && sampleResult.HasChannel(UV))
			channel_UV->SetPixel(x, y, &sampleResult.uv.u);

		// Faster than HasChannel(OBJECT_ID)
		if (channel_OBJECT_ID && sampleResult.HasChannel(OBJECT_ID) &&
				(sampleResult.objectID != std::numeric_limits<u_int>::max()))
			channel_OBJECT_ID->SetPixel(x, y, &sampleResult.objectID);
	}

	if (channel_RAYCOUNT && sampleResult.HasChannel(RAYCOUNT))
		channel_RAYCOUNT->AtomicAddPixel(x, y, &sampleResult.rayCount);

	if (channel_SAMPLECOUNT && sampleResult.HasChannel(SAMPLECOUNT)) {
		static u_int one = 1;
		channel_SAMPLECOUNT->AtomicAddPixel(x, y, &one);
	}
	
	// Nothing to do for CONVERGENCE channel because it is updated
	// by the convergence test
}

void Film::AtomicAddSample(const u_int x, const u_int y,
		const SampleResult &sampleResult, const float weight) {
	AtomicAddSampleResultColor(x, y, sampleResult, weight);
	if (hasDataChannel)
		AtomicAddSampleResultData(x, y, sampleResult);
}
