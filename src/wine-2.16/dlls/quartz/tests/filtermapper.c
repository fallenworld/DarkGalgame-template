/*
 * Filtermapper unit tests for Quartz
 *
 * Copyright (C) 2008 Alexander Dorofeyev
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

#include "wine/test.h"
#include "winbase.h"
#include "initguid.h"
#include "dshow.h"
#include "winternl.h"

#include "fil_data.h"

DEFINE_GUID(GUID_NULL,0,0,0,0,0,0,0,0,0,0,0);

/* Helper function, checks if filter with given name was enumerated. */
static BOOL enum_find_filter(const WCHAR *wszFilterName, IEnumMoniker *pEnum)
{
    IMoniker *pMoniker = NULL;
    BOOL found = FALSE;
    ULONG nb;
    HRESULT hr;
    static const WCHAR wszFriendlyName[] = {'F','r','i','e','n','d','l','y','N','a','m','e',0};

    while(!found && IEnumMoniker_Next(pEnum, 1, &pMoniker, &nb) == S_OK)
    {
        IPropertyBag * pPropBagCat = NULL;
        VARIANT var;

        VariantInit(&var);

        hr = IMoniker_BindToStorage(pMoniker, NULL, NULL, &IID_IPropertyBag, (LPVOID*)&pPropBagCat);
        ok(SUCCEEDED(hr), "IMoniker_BindToStorage failed with %x\n", hr);
        if (FAILED(hr) || !pPropBagCat)
        {
            VariantClear(&var);
            IMoniker_Release(pMoniker);
            continue;
        }

        hr = IPropertyBag_Read(pPropBagCat, wszFriendlyName, &var, NULL);
        ok(SUCCEEDED(hr), "IPropertyBag_Read failed with %x\n", hr);

        if (SUCCEEDED(hr))
        {
            CHAR val1[512], val2[512];

            WideCharToMultiByte(CP_ACP, 0, V_BSTR(&var), -1, val1, sizeof(val1), 0, 0);
            WideCharToMultiByte(CP_ACP, 0, wszFilterName, -1, val2, sizeof(val2), 0, 0);
            if (!lstrcmpA(val1, val2)) found = TRUE;
        }

        IPropertyBag_Release(pPropBagCat);
        IMoniker_Release(pMoniker);
        VariantClear(&var);
    }

    return found;
}

