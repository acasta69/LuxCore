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

#ifndef _SLG_FILM_H
#define	_SLG_FILM_H

#include <cstddef>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <vector>
#include <set>

#include <boost/thread/mutex.hpp>
#include <bcd/core/SamplesAccumulator.h>

#include "luxrays/core/geometry/point.h"
#include "luxrays/core/geometry/normal.h"
#include "luxrays/core/geometry/uv.h"
#include "luxrays/core/oclintersectiondevice.h"
#include "luxrays/utils/oclcache.h"
#include "luxrays/utils/properties.h"
#include "luxrays/utils/serializationutils.h"
#include "slg/slg.h"
#include "slg/bsdf/bsdf.h"
#include "slg/film/imagepipeline/imagepipeline.h"
#include "slg/film/framebuffer.h"
#include "slg/film/filmoutputs.h"
#include "slg/film/convtest/filmconvtest.h"
#include "slg/film/denoiser/filmdenoiser.h"
#include "slg/utils/varianceclamping.h"
#include "denoiser/filmdenoiser.h"

namespace slg {

//------------------------------------------------------------------------------
// Film
//------------------------------------------------------------------------------

class SampleResult;

class Film {
public:
	typedef enum {
		NONE = 0,
		RADIANCE_PER_PIXEL_NORMALIZED = 1 << 0,
		RADIANCE_PER_SCREEN_NORMALIZED = 1 << 1,
		ALPHA = 1 << 2,
		// RGB_TONEMAPPED is deprecated and replaced by IMAGEPIPELINE
		IMAGEPIPELINE = 1 << 3,
		DEPTH = 1 << 4,
		POSITION = 1 << 5,
		GEOMETRY_NORMAL = 1 << 6,
		SHADING_NORMAL = 1 << 7,
		MATERIAL_ID = 1 << 8,
		DIRECT_DIFFUSE = 1 << 9,
		DIRECT_GLOSSY = 1 << 10,
		EMISSION = 1 << 11,
		INDIRECT_DIFFUSE = 1 << 12,
		INDIRECT_GLOSSY = 1 << 13,
		INDIRECT_SPECULAR = 1 << 14,
		MATERIAL_ID_MASK = 1 << 15,
		DIRECT_SHADOW_MASK = 1 << 16,
		INDIRECT_SHADOW_MASK = 1 << 17,
		UV = 1 << 18,
		RAYCOUNT = 1 << 19,
		BY_MATERIAL_ID = 1 << 20,
		IRRADIANCE = 1 << 21,
		OBJECT_ID = 1 << 22,
		OBJECT_ID_MASK = 1 << 23,
		BY_OBJECT_ID = 1 << 24,
		SAMPLECOUNT = 1 << 25,
		CONVERGENCE = 1 << 26,
		MATERIAL_ID_COLOR = 1 << 27,
		ALBEDO = 1 << 28,
		AVG_SHADING_NORMAL = 1 << 29
	} FilmChannelType;

	Film(const u_int width, const u_int height, const u_int *subRegion = NULL);
	~Film();

	void Init();
	bool IsInitiliazed() const { return initialized; }
	void Resize(const u_int w, const u_int h);
	void Reset();
	void Clear();
	void Parse(const luxrays::Properties &props);

	//--------------------------------------------------------------------------
	// Dynamic settings
	//--------------------------------------------------------------------------

	void SetImagePipelines(const u_int index, ImagePipeline *newImagePiepeline);
	void SetImagePipelines(ImagePipeline *newImagePiepeline);
	void SetImagePipelines(std::vector<ImagePipeline *> &newImagePiepelines);
	const u_int GetImagePipelineCount() const { return imagePipelines.size(); }
	const ImagePipeline *GetImagePipeline(const u_int index) const { return imagePipelines[index]; }

	void CopyDynamicSettings(const Film &film);

	//--------------------------------------------------------------------------

	float GetFilmY(const u_int imagePipeLineIndex) const;
	float GetFilmMaxValue(const u_int imagePipeLineIndex) const;

