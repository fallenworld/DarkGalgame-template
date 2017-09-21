/*
 * Copyright 2012, 2014 Michael Stefaniuc
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

#include <stdarg.h>
#include <windef.h>
#include <wine/test.h>
#include <dmusici.h>
#include <audioclient.h>
#include <guiddef.h>

#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))

static BOOL missing_dmime(void)
{
    IDirectMusicSegment8 *dms;
    HRESULT hr = CoCreateInstance(&CLSID_DirectMusicSegment, NULL, CLSCTX_INPROC_SERVER,
            &IID_IDirectMusicSegment, (void**)&dms);

    if (hr == S_OK && dms)
    {
        IDirectMusicSegment_Release(dms);
        return FALSE;
    }
    return TRUE;
}

static void test_COM_audiopath(void)
{
    IDirectMusicAudioPath *dmap;
    IUnknown *unk;
    IDirectMusicPerformance8 *performance;
    IDirectSoundBuffer *dsound;
    IDirectSoundBuffer8 *dsound8;
    IDirectSoundNotify *notify;
    IDirectSound3DBuffer *dsound3d;
    IKsPropertySet *propset;
    ULONG refcount;
    HRESULT hr;
    DWORD buffer = 0;

    hr = CoCreateInstance(&CLSID_DirectMusicPerformance, NULL, CLSCTX_INPROC_SERVER,
            &IID_IDirectMusicPerformance8, (void**)&performance);
    ok(hr == S_OK || broken(hr == E_NOINTERFACE),
                "DirectMusicPerformance create failed: %08x\n", hr);
    if (!performance) {
        win_skip("IDirectMusicPerformance8 not available\n");
        return;
    }
    hr = IDirectMusicPerformance8_InitAudio(performance, NULL, NULL, NULL,
            DMUS_APATH_SHARED_STEREOPLUSREVERB, 64, DMUS_AUDIOF_ALL, NULL);
    ok(hr == S_OK || hr == DSERR_NODRIVER ||
       broken(hr == AUDCLNT_E_ENDPOINT_CREATE_FAILED), /* Win 10 testbot */
       "DirectMusicPerformance_InitAudio failed: %08x\n", hr);
    if (FAILED(hr)) {
        skip("Audio failed to initialize\n");
        return;
    }
    hr = IDirectMusicPerformance8_GetDefaultAudioPath(performance, &dmap);
    ok(hr == S_OK, "DirectMusicPerformance_GetDefaultAudioPath failed: %08x\n", hr);

    /* IDirectMusicObject and IPersistStream are not supported */
    hr = IDirectMusicAudioPath_QueryInterface(dmap, &IID_IDirectMusicObject, (void**)&unk);
    todo_wine ok(FAILED(hr) && !unk, "Unexpected IDirectMusicObject interface: hr=%08x, iface=%p\n",
            hr, unk);
    if (unk) IUnknown_Release(unk);
    hr = IDirectMusicAudioPath_QueryInterface(dmap, &IID_IPersistStream, (void**)&unk);
    todo_wine ok(FAILED(hr) && !unk, "Unexpected IPersistStream interface: hr=%08x, iface=%p\n",
            hr, unk);
    if (unk) IUnknown_Release(unk);

    /* Same refcount for all DirectMusicAudioPath interfaces */
    refcount = IDirectMusicAudioPath_AddRef(dmap);
    ok(refcount == 3, "refcount == %u, expected 3\n", refcount);

    hr = IDirectMusicAudioPath_QueryInterface(dmap, &IID_IUnknown, (void**)&unk);
    ok(hr == S_OK, "QueryInterface for IID_IUnknown failed: %08x\n", hr);
    ok(unk == (IUnknown*)dmap, "got %p, %p\n", unk, dmap);
    refcount = IUnknown_AddRef(unk);
    ok(refcount == 5, "refcount == %u, expected 5\n", refcount);
    refcount = IUnknown_Release(unk);

    hr = IDirectMusicAudioPath_GetObjectInPath(dmap, DMUS_PCHANNEL_ALL, DMUS_PATH_BUFFER, buffer, &GUID_NULL,
                0, &IID_IDirectSoundBuffer, (void**)&dsound);
    ok(hr == S_OK, "Failed: %08x\n", hr);
    IDirectSoundBuffer_Release(dsound);

    hr = IDirectMusicAudioPath_GetObjectInPath(dmap, DMUS_PCHANNEL_ALL, DMUS_PATH_BUFFER, buffer, &GUID_NULL,
                0, &IID_IDirectSoundBuffer8, (void**)&dsound8);
    ok(hr == S_OK, "Failed: %08x\n", hr);
    IDirectSoundBuffer8_Release(dsound8);

    hr = IDirectMusicAudioPath_GetObjectInPath(dmap, DMUS_PCHANNEL_ALL, DMUS_PATH_BUFFER, buffer, &GUID_NULL,
                0, &IID_IDirectSoundNotify, (void**)&notify);
    ok(hr == E_NOINTERFACE, "Failed: %08x\n", hr);

    hr = IDirectMusicAudioPath_GetObjectInPath(dmap, DMUS_PCHANNEL_ALL, DMUS_PATH_BUFFER, buffer, &GUID_NULL,
                0, &IID_IDirectSound3DBuffer, (void**)&dsound3d);
    ok(hr == E_NOINTERFACE, "Failed: %08x\n", hr);

    hr = IDirectMusicAudioPath_GetObjectInPath(dmap, DMUS_PCHANNEL_ALL, DMUS_PATH_BUFFER, buffer, &GUID_NULL,
                0, &IID_IKsPropertySet, (void**)&propset);
    todo_wine ok(hr == S_OK, "Failed: %08x\n", hr);
    if (propset)
        IKsPropertySet_Release(propset);

    hr = IDirectMusicAudioPath_GetObjectInPath(dmap, DMUS_PCHANNEL_ALL, DMUS_PATH_BUFFER, buffer, &GUID_NULL,
                0, &IID_IUnknown, (void**)&unk);
    ok(hr == S_OK, "Failed: %08x\n", hr);
    IUnknown_Release(unk);

    hr = IDirectMusicAudioPath_GetObjectInPath(dmap, DMUS_PCHANNEL_ALL, DMUS_PATH_BUFFER, buffer, &GUID_NULL,
                0, &GUID_NULL, (void**)&unk);
    ok(hr == E_NOINTERFACE, "Failed: %08x\n", hr);

    while (IDirectMusicAudioPath_Release(dmap) > 1); /* performance has a reference too */
    IDirectMusicPerformance8_CloseDown(performance);
    IDirectMusicPerformance8_Release(performance);
}