static void test_fm2_enummatchingfilters(void)
{
    IFilterMapper2 *pMapper = NULL;
    HRESULT hr;
    REGFILTER2 rgf2;
    REGFILTERPINS2 rgPins2[2];
    REGPINTYPES rgPinType;
    static const WCHAR wszFilterName1[] = {'T', 'e', 's', 't', 'f', 'i', 'l', 't', 'e', 'r', '1', 0 };
    static const WCHAR wszFilterName2[] = {'T', 'e', 's', 't', 'f', 'i', 'l', 't', 'e', 'r', '2', 0 };
    CLSID clsidFilter1;
    CLSID clsidFilter2;
    IEnumMoniker *pEnum = NULL;
    BOOL found, registered = TRUE;

    ZeroMemory(&rgf2, sizeof(rgf2));

    hr = CoCreateInstance(&CLSID_FilterMapper2, NULL, CLSCTX_INPROC_SERVER,
            &IID_IFilterMapper2, (LPVOID*)&pMapper);
    ok(hr == S_OK, "CoCreateInstance failed with %x\n", hr);
    if (FAILED(hr)) goto out;

    hr = CoCreateGuid(&clsidFilter1);
    ok(hr == S_OK, "CoCreateGuid failed with %x\n", hr);
    hr = CoCreateGuid(&clsidFilter2);
    ok(hr == S_OK, "CoCreateGuid failed with %x\n", hr);

    /* Test that a test renderer filter is returned when enumerating filters with bRender=FALSE */
    rgf2.dwVersion = 2;
    rgf2.dwMerit = MERIT_UNLIKELY;
    S2(U(rgf2)).cPins2 = 1;
    S2(U(rgf2)).rgPins2 = rgPins2;

    rgPins2[0].dwFlags = REG_PINFLAG_B_RENDERER;
    rgPins2[0].cInstances = 1;
    rgPins2[0].nMediaTypes = 1;
    rgPins2[0].lpMediaType = &rgPinType;
    rgPins2[0].nMediums = 0;
    rgPins2[0].lpMedium = NULL;
    rgPins2[0].clsPinCategory = NULL;

    rgPinType.clsMajorType = &GUID_NULL;
    rgPinType.clsMinorType = &GUID_NULL;

    hr = IFilterMapper2_RegisterFilter(pMapper, &clsidFilter1, wszFilterName1, NULL,
                    &CLSID_LegacyAmFilterCategory, NULL, &rgf2);
    if (hr == E_ACCESSDENIED)
    {
        registered = FALSE;
        skip("Not authorized to register filters\n");
    }
    else
    {
        ok(hr == S_OK, "IFilterMapper2_RegisterFilter failed with %x\n", hr);

        rgPins2[0].dwFlags = 0;

        rgPins2[1].dwFlags = REG_PINFLAG_B_OUTPUT;
        rgPins2[1].cInstances = 1;
        rgPins2[1].nMediaTypes = 1;
        rgPins2[1].lpMediaType = &rgPinType;
        rgPins2[1].nMediums = 0;
        rgPins2[1].lpMedium = NULL;
        rgPins2[1].clsPinCategory = NULL;

        S2(U(rgf2)).cPins2 = 2;

        hr = IFilterMapper2_RegisterFilter(pMapper, &clsidFilter2, wszFilterName2, NULL,
                    &CLSID_LegacyAmFilterCategory, NULL, &rgf2);
        ok(hr == S_OK, "IFilterMapper2_RegisterFilter failed with %x\n", hr);

        hr = IFilterMapper2_EnumMatchingFilters(pMapper, &pEnum, 0, TRUE, MERIT_UNLIKELY, TRUE,
                0, NULL, NULL, &GUID_NULL, FALSE, FALSE, 0, NULL, NULL, &GUID_NULL);
        ok(hr == S_OK, "IFilterMapper2_EnumMatchingFilters failed with %x\n", hr);
        if (SUCCEEDED(hr) && pEnum)
        {
            found = enum_find_filter(wszFilterName1, pEnum);
            ok(found, "EnumMatchingFilters failed to return the test filter 1\n");
        }

        if (pEnum) IEnumMoniker_Release(pEnum);
        pEnum = NULL;

        hr = IFilterMapper2_EnumMatchingFilters(pMapper, &pEnum, 0, TRUE, MERIT_UNLIKELY, TRUE,
                0, NULL, NULL, &GUID_NULL, FALSE, FALSE, 0, NULL, NULL, &GUID_NULL);
        ok(hr == S_OK, "IFilterMapper2_EnumMatchingFilters failed with %x\n", hr);
        if (SUCCEEDED(hr) && pEnum)
        {
            found = enum_find_filter(wszFilterName2, pEnum);
            ok(found, "EnumMatchingFilters failed to return the test filter 2\n");
        }

        if (pEnum) IEnumMoniker_Release(pEnum);
        pEnum = NULL;

        /* Non renderer must not be returned with bRender=TRUE */

        hr = IFilterMapper2_EnumMatchingFilters(pMapper, &pEnum, 0, TRUE, MERIT_UNLIKELY, TRUE,
                0, NULL, NULL, &GUID_NULL, TRUE, FALSE, 0, NULL, NULL, &GUID_NULL);
        ok(hr == S_OK, "IFilterMapper2_EnumMatchingFilters failed with %x\n", hr);

        if (SUCCEEDED(hr) && pEnum)
        {
            found = enum_find_filter(wszFilterName1, pEnum);
            ok(found, "EnumMatchingFilters failed to return the test filter 1\n");
        }
    }

    if (pEnum) IEnumMoniker_Release(pEnum);
    pEnum = NULL;

    hr = IFilterMapper2_EnumMatchingFilters(pMapper, &pEnum, 0, TRUE, MERIT_UNLIKELY, TRUE,
                0, NULL, NULL, &GUID_NULL, TRUE, FALSE, 0, NULL, NULL, &GUID_NULL);
    ok(hr == S_OK, "IFilterMapper2_EnumMatchingFilters failed with %x\n", hr);

    if (SUCCEEDED(hr) && pEnum)
    {
        found = enum_find_filter(wszFilterName2, pEnum);
        ok(!found, "EnumMatchingFilters should not return the test filter 2\n");
    }

    if (registered)
    {
        hr = IFilterMapper2_UnregisterFilter(pMapper, &CLSID_LegacyAmFilterCategory, NULL,
                &clsidFilter1);
        ok(SUCCEEDED(hr), "IFilterMapper2_UnregisterFilter failed with %x\n", hr);

        hr = IFilterMapper2_UnregisterFilter(pMapper, &CLSID_LegacyAmFilterCategory, NULL,
                &clsidFilter2);
        ok(SUCCEEDED(hr), "IFilterMapper2_UnregisterFilter failed with %x\n", hr);
    }

    out:

    if (pEnum) IEnumMoniker_Release(pEnum);
    if (pMapper) IFilterMapper2_Release(pMapper);
}

