/***************************************************************************
 * Copyright 1998-2018 by authors (see AUTHORS.txt)                        *
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

#include "slg/lights/infinitelight.h"
#include "slg/scene/scene.h"
#include "slg/lights/visibility/envlightvisibility.h"

using namespace std;
using namespace luxrays;
using namespace slg;
		
//------------------------------------------------------------------------------
// InfiniteLight
//------------------------------------------------------------------------------

InfiniteLight::InfiniteLight() :
	imageMap(NULL), sampleUpperHemisphereOnly(false),
	visibilityMapWidth(512), visibilityMapHeight(256),
	visibilityMapSamples(1000000), visibilityMapMaxDepth(4),
	useVisibilityMap(false) {
}

InfiniteLight::~InfiniteLight() {
	delete imageMapDistribution;
}

void InfiniteLight::Preprocess() {
	const ImageMapStorage *imageMapStorage = imageMap->GetStorage();

	vector<float> data(imageMap->GetWidth() * imageMap->GetHeight());
	for (u_int y = 0; y < imageMap->GetHeight(); ++y) {
		for (u_int x = 0; x < imageMap->GetWidth(); ++x) {
			const u_int index = x + y * imageMap->GetWidth();

			if (sampleUpperHemisphereOnly && (y > imageMap->GetHeight() / 2))
				data[index] = 0.f;
			else
				data[index] = imageMapStorage->GetFloat(index);
		}
	}

	imageMapDistribution = new Distribution2D(&data[0], imageMap->GetWidth(), imageMap->GetHeight());
}

void InfiniteLight::GetPreprocessedData(const Distribution2D **imageMapDistributionData) const {
	if (imageMapDistributionData)
		*imageMapDistributionData = imageMapDistribution;
	
}

float InfiniteLight::GetPower(const Scene &scene) const {
	const float envRadius = GetEnvRadius(scene);

	// TODO: I should consider sampleUpperHemisphereOnly here
	return gain.Y() * imageMap->GetSpectrumMeanY() *
			(4.f * M_PI * M_PI * envRadius * envRadius);
}

UV InfiniteLight::GetEnvUV(const luxrays::Vector &dir) const {
	UV uv;
	const Vector localDir = Normalize(Inverse(lightToWorld) * -dir);
	ToLatLongMapping(localDir, &uv.u, &uv.v);
	
	return uv;
}

Spectrum InfiniteLight::GetRadiance(const Scene &scene,
		const Vector &dir,
		float *directPdfA,
		float *emissionPdfW) const {
	const Vector localDir = Normalize(Inverse(lightToWorld) * -dir);

	float u, v, latLongMappingPdf;
	ToLatLongMapping(localDir, &u, &v, &latLongMappingPdf);
	if (latLongMappingPdf == 0.f)
		return Spectrum();

	const float distPdf = imageMapDistribution->Pdf(u, v);
	if (directPdfA)
		*directPdfA = distPdf * latLongMappingPdf;

	if (emissionPdfW) {
		const float envRadius = GetEnvRadius(scene);
		*emissionPdfW = distPdf * latLongMappingPdf / (M_PI * envRadius * envRadius);
	}

	return gain * imageMap->GetSpectrum(UV(u, v));
}

Spectrum InfiniteLight::Emit(const Scene &scene,
		const float u0, const float u1, const float u2, const float u3, const float passThroughEvent,
		Point *orig, Vector *dir,
		float *emissionPdfW, float *directPdfA, float *cosThetaAtLight) const {
	float uv[2];
	float distPdf;
	imageMapDistribution->SampleContinuous(u0, u1, uv, &distPdf);
	
	Vector localDir;
	float latLongMappingPdf;
	FromLatLongMapping(uv[0], uv[1], &localDir, &latLongMappingPdf);
	if (latLongMappingPdf == 0.f)
		return Spectrum();

	// Compute the ray direction
	const Vector rayDir = -Normalize(lightToWorld * localDir);

	// Compute the ray origin
	Vector x, y;
    CoordinateSystem(-rayDir, &x, &y);
    float d1, d2;
    ConcentricSampleDisk(u2, u3, &d1, &d2);

	const Point worldCenter = scene.dataSet->GetBSphere().center;
	const float envRadius = GetEnvRadius(scene);
	const Point pDisk = worldCenter + envRadius * (d1 * x + d2 * y);
	const Point rayOrig = pDisk - envRadius * rayDir;

	// Assign ray origin and direction
	*orig = rayOrig;
	*dir = rayDir;

	// Compute InfiniteLight ray weight
	*emissionPdfW = distPdf * latLongMappingPdf / (M_PI * envRadius * envRadius);

	if (directPdfA)
		*directPdfA = distPdf * latLongMappingPdf;

	if (cosThetaAtLight)
		*cosThetaAtLight = Dot(Normalize(worldCenter - rayOrig), rayDir);

	const Spectrum result = gain * imageMap->GetSpectrum(uv);
	assert (!result.IsNaN() && !result.IsInf() && !result.IsNeg());

	return result;
}

Spectrum InfiniteLight::Illuminate(const Scene &scene, const Point &p,
		const float u0, const float u1, const float passThroughEvent,
        Vector *dir, float *distance, float *directPdfW,
		float *emissionPdfW, float *cosThetaAtLight) const {
	float uv[2];
	float distPdf;
	imageMapDistribution->SampleContinuous(u0, u1, uv, &distPdf);

	Vector localDir;
	float latLongMappingPdf;
	FromLatLongMapping(uv[0], uv[1], &localDir, &latLongMappingPdf);
	if (latLongMappingPdf == 0.f)
		return Spectrum();

	*dir = Normalize(lightToWorld * localDir);

	const Point worldCenter = scene.dataSet->GetBSphere().center;
	const float envRadius = GetEnvRadius(scene);

	const Vector toCenter(worldCenter - p);
	const float centerDistance2 = Dot(toCenter, toCenter);
	const float approach = Dot(toCenter, *dir);
	*distance = approach + sqrtf(Max(0.f, envRadius * envRadius -
		centerDistance2 + approach * approach));

	const Point emisPoint(p + (*distance) * (*dir));
	const Normal emisNormal(Normalize(worldCenter - emisPoint));

	const float cosAtLight = Dot(emisNormal, -(*dir));
	if (cosAtLight < DEFAULT_COS_EPSILON_STATIC)
		return Spectrum();
	if (cosThetaAtLight)
		*cosThetaAtLight = cosAtLight;

	*directPdfW = distPdf * latLongMappingPdf;
	assert (!isnan(*directPdfW) && !isinf(*directPdfW) && (*directPdfW > 0.f));

	if (emissionPdfW)
		*emissionPdfW = distPdf * latLongMappingPdf / (M_PI * envRadius * envRadius);

	const Spectrum result = gain * imageMap->GetSpectrum(UV(uv[0], uv[1]));
	assert (!result.IsNaN() && !result.IsInf() && !result.IsNeg());

	return result;
}

void InfiniteLight::UpdateVisibilityMap(const Scene *scene) {
	if (useVisibilityMap) {
		// Scale the infinitelight image map to the requested size
		ImageMap *luminanceMapImage = imageMap->Copy();
		// Select luminance
		luminanceMapImage->SelectChannel(ImageMapStorage::WEIGHTED_MEAN);
		// Scale the image
		luminanceMapImage->Resize(visibilityMapWidth, visibilityMapHeight);
		
		EnvLightVisibility envLightVisibilityMapBuilder(scene, this,
				luminanceMapImage, sampleUpperHemisphereOnly,
				visibilityMapWidth, visibilityMapHeight,
				visibilityMapSamples, visibilityMapMaxDepth);
		
		Distribution2D *newDist = envLightVisibilityMapBuilder.Build();
		if (newDist) {
			delete imageMapDistribution;
			imageMapDistribution = newDist;
		}

		delete luminanceMapImage;
	}
}

Properties InfiniteLight::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	const string prefix = "scene.lights." + GetName();
	Properties props = EnvLightSource::ToProperties(imgMapCache, useRealFileName);

	props.Set(Property(prefix + ".type")("infinite"));
	const string fileName = useRealFileName ?
		imageMap->GetName() : imgMapCache.GetSequenceFileName(imageMap);
	props.Set(Property(prefix + ".file")(fileName));
	props.Set(imageMap->ToProperties(prefix, false));
	props.Set(Property(prefix + ".gamma")(1.f));
	props.Set(Property(prefix + ".sampleupperhemisphereonly")(sampleUpperHemisphereOnly));
	props.Set(Property(prefix + ".visibilitymap.enable")(useVisibilityMap));
	props.Set(Property(prefix + ".visibilitymap.width")(visibilityMapWidth));
	props.Set(Property(prefix + ".visibilitymap.height")(visibilityMapHeight));
	props.Set(Property(prefix + ".visibilitymap.samples")(visibilityMapSamples));
	props.Set(Property(prefix + ".visibilitymap.maxdepth")(visibilityMapMaxDepth));

	return props;
}
