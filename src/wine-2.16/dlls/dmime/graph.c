/* IDirectMusicGraph
 *
 * Copyright (C) 2003-2004 Rok Mandeljc
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "dmime_private.h"
#include "dmobject.h"

WINE_DEFAULT_DEBUG_CHANNEL(dmime);
WINE_DECLARE_DEBUG_CHANNEL(dmfile);

struct IDirectMusicGraphImpl {
  IDirectMusicGraph IDirectMusicGraph_iface;
  struct dmobject dmobj;
  LONG ref;
  WORD              num_tools;
  struct list       Tools;
};

static inline IDirectMusicGraphImpl *impl_from_IDirectMusicGraph(IDirectMusicGraph *iface)
{
    return CONTAINING_RECORD(iface, IDirectMusicGraphImpl, IDirectMusicGraph_iface);
}

static inline IDirectMusicGraphImpl *impl_from_IDirectMusicObject(IDirectMusicObject *iface)
{
    return CONTAINING_RECORD(iface, IDirectMusicGraphImpl, dmobj.IDirectMusicObject_iface);
}

static inline IDirectMusicGraphImpl *impl_from_IPersistStream(IPersistStream *iface)
{
    return CONTAINING_RECORD(iface, IDirectMusicGraphImpl, dmobj.IPersistStream_iface);
}

static HRESULT WINAPI DirectMusicGraph_QueryInterface(IDirectMusicGraph *iface, REFIID riid, void **ret_iface)
{
    IDirectMusicGraphImpl *This = impl_from_IDirectMusicGraph(iface);

    TRACE("(%p, %s, %p)\n", This, debugstr_guid(riid), ret_iface);

    *ret_iface = NULL;

    if (IsEqualIID(riid, &IID_IUnknown) ||
        IsEqualIID(riid, &IID_IDirectMusicGraph))
    {
        *ret_iface = &This->IDirectMusicGraph_iface;
    }
    else if (IsEqualIID(riid, &IID_IDirectMusicObject))
        *ret_iface = &This->dmobj.IDirectMusicObject_iface;
    else if (IsEqualIID(riid, &IID_IPersistStream))
        *ret_iface = &This->dmobj.IPersistStream_iface;

    if (*ret_iface)
    {
        IDirectMusicGraph_AddRef(iface);
        return S_OK;
    }

    WARN("(%p, %s, %p): not found\n", This, debugstr_guid(riid), ret_iface);
    return E_NOINTERFACE;
}

static ULONG WINAPI DirectMusicGraph_AddRef(IDirectMusicGraph *iface)
{
    IDirectMusicGraphImpl *This = impl_from_IDirectMusicGraph(iface);
    ULONG ref = InterlockedIncrement(&This->ref);

    TRACE("(%p): %d\n", This, ref);

    DMIME_LockModule();
    return ref;
}

static ULONG WINAPI DirectMusicGraph_Release(IDirectMusicGraph *iface)
{
    IDirectMusicGraphImpl *This = impl_from_IDirectMusicGraph(iface);
    ULONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p): %d\n", This, ref);

    if (ref == 0)
        HeapFree(GetProcessHeap(), 0, This);

    DMIME_UnlockModule();
    return ref;
}

static HRESULT WINAPI DirectMusicGraph_StampPMsg(IDirectMusicGraph *iface, DMUS_PMSG *msg)
{
    IDirectMusicGraphImpl *This = impl_from_IDirectMusicGraph(iface);
    FIXME("(%p, %p): stub\n", This, msg);
    return S_OK;
}

static HRESULT WINAPI DirectMusicGraph_InsertTool(IDirectMusicGraph *iface, IDirectMusicTool* pTool, DWORD* pdwPChannels, DWORD cPChannels, LONG lIndex)
{
    IDirectMusicGraphImpl *This = impl_from_IDirectMusicGraph(iface);
    struct list* pEntry = NULL;
    struct list* pPrevEntry = NULL;
    LPDMUS_PRIVATE_GRAPH_TOOL pIt = NULL;
    LPDMUS_PRIVATE_GRAPH_TOOL pNewTool = NULL;

    FIXME("(%p, %p, %p, %d, %i): use of pdwPChannels\n", This, pTool, pdwPChannels, cPChannels, lIndex);
  
    if (!pTool)
        return E_POINTER;

    if (lIndex < 0)
        lIndex = This->num_tools + lIndex;

    pPrevEntry = &This->Tools;
    LIST_FOR_EACH(pEntry, &This->Tools)
    {
        pIt = LIST_ENTRY(pEntry, DMUS_PRIVATE_GRAPH_TOOL, entry);
        if (pIt->dwIndex == lIndex)
            return DMUS_E_ALREADY_EXISTS;

        if (pIt->dwIndex > lIndex)
          break ;
        pPrevEntry = pEntry;
    }

    ++This->num_tools;
    pNewTool = HeapAlloc (GetProcessHeap (), HEAP_ZERO_MEMORY, sizeof(DMUS_PRIVATE_GRAPH_TOOL));
    pNewTool->pTool = pTool;
    pNewTool->dwIndex = lIndex;
    IDirectMusicTool8_AddRef(pTool);
    IDirectMusicTool8_Init(pTool, iface);
    list_add_tail (pPrevEntry->next, &pNewTool->entry);

#if 0
  DWORD dwNum = 0;
  IDirectMusicTool8_GetMediaTypes(pTool, &dwNum);
#endif

    return DS_OK;
}

static HRESULT WINAPI DirectMusicGraph_GetTool(IDirectMusicGraph *iface, DWORD dwIndex, IDirectMusicTool** ppTool)
{
    IDirectMusicGraphImpl *This = impl_from_IDirectMusicGraph(iface);
    struct list* pEntry = NULL;
    LPDMUS_PRIVATE_GRAPH_TOOL pIt = NULL;
  
    FIXME("(%p, %d, %p): stub\n", This, dwIndex, ppTool);

    LIST_FOR_EACH (pEntry, &This->Tools)
    {
        pIt = LIST_ENTRY(pEntry, DMUS_PRIVATE_GRAPH_TOOL, entry);
        if (pIt->dwIndex == dwIndex)
        {
            *ppTool = pIt->pTool;
            if (*ppTool)
                IDirectMusicTool_AddRef(*ppTool);
            return S_OK;
        }
        if (pIt->dwIndex > dwIndex)
            break ;
    }

    return DMUS_E_NOT_FOUND;
}

static HRESULT WINAPI DirectMusicGraph_RemoveTool(IDirectMusicGraph *iface, IDirectMusicTool* pTool)
{
    IDirectMusicGraphImpl *This = impl_from_IDirectMusicGraph(iface);
    FIXME("(%p, %p): stub\n", This, pTool);
    return S_OK;
}

static const IDirectMusicGraphVtbl DirectMusicGraphVtbl =
{
    DirectMusicGraph_QueryInterface,
    DirectMusicGraph_AddRef,
    DirectMusicGraph_Release,
    DirectMusicGraph_StampPMsg,
    DirectMusicGraph_InsertTool,
    DirectMusicGraph_GetTool,
    DirectMusicGraph_RemoveTool
};

static HRESULT WINAPI DirectMusicObject_ParseDescriptor(IDirectMusicObject *iface, IStream *pStream, DMUS_OBJECTDESC *pDesc)
{
    IDirectMusicGraphImpl *This = impl_from_IDirectMusicObject(iface);
    DMUS_PRIVATE_CHUNK Chunk;
    DWORD StreamSize, StreamCount, ListSize[1], ListCount[1];
    LARGE_INTEGER liMove; /* used when skipping chunks */

    TRACE("(%p, %p, %p)\n", This, pStream, pDesc);

	/* FIXME: should this be determined from stream? */
	pDesc->dwValidData |= DMUS_OBJ_CLASS;
	pDesc->guidClass = CLSID_DirectMusicGraph;

	IStream_Read (pStream, &Chunk, sizeof(FOURCC)+sizeof(DWORD), NULL);
	TRACE_(dmfile)(": %s chunk (size = 0x%04x)", debugstr_fourcc (Chunk.fccID), Chunk.dwSize);
	switch (Chunk.fccID) {	
		case FOURCC_RIFF: {
			IStream_Read (pStream, &Chunk.fccID, sizeof(FOURCC), NULL);				
			TRACE_(dmfile)(": RIFF chunk of type %s", debugstr_fourcc(Chunk.fccID));
			StreamSize = Chunk.dwSize - sizeof(FOURCC);
			StreamCount = 0;
			if (Chunk.fccID == DMUS_FOURCC_TOOLGRAPH_FORM) {
				TRACE_(dmfile)(": graph form\n");
				do {
					IStream_Read (pStream, &Chunk, sizeof(FOURCC)+sizeof(DWORD), NULL);
					StreamCount += sizeof(FOURCC) + sizeof(DWORD) + Chunk.dwSize;
					TRACE_(dmfile)(": %s chunk (size = 0x%04x)", debugstr_fourcc (Chunk.fccID), Chunk.dwSize);
					switch (Chunk.fccID) {
						case DMUS_FOURCC_GUID_CHUNK: {
							TRACE_(dmfile)(": GUID chunk\n");
							pDesc->dwValidData |= DMUS_OBJ_OBJECT;
							IStream_Read (pStream, &pDesc->guidObject, Chunk.dwSize, NULL);
							break;
						}
						case DMUS_FOURCC_VERSION_CHUNK: {
							TRACE_(dmfile)(": version chunk\n");
							pDesc->dwValidData |= DMUS_OBJ_VERSION;
							IStream_Read (pStream, &pDesc->vVersion, Chunk.dwSize, NULL);
							break;
						}
						case DMUS_FOURCC_CATEGORY_CHUNK: {
							TRACE_(dmfile)(": category chunk\n");
							pDesc->dwValidData |= DMUS_OBJ_CATEGORY;
							IStream_Read (pStream, pDesc->wszCategory, Chunk.dwSize, NULL);
							break;
						}
						case FOURCC_LIST: {
							IStream_Read (pStream, &Chunk.fccID, sizeof(FOURCC), NULL);				
							TRACE_(dmfile)(": LIST chunk of type %s", debugstr_fourcc(Chunk.fccID));
							ListSize[0] = Chunk.dwSize - sizeof(FOURCC);
							ListCount[0] = 0;
							switch (Chunk.fccID) {
								/* evil M$ UNFO list, which can (!?) contain INFO elements */
								case DMUS_FOURCC_UNFO_LIST: {
									TRACE_(dmfile)(": UNFO list\n");
									do {
										IStream_Read (pStream, &Chunk, sizeof(FOURCC)+sizeof(DWORD), NULL);
										ListCount[0] += sizeof(FOURCC) + sizeof(DWORD) + Chunk.dwSize;
										TRACE_(dmfile)(": %s chunk (size = 0x%04x)", debugstr_fourcc (Chunk.fccID), Chunk.dwSize);
										switch (Chunk.fccID) {
											/* don't ask me why, but M$ puts INFO elements in UNFO list sometimes
                                             (though strings seem to be valid unicode) */
											case mmioFOURCC('I','N','A','M'):
											case DMUS_FOURCC_UNAM_CHUNK: {
												TRACE_(dmfile)(": name chunk\n");
												pDesc->dwValidData |= DMUS_OBJ_NAME;
												IStream_Read (pStream, pDesc->wszName, Chunk.dwSize, NULL);
												break;
											}
											case mmioFOURCC('I','A','R','T'):
											case DMUS_FOURCC_UART_CHUNK: {
												TRACE_(dmfile)(": artist chunk (ignored)\n");
												liMove.QuadPart = Chunk.dwSize;
												IStream_Seek (pStream, liMove, STREAM_SEEK_CUR, NULL);
												break;
											}
											case mmioFOURCC('I','C','O','P'):
											case DMUS_FOURCC_UCOP_CHUNK: {
												TRACE_(dmfile)(": copyright chunk (ignored)\n");
												liMove.QuadPart = Chunk.dwSize;
												IStream_Seek (pStream, liMove, STREAM_SEEK_CUR, NULL);
												break;
											}
											case mmioFOURCC('I','S','B','J'):
											case DMUS_FOURCC_USBJ_CHUNK: {
												TRACE_(dmfile)(": subject chunk (ignored)\n");
												liMove.QuadPart = Chunk.dwSize;
												IStream_Seek (pStream, liMove, STREAM_SEEK_CUR, NULL);
												break;
											}
											case mmioFOURCC('I','C','M','T'):
											case DMUS_FOURCC_UCMT_CHUNK: {
												TRACE_(dmfile)(": comment chunk (ignored)\n");
												liMove.QuadPart = Chunk.dwSize;
												IStream_Seek (pStream, liMove, STREAM_SEEK_CUR, NULL);
												break;
											}
											default: {
												TRACE_(dmfile)(": unknown chunk (irrelevant & skipping)\n");
												liMove.QuadPart = Chunk.dwSize;
												IStream_Seek (pStream, liMove, STREAM_SEEK_CUR, NULL);
												break;						
											}
										}
										TRACE_(dmfile)(": ListCount[0] = %d < ListSize[0] = %d\n", ListCount[0], ListSize[0]);
									} while (ListCount[0] < ListSize[0]);
									break;
								}
								default: {
									TRACE_(dmfile)(": unknown (skipping)\n");
									liMove.QuadPart = Chunk.dwSize - sizeof(FOURCC);
									IStream_Seek (pStream, liMove, STREAM_SEEK_CUR, NULL);
									break;						
								}
							}
							break;
						}	
						default: {
							TRACE_(dmfile)(": unknown chunk (irrelevant & skipping)\n");
							liMove.QuadPart = Chunk.dwSize;
							IStream_Seek (pStream, liMove, STREAM_SEEK_CUR, NULL);
							break;						
						}
					}
					TRACE_(dmfile)(": StreamCount[0] = %d < StreamSize[0] = %d\n", StreamCount, StreamSize);
				} while (StreamCount < StreamSize);
			} else {
				TRACE_(dmfile)(": unexpected chunk; loading failed)\n");
				liMove.QuadPart = StreamSize;
				IStream_Seek (pStream, liMove, STREAM_SEEK_CUR, NULL); /* skip the rest of the chunk */
				return E_FAIL;
			}
		
			TRACE_(dmfile)(": reading finished\n");
			break;
		}
		default: {
			TRACE_(dmfile)(": unexpected chunk; loading failed)\n");
			liMove.QuadPart = Chunk.dwSize;
			IStream_Seek (pStream, liMove, STREAM_SEEK_CUR, NULL); /* skip the rest of the chunk */
			return DMUS_E_INVALIDFILE;
		}
	}	
	
	TRACE(": returning descriptor:\n%s\n", debugstr_DMUS_OBJECTDESC (pDesc));
	
	return S_OK;
}