static void test_legacy_filter_registration(void)
{
    IFilterMapper2 *pMapper2 = NULL;
    IFilterMapper *pMapper = NULL;
    HRESULT hr;
    static const WCHAR wszFilterName[] = {'T', 'e', 's', 't', 'f', 'i', 'l', 't', 'e', 'r', 0 };
    static const CHAR szFilterName[] = "Testfilter";
    static const WCHAR wszPinName[] = {'P', 'i', 'n', '1', 0 };
    CLSID clsidFilter;
    CHAR szRegKey[MAX_PATH];
    static const CHAR szClsid[] = "CLSID";
    WCHAR wszGuidstring[MAX_PATH];
    CHAR szGuidstring[MAX_PATH];
    LONG lRet;
    HKEY hKey = NULL;
    IEnumMoniker *pEnum = NULL;
    BOOL found;
    IEnumRegFilters *pRegEnum = NULL;

    /* Test if legacy filter registration scheme works (filter is added to HKCR\Filter). IFilterMapper_RegisterFilter
     * registers in this way. Filters so registered must then be accessible through both IFilterMapper_EnumMatchingFilters
     * and IFilterMapper2_EnumMatchingFilters. */
    hr = CoCreateInstance(&CLSID_FilterMapper2, NULL, CLSCTX_INPROC_SERVER,
            &IID_IFilterMapper2, (LPVOID*)&pMapper2);
    ok(hr == S_OK, "CoCreateInstance failed with %x\n", hr);
    if (FAILED(hr)) goto out;

    hr = IFilterMapper2_QueryInterface(pMapper2, &IID_IFilterMapper, (void **)&pMapper);
    ok(hr == S_OK, "IFilterMapper2_QueryInterface failed with %x\n", hr);
    if (FAILED(hr)) goto out;

    /* Register a test filter. */
    hr = CoCreateGuid(&clsidFilter);
    ok(hr == S_OK, "CoCreateGuid failed with %x\n", hr);

    lRet = StringFromGUID2(&clsidFilter, wszGuidstring, MAX_PATH);
    ok(lRet > 0, "StringFromGUID2 failed\n");
    if (!lRet) goto out;
    WideCharToMultiByte(CP_ACP, 0, wszGuidstring, -1, szGuidstring, MAX_PATH, 0, 0);

    lstrcpyA(szRegKey, szClsid);
    lstrcatA(szRegKey, "\\");
    lstrcatA(szRegKey, szGuidstring);

    /* Register---- functions need a filter class key to write pin and pin media type data to. Create a bogus
     * class key for it. */
    lRet = RegCreateKeyExA(HKEY_CLASSES_ROOT, szRegKey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL);
    if (lRet == ERROR_ACCESS_DENIED)
        skip("Not authorized to register filters\n");
    else
    {
        ok(lRet == ERROR_SUCCESS, "RegCreateKeyExA failed with %x\n", HRESULT_FROM_WIN32(lRet));

        /* Set default value - this is interpreted as "friendly name" later. */
        lRet = RegSetValueExA(hKey, NULL, 0, REG_SZ, (LPBYTE)szFilterName, lstrlenA(szFilterName) + 1);
        ok(lRet == ERROR_SUCCESS, "RegSetValueExA failed with %x\n", HRESULT_FROM_WIN32(lRet));

        if (hKey) RegCloseKey(hKey);
        hKey = NULL;

        hr = IFilterMapper_RegisterFilter(pMapper, clsidFilter, wszFilterName, MERIT_UNLIKELY);
        ok(hr == S_OK, "IFilterMapper_RegisterFilter failed with %x\n", hr);

        hr = IFilterMapper_RegisterPin(pMapper, clsidFilter, wszPinName, TRUE, FALSE, FALSE, FALSE, GUID_NULL, NULL);
        ok(hr == S_OK, "IFilterMapper_RegisterPin failed with %x\n", hr);

        hr = IFilterMapper_RegisterPinType(pMapper, clsidFilter, wszPinName, GUID_NULL, GUID_NULL);
        ok(hr == S_OK, "IFilterMapper_RegisterPinType failed with %x\n", hr);

        hr = IFilterMapper2_EnumMatchingFilters(pMapper2, &pEnum, 0, TRUE, MERIT_UNLIKELY, TRUE,
                0, NULL, NULL, &GUID_NULL, FALSE, FALSE, 0, NULL, NULL, &GUID_NULL);
        ok(hr == S_OK, "IFilterMapper2_EnumMatchingFilters failed with %x\n", hr);
        if (SUCCEEDED(hr) && pEnum)
        {
            found = enum_find_filter(wszFilterName, pEnum);
            ok(found, "IFilterMapper2_EnumMatchingFilters failed to return the test filter\n");
        }

        if (pEnum) IEnumMoniker_Release(pEnum);
        pEnum = NULL;

        found = FALSE;
        hr = IFilterMapper_EnumMatchingFilters(pMapper, &pRegEnum, MERIT_UNLIKELY, TRUE, GUID_NULL, GUID_NULL,
            FALSE, FALSE, GUID_NULL, GUID_NULL);
        ok(hr == S_OK, "IFilterMapper_EnumMatchingFilters failed with %x\n", hr);
        if (SUCCEEDED(hr) && pRegEnum)
        {
            ULONG cFetched;
            REGFILTER *prgf;

            while(!found && IEnumRegFilters_Next(pRegEnum, 1, &prgf, &cFetched) == S_OK)
            {
                CHAR val[512];

                WideCharToMultiByte(CP_ACP, 0, prgf->Name, -1, val, sizeof(val), 0, 0);
                if (!lstrcmpA(val, szFilterName)) found = TRUE;

                CoTaskMemFree(prgf);
            }

            IEnumRegFilters_Release(pRegEnum);
        }
        ok(found, "IFilterMapper_EnumMatchingFilters failed to return the test filter\n");

        hr = IFilterMapper_UnregisterFilter(pMapper, clsidFilter);
        ok(hr == S_OK, "FilterMapper_UnregisterFilter failed with %x\n", hr);

        lRet = RegOpenKeyExA(HKEY_CLASSES_ROOT, szClsid, 0, KEY_WRITE | DELETE, &hKey);
        ok(lRet == ERROR_SUCCESS, "RegOpenKeyExA failed with %x\n", HRESULT_FROM_WIN32(lRet));

        lRet = RegDeleteKeyA(hKey, szGuidstring);
        ok(lRet == ERROR_SUCCESS, "RegDeleteKeyA failed with %x\n", HRESULT_FROM_WIN32(lRet));
    }

    if (hKey) RegCloseKey(hKey);
    hKey = NULL;

    out:

    if (pMapper) IFilterMapper_Release(pMapper);
    if (pMapper2) IFilterMapper2_Release(pMapper2);
}