static void test_COM_audiopathconfig(void)
{
    IDirectMusicAudioPath *dmap = (IDirectMusicAudioPath*)0xdeadbeef;
    IDirectMusicObject *dmo;
    IPersistStream *ps;
    IUnknown *unk;
    ULONG refcount;
    HRESULT hr;

    /* COM aggregation */
    hr = CoCreateInstance(&CLSID_DirectMusicAudioPathConfig, (IUnknown *)0xdeadbeef, CLSCTX_INPROC_SERVER,
            &IID_IUnknown, (void**)&dmap);
    if (hr == REGDB_E_CLASSNOTREG) {
        win_skip("DirectMusicAudioPathConfig not registered\n");
        return;
    }
    ok(hr == CLASS_E_NOAGGREGATION,
            "DirectMusicAudioPathConfig create failed: %08x, expected CLASS_E_NOAGGREGATION\n", hr);
    ok(!dmap, "dmap = %p\n", dmap);

    /* IDirectMusicAudioPath not supported */
    hr = CoCreateInstance(&CLSID_DirectMusicAudioPathConfig, NULL, CLSCTX_INPROC_SERVER,
            &IID_IDirectMusicAudioPath, (void**)&dmap);
    todo_wine ok(FAILED(hr) && !dmap,
            "Unexpected IDirectMusicAudioPath interface: hr=%08x, iface=%p\n", hr, dmap);

    /* IDirectMusicObject and IPersistStream supported */
    hr = CoCreateInstance(&CLSID_DirectMusicAudioPathConfig, NULL, CLSCTX_INPROC_SERVER,
            &IID_IPersistStream, (void**)&ps);
    ok(hr == S_OK, "DirectMusicObject create failed: %08x, expected S_OK\n", hr);
    IPersistStream_Release(ps);
    hr = CoCreateInstance(&CLSID_DirectMusicAudioPathConfig, NULL, CLSCTX_INPROC_SERVER,
            &IID_IDirectMusicObject, (void**)&dmo);
    ok(hr == S_OK, "DirectMusicObject create failed: %08x, expected S_OK\n", hr);

    /* Same refcount for all DirectMusicObject interfaces */
    refcount = IDirectMusicObject_AddRef(dmo);
    ok(refcount == 2, "refcount == %u, expected 2\n", refcount);

    hr = IDirectMusicObject_QueryInterface(dmo, &IID_IPersistStream, (void**)&ps);
    ok(hr == S_OK, "QueryInterface for IID_IPersistStream failed: %08x\n", hr);
    refcount = IPersistStream_AddRef(ps);
    ok(refcount == 4, "refcount == %u, expected 4\n", refcount);
    IPersistStream_Release(ps);

    hr = IDirectMusicObject_QueryInterface(dmo, &IID_IUnknown, (void**)&unk);
    ok(hr == S_OK, "QueryInterface for IID_IUnknown failed: %08x\n", hr);
    refcount = IUnknown_AddRef(unk);
    ok(refcount == 5, "refcount == %u, expected 5\n", refcount);
    refcount = IUnknown_Release(unk);

    /* IDirectMusicAudioPath still not supported */
    hr = IDirectMusicObject_QueryInterface(dmo, &IID_IDirectMusicAudioPath, (void**)&dmap);
    todo_wine ok(FAILED(hr) && !dmap,
            "Unexpected IDirectMusicAudioPath interface: hr=%08x, iface=%p\n", hr, dmap);

    while (IDirectMusicObject_Release(dmo));
}