static const IDirectMusicObjectVtbl dmobject_vtbl = {
    dmobj_IDirectMusicObject_QueryInterface,
    dmobj_IDirectMusicObject_AddRef,
    dmobj_IDirectMusicObject_Release,
    dmobj_IDirectMusicObject_GetDescriptor,
    dmobj_IDirectMusicObject_SetDescriptor,
    DirectMusicObject_ParseDescriptor
};

static HRESULT WINAPI PersistStream_Load(IPersistStream *iface, IStream* pStm)
{
    IDirectMusicGraphImpl *This = impl_from_IPersistStream(iface);
	FOURCC chunkID;
	DWORD chunkSize, StreamSize, StreamCount, ListSize[3], ListCount[3];
	LARGE_INTEGER liMove; /* used when skipping chunks */

	FIXME("(%p, %p): Loading not implemented yet\n", This, pStm);
	IStream_Read (pStm, &chunkID, sizeof(FOURCC), NULL);
	IStream_Read (pStm, &chunkSize, sizeof(DWORD), NULL);
	TRACE_(dmfile)(": %s chunk (size = %d)", debugstr_fourcc (chunkID), chunkSize);
	switch (chunkID) {	
		case FOURCC_RIFF: {
			IStream_Read (pStm, &chunkID, sizeof(FOURCC), NULL);				
			TRACE_(dmfile)(": RIFF chunk of type %s", debugstr_fourcc(chunkID));
			StreamSize = chunkSize - sizeof(FOURCC);
			StreamCount = 0;
			switch (chunkID) {
				case DMUS_FOURCC_TOOLGRAPH_FORM: {
					TRACE_(dmfile)(": graph form\n");
					do {
						IStream_Read (pStm, &chunkID, sizeof(FOURCC), NULL);
						IStream_Read (pStm, &chunkSize, sizeof(FOURCC), NULL);
						StreamCount += sizeof(FOURCC) + sizeof(DWORD) + chunkSize;
						TRACE_(dmfile)(": %s chunk (size = %d)", debugstr_fourcc (chunkID), chunkSize);
						switch (chunkID) {
							case DMUS_FOURCC_GUID_CHUNK: {
								TRACE_(dmfile)(": GUID chunk\n");
								This->dmobj.desc.dwValidData |= DMUS_OBJ_OBJECT;
								IStream_Read (pStm, &This->dmobj.desc.guidObject, chunkSize, NULL);
								break;
							}
							case DMUS_FOURCC_VERSION_CHUNK: {
								TRACE_(dmfile)(": version chunk\n");
								This->dmobj.desc.dwValidData |= DMUS_OBJ_VERSION;
								IStream_Read (pStm, &This->dmobj.desc.vVersion, chunkSize, NULL);
								break;
							}
							case DMUS_FOURCC_CATEGORY_CHUNK: {
								TRACE_(dmfile)(": category chunk\n");
								This->dmobj.desc.dwValidData |= DMUS_OBJ_CATEGORY;
								IStream_Read (pStm, This->dmobj.desc.wszCategory, chunkSize, NULL);
								break;
							}
							case FOURCC_LIST: {
								IStream_Read (pStm, &chunkID, sizeof(FOURCC), NULL);				
								TRACE_(dmfile)(": LIST chunk of type %s", debugstr_fourcc(chunkID));
								ListSize[0] = chunkSize - sizeof(FOURCC);
								ListCount[0] = 0;
								switch (chunkID) {
									case DMUS_FOURCC_UNFO_LIST: {
										TRACE_(dmfile)(": UNFO list\n");
										do {
											IStream_Read (pStm, &chunkID, sizeof(FOURCC), NULL);
											IStream_Read (pStm, &chunkSize, sizeof(FOURCC), NULL);
											ListCount[0] += sizeof(FOURCC) + sizeof(DWORD) + chunkSize;
											TRACE_(dmfile)(": %s chunk (size = %d)", debugstr_fourcc (chunkID), chunkSize);
											switch (chunkID) {
												/* don't ask me why, but M$ puts INFO elements in UNFO list sometimes
                                              (though strings seem to be valid unicode) */
												case mmioFOURCC('I','N','A','M'):
												case DMUS_FOURCC_UNAM_CHUNK: {
													TRACE_(dmfile)(": name chunk\n");
													This->dmobj.desc.dwValidData |= DMUS_OBJ_NAME;
													IStream_Read (pStm, This->dmobj.desc.wszName, chunkSize, NULL);
													break;
												}
												case mmioFOURCC('I','A','R','T'):
												case DMUS_FOURCC_UART_CHUNK: {
													TRACE_(dmfile)(": artist chunk (ignored)\n");
													liMove.QuadPart = chunkSize;
													IStream_Seek (pStm, liMove, STREAM_SEEK_CUR, NULL);
													break;
												}
												case mmioFOURCC('I','C','O','P'):
												case DMUS_FOURCC_UCOP_CHUNK: {
													TRACE_(dmfile)(": copyright chunk (ignored)\n");
													liMove.QuadPart = chunkSize;
													IStream_Seek (pStm, liMove, STREAM_SEEK_CUR, NULL);
													break;
												}
												case mmioFOURCC('I','S','B','J'):
												case DMUS_FOURCC_USBJ_CHUNK: {
													TRACE_(dmfile)(": subject chunk (ignored)\n");
													liMove.QuadPart = chunkSize;
													IStream_Seek (pStm, liMove, STREAM_SEEK_CUR, NULL);
													break;
												}
												case mmioFOURCC('I','C','M','T'):
												case DMUS_FOURCC_UCMT_CHUNK: {
													TRACE_(dmfile)(": comment chunk (ignored)\n");
													liMove.QuadPart = chunkSize;
													IStream_Seek (pStm, liMove, STREAM_SEEK_CUR, NULL);
													break;
												}
												default: {
													TRACE_(dmfile)(": unknown chunk (irrelevant & skipping)\n");
													liMove.QuadPart = chunkSize;
													IStream_Seek (pStm, liMove, STREAM_SEEK_CUR, NULL);
													break;						
												}
											}
											TRACE_(dmfile)(": ListCount[0] = %d < ListSize[0] = %d\n", ListCount[0], ListSize[0]);
										} while (ListCount[0] < ListSize[0]);
										break;
									}
									default: {
										TRACE_(dmfile)(": unknown (skipping)\n");
										liMove.QuadPart = chunkSize - sizeof(FOURCC);
										IStream_Seek (pStm, liMove, STREAM_SEEK_CUR, NULL);
										break;						
									}
								}
								break;
							}	
							default: {
								TRACE_(dmfile)(": unknown chunk (irrelevant & skipping)\n");
								liMove.QuadPart = chunkSize;
								IStream_Seek (pStm, liMove, STREAM_SEEK_CUR, NULL);
								break;						
							}
						}
						TRACE_(dmfile)(": StreamCount[0] = %d < StreamSize[0] = %d\n", StreamCount, StreamSize);
					} while (StreamCount < StreamSize);
					break;
				}
				default: {
					TRACE_(dmfile)(": unexpected chunk; loading failed)\n");
					liMove.QuadPart = StreamSize;
					IStream_Seek (pStm, liMove, STREAM_SEEK_CUR, NULL); /* skip the rest of the chunk */
					return E_FAIL;
				}
			}
			TRACE_(dmfile)(": reading finished\n");
			break;
		}
		default: {
			TRACE_(dmfile)(": unexpected chunk; loading failed)\n");
			liMove.QuadPart = chunkSize;
			IStream_Seek (pStm, liMove, STREAM_SEEK_CUR, NULL); /* skip the rest of the chunk */
			return E_FAIL;
		}
	}

	return S_OK;
}

