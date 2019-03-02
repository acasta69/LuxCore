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

#include "slg/engines/lightcpu/lightcpu.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// LightCPU RenderThread
//------------------------------------------------------------------------------

LightCPURenderThread::LightCPURenderThread(LightCPURenderEngine *engine,
		const u_int index, IntersectionDevice *device) :
		CPUNoTileRenderThread(engine, index, device) {
}

SampleResult &LightCPURenderThread::AddResult(vector<SampleResult> &sampleResults, const bool fromLight) const {
	LightCPURenderEngine *engine = (LightCPURenderEngine *)renderEngine;

	const u_int size = sampleResults.size();
	sampleResults.resize(size + 1);

	SampleResult &sampleResult = sampleResults[size];

	sampleResult.Init(
			fromLight ?
				Film::RADIANCE_PER_SCREEN_NORMALIZED :
				(Film::RADIANCE_PER_PIXEL_NORMALIZED | Film::ALPHA | Film::DEPTH |
				Film::POSITION | Film::GEOMETRY_NORMAL | Film::SHADING_NORMAL | Film::MATERIAL_ID |
				Film::UV | Film::OBJECT_ID | Film::SAMPLECOUNT | Film::CONVERGENCE |
				Film::MATERIAL_ID_COLOR | Film::ALBEDO | Film::AVG_SHADING_NORMAL),
			engine->film->GetRadianceGroupCount());

	return sampleResult;
}

void LightCPURenderThread::ConnectToEye(const float time, const float u0,
		const LightSource &light,
		const BSDF &bsdf, const Point &lensPoint,
		const Spectrum &flux, PathVolumeInfo volInfo,
		vector<SampleResult> &sampleResults) {
	// I don't connect camera invisible objects with the eye
	if (bsdf.IsCameraInvisible())
		return;

	LightCPURenderEngine *engine = (LightCPURenderEngine *)renderEngine;
	Scene *scene = engine->renderConfig->scene;

	Vector eyeDir(bsdf.hitPoint.p - lensPoint);
	const float eyeDistance = eyeDir.Length();
	eyeDir /= eyeDistance;

	BSDFEvent event;
	Spectrum bsdfEval = bsdf.Evaluate(-eyeDir, &event);

	if (!bsdfEval.Black()) {
		Ray eyeRay(lensPoint, eyeDir,
				0.f,
				eyeDistance,
				time);
		eyeRay.UpdateMinMaxWithEpsilon();

		float filmX, filmY;
		if (scene->camera->GetSamplePosition(&eyeRay, &filmX, &filmY)) {
			// I have to flip the direction of the traced ray because
			// the information inside PathVolumeInfo are about the path from
			// the light toward the camera (i.e. ray.o would be in the wrong
			// place).
			Ray traceRay(bsdf.hitPoint.p, -eyeRay.d,
					0.f,
					eyeRay.maxt,
					time);
			traceRay.UpdateMinMaxWithEpsilon();
			RayHit traceRayHit;

			BSDF bsdfConn;
			Spectrum connectionThroughput;
			if (!scene->Intersect(device, true, true, &volInfo, u0, &traceRay, &traceRayHit, &bsdfConn,
					&connectionThroughput)) {
				// Nothing was hit, the light path vertex is visible

				const float cameraPdfW = scene->camera->GetPDF(eyeDir, filmX, filmY);
				const float fluxToRadianceFactor = cameraPdfW / (eyeDistance * eyeDistance);

				SampleResult &sampleResult = AddResult(sampleResults, true);
				sampleResult.filmX = filmX;
				sampleResult.filmY = filmY;

				// Add radiance from the light source
				sampleResult.radiance[light.GetID()] = connectionThroughput * flux * fluxToRadianceFactor * bsdfEval;
			}
		}
	}
}