static void test_COM_graph(void)
{
    IDirectMusicGraph *dmg = (IDirectMusicGraph*)0xdeadbeef;
    IDirectMusicObject *dmo;
    IPersistStream *ps;
    IUnknown *unk;
    ULONG refcount;
    HRESULT hr;

    /* COM aggregation */
    hr = CoCreateInstance(&CLSID_DirectMusicGraph, (IUnknown *)0xdeadbeef, CLSCTX_INPROC_SERVER,
            &IID_IUnknown, (void**)&dmg);
    ok(hr == CLASS_E_NOAGGREGATION,
            "DirectMusicGraph create failed: %08x, expected CLASS_E_NOAGGREGATION\n", hr);
    ok(!dmg, "dmg = %p\n", dmg);

    /* Invalid RIID */
    hr = CoCreateInstance(&CLSID_DirectMusicGraph, NULL, CLSCTX_INPROC_SERVER, &IID_IClassFactory,
            (void**)&dmg);
    ok(hr == E_NOINTERFACE, "DirectMusicGraph create failed: %08x, expected E_NOINTERFACE\n", hr);

    /* Same refcount for all DirectMusicGraph interfaces */
    hr = CoCreateInstance(&CLSID_DirectMusicGraph, NULL, CLSCTX_INPROC_SERVER,
            &IID_IDirectMusicGraph, (void**)&dmg);
    ok(hr == S_OK, "DirectMusicGraph create failed: %08x, expected S_OK\n", hr);
    refcount = IDirectMusicGraph_AddRef(dmg);
    ok(refcount == 2, "refcount == %u, expected 2\n", refcount);

    hr = IDirectMusicGraph_QueryInterface(dmg, &IID_IDirectMusicObject, (void**)&dmo);
    if (hr == E_NOINTERFACE) {
        win_skip("DirectMusicGraph without IDirectMusicObject\n");
        return;
    }
    ok(hr == S_OK, "QueryInterface for IID_IDirectMusicObject failed: %08x\n", hr);
    refcount = IDirectMusicObject_AddRef(dmo);
    ok(refcount == 4, "refcount == %u, expected 4\n", refcount);
    refcount = IDirectMusicObject_Release(dmo);

    hr = IDirectMusicGraph_QueryInterface(dmg, &IID_IPersistStream, (void**)&ps);
    ok(hr == S_OK, "QueryInterface for IID_IPersistStream failed: %08x\n", hr);
    refcount = IPersistStream_AddRef(ps);
    ok(refcount == 5, "refcount == %u, expected 5\n", refcount);
    refcount = IPersistStream_Release(ps);

    hr = IDirectMusicGraph_QueryInterface(dmg, &IID_IUnknown, (void**)&unk);
    ok(hr == S_OK, "QueryInterface for IID_IUnknown failed: %08x\n", hr);
    refcount = IUnknown_AddRef(unk);
    ok(refcount == 6, "refcount == %u, expected 6\n", refcount);
    refcount = IUnknown_Release(unk);

    while (IDirectMusicGraph_Release(dmg));
}

