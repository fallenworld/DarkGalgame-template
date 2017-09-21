/*
 * IXmlWriter implementation
 *
 * Copyright 2011 Alistair Leslie-Hughes
 * Copyright 2014, 2016 Nikolay Sivov for CodeWeavers
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
#include "windef.h"
#include "winbase.h"
#include "objbase.h"
#include "xmllite.h"
#include "xmllite_private.h"
#include "initguid.h"

#include "wine/debug.h"
#include "wine/list.h"
#include "wine/unicode.h"

WINE_DEFAULT_DEBUG_CHANNEL(xmllite);

/* not defined in public headers */
DEFINE_GUID(IID_IXmlWriterOutput, 0xc1131708, 0x0f59, 0x477f, 0x93, 0x59, 0x7d, 0x33, 0x24, 0x51, 0xbc, 0x1a);

#define ARRAY_SIZE(array) (sizeof(array)/sizeof((array)[0]))

static const WCHAR closeelementW[] = {'<','/'};
static const WCHAR closetagW[] = {' ','/','>'};
static const WCHAR closepiW[] = {'?','>'};
static const WCHAR ltW[] = {'<'};
static const WCHAR gtW[] = {'>'};
static const WCHAR spaceW[] = {' '};
static const WCHAR quoteW[] = {'"'};

struct output_buffer
{
    char *data;
    unsigned int allocated;
    unsigned int written;
    UINT codepage;
};

typedef enum
{
    XmlWriterState_Initial,         /* output is not set yet */
    XmlWriterState_Ready,           /* SetOutput() was called, ready to start */
    XmlWriterState_InvalidEncoding, /* SetOutput() was called, but output had invalid encoding */
    XmlWriterState_PIDocStarted,    /* document was started with manually added 'xml' PI */
    XmlWriterState_DocStarted,      /* document was started with WriteStartDocument() */
    XmlWriterState_ElemStarted,     /* writing element */
    XmlWriterState_Content,         /* content is accepted at this point */
    XmlWriterState_DocClosed        /* WriteEndDocument was called */
} XmlWriterState;

typedef struct
{
    IXmlWriterOutput IXmlWriterOutput_iface;
    LONG ref;
    IUnknown *output;
    ISequentialStream *stream;
    IMalloc *imalloc;
    xml_encoding encoding;
    WCHAR *encoding_name; /* exactly as specified on output creation */
    struct output_buffer buffer;
} xmlwriteroutput;

static const struct IUnknownVtbl xmlwriteroutputvtbl;

struct element
{
    struct list entry;
    WCHAR *qname;
    unsigned int len; /* qname length in chars */
};

typedef struct _xmlwriter
{
    IXmlWriter IXmlWriter_iface;
    LONG ref;
    IMalloc *imalloc;
    xmlwriteroutput *output;
    unsigned int indent_level;
    BOOL indent;
    BOOL bom;
    BOOL omitxmldecl;
    XmlConformanceLevel conformance;
    XmlWriterState state;
    BOOL bomwritten;
    BOOL starttagopen;
    struct list elements;
} xmlwriter;

static inline xmlwriter *impl_from_IXmlWriter(IXmlWriter *iface)
{
    return CONTAINING_RECORD(iface, xmlwriter, IXmlWriter_iface);
}

static inline xmlwriteroutput *impl_from_IXmlWriterOutput(IXmlWriterOutput *iface)
{
    return CONTAINING_RECORD(iface, xmlwriteroutput, IXmlWriterOutput_iface);
}

static const char *debugstr_writer_prop(XmlWriterProperty prop)
{
    static const char * const prop_names[] =
    {
        "MultiLanguage",
        "Indent",
        "ByteOrderMark",
        "OmitXmlDeclaration",
        "ConformanceLevel"
    };

    if (prop > _XmlWriterProperty_Last)
        return wine_dbg_sprintf("unknown property=%d", prop);

    return prop_names[prop];
}

/* writer output memory allocation functions */
static inline void *writeroutput_alloc(xmlwriteroutput *output, size_t len)
{
    return m_alloc(output->imalloc, len);
}

static inline void writeroutput_free(xmlwriteroutput *output, void *mem)
{
    m_free(output->imalloc, mem);
}

static inline void *writeroutput_realloc(xmlwriteroutput *output, void *mem, size_t len)
{
    return m_realloc(output->imalloc, mem, len);
}

/* writer memory allocation functions */
static inline void *writer_alloc(xmlwriter *writer, size_t len)
{
    return m_alloc(writer->imalloc, len);
}

static inline void writer_free(xmlwriter *writer, void *mem)
{
    m_free(writer->imalloc, mem);
}

static struct element *alloc_element(xmlwriter *writer, const WCHAR *prefix, const WCHAR *local)
{
    struct element *ret;
    int len;

    ret = writer_alloc(writer, sizeof(*ret));
    if (!ret) return ret;

    len = prefix ? strlenW(prefix) + 1 /* ':' */ : 0;
    len += strlenW(local);

    ret->qname = writer_alloc(writer, (len + 1)*sizeof(WCHAR));
    ret->len = len;
    if (prefix) {
        static const WCHAR colonW[] = {':',0};
        strcpyW(ret->qname, prefix);
        strcatW(ret->qname, colonW);
    }
    else
        ret->qname[0] = 0;
    strcatW(ret->qname, local);

    return ret;
}

static void free_element(xmlwriter *writer, struct element *element)
{
    writer_free(writer, element->qname);
    writer_free(writer, element);
}

static void push_element(xmlwriter *writer, struct element *element)
{
    list_add_head(&writer->elements, &element->entry);
}

static struct element *pop_element(xmlwriter *writer)
{
    struct element *element = LIST_ENTRY(list_head(&writer->elements), struct element, entry);

    if (element)
        list_remove(&element->entry);

    return element;
}

static HRESULT init_output_buffer(xmlwriteroutput *output)
{
    struct output_buffer *buffer = &output->buffer;
    const int initial_len = 0x2000;
    UINT cp = ~0u;
    HRESULT hr;

    if (FAILED(hr = get_code_page(output->encoding, &cp)))
        WARN("Failed to get code page for specified encoding.\n");

    buffer->data = writeroutput_alloc(output, initial_len);
    if (!buffer->data) return E_OUTOFMEMORY;

    memset(buffer->data, 0, 4);
    buffer->allocated = initial_len;
    buffer->written = 0;
    buffer->codepage = cp;

    return S_OK;
}