static ULONG getRefcount(IUnknown *iface)
{
    IUnknown_AddRef(iface);
    return IUnknown_Release(iface);
}

static void test_ifiltermapper_from_filtergraph(void)
{
    IFilterGraph2* pgraph2 = NULL;
    IFilterMapper2 *pMapper2 = NULL;
    IFilterGraph *filtergraph = NULL;
    HRESULT hr;
    ULONG refcount;

    hr = CoCreateInstance(&CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, &IID_IFilterGraph2, (LPVOID*)&pgraph2);
    ok(hr == S_OK, "CoCreateInstance failed with %08x\n", hr);
    if (!pgraph2) goto out;

    hr = IFilterGraph2_QueryInterface(pgraph2, &IID_IFilterMapper2, (LPVOID*)&pMapper2);
    ok(hr == S_OK, "IFilterGraph2_QueryInterface failed with %08x\n", hr);
    if (!pMapper2) goto out;

    refcount = getRefcount((IUnknown*)pgraph2);
    ok(refcount == 2, "unexpected reference count: %u\n", refcount);
    refcount = getRefcount((IUnknown*)pMapper2);
    ok(refcount == 2, "unexpected reference count: %u\n", refcount);

    IFilterMapper2_AddRef(pMapper2);
    refcount = getRefcount((IUnknown*)pgraph2);
    ok(refcount == 3, "unexpected reference count: %u\n", refcount);
    refcount = getRefcount((IUnknown*)pMapper2);
    ok(refcount == 3, "unexpected reference count: %u\n", refcount);
    IFilterMapper2_Release(pMapper2);

    hr = IFilterMapper2_QueryInterface(pMapper2, &IID_IFilterGraph, (LPVOID*)&filtergraph);
    ok(hr == S_OK, "IFilterMapper2_QueryInterface failed with %08x\n", hr);
    if (!filtergraph) goto out;

    IFilterMapper2_Release(pMapper2);
    pMapper2 = NULL;
    IFilterGraph_Release(filtergraph);
    filtergraph = NULL;

    hr = CoCreateInstance(&CLSID_FilterMapper2, NULL, CLSCTX_INPROC_SERVER, &IID_IFilterMapper2, (LPVOID*)&pMapper2);
    ok(hr == S_OK, "CoCreateInstance failed with %08x\n", hr);
    if (!pMapper2) goto out;

    hr = IFilterMapper2_QueryInterface(pMapper2, &IID_IFilterGraph, (LPVOID*)&filtergraph);
    ok(hr == E_NOINTERFACE, "IFilterMapper2_QueryInterface unexpected result: %08x\n", hr);

    out:

    if (pMapper2) IFilterMapper2_Release(pMapper2);
    if (filtergraph) IFilterGraph_Release(filtergraph);
    if (pgraph2) IFilterGraph2_Release(pgraph2);
}

