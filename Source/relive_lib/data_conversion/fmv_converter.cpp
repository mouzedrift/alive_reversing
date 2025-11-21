#include "fmv_converter.hpp"
#include "../../AliveLibAE/PathData.hpp"
#include "../FatalError.hpp"

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable : 4505)
#endif
#include "aom/aom_encoder.h"
#include "aom/aomcx.h"
#ifdef _MSC_VER
    #pragma warning(pop)
#endif

#include "aom/third_party/libwebm/mkvmuxer/mkvmuxer.h"
#include "aom/third_party/libwebm/mkvmuxer/mkvmuxerutil.h"
#include "aom/third_party/libwebm/mkvmuxer/mkvwriter.h"

#include "aom/common/av1_config.h"

#include "../Masher.hpp"
#include "rgb_conversion.hpp"

// TODO: An interface around masher + mdec reading
class IDDVReader
{
public:
    IDDVReader()
    {

    }

    // TODO: Remove
    ~IDDVReader()
    {

    }

    // TODO: Remove
    virtual bool Open(const char_type* )
    {
        return false;
    }

    s8 Mash_DecompressAudio()
    {
        if (!mHasAudio)
        {
            return 1;
        }

        u32 audioBufferStartOffset = 0;
        mAudioSampleOffset = 0;

        // Keep reading frames till we have >= number of interleaved so that we have 1 full frame
        if (mNumReadFrames < mMasher->field_2C_audio_header.field_10_num_frames_interleave)
        {
            while (Masher::ReadNextFrameToMemory_4EAC30(mMasher))
            {
                //const int bitsPerSample = (mMasher->field_2C_audio_header.field_0_audio_format & 2) ? 16 : 8;
                //const int channels = (mMasher->field_2C_audio_header.field_0_audio_format & 1) ? 2 : 1;

                void* pDecompressedAudioFrame = Masher::GetDecompressedAudioFrame_4EAC60(mMasher);

                if (pDecompressedAudioFrame)
                {
                    //ffmpeg_push_audio(reinterpret_cast<u8*>(pDecompressedAudioFrame), (bitsPerSample / 8) * mSingleAudioFrameSizeInSamples * channels);

                    /*if (GetSoundAPI().SND_LoadSamples(
                        &g_fmv_sound_entry_5CA208,
                        mAudioSampleOffset,
                        (u8*)pDecompressedAudioBuffer,
                        mSingleAudioFrameSizeInSamples))
                    {
                        mNoAudioOrAudioError = 1;
                    }*/
                }

                mAudioSampleOffset += mSingleAudioFrameSizeInSamples;
                audioBufferStartOffset = mAudioSampleOffset;
                mNumReadFrames++;

                if (mNumReadFrames >= mMasher->field_2C_audio_header.field_10_num_frames_interleave)
                {
                    break;
                }
            }
        }

        if (mNumReadFrames >= mMasher->field_2C_audio_header.field_10_num_frames_interleave)
        {
            // Update the offset to the size of the first demuxed frame
            mCurrentAudioOffset = audioBufferStartOffset;
            if (!mNoAudioOrAudioError)
            {
                // Sound entry is created and populated with 1 frame, play it
                /*if (FAILED(GetSoundAPI().SND_PlayEx(&fmv_sound_entry_5CA208, 116, 116, 1.0, 0, 1, 100)))
                {
                    mNoAudioOrAudioError = 1;
                }*/
            }
            mNumPlayedAudioFrames = 0;
            mOldBufferPlayPos = 0;
            return 1;
        }

        return 0;
    }

