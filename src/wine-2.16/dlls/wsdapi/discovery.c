/*
 * Web Services on Devices
 *
 * Copyright 2017 Owen Rudge for CodeWeavers
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

#include <stdarg.h>

#define COBJMACROS
#define INITGUID

#include "windef.h"
#include "winbase.h"
#include "wine/debug.h"
#include "wine/list.h"
#include "objbase.h"
#include "guiddef.h"
#include "wsdapi.h"

WINE_DEFAULT_DEBUG_CHANNEL(wsdapi);

struct notificationSink
{
    struct list entry;
    IWSDiscoveryPublisherNotify *notificationSink;
};

typedef struct IWSDiscoveryPublisherImpl {
    IWSDiscoveryPublisher IWSDiscoveryPublisher_iface;
    LONG                  ref;
    IWSDXMLContext        *xmlContext;
    DWORD                 addressFamily;
    struct list           notificationSinks;
} IWSDiscoveryPublisherImpl;

static inline IWSDiscoveryPublisherImpl *impl_from_IWSDiscoveryPublisher(IWSDiscoveryPublisher *iface)
{
    return CONTAINING_RECORD(iface, IWSDiscoveryPublisherImpl, IWSDiscoveryPublisher_iface);
}

static HRESULT WINAPI IWSDiscoveryPublisherImpl_QueryInterface(IWSDiscoveryPublisher *iface, REFIID riid, void **ppv)
{
    IWSDiscoveryPublisherImpl *This = impl_from_IWSDiscoveryPublisher(iface);

    TRACE("(%p, %s, %p)\n", This, debugstr_guid(riid), ppv);

    if (!ppv)
    {
        WARN("Invalid parameter\n");
        return E_INVALIDARG;
    }

    *ppv = NULL;

    if (IsEqualIID(riid, &IID_IUnknown) ||
        IsEqualIID(riid, &IID_IWSDiscoveryPublisher))
    {
        *ppv = &This->IWSDiscoveryPublisher_iface;
    }
    else
    {
        WARN("Unknown IID %s\n", debugstr_guid(riid));
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown*)*ppv);
    return S_OK;
}

static ULONG WINAPI IWSDiscoveryPublisherImpl_AddRef(IWSDiscoveryPublisher *iface)
{
    IWSDiscoveryPublisherImpl *This = impl_from_IWSDiscoveryPublisher(iface);
    ULONG ref = InterlockedIncrement(&This->ref);

    TRACE("(%p) ref=%d\n", This, ref);
    return ref;
}

static ULONG WINAPI IWSDiscoveryPublisherImpl_Release(IWSDiscoveryPublisher *iface)
{
    IWSDiscoveryPublisherImpl *This = impl_from_IWSDiscoveryPublisher(iface);
    ULONG ref = InterlockedDecrement(&This->ref);
    struct notificationSink *sink, *cursor;

    TRACE("(%p) ref=%d\n", This, ref);

    if (ref == 0)
    {
        if (This->xmlContext != NULL)
        {
            IWSDXMLContext_Release(This->xmlContext);
        }

        LIST_FOR_EACH_ENTRY_SAFE(sink, cursor, &This->notificationSinks, struct notificationSink, entry)
        {
            IWSDiscoveryPublisherNotify_Release(sink->notificationSink);
            list_remove(&sink->entry);
            HeapFree(GetProcessHeap(), 0, sink);
        }

        HeapFree(GetProcessHeap(), 0, This);
    }

    return ref;
}

static HRESULT WINAPI IWSDiscoveryPublisherImpl_SetAddressFamily(IWSDiscoveryPublisher *This, DWORD dwAddressFamily)
{
    IWSDiscoveryPublisherImpl *impl = impl_from_IWSDiscoveryPublisher(This);

    TRACE("(%p, %d)\n", This, dwAddressFamily);

    /* Has the address family already been set? */
    if (impl->addressFamily != 0)
    {
        return STG_E_INVALIDFUNCTION;
    }

    if ((dwAddressFamily == WSDAPI_ADDRESSFAMILY_IPV4) || (dwAddressFamily == WSDAPI_ADDRESSFAMILY_IPV6) ||
        (dwAddressFamily == (WSDAPI_ADDRESSFAMILY_IPV4 | WSDAPI_ADDRESSFAMILY_IPV6)))
    {
        /* TODO: Check that the address family is supported by the system */
        impl->addressFamily = dwAddressFamily;
        return S_OK;
    }

    return E_INVALIDARG;
}