static void test_COM_segment(void)
{
    IDirectMusicSegment8 *dms = (IDirectMusicSegment8*)0xdeadbeef;
    IDirectMusicObject *dmo;
    IPersistStream *stream;
    IUnknown *unk;
    ULONG refcount;
    HRESULT hr;

    /* COM aggregation */
    hr = CoCreateInstance(&CLSID_DirectMusicSegment, (IUnknown *)0xdeadbeef, CLSCTX_INPROC_SERVER,
            &IID_IUnknown, (void**)&dms);
    ok(hr == CLASS_E_NOAGGREGATION,
            "DirectMusicSegment create failed: %08x, expected CLASS_E_NOAGGREGATION\n", hr);
    ok(!dms, "dms = %p\n", dms);

    /* Invalid RIID */
    hr = CoCreateInstance(&CLSID_DirectMusicSegment, NULL, CLSCTX_INPROC_SERVER,
            &IID_IDirectSound, (void**)&dms);
    ok(hr == E_NOINTERFACE,
            "DirectMusicSegment create failed: %08x, expected E_NOINTERFACE\n", hr);

    /* Same refcount */
    hr = CoCreateInstance(&CLSID_DirectMusicSegment, NULL, CLSCTX_INPROC_SERVER,
            &IID_IDirectMusicSegment8, (void**)&dms);
    if (hr == E_NOINTERFACE) {
        win_skip("DirectMusicSegment without IDirectMusicSegment8\n");
        return;
    }
    ok(hr == S_OK, "DirectMusicSegment create failed: %08x, expected S_OK\n", hr);
    refcount = IDirectMusicSegment8_AddRef(dms);
    ok (refcount == 2, "refcount == %u, expected 2\n", refcount);
    hr = IDirectMusicSegment8_QueryInterface(dms, &IID_IDirectMusicObject, (void**)&dmo);
    ok(hr == S_OK, "QueryInterface for IID_IDirectMusicObject failed: %08x\n", hr);
    IDirectMusicSegment8_AddRef(dms);
    refcount = IDirectMusicSegment8_Release(dms);
    ok (refcount == 3, "refcount == %u, expected 3\n", refcount);
    hr = IDirectMusicSegment8_QueryInterface(dms, &IID_IPersistStream, (void**)&stream);
    ok(hr == S_OK, "QueryInterface for IID_IPersistStream failed: %08x\n", hr);
    refcount = IDirectMusicSegment8_Release(dms);
    ok (refcount == 3, "refcount == %u, expected 3\n", refcount);
    hr = IDirectMusicSegment8_QueryInterface(dms, &IID_IUnknown, (void**)&unk);
    ok(hr == S_OK, "QueryInterface for IID_IUnknown failed: %08x\n", hr);
    refcount = IUnknown_Release(unk);
    ok (refcount == 3, "refcount == %u, expected 3\n", refcount);
    refcount = IDirectMusicObject_Release(dmo);
    ok (refcount == 2, "refcount == %u, expected 2\n", refcount);
    refcount = IPersistStream_Release(stream);
    ok (refcount == 1, "refcount == %u, expected 1\n", refcount);
    refcount = IDirectMusicSegment8_Release(dms);
    ok (refcount == 0, "refcount == %u, expected 0\n", refcount);
}

