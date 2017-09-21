/*
 * Generic Implementation of strmbase video classes
 *
 * Copyright 2012 Aric Stewart, CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#define COBJMACROS

#include <assert.h>
#include "dshow.h"
#include "uuids.h"
#include "vfwmsgs.h"
#include "wine/debug.h"
#include "wine/unicode.h"
#include "wine/strmbase.h"

WINE_DEFAULT_DEBUG_CHANNEL(strmbase);

static inline BaseControlVideo *impl_from_IBasicVideo(IBasicVideo *iface)
{
    return CONTAINING_RECORD(iface, BaseControlVideo, IBasicVideo_iface);
}

HRESULT WINAPI BaseControlVideo_Init(BaseControlVideo *pControlVideo, const IBasicVideoVtbl *lpVtbl, BaseFilter *owner, CRITICAL_SECTION *lock, BasePin* pPin, const BaseControlVideoFuncTable* pFuncsTable)
{
    pControlVideo->IBasicVideo_iface.lpVtbl = lpVtbl;
    pControlVideo->pFilter = owner;
    pControlVideo->pInterfaceLock = lock;
    pControlVideo->pPin = pPin;
    pControlVideo->pFuncsTable = pFuncsTable;

    BaseDispatch_Init(&pControlVideo->baseDispatch, &IID_IBasicVideo);

    return S_OK;
}

HRESULT WINAPI BaseControlVideo_Destroy(BaseControlVideo *pControlVideo)
{
    return BaseDispatch_Destroy(&pControlVideo->baseDispatch);
}

static HRESULT BaseControlVideoImpl_CheckSourceRect(BaseControlVideo *This, RECT *pSourceRect)
{
    LONG VideoWidth, VideoHeight;
    HRESULT hr;

    if (IsRectEmpty(pSourceRect))
        return E_INVALIDARG;

    hr = BaseControlVideoImpl_GetVideoSize((IBasicVideo *)This, &VideoWidth, &VideoHeight);
    if (FAILED(hr))
        return hr;

    if (pSourceRect->top < 0 || pSourceRect->left < 0 ||
        pSourceRect->bottom > VideoHeight || pSourceRect->right > VideoWidth)
        return E_INVALIDARG;

    return S_OK;
}

static HRESULT BaseControlVideoImpl_CheckTargetRect(BaseControlVideo *This, RECT *pTargetRect)
{
    if (IsRectEmpty(pTargetRect))
        return E_INVALIDARG;

    return S_OK;
}

HRESULT WINAPI BaseControlVideoImpl_GetTypeInfoCount(IBasicVideo *iface, UINT *pctinfo)
{
    BaseControlVideo *This = impl_from_IBasicVideo(iface);

    return BaseDispatchImpl_GetTypeInfoCount(&This->baseDispatch, pctinfo);
}

HRESULT WINAPI BaseControlVideoImpl_GetTypeInfo(IBasicVideo *iface, UINT iTInfo, LCID lcid, ITypeInfo **ppTInfo)
{
    BaseControlVideo *This = impl_from_IBasicVideo(iface);

    return BaseDispatchImpl_GetTypeInfo(&This->baseDispatch, &IID_NULL, iTInfo, lcid, ppTInfo);
}

HRESULT WINAPI BaseControlVideoImpl_GetIDsOfNames(IBasicVideo *iface, REFIID riid, LPOLESTR *rgszNames, UINT cNames, LCID lcid, DISPID *rgDispId)
{
    BaseControlVideo *This = impl_from_IBasicVideo(iface);

    return BaseDispatchImpl_GetIDsOfNames(&This->baseDispatch, riid, rgszNames, cNames, lcid, rgDispId);
}

HRESULT WINAPI BaseControlVideoImpl_Invoke(IBasicVideo *iface, DISPID dispIdMember, REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *pDispParams, VARIANT *pVarResult, EXCEPINFO *pExepInfo, UINT *puArgErr)
{
    BaseControlVideo *This = impl_from_IBasicVideo(iface);
    ITypeInfo *pTypeInfo;
    HRESULT hr;

    hr = BaseDispatchImpl_GetTypeInfo(&This->baseDispatch, riid, 1, lcid, &pTypeInfo);
    if (SUCCEEDED(hr))
    {
        hr = ITypeInfo_Invoke(pTypeInfo, &This->IBasicVideo_iface, dispIdMember, wFlags, pDispParams, pVarResult, pExepInfo, puArgErr);
        ITypeInfo_Release(pTypeInfo);
    }

    return hr;
}

HRESULT WINAPI BaseControlVideoImpl_get_AvgTimePerFrame(IBasicVideo *iface, REFTIME *pAvgTimePerFrame)
{
    VIDEOINFOHEADER *vih;
    BaseControlVideo *This = impl_from_IBasicVideo(iface);

    if (!pAvgTimePerFrame)
        return E_POINTER;
    if (!This->pPin->pConnectedTo)
        return VFW_E_NOT_CONNECTED;

    TRACE("(%p/%p)->(%p)\n", This, iface, pAvgTimePerFrame);

    vih = This->pFuncsTable->pfnGetVideoFormat(This);
    *pAvgTimePerFrame = vih->AvgTimePerFrame;
    return S_OK;
}

HRESULT WINAPI BaseControlVideoImpl_get_BitRate(IBasicVideo *iface, LONG *pBitRate)
{
    VIDEOINFOHEADER *vih;
    BaseControlVideo *This = impl_from_IBasicVideo(iface);

    TRACE("(%p/%p)->(%p)\n", This, iface, pBitRate);

    if (!pBitRate)
        return E_POINTER;
    if (!This->pPin->pConnectedTo)
        return VFW_E_NOT_CONNECTED;

    vih = This->pFuncsTable->pfnGetVideoFormat(This);
    *pBitRate = vih->dwBitRate;
    return S_OK;
}

HRESULT WINAPI BaseControlVideoImpl_get_BitErrorRate(IBasicVideo *iface, LONG *pBitErrorRate)
{
    VIDEOINFOHEADER *vih;
    BaseControlVideo *This = impl_from_IBasicVideo(iface);

    TRACE("(%p/%p)->(%p)\n", This, iface, pBitErrorRate);

    if (!pBitErrorRate)
        return E_POINTER;
    if (!This->pPin->pConnectedTo)
        return VFW_E_NOT_CONNECTED;

    vih = This->pFuncsTable->pfnGetVideoFormat(This);
    *pBitErrorRate = vih->dwBitErrorRate;
    return S_OK;
}

HRESULT WINAPI BaseControlVideoImpl_get_VideoWidth(IBasicVideo *iface, LONG *pVideoWidth)
{
    VIDEOINFOHEADER *vih;
    BaseControlVideo *This = impl_from_IBasicVideo(iface);

    TRACE("(%p/%p)->(%p)\n", This, iface, pVideoWidth);
    if (!pVideoWidth)
        return E_POINTER;

    vih = This->pFuncsTable->pfnGetVideoFormat(This);
    *pVideoWidth = vih->bmiHeader.biWidth;

    return S_OK;
}

HRESULT WINAPI BaseControlVideoImpl_get_VideoHeight(IBasicVideo *iface, LONG *pVideoHeight)
{
    VIDEOINFOHEADER *vih;
    BaseControlVideo *This = impl_from_IBasicVideo(iface);

    TRACE("(%p/%p)->(%p)\n", This, iface, pVideoHeight);
    if (!pVideoHeight)
        return E_POINTER;

    vih = This->pFuncsTable->pfnGetVideoFormat(This);
    *pVideoHeight = abs(vih->bmiHeader.biHeight);

    return S_OK;
}

HRESULT WINAPI BaseControlVideoImpl_put_SourceLeft(IBasicVideo *iface, LONG SourceLeft)
{
    RECT SourceRect;
    BaseControlVideo *This = impl_from_IBasicVideo(iface);
    HRESULT hr;

    TRACE("(%p/%p)->(%d)\n", This, iface, SourceLeft);
    hr = This->pFuncsTable->pfnGetSourceRect(This, &SourceRect);
    if (SUCCEEDED(hr))
    {
        SourceRect.right = (SourceRect.right - SourceRect.left) + SourceLeft;
        SourceRect.left = SourceLeft;
        hr = BaseControlVideoImpl_CheckSourceRect(This, &SourceRect);
    }
    if (SUCCEEDED(hr))
        hr = This->pFuncsTable->pfnSetSourceRect(This, &SourceRect);

    return hr;
}

HRESULT WINAPI BaseControlVideoImpl_get_SourceLeft(IBasicVideo *iface, LONG *pSourceLeft)
{
    RECT SourceRect;
    BaseControlVideo *This = impl_from_IBasicVideo(iface);

    TRACE("(%p/%p)->(%p)\n", This, iface, pSourceLeft);
    if (!pSourceLeft)
        return E_POINTER;
    This->pFuncsTable->pfnGetSourceRect(This, &SourceRect);
    *pSourceLeft = SourceRect.left;

    return S_OK;
}

HRESULT WINAPI BaseControlVideoImpl_put_SourceWidth(IBasicVideo *iface, LONG SourceWidth)
{
    RECT SourceRect;
    BaseControlVideo *This = impl_from_IBasicVideo(iface);
    HRESULT hr;

    TRACE("(%p/%p)->(%d)\n", This, iface, SourceWidth);
    hr = This->pFuncsTable->pfnGetSourceRect(This, &SourceRect);
    if (SUCCEEDED(hr))
    {
        SourceRect.right = SourceRect.left + SourceWidth;
        hr = BaseControlVideoImpl_CheckSourceRect(This, &SourceRect);
    }
    if (SUCCEEDED(hr))
        hr = This->pFuncsTable->pfnSetSourceRect(This, &SourceRect);

    return hr;
}

HRESULT WINAPI BaseControlVideoImpl_get_SourceWidth(IBasicVideo *iface, LONG *pSourceWidth)
{
    RECT SourceRect;
    BaseControlVideo *This = impl_from_IBasicVideo(iface);

    TRACE("(%p/%p)->(%p)\n", This, iface, pSourceWidth);
    if (!pSourceWidth)
        return E_POINTER;
    This->pFuncsTable->pfnGetSourceRect(This, &SourceRect);
    *pSourceWidth = SourceRect.right - SourceRect.left;

    return S_OK;
}

HRESULT WINAPI BaseControlVideoImpl_put_SourceTop(IBasicVideo *iface, LONG SourceTop)
{
    RECT SourceRect;
    BaseControlVideo *This = impl_from_IBasicVideo(iface);
    HRESULT hr;

    TRACE("(%p/%p)->(%d)\n", This, iface, SourceTop);
    hr = This->pFuncsTable->pfnGetSourceRect(This, &SourceRect);
    if (SUCCEEDED(hr))
    {
        SourceRect.bottom = (SourceRect.bottom - SourceRect.top) + SourceTop;
        SourceRect.top = SourceTop;
        hr = BaseControlVideoImpl_CheckSourceRect(This, &SourceRect);
    }
    if (SUCCEEDED(hr))
        hr = This->pFuncsTable->pfnSetSourceRect(This, &SourceRect);

    return hr;
}

HRESULT WINAPI BaseControlVideoImpl_get_SourceTop(IBasicVideo *iface, LONG *pSourceTop)
{
    RECT SourceRect;
    BaseControlVideo *This = impl_from_IBasicVideo(iface);

    TRACE("(%p/%p)->(%p)\n", This, iface, pSourceTop);
    if (!pSourceTop)
        return E_POINTER;
    This->pFuncsTable->pfnGetSourceRect(This, &SourceRect);
    *pSourceTop = SourceRect.top;

    return S_OK;
}

HRESULT WINAPI BaseControlVideoImpl_put_SourceHeight(IBasicVideo *iface, LONG SourceHeight)
{
    RECT SourceRect;
    BaseControlVideo *This = impl_from_IBasicVideo(iface);
    HRESULT hr;

    TRACE("(%p/%p)->(%d)\n", This, iface, SourceHeight);
    hr = This->pFuncsTable->pfnGetSourceRect(This, &SourceRect);
    if (SUCCEEDED(hr))
    {
        SourceRect.bottom = SourceRect.top + SourceHeight;
        hr = BaseControlVideoImpl_CheckSourceRect(This, &SourceRect);
    }
    if (SUCCEEDED(hr))
        hr = This->pFuncsTable->pfnSetSourceRect(This, &SourceRect);

    return hr;
}

HRESULT WINAPI BaseControlVideoImpl_get_SourceHeight(IBasicVideo *iface, LONG *pSourceHeight)
{
    RECT SourceRect;
    BaseControlVideo *This = impl_from_IBasicVideo(iface);

    TRACE("(%p/%p)->(%p)\n", This, iface, pSourceHeight);
    if (!pSourceHeight)
        return E_POINTER;
    This->pFuncsTable->pfnGetSourceRect(This, &SourceRect);

    *pSourceHeight = SourceRect.bottom - SourceRect.top;

    return S_OK;
}

HRESULT WINAPI BaseControlVideoImpl_put_DestinationLeft(IBasicVideo *iface, LONG DestinationLeft)
{
    RECT DestRect;
    BaseControlVideo *This = impl_from_IBasicVideo(iface);
    HRESULT hr;

    TRACE("(%p/%p)->(%d)\n", This, iface, DestinationLeft);
    hr = This->pFuncsTable->pfnGetTargetRect(This, &DestRect);
    if (SUCCEEDED(hr))
    {
        DestRect.right = (DestRect.right - DestRect.left) + DestinationLeft;
        DestRect.left = DestinationLeft;
        hr = BaseControlVideoImpl_CheckTargetRect(This, &DestRect);
    }
    if (SUCCEEDED(hr))
        hr = This->pFuncsTable->pfnSetTargetRect(This, &DestRect);

    return hr;
}

HRESULT WINAPI BaseControlVideoImpl_get_DestinationLeft(IBasicVideo *iface, LONG *pDestinationLeft)
{
    RECT DestRect;
    BaseControlVideo *This = impl_from_IBasicVideo(iface);

    TRACE("(%p/%p)->(%p)\n", This, iface, pDestinationLeft);
    if (!pDestinationLeft)
        return E_POINTER;
    This->pFuncsTable->pfnGetTargetRect(This, &DestRect);
    *pDestinationLeft = DestRect.left;

    return S_OK;
}

HRESULT WINAPI BaseControlVideoImpl_put_DestinationWidth(IBasicVideo *iface, LONG DestinationWidth)
{
    RECT DestRect;
    BaseControlVideo *This = impl_from_IBasicVideo(iface);
    HRESULT hr;

    TRACE("(%p/%p)->(%d)\n", This, iface, DestinationWidth);
    hr = This->pFuncsTable->pfnGetTargetRect(This, &DestRect);
    if (SUCCEEDED(hr))
    {
        DestRect.right = DestRect.left + DestinationWidth;
        hr = BaseControlVideoImpl_CheckTargetRect(This, &DestRect);
    }
    if (SUCCEEDED(hr))
        hr = This->pFuncsTable->pfnSetTargetRect(This, &DestRect);

    return hr;
}

HRESULT WINAPI BaseControlVideoImpl_get_DestinationWidth(IBasicVideo *iface, LONG *pDestinationWidth)
{
    RECT DestRect;
    BaseControlVideo *This = impl_from_IBasicVideo(iface);

    TRACE("(%p/%p)->(%p)\n", This, iface, pDestinationWidth);
    if (!pDestinationWidth)
        return E_POINTER;
    This->pFuncsTable->pfnGetTargetRect(This, &DestRect);
    *pDestinationWidth = DestRect.right - DestRect.left;

    return S_OK;
}

HRESULT WINAPI BaseControlVideoImpl_put_DestinationTop(IBasicVideo *iface, LONG DestinationTop)
{
    RECT DestRect;
    BaseControlVideo *This = impl_from_IBasicVideo(iface);
    HRESULT hr;

    TRACE("(%p/%p)->(%d)\n", This, iface, DestinationTop);
    hr = This->pFuncsTable->pfnGetTargetRect(This, &DestRect);
    if (SUCCEEDED(hr))
    {
        DestRect.bottom = (DestRect.bottom - DestRect.top) + DestinationTop;
        DestRect.top = DestinationTop;
        hr = BaseControlVideoImpl_CheckTargetRect(This, &DestRect);
    }
    if (SUCCEEDED(hr))
        hr = This->pFuncsTable->pfnSetTargetRect(This, &DestRect);

    return hr;
}

HRESULT WINAPI BaseControlVideoImpl_get_DestinationTop(IBasicVideo *iface, LONG *pDestinationTop)
{
    RECT DestRect;
    BaseControlVideo *This = impl_from_IBasicVideo(iface);

    TRACE("(%p/%p)->(%p)\n", This, iface, pDestinationTop);
    if (!pDestinationTop)
        return E_POINTER;
    This->pFuncsTable->pfnGetTargetRect(This, &DestRect);
    *pDestinationTop = DestRect.top;

    return S_OK;
}

HRESULT WINAPI BaseControlVideoImpl_put_DestinationHeight(IBasicVideo *iface, LONG DestinationHeight)
{
    RECT DestRect;
    BaseControlVideo *This = impl_from_IBasicVideo(iface);
    HRESULT hr;

    TRACE("(%p/%p)->(%d)\n", This, iface, DestinationHeight);
    hr = This->pFuncsTable->pfnGetTargetRect(This, &DestRect);
    if (SUCCEEDED(hr))
    {
        DestRect.bottom = DestRect.top + DestinationHeight;
        hr = BaseControlVideoImpl_CheckTargetRect(This, &DestRect);
    }
    if (SUCCEEDED(hr))
        hr = This->pFuncsTable->pfnSetTargetRect(This, &DestRect);

    return hr;
}

HRESULT WINAPI BaseControlVideoImpl_get_DestinationHeight(IBasicVideo *iface, LONG *pDestinationHeight)
{
    RECT DestRect;
    BaseControlVideo *This = impl_from_IBasicVideo(iface);

    TRACE("(%p/%p)->(%p)\n", This, iface, pDestinationHeight);
    if (!pDestinationHeight)
        return E_POINTER;
    This->pFuncsTable->pfnGetTargetRect(This, &DestRect);
    *pDestinationHeight = DestRect.bottom - DestRect.top;

    return S_OK;
}

HRESULT WINAPI BaseControlVideoImpl_SetSourcePosition(IBasicVideo *iface, LONG Left, LONG Top, LONG Width, LONG Height)
{
    RECT SourceRect;
    BaseControlVideo *This = impl_from_IBasicVideo(iface);

    TRACE("(%p/%p)->(%d, %d, %d, %d)\n", This, iface, Left, Top, Width, Height);

    SetRect(&SourceRect, Left, Top, Left + Width, Top + Height);
    if (FAILED(BaseControlVideoImpl_CheckSourceRect(This, &SourceRect)))
        return E_INVALIDARG;
    return This->pFuncsTable->pfnSetSourceRect(This, &SourceRect);
}

HRESULT WINAPI BaseControlVideoImpl_GetSourcePosition(IBasicVideo *iface, LONG *pLeft, LONG *pTop, LONG *pWidth, LONG *pHeight)
{
    RECT SourceRect;
    BaseControlVideo *This = impl_from_IBasicVideo(iface);

    TRACE("(%p/%p)->(%p, %p, %p, %p)\n", This, iface, pLeft, pTop, pWidth, pHeight);
    if (!pLeft || !pTop || !pWidth || !pHeight)
        return E_POINTER;
    This->pFuncsTable->pfnGetSourceRect(This, &SourceRect);

    *pLeft = SourceRect.left;
    *pTop = SourceRect.top;
    *pWidth = SourceRect.right - SourceRect.left;
    *pHeight = SourceRect.bottom - SourceRect.top;

    return S_OK;
}

HRESULT WINAPI BaseControlVideoImpl_SetDefaultSourcePosition(IBasicVideo *iface)
{
    BaseControlVideo *This = impl_from_IBasicVideo(iface);

    TRACE("(%p/%p)->()\n", This, iface);
    return This->pFuncsTable->pfnSetDefaultSourceRect(This);
}

HRESULT WINAPI BaseControlVideoImpl_SetDestinationPosition(IBasicVideo *iface, LONG Left, LONG Top, LONG Width, LONG Height)
{
    RECT DestRect;
    BaseControlVideo *This = impl_from_IBasicVideo(iface);

    TRACE("(%p/%p)->(%d, %d, %d, %d)\n", This, iface, Left, Top, Width, Height);

    SetRect(&DestRect, Left, Top, Left + Width, Top + Height);
    if (FAILED(BaseControlVideoImpl_CheckTargetRect(This, &DestRect)))
        return E_INVALIDARG;
    return This->pFuncsTable->pfnSetTargetRect(This, &DestRect);
}

HRESULT WINAPI BaseControlVideoImpl_GetDestinationPosition(IBasicVideo *iface, LONG *pLeft, LONG *pTop, LONG *pWidth, LONG *pHeight)
{
    RECT DestRect;
    BaseControlVideo *This = impl_from_IBasicVideo(iface);

    TRACE("(%p/%p)->(%p, %p, %p, %p)\n", This, iface, pLeft, pTop, pWidth, pHeight);
    if (!pLeft || !pTop || !pWidth || !pHeight)
        return E_POINTER;
    This->pFuncsTable->pfnGetTargetRect(This, &DestRect);

    *pLeft = DestRect.left;
    *pTop = DestRect.top;
    *pWidth = DestRect.right - DestRect.left;
    *pHeight = DestRect.bottom - DestRect.top;

    return S_OK;
}

HRESULT WINAPI BaseControlVideoImpl_SetDefaultDestinationPosition(IBasicVideo *iface)
{
    BaseControlVideo *This = impl_from_IBasicVideo(iface);

    TRACE("(%p/%p)->()\n", This, iface);
    return This->pFuncsTable->pfnSetDefaultTargetRect(This);
}

HRESULT WINAPI BaseControlVideoImpl_GetVideoSize(IBasicVideo *iface, LONG *pWidth, LONG *pHeight)
{
    VIDEOINFOHEADER *vih;
    BaseControlVideo *This = impl_from_IBasicVideo(iface);

    TRACE("(%p/%p)->(%p, %p)\n", This, iface, pWidth, pHeight);
    if (!pWidth || !pHeight)
        return E_POINTER;

    vih = This->pFuncsTable->pfnGetVideoFormat(This);
    *pHeight = vih->bmiHeader.biHeight;
    *pWidth = vih->bmiHeader.biWidth;

    return S_OK;
}

HRESULT WINAPI BaseControlVideoImpl_GetVideoPaletteEntries(IBasicVideo *iface, LONG StartIndex, LONG Entries, LONG *pRetrieved, LONG *pPalette)
{
    BaseControlVideo *This = impl_from_IBasicVideo(iface);

    TRACE("(%p/%p)->(%d, %d, %p, %p)\n", This, iface, StartIndex, Entries, pRetrieved, pPalette);
    if (!pRetrieved || !pPalette)
        return E_POINTER;

    *pRetrieved = 0;
    return VFW_E_NO_PALETTE_AVAILABLE;
}

HRESULT WINAPI BaseControlVideoImpl_GetCurrentImage(IBasicVideo *iface, LONG *pBufferSize, LONG *pDIBImage)
{
    BaseControlVideo *This = impl_from_IBasicVideo(iface);
    if (!pBufferSize || !pDIBImage)
        return E_POINTER;

    return This->pFuncsTable->pfnGetStaticImage(This, pBufferSize, pDIBImage);
}

HRESULT WINAPI BaseControlVideoImpl_IsUsingDefaultSource(IBasicVideo *iface)
{
    BaseControlVideo *This = impl_from_IBasicVideo(iface);

    return This->pFuncsTable->pfnIsDefaultSourceRect(This);
}

HRESULT WINAPI BaseControlVideoImpl_IsUsingDefaultDestination(IBasicVideo *iface)
{
    BaseControlVideo *This = impl_from_IBasicVideo(iface);

    return This->pFuncsTable->pfnIsDefaultTargetRect(This);
}