static HRESULT WINAPI IWSDiscoveryPublisherImpl_RegisterNotificationSink(IWSDiscoveryPublisher *This, IWSDiscoveryPublisherNotify *pSink)
{
    IWSDiscoveryPublisherImpl *impl = impl_from_IWSDiscoveryPublisher(This);
    struct notificationSink *sink;

    TRACE("(%p, %p)\n", This, pSink);

    if (pSink == NULL)
    {
        return E_INVALIDARG;
    }

    sink = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*sink));

    if (!sink)
    {
        return E_OUTOFMEMORY;
    }

    sink->notificationSink = pSink;
    IWSDiscoveryPublisherNotify_AddRef(pSink);

    list_add_tail(&impl->notificationSinks, &sink->entry);

    return S_OK;
}

static HRESULT WINAPI IWSDiscoveryPublisherImpl_UnRegisterNotificationSink(IWSDiscoveryPublisher *This, IWSDiscoveryPublisherNotify *pSink)
{
    IWSDiscoveryPublisherImpl *impl = impl_from_IWSDiscoveryPublisher(This);
    struct notificationSink *sink;

    TRACE("(%p, %p)\n", This, pSink);

    if (pSink == NULL)
    {
        return E_INVALIDARG;
    }

    LIST_FOR_EACH_ENTRY(sink, &impl->notificationSinks, struct notificationSink, entry)
    {
        if (sink->notificationSink == pSink)
        {
            IWSDiscoveryPublisherNotify_Release(pSink);
            list_remove(&sink->entry);
            HeapFree(GetProcessHeap(), 0, sink);

            return S_OK;
        }
    }

    /* Notification sink is not registered */
    return E_FAIL;
}

static HRESULT WINAPI IWSDiscoveryPublisherImpl_Publish(IWSDiscoveryPublisher *This, LPCWSTR pszId, ULONGLONG ullMetadataVersion, ULONGLONG ullInstanceId,
                                                        ULONGLONG ullMessageNumber, LPCWSTR pszSessionId, const WSD_NAME_LIST *pTypesList,
                                                        const WSD_URI_LIST *pScopesList, const WSD_URI_LIST *pXAddrsList)
{
    FIXME("(%p, %s, %s, %s, %s, %s, %p, %p, %p)\n", This, debugstr_w(pszId), wine_dbgstr_longlong(ullMetadataVersion), wine_dbgstr_longlong(ullInstanceId),
        wine_dbgstr_longlong(ullMessageNumber), debugstr_w(pszSessionId), pTypesList, pScopesList, pXAddrsList);

    return E_NOTIMPL;
}

static HRESULT WINAPI IWSDiscoveryPublisherImpl_UnPublish(IWSDiscoveryPublisher *This, LPCWSTR pszId, ULONGLONG ullInstanceId, ULONGLONG ullMessageNumber,
                                                          LPCWSTR pszSessionId, const WSDXML_ELEMENT *pAny)
{
    FIXME("(%p, %s, %s, %s, %s, %p)\n", This, debugstr_w(pszId), wine_dbgstr_longlong(ullInstanceId), wine_dbgstr_longlong(ullMessageNumber),
        debugstr_w(pszSessionId), pAny);

    return E_NOTIMPL;
}

static HRESULT WINAPI IWSDiscoveryPublisherImpl_MatchProbe(IWSDiscoveryPublisher *This, const WSD_SOAP_MESSAGE *pProbeMessage,
                                                           IWSDMessageParameters *pMessageParameters, LPCWSTR pszId, ULONGLONG ullMetadataVersion,
                                                           ULONGLONG ullInstanceId, ULONGLONG ullMessageNumber, LPCWSTR pszSessionId,
                                                           const WSD_NAME_LIST *pTypesList, const WSD_URI_LIST *pScopesList,
                                                           const WSD_URI_LIST *pXAddrsList)
{
    FIXME("(%p, %p, %p, %s, %s, %s, %s, %s, %p, %p, %p)\n", This, pProbeMessage, pMessageParameters, debugstr_w(pszId),
        wine_dbgstr_longlong(ullMetadataVersion), wine_dbgstr_longlong(ullInstanceId), wine_dbgstr_longlong(ullMessageNumber), debugstr_w(pszSessionId),
        pTypesList, pScopesList, pXAddrsList);

    return E_NOTIMPL;
}