	void VarianceClampFilm(const VarianceClamping &varianceClamping, const Film &film,
		const u_int srcOffsetX, const u_int srcOffsetY,
		const u_int srcWidth, const u_int srcHeight,
		const u_int dstOffsetX, const u_int dstOffsetY);
	void VarianceClampFilm(const VarianceClamping &varianceClamping, const Film &film) {
		VarianceClampFilm(varianceClamping, film, 0, 0, width, height, 0, 0);
	}

	void AddFilm(const Film &film,
		const u_int srcOffsetX, const u_int srcOffsetY,
		const u_int srcWidth, const u_int srcHeight,
		const u_int dstOffsetX, const u_int dstOffsetY);
	void AddFilm(const Film &film) {
		AddFilm(film, 0, 0, width, height, 0, 0);
	}

	//--------------------------------------------------------------------------
	// Channels
	//--------------------------------------------------------------------------

	bool HasChannel(const FilmChannelType type) const { return channels.count(type) > 0; }
	u_int GetChannelCount(const FilmChannelType type) const;

	// This one must be called before Init()
	void AddChannel(const FilmChannelType type,
		const luxrays::Properties *prop = NULL);
	// This one must be called before Init()
	void RemoveChannel(const FilmChannelType type);
	// This one must be called before Init()
	void SetRadianceGroupCount(const u_int count) { radianceGroupCount = count; }

	u_int GetRadianceGroupCount() const { return radianceGroupCount; }
	u_int GetMaskMaterialID(const u_int index) const { return maskMaterialIDs[index]; }
	u_int GetByMaterialID(const u_int index) const { return byMaterialIDs[index]; }
	u_int GetMaskObjectID(const u_int index) const { return maskObjectIDs[index]; }
	u_int GetByObjectID(const u_int index) const { return byObjectIDs[index]; }

	template<class T> const T *GetChannel(const FilmChannelType type, const u_int index = 0,
			const bool executeImagePipeline = true) {
		throw std::runtime_error("Called Film::GetChannel() with wrong type");
	}

	bool HasDataChannel() { return hasDataChannel; }
	bool HasComposingChannel() { return hasComposingChannel; }

	void AsyncExecuteImagePipeline(const u_int index);
	void WaitAsyncExecuteImagePipeline();
	bool HasDoneAsyncExecuteImagePipeline();
	void ExecuteImagePipeline(const u_int index);

	//--------------------------------------------------------------------------
	// Outputs
	//--------------------------------------------------------------------------

	bool HasOutput(const FilmOutputs::FilmOutputType type) const;
	u_int GetOutputCount(const FilmOutputs::FilmOutputType type) const;
	size_t GetOutputSize(const FilmOutputs::FilmOutputType type) const;

	void Output();
	void Output(const std::string &fileName, const FilmOutputs::FilmOutputType type,
			const luxrays::Properties *props = NULL, const bool executeImagePipeline = true);

	template<class T> void GetOutput(const FilmOutputs::FilmOutputType type, T *buffer,
			const u_int index = 0, const bool executeImagePipeline = true) {
		throw std::runtime_error("Called Film::GetOutput() with wrong type");
	}

	//--------------------------------------------------------------------------

	u_int GetWidth() const { return width; }
	u_int GetHeight() const { return height; }
	u_int GetPixelCount() const { return pixelCount; }
	const u_int *GetSubRegion() const { return subRegion; }
	double GetTotalSampleCount() const {
		return statsTotalSampleCount;
	}
	double GetTotalTime() const {
		return luxrays::WallClockTime() - statsStartSampleTime;
	}
	double GetAvgSampleSec() {
		const double t = GetTotalTime();
		return (t > 0.0) ? (GetTotalSampleCount() / t) : 0.0;
	}

	//--------------------------------------------------------------------------
	// Halt tests related methods
	//--------------------------------------------------------------------------

	void ResetHaltTests();
	void RunHaltTests();
	// Convergence can be set by external source (like TileRepository convergence test)
	void SetConvergence(const float conv) { statsConvergence = conv; }
	float GetConvergence() { return statsConvergence; }