static void free_output_buffer(xmlwriteroutput *output)
{
    struct output_buffer *buffer = &output->buffer;
    writeroutput_free(output, buffer->data);
    buffer->data = NULL;
    buffer->allocated = 0;
    buffer->written = 0;
}

static HRESULT grow_output_buffer(xmlwriteroutput *output, int length)
{
    struct output_buffer *buffer = &output->buffer;
    /* grow if needed, plus 4 bytes to be sure null terminator will fit in */
    if (buffer->allocated < buffer->written + length + 4) {
        int grown_size = max(2*buffer->allocated, buffer->allocated + length);
        char *ptr = writeroutput_realloc(output, buffer->data, grown_size);
        if (!ptr) return E_OUTOFMEMORY;
        buffer->data = ptr;
        buffer->allocated = grown_size;
    }

    return S_OK;
}

static HRESULT write_output_buffer(xmlwriteroutput *output, const WCHAR *data, int len)
{
    struct output_buffer *buffer = &output->buffer;
    int length;
    HRESULT hr;
    char *ptr;

    if (buffer->codepage == 1200) {
        /* For UTF-16 encoding just copy. */
        length = len == -1 ? strlenW(data) : len;
        if (length) {
            length *= sizeof(WCHAR);

            hr = grow_output_buffer(output, length);
            if (FAILED(hr)) return hr;
            ptr = buffer->data + buffer->written;

            memcpy(ptr, data, length);
            buffer->written += length;
            ptr += length;
            /* null termination */
            *(WCHAR*)ptr = 0;
        }
    }
    else {
        length = WideCharToMultiByte(buffer->codepage, 0, data, len, NULL, 0, NULL, NULL);
        hr = grow_output_buffer(output, length);
        if (FAILED(hr)) return hr;
        ptr = buffer->data + buffer->written;
        length = WideCharToMultiByte(buffer->codepage, 0, data, len, ptr, length, NULL, NULL);
        buffer->written += len == -1 ? length-1 : length;
    }

    return S_OK;
}

static HRESULT write_output_buffer_quoted(xmlwriteroutput *output, const WCHAR *data, int len)
{
    write_output_buffer(output, quoteW, ARRAY_SIZE(quoteW));
    write_output_buffer(output, data, len);
    write_output_buffer(output, quoteW, ARRAY_SIZE(quoteW));
    return S_OK;
}

/* TODO: test if we need to validate char range */
static HRESULT write_output_qname(xmlwriteroutput *output, const WCHAR *prefix, const WCHAR *local_name)
{
    if (prefix) {
        static const WCHAR colW[] = {':'};
        write_output_buffer(output, prefix, -1);
        write_output_buffer(output, colW, ARRAY_SIZE(colW));
    }

    write_output_buffer(output, local_name, -1);

    return S_OK;
}

static void writeroutput_release_stream(xmlwriteroutput *writeroutput)
{
    if (writeroutput->stream) {
        ISequentialStream_Release(writeroutput->stream);
        writeroutput->stream = NULL;
    }
}

static inline HRESULT writeroutput_query_for_stream(xmlwriteroutput *writeroutput)
{
    HRESULT hr;

    writeroutput_release_stream(writeroutput);
    hr = IUnknown_QueryInterface(writeroutput->output, &IID_IStream, (void**)&writeroutput->stream);
    if (hr != S_OK)
        hr = IUnknown_QueryInterface(writeroutput->output, &IID_ISequentialStream, (void**)&writeroutput->stream);

    return hr;
}

static HRESULT writeroutput_flush_stream(xmlwriteroutput *output)
{
    struct output_buffer *buffer;
    ULONG written, offset = 0;
    HRESULT hr;

    if (!output || !output->stream)
        return S_OK;

    buffer = &output->buffer;

    /* It will loop forever until everything is written or an error occurred. */
    do {
        written = 0;
        hr = ISequentialStream_Write(output->stream, buffer->data + offset, buffer->written, &written);
        if (FAILED(hr)) {
            WARN("write to stream failed (0x%08x)\n", hr);
            buffer->written = 0;
            return hr;
        }

        offset += written;
        buffer->written -= written;
    } while (buffer->written > 0);

    return S_OK;
}

static HRESULT write_encoding_bom(xmlwriter *writer)
{
    if (!writer->bom || writer->bomwritten) return S_OK;

    if (writer->output->encoding == XmlEncoding_UTF16) {
        static const char utf16bom[] = {0xff, 0xfe};
        struct output_buffer *buffer = &writer->output->buffer;
        int len = sizeof(utf16bom);
        HRESULT hr;

        hr = grow_output_buffer(writer->output, len);
        if (FAILED(hr)) return hr;
        memcpy(buffer->data + buffer->written, utf16bom, len);
        buffer->written += len;
    }

    writer->bomwritten = TRUE;
    return S_OK;
}

static const WCHAR *get_output_encoding_name(xmlwriteroutput *output)
{
    if (output->encoding_name)
        return output->encoding_name;

    return get_encoding_name(output->encoding);
}

static HRESULT write_xmldecl(xmlwriter *writer, XmlStandalone standalone)
{
    static const WCHAR versionW[] = {'<','?','x','m','l',' ','v','e','r','s','i','o','n','=','"','1','.','0','"'};
    static const WCHAR encodingW[] = {' ','e','n','c','o','d','i','n','g','='};

    write_encoding_bom(writer);
    writer->state = XmlWriterState_DocStarted;
    if (writer->omitxmldecl) return S_OK;

    /* version */
    write_output_buffer(writer->output, versionW, ARRAY_SIZE(versionW));

    /* encoding */
    write_output_buffer(writer->output, encodingW, ARRAY_SIZE(encodingW));
    write_output_buffer_quoted(writer->output, get_output_encoding_name(writer->output), -1);

    /* standalone */
    if (standalone == XmlStandalone_Omit)
        write_output_buffer(writer->output, closepiW, ARRAY_SIZE(closepiW));
    else {
        static const WCHAR standaloneW[] = {' ','s','t','a','n','d','a','l','o','n','e','=','\"'};
        static const WCHAR yesW[] = {'y','e','s','\"','?','>'};
        static const WCHAR noW[] = {'n','o','\"','?','>'};

        write_output_buffer(writer->output, standaloneW, ARRAY_SIZE(standaloneW));
        if (standalone == XmlStandalone_Yes)
            write_output_buffer(writer->output, yesW, ARRAY_SIZE(yesW));
        else
            write_output_buffer(writer->output, noW, ARRAY_SIZE(noW));
    }

    return S_OK;
}