static HRESULT WINAPI IWSDiscoveryPublisherImpl_MatchResolve(IWSDiscoveryPublisher *This, const WSD_SOAP_MESSAGE *pResolveMessage,
                                                             IWSDMessageParameters *pMessageParameters, LPCWSTR pszId, ULONGLONG ullMetadataVersion,
                                                             ULONGLONG ullInstanceId, ULONGLONG ullMessageNumber, LPCWSTR pszSessionId,
                                                             const WSD_NAME_LIST *pTypesList, const WSD_URI_LIST *pScopesList,
                                                             const WSD_URI_LIST *pXAddrsList)
{
    FIXME("(%p, %p, %p, %s, %s, %s, %s, %s, %p, %p, %p)\n", This, pResolveMessage, pMessageParameters, debugstr_w(pszId),
        wine_dbgstr_longlong(ullMetadataVersion), wine_dbgstr_longlong(ullInstanceId), wine_dbgstr_longlong(ullMessageNumber), debugstr_w(pszSessionId),
        pTypesList, pScopesList, pXAddrsList);

    return E_NOTIMPL;
}

static HRESULT WINAPI IWSDiscoveryPublisherImpl_PublishEx(IWSDiscoveryPublisher *This, LPCWSTR pszId, ULONGLONG ullMetadataVersion,
                                                          ULONGLONG ullInstanceId, ULONGLONG ullMessageNumber, LPCWSTR pszSessionId,
                                                          const WSD_NAME_LIST *pTypesList, const WSD_URI_LIST *pScopesList,
                                                          const WSD_URI_LIST *pXAddrsList, const WSDXML_ELEMENT *pHeaderAny,
                                                          const WSDXML_ELEMENT *pReferenceParameterAny, const WSDXML_ELEMENT *pPolicyAny,
                                                          const WSDXML_ELEMENT *pEndpointReferenceAny, const WSDXML_ELEMENT *pAny)
{
    FIXME("(%p, %s, %s, %s, %s, %s, %p, %p, %p, %p, %p, %p, %p, %p)\n", This, debugstr_w(pszId), wine_dbgstr_longlong(ullMetadataVersion),
        wine_dbgstr_longlong(ullInstanceId), wine_dbgstr_longlong(ullMessageNumber), debugstr_w(pszSessionId), pTypesList, pScopesList, pXAddrsList,
        pHeaderAny, pReferenceParameterAny, pPolicyAny, pEndpointReferenceAny, pAny);

    return E_NOTIMPL;
}


static HRESULT WINAPI IWSDiscoveryPublisherImpl_MatchProbeEx(IWSDiscoveryPublisher *This, const WSD_SOAP_MESSAGE *pProbeMessage,
                                                             IWSDMessageParameters *pMessageParameters, LPCWSTR pszId, ULONGLONG ullMetadataVersion,
                                                             ULONGLONG ullInstanceId, ULONGLONG ullMessageNumber, LPCWSTR pszSessionId,
                                                             const WSD_NAME_LIST *pTypesList, const WSD_URI_LIST *pScopesList,
                                                             const WSD_URI_LIST *pXAddrsList, const WSDXML_ELEMENT *pHeaderAny,
                                                             const WSDXML_ELEMENT *pReferenceParameterAny, const WSDXML_ELEMENT *pPolicyAny,
                                                             const WSDXML_ELEMENT *pEndpointReferenceAny, const WSDXML_ELEMENT *pAny)
{
    FIXME("(%p, %p, %p, %s, %s, %s, %s, %s, %p, %p, %p, %p, %p, %p, %p, %p)\n", This, pProbeMessage, pMessageParameters, debugstr_w(pszId),
        wine_dbgstr_longlong(ullMetadataVersion), wine_dbgstr_longlong(ullInstanceId), wine_dbgstr_longlong(ullMessageNumber), debugstr_w(pszSessionId),
        pTypesList, pScopesList, pXAddrsList, pHeaderAny, pReferenceParameterAny, pPolicyAny, pEndpointReferenceAny, pAny);

    return E_NOTIMPL;
}

static HRESULT WINAPI IWSDiscoveryPublisherImpl_MatchResolveEx(IWSDiscoveryPublisher *This, const WSD_SOAP_MESSAGE *pResolveMessage,
                                                               IWSDMessageParameters *pMessageParameters, LPCWSTR pszId, ULONGLONG ullMetadataVersion,
                                                               ULONGLONG ullInstanceId, ULONGLONG ullMessageNumber, LPCWSTR pszSessionId,
                                                               const WSD_NAME_LIST *pTypesList, const WSD_URI_LIST *pScopesList,
                                                               const WSD_URI_LIST *pXAddrsList, const WSDXML_ELEMENT *pHeaderAny,
                                                               const WSDXML_ELEMENT *pReferenceParameterAny, const WSDXML_ELEMENT *pPolicyAny,
                                                               const WSDXML_ELEMENT *pEndpointReferenceAny, const WSDXML_ELEMENT *pAny)
{
    FIXME("(%p, %p, %p, %s, %s, %s, %s, %s, %p, %p, %p, %p, %p, %p, %p, %p)\n", This, pResolveMessage, pMessageParameters, debugstr_w(pszId),
        wine_dbgstr_longlong(ullMetadataVersion), wine_dbgstr_longlong(ullInstanceId), wine_dbgstr_longlong(ullMessageNumber), debugstr_w(pszSessionId),
        pTypesList, pScopesList, pXAddrsList, pHeaderAny, pReferenceParameterAny, pPolicyAny, pEndpointReferenceAny, pAny);

    return E_NOTIMPL;
}