static void test_COM_segmentstate(void)
{
    IDirectMusicSegmentState8 *dmss8 = (IDirectMusicSegmentState8*)0xdeadbeef;
    IUnknown *unk;
    ULONG refcount;
    HRESULT hr;

    /* COM aggregation */
    hr = CoCreateInstance(&CLSID_DirectMusicSegmentState, (IUnknown *)0xdeadbeef, CLSCTX_INPROC_SERVER,
            &IID_IUnknown, (void**)&dmss8);
    ok(hr == CLASS_E_NOAGGREGATION,
            "DirectMusicSegmentState8 create failed: %08x, expected CLASS_E_NOAGGREGATION\n", hr);
    ok(!dmss8, "dmss8 = %p\n", dmss8);

    /* Invalid RIID */
    hr = CoCreateInstance(&CLSID_DirectMusicSegmentState, NULL, CLSCTX_INPROC_SERVER,
            &IID_IDirectMusicObject, (void**)&dmss8);
    ok(hr == E_NOINTERFACE,
            "DirectMusicSegmentState8 create failed: %08x, expected E_NOINTERFACE\n", hr);

    /* Same refcount for all DirectMusicSegmentState interfaces */
    hr = CoCreateInstance(&CLSID_DirectMusicSegmentState, NULL, CLSCTX_INPROC_SERVER,
            &IID_IDirectMusicSegmentState8, (void**)&dmss8);
    if (hr == E_NOINTERFACE) {
        win_skip("DirectMusicSegmentState without IDirectMusicSegmentState8\n");
        return;
    }
    ok(hr == S_OK, "DirectMusicSegmentState8 create failed: %08x, expected S_OK\n", hr);
    refcount = IDirectMusicSegmentState8_AddRef(dmss8);
    ok(refcount == 2, "refcount == %u, expected 2\n", refcount);

    hr = IDirectMusicSegmentState8_QueryInterface(dmss8, &IID_IUnknown, (void**)&unk);
    ok(hr == S_OK, "QueryInterface for IID_IUnknown failed: %08x\n", hr);
    refcount = IUnknown_AddRef(unk);
    ok(refcount == 4, "refcount == %u, expected 4\n", refcount);
    refcount = IUnknown_Release(unk);

    hr = IDirectMusicSegmentState8_QueryInterface(dmss8, &IID_IUnknown, NULL);
    ok(hr == E_POINTER, "got %08x\n", hr);

    while (IDirectMusicSegmentState8_Release(dmss8));
}

static void test_COM_track(void)
{
    IDirectMusicTrack *dmt;
    IDirectMusicTrack8 *dmt8;
    IPersistStream *ps;
    IUnknown *unk;
    ULONG refcount;
    HRESULT hr;
#define X(class)        &CLSID_ ## class, #class
    const struct {
        REFCLSID clsid;
        const char *name;
        BOOL has_dmt8;
    } class[] = {
        { X(DirectMusicLyricsTrack), TRUE },
        { X(DirectMusicMarkerTrack), FALSE },
        { X(DirectMusicParamControlTrack), TRUE },
        { X(DirectMusicSegmentTriggerTrack), TRUE },
        { X(DirectMusicSeqTrack), TRUE },
        { X(DirectMusicSysExTrack), TRUE },
        { X(DirectMusicTempoTrack), TRUE },
        { X(DirectMusicTimeSigTrack), FALSE },
        { X(DirectMusicWaveTrack), TRUE }
    };
#undef X
    unsigned int i;

    for (i = 0; i < ARRAY_SIZE(class); i++) {
        trace("Testing %s\n", class[i].name);
        /* COM aggregation */
        dmt8 = (IDirectMusicTrack8*)0xdeadbeef;
        hr = CoCreateInstance(class[i].clsid, (IUnknown *)0xdeadbeef, CLSCTX_INPROC_SERVER, &IID_IUnknown,
                (void**)&dmt8);
        if (hr == REGDB_E_CLASSNOTREG) {
            win_skip("%s not registered\n", class[i].name);
            continue;
        }
        ok(hr == CLASS_E_NOAGGREGATION,
                "%s create failed: %08x, expected CLASS_E_NOAGGREGATION\n", class[i].name, hr);
        ok(!dmt8, "dmt8 = %p\n", dmt8);

        /* Invalid RIID */
        hr = CoCreateInstance(class[i].clsid, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusicObject,
                (void**)&dmt8);
        ok(hr == E_NOINTERFACE, "%s create failed: %08x, expected E_NOINTERFACE\n",
                class[i].name, hr);

        /* Same refcount for all DirectMusicTrack interfaces */
        hr = CoCreateInstance(class[i].clsid, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusicTrack,
                (void**)&dmt);
        ok(hr == S_OK, "%s create failed: %08x, expected S_OK\n", class[i].name, hr);
        refcount = IDirectMusicTrack_AddRef(dmt);
        ok(refcount == 2, "refcount == %u, expected 2\n", refcount);

        hr = IDirectMusicTrack_QueryInterface(dmt, &IID_IPersistStream, (void**)&ps);
        ok(hr == S_OK, "QueryInterface for IID_IPersistStream failed: %08x\n", hr);
        refcount = IPersistStream_AddRef(ps);
        ok(refcount == 4, "refcount == %u, expected 4\n", refcount);
        IPersistStream_Release(ps);

        hr = IDirectMusicTrack_QueryInterface(dmt, &IID_IUnknown, (void**)&unk);
        ok(hr == S_OK, "QueryInterface for IID_IUnknown failed: %08x\n", hr);
        refcount = IUnknown_AddRef(unk);
        ok(refcount == 5, "refcount == %u, expected 5\n", refcount);
        refcount = IUnknown_Release(unk);

        hr = IDirectMusicTrack_QueryInterface(dmt, &IID_IDirectMusicTrack8, (void**)&dmt8);
        if (class[i].has_dmt8) {
            ok(hr == S_OK, "QueryInterface for IID_IDirectMusicTrack8 failed: %08x\n", hr);
            refcount = IDirectMusicTrack8_AddRef(dmt8);
            ok(refcount == 6, "refcount == %u, expected 6\n", refcount);
            refcount = IDirectMusicTrack8_Release(dmt8);
        } else {
            ok(hr == E_NOINTERFACE, "QueryInterface for IID_IDirectMusicTrack8 failed: %08x\n", hr);
            refcount = IDirectMusicTrack_AddRef(dmt);
            ok(refcount == 5, "refcount == %u, expected 5\n", refcount);
        }

        while (IDirectMusicTrack_Release(dmt));
    }
}