static const IPersistStreamVtbl persiststream_vtbl = {
    dmobj_IPersistStream_QueryInterface,
    dmobj_IPersistStream_AddRef,
    dmobj_IPersistStream_Release,
    dmobj_IPersistStream_GetClassID,
    unimpl_IPersistStream_IsDirty,
    PersistStream_Load,
    unimpl_IPersistStream_Save,
    unimpl_IPersistStream_GetSizeMax
};

/* for ClassFactory */
HRESULT WINAPI create_dmgraph(REFIID riid, void **ret_iface)
{
    IDirectMusicGraphImpl* obj;
    HRESULT hr;

    *ret_iface = NULL;
  
    obj = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(IDirectMusicGraphImpl));
    if (!obj)
        return E_OUTOFMEMORY;

    obj->IDirectMusicGraph_iface.lpVtbl = &DirectMusicGraphVtbl;
    obj->ref = 1;
    dmobject_init(&obj->dmobj, &CLSID_DirectMusicGraph, (IUnknown *)&obj->IDirectMusicGraph_iface);
    obj->dmobj.IDirectMusicObject_iface.lpVtbl = &dmobject_vtbl;
    obj->dmobj.IPersistStream_iface.lpVtbl = &persiststream_vtbl;
    list_init(&obj->Tools);

    hr = IDirectMusicGraph_QueryInterface(&obj->IDirectMusicGraph_iface, riid, ret_iface);
    IDirectMusicGraph_Release(&obj->IDirectMusicGraph_iface);

    return hr;
}