    bool InitDDVPlayback(const std::string& filePath, bool ffmpegExport)
    {
        mHasAudio = 0;
        mAudioSampleOffset = 0;
        mNumReadFrames = 0;
        mNoAudioOrAudioError = 0;
        mSingleAudioFrameSizeInSamples = 0;
        mCurrentAudioOffset = 0;
        mNumPlayedAudioFrames = 0;
        mOldBufferPlayPos = 0;

        //gFrameBuffer.resize(640 * 480 * 4);

        if (mMasher != nullptr)
        {
            delete mMasher;
            mMasher = nullptr;
        }

        mMasher = new Masher();

        //gMasherTexture = CreateVideoPlayerTexture();

        mMasher->Init(filePath.c_str());

        /*
        AssetFMVParams params;
        params.video.fps = 15;
        params.video.width = mMasher->field_14_video_header.field_4_width;
        params.video.height = mMasher->field_14_video_header.field_8_height;
        params.outputPath = filePath;

        params.audio.audioSampleRate = mMasher->field_2C_audio_header.field_4_samples_per_second;
        params.audio.audioChannels = (mMasher->field_2C_audio_header.field_0_audio_format & 1) ? 2 : 1;
        */

        if (ffmpegExport)
        {
            //ffmpeg_begin(params);
        }

        mHasAudio = ((u32) mMasher->field_4_ddv_header.field_4_contains >> 1) & 1;
        mSingleAudioFrameSizeInSamples = mMasher->field_2C_audio_header.field_C_single_audio_frame_size;
        // const auto fmv_sound_entry_size = mSingleAudioFrameSizeInSamples * (mMasher->field_2C_audio_header.field_10_num_frames_interleave + 6);

        mNoAudioOrAudioError = 0;
        if (mHasAudio && mMasher->field_2C_audio_header.field_0_audio_format)
        {
            // if (GetSoundAPI().SND_New(
            //     &fmv_sound_entry_5CA208,
            //     fmv_sound_entry_size,
            //     pMasher_audio_header_5CA1E0->field_4_samples_per_second,
            //     (pMasher_audio_header_5CA1E0->field_0_audio_format & 2) != 0 ? 16 : 8,
            //     (pMasher_audio_header_5CA1E0->field_0_audio_format & 1) | 6)
            //     < 0)
            //{
            //     // SND_New failed
            //     fmv_sound_entry_5CA208.field_4_pDSoundBuffer = nullptr;
            //     mNoAudioOrAudioError = 1;
            // }
        }
        else
        {
            // Source DDV has no audio
            mNoAudioOrAudioError = 1;
        }

        if (Mash_DecompressAudio() && mMasher->ReadNextFrame() && mMasher->ReadNextFrame())
        {
            return true;
        }
        else
        {
            return false;
        }
    }

    bool StepDDVPlayback(bool ffmpegExport, RGBA32* pPixelBuffer)
    {
        //mMasher->VideoFrameDecode_Raw(gFrameBuffer.data());
        mMasher->VideoFrameDecode(pPixelBuffer);

        if (ffmpegExport)
        {
            //ffmpeg_push_frame(gFrameBuffer.data(), mMasher->field_14_video_header.field_4_width, mMasher->field_14_video_header.field_8_height);
        }
        else
        {
            //glBindTexture(GL_TEXTURE_2D, gMasherTexture);
            //glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, mMasher->field_14_video_header.field_4_width, mMasher->field_14_video_header.field_8_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, gFrameBuffer.data());
        }

        mNumReadFrames++;

        if (!mNoAudioOrAudioError)
        {
            //const int bitsPerSample = (mMasher->field_2C_audio_header.field_0_audio_format & 2) ? 16 : 8;
            //const int channels = (mMasher->field_2C_audio_header.field_0_audio_format & 1) ? 2 : 1;

            void* pDecompressedAudioFrame = Masher::GetDecompressedAudioFrame_4EAC60(mMasher);

            if (pDecompressedAudioFrame)
            {
                if (ffmpegExport)
                {
                    //ffmpeg_push_audio(reinterpret_cast<u8*>(pDecompressedAudioFrame), (bitsPerSample / 8) * mSingleAudioFrameSizeInSamples * channels);
                }

                //// Push new samples into the buffer
                // if (GetSoundAPI().SND_LoadSamples(&fmv_sound_entry_5CA208, fmv_audio_sample_offset_5CA238, (u8*)pDecompressedAudioFrame, fmv_single_audio_frame_size_in_samples_5CA240) < 0)
                //{
                //     // Reload with data fail
                //     bNoAudioOrAudioError_5CA1F4 = 1;
                // }
            }
            else
            {
                // if (GetSoundAPI().SND_Clear(&fmv_sound_entry_5CA208, fmv_audio_sample_offset_5CA238, fmv_single_audio_frame_size_in_samples_5CA240) < 0)
                //{
                //     // Reload with silence on failure or no data
                //     bNoAudioOrAudioError_5CA1F4 = 1;
                // }
            }

            mAudioSampleOffset += mSingleAudioFrameSizeInSamples;
        }
        const s32 bMoreFrames = mMasher->ReadNextFrame();

        if (!bMoreFrames)
        {
            if (ffmpegExport)
            {
                //ffmpeg_end();
            }

            return false;
        }

        return true;
    }

