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

#ifndef D3D12_VIDEO_DEC_REFMGR_H
#define D3D12_VIDEO_DEC_REFMGR_H

#include "d3d12_video_dec_types.h"

struct D3D12VidDecReferenceDataManager
{
    D3D12VidDecReferenceDataManager(
        const struct d3d12_screen* pD3D12Screen,
        D3D12_VIDEO_DECODE_PROFILE_TYPE DecodeProfileType,
        UINT NodeMask);

    UINT Size() const { return (UINT)textures.size(); }
    bool IsReferenceOnly() { return m_fReferenceOnly; }

    void Resize(UINT16 dbp, _In_opt_ D3D12ReferenceOnlyDesc* pReferenceOnly, bool fArrayOfTexture);
    
    void ResetInternalTrackingReferenceUsage();
    void ResetReferenceFramesInformation();

    template<typename T, size_t size> 
    void MarkReferencesInUse(const T (&picEntries)[size]);
    void MarkReferenceInUse(UINT16 index);

    void ReleaseUnusedReferences();
    
    UINT16 StoreFutureReference(UINT16 index, _In_ ComPtr<ID3D12VideoDecoderHeap> & decoderHeap, ID3D12Resource* pTexture2D, UINT subresourceIndex);
    
    // Will clear() argument outNeededTransitions and fill it with the necessary transitions to perform by the caller after the method returns
    template<typename T, size_t size> 
    void UpdateEntries(T (&picEntries)[size], std::vector<D3D12_RESOURCE_BARRIER> & outNeededTransitions);

    void GetReferenceOnlyOutput(
        ID3D12Resource*& pOutputReferenceNoRef, // out -> new reference slot assigned or nullptr
        UINT& OutputSubresource, // out -> new reference slot assigned or nullptr
        bool& outNeedsTransitionToDecodeWrite // out -> indicates if output resource argument has to be transitioned to D3D12_RESOURCE_STATE_VIDEO_DECODE_READ by the caller
    );

    // D3D12 DecodeFrame Parameters.
    std::vector<ID3D12Resource *>                        textures;
    std::vector<UINT>                                    texturesSubresources;
    std::vector<ID3D12VideoDecoderHeap *>                decoderHeapsParameter;

protected:
    
    template<typename T, size_t size> 
    void GetUpdatedEntries(T (&picEntries)[size]);
    UINT16 GetUpdatedEntry(UINT16 index);

    UINT16 UpdateEntry(
                UINT16 index, // in
                ID3D12Resource*& pOutputReferenceNoRef, // out -> new reference slot assigned or nullptr
                UINT& OutputSubresource, // out -> new reference slot assigned or 0
                bool& outNeedsTransitionToDecodeRead // out -> indicates if output resource argument has to be transitioned to D3D12_RESOURCE_STATE_VIDEO_DECODE_READ by the caller
            );

    struct ReferenceData
    {
        ComPtr<ID3D12VideoDecoderHeap>      decoderHeap;
        ComPtr<ID3D12Resource>              referenceOnlyTexture; // Allocated and lifetime managed by translation layer
        ID3D12Resource*                     referenceTexture;     // May point to caller allocated resource or referenceOnlyTexture
        UINT                                subresourceIndex;
        UINT16                              originalIndex;
        bool                                fUsed;
    };

    void ResizeDataStructures(UINT size);
    UINT16 FindRemappedIndex(UINT16 originalIndex);

    std::vector<ReferenceData>                           referenceDatas;

    const struct d3d12_screen*                           m_pD3D12Screen;
    UINT16                                               m_invalidIndex;
    UINT16                                               m_currentOutputIndex = 0;
    bool                                                 m_fReferenceOnly = false;
    bool                                                 m_fArrayOfTexture = false;
    UINT                                                 m_NodeMask;
};

//----------------------------------------------------------------------------------------------------------------------------------
template<typename T, size_t size>
inline void D3D12VidDecReferenceDataManager::UpdateEntries(T (&picEntries)[size], std::vector<D3D12_RESOURCE_BARRIER> & outNeededTransitions)
{
    outNeededTransitions.clear();

    for (auto& picEntry : picEntries)
    {
            // UINT16 UpdateEntry(
            //     UINT16 index, // in
            //     ID3D12Resource*& pOutputReferenceNoRef, // out -> new reference slot assigned or nullptr
            //     UINT& OutputSubresource, // out -> new reference slot assigned or 0
            //     bool& outNeedsTransitionToDecodeRead // out -> indicates if output resource argument has to be transitioned to D3D12_RESOURCE_STATE_VIDEO_DECODE_READ by the caller
            // );

        ID3D12Resource* pOutputReferenceNoRef = { };
        UINT OutputSubresource = 0u;
        bool outNeedsTransitionToDecodeRead = false;

        picEntry.Index7Bits = UpdateEntry(picEntry.Index7Bits, pOutputReferenceNoRef, OutputSubresource, outNeedsTransitionToDecodeRead);
        if(outNeedsTransitionToDecodeRead)
        {
            outNeededTransitions.push_back(CD3DX12_RESOURCE_BARRIER::Transition(pOutputReferenceNoRef, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_VIDEO_DECODE_READ, OutputSubresource));
        }        
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
template<typename T, size_t size>
inline void D3D12VidDecReferenceDataManager::GetUpdatedEntries(T (&picEntries)[size])
{
    for (auto& picEntry : picEntries)
    {
        picEntry.Index7Bits = GetUpdatedEntry(picEntry.Index7Bits);
    }
}

//----------------------------------------------------------------------------------------------------------------------------------    
template<typename T, size_t size> 
inline void D3D12VidDecReferenceDataManager::MarkReferencesInUse(const T (&picEntries)[size])
{
    for (auto& picEntry : picEntries)
    {
        MarkReferenceInUse(picEntry.Index7Bits);
    }
}

#endif