static HRESULT writer_close_starttag(xmlwriter *writer)
{
    HRESULT hr;

    if (!writer->starttagopen) return S_OK;
    hr = write_output_buffer(writer->output, gtW, ARRAY_SIZE(gtW));
    writer->starttagopen = FALSE;
    return hr;
}

static void writer_inc_indent(xmlwriter *writer)
{
    writer->indent_level++;
}

static void writer_dec_indent(xmlwriter *writer)
{
    if (writer->indent_level)
        writer->indent_level--;
}

static void write_node_indent(xmlwriter *writer)
{
    static const WCHAR dblspaceW[] = {' ',' '};
    static const WCHAR crlfW[] = {'\r','\n'};
    unsigned int indent_level = writer->indent_level;

    if (!writer->indent)
        return;

    /* Do state check to prevent newline inserted after BOM. It is assumed that
       state does not change between writing BOM and inserting indentation. */
    if (writer->output->buffer.written && writer->state != XmlWriterState_Ready)
        write_output_buffer(writer->output, crlfW, ARRAY_SIZE(crlfW));
    while (indent_level--)
        write_output_buffer(writer->output, dblspaceW, ARRAY_SIZE(dblspaceW));
}

static HRESULT WINAPI xmlwriter_QueryInterface(IXmlWriter *iface, REFIID riid, void **ppvObject)
{
    xmlwriter *This = impl_from_IXmlWriter(iface);

    TRACE("(%p)->(%s %p)\n", This, debugstr_guid(riid), ppvObject);

    if (IsEqualGUID(riid, &IID_IXmlWriter) ||
        IsEqualGUID(riid, &IID_IUnknown))
    {
        *ppvObject = iface;
    }
    else
    {
        FIXME("interface %s is not supported\n", debugstr_guid(riid));
        *ppvObject = NULL;
        return E_NOINTERFACE;
    }

    IXmlWriter_AddRef(iface);

    return S_OK;
}

static ULONG WINAPI xmlwriter_AddRef(IXmlWriter *iface)
{
    xmlwriter *This = impl_from_IXmlWriter(iface);
    ULONG ref = InterlockedIncrement(&This->ref);
    TRACE("(%p)->(%u)\n", This, ref);
    return ref;
}

static ULONG WINAPI xmlwriter_Release(IXmlWriter *iface)
{
    xmlwriter *This = impl_from_IXmlWriter(iface);
    ULONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p)->(%u)\n", This, ref);

    if (ref == 0) {
        struct element *element, *element2;
        IMalloc *imalloc = This->imalloc;

        writeroutput_flush_stream(This->output);
        if (This->output) IUnknown_Release(&This->output->IXmlWriterOutput_iface);

        /* element stack */
        LIST_FOR_EACH_ENTRY_SAFE(element, element2, &This->elements, struct element, entry) {
            list_remove(&element->entry);
            free_element(This, element);
        }

        writer_free(This, This);
        if (imalloc) IMalloc_Release(imalloc);
    }

    return ref;
}

/*** IXmlWriter methods ***/
static HRESULT WINAPI xmlwriter_SetOutput(IXmlWriter *iface, IUnknown *output)
{
    xmlwriter *This = impl_from_IXmlWriter(iface);
    IXmlWriterOutput *writeroutput;
    HRESULT hr;

    TRACE("(%p)->(%p)\n", This, output);

    if (This->output) {
        writeroutput_release_stream(This->output);
        IUnknown_Release(&This->output->IXmlWriterOutput_iface);
        This->output = NULL;
        This->bomwritten = FALSE;
        This->indent_level = 0;
    }

    /* just reset current output */
    if (!output) {
        This->state = XmlWriterState_Initial;
        return S_OK;
    }

    /* now try IXmlWriterOutput, ISequentialStream, IStream */
    hr = IUnknown_QueryInterface(output, &IID_IXmlWriterOutput, (void**)&writeroutput);
    if (hr == S_OK) {
        if (writeroutput->lpVtbl == &xmlwriteroutputvtbl)
            This->output = impl_from_IXmlWriterOutput(writeroutput);
        else {
            ERR("got external IXmlWriterOutput implementation: %p, vtbl=%p\n",
                writeroutput, writeroutput->lpVtbl);
            IUnknown_Release(writeroutput);
            return E_FAIL;
        }
    }

    if (hr != S_OK || !writeroutput) {
        /* create IXmlWriterOutput basing on supplied interface */
        hr = CreateXmlWriterOutputWithEncodingName(output, This->imalloc, NULL, &writeroutput);
        if (hr != S_OK) return hr;
        This->output = impl_from_IXmlWriterOutput(writeroutput);
    }

    if (This->output->encoding == XmlEncoding_Unknown)
        This->state = XmlWriterState_InvalidEncoding;
    else
        This->state = XmlWriterState_Ready;
    return writeroutput_query_for_stream(This->output);
}

static HRESULT WINAPI xmlwriter_GetProperty(IXmlWriter *iface, UINT property, LONG_PTR *value)
{
    xmlwriter *This = impl_from_IXmlWriter(iface);

    TRACE("(%p)->(%s %p)\n", This, debugstr_writer_prop(property), value);

    if (!value) return E_INVALIDARG;

    switch (property)
    {
        case XmlWriterProperty_Indent:
            *value = This->indent;
            break;
        case XmlWriterProperty_ByteOrderMark:
            *value = This->bom;
            break;
        case XmlWriterProperty_OmitXmlDeclaration:
            *value = This->omitxmldecl;
            break;
        case XmlWriterProperty_ConformanceLevel:
            *value = This->conformance;
            break;
        default:
            FIXME("Unimplemented property (%u)\n", property);
            return E_NOTIMPL;
    }

    return S_OK;
}

