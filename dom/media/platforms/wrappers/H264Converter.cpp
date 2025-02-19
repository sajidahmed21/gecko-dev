/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/TaskQueue.h"

#include "H264Converter.h"
#include "ImageContainer.h"
#include "MediaInfo.h"
#include "MediaPrefs.h"
#include "mp4_demuxer/AnnexB.h"
#include "mp4_demuxer/H264.h"

namespace mozilla
{

H264Converter::H264Converter(PlatformDecoderModule* aPDM,
                             const CreateDecoderParams& aParams)
  : mPDM(aPDM)
  , mOriginalConfig(aParams.VideoConfig())
  , mCurrentConfig(aParams.VideoConfig())
  , mKnowsCompositor(aParams.mKnowsCompositor)
  , mImageContainer(aParams.mImageContainer)
  , mTaskQueue(aParams.mTaskQueue)
  , mDecoder(nullptr)
  , mGMPCrashHelper(aParams.mCrashHelper)
  , mLastError(NS_OK)
  , mType(aParams.mType)
  , mOnWaitingForKeyEvent(aParams.mOnWaitingForKeyEvent)
{
  CreateDecoder(aParams.mDiagnostics);
}

H264Converter::~H264Converter()
{
}

RefPtr<MediaDataDecoder::InitPromise>
H264Converter::Init()
{
  if (mDecoder) {
    return mDecoder->Init();
  }

  // We haven't been able to initialize a decoder due to a missing SPS/PPS.
  return MediaDataDecoder::InitPromise::CreateAndResolve(
           TrackType::kVideoTrack, __func__);
}

RefPtr<MediaDataDecoder::DecodePromise>
H264Converter::Decode(MediaRawData* aSample)
{
  MOZ_RELEASE_ASSERT(!mDecodePromiseRequest.Exists()
                     && !mInitPromiseRequest.Exists(),
                     "Can't request a new decode until previous one completed");

  if (!mp4_demuxer::AnnexB::ConvertSampleToAVCC(aSample)) {
    // We need AVCC content to be able to later parse the SPS.
    // This is a no-op if the data is already AVCC.
    return DecodePromise::CreateAndReject(
      MediaResult(NS_ERROR_OUT_OF_MEMORY, RESULT_DETAIL("ConvertSampleToAVCC")),
      __func__);
  }

  nsresult rv;
  if (!mDecoder) {
    // It is not possible to create an AVCC H264 decoder without SPS.
    // As such, creation will fail if the extra_data just extracted doesn't
    // contain a SPS.
    rv = CreateDecoderAndInit(aSample);
    if (rv == NS_ERROR_NOT_INITIALIZED) {
      // We are missing the required SPS to create the decoder.
      // Ignore for the time being, the MediaRawData will be dropped.
      return DecodePromise::CreateAndResolve(DecodedData(), __func__);
    }
  } else {
    rv = CheckForSPSChange(aSample);
  }

  if (rv == NS_ERROR_DOM_MEDIA_INITIALIZING_DECODER) {
    // The decoder is pending initialization.
    RefPtr<DecodePromise> p = mDecodePromise.Ensure(__func__);
    return p;
  }

  if (NS_FAILED(rv)) {
    return DecodePromise::CreateAndReject(
      MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                  RESULT_DETAIL("Unable to create H264 decoder")),
      __func__);
  }

  if (mNeedKeyframe && !aSample->mKeyframe) {
    return DecodePromise::CreateAndResolve(DecodedData(), __func__);
  }

  if (!mNeedAVCC) {
    mNeedAVCC =
      Some(mDecoder->NeedsConversion() == ConversionRequired::kNeedAVCC);
  }

  if (!*mNeedAVCC
      && !mp4_demuxer::AnnexB::ConvertSampleToAnnexB(aSample, mNeedKeyframe)) {
    return DecodePromise::CreateAndReject(
      MediaResult(NS_ERROR_OUT_OF_MEMORY,
                  RESULT_DETAIL("ConvertSampleToAnnexB")),
      __func__);
  }

  mNeedKeyframe = false;

  aSample->mExtraData = mCurrentConfig.mExtraData;

  return mDecoder->Decode(aSample);
}

RefPtr<MediaDataDecoder::FlushPromise>
H264Converter::Flush()
{
  mNeedKeyframe = true;
  if (mDecoder) {
    return mDecoder->Flush();
  }
  return FlushPromise::CreateAndResolve(true, __func__);
}

RefPtr<MediaDataDecoder::DecodePromise>
H264Converter::Drain()
{
  mNeedKeyframe = true;
  if (mDecoder) {
    return mDecoder->Drain();
  }
  return DecodePromise::CreateAndResolve(DecodedData(), __func__);
}