static void test_audiopathconfig(void)
{
    IDirectMusicObject *dmo;
    IPersistStream *ps;
    CLSID class = { 0 };
    ULARGE_INTEGER size;
    HRESULT hr;

    hr = CoCreateInstance(&CLSID_DirectMusicAudioPathConfig, NULL, CLSCTX_INPROC_SERVER,
            &IID_IDirectMusicObject, (void**)&dmo);
    if (hr == REGDB_E_CLASSNOTREG) {
        win_skip("DirectMusicAudioPathConfig not registered\n");
        return;
    }
    ok(hr == S_OK, "DirectMusicAudioPathConfig create failed: %08x, expected S_OK\n", hr);

    /* IPersistStream */
    hr = IDirectMusicObject_QueryInterface(dmo, &IID_IPersistStream, (void**)&ps);
    ok(hr == S_OK, "QueryInterface for IID_IPersistStream failed: %08x\n", hr);
    hr = IPersistStream_GetClassID(ps, &class);
    ok(hr == S_OK, "IPersistStream_GetClassID failed: %08x\n", hr);
    ok(IsEqualGUID(&class, &CLSID_DirectMusicAudioPathConfig),
            "Expected class CLSID_DirectMusicAudioPathConfig got %s\n", wine_dbgstr_guid(&class));

    /* Unimplemented IPersistStream methods */
    hr = IPersistStream_IsDirty(ps);
    ok(hr == S_FALSE, "IPersistStream_IsDirty failed: %08x\n", hr);
    hr = IPersistStream_GetSizeMax(ps, &size);
    ok(hr == E_NOTIMPL, "IPersistStream_GetSizeMax failed: %08x\n", hr);
    hr = IPersistStream_Save(ps, NULL, TRUE);
    ok(hr == E_NOTIMPL, "IPersistStream_Save failed: %08x\n", hr);

    while (IDirectMusicObject_Release(dmo));
}

static void test_graph(void)
{
    IDirectMusicGraph *dmg;
    IPersistStream *ps;
    CLSID class = { 0 };
    ULARGE_INTEGER size;
    HRESULT hr;

    hr = CoCreateInstance(&CLSID_DirectMusicGraph, NULL, CLSCTX_INPROC_SERVER,
            &IID_IDirectMusicGraph, (void**)&dmg);
    ok(hr == S_OK, "DirectMusicGraph create failed: %08x, expected S_OK\n", hr);

    /* IPersistStream */
    hr = IDirectMusicGraph_QueryInterface(dmg, &IID_IPersistStream, (void**)&ps);
    ok(hr == S_OK, "QueryInterface for IID_IPersistStream failed: %08x\n", hr);
    hr = IPersistStream_GetClassID(ps, &class);
    ok(hr == S_OK || broken(hr == E_NOTIMPL) /* win2k */, "IPersistStream_GetClassID failed: %08x\n", hr);
    if (hr == S_OK)
        ok(IsEqualGUID(&class, &CLSID_DirectMusicGraph),
                "Expected class CLSID_DirectMusicGraph got %s\n", wine_dbgstr_guid(&class));

    /* Unimplemented IPersistStream methods */
    hr = IPersistStream_IsDirty(ps);
    ok(hr == S_FALSE, "IPersistStream_IsDirty failed: %08x\n", hr);
    hr = IPersistStream_GetSizeMax(ps, &size);
    ok(hr == E_NOTIMPL, "IPersistStream_GetSizeMax failed: %08x\n", hr);
    hr = IPersistStream_Save(ps, NULL, TRUE);
    ok(hr == E_NOTIMPL, "IPersistStream_Save failed: %08x\n", hr);

    while (IDirectMusicGraph_Release(dmg));
}