    void StartPlayback(const std::string& filePath, bool ffmpegExport)
    {
        mFrameTime = 0;
        mFMVHasFrames = InitDDVPlayback(filePath, ffmpegExport);

        LOG_INFO("fps: %d", mMasher->field_14_video_header.field_14_key_frame_rate);
    }

    void ExportDDVThreadFunc(std::string name)
    {
        mFMVExportFilePath = name;

        int currentFrame = 0;
        StartPlayback(mFMVExportFilePath, true);

        //int width = mMasher->field_14_video_header.field_4_width;
        //int height = mMasher->field_14_video_header.field_8_height;

        int width = 640;
        int height = 240;

        std::vector<RGBA32> frame(width * height);

        while (StepDDVPlayback(true, frame.data()))
        {
            currentFrame++;
            mFMVExportProgress = (float) currentFrame / mMasher->field_4_ddv_header.field_C_number_of_frames;
            mFMVExportMessage = mFMVExportFilePath + "\nExporting frame " + std::to_string(currentFrame) + " of " + std::to_string(mMasher->field_4_ddv_header.field_C_number_of_frames);
            LOG_INFO(mFMVExportMessage.c_str());
        }

        mFMVExportProgress = 1;
        mFMVExportMessage = "Done!";
        mFMVExporting = false;
    }

private:
    bool mHasAudio = false;
    u32 mAudioSampleOffset = 0;
    s32 mNumReadFrames = 0;
    bool mNoAudioOrAudioError = false;
    u32 mSingleAudioFrameSizeInSamples = 0;
    u32 mCurrentAudioOffset = 0;
    u32 mNumPlayedAudioFrames = 0;
    u32 mOldBufferPlayPos = 0;
    Masher* mMasher = nullptr;

    std::string mFMVExportFilePath;
 
    u32 mFMVHasFrames = 0;
    float mFMVExportProgress = 0.0f;
    int mFrameTime = 0;
    std::string mFMVExportMessage;
    bool mFMVExporting = false;
};

class FmvConv final
{
public:
    explicit FmvConv(IDDVReader& ddvReader)
        : mDDVReader(ddvReader)
    {

    }