static HRESULT WINAPI IWSDiscoveryPublisherImpl_RegisterScopeMatchingRule(IWSDiscoveryPublisher *This, IWSDScopeMatchingRule *pScopeMatchingRule)
{
    FIXME("(%p, %p)\n", This, pScopeMatchingRule);
    return E_NOTIMPL;
}

static HRESULT WINAPI IWSDiscoveryPublisherImpl_UnRegisterScopeMatchingRule(IWSDiscoveryPublisher *This, IWSDScopeMatchingRule *pScopeMatchingRule)
{
    FIXME("(%p, %p)\n", This, pScopeMatchingRule);
    return E_NOTIMPL;
}

static HRESULT WINAPI IWSDiscoveryPublisherImpl_GetXMLContext(IWSDiscoveryPublisher *This, IWSDXMLContext **ppContext)
{
    IWSDiscoveryPublisherImpl *impl = impl_from_IWSDiscoveryPublisher(This);

    TRACE("%p, %p)\n", This, ppContext);

    if (ppContext == NULL)
        return E_INVALIDARG;

    if (impl->xmlContext != NULL)
    {
        IWSDXMLContext_AddRef(impl->xmlContext);
    }

    *ppContext = impl->xmlContext;
    return S_OK;
}

static const IWSDiscoveryPublisherVtbl publisher_vtbl =
{
    IWSDiscoveryPublisherImpl_QueryInterface,
    IWSDiscoveryPublisherImpl_AddRef,
    IWSDiscoveryPublisherImpl_Release,
    IWSDiscoveryPublisherImpl_SetAddressFamily,
    IWSDiscoveryPublisherImpl_RegisterNotificationSink,
    IWSDiscoveryPublisherImpl_UnRegisterNotificationSink,
    IWSDiscoveryPublisherImpl_Publish,
    IWSDiscoveryPublisherImpl_UnPublish,
    IWSDiscoveryPublisherImpl_MatchProbe,
    IWSDiscoveryPublisherImpl_MatchResolve,
    IWSDiscoveryPublisherImpl_PublishEx,
    IWSDiscoveryPublisherImpl_MatchProbeEx,
    IWSDiscoveryPublisherImpl_MatchResolveEx,
    IWSDiscoveryPublisherImpl_RegisterScopeMatchingRule,
    IWSDiscoveryPublisherImpl_UnRegisterScopeMatchingRule,
    IWSDiscoveryPublisherImpl_GetXMLContext
};

HRESULT WINAPI WSDCreateDiscoveryPublisher(IWSDXMLContext *pContext, IWSDiscoveryPublisher **ppPublisher)
{
    IWSDiscoveryPublisherImpl *obj;

    TRACE("(%p, %p)\n", pContext, ppPublisher);

    if (ppPublisher == NULL)
    {
        WARN("Invalid parameter: ppPublisher == NULL\n");
        return E_POINTER;
    }

    *ppPublisher = NULL;

    obj = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*obj));

    if (!obj)
    {
        WARN("Out of memory\n");
        return E_OUTOFMEMORY;
    }

    obj->IWSDiscoveryPublisher_iface.lpVtbl = &publisher_vtbl;
    obj->ref = 1;

    if (pContext == NULL)
    {
        if (FAILED(WSDXMLCreateContext(&obj->xmlContext)))
        {
            WARN("Unable to create XML context\n");
            HeapFree (GetProcessHeap(), 0, obj);
            return E_OUTOFMEMORY;
        }
    }
    else
    {
        obj->xmlContext = pContext;
        IWSDXMLContext_AddRef(pContext);
    }

    list_init(&obj->notificationSinks);

    *ppPublisher = &obj->IWSDiscoveryPublisher_iface;
    TRACE("Returning iface %p\n", *ppPublisher);

    return S_OK;
}