RefPtr<ShutdownPromise>
H264Converter::Shutdown()
{
  mInitPromiseRequest.DisconnectIfExists();
  mDecodePromiseRequest.DisconnectIfExists();
  mFlushRequest.DisconnectIfExists();
  mShutdownRequest.DisconnectIfExists();
  mPendingSample = nullptr;
  if (mShutdownPromise) {
    // We have a shutdown in progress, return that promise instead as we can't
    // shutdown a decoder twice.
    return mShutdownPromise.forget();
  }
  if (mDecoder) {
    RefPtr<MediaDataDecoder> decoder = mDecoder.forget();
    return decoder->Shutdown();
  }
  return ShutdownPromise::CreateAndResolve(true, __func__);
}

bool
H264Converter::IsHardwareAccelerated(nsACString& aFailureReason) const
{
  if (mDecoder) {
    return mDecoder->IsHardwareAccelerated(aFailureReason);
  }
  return MediaDataDecoder::IsHardwareAccelerated(aFailureReason);
}

void
H264Converter::SetSeekThreshold(const media::TimeUnit& aTime)
{
  if (mDecoder) {
    mDecoder->SetSeekThreshold(aTime);
  } else {
    MediaDataDecoder::SetSeekThreshold(aTime);
  }
}

nsresult
H264Converter::CreateDecoder(DecoderDoctorDiagnostics* aDiagnostics)
{
  if (!mp4_demuxer::AnnexB::HasSPS(mCurrentConfig.mExtraData)) {
    // nothing found yet, will try again later
    return NS_ERROR_NOT_INITIALIZED;
  }
  UpdateConfigFromExtraData(mCurrentConfig.mExtraData);

  mp4_demuxer::SPSData spsdata;
  if (mp4_demuxer::H264::DecodeSPSFromExtraData(mCurrentConfig.mExtraData, spsdata)) {
    // Do some format check here.
    // WMF H.264 Video Decoder and Apple ATDecoder do not support YUV444 format.
    if (spsdata.profile_idc == 244 /* Hi444PP */
        || spsdata.chroma_format_idc == PDMFactory::kYUV444) {
      mLastError = NS_ERROR_FAILURE;
      if (aDiagnostics) {
        aDiagnostics->SetVideoNotSupported();
      }
      return NS_ERROR_FAILURE;
    }
  } else {
    // SPS was invalid.
    mLastError = NS_ERROR_FAILURE;
    return NS_ERROR_FAILURE;
  }

  mDecoder = mPDM->CreateVideoDecoder({
    mUseOriginalConfig ? mOriginalConfig : mCurrentConfig,
    mTaskQueue,
    aDiagnostics,
    mImageContainer,
    mKnowsCompositor,
    mGMPCrashHelper,
    mType,
    mOnWaitingForKeyEvent
  });

  if (!mDecoder) {
    mLastError = NS_ERROR_FAILURE;
    return NS_ERROR_FAILURE;
  }

  mUseOriginalConfig = false;
  mNeedKeyframe = true;

  return NS_OK;
}

nsresult
H264Converter::CreateDecoderAndInit(MediaRawData* aSample)
{
  RefPtr<MediaByteBuffer> extra_data =
    mp4_demuxer::AnnexB::ExtractExtraData(aSample);
  if (!mp4_demuxer::AnnexB::HasSPS(extra_data)) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  UpdateConfigFromExtraData(extra_data);

  nsresult rv = CreateDecoder(/* DecoderDoctorDiagnostics* */ nullptr);

  if (NS_SUCCEEDED(rv)) {
    // Queue the incoming sample.
    mPendingSample = aSample;

    mDecoder->Init()
      ->Then(AbstractThread::GetCurrent()->AsTaskQueue(), __func__, this,
             &H264Converter::OnDecoderInitDone,
             &H264Converter::OnDecoderInitFailed)
      ->Track(mInitPromiseRequest);
    return NS_ERROR_DOM_MEDIA_INITIALIZING_DECODER;
  }
  return rv;
}

void
H264Converter::OnDecoderInitDone(const TrackType aTrackType)
{
  mInitPromiseRequest.Complete();
  RefPtr<MediaRawData> sample = mPendingSample.forget();
  DecodeFirstSample(sample);
}

void
H264Converter::OnDecoderInitFailed(const MediaResult& aError)
{
  mInitPromiseRequest.Complete();
  mDecodePromise.Reject(
    MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                RESULT_DETAIL("Unable to initialize H264 decoder")),
    __func__);
}