    void Convert(std::string fName)
    {
        TRACE_ENTRYEXIT;

        struct VideoInfo final
        {
            u32 width = 0;
            u32 height = 0;
        };

        VideoInfo info;
        // keep these hardcoded until you expose FrameWidth/FrameHeight on IDDVReader
        info.width = 640;  // mDDVReader.FrameWidth();
        info.height = 240; // mDDVReader.FrameHeight();

        aom_codec_iface_t* encoder = &aom_codec_av1_cx_algo;
        if (!encoder)
        {
            ALIVE_FATAL("Unsupported codec.");
        }

        aom_image_t rawImageFrameData;
        if (!aom_img_alloc(&rawImageFrameData, AOM_IMG_FMT_YV12, info.width, info.height, 1))
        {
            ALIVE_FATAL("Failed to allocate image.");
        }

        aom_codec_enc_cfg_t cfg = {};
        if (aom_codec_enc_config_default(encoder, &cfg, AOM_USAGE_REALTIME))
        {
            ALIVE_FATAL("Failed to get default codec config.");
        }
        cfg.g_w = info.width;
        cfg.g_h = info.height;
        cfg.g_timebase.num = 1;
        cfg.g_timebase.den = 15; // fps
        // cfg.rc_target_bitrate = bitrate;

        aom_codec_ctx_t codec = {};
        if (aom_codec_enc_init(&codec, encoder, &cfg, 0))
        {
            ALIVE_FATAL("Failed to initialize encoder");
        }

        const int speed = 8;
        if (aom_codec_control(&codec, AOME_SET_CPUUSED, speed))
        {
            ALIVE_FATAL("Failed to set cpu-used");
        }

        LOG_INFO("Opening");

        const std::string outPath = "test2.webm";
        FILE* outFile = fopen(outPath.c_str(), "wb");
        if (!outFile)
        {
            ALIVE_FATAL("Failed to open output file");
        }

        LOG_INFO("Output opened");

        {
            mkvmuxer::MkvWriter writer(outFile);
            mkvmuxer::Segment segment;
            if (mkv_init(&writer, &segment, &cfg, &codec) != 0)
            {
                ALIVE_FATAL("mkv_init failed");
            }


            // helper lambda: convert RGBA32 -> YV12 into rawImageFrameData
            auto FillYV12FromRGBA = [&](const RGBA32* src)
            {
                const int w = static_cast<int>(info.width);
                const int h = static_cast<int>(info.height);

                uint8_t* yPlane = rawImageFrameData.planes[AOM_PLANE_Y];
                uint8_t* uPlane = rawImageFrameData.planes[AOM_PLANE_U];
                uint8_t* vPlane = rawImageFrameData.planes[AOM_PLANE_V];

                const int yStride = rawImageFrameData.stride[AOM_PLANE_Y];
                const int uStride = rawImageFrameData.stride[AOM_PLANE_U];
                const int vStride = rawImageFrameData.stride[AOM_PLANE_V];

                // Fill Y plane
                for (int y = 0; y < h; ++y)
                {
                    uint8_t* yRow = yPlane + y * yStride;
                    const RGBA32* srcRow = src + y * w;
                    for (int x = 0; x < w; ++x)
                    {
                        const RGBA32& p = srcRow[x];
                        // BT.601-ish integer approximation
                        int Y = (66 * p.r + 129 * p.g + 25 * p.b + 128) >> 8;
                        Y += 16;
                        if (Y < 0)
                            Y = 0;
                        if (Y > 255)
                            Y = 255;
                        yRow[x] = static_cast<uint8_t>(Y);
                    }
                }

                // Fill U and V planes (2x2 subsampling)
                for (int y = 0; y < h; y += 2)
                {
                    uint8_t* uRow = uPlane + (y / 2) * uStride;
                    uint8_t* vRow = vPlane + (y / 2) * vStride;
                    const RGBA32* srcRow0 = src + y * w;
                    const RGBA32* srcRow1 = src + (y + 1 < h ? (y + 1) * w : y * w); // handle odd height
                    for (int x = 0; x < w; x += 2)
                    {
                        // accumulate four pixels (handle right/bottom edges)
                        int r0 = srcRow0[x].r;
                        int g0 = srcRow0[x].g;
                        int b0 = srcRow0[x].b;

                        int r1 = (x + 1 < w) ? srcRow0[x + 1].r : r0;
                        int g1 = (x + 1 < w) ? srcRow0[x + 1].g : g0;
                        int b1 = (x + 1 < w) ? srcRow0[x + 1].b : b0;

                        int r2 = (y + 1 < h) ? srcRow1[x].r : r0;
                        int g2 = (y + 1 < h) ? srcRow1[x].g : g0;
                        int b2 = (y + 1 < h) ? srcRow1[x].b : b0;

                        int r3 = (y + 1 < h && x + 1 < w) ? srcRow1[x + 1].r : r2;
                        int g3 = (y + 1 < h && x + 1 < w) ? srcRow1[x + 1].g : g2;
                        int b3 = (y + 1 < h && x + 1 < w) ? srcRow1[x + 1].b : b2;

                        // average the 2x2 block
                        int rAvg = (r0 + r1 + r2 + r3) >> 2;
                        int gAvg = (g0 + g1 + g2 + g3) >> 2;
                        int bAvg = (b0 + b1 + b2 + b3) >> 2;

                        // integer conversion
                        int U = ((-38 * rAvg - 74 * gAvg + 112 * bAvg + 128) >> 8) + 128;
                        int V = ((112 * rAvg - 94 * gAvg - 18 * bAvg + 128) >> 8) + 128;

                        if (U < 0)
                            U = 0;
                        if (U > 255)
                            U = 255;
                        if (V < 0)
                            V = 0;
                        if (V > 255)
                            V = 255;

                        const int uv_x = x / 2;
                        uRow[uv_x] = static_cast<uint8_t>(U);
                        vRow[uv_x] = static_cast<uint8_t>(V);
                    }
                }
            };

            // Begin DDV decoding and encode loop
            mDDVReader.StartPlayback(fName, false);

            const int width = static_cast<int>(info.width);
            const int height = static_cast<int>(info.height);
            std::vector<RGBA32> frameBuf(width * height);

            int frame_index = 0;
            while (mDDVReader.StepDDVPlayback(false, frameBuf.data()))
            {
                LOG_INFO("StepDDVPlayback frame: %d", frame_index);

                // Convert RGBA -> YV12 in the aom_image_t
                FillYV12FromRGBA(frameBuf.data());

                // encode the frame into aom / mux it
                if (!encode_frame(&segment, &cfg, &codec, &rawImageFrameData, frame_index++, 0))
                {
                    // encode_frame returns false when encoder produced no pkt this round.
                    // we still continue; packets may be produced later or on flush.
                }
            }

            // Flush encoder - call encode_frame with img = NULL and frame_index = -1 until it returns false
            while (encode_frame(&segment, &cfg, &codec, NULL, -1, 0))
            {
                continue;
            }

            const bool ok = segment.Finalize();
            if (!ok)
            {
                fprintf(stderr, "webmenc> Segment::Finalize failed.\n");
            }

            fclose(outFile);
        }

        aom_img_free(&rawImageFrameData);
    }


private:

    int mkv_write_block(mkvmuxer::Segment* segment, const aom_codec_enc_cfg_t* cfg, const aom_codec_cx_pkt_t* pkt)
    {
        int64_t pts_ns = pkt->data.frame.pts * 1000000000ll * cfg->g_timebase.num / cfg->g_timebase.den;
        if (pts_ns <= mLast_pts_ns)
        {
            pts_ns = mLast_pts_ns + 1000000;
        }

        mLast_pts_ns = pts_ns;

        if (!segment->AddFrame(static_cast<uint8_t*>(pkt->data.frame.buf),
                               pkt->data.frame.sz, kVideoTrackNumber, pts_ns,
                               pkt->data.frame.flags & AOM_FRAME_IS_KEY))
        {
            fprintf(stderr, "webmenc> AddFrame failed.\n");
            return -1;
        }
        return 0;
    }

    int mkv_init(mkvmuxer::MkvWriter* writer, mkvmuxer::Segment* segment, aom_codec_enc_cfg_t* cfg, aom_codec_ctx_t* codec)
    {
        mLast_pts_ns = 0;

        bool ok = segment->Init(writer);
        if (!ok)
        {
            fprintf(stderr, "webmenc> mkvmuxer Init failed.\n");
            return -1;
        }

        segment->set_mode(mkvmuxer::Segment::kFile);
        segment->OutputCues(true);

        mkvmuxer::SegmentInfo* const info = segment->GetSegmentInfo();
        if (!info)
        {
            fprintf(stderr, "webmenc> Cannot retrieve Segment Info.\n");
            return -1;
        }

        const uint64_t kTimecodeScale = 1000000;
        info->set_timecode_scale(kTimecodeScale);
        std::string version = "aomenc";
        /*
        if (!webm_ctx->debug)
        {
            version.append(std::string(" ") + aom_codec_version_str());
        }*/

        info->set_writing_app(version.c_str());


        const uint64_t video_track_id = segment->AddVideoTrack(static_cast<int>(cfg->g_w),
                                                               static_cast<int>(cfg->g_h), kVideoTrackNumber);
        mkvmuxer::VideoTrack* const video_track = static_cast<mkvmuxer::VideoTrack*>(
            segment->GetTrackByNumber(video_track_id));

        if (!video_track)
        {
            fprintf(stderr, "webmenc> Video track creation failed.\n");
            return -1;
        }

        //const uint64_t audio_track_id = segment->AddAudioTrack(22050, 2, kAudioTrackNumber);
        //mkvmuxer::AudioTrack* const audio_track = static_cast<mkvmuxer::AudioTrack*>(segment->GetTrackByNumber(kAudioTrackNumber));
        //
        //if (!audio_track)
        //{
        //    fprintf(stderr, "webmenc> Audio track creation failed.\n");
        //    return -1;
        //}

        //audio_track->set_bit_depth(16);
        //audio_track->set_codec_id(mkvmuxer::Tracks::kOpusCodecId);


        ok = false;
        aom_fixed_buf_t* obu_sequence_header = aom_codec_get_global_headers(codec);
        if (obu_sequence_header)
        {
            Av1Config av1_config;
            if (get_av1config_from_obu(
                    reinterpret_cast<const uint8_t*>(obu_sequence_header->buf),
                    obu_sequence_header->sz, false, &av1_config)
                == 0)
            {
                uint8_t av1_config_buffer[4] = {0};
                size_t bytes_written = 0;
                if (write_av1config(&av1_config, sizeof(av1_config_buffer),
                                    &bytes_written, av1_config_buffer)
                    == 0)
                {
                    ok = video_track->SetCodecPrivate(av1_config_buffer,
                                                      sizeof(av1_config_buffer));
                }
            }
            free(obu_sequence_header->buf);
            free(obu_sequence_header);
        }
        if (!ok)
        {
            fprintf(stderr, "webmenc> Unable to set AV1 config.\n");
            return -1;
        }

        ok = video_track->SetStereoMode(1); // STEREO_FORMAT_LEFT_RIGHT
        if (!ok)
        {
            fprintf(stderr, "webmenc> Unable to set stereo mode.\n");
            return -1;
        }

        video_track->set_codec_id("V_AV1");

        /*
        // Default to 1:1 pixel aspect ratio.
        input->pixel_aspect_ratio.numerator = 1;
        input->pixel_aspect_ratio.denominator = 1;

        if (par->numerator > 1 || par->denominator > 1)
        {
            const uint64_t display_width = static_cast<uint64_t>(((cfg->g_w * par->numerator * 1.0) / par->denominator) + .5);
            video_track->set_display_width(display_width);
            video_track->set_display_height(cfg->g_h);
        }
        */

        /*
        if (encoder_settings != nullptr)
        {
            mkvmuxer::Tag* tag = segment->AddTag();
            if (tag == nullptr)
            {
                fprintf(stderr, "webmenc> Unable to allocate memory for encoder settings tag.\n");
                return -1;
            }
            ok = tag->add_simple_tag("ENCODER_SETTINGS", encoder_settings);
            if (!ok)
            {
                fprintf(stderr, "webmenc> Unable to allocate memory for encoder settings tag.\n");
                return -1;
            }
        }*/

        /*
        if (webm_ctx->debug)
        {
            video_track->set_uid(kDebugTrackUid);
        }*/

        // webm_ctx->writer = writer.release();
        // webm_ctx->segment = segment.release();
        return 0;
    }