static HRESULT WINAPI xmlwriter_SetProperty(IXmlWriter *iface, UINT property, LONG_PTR value)
{
    xmlwriter *This = impl_from_IXmlWriter(iface);

    TRACE("(%p)->(%s %lu)\n", This, debugstr_writer_prop(property), value);

    switch (property)
    {
        case XmlWriterProperty_Indent:
            This->indent = !!value;
            break;
        case XmlWriterProperty_ByteOrderMark:
            This->bom = !!value;
            break;
        case XmlWriterProperty_OmitXmlDeclaration:
            This->omitxmldecl = !!value;
            break;
        default:
            FIXME("Unimplemented property (%u)\n", property);
            return E_NOTIMPL;
    }

    return S_OK;
}

static HRESULT WINAPI xmlwriter_WriteAttributes(IXmlWriter *iface, IXmlReader *pReader,
                                  BOOL fWriteDefaultAttributes)
{
    xmlwriter *This = impl_from_IXmlWriter(iface);

    FIXME("%p %p %d\n", This, pReader, fWriteDefaultAttributes);

    return E_NOTIMPL;
}

static HRESULT WINAPI xmlwriter_WriteAttributeString(IXmlWriter *iface, LPCWSTR ns_prefix,
    LPCWSTR local_name, LPCWSTR ns_uri, LPCWSTR value)
{
    static const WCHAR eqW[] = {'=','"'};
    xmlwriter *This = impl_from_IXmlWriter(iface);

    TRACE("%p %s %s %s %s\n", This, debugstr_w(ns_prefix), debugstr_w(local_name),
                        debugstr_w(ns_uri), debugstr_w(value));

    switch (This->state)
    {
    case XmlWriterState_Initial:
        return E_UNEXPECTED;
    case XmlWriterState_Ready:
    case XmlWriterState_DocClosed:
        This->state = XmlWriterState_DocClosed;
        return WR_E_INVALIDACTION;
    case XmlWriterState_InvalidEncoding:
        return MX_E_ENCODING;
    default:
        ;
    }

    if (ns_prefix || ns_uri)
    {
        FIXME("namespaces are not supported.\n");
        return E_NOTIMPL;
    }

    write_output_buffer(This->output, spaceW, ARRAY_SIZE(spaceW));
    write_output_buffer(This->output, local_name, -1);
    write_output_buffer(This->output, eqW, ARRAY_SIZE(eqW));
    write_output_buffer(This->output, value, -1);
    write_output_buffer(This->output, quoteW, ARRAY_SIZE(quoteW));

    return S_OK;
}

static void write_cdata_section(xmlwriteroutput *output, const WCHAR *data, int len)
{
    static const WCHAR cdataopenW[] = {'<','!','[','C','D','A','T','A','['};
    static const WCHAR cdatacloseW[] = {']',']','>'};
    write_output_buffer(output, cdataopenW, ARRAY_SIZE(cdataopenW));
    if (data)
        write_output_buffer(output, data, len);
    write_output_buffer(output, cdatacloseW, ARRAY_SIZE(cdatacloseW));
}

static HRESULT WINAPI xmlwriter_WriteCData(IXmlWriter *iface, LPCWSTR data)
{
    xmlwriter *This = impl_from_IXmlWriter(iface);
    int len;

    TRACE("%p %s\n", This, debugstr_w(data));

    switch (This->state)
    {
    case XmlWriterState_Initial:
        return E_UNEXPECTED;
    case XmlWriterState_ElemStarted:
        writer_close_starttag(This);
        break;
    case XmlWriterState_Ready:
    case XmlWriterState_DocClosed:
        This->state = XmlWriterState_DocClosed;
        return WR_E_INVALIDACTION;
    case XmlWriterState_InvalidEncoding:
        return MX_E_ENCODING;
    default:
        ;
    }

    len = data ? strlenW(data) : 0;

    write_node_indent(This);
    if (!len)
        write_cdata_section(This->output, NULL, 0);
    else {
        static const WCHAR cdatacloseW[] = {']',']','>',0};
        while (len) {
            const WCHAR *str = strstrW(data, cdatacloseW);
            if (str) {
                str += 2;
                write_cdata_section(This->output, data, str - data);
                len -= str - data;
                data = str;
            }
            else {
                write_cdata_section(This->output, data, len);
                break;
            }
        }
    }

    return S_OK;
}

static HRESULT WINAPI xmlwriter_WriteCharEntity(IXmlWriter *iface, WCHAR ch)
{
    static const WCHAR fmtW[] = {'&','#','x','%','x',';',0};
    xmlwriter *This = impl_from_IXmlWriter(iface);
    WCHAR bufW[16];

    TRACE("%p %#x\n", This, ch);

    switch (This->state)
    {
    case XmlWriterState_Initial:
        return E_UNEXPECTED;
    case XmlWriterState_InvalidEncoding:
        return MX_E_ENCODING;
    case XmlWriterState_ElemStarted:
        writer_close_starttag(This);
        break;
    case XmlWriterState_DocClosed:
        return WR_E_INVALIDACTION;
    default:
        ;
    }

    sprintfW(bufW, fmtW, ch);
    write_output_buffer(This->output, bufW, -1);

    return S_OK;
}

static HRESULT WINAPI xmlwriter_WriteChars(IXmlWriter *iface, const WCHAR *pwch, UINT cwch)
{
    xmlwriter *This = impl_from_IXmlWriter(iface);

    FIXME("%p %s %d\n", This, wine_dbgstr_w(pwch), cwch);

    switch (This->state)
    {
    case XmlWriterState_Initial:
        return E_UNEXPECTED;
    case XmlWriterState_InvalidEncoding:
        return MX_E_ENCODING;
    case XmlWriterState_DocClosed:
        return WR_E_INVALIDACTION;
    default:
        ;
    }

    return E_NOTIMPL;
}