static void test_register_filter_with_null_clsMinorType(void)
{
    IFilterMapper2 *pMapper = NULL;
    HRESULT hr;
    REGFILTER2 rgf2;
    REGFILTERPINS rgPins;
    REGFILTERPINS2 rgPins2;
    REGPINTYPES rgPinType;
    static WCHAR wszPinName[] = {'P', 'i', 'n', 0 };
    static const WCHAR wszFilterName1[] = {'T', 'e', 's', 't', 'f', 'i', 'l', 't', 'e', 'r', '1', 0 };
    static const WCHAR wszFilterName2[] = {'T', 'e', 's', 't', 'f', 'i', 'l', 't', 'e', 'r', '2', 0 };
    CLSID clsidFilter1;
    CLSID clsidFilter2;

    hr = CoCreateInstance(&CLSID_FilterMapper2, NULL, CLSCTX_INPROC_SERVER,
            &IID_IFilterMapper2, (LPVOID*)&pMapper);
    ok(hr == S_OK, "CoCreateInstance failed with %x\n", hr);
    if (FAILED(hr)) goto out;

    hr = CoCreateGuid(&clsidFilter1);
    ok(hr == S_OK, "CoCreateGuid failed with %x\n", hr);
    hr = CoCreateGuid(&clsidFilter2);
    ok(hr == S_OK, "CoCreateGuid failed with %x\n", hr);

    rgPinType.clsMajorType = &GUID_NULL;
    /* Make sure quartz accepts it without crashing */
    rgPinType.clsMinorType = NULL;

    /* Test with pin descript version 1 */
    ZeroMemory(&rgf2, sizeof(rgf2));
    rgf2.dwVersion = 1;
    rgf2.dwMerit = MERIT_UNLIKELY;
    S1(U(rgf2)).cPins = 1;
    S1(U(rgf2)).rgPins = &rgPins;

    rgPins.strName = wszPinName;
    rgPins.bRendered = 1;
    rgPins.bOutput = 0;
    rgPins.bZero = 0;
    rgPins.bMany = 0;
    rgPins.clsConnectsToFilter = NULL;
    rgPins.strConnectsToPin = NULL;
    rgPins.nMediaTypes = 1;
    rgPins.lpMediaType = &rgPinType;

    hr = IFilterMapper2_RegisterFilter(pMapper, &clsidFilter1, wszFilterName1, NULL,
                    &CLSID_LegacyAmFilterCategory, NULL, &rgf2);
    if (hr == E_ACCESSDENIED)
    {
        skip("Not authorized to register filters\n");
        goto out;
    }
    ok(hr == S_OK, "IFilterMapper2_RegisterFilter failed with %x\n", hr);

    hr = IFilterMapper2_UnregisterFilter(pMapper, &CLSID_LegacyAmFilterCategory, NULL, &clsidFilter1);
    ok(hr == S_OK, "FilterMapper_UnregisterFilter failed with %x\n", hr);

    /* Test with pin descript version 2 */
    ZeroMemory(&rgf2, sizeof(rgf2));
    rgf2.dwVersion = 2;
    rgf2.dwMerit = MERIT_UNLIKELY;
    S2(U(rgf2)).cPins2 = 1;
    S2(U(rgf2)).rgPins2 = &rgPins2;

    rgPins2.dwFlags = REG_PINFLAG_B_RENDERER;
    rgPins2.cInstances = 1;
    rgPins2.nMediaTypes = 1;
    rgPins2.lpMediaType = &rgPinType;
    rgPins2.nMediums = 0;
    rgPins2.lpMedium = NULL;
    rgPins2.clsPinCategory = NULL;

    hr = IFilterMapper2_RegisterFilter(pMapper, &clsidFilter2, wszFilterName2, NULL,
                    &CLSID_LegacyAmFilterCategory, NULL, &rgf2);
    ok(hr == S_OK, "IFilterMapper2_RegisterFilter failed with %x\n", hr);

    hr = IFilterMapper2_UnregisterFilter(pMapper, &CLSID_LegacyAmFilterCategory, NULL, &clsidFilter2);
    ok(hr == S_OK, "FilterMapper_UnregisterFilter failed with %x\n", hr);

    out:

    if (pMapper) IFilterMapper2_Release(pMapper);
}