	//--------------------------------------------------------------------------
	// Used by BCD denoiser plugin
	//--------------------------------------------------------------------------

	const FilmDenoiser &GetDenoiser() const { return filmDenoiser; }
	FilmDenoiser &GetDenoiser() { return filmDenoiser; }

	//--------------------------------------------------------------------------
	// Samples related methods
	//--------------------------------------------------------------------------

	void SetSampleCount(const double count);
	void AddSampleCount(const double count) {
		statsTotalSampleCount += count;
	}

	// Normal method versions
	void AddSample(const u_int x, const u_int y,
		const SampleResult &sampleResult, const float weight = 1.f);
	void AddSampleResultColor(const u_int x, const u_int y,
		const SampleResult &sampleResult, const float weight);
	void AddSampleResultData(const u_int x, const u_int y,
		const SampleResult &sampleResult);
	
	// Atomic method versions
	void AtomicAddSample(const u_int x, const u_int y,
		const SampleResult &sampleResult, const float weight = 1.f);
	void AtomicAddSampleResultColor(const u_int x, const u_int y,
		const SampleResult &sampleResult, const float weight);
	void AtomicAddSampleResultData(const u_int x, const u_int y,
		const SampleResult &sampleResult);
	
#if !defined(LUXRAYS_DISABLE_OPENCL)
	void ReadOCLBuffer_IMAGEPIPELINE(const u_int index);
	void WriteOCLBuffer_IMAGEPIPELINE(const u_int index);
#endif

	void GetPixelFromMergedSampleBuffers(const FilmChannelType channels,
		const std::vector<RadianceChannelScale> *radianceChannelScales,
		const u_int index, float *c) const;
	void GetPixelFromMergedSampleBuffers(const FilmChannelType channels,
		const std::vector<RadianceChannelScale> *radianceChannelScales,
		const u_int x, const u_int y, float *c) const {
		GetPixelFromMergedSampleBuffers(channels, radianceChannelScales, x + y * width, c);
	}
	void GetPixelFromMergedSampleBuffers(const u_int imagePipelineIndex, const u_int x, const u_int y, float *c) const {
		const ImagePipeline *ip = (imagePipelineIndex < imagePipelines.size()) ? imagePipelines[imagePipelineIndex] : NULL;
		const std::vector<RadianceChannelScale> *radianceChannelScales = ip ? &ip->radianceChannelScales : NULL;

		GetPixelFromMergedSampleBuffers((FilmChannelType)(RADIANCE_PER_PIXEL_NORMALIZED | RADIANCE_PER_SCREEN_NORMALIZED),
				radianceChannelScales, x, y, c);
	}
	void GetPixelFromMergedSampleBuffers(const u_int imagePipelineIndex, const u_int index, float *c) const {
		const ImagePipeline *ip = (imagePipelineIndex < imagePipelines.size()) ? imagePipelines[imagePipelineIndex] : NULL;
		const std::vector<RadianceChannelScale> *radianceChannelScales = ip ? &ip->radianceChannelScales : NULL;

		GetPixelFromMergedSampleBuffers((FilmChannelType)(RADIANCE_PER_PIXEL_NORMALIZED | RADIANCE_PER_SCREEN_NORMALIZED),
				radianceChannelScales, index, c);
	}
	
	bool HasSamples(const bool has_RADIANCE_PER_PIXEL_NORMALIZEDs, const bool has_RADIANCE_PER_SCREEN_NORMALIZEDs,
			const u_int index) const {
		for (u_int i = 0; i < radianceGroupCount; ++i) {
			if (has_RADIANCE_PER_PIXEL_NORMALIZEDs && channel_RADIANCE_PER_PIXEL_NORMALIZEDs[i]->GetPixel(index)[3] > 0.f)
				return true;

			if (has_RADIANCE_PER_SCREEN_NORMALIZEDs) {
				luxrays::Spectrum s(channel_RADIANCE_PER_SCREEN_NORMALIZEDs[i]->GetPixel(index));

				if (!s.Black())
					return true;
			}
		}

		return false;
	}
		