static HRESULT WINAPI xmlwriter_WriteComment(IXmlWriter *iface, LPCWSTR comment)
{
    static const WCHAR copenW[] = {'<','!','-','-'};
    static const WCHAR ccloseW[] = {'-','-','>'};
    xmlwriter *This = impl_from_IXmlWriter(iface);

    TRACE("%p %s\n", This, debugstr_w(comment));

    switch (This->state)
    {
    case XmlWriterState_Initial:
        return E_UNEXPECTED;
    case XmlWriterState_InvalidEncoding:
        return MX_E_ENCODING;
    case XmlWriterState_ElemStarted:
        writer_close_starttag(This);
        break;
    case XmlWriterState_DocClosed:
        return WR_E_INVALIDACTION;
    default:
        ;
    }

    write_node_indent(This);
    write_output_buffer(This->output, copenW, ARRAY_SIZE(copenW));
    if (comment) {
        int len = strlenW(comment), i;

        /* Make sure there's no two hyphen sequences in a string, space is used as a separator to produce compliant
           comment string */
        if (len > 1) {
            for (i = 0; i < len; i++) {
                write_output_buffer(This->output, comment + i, 1);
                if (comment[i] == '-' && (i + 1 < len) && comment[i+1] == '-')
                    write_output_buffer(This->output, spaceW, ARRAY_SIZE(spaceW));
            }
        }
        else
            write_output_buffer(This->output, comment, len);

        if (len && comment[len-1] == '-')
            write_output_buffer(This->output, spaceW, ARRAY_SIZE(spaceW));
    }
    write_output_buffer(This->output, ccloseW, ARRAY_SIZE(ccloseW));

    return S_OK;
}

static HRESULT WINAPI xmlwriter_WriteDocType(IXmlWriter *iface, LPCWSTR pwszName, LPCWSTR pwszPublicId,
                               LPCWSTR pwszSystemId, LPCWSTR pwszSubset)
{
    xmlwriter *This = impl_from_IXmlWriter(iface);

    FIXME("%p %s %s %s %s\n", This, wine_dbgstr_w(pwszName), wine_dbgstr_w(pwszPublicId),
                        wine_dbgstr_w(pwszSystemId), wine_dbgstr_w(pwszSubset));

    return E_NOTIMPL;
}

static HRESULT WINAPI xmlwriter_WriteElementString(IXmlWriter *iface, LPCWSTR prefix,
                                     LPCWSTR local_name, LPCWSTR uri, LPCWSTR value)
{
    xmlwriter *This = impl_from_IXmlWriter(iface);

    TRACE("(%p)->(%s %s %s %s)\n", This, wine_dbgstr_w(prefix), wine_dbgstr_w(local_name),
                        wine_dbgstr_w(uri), wine_dbgstr_w(value));

    switch (This->state)
    {
    case XmlWriterState_Initial:
        return E_UNEXPECTED;
    case XmlWriterState_InvalidEncoding:
        return MX_E_ENCODING;
    case XmlWriterState_ElemStarted:
        writer_close_starttag(This);
        break;
    case XmlWriterState_DocClosed:
        return WR_E_INVALIDACTION;
    default:
        ;
    }

    write_encoding_bom(This);
    write_node_indent(This);
    write_output_buffer(This->output, ltW, ARRAY_SIZE(ltW));
    write_output_qname(This->output, prefix, local_name);

    if (value)
    {
        write_output_buffer(This->output, gtW, ARRAY_SIZE(gtW));
        write_output_buffer(This->output, value, -1);
        write_output_buffer(This->output, closeelementW, ARRAY_SIZE(closeelementW));
        write_output_qname(This->output, prefix, local_name);
        write_output_buffer(This->output, gtW, ARRAY_SIZE(gtW));
    }
    else
        write_output_buffer(This->output, closetagW, ARRAY_SIZE(closetagW));

    This->state = XmlWriterState_Content;

    return S_OK;
}

static HRESULT WINAPI xmlwriter_WriteEndDocument(IXmlWriter *iface)
{
    xmlwriter *This = impl_from_IXmlWriter(iface);

    TRACE("%p\n", This);

    switch (This->state)
    {
    case XmlWriterState_Initial:
        return E_UNEXPECTED;
    case XmlWriterState_Ready:
    case XmlWriterState_DocClosed:
        This->state = XmlWriterState_DocClosed;
        return WR_E_INVALIDACTION;
    case XmlWriterState_InvalidEncoding:
        return MX_E_ENCODING;
    default:
        ;
    }

    /* empty element stack */
    while (IXmlWriter_WriteEndElement(iface) == S_OK)
        ;

    This->state = XmlWriterState_DocClosed;
    return S_OK;
}

static HRESULT WINAPI xmlwriter_WriteEndElement(IXmlWriter *iface)
{
    xmlwriter *This = impl_from_IXmlWriter(iface);
    struct element *element;

    TRACE("%p\n", This);

    switch (This->state)
    {
    case XmlWriterState_Initial:
        return E_UNEXPECTED;
    case XmlWriterState_Ready:
    case XmlWriterState_DocClosed:
        This->state = XmlWriterState_DocClosed;
        return WR_E_INVALIDACTION;
    case XmlWriterState_InvalidEncoding:
        return MX_E_ENCODING;
    default:
        ;
    }

    element = pop_element(This);
    if (!element)
        return WR_E_INVALIDACTION;

    writer_dec_indent(This);

    if (This->starttagopen)
    {
        write_output_buffer(This->output, closetagW, ARRAY_SIZE(closetagW));
        This->starttagopen = FALSE;
    }
    else {
        /* write full end tag */
        write_node_indent(This);
        write_output_buffer(This->output, closeelementW, ARRAY_SIZE(closeelementW));
        write_output_buffer(This->output, element->qname, element->len);
        write_output_buffer(This->output, gtW, ARRAY_SIZE(gtW));
    }

    return S_OK;
}

static HRESULT WINAPI xmlwriter_WriteEntityRef(IXmlWriter *iface, LPCWSTR pwszName)
{
    xmlwriter *This = impl_from_IXmlWriter(iface);

    FIXME("%p %s\n", This, wine_dbgstr_w(pwszName));

    switch (This->state)
    {
    case XmlWriterState_Initial:
        return E_UNEXPECTED;
    case XmlWriterState_InvalidEncoding:
        return MX_E_ENCODING;
    case XmlWriterState_DocClosed:
        return WR_E_INVALIDACTION;
    default:
        ;
    }

    return E_NOTIMPL;
}

