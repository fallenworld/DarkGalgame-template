 /*
 * Copyright 2011 Jacek Caban for CodeWeavers
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

#include "windef.h"
#include "winbase.h"
#include "winuser.h"
#include "ole2.h"
#include "mshtmdid.h"

#include "wine/debug.h"

#include "mshtml_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(mshtml);

struct HTMLTitleElement {
    HTMLElement element;

    IHTMLTitleElement IHTMLTitleElement_iface;
};

static inline HTMLTitleElement *impl_from_IHTMLTitleElement(IHTMLTitleElement *iface)
{
    return CONTAINING_RECORD(iface, HTMLTitleElement, IHTMLTitleElement_iface);
}

static HRESULT WINAPI HTMLTitleElement_QueryInterface(IHTMLTitleElement *iface,
                                                         REFIID riid, void **ppv)
{
    HTMLTitleElement *This = impl_from_IHTMLTitleElement(iface);

    return IHTMLDOMNode_QueryInterface(&This->element.node.IHTMLDOMNode_iface, riid, ppv);
}

static ULONG WINAPI HTMLTitleElement_AddRef(IHTMLTitleElement *iface)
{
    HTMLTitleElement *This = impl_from_IHTMLTitleElement(iface);

    return IHTMLDOMNode_AddRef(&This->element.node.IHTMLDOMNode_iface);
}

static ULONG WINAPI HTMLTitleElement_Release(IHTMLTitleElement *iface)
{
    HTMLTitleElement *This = impl_from_IHTMLTitleElement(iface);

    return IHTMLDOMNode_Release(&This->element.node.IHTMLDOMNode_iface);
}

static HRESULT WINAPI HTMLTitleElement_GetTypeInfoCount(IHTMLTitleElement *iface, UINT *pctinfo)
{
    HTMLTitleElement *This = impl_from_IHTMLTitleElement(iface);

    return IDispatchEx_GetTypeInfoCount(&This->element.node.event_target.dispex.IDispatchEx_iface, pctinfo);
}

static HRESULT WINAPI HTMLTitleElement_GetTypeInfo(IHTMLTitleElement *iface, UINT iTInfo,
        LCID lcid, ITypeInfo **ppTInfo)
{
    HTMLTitleElement *This = impl_from_IHTMLTitleElement(iface);

    return IDispatchEx_GetTypeInfo(&This->element.node.event_target.dispex.IDispatchEx_iface, iTInfo, lcid,
            ppTInfo);
}

static HRESULT WINAPI HTMLTitleElement_GetIDsOfNames(IHTMLTitleElement *iface, REFIID riid,
        LPOLESTR *rgszNames, UINT cNames, LCID lcid, DISPID *rgDispId)
{
    HTMLTitleElement *This = impl_from_IHTMLTitleElement(iface);

    return IDispatchEx_GetIDsOfNames(&This->element.node.event_target.dispex.IDispatchEx_iface, riid, rgszNames,
            cNames, lcid, rgDispId);
}

static HRESULT WINAPI HTMLTitleElement_Invoke(IHTMLTitleElement *iface, DISPID dispIdMember,
        REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *pDispParams,
        VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr)
{
    HTMLTitleElement *This = impl_from_IHTMLTitleElement(iface);

    return IDispatchEx_Invoke(&This->element.node.event_target.dispex.IDispatchEx_iface, dispIdMember, riid,
            lcid, wFlags, pDispParams, pVarResult, pExcepInfo, puArgErr);
}

static HRESULT WINAPI HTMLTitleElement_put_text(IHTMLTitleElement *iface, BSTR v)
{
    HTMLTitleElement *This = impl_from_IHTMLTitleElement(iface);
    FIXME("(%p)->(%s)\n", This, debugstr_w(v));
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLTitleElement_get_text(IHTMLTitleElement *iface, BSTR *p)
{
    HTMLTitleElement *This = impl_from_IHTMLTitleElement(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static const IHTMLTitleElementVtbl HTMLTitleElementVtbl = {
    HTMLTitleElement_QueryInterface,
    HTMLTitleElement_AddRef,
    HTMLTitleElement_Release,
    HTMLTitleElement_GetTypeInfoCount,
    HTMLTitleElement_GetTypeInfo,
    HTMLTitleElement_GetIDsOfNames,
    HTMLTitleElement_Invoke,
    HTMLTitleElement_put_text,
    HTMLTitleElement_get_text
};

static inline HTMLTitleElement *HTMLTitleElement_from_HTMLDOMNode(HTMLDOMNode *iface)
{
    return CONTAINING_RECORD(iface, HTMLTitleElement, element.node);
}

static HRESULT HTMLTitleElement_QI(HTMLDOMNode *iface, REFIID riid, void **ppv)
{
    HTMLTitleElement *This = HTMLTitleElement_from_HTMLDOMNode(iface);

    TRACE("(%p)->(%s %p)\n", This, debugstr_mshtml_guid(riid), ppv);

    if(IsEqualGUID(&IID_IHTMLTitleElement, riid))
        *ppv = &This->IHTMLTitleElement_iface;
    else
        return HTMLElement_QI(&This->element.node, riid, ppv);

    IUnknown_AddRef((IUnknown*)*ppv);
    return S_OK;
}

static void HTMLTitleElement_destructor(HTMLDOMNode *iface)
{
    HTMLTitleElement *This = HTMLTitleElement_from_HTMLDOMNode(iface);

    HTMLElement_destructor(&This->element.node);
}

static const NodeImplVtbl HTMLTitleElementImplVtbl = {
    &CLSID_HTMLTitleElement,
    HTMLTitleElement_QI,
    HTMLTitleElement_destructor,
    HTMLElement_cpc,
    HTMLElement_clone,
    HTMLElement_handle_event,
    HTMLElement_get_attr_col
};

static const tid_t HTMLTitleElement_iface_tids[] = {
    HTMLELEMENT_TIDS,
    IHTMLTitleElement_tid,
    0
};
static dispex_static_data_t HTMLTitleElement_dispex = {
    NULL,
    DispHTMLTitleElement_tid,
    HTMLTitleElement_iface_tids,
    HTMLElement_init_dispex_info
};

HRESULT HTMLTitleElement_Create(HTMLDocumentNode *doc, nsIDOMHTMLElement *nselem, HTMLElement **elem)
{
    HTMLTitleElement *ret;

    ret = heap_alloc_zero(sizeof(*ret));
    if(!ret)
        return E_OUTOFMEMORY;

    ret->IHTMLTitleElement_iface.lpVtbl = &HTMLTitleElementVtbl;
    ret->element.node.vtbl = &HTMLTitleElementImplVtbl;

    HTMLElement_Init(&ret->element, doc, nselem, &HTMLTitleElement_dispex);

    *elem = &ret->element;
    return S_OK;
}

struct HTMLHtmlElement {
    HTMLElement element;

    IHTMLHtmlElement IHTMLHtmlElement_iface;
};

static inline HTMLHtmlElement *impl_from_IHTMLHtmlElement(IHTMLHtmlElement *iface)
{
    return CONTAINING_RECORD(iface, HTMLHtmlElement, IHTMLHtmlElement_iface);
}

static HRESULT WINAPI HTMLHtmlElement_QueryInterface(IHTMLHtmlElement *iface,
                                                         REFIID riid, void **ppv)
{
    HTMLHtmlElement *This = impl_from_IHTMLHtmlElement(iface);

    return IHTMLDOMNode_QueryInterface(&This->element.node.IHTMLDOMNode_iface, riid, ppv);
}

static ULONG WINAPI HTMLHtmlElement_AddRef(IHTMLHtmlElement *iface)
{
    HTMLHtmlElement *This = impl_from_IHTMLHtmlElement(iface);

    return IHTMLDOMNode_AddRef(&This->element.node.IHTMLDOMNode_iface);
}

static ULONG WINAPI HTMLHtmlElement_Release(IHTMLHtmlElement *iface)
{
    HTMLHtmlElement *This = impl_from_IHTMLHtmlElement(iface);

    return IHTMLDOMNode_Release(&This->element.node.IHTMLDOMNode_iface);
}

static HRESULT WINAPI HTMLHtmlElement_GetTypeInfoCount(IHTMLHtmlElement *iface, UINT *pctinfo)
{
    HTMLHtmlElement *This = impl_from_IHTMLHtmlElement(iface);

    return IDispatchEx_GetTypeInfoCount(&This->element.node.event_target.dispex.IDispatchEx_iface, pctinfo);
}

static HRESULT WINAPI HTMLHtmlElement_GetTypeInfo(IHTMLHtmlElement *iface, UINT iTInfo,
                                              LCID lcid, ITypeInfo **ppTInfo)
{
    HTMLHtmlElement *This = impl_from_IHTMLHtmlElement(iface);

    return IDispatchEx_GetTypeInfo(&This->element.node.event_target.dispex.IDispatchEx_iface, iTInfo, lcid,
            ppTInfo);
}

static HRESULT WINAPI HTMLHtmlElement_GetIDsOfNames(IHTMLHtmlElement *iface, REFIID riid,
                                                LPOLESTR *rgszNames, UINT cNames,
                                                LCID lcid, DISPID *rgDispId)
{
    HTMLHtmlElement *This = impl_from_IHTMLHtmlElement(iface);

    return IDispatchEx_GetIDsOfNames(&This->element.node.event_target.dispex.IDispatchEx_iface, riid, rgszNames,
            cNames, lcid, rgDispId);
}

static HRESULT WINAPI HTMLHtmlElement_Invoke(IHTMLHtmlElement *iface, DISPID dispIdMember,
                            REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *pDispParams,
                            VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr)
{
    HTMLHtmlElement *This = impl_from_IHTMLHtmlElement(iface);

    return IDispatchEx_Invoke(&This->element.node.event_target.dispex.IDispatchEx_iface, dispIdMember, riid,
            lcid, wFlags, pDispParams, pVarResult, pExcepInfo, puArgErr);
}

static HRESULT WINAPI HTMLHtmlElement_put_version(IHTMLHtmlElement *iface, BSTR v)
{
    HTMLHtmlElement *This = impl_from_IHTMLHtmlElement(iface);
    FIXME("(%p)->(%s)\n", This, debugstr_w(v));
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLHtmlElement_get_version(IHTMLHtmlElement *iface, BSTR *p)
{
    HTMLHtmlElement *This = impl_from_IHTMLHtmlElement(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static const IHTMLHtmlElementVtbl HTMLHtmlElementVtbl = {
    HTMLHtmlElement_QueryInterface,
    HTMLHtmlElement_AddRef,
    HTMLHtmlElement_Release,
    HTMLHtmlElement_GetTypeInfoCount,
    HTMLHtmlElement_GetTypeInfo,
    HTMLHtmlElement_GetIDsOfNames,
    HTMLHtmlElement_Invoke,
    HTMLHtmlElement_put_version,
    HTMLHtmlElement_get_version
};

static inline HTMLHtmlElement *HTMLHtmlElement_from_HTMLDOMNode(HTMLDOMNode *iface)
{
    return CONTAINING_RECORD(iface, HTMLHtmlElement, element.node);
}

static HRESULT HTMLHtmlElement_QI(HTMLDOMNode *iface, REFIID riid, void **ppv)
{
    HTMLHtmlElement *This = HTMLHtmlElement_from_HTMLDOMNode(iface);

    TRACE("(%p)->(%s %p)\n", This, debugstr_mshtml_guid(riid), ppv);

    if(IsEqualGUID(&IID_IHTMLHtmlElement, riid))
        *ppv = &This->IHTMLHtmlElement_iface;
    else
        return HTMLElement_QI(&This->element.node, riid, ppv);

    IUnknown_AddRef((IUnknown*)*ppv);
    return S_OK;
}

static void HTMLHtmlElement_destructor(HTMLDOMNode *iface)
{
    HTMLHtmlElement *This = HTMLHtmlElement_from_HTMLDOMNode(iface);

    HTMLElement_destructor(&This->element.node);
}

static BOOL HTMLHtmlElement_is_settable(HTMLDOMNode *iface, DISPID dispid)
{
    switch(dispid) {
    case DISPID_IHTMLELEMENT_OUTERTEXT:
        return FALSE;
    default:
        return TRUE;
    }
}

static const NodeImplVtbl HTMLHtmlElementImplVtbl = {
    &CLSID_HTMLHtmlElement,
    HTMLHtmlElement_QI,
    HTMLHtmlElement_destructor,
    HTMLElement_cpc,
    HTMLElement_clone,
    HTMLElement_handle_event,
    HTMLElement_get_attr_col,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    HTMLHtmlElement_is_settable
};

static const tid_t HTMLHtmlElement_iface_tids[] = {
    HTMLELEMENT_TIDS,
    IHTMLHtmlElement_tid,
    0
};
static dispex_static_data_t HTMLHtmlElement_dispex = {
    NULL,
    DispHTMLHtmlElement_tid,
    HTMLHtmlElement_iface_tids,
    HTMLElement_init_dispex_info
};

HRESULT HTMLHtmlElement_Create(HTMLDocumentNode *doc, nsIDOMHTMLElement *nselem, HTMLElement **elem)
{
    HTMLHtmlElement *ret;

    ret = heap_alloc_zero(sizeof(*ret));
    if(!ret)
        return E_OUTOFMEMORY;

    ret->IHTMLHtmlElement_iface.lpVtbl = &HTMLHtmlElementVtbl;
    ret->element.node.vtbl = &HTMLHtmlElementImplVtbl;

    HTMLElement_Init(&ret->element, doc, nselem, &HTMLHtmlElement_dispex);

    *elem = &ret->element;
    return S_OK;
}

struct HTMLHeadElement {
    HTMLElement element;

    IHTMLHeadElement IHTMLHeadElement_iface;
};

static inline HTMLHeadElement *impl_from_IHTMLHeadElement(IHTMLHeadElement *iface)
{
    return CONTAINING_RECORD(iface, HTMLHeadElement, IHTMLHeadElement_iface);
}

static HRESULT WINAPI HTMLHeadElement_QueryInterface(IHTMLHeadElement *iface,
                                                         REFIID riid, void **ppv)
{
    HTMLHeadElement *This = impl_from_IHTMLHeadElement(iface);

    return IHTMLDOMNode_QueryInterface(&This->element.node.IHTMLDOMNode_iface, riid, ppv);
}

static ULONG WINAPI HTMLHeadElement_AddRef(IHTMLHeadElement *iface)
{
    HTMLHeadElement *This = impl_from_IHTMLHeadElement(iface);

    return IHTMLDOMNode_AddRef(&This->element.node.IHTMLDOMNode_iface);
}

static ULONG WINAPI HTMLHeadElement_Release(IHTMLHeadElement *iface)
{
    HTMLHeadElement *This = impl_from_IHTMLHeadElement(iface);

    return IHTMLDOMNode_Release(&This->element.node.IHTMLDOMNode_iface);
}

static HRESULT WINAPI HTMLHeadElement_GetTypeInfoCount(IHTMLHeadElement *iface, UINT *pctinfo)
{
    HTMLHeadElement *This = impl_from_IHTMLHeadElement(iface);

    return IDispatchEx_GetTypeInfoCount(&This->element.node.event_target.dispex.IDispatchEx_iface, pctinfo);
}

static HRESULT WINAPI HTMLHeadElement_GetTypeInfo(IHTMLHeadElement *iface, UINT iTInfo,
                                              LCID lcid, ITypeInfo **ppTInfo)
{
    HTMLHeadElement *This = impl_from_IHTMLHeadElement(iface);

    return IDispatchEx_GetTypeInfo(&This->element.node.event_target.dispex.IDispatchEx_iface, iTInfo, lcid,
            ppTInfo);
}

static HRESULT WINAPI HTMLHeadElement_GetIDsOfNames(IHTMLHeadElement *iface, REFIID riid,
                                                LPOLESTR *rgszNames, UINT cNames,
                                                LCID lcid, DISPID *rgDispId)
{
    HTMLHeadElement *This = impl_from_IHTMLHeadElement(iface);

    return IDispatchEx_GetIDsOfNames(&This->element.node.event_target.dispex.IDispatchEx_iface, riid, rgszNames,
            cNames, lcid, rgDispId);
}

static HRESULT WINAPI HTMLHeadElement_Invoke(IHTMLHeadElement *iface, DISPID dispIdMember,
                            REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *pDispParams,
                            VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr)
{
    HTMLHeadElement *This = impl_from_IHTMLHeadElement(iface);

    return IDispatchEx_Invoke(&This->element.node.event_target.dispex.IDispatchEx_iface, dispIdMember, riid,
            lcid, wFlags, pDispParams, pVarResult, pExcepInfo, puArgErr);
}

static HRESULT WINAPI HTMLHeadElement_put_profile(IHTMLHeadElement *iface, BSTR v)
{
    HTMLHeadElement *This = impl_from_IHTMLHeadElement(iface);
    FIXME("(%p)->(%s)\n", This, debugstr_w(v));
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLHeadElement_get_profile(IHTMLHeadElement *iface, BSTR *p)
{
    HTMLHeadElement *This = impl_from_IHTMLHeadElement(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static const IHTMLHeadElementVtbl HTMLHeadElementVtbl = {
    HTMLHeadElement_QueryInterface,
    HTMLHeadElement_AddRef,
    HTMLHeadElement_Release,
    HTMLHeadElement_GetTypeInfoCount,
    HTMLHeadElement_GetTypeInfo,
    HTMLHeadElement_GetIDsOfNames,
    HTMLHeadElement_Invoke,
    HTMLHeadElement_put_profile,
    HTMLHeadElement_get_profile
};

static inline HTMLHeadElement *impl_from_HTMLDOMNode(HTMLDOMNode *iface)
{
    return CONTAINING_RECORD(iface, HTMLHeadElement, element.node);
}

static HRESULT HTMLHeadElement_QI(HTMLDOMNode *iface, REFIID riid, void **ppv)
{
    HTMLHeadElement *This = impl_from_HTMLDOMNode(iface);

    TRACE("(%p)->(%s %p)\n", This, debugstr_mshtml_guid(riid), ppv);

    if(IsEqualGUID(&IID_IHTMLHeadElement, riid))
        *ppv = &This->IHTMLHeadElement_iface;
    else
        return HTMLElement_QI(&This->element.node, riid, ppv);

    IUnknown_AddRef((IUnknown*)*ppv);
    return S_OK;
}

static void HTMLHeadElement_destructor(HTMLDOMNode *iface)
{
    HTMLHeadElement *This = impl_from_HTMLDOMNode(iface);

    HTMLElement_destructor(&This->element.node);
}

static const NodeImplVtbl HTMLHeadElementImplVtbl = {
    &CLSID_HTMLHeadElement,
    HTMLHeadElement_QI,
    HTMLHeadElement_destructor,
    HTMLElement_cpc,
    HTMLElement_clone,
    HTMLElement_handle_event,
    HTMLElement_get_attr_col
};

static const tid_t HTMLHeadElement_iface_tids[] = {
    HTMLELEMENT_TIDS,
    IHTMLHeadElement_tid,
    0
};
static dispex_static_data_t HTMLHeadElement_dispex = {
    NULL,
    DispHTMLHeadElement_tid,
    HTMLHeadElement_iface_tids,
    HTMLElement_init_dispex_info
};

HRESULT HTMLHeadElement_Create(HTMLDocumentNode *doc, nsIDOMHTMLElement *nselem, HTMLElement **elem)
{
    HTMLHeadElement *ret;

    ret = heap_alloc_zero(sizeof(*ret));
    if(!ret)
        return E_OUTOFMEMORY;

    ret->IHTMLHeadElement_iface.lpVtbl = &HTMLHeadElementVtbl;
    ret->element.node.vtbl = &HTMLHeadElementImplVtbl;

    HTMLElement_Init(&ret->element, doc, nselem, &HTMLHeadElement_dispex);

    *elem = &ret->element;
    return S_OK;
}