	bool HasSamples(const bool has_RADIANCE_PER_PIXEL_NORMALIZEDs, const bool has_RADIANCE_PER_SCREEN_NORMALIZEDs,
		const u_int x, const u_int y) const {
		return HasSamples(has_RADIANCE_PER_PIXEL_NORMALIZEDs, has_RADIANCE_PER_SCREEN_NORMALIZEDs, x + y * width);
	}

	std::vector<GenericFrameBuffer<4, 1, float> *> channel_RADIANCE_PER_PIXEL_NORMALIZEDs;
	std::vector<GenericFrameBuffer<3, 0, float> *> channel_RADIANCE_PER_SCREEN_NORMALIZEDs;
	GenericFrameBuffer<2, 1, float> *channel_ALPHA;
	std::vector<GenericFrameBuffer<3, 0, float> *> channel_IMAGEPIPELINEs;
	GenericFrameBuffer<1, 0, float> *channel_DEPTH;
	GenericFrameBuffer<3, 0, float> *channel_POSITION;
	GenericFrameBuffer<3, 0, float> *channel_GEOMETRY_NORMAL;
	GenericFrameBuffer<3, 0, float> *channel_SHADING_NORMAL;
	GenericFrameBuffer<4, 1, float> *channel_AVG_SHADING_NORMAL;
	GenericFrameBuffer<1, 0, u_int> *channel_MATERIAL_ID;
	GenericFrameBuffer<4, 1, float> *channel_DIRECT_DIFFUSE;
	GenericFrameBuffer<4, 1, float> *channel_DIRECT_GLOSSY;
	GenericFrameBuffer<4, 1, float> *channel_EMISSION;
	GenericFrameBuffer<4, 1, float> *channel_INDIRECT_DIFFUSE;
	GenericFrameBuffer<4, 1, float> *channel_INDIRECT_GLOSSY;
	GenericFrameBuffer<4, 1, float> *channel_INDIRECT_SPECULAR;
	std::vector<GenericFrameBuffer<2, 1, float> *> channel_MATERIAL_ID_MASKs;
	GenericFrameBuffer<2, 1, float> *channel_DIRECT_SHADOW_MASK;
	GenericFrameBuffer<2, 1, float> *channel_INDIRECT_SHADOW_MASK;
	GenericFrameBuffer<2, 0, float> *channel_UV;
	GenericFrameBuffer<1, 0, float> *channel_RAYCOUNT;
	std::vector<GenericFrameBuffer<4, 1, float> *> channel_BY_MATERIAL_IDs;
	GenericFrameBuffer<4, 1, float> *channel_IRRADIANCE;
	GenericFrameBuffer<1, 0, u_int> *channel_OBJECT_ID;
	std::vector<GenericFrameBuffer<2, 1, float> *> channel_OBJECT_ID_MASKs;
	std::vector<GenericFrameBuffer<4, 1, float> *> channel_BY_OBJECT_IDs;
	GenericFrameBuffer<1, 0, u_int> *channel_SAMPLECOUNT;
	GenericFrameBuffer<1, 0, float> *channel_CONVERGENCE;
	GenericFrameBuffer<4, 1, float> *channel_MATERIAL_ID_COLOR;
	GenericFrameBuffer<4, 1, float> *channel_ALBEDO;

	// (Optional) OpenCL context
	bool oclEnable;
	int oclPlatformIndex;
	int oclDeviceIndex;

#if !defined(LUXRAYS_DISABLE_OPENCL)
	luxrays::Context *ctx;
	luxrays::DataSet *dataSet;
	luxrays::OpenCLDeviceDescription *selectedDeviceDesc;
	luxrays::OpenCLIntersectionDevice *oclIntersectionDevice;

	luxrays::oclKernelCache *kernelCache;

	cl::Buffer *ocl_IMAGEPIPELINE;
	cl::Buffer *ocl_ALPHA;
	cl::Buffer *ocl_OBJECT_ID;
	
	cl::Buffer *ocl_mergeBuffer;
	