    bool encode_frame(mkvmuxer::Segment* segment, const aom_codec_enc_cfg_t* cfg, aom_codec_ctx_t* codec, aom_image_t* img, int frame_index, int flags)
    {
        bool got_pkts = false;
        aom_codec_iter_t iter = nullptr;
        const aom_codec_cx_pkt_t* pkt = nullptr;
        const aom_codec_err_t res = aom_codec_encode(codec, img, frame_index, 1, flags);
        if (res != AOM_CODEC_OK)
        {
            ALIVE_FATAL("Failed to encode frame");
        }

        while ((pkt = aom_codec_get_cx_data(codec, &iter)) != nullptr)
        {
            got_pkts = true;

            if (pkt->kind == AOM_CODEC_CX_FRAME_PKT)
            {
                //const int keyframe = (pkt->data.frame.flags & AOM_FRAME_IS_KEY) != 0;

                if (mkv_write_block(segment, cfg, pkt) != 0)
                {
                    ALIVE_FATAL("Failed to write compressed frame");
                }
                //LOG_INFO(keyframe ? "K" : ".");
            }
        }

        return got_pkts;
    }

private:
    const int kVideoTrackNumber = 1;
    const int kAudioTrackNumber = 2;
    int64_t mLast_pts_ns = 0;
    IDDVReader& mDDVReader;
};

void ConvertFMVs(const FileSystem::Path& /*dataDir*/, bool isAo)
{
    // TODO: Conversion
    if (!isAo)
    {
        const FmvInfo* pInfo = Path_Get_FMV_Record(EReliveLevelIds::eMines, 1);
        if (pInfo)
        {
            IDDVReader reader;
            {
                FmvConv fmvConv(reader);
                fmvConv.Convert(pInfo->field_0_pName);
            }
        }
    }
}