static void test_segment(void)
{
    IDirectMusicSegment *dms;
    IPersistStream *ps;
    CLSID class = { 0 };
    ULARGE_INTEGER size;
    HRESULT hr;

    hr = CoCreateInstance(&CLSID_DirectMusicSegment, NULL, CLSCTX_INPROC_SERVER,
            &IID_IDirectMusicSegment, (void**)&dms);
    ok(hr == S_OK, "DirectMusicSegment create failed: %08x, expected S_OK\n", hr);

    /* IPersistStream */
    hr = IDirectMusicSegment_QueryInterface(dms, &IID_IPersistStream, (void**)&ps);
    ok(hr == S_OK, "QueryInterface for IID_IPersistStream failed: %08x\n", hr);
    hr = IPersistStream_GetClassID(ps, &class);
    ok(hr == S_OK || broken(hr == E_NOTIMPL) /* win2k */, "IPersistStream_GetClassID failed: %08x\n", hr);
    if (hr == S_OK)
        ok(IsEqualGUID(&class, &CLSID_DirectMusicSegment),
                "Expected class CLSID_DirectMusicSegment got %s\n", wine_dbgstr_guid(&class));

    /* Unimplemented IPersistStream methods */
    hr = IPersistStream_IsDirty(ps);
    ok(hr == S_FALSE, "IPersistStream_IsDirty failed: %08x\n", hr);
    hr = IPersistStream_GetSizeMax(ps, &size);
    ok(hr == E_NOTIMPL, "IPersistStream_GetSizeMax failed: %08x\n", hr);
    hr = IPersistStream_Save(ps, NULL, TRUE);
    ok(hr == E_NOTIMPL, "IPersistStream_Save failed: %08x\n", hr);

    while (IDirectMusicSegment_Release(dms));
}