void
H264Converter::DecodeFirstSample(MediaRawData* aSample)
{
  if (mNeedKeyframe && !aSample->mKeyframe) {
    mDecodePromise.Resolve(DecodedData(), __func__);
    return;
  }
  mNeedKeyframe = false;

  mNeedAVCC =
    Some(mDecoder->NeedsConversion() == ConversionRequired::kNeedAVCC);

  if (!*mNeedAVCC
      && !mp4_demuxer::AnnexB::ConvertSampleToAnnexB(aSample, mNeedKeyframe)) {
    mDecodePromise.Reject(
      MediaResult(NS_ERROR_OUT_OF_MEMORY,
                  RESULT_DETAIL("ConvertSampleToAnnexB")),
      __func__);
    return;
  }
  if (CanRecycleDecoder()) {
    mDecoder->ConfigurationChanged(mCurrentConfig);
  }
  RefPtr<H264Converter> self = this;
  mDecoder->Decode(aSample)
    ->Then(AbstractThread::GetCurrent()->AsTaskQueue(), __func__,
           [self, this](const MediaDataDecoder::DecodedData& aResults) {
             mDecodePromiseRequest.Complete();
             mDecodePromise.Resolve(aResults, __func__);
           },
           [self, this](const MediaResult& aError) {
             mDecodePromiseRequest.Complete();
             mDecodePromise.Reject(aError, __func__);
           })
    ->Track(mDecodePromiseRequest);
}

nsresult
H264Converter::CheckForSPSChange(MediaRawData* aSample)
{
  RefPtr<MediaByteBuffer> extra_data =
    mp4_demuxer::AnnexB::ExtractExtraData(aSample);
  if (!mp4_demuxer::AnnexB::HasSPS(extra_data)
      || mp4_demuxer::AnnexB::CompareExtraData(extra_data,
                                               mCurrentConfig.mExtraData)) {
        return NS_OK;
      }

  mPendingSample = aSample;

  if (CanRecycleDecoder()) {
    // Do not recreate the decoder, reuse it.
    UpdateConfigFromExtraData(extra_data);
    // Ideally we would want to drain the decoder instead of flushing it.
    // However the draining operation requires calling Drain and looping several
    // times which isn't possible from within the H264Converter. So instead we
    // flush the decoder. In practice, this is a no-op as SPS change will only
    // be used with MSE. And with MSE, the MediaFormatReader would have drained
    // the decoder already.
    RefPtr<H264Converter> self = this;
    mDecoder->Flush()
      ->Then(AbstractThread::GetCurrent()->AsTaskQueue(),
             __func__,
             [self, this]() {
               mFlushRequest.Complete();
               DecodeFirstSample(mPendingSample);
               mPendingSample = nullptr;
             },
             [self, this](const MediaResult& aError) {
               mFlushRequest.Complete();
               mDecodePromise.Reject(aError, __func__);
             })
      ->Track(mFlushRequest);
    mNeedKeyframe = true;
    // This is not really initializing the decoder, but it will do as it
    // indicates an operation is pending.
    return NS_ERROR_DOM_MEDIA_INITIALIZING_DECODER;
  }

  // The SPS has changed, signal to flush the current decoder and create a
  // new one.
  RefPtr<H264Converter> self = this;
  mDecoder->Flush()
    ->Then(AbstractThread::GetCurrent()->AsTaskQueue(),
           __func__,
           [self, this]() {
             mFlushRequest.Complete();
             mShutdownPromise = Shutdown();
             mShutdownPromise
               ->Then(AbstractThread::GetCurrent()->AsTaskQueue(),
                      __func__,
                      [self, this]() {
                        mShutdownRequest.Complete();
                        mShutdownPromise = nullptr;
                        mNeedAVCC.reset();
                        RefPtr<MediaRawData> sample = mPendingSample.forget();
                        nsresult rv = CreateDecoderAndInit(sample);
                        if (rv == NS_ERROR_DOM_MEDIA_INITIALIZING_DECODER) {
                          // All good so far, will continue later.
                          return;
                        }
                        MOZ_ASSERT(NS_FAILED(rv));
                        mDecodePromise.Reject(rv, __func__);
                        return;
                      },
                      [] { MOZ_CRASH("Can't reach here'"); })
               ->Track(mShutdownRequest);
           },
           [self, this](const MediaResult& aError) {
             mFlushRequest.Complete();
             mDecodePromise.Reject(aError, __func__);
           })
    ->Track(mFlushRequest);
  return NS_ERROR_DOM_MEDIA_INITIALIZING_DECODER;
}

void
H264Converter::UpdateConfigFromExtraData(MediaByteBuffer* aExtraData)
{
  mp4_demuxer::SPSData spsdata;
  if (mp4_demuxer::H264::DecodeSPSFromExtraData(aExtraData, spsdata)
      && spsdata.pic_width > 0
      && spsdata.pic_height > 0) {
    mp4_demuxer::H264::EnsureSPSIsSane(spsdata);
    mCurrentConfig.mImage.width = spsdata.pic_width;
    mCurrentConfig.mImage.height = spsdata.pic_height;
    mCurrentConfig.mDisplay.width = spsdata.display_width;
    mCurrentConfig.mDisplay.height = spsdata.display_height;
  }
  mCurrentConfig.mExtraData = aExtraData;
}

} // namespace mozilla