static HRESULT WINAPI xmlwriter_WriteFullEndElement(IXmlWriter *iface)
{
    xmlwriter *This = impl_from_IXmlWriter(iface);
    struct element *element;

    TRACE("%p\n", This);

    switch (This->state)
    {
    case XmlWriterState_Initial:
        return E_UNEXPECTED;
    case XmlWriterState_Ready:
    case XmlWriterState_DocClosed:
        This->state = XmlWriterState_DocClosed;
        return WR_E_INVALIDACTION;
    case XmlWriterState_InvalidEncoding:
        return MX_E_ENCODING;
    default:
        ;
    }

    element = pop_element(This);
    if (!element)
        return WR_E_INVALIDACTION;

    writer_close_starttag(This);
    writer_dec_indent(This);

    /* don't force full end tag to the next line */
    if (This->state == XmlWriterState_ElemStarted)
        This->state = XmlWriterState_Content;
    else
        write_node_indent(This);

    /* write full end tag */
    write_output_buffer(This->output, closeelementW, ARRAY_SIZE(closeelementW));
    write_output_buffer(This->output, element->qname, element->len);
    write_output_buffer(This->output, gtW, ARRAY_SIZE(gtW));

    return S_OK;
}

static HRESULT WINAPI xmlwriter_WriteName(IXmlWriter *iface, LPCWSTR pwszName)
{
    xmlwriter *This = impl_from_IXmlWriter(iface);

    FIXME("%p %s\n", This, wine_dbgstr_w(pwszName));

    switch (This->state)
    {
    case XmlWriterState_Initial:
        return E_UNEXPECTED;
    case XmlWriterState_Ready:
    case XmlWriterState_DocClosed:
        This->state = XmlWriterState_DocClosed;
        return WR_E_INVALIDACTION;
    case XmlWriterState_InvalidEncoding:
        return MX_E_ENCODING;
    default:
        ;
    }

    return E_NOTIMPL;
}

static HRESULT WINAPI xmlwriter_WriteNmToken(IXmlWriter *iface, LPCWSTR pwszNmToken)
{
    xmlwriter *This = impl_from_IXmlWriter(iface);

    FIXME("%p %s\n", This, wine_dbgstr_w(pwszNmToken));

    switch (This->state)
    {
    case XmlWriterState_Initial:
        return E_UNEXPECTED;
    case XmlWriterState_Ready:
    case XmlWriterState_DocClosed:
        This->state = XmlWriterState_DocClosed;
        return WR_E_INVALIDACTION;
    case XmlWriterState_InvalidEncoding:
        return MX_E_ENCODING;
    default:
        ;
    }

    return E_NOTIMPL;
}

static HRESULT WINAPI xmlwriter_WriteNode(IXmlWriter *iface, IXmlReader *pReader,
                            BOOL fWriteDefaultAttributes)
{
    xmlwriter *This = impl_from_IXmlWriter(iface);

    FIXME("%p %p %d\n", This, pReader, fWriteDefaultAttributes);

    return E_NOTIMPL;
}

static HRESULT WINAPI xmlwriter_WriteNodeShallow(IXmlWriter *iface, IXmlReader *pReader,
                                   BOOL fWriteDefaultAttributes)
{
    xmlwriter *This = impl_from_IXmlWriter(iface);

    FIXME("%p %p %d\n", This, pReader, fWriteDefaultAttributes);

    return E_NOTIMPL;
}

static HRESULT WINAPI xmlwriter_WriteProcessingInstruction(IXmlWriter *iface, LPCWSTR name,
                                             LPCWSTR text)
{
    xmlwriter *This = impl_from_IXmlWriter(iface);
    static const WCHAR xmlW[] = {'x','m','l',0};
    static const WCHAR openpiW[] = {'<','?'};

    TRACE("(%p)->(%s %s)\n", This, wine_dbgstr_w(name), wine_dbgstr_w(text));

    switch (This->state)
    {
    case XmlWriterState_Initial:
        return E_UNEXPECTED;
    case XmlWriterState_InvalidEncoding:
        return MX_E_ENCODING;
    case XmlWriterState_DocStarted:
        if (!strcmpW(name, xmlW))
            return WR_E_INVALIDACTION;
        break;
    case XmlWriterState_ElemStarted:
    case XmlWriterState_DocClosed:
        return WR_E_INVALIDACTION;
    default:
        ;
    }

    write_encoding_bom(This);
    write_node_indent(This);
    write_output_buffer(This->output, openpiW, ARRAY_SIZE(openpiW));
    write_output_buffer(This->output, name, -1);
    write_output_buffer(This->output, spaceW, ARRAY_SIZE(spaceW));
    write_output_buffer(This->output, text, -1);
    write_output_buffer(This->output, closepiW, ARRAY_SIZE(closepiW));

    if (!strcmpW(name, xmlW))
        This->state = XmlWriterState_PIDocStarted;

    return S_OK;
}

static HRESULT WINAPI xmlwriter_WriteQualifiedName(IXmlWriter *iface, LPCWSTR pwszLocalName,
                                     LPCWSTR pwszNamespaceUri)
{
    xmlwriter *This = impl_from_IXmlWriter(iface);

    FIXME("%p %s %s\n", This, wine_dbgstr_w(pwszLocalName), wine_dbgstr_w(pwszNamespaceUri));

    switch (This->state)
    {
    case XmlWriterState_Initial:
        return E_UNEXPECTED;
    case XmlWriterState_InvalidEncoding:
        return MX_E_ENCODING;
    case XmlWriterState_DocClosed:
        return WR_E_INVALIDACTION;
    default:
        ;
    }

    return E_NOTIMPL;
}

static HRESULT WINAPI xmlwriter_WriteRaw(IXmlWriter *iface, LPCWSTR data)
{
    xmlwriter *This = impl_from_IXmlWriter(iface);

    TRACE("%p %s\n", This, debugstr_w(data));

    if (!data)
        return S_OK;

    switch (This->state)
    {
    case XmlWriterState_Initial:
        return E_UNEXPECTED;
    case XmlWriterState_Ready:
        write_xmldecl(This, XmlStandalone_Omit);
        /* fallthrough */
    case XmlWriterState_DocStarted:
    case XmlWriterState_PIDocStarted:
        break;
    case XmlWriterState_InvalidEncoding:
        return MX_E_ENCODING;
    default:
        This->state = XmlWriterState_DocClosed;
        return WR_E_INVALIDACTION;
    }

    write_output_buffer(This->output, data, -1);
    return S_OK;
}