	cl::Kernel *mergeInitializeKernel;
	cl::Kernel *mergeRADIANCE_PER_PIXEL_NORMALIZEDKernel;
	cl::Kernel *mergeRADIANCE_PER_SCREEN_NORMALIZEDKernel;
	cl::Kernel *mergeFinalizeKernel;
#endif

	static Film *LoadSerialized(const std::string &fileName);
	static void SaveSerialized(const std::string &fileName, const Film *film);

	static bool GetFilmSize(const luxrays::Properties &cfg,
		u_int *filmFullWidth, u_int *filmFullHeight,
		u_int *filmSubRegion);

	static luxrays::Properties ToProperties(const luxrays::Properties &cfg);
	static Film *FromProperties(const luxrays::Properties &cfg);

	static FilmChannelType String2FilmChannelType(const std::string &type);
	static const std::string FilmChannelType2String(const FilmChannelType type);

	friend class FilmDenoiser;
	friend class boost::serialization::access;

private:
	// Used by serialization
	Film();

	template<class Archive> void save(Archive &ar, const unsigned int version) const;
	template<class Archive>	void load(Archive &ar, const unsigned int version);
	BOOST_SERIALIZATION_SPLIT_MEMBER()

	void FreeChannels();
	void MergeSampleBuffers(const u_int imagePipelineIndex);

	void ParseRadianceGroupsScale(const luxrays::Properties &props, const u_int imagePipelineIndex,
			const std::string &radianceGroupsScalePrefix);
	void ParseRadianceGroupsScales(const luxrays::Properties &props);
	void ParseOutputs(const luxrays::Properties &props);
	ImagePipeline *CreateImagePipeline(const luxrays::Properties &props,
			const std::string &imagePipelinePrefix);
	void ParseImagePipelines(const luxrays::Properties &props);

	void SetUpOCL();
#if !defined(LUXRAYS_DISABLE_OPENCL)
	void CreateOCLContext();
	void DeleteOCLContext();
	void AllocateOCLBuffers();
	void CompileOCLKernels();
	void WriteAllOCLBuffers();
	void MergeSampleBuffersOCL(const u_int imagePipelineIndex);
#endif

	void ExecuteImagePipelineThreadImpl(const u_int index);
	void ExecuteImagePipelineImpl(const u_int index);

	std::set<FilmChannelType> channels;
	u_int width, height, pixelCount, radianceGroupCount;
	u_int subRegion[4];
	std::vector<u_int> maskMaterialIDs, byMaterialIDs;
	std::vector<u_int> maskObjectIDs, byObjectIDs;

	// Used to speedup sample splatting, initialized inside Init()
	bool hasDataChannel, hasComposingChannel;

	double statsTotalSampleCount, statsStartSampleTime, statsConvergence;

	std::vector<ImagePipeline *> imagePipelines;
	boost::thread *imagePipelineThread;
	bool isAsyncImagePipelineRunning;

	// Halt conditions
	FilmConvTest *convTest;
	double haltTime;
	u_int haltSPP;
	
	float haltThreshold;
	u_int haltThresholdWarmUp, haltThresholdTestStep;
	bool haltThresholdUseFilter, haltThresholdStopRendering;

	FilmOutputs filmOutputs;

	FilmDenoiser filmDenoiser;
	
	bool initialized;
};

template<> const float *Film::GetChannel<float>(const FilmChannelType type, const u_int index, const bool executeImagePipeline);
template<> const u_int *Film::GetChannel<u_int>(const FilmChannelType type, const u_int index, const bool executeImagePipeline);
template<> void Film::GetOutput<float>(const FilmOutputs::FilmOutputType type, float *buffer, const u_int index, const bool executeImagePipeline);
template<> void Film::GetOutput<u_int>(const FilmOutputs::FilmOutputType type, u_int *buffer, const u_int index, const bool executeImagePipeline);

}

BOOST_CLASS_VERSION(slg::Film, 20)

BOOST_CLASS_EXPORT_KEY(slg::Film)

#endif	/* _SLG_FILM_H */