static void test_parse_filter_data(void)
{
    static const BYTE data_block[] = {
  0x02,0x00,0x00,0x00,0xff,0xff,0x5f,0x00,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x30,0x70,0x69,0x33,
  0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x30,0x74,0x79,0x33,0x00,0x00,0x00,0x00,0x60,0x00,0x00,0x00,0x70,0x00,0x00,0x00,0x31,0x70,0x69,0x33,
  0x08,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x30,0x74,0x79,0x33,0x00,0x00,0x00,0x00,0x60,0x00,0x00,0x00,0x70,0x00,0x00,0x00,0x76,0x69,0x64,0x73,
  0x00,0x00,0x10,0x00,0x80,0x00,0x00,0xaa,0x00,0x38,0x9b,0x71,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};

    BYTE *prgbRegFilter2 = NULL;
    REGFILTER2 *pRegFilter = NULL;
    IFilterMapper2 *pMapper = NULL;
    SAFEARRAYBOUND saBound;
    SAFEARRAY *psa = NULL;
    LPBYTE pbSAData = NULL;
    HRESULT hr;

    IAMFilterData *pData = NULL;

    hr = CoCreateInstance(&CLSID_FilterMapper2, NULL, CLSCTX_INPROC_SERVER,
            &IID_IFilterMapper2, (LPVOID*)&pMapper);
    ok((hr == S_OK || broken(hr != S_OK)), "CoCreateInstance failed with %x\n", hr);
    if (FAILED(hr)) goto out;

    hr = IFilterMapper2_QueryInterface(pMapper, &IID_IAMFilterData, (LPVOID*)&pData);
    ok((hr == S_OK || broken(hr != S_OK)), "Unable to find IID_IAMFilterData interface\n");
    if (FAILED(hr)) goto out;

    saBound.lLbound = 0;
    saBound.cElements = sizeof(data_block);
    psa = SafeArrayCreate(VT_UI1, 1, &saBound);
    ok(psa != NULL, "Unable to create safe array\n");
    if (!psa) goto out;
    hr = SafeArrayAccessData(psa, (LPVOID *)&pbSAData);
    ok(hr == S_OK, "Unable to access array data\n");
    if (FAILED(hr)) goto out;
    memcpy(pbSAData, data_block, sizeof(data_block));

    hr = IAMFilterData_ParseFilterData(pData, pbSAData, sizeof(data_block), &prgbRegFilter2);
    /* We cannot do anything here.  prgbRegFilter2 is very unstable */
    /* Pre Vista, this is a stack pointer so anything that changes the stack invalidats it */
    /* Post Vista, it is a static pointer in the data section of the module */
    pRegFilter =((REGFILTER2**)prgbRegFilter2)[0];
    ok (hr==S_OK,"Failed to Parse filter Data\n");

    ok(IsBadReadPtr(prgbRegFilter2,sizeof(REGFILTER2*))==0,"Bad read pointer returned\n");
    ok(IsBadReadPtr(pRegFilter,sizeof(REGFILTER2))==0,"Bad read pointer for FilterData\n");
    ok(pRegFilter->dwMerit == 0x5fffff,"Incorrect merit returned\n");

out:
    CoTaskMemFree(pRegFilter);
    if (psa)
    {
        SafeArrayUnaccessData(psa);
        SafeArrayDestroy(psa);
    }
    if (pData)
        IAMFilterData_Release(pData);
    if (pMapper)
        IFilterMapper2_Release(pMapper);
}