static HRESULT WINAPI xmlwriter_WriteRawChars(IXmlWriter *iface,  const WCHAR *pwch, UINT cwch)
{
    xmlwriter *This = impl_from_IXmlWriter(iface);

    FIXME("%p %s %d\n", This, wine_dbgstr_w(pwch), cwch);

    switch (This->state)
    {
    case XmlWriterState_Initial:
        return E_UNEXPECTED;
    case XmlWriterState_InvalidEncoding:
        return MX_E_ENCODING;
    case XmlWriterState_DocClosed:
        return WR_E_INVALIDACTION;
    default:
        ;
    }

    return E_NOTIMPL;
}

static HRESULT WINAPI xmlwriter_WriteStartDocument(IXmlWriter *iface, XmlStandalone standalone)
{
    xmlwriter *This = impl_from_IXmlWriter(iface);

    TRACE("(%p)->(%d)\n", This, standalone);

    switch (This->state)
    {
    case XmlWriterState_Initial:
        return E_UNEXPECTED;
    case XmlWriterState_PIDocStarted:
        This->state = XmlWriterState_DocStarted;
        return S_OK;
    case XmlWriterState_Ready:
        break;
    case XmlWriterState_InvalidEncoding:
        return MX_E_ENCODING;
    default:
        This->state = XmlWriterState_DocClosed;
        return WR_E_INVALIDACTION;
    }

    return write_xmldecl(This, standalone);
}

static HRESULT WINAPI xmlwriter_WriteStartElement(IXmlWriter *iface, LPCWSTR prefix, LPCWSTR local_name, LPCWSTR uri)
{
    xmlwriter *This = impl_from_IXmlWriter(iface);
    struct element *element;

    TRACE("(%p)->(%s %s %s)\n", This, wine_dbgstr_w(prefix), wine_dbgstr_w(local_name), wine_dbgstr_w(uri));

    if (!local_name)
        return E_INVALIDARG;

    switch (This->state)
    {
    case XmlWriterState_Initial:
        return E_UNEXPECTED;
    case XmlWriterState_InvalidEncoding:
        return MX_E_ENCODING;
    case XmlWriterState_DocClosed:
        return WR_E_INVALIDACTION;
    default:
        ;
    }

    /* close pending element */
    if (This->starttagopen)
        write_output_buffer(This->output, gtW, ARRAY_SIZE(gtW));

    element = alloc_element(This, prefix, local_name);
    if (!element)
        return E_OUTOFMEMORY;

    write_encoding_bom(This);
    write_node_indent(This);

    This->state = XmlWriterState_ElemStarted;
    This->starttagopen = TRUE;

    push_element(This, element);

    write_output_buffer(This->output, ltW, ARRAY_SIZE(ltW));
    write_output_qname(This->output, prefix, local_name);
    writer_inc_indent(This);

    return S_OK;
}

static void write_escaped_string(xmlwriter *writer, const WCHAR *string)
{
    static const WCHAR ampW[] = {'&','a','m','p',';'};
    static const WCHAR ltW[] = {'&','l','t',';'};
    static const WCHAR gtW[] = {'&','g','t',';'};

    while (*string)
    {
        switch (*string)
        {
        case '<':
            write_output_buffer(writer->output, ltW, ARRAY_SIZE(ltW));
            break;
        case '&':
            write_output_buffer(writer->output, ampW, ARRAY_SIZE(ampW));
            break;
        case '>':
            write_output_buffer(writer->output, gtW, ARRAY_SIZE(gtW));
            break;
        default:
            write_output_buffer(writer->output, string, 1);
        }

        string++;
    }
}

static HRESULT WINAPI xmlwriter_WriteString(IXmlWriter *iface, const WCHAR *string)
{
    xmlwriter *This = impl_from_IXmlWriter(iface);

    TRACE("%p %s\n", This, debugstr_w(string));

    if (!string)
        return S_OK;

    switch (This->state)
    {
    case XmlWriterState_Initial:
        return E_UNEXPECTED;
    case XmlWriterState_ElemStarted:
        writer_close_starttag(This);
        break;
    case XmlWriterState_Ready:
    case XmlWriterState_DocClosed:
        This->state = XmlWriterState_DocClosed;
        return WR_E_INVALIDACTION;
    case XmlWriterState_InvalidEncoding:
        return MX_E_ENCODING;
    default:
        ;
    }

    write_escaped_string(This, string);
    return S_OK;
}

static HRESULT WINAPI xmlwriter_WriteSurrogateCharEntity(IXmlWriter *iface, WCHAR wchLow, WCHAR wchHigh)
{
    xmlwriter *This = impl_from_IXmlWriter(iface);

    FIXME("%p %d %d\n", This, wchLow, wchHigh);

    return E_NOTIMPL;
}

static HRESULT WINAPI xmlwriter_WriteWhitespace(IXmlWriter *iface, LPCWSTR pwszWhitespace)
{
    xmlwriter *This = impl_from_IXmlWriter(iface);

    FIXME("%p %s\n", This, wine_dbgstr_w(pwszWhitespace));

    return E_NOTIMPL;
}

static HRESULT WINAPI xmlwriter_Flush(IXmlWriter *iface)
{
    xmlwriter *This = impl_from_IXmlWriter(iface);

    TRACE("%p\n", This);

    return writeroutput_flush_stream(This->output);
}

static const struct IXmlWriterVtbl xmlwriter_vtbl =
{
    xmlwriter_QueryInterface,
    xmlwriter_AddRef,
    xmlwriter_Release,
    xmlwriter_SetOutput,
    xmlwriter_GetProperty,
    xmlwriter_SetProperty,
    xmlwriter_WriteAttributes,
    xmlwriter_WriteAttributeString,
    xmlwriter_WriteCData,
    xmlwriter_WriteCharEntity,
    xmlwriter_WriteChars,
    xmlwriter_WriteComment,
    xmlwriter_WriteDocType,
    xmlwriter_WriteElementString,
    xmlwriter_WriteEndDocument,
    xmlwriter_WriteEndElement,
    xmlwriter_WriteEntityRef,
    xmlwriter_WriteFullEndElement,
    xmlwriter_WriteName,
    xmlwriter_WriteNmToken,
    xmlwriter_WriteNode,
    xmlwriter_WriteNodeShallow,
    xmlwriter_WriteProcessingInstruction,
    xmlwriter_WriteQualifiedName,
    xmlwriter_WriteRaw,
    xmlwriter_WriteRawChars,
    xmlwriter_WriteStartDocument,
    xmlwriter_WriteStartElement,
    xmlwriter_WriteString,
    xmlwriter_WriteSurrogateCharEntity,
    xmlwriter_WriteWhitespace,
    xmlwriter_Flush
};