static void test_track(void)
{
    IDirectMusicTrack *dmt;
    IDirectMusicTrack8 *dmt8;
    IPersistStream *ps;
    CLSID classid;
    ULARGE_INTEGER size;
    HRESULT hr;
#define X(class)        &CLSID_ ## class, #class
    const struct {
        REFCLSID clsid;
        const char *name;
        BOOL has_param;
    } class[] = {
        { X(DirectMusicLyricsTrack), TRUE },
        { X(DirectMusicMarkerTrack), TRUE },
        { X(DirectMusicParamControlTrack), TRUE },
        { X(DirectMusicSegmentTriggerTrack), TRUE },
        { X(DirectMusicSeqTrack), FALSE },
        { X(DirectMusicSysExTrack), FALSE },
        { X(DirectMusicTempoTrack), TRUE },
        { X(DirectMusicTimeSigTrack), TRUE },
        { X(DirectMusicWaveTrack), TRUE }
    };
#undef X
    unsigned int i;

    for (i = 0; i < ARRAY_SIZE(class); i++) {
        trace("Testing %s\n", class[i].name);
        hr = CoCreateInstance(class[i].clsid, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusicTrack,
                (void**)&dmt);
        ok(hr == S_OK, "%s create failed: %08x, expected S_OK\n", class[i].name, hr);

        /* IDirectMusicTrack */
        if (!class[i].has_param) {
            hr = IDirectMusicTrack_GetParam(dmt, NULL, 0, NULL, NULL);
            ok(hr == E_NOTIMPL, "IDirectMusicTrack_GetParam failed: %08x\n", hr);
            hr = IDirectMusicTrack_SetParam(dmt, NULL, 0, NULL);
            ok(hr == E_NOTIMPL, "IDirectMusicTrack_SetParam failed: %08x\n", hr);
            hr = IDirectMusicTrack_IsParamSupported(dmt, NULL);
            ok(hr == E_NOTIMPL, "IDirectMusicTrack_IsParamSupported failed: %08x\n", hr);
        }
        if (class[i].clsid != &CLSID_DirectMusicMarkerTrack &&
                class[i].clsid != &CLSID_DirectMusicTimeSigTrack) {
            hr = IDirectMusicTrack_AddNotificationType(dmt, NULL);
            ok(hr == E_NOTIMPL, "IDirectMusicTrack_AddNotificationType failed: %08x\n", hr);
            hr = IDirectMusicTrack_RemoveNotificationType(dmt, NULL);
            ok(hr == E_NOTIMPL, "IDirectMusicTrack_RemoveNotificationType failed: %08x\n", hr);
        }
        hr = IDirectMusicTrack_Clone(dmt, 0, 0, NULL);
        todo_wine ok(hr == E_POINTER, "IDirectMusicTrack_Clone failed: %08x\n", hr);

        /* IDirectMusicTrack8 */
        hr = IDirectMusicTrack_QueryInterface(dmt, &IID_IDirectMusicTrack8, (void**)&dmt8);
        if (hr == S_OK) {
            hr = IDirectMusicTrack8_PlayEx(dmt8, NULL, 0, 0, 0, 0, NULL, NULL, 0);
            todo_wine ok(hr == E_POINTER, "IDirectMusicTrack8_PlayEx failed: %08x\n", hr);
            if (!class[i].has_param) {
                hr = IDirectMusicTrack8_GetParamEx(dmt8, NULL, 0, NULL, NULL, NULL, 0);
                ok(hr == E_NOTIMPL, "IDirectMusicTrack8_GetParamEx failed: %08x\n", hr);
                hr = IDirectMusicTrack8_SetParamEx(dmt8, NULL, 0, NULL, NULL, 0);
                ok(hr == E_NOTIMPL, "IDirectMusicTrack8_SetParamEx failed: %08x\n", hr);
            }
            hr = IDirectMusicTrack8_Compose(dmt8, NULL, 0, NULL);
            ok(hr == E_NOTIMPL, "IDirectMusicTrack8_Compose failed: %08x\n", hr);
            hr = IDirectMusicTrack8_Join(dmt8, NULL, 0, NULL, 0, NULL);
            if (class[i].clsid == &CLSID_DirectMusicTempoTrack)
                todo_wine ok(hr == E_POINTER, "IDirectMusicTrack8_Join failed: %08x\n", hr);
            else
                ok(hr == E_NOTIMPL, "IDirectMusicTrack8_Join failed: %08x\n", hr);
            IDirectMusicTrack8_Release(dmt8);
        }

        /* IPersistStream */
        hr = IDirectMusicTrack_QueryInterface(dmt, &IID_IPersistStream, (void**)&ps);
        ok(hr == S_OK, "QueryInterface for IID_IPersistStream failed: %08x\n", hr);
        hr = IPersistStream_GetClassID(ps, &classid);
        ok(hr == S_OK, "IPersistStream_GetClassID failed: %08x\n", hr);
        ok(IsEqualGUID(&classid, class[i].clsid),
                "Expected class %s got %s\n", class[i].name, wine_dbgstr_guid(&classid));
        hr = IPersistStream_IsDirty(ps);
        ok(hr == S_FALSE, "IPersistStream_IsDirty failed: %08x\n", hr);

        /* Unimplemented IPersistStream methods */
        hr = IPersistStream_GetSizeMax(ps, &size);
        ok(hr == E_NOTIMPL, "IPersistStream_GetSizeMax failed: %08x\n", hr);
        hr = IPersistStream_Save(ps, NULL, TRUE);
        ok(hr == E_NOTIMPL, "IPersistStream_Save failed: %08x\n", hr);

        while (IDirectMusicTrack_Release(dmt));
    }
}

START_TEST(dmime)
{
    CoInitialize(NULL);

    if (missing_dmime())
    {
        skip("dmime not available\n");
        CoUninitialize();
        return;
    }
    test_COM_audiopath();
    test_COM_audiopathconfig();
    test_COM_graph();
    test_COM_segment();
    test_COM_segmentstate();
    test_COM_track();
    test_audiopathconfig();
    test_graph();
    test_segment();
    test_track();

    CoUninitialize();
}