void LightCPURenderThread::TraceEyePath(const float timeSample,
		Sampler *sampler, PathVolumeInfo volInfo, vector<SampleResult> &sampleResults) {
	LightCPURenderEngine *engine = (LightCPURenderEngine *)renderEngine;
	Scene *scene = engine->renderConfig->scene;
	Camera *camera = scene->camera;

	SampleResult &sampleResult = AddResult(sampleResults, false);

	sampleResult.filmX = sampler->GetSample(0);
	sampleResult.filmY = sampler->GetSample(1);
	Ray eyeRay;
	camera->GenerateRay(sampleResult.filmX, sampleResult.filmY, &eyeRay, &volInfo,
		sampler->GetSample(10), sampler->GetSample(11), timeSample);

	bool albedoToDo = true;
	Spectrum eyePathThroughput(1.f);
	int depth = 1;
	while (depth <= engine->maxPathDepth) {
		sampleResult.firstPathVertex = (depth == 1);
		sampleResult.lastPathVertex = (depth == engine->maxPathDepth);

		const u_int sampleOffset = sampleBootSize + (depth - 1) * sampleEyeStepSize;

		BSDF bsdf;	
		RayHit eyeRayHit;
		Spectrum connectionThroughput;
		const bool hit = scene->Intersect(device, false, sampleResult.firstPathVertex,
				&volInfo, sampler->GetSample(sampleOffset),
				&eyeRay, &eyeRayHit, &bsdf, &connectionThroughput,
				&eyePathThroughput, &sampleResult);

		if (!hit) {
			// Nothing was hit, check infinite lights (including sun)
			BOOST_FOREACH(EnvLightSource *envLight, scene->lightDefs.GetEnvLightSources()) {
				const Spectrum envRadiance = envLight->GetRadiance(*scene, -eyeRay.d);
				sampleResult.AddEmission(envLight->GetID(), eyePathThroughput * connectionThroughput,
						envRadiance);
			}

			if (sampleResult.firstPathVertex) {
				sampleResult.alpha = 0.f;
				sampleResult.depth = numeric_limits<float>::infinity();
				sampleResult.position = Point(
						numeric_limits<float>::infinity(),
						numeric_limits<float>::infinity(),
						numeric_limits<float>::infinity());
				sampleResult.geometryNormal = Normal();
				sampleResult.shadingNormal = Normal();
				sampleResult.materialID = numeric_limits<u_int>::max();
				sampleResult.objectID = numeric_limits<u_int>::max();
				sampleResult.uv = UV(numeric_limits<float>::infinity(),
						numeric_limits<float>::infinity());
			}
			break;
		}
		eyePathThroughput *= connectionThroughput;

		// Something was hit, check if it is a light source
			
		if (albedoToDo && bsdf.IsAlbedoEndPoint()) {
			sampleResult.albedo = eyePathThroughput * bsdf.Albedo();
			albedoToDo = false;
		}

		if (sampleResult.firstPathVertex) {
			sampleResult.alpha = 1.f;
			sampleResult.depth = eyeRayHit.t;
			sampleResult.position = bsdf.hitPoint.p;
			sampleResult.geometryNormal = bsdf.hitPoint.geometryN;
			sampleResult.shadingNormal = bsdf.hitPoint.shadeN;
			sampleResult.materialID = bsdf.GetMaterialID();
			sampleResult.objectID = bsdf.GetObjectID();
			sampleResult.uv = bsdf.hitPoint.uv;
		}

		if (bsdf.IsLightSource()) {
			sampleResult.AddEmission(bsdf.GetLightID(), eyePathThroughput,
					bsdf.GetEmittedRadiance());
			break;
		}

		// Check if it is a specular bounce

		float bsdfPdf;
		Vector sampledDir;
		BSDFEvent event;
		float cosSampleDir;
		const Spectrum bsdfSample = bsdf.Sample(&sampledDir,
				sampler->GetSample(sampleOffset + 1),
				sampler->GetSample(sampleOffset + 2),
				&bsdfPdf, &cosSampleDir, &event);
		if (bsdfSample.Black() || ((depth == 1) && !(event & SPECULAR)))
			break;

		// If depth = 1 and it is a specular bounce, I continue to trace the
		// eye path looking for a light source

		eyePathThroughput *= bsdfSample;
		assert (!eyePathThroughput.IsNaN() && !eyePathThroughput.IsInf());

		eyeRay.Update(bsdf.hitPoint.p, sampledDir);

		++depth;
	}
}