/** IXmlWriterOutput **/
static HRESULT WINAPI xmlwriteroutput_QueryInterface(IXmlWriterOutput *iface, REFIID riid, void** ppvObject)
{
    xmlwriteroutput *This = impl_from_IXmlWriterOutput(iface);

    TRACE("(%p)->(%s %p)\n", This, debugstr_guid(riid), ppvObject);

    if (IsEqualGUID(riid, &IID_IXmlWriterOutput) ||
        IsEqualGUID(riid, &IID_IUnknown))
    {
        *ppvObject = iface;
    }
    else
    {
        WARN("interface %s not implemented\n", debugstr_guid(riid));
        *ppvObject = NULL;
        return E_NOINTERFACE;
    }

    IUnknown_AddRef(iface);

    return S_OK;
}

static ULONG WINAPI xmlwriteroutput_AddRef(IXmlWriterOutput *iface)
{
    xmlwriteroutput *This = impl_from_IXmlWriterOutput(iface);
    ULONG ref = InterlockedIncrement(&This->ref);
    TRACE("(%p)->(%d)\n", This, ref);
    return ref;
}

static ULONG WINAPI xmlwriteroutput_Release(IXmlWriterOutput *iface)
{
    xmlwriteroutput *This = impl_from_IXmlWriterOutput(iface);
    LONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p)->(%d)\n", This, ref);

    if (ref == 0)
    {
        IMalloc *imalloc = This->imalloc;
        if (This->output) IUnknown_Release(This->output);
        if (This->stream) ISequentialStream_Release(This->stream);
        free_output_buffer(This);
        writeroutput_free(This, This->encoding_name);
        writeroutput_free(This, This);
        if (imalloc) IMalloc_Release(imalloc);
    }

    return ref;
}

static const struct IUnknownVtbl xmlwriteroutputvtbl =
{
    xmlwriteroutput_QueryInterface,
    xmlwriteroutput_AddRef,
    xmlwriteroutput_Release
};

HRESULT WINAPI CreateXmlWriter(REFIID riid, void **obj, IMalloc *imalloc)
{
    xmlwriter *writer;
    HRESULT hr;

    TRACE("(%s, %p, %p)\n", wine_dbgstr_guid(riid), obj, imalloc);

    if (imalloc)
        writer = IMalloc_Alloc(imalloc, sizeof(*writer));
    else
        writer = heap_alloc(sizeof(*writer));
    if(!writer) return E_OUTOFMEMORY;

    writer->IXmlWriter_iface.lpVtbl = &xmlwriter_vtbl;
    writer->ref = 1;
    writer->imalloc = imalloc;
    if (imalloc) IMalloc_AddRef(imalloc);
    writer->output = NULL;
    writer->indent_level = 0;
    writer->indent = FALSE;
    writer->bom = TRUE;
    writer->omitxmldecl = FALSE;
    writer->conformance = XmlConformanceLevel_Document;
    writer->state = XmlWriterState_Initial;
    writer->bomwritten = FALSE;
    writer->starttagopen = FALSE;
    list_init(&writer->elements);

    hr = IXmlWriter_QueryInterface(&writer->IXmlWriter_iface, riid, obj);
    IXmlWriter_Release(&writer->IXmlWriter_iface);

    TRACE("returning iface %p, hr %#x\n", *obj, hr);

    return hr;
}

static HRESULT create_writer_output(IUnknown *stream, IMalloc *imalloc, xml_encoding encoding,
    const WCHAR *encoding_name, IXmlWriterOutput **output)
{
    xmlwriteroutput *writeroutput;
    HRESULT hr;

    *output = NULL;

    if (imalloc)
        writeroutput = IMalloc_Alloc(imalloc, sizeof(*writeroutput));
    else
        writeroutput = heap_alloc(sizeof(*writeroutput));
    if (!writeroutput)
        return E_OUTOFMEMORY;

    writeroutput->IXmlWriterOutput_iface.lpVtbl = &xmlwriteroutputvtbl;
    writeroutput->ref = 1;
    writeroutput->imalloc = imalloc;
    if (imalloc)
        IMalloc_AddRef(imalloc);
    writeroutput->encoding = encoding;
    writeroutput->stream = NULL;
    hr = init_output_buffer(writeroutput);
    if (FAILED(hr)) {
        IUnknown_Release(&writeroutput->IXmlWriterOutput_iface);
        return hr;
    }

    if (encoding_name) {
        unsigned int size = (strlenW(encoding_name) + 1) * sizeof(WCHAR);
        writeroutput->encoding_name = writeroutput_alloc(writeroutput, size);
        memcpy(writeroutput->encoding_name, encoding_name, size);
    }
    else
        writeroutput->encoding_name = NULL;

    IUnknown_QueryInterface(stream, &IID_IUnknown, (void**)&writeroutput->output);

    *output = &writeroutput->IXmlWriterOutput_iface;

    TRACE("returning iface %p\n", *output);

    return S_OK;
}

HRESULT WINAPI CreateXmlWriterOutputWithEncodingName(IUnknown *stream,
                                                     IMalloc *imalloc,
                                                     LPCWSTR encoding,
                                                     IXmlWriterOutput **output)
{
    static const WCHAR utf8W[] = {'U','T','F','-','8',0};
    xml_encoding xml_enc;

    TRACE("%p %p %s %p\n", stream, imalloc, debugstr_w(encoding), output);

    if (!stream || !output) return E_INVALIDARG;

    xml_enc = parse_encoding_name(encoding ? encoding : utf8W, -1);
    return create_writer_output(stream, imalloc, xml_enc, encoding, output);
}

HRESULT WINAPI CreateXmlWriterOutputWithEncodingCodePage(IUnknown *stream,
                                                         IMalloc *imalloc,
                                                         UINT codepage,
                                                         IXmlWriterOutput **output)
{
    xml_encoding xml_enc;

    TRACE("%p %p %u %p\n", stream, imalloc, codepage, output);

    if (!stream || !output) return E_INVALIDARG;

    xml_enc = get_encoding_from_codepage(codepage);
    return create_writer_output(stream, imalloc, xml_enc, NULL, output);
}