typedef struct IUnknownImpl
{
    IUnknown IUnknown_iface;
    int AddRef_called;
    int Release_called;
} IUnknownImpl;

static IUnknownImpl *IUnknownImpl_from_iface(IUnknown * iface)
{
    return CONTAINING_RECORD(iface, IUnknownImpl, IUnknown_iface);
}

static HRESULT WINAPI IUnknownImpl_QueryInterface(IUnknown * iface, REFIID riid, LPVOID * ppv)
{
    ok(0, "QueryInterface should not be called for %s\n", wine_dbgstr_guid(riid));
    return E_NOINTERFACE;
}

static ULONG WINAPI IUnknownImpl_AddRef(IUnknown * iface)
{
    IUnknownImpl *This = IUnknownImpl_from_iface(iface);
    This->AddRef_called++;
    return 2;
}

static ULONG WINAPI IUnknownImpl_Release(IUnknown * iface)
{
    IUnknownImpl *This = IUnknownImpl_from_iface(iface);
    This->Release_called++;
    return 1;
}

static CONST_VTBL IUnknownVtbl IUnknownImpl_Vtbl =
{
    IUnknownImpl_QueryInterface,
    IUnknownImpl_AddRef,
    IUnknownImpl_Release
};

static void test_aggregate_filter_mapper(void)
{
    HRESULT hr;
    IUnknown *pmapper;
    IUnknown *punk;
    IUnknownImpl unk_outer = { { &IUnknownImpl_Vtbl }, 0, 0 };

    hr = CoCreateInstance(&CLSID_FilterMapper2, &unk_outer.IUnknown_iface, CLSCTX_INPROC_SERVER,
                          &IID_IUnknown, (void **)&pmapper);
    ok(hr == S_OK, "CoCreateInstance returned %x\n", hr);
    ok(pmapper != &unk_outer.IUnknown_iface, "pmapper = %p, expected not %p\n", pmapper, &unk_outer.IUnknown_iface);

    hr = IUnknown_QueryInterface(pmapper, &IID_IUnknown, (void **)&punk);
    ok(hr == S_OK, "IUnknown_QueryInterface returned %x\n", hr);
    ok(punk != &unk_outer.IUnknown_iface, "punk = %p, expected not %p\n", punk, &unk_outer.IUnknown_iface);
    IUnknown_Release(punk);

    ok(unk_outer.AddRef_called == 0, "IUnknownImpl_AddRef called %d times\n", unk_outer.AddRef_called);
    ok(unk_outer.Release_called == 0, "IUnknownImpl_Release called %d times\n", unk_outer.Release_called);
    unk_outer.AddRef_called = 0;
    unk_outer.Release_called = 0;

    hr = IUnknown_QueryInterface(pmapper, &IID_IFilterMapper, (void **)&punk);
    ok(hr == S_OK, "IUnknown_QueryInterface returned %x\n", hr);
    ok(punk != &unk_outer.IUnknown_iface, "punk = %p, expected not %p\n", punk, &unk_outer.IUnknown_iface);
    IUnknown_Release(punk);

    ok(unk_outer.AddRef_called == 1, "IUnknownImpl_AddRef called %d times\n", unk_outer.AddRef_called);
    ok(unk_outer.Release_called == 1, "IUnknownImpl_Release called %d times\n", unk_outer.Release_called);
    unk_outer.AddRef_called = 0;
    unk_outer.Release_called = 0;

    hr = IUnknown_QueryInterface(pmapper, &IID_IFilterMapper2, (void **)&punk);
    ok(hr == S_OK, "IUnknown_QueryInterface returned %x\n", hr);
    ok(punk != &unk_outer.IUnknown_iface, "punk = %p, expected not %p\n", punk, &unk_outer.IUnknown_iface);
    IUnknown_Release(punk);

    ok(unk_outer.AddRef_called == 1, "IUnknownImpl_AddRef called %d times\n", unk_outer.AddRef_called);
    ok(unk_outer.Release_called == 1, "IUnknownImpl_Release called %d times\n", unk_outer.Release_called);
    unk_outer.AddRef_called = 0;
    unk_outer.Release_called = 0;

    hr = IUnknown_QueryInterface(pmapper, &IID_IFilterMapper3, (void **)&punk);
    ok(hr == S_OK, "IUnknown_QueryInterface returned %x\n", hr);
    ok(punk != &unk_outer.IUnknown_iface, "punk = %p, expected not %p\n", punk, &unk_outer.IUnknown_iface);
    IUnknown_Release(punk);

    ok(unk_outer.AddRef_called == 1, "IUnknownImpl_AddRef called %d times\n", unk_outer.AddRef_called);
    ok(unk_outer.Release_called == 1, "IUnknownImpl_Release called %d times\n", unk_outer.Release_called);

    IUnknown_Release(pmapper);
}

START_TEST(filtermapper)
{
    CoInitialize(NULL);

    test_fm2_enummatchingfilters();
    test_legacy_filter_registration();
    test_ifiltermapper_from_filtergraph();
    test_register_filter_with_null_clsMinorType();
    test_parse_filter_data();
    test_aggregate_filter_mapper();

    CoUninitialize();
}
