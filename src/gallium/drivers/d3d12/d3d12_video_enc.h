/*
 * Copyright © Microsoft Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef D3D12_VIDEO_ENC_H
#define D3D12_VIDEO_ENC_H

#include "d3d12_video_types.h"
#include "d3d12_resource_copy_helper.h"
#include "d3d12_video_encoder_references_manager.h"
#include "d3d12_video_dpb_storage_manager.h"

///
/// Pipe video interface starts
///

/**
 * creates a video encoder
 */
struct pipe_video_codec *d3d12_video_encoder_create_encoder(struct pipe_context *context,
                                               const struct pipe_video_codec *templ);

/**
 * destroy this video encoder
 */
void d3d12_video_encoder_destroy(struct pipe_video_codec *codec);

/**
 * start encoding of a new frame
 */
void d3d12_video_encoder_begin_frame(struct pipe_video_codec *codec,
                     struct pipe_video_buffer *target,
                     struct pipe_picture_desc *picture);

/**
 * encode to a bitstream
 */
void d3d12_video_encoder_encode_bitstream(struct pipe_video_codec *codec,
                           struct pipe_video_buffer *source,
                           struct pipe_resource *destination,
                           void **feedback);

/**
 * end encoding of the current frame
 */
void d3d12_video_encoder_end_frame(struct pipe_video_codec *codec,
                  struct pipe_video_buffer *target,
                  struct pipe_picture_desc *picture);

/**
 * flush any outstanding command buffers to the hardware
 * should be called before a video_buffer is acessed by the gallium frontend again
 */
void d3d12_video_encoder_flush(struct pipe_video_codec *codec);

///
/// Pipe video interface ends
///

///
/// d3d12_video_encoder functions starts
///

struct d3d12_video_encoder
{
    struct pipe_video_codec base;
    struct pipe_screen* m_screen;
    struct d3d12_screen* m_pD3D12Screen;

    ///
    /// D3D12 objects and context info
    ///

    const uint m_NodeMask = 1 << 0u;
    const uint m_NodeIndex = 0u;

    ComPtr<ID3D12Fence> m_spFence;
    uint m_fenceValue = 1u;

    ComPtr<ID3D12VideoDevice3> m_spD3D12VideoDevice;
    ComPtr<ID3D12VideoEncoder> m_spVideoEncoder;
    ComPtr<ID3D12VideoEncoderHeap> m_spVideoEncoderHeap;
    ComPtr<ID3D12CommandQueue> m_spEncodeCommandQueue;
    ComPtr<ID3D12CommandAllocator> m_spCommandAllocator;
    ComPtr<ID3D12VideoEncodeCommandList2> m_spEncodeCommandList;
    ComPtr<ID3D12CommandQueue> m_spCopyQueue;
    std::unique_ptr<D3D12ResourceCopyHelper> m_D3D12ResourceCopyHelper;  
    std::vector<D3D12_RESOURCE_BARRIER> m_transitionsBeforeCloseCmdList;

    // Number of consecutive encode_frame calls without end_frame call
    UINT m_numConsecutiveEncodeFrame = 0;

    // Number of consecutive begin_frame calls without end_frame call
    UINT m_numNestedBeginFrame = 0;

    std::unique_ptr<ID3D12AutomaticVideoEncodeReferencePicManager> m_upDPBManager;
    std::unique_ptr<ID3D12VideoDPBStorageManager<ID3D12VideoEncoderHeap> > m_upDPBStorageManager;

    struct
    {
        pipe_h264_enc_picture_desc m_currentRequestedConfig;

        D3D12_VIDEO_ENCODER_PICTURE_RESOLUTION_DESC m_currentResolution = { };

        D3D12_FEATURE_DATA_FORMAT_INFO m_encodeFormatInfo = { };

        D3D12_VIDEO_ENCODER_CODEC m_encoderCodecDesc = { };

        /// As the following D3D12 Encode types have pointers in their structures, we need to keep a deep copy of them

        union 
        {
            D3D12_VIDEO_ENCODER_PROFILE_H264 m_H264Profile;
            D3D12_VIDEO_ENCODER_PROFILE_HEVC m_HEVCProfile;
        } m_encoderProfileDesc = { };

