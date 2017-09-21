/* OLE DB Internal header
 *
 * Copyright 2009 Huw Davies
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

HRESULT create_oledb_convert(IUnknown *outer, void **obj) DECLSPEC_HIDDEN;
HRESULT create_data_init(IUnknown *outer, void **obj) DECLSPEC_HIDDEN;
HRESULT create_error_info(IUnknown *outer, void **obj) DECLSPEC_HIDDEN;
HRESULT create_oledb_rowpos(IUnknown *outer, void **obj) DECLSPEC_HIDDEN;
HRESULT create_dslocator(IUnknown *outer, void **obj) DECLSPEC_HIDDEN;

HRESULT get_data_source(IUnknown *outer, DWORD clsctx, LPWSTR initstring, REFIID riid,
    IUnknown **datasource) DECLSPEC_HIDDEN;

static inline void* __WINE_ALLOC_SIZE(1) heap_alloc(size_t size)
{
    return HeapAlloc(GetProcessHeap(), 0, size);
}

static inline void* __WINE_ALLOC_SIZE(1) heap_alloc_zero(size_t size)
{
    return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size);
}

static inline void* __WINE_ALLOC_SIZE(2) heap_realloc(void *mem, size_t size)
{
    return HeapReAlloc(GetProcessHeap(), 0, mem, size);
}

static inline void* __WINE_ALLOC_SIZE(2) heap_realloc_zero(void *mem, size_t size)
{
    return HeapReAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, mem, size);
}

static inline BOOL heap_free(void *mem)
{
    return HeapFree(GetProcessHeap(), 0, mem);
}