void LightCPURenderThread::RenderFunc() {
	//SLG_LOG("[LightCPURenderThread::" << threadIndex << "] Rendering thread started");

	//--------------------------------------------------------------------------
	// Initialization
	//--------------------------------------------------------------------------

	LightCPURenderEngine *engine = (LightCPURenderEngine *)renderEngine;
	// (engine->seedBase + 1) seed is used for sharedRndGen
	RandomGenerator *rndGen = new RandomGenerator(engine->seedBase + 1 + threadIndex);
	Scene *scene = engine->renderConfig->scene;
	Camera *camera = scene->camera;

	// Setup the sampler
	Sampler *sampler = engine->renderConfig->AllocSampler(rndGen, engine->film,
			engine->sampleSplatter, engine->samplerSharedData);
	const u_int sampleSize = 
		sampleBootSize + // To generate the initial setup
		engine->maxPathDepth * sampleEyeStepSize + // For each eye vertex
		engine->maxPathDepth * sampleLightStepSize; // For each light vertex
	sampler->RequestSamples(sampleSize);

	VarianceClamping varianceClamping(engine->sqrtVarianceClampMaxValue);

	//--------------------------------------------------------------------------
	// Trace light paths
	//--------------------------------------------------------------------------

	// I can not use engine->renderConfig->GetProperty() here because the
	// RenderConfig properties cache is not thread safe
	const u_int haltDebug = engine->renderConfig->cfg.Get(Property("batch.haltdebug")(0u)).Get<u_int>() *
		engine->film->GetWidth() * engine->film->GetHeight();
	
	vector<SampleResult> sampleResults;
	Spectrum lightPathFlux;
	for(u_int steps = 0; !boost::this_thread::interruption_requested(); ++steps) {
		// Check if we are in pause mode
		if (engine->pauseMode) {
			// Check every 100ms if I have to continue the rendering
			while (!boost::this_thread::interruption_requested() && engine->pauseMode)
				boost::this_thread::sleep(boost::posix_time::millisec(100));

			if (boost::this_thread::interruption_requested())
				break;
		}

		sampleResults.clear();
		lightPathFlux = Spectrum();

		const float timeSample = sampler->GetSample(12);
		const float time = scene->camera->GenerateRayTime(timeSample);

		// Select one light source
		float lightPickPdf;
		const LightSource *light = scene->lightDefs.GetEmitLightStrategy()->
				SampleLights(sampler->GetSample(2), &lightPickPdf);

		if (light) {
			// Initialize the light path
			float lightEmitPdfW;
			Ray nextEventRay;
			lightPathFlux = light->Emit(*scene,
				sampler->GetSample(3), sampler->GetSample(4), sampler->GetSample(5), sampler->GetSample(6), sampler->GetSample(7),
					&nextEventRay.o, &nextEventRay.d, &lightEmitPdfW);
			nextEventRay.UpdateMinMaxWithEpsilon();
			nextEventRay.time = time;

			if (lightPathFlux.Black()) {
				sampler->NextSample(sampleResults);
				continue;
			}
			lightPathFlux /= lightEmitPdfW * lightPickPdf;
			assert (!lightPathFlux.IsNaN() && !lightPathFlux.IsInf());

			// Sample a point on the camera lens
			Point lensPoint;
			if (!camera->SampleLens(time, sampler->GetSample(8), sampler->GetSample(9),
					&lensPoint)) {
				sampler->NextSample(sampleResults);
				continue;
			}

			//----------------------------------------------------------------------
			// I don't try to connect the light vertex directly with the eye
			// because InfiniteLight::Emit() returns a point on the scene bounding
			// sphere. Instead, I trace a ray from the camera like in BiDir.
			// This is also a good way to test the Film Per-Pixel-Normalization and
			// the Per-Screen-Normalization Buffers used by BiDir.
			//----------------------------------------------------------------------

			PathVolumeInfo eyeVolInfo;
			TraceEyePath(timeSample, sampler, eyeVolInfo, sampleResults);

			//----------------------------------------------------------------------
			// Trace the light path
			//----------------------------------------------------------------------

			int depth = 1;
			PathVolumeInfo volInfo;
			while (depth <= engine->maxPathDepth) {
				const u_int sampleOffset = sampleBootSize + sampleEyeStepSize * engine->maxPathDepth +
					(depth - 1) * sampleLightStepSize;

				RayHit nextEventRayHit;
				BSDF bsdf;
				Spectrum connectionThroughput;
				const bool hit = scene->Intersect(device, true, false, &volInfo, sampler->GetSample(sampleOffset),
						&nextEventRay, &nextEventRayHit, &bsdf,
						&connectionThroughput);

				if (hit) {
					// Something was hit

					lightPathFlux *= connectionThroughput;

					//--------------------------------------------------------------
					// Try to connect the light path vertex with the eye
					//--------------------------------------------------------------

					ConnectToEye(nextEventRay.time,
							sampler->GetSample(sampleOffset + 1), *light,
							bsdf, lensPoint, lightPathFlux, volInfo, sampleResults);

					if (depth >= engine->maxPathDepth)
						break;

					//--------------------------------------------------------------
					// Build the next vertex path ray
					//--------------------------------------------------------------

					float bsdfPdf;
					Vector sampledDir;
					BSDFEvent event;
					float cosSampleDir;
					const Spectrum bsdfSample = bsdf.Sample(&sampledDir,
							sampler->GetSample(sampleOffset + 2),
							sampler->GetSample(sampleOffset + 3),
							&bsdfPdf, &cosSampleDir, &event);
					if (bsdfSample.Black())
						break;

					if (depth >= engine->rrDepth) {
						// Russian Roulette
						const float prob = RenderEngine::RussianRouletteProb(bsdfSample, engine->rrImportanceCap);
						if (sampler->GetSample(sampleOffset + 4) < prob)
							bsdfPdf *= prob;
						else
							break;
					}

					lightPathFlux *= bsdfSample;
					assert (!lightPathFlux.IsNaN() && !lightPathFlux.IsInf());

					// Update volume information
					volInfo.Update(event, bsdf);

					nextEventRay.Update(bsdf.hitPoint.p, sampledDir);
					++depth;
				} else {
					// Ray lost in space...
					break;
				}
			}
		}

		// Variance clamping
		if (varianceClamping.hasClamping())
			for(u_int i = 0; i < sampleResults.size(); ++i)
				varianceClamping.Clamp(*(engine->film), sampleResults[i]);

		sampler->NextSample(sampleResults);

#ifdef WIN32
		// Work around Windows bad scheduling
		renderThread->yield();
#endif

		// Check halt conditions
		if ((haltDebug > 0u) && (steps >= haltDebug))
			break;
		if (engine->film->GetConvergence() == 1.f)
			break;
	}

	delete sampler;
	delete rndGen;

	threadDone = true;

	//SLG_LOG("[LightCPURenderThread::" << threadIndex << "] Rendering thread halted");
}