        union 
        {
            D3D12_VIDEO_ENCODER_LEVELS_H264 m_H264LevelSetting;
            D3D12_VIDEO_ENCODER_LEVEL_TIER_CONSTRAINTS_HEVC m_HEVCLevelSetting;
        } m_encoderLevelDesc = { };

        struct
        {
            D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE m_Mode;
            D3D12_VIDEO_ENCODER_RATE_CONTROL_FLAGS m_Flags;
            DXGI_RATIONAL m_FrameRate;
            union
            {
                D3D12_VIDEO_ENCODER_RATE_CONTROL_CQP m_Configuration_CQP;
                D3D12_VIDEO_ENCODER_RATE_CONTROL_CBR m_Configuration_CBR;
                D3D12_VIDEO_ENCODER_RATE_CONTROL_VBR m_Configuration_VBR;
                D3D12_VIDEO_ENCODER_RATE_CONTROL_QVBR m_Configuration_QVBR;
            } m_Config;
        } m_encoderRateControlDesc = { };

        union 
        {
            D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_H264 m_H264Config;
            D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_HEVC m_HEVCConfig;
        } m_encoderCodecSpecificConfigDesc = { };

        
        D3D12_VIDEO_ENCODER_FRAME_SUBREGION_LAYOUT_MODE m_encoderSliceConfigMode;
        union 
        {
            D3D12_VIDEO_ENCODER_PICTURE_CONTROL_SUBREGIONS_LAYOUT_DATA_SLICES m_SlicesPartition_H264;
            D3D12_VIDEO_ENCODER_PICTURE_CONTROL_SUBREGIONS_LAYOUT_DATA_SLICES m_SlicesPartition_HEVC;
        } m_encoderSliceConfigDesc = { };

        union 
        {
            D3D12_VIDEO_ENCODER_SEQUENCE_GOP_STRUCTURE_H264 m_H264GroupOfPictures;
            D3D12_VIDEO_ENCODER_SEQUENCE_GOP_STRUCTURE_HEVC m_HEVCGroupOfPictures;
        } m_encoderGOPConfigDesc = { };

        union 
        {
            D3D12_VIDEO_ENCODER_PICTURE_CONTROL_CODEC_DATA_H264 m_H264PicData;
            D3D12_VIDEO_ENCODER_PICTURE_CONTROL_CODEC_DATA_HEVC m_HEVCPicData;
        } m_encoderPicParamsDesc = { };

        D3D12_VIDEO_ENCODER_MOTION_ESTIMATION_PRECISION_MODE m_encoderMotionPrecisionLimit = D3D12_VIDEO_ENCODER_MOTION_ESTIMATION_PRECISION_MODE_MAXIMUM;

    } m_currentEncodeConfig;
};

bool d3d12_create_video_encode_command_objects(struct d3d12_video_encoder* pD3D12Enc);
bool d3d12_video_encoder_reconfigure_session(struct d3d12_video_encoder* pD3D12Enc, struct pipe_video_buffer *srcTexture, struct pipe_picture_desc *picture);
void d3d12_video_encoder_update_current_encoder_config_state(struct d3d12_video_encoder* pD3D12Enc, struct pipe_video_buffer *srcTexture, struct pipe_picture_desc *picture);
void d3d12_video_encoder_reconfigure_encoder_objects(struct d3d12_video_encoder* pD3D12Enc, struct pipe_video_buffer *srcTexture, struct pipe_picture_desc *picture);
D3D12_VIDEO_ENCODER_LEVEL_SETTING d3d12_video_encoder_get_current_level_desc(struct d3d12_video_encoder* pD3D12Enc);
D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION d3d12_video_encoder_get_current_codec_config_desc(struct d3d12_video_encoder* pD3D12Enc);
D3D12_VIDEO_ENCODER_PROFILE_DESC d3d12_video_encoder_get_current_profile_desc(struct d3d12_video_encoder* pD3D12Enc);
///
/// d3d12_video_encoder functions ends
///

#endif