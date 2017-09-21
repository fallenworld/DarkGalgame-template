/*
 * Copyright 2016 Hans Leidekker for CodeWeavers
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

#include "windef.h"
#include "winbase.h"
#include "winuser.h"
#include "rpc.h"
#include "webservices.h"

#include "wine/debug.h"
#include "wine/list.h"
#include "wine/unicode.h"
#include "webservices_private.h"
#include "sock.h"

WINE_DEFAULT_DEBUG_CHANNEL(webservices);

static const struct prop_desc channel_props[] =
{
    { sizeof(ULONG), FALSE },                               /* WS_CHANNEL_PROPERTY_MAX_BUFFERED_MESSAGE_SIZE */
    { sizeof(UINT64), FALSE },                              /* WS_CHANNEL_PROPERTY_MAX_STREAMED_MESSAGE_SIZE */
    { sizeof(ULONG), FALSE },                               /* WS_CHANNEL_PROPERTY_MAX_STREAMED_START_SIZE */
    { sizeof(ULONG), FALSE },                               /* WS_CHANNEL_PROPERTY_MAX_STREAMED_FLUSH_SIZE */
    { sizeof(WS_ENCODING), TRUE },                          /* WS_CHANNEL_PROPERTY_ENCODING */
    { sizeof(WS_ENVELOPE_VERSION), FALSE },                 /* WS_CHANNEL_PROPERTY_ENVELOPE_VERSION */
    { sizeof(WS_ADDRESSING_VERSION), FALSE },               /* WS_CHANNEL_PROPERTY_ADDRESSING_VERSION */
    { sizeof(ULONG), FALSE },                               /* WS_CHANNEL_PROPERTY_MAX_SESSION_DICTIONARY_SIZE */
    { sizeof(WS_CHANNEL_STATE), TRUE },                     /* WS_CHANNEL_PROPERTY_STATE */
    { sizeof(WS_CALLBACK_MODEL), FALSE },                   /* WS_CHANNEL_PROPERTY_ASYNC_CALLBACK_MODEL */
    { sizeof(WS_IP_VERSION), FALSE },                       /* WS_CHANNEL_PROPERTY_IP_VERSION */
    { sizeof(ULONG), FALSE },                               /* WS_CHANNEL_PROPERTY_RESOLVE_TIMEOUT */
    { sizeof(ULONG), FALSE },                               /* WS_CHANNEL_PROPERTY_CONNECT_TIMEOUT */
    { sizeof(ULONG), FALSE },                               /* WS_CHANNEL_PROPERTY_SEND_TIMEOUT */
    { sizeof(ULONG), FALSE },                               /* WS_CHANNEL_PROPERTY_RECEIVE_RESPONSE_TIMEOUT */
    { sizeof(ULONG), FALSE },                               /* WS_CHANNEL_PROPERTY_RECEIVE_TIMEOUT */
    { sizeof(ULONG), FALSE },                               /* WS_CHANNEL_PROPERTY_CLOSE_TIMEOUT */
    { sizeof(BOOL), FALSE },                                /* WS_CHANNEL_PROPERTY_ENABLE_TIMEOUTS */
    { sizeof(WS_TRANSFER_MODE), FALSE },                    /* WS_CHANNEL_PROPERTY_TRANSFER_MODE */
    { sizeof(ULONG), FALSE },                               /* WS_CHANNEL_PROPERTY_MULTICAST_INTERFACE */
    { sizeof(ULONG), FALSE },                               /* WS_CHANNEL_PROPERTY_MULTICAST_HOPS */
    { sizeof(WS_ENDPOINT_ADDRESS), TRUE },                  /* WS_CHANNEL_PROPERTY_REMOTE_ADDRESS */
    { sizeof(SOCKADDR_STORAGE), TRUE },                     /* WS_CHANNEL_PROPERTY_REMOTE_IP_ADDRESS */
    { sizeof(ULONGLONG), TRUE },                            /* WS_CHANNEL_PROPERTY_HTTP_CONNECTION_ID */
    { sizeof(WS_CUSTOM_CHANNEL_CALLBACKS), FALSE },         /* WS_CHANNEL_PROPERTY_CUSTOM_CHANNEL_CALLBACKS */
    { 0, FALSE },                                           /* WS_CHANNEL_PROPERTY_CUSTOM_CHANNEL_PARAMETERS */
    { sizeof(void *), FALSE },                              /* WS_CHANNEL_PROPERTY_CUSTOM_CHANNEL_INSTANCE */
    { sizeof(WS_STRING), TRUE },                            /* WS_CHANNEL_PROPERTY_TRANSPORT_URL */
    { sizeof(BOOL), FALSE },                                /* WS_CHANNEL_PROPERTY_NO_DELAY */
    { sizeof(BOOL), FALSE },                                /* WS_CHANNEL_PROPERTY_SEND_KEEP_ALIVES */
    { sizeof(ULONG), FALSE },                               /* WS_CHANNEL_PROPERTY_KEEP_ALIVE_TIME */
    { sizeof(ULONG), FALSE },                               /* WS_CHANNEL_PROPERTY_KEEP_ALIVE_INTERVAL */
    { sizeof(ULONG), FALSE },                               /* WS_CHANNEL_PROPERTY_MAX_HTTP_SERVER_CONNECTIONS */
    { sizeof(BOOL), TRUE },                                 /* WS_CHANNEL_PROPERTY_IS_SESSION_SHUT_DOWN */
    { sizeof(WS_CHANNEL_TYPE), TRUE },                      /* WS_CHANNEL_PROPERTY_CHANNEL_TYPE */
    { sizeof(ULONG), FALSE },                               /* WS_CHANNEL_PROPERTY_TRIM_BUFFERED_MESSAGE_SIZE */
    { sizeof(WS_CHANNEL_ENCODER), FALSE },                  /* WS_CHANNEL_PROPERTY_ENCODER */
    { sizeof(WS_CHANNEL_DECODER), FALSE },                  /* WS_CHANNEL_PROPERTY_DECODER */
    { sizeof(WS_PROTECTION_LEVEL), TRUE },                  /* WS_CHANNEL_PROPERTY_PROTECTION_LEVEL */
    { sizeof(WS_COOKIE_MODE), FALSE },                      /* WS_CHANNEL_PROPERTY_COOKIE_MODE */
    { sizeof(WS_HTTP_PROXY_SETTING_MODE), FALSE },          /* WS_CHANNEL_PROPERTY_HTTP_PROXY_SETTING_MODE */
    { sizeof(WS_CUSTOM_HTTP_PROXY), FALSE },                /* WS_CHANNEL_PROPERTY_CUSTOM_HTTP_PROXY */
    { sizeof(WS_HTTP_MESSAGE_MAPPING), FALSE },             /* WS_CHANNEL_PROPERTY_HTTP_MESSAGE_MAPPING */
    { sizeof(BOOL), FALSE },                                /* WS_CHANNEL_PROPERTY_ENABLE_HTTP_REDIRECT */
    { sizeof(WS_HTTP_REDIRECT_CALLBACK_CONTEXT), FALSE },   /* WS_CHANNEL_PROPERTY_HTTP_REDIRECT_CALLBACK_CONTEXT */
    { sizeof(BOOL), FALSE },                                /* WS_CHANNEL_PROPERTY_FAULTS_AS_ERRORS */
    { sizeof(BOOL), FALSE },                                /* WS_CHANNEL_PROPERTY_ALLOW_UNSECURED_FAULTS */
    { sizeof(WCHAR *), TRUE },                              /* WS_CHANNEL_PROPERTY_HTTP_SERVER_SPN */
    { sizeof(WCHAR *), TRUE },                              /* WS_CHANNEL_PROPERTY_HTTP_PROXY_SPN */
    { sizeof(ULONG), FALSE }                                /* WS_CHANNEL_PROPERTY_MAX_HTTP_REQUEST_HEADERS_BUFFER_SIZE */
};

enum session_state
{
    SESSION_STATE_UNINITIALIZED,
    SESSION_STATE_SETUP_COMPLETE,
};

struct channel
{
    ULONG                   magic;
    CRITICAL_SECTION        cs;
    WS_CHANNEL_TYPE         type;
    WS_CHANNEL_BINDING      binding;
    WS_CHANNEL_STATE        state;
    WS_ENDPOINT_ADDRESS     addr;
    WS_XML_WRITER          *writer;
    WS_XML_READER          *reader;
    WS_MESSAGE             *msg;
    WS_ENCODING             encoding;
    enum session_state      session_state;
    struct dictionary       dict;
    union
    {
        struct
        {
            HINTERNET session;
            HINTERNET connect;
            HINTERNET request;
            WCHAR    *path;
            DWORD     flags;
        } http;
        struct
        {
            SOCKET socket;
        } tcp;
        struct
        {
            SOCKET socket;
        } udp;
    } u;
    char                   *read_buf;
    ULONG                   read_buflen;
    ULONG                   read_size;
    ULONG                   prop_count;
    struct prop             prop[sizeof(channel_props)/sizeof(channel_props[0])];
};

#define CHANNEL_MAGIC (('C' << 24) | ('H' << 16) | ('A' << 8) | 'N')

static struct channel *alloc_channel(void)
{
    static const ULONG count = sizeof(channel_props)/sizeof(channel_props[0]);
    struct channel *ret;
    ULONG size = sizeof(*ret) + prop_size( channel_props, count );

    if (!(ret = heap_alloc_zero( size ))) return NULL;

    ret->magic      = CHANNEL_MAGIC;
    InitializeCriticalSection( &ret->cs );
    ret->cs.DebugInfo->Spare[0] = (DWORD_PTR)(__FILE__ ": channel.cs");

    prop_init( channel_props, count, ret->prop, &ret[1] );
    ret->prop_count = count;
    return ret;
}

static void reset_channel( struct channel *channel )
{
    channel->state                = WS_CHANNEL_STATE_CREATED;
    heap_free( channel->addr.url.chars );
    channel->addr.url.chars       = NULL;
    channel->addr.url.length      = 0;
    channel->msg                  = NULL;
    channel->read_size            = 0;
    channel->session_state        = SESSION_STATE_UNINITIALIZED;
    clear_dict( &channel->dict );

    switch (channel->binding)
    {
    case WS_HTTP_CHANNEL_BINDING:
        WinHttpCloseHandle( channel->u.http.request );
        channel->u.http.request = NULL;
        WinHttpCloseHandle( channel->u.http.connect );
        channel->u.http.connect = NULL;
        WinHttpCloseHandle( channel->u.http.session );
        channel->u.http.session = NULL;
        heap_free( channel->u.http.path );
        channel->u.http.path    = NULL;
        channel->u.http.flags   = 0;
        break;

    case WS_TCP_CHANNEL_BINDING:
        closesocket( channel->u.tcp.socket );
        channel->u.tcp.socket = -1;
        break;

    case WS_UDP_CHANNEL_BINDING:
        closesocket( channel->u.udp.socket );
        channel->u.udp.socket = -1;
        break;

    default: break;
    }
}

static void free_channel( struct channel *channel )
{
    reset_channel( channel );

    WsFreeWriter( channel->writer );
    WsFreeReader( channel->reader );

    heap_free( channel->read_buf );

    channel->cs.DebugInfo->Spare[0] = 0;
    DeleteCriticalSection( &channel->cs );
    heap_free( channel );
}

static HRESULT create_channel( WS_CHANNEL_TYPE type, WS_CHANNEL_BINDING binding,
                               const WS_CHANNEL_PROPERTY *properties, ULONG count, struct channel **ret )
{
    struct channel *channel;
    ULONG i, msg_size = 65536;
    WS_ENVELOPE_VERSION env_version = WS_ENVELOPE_VERSION_SOAP_1_2;
    WS_ADDRESSING_VERSION addr_version = WS_ADDRESSING_VERSION_1_0;
    HRESULT hr;

    if (!(channel = alloc_channel())) return E_OUTOFMEMORY;

    prop_set( channel->prop, channel->prop_count, WS_CHANNEL_PROPERTY_MAX_BUFFERED_MESSAGE_SIZE,
              &msg_size, sizeof(msg_size) );
    prop_set( channel->prop, channel->prop_count, WS_CHANNEL_PROPERTY_ENVELOPE_VERSION,
              &env_version, sizeof(env_version) );
    prop_set( channel->prop, channel->prop_count, WS_CHANNEL_PROPERTY_ADDRESSING_VERSION,
              &addr_version, sizeof(addr_version) );

    channel->type    = type;
    channel->binding = binding;

    switch (channel->binding)
    {
    case WS_HTTP_CHANNEL_BINDING:
        channel->encoding = WS_ENCODING_XML_UTF8;
        break;

    case WS_TCP_CHANNEL_BINDING:
        channel->u.tcp.socket = -1;
        channel->encoding     = WS_ENCODING_XML_BINARY_SESSION_1;
        break;

    case WS_UDP_CHANNEL_BINDING:
        channel->u.udp.socket = -1;
        channel->encoding     = WS_ENCODING_XML_UTF8;
        break;

    default: break;
    }

    for (i = 0; i < count; i++)
    {
        switch (properties[i].id)
        {
        case WS_CHANNEL_PROPERTY_ENCODING:
            if (!properties[i].value || properties[i].valueSize != sizeof(channel->encoding))
            {
                free_channel( channel );
                return E_INVALIDARG;
            }
            channel->encoding = *(WS_ENCODING *)properties[i].value;
            break;

        default:
            if ((hr = prop_set( channel->prop, channel->prop_count, properties[i].id, properties[i].value,
                                properties[i].valueSize )) != S_OK)
            {
                free_channel( channel );
                return hr;
            }
            break;
        }
    }

    *ret = channel;
    return S_OK;
}

/**************************************************************************
 *          WsCreateChannel		[webservices.@]
 */
HRESULT WINAPI WsCreateChannel( WS_CHANNEL_TYPE type, WS_CHANNEL_BINDING binding,
                                const WS_CHANNEL_PROPERTY *properties, ULONG count,
                                const WS_SECURITY_DESCRIPTION *desc, WS_CHANNEL **handle,
                                WS_ERROR *error )
{
    struct channel *channel;
    HRESULT hr;

    TRACE( "%u %u %p %u %p %p %p\n", type, binding, properties, count, desc, handle, error );
    if (error) FIXME( "ignoring error parameter\n" );
    if (desc) FIXME( "ignoring security description\n" );

    if (!handle) return E_INVALIDARG;

    if (type != WS_CHANNEL_TYPE_REQUEST && type != WS_CHANNEL_TYPE_DUPLEX &&
        type != WS_CHANNEL_TYPE_DUPLEX_SESSION)
    {
        FIXME( "channel type %u not implemented\n", type );
        return E_NOTIMPL;
    }
    if (binding != WS_HTTP_CHANNEL_BINDING && binding != WS_TCP_CHANNEL_BINDING &&
        binding != WS_UDP_CHANNEL_BINDING)
    {
        FIXME( "channel binding %u not implemented\n", binding );
        return E_NOTIMPL;
    }

    if ((hr = create_channel( type, binding, properties, count, &channel )) != S_OK) return hr;

    *handle = (WS_CHANNEL *)channel;
    return S_OK;
}

/**************************************************************************
 *          WsCreateChannelForListener		[webservices.@]
 */
HRESULT WINAPI WsCreateChannelForListener( WS_LISTENER *listener_handle, const WS_CHANNEL_PROPERTY *properties,
                                           ULONG count, WS_CHANNEL **handle, WS_ERROR *error )
{
    struct channel *channel;
    WS_CHANNEL_TYPE type;
    WS_CHANNEL_BINDING binding;
    HRESULT hr;

    TRACE( "%p %p %u %p %p\n", listener_handle, properties, count, handle, error );
    if (error) FIXME( "ignoring error parameter\n" );

    if (!listener_handle || !handle) return E_INVALIDARG;

    if ((hr = WsGetListenerProperty( listener_handle, WS_LISTENER_PROPERTY_CHANNEL_TYPE, &type,
                                     sizeof(type), NULL )) != S_OK) return hr;

    if ((hr = WsGetListenerProperty( listener_handle, WS_LISTENER_PROPERTY_CHANNEL_BINDING, &binding,
                                     sizeof(binding), NULL )) != S_OK) return hr;

    if ((hr = create_channel( type, binding, properties, count, &channel )) != S_OK) return hr;

    *handle = (WS_CHANNEL *)channel;
    return S_OK;
}

/**************************************************************************
 *          WsFreeChannel		[webservices.@]
 */
void WINAPI WsFreeChannel( WS_CHANNEL *handle )
{
    struct channel *channel = (struct channel *)handle;

    TRACE( "%p\n", handle );

    if (!channel) return;

    EnterCriticalSection( &channel->cs );

    if (channel->magic != CHANNEL_MAGIC)
    {
        LeaveCriticalSection( &channel->cs );
        return;
    }

    channel->magic = 0;

    LeaveCriticalSection( &channel->cs );
    free_channel( channel );
}

/**************************************************************************
 *          WsResetChannel		[webservices.@]
 */
HRESULT WINAPI WsResetChannel( WS_CHANNEL *handle, WS_ERROR *error )
{
    struct channel *channel = (struct channel *)handle;

    TRACE( "%p %p\n", handle, error );
    if (error) FIXME( "ignoring error parameter\n" );

    if (!channel) return E_INVALIDARG;

    EnterCriticalSection( &channel->cs );

    if (channel->magic != CHANNEL_MAGIC)
    {
        LeaveCriticalSection( &channel->cs );
        return E_INVALIDARG;
    }

    if (channel->state != WS_CHANNEL_STATE_CREATED && channel->state != WS_CHANNEL_STATE_CLOSED)
    {
        LeaveCriticalSection( &channel->cs );
        return WS_E_INVALID_OPERATION;
    }

    reset_channel( channel );

    LeaveCriticalSection( &channel->cs );
    return S_OK;
}

/**************************************************************************
 *          WsGetChannelProperty		[webservices.@]
 */
HRESULT WINAPI WsGetChannelProperty( WS_CHANNEL *handle, WS_CHANNEL_PROPERTY_ID id, void *buf,
                                     ULONG size, WS_ERROR *error )
{
    struct channel *channel = (struct channel *)handle;
    HRESULT hr = S_OK;

    TRACE( "%p %u %p %u %p\n", handle, id, buf, size, error );
    if (error) FIXME( "ignoring error parameter\n" );

    if (!channel) return E_INVALIDARG;

    EnterCriticalSection( &channel->cs );

    if (channel->magic != CHANNEL_MAGIC)
    {
        LeaveCriticalSection( &channel->cs );
        return E_INVALIDARG;
    }

    switch (id)
    {
    case WS_CHANNEL_PROPERTY_CHANNEL_TYPE:
        if (!buf || size != sizeof(channel->type)) hr = E_INVALIDARG;
        else *(WS_CHANNEL_TYPE *)buf = channel->type;
        break;

    case WS_CHANNEL_PROPERTY_ENCODING:
        if (!buf || size != sizeof(channel->encoding)) hr = E_INVALIDARG;
        else *(WS_ENCODING *)buf = channel->encoding;
        break;

    default:
        hr = prop_get( channel->prop, channel->prop_count, id, buf, size );
    }

    LeaveCriticalSection( &channel->cs );
    return hr;
}

/**************************************************************************
 *          WsSetChannelProperty		[webservices.@]
 */
HRESULT WINAPI WsSetChannelProperty( WS_CHANNEL *handle, WS_CHANNEL_PROPERTY_ID id, const void *value,
                                     ULONG size, WS_ERROR *error )
{
    struct channel *channel = (struct channel *)handle;
    HRESULT hr;

    TRACE( "%p %u %p %u\n", handle, id, value, size );
    if (error) FIXME( "ignoring error parameter\n" );

    if (!channel) return E_INVALIDARG;

    EnterCriticalSection( &channel->cs );

    if (channel->magic != CHANNEL_MAGIC)
    {
        LeaveCriticalSection( &channel->cs );
        return E_INVALIDARG;
    }

    hr = prop_set( channel->prop, channel->prop_count, id, value, size );

    LeaveCriticalSection( &channel->cs );
    return hr;
}

static HRESULT open_channel( struct channel *channel, const WS_ENDPOINT_ADDRESS *endpoint )
{
    if (endpoint->headers || endpoint->extensions || endpoint->identity)
    {
        FIXME( "headers, extensions or identity not supported\n" );
        return E_NOTIMPL;
    }

    if (!(channel->addr.url.chars = heap_alloc( endpoint->url.length * sizeof(WCHAR) ))) return E_OUTOFMEMORY;
    memcpy( channel->addr.url.chars, endpoint->url.chars, endpoint->url.length * sizeof(WCHAR) );
    channel->addr.url.length = endpoint->url.length;

    channel->state = WS_CHANNEL_STATE_OPEN;
    return S_OK;
}

/**************************************************************************
 *          WsOpenChannel		[webservices.@]
 */
HRESULT WINAPI WsOpenChannel( WS_CHANNEL *handle, const WS_ENDPOINT_ADDRESS *endpoint,
                              const WS_ASYNC_CONTEXT *ctx, WS_ERROR *error )
{
    struct channel *channel = (struct channel *)handle;
    HRESULT hr;

    TRACE( "%p %p %p %p\n", handle, endpoint, ctx, error );
    if (error) FIXME( "ignoring error parameter\n" );
    if (ctx) FIXME( "ignoring ctx parameter\n" );

    if (!channel || !endpoint) return E_INVALIDARG;

    EnterCriticalSection( &channel->cs );

    if (channel->magic != CHANNEL_MAGIC)
    {
        LeaveCriticalSection( &channel->cs );
        return E_INVALIDARG;
    }

    if (channel->state != WS_CHANNEL_STATE_CREATED)
    {
        LeaveCriticalSection( &channel->cs );
        return WS_E_INVALID_OPERATION;
    }

    hr = open_channel( channel, endpoint );

    LeaveCriticalSection( &channel->cs );
    return hr;
}

static void close_channel( struct channel *channel )
{
    reset_channel( channel );
    channel->state = WS_CHANNEL_STATE_CLOSED;
}

/**************************************************************************
 *          WsCloseChannel		[webservices.@]
 */
HRESULT WINAPI WsCloseChannel( WS_CHANNEL *handle, const WS_ASYNC_CONTEXT *ctx, WS_ERROR *error )
{
    struct channel *channel = (struct channel *)handle;

    TRACE( "%p %p %p\n", handle, ctx, error );
    if (error) FIXME( "ignoring error parameter\n" );
    if (ctx) FIXME( "ignoring ctx parameter\n" );

    if (!channel) return E_INVALIDARG;

    EnterCriticalSection( &channel->cs );

    if (channel->magic != CHANNEL_MAGIC)
    {
        LeaveCriticalSection( &channel->cs );
        return E_INVALIDARG;
    }

    close_channel( channel );

    LeaveCriticalSection( &channel->cs );
    return S_OK;
}

static HRESULT parse_http_url( const WCHAR *url, ULONG len, URL_COMPONENTS *uc )
{
    HRESULT hr = E_OUTOFMEMORY;
    WCHAR *tmp;
    DWORD err;

    memset( uc, 0, sizeof(*uc) );
    uc->dwStructSize      = sizeof(*uc);
    uc->dwHostNameLength  = 128;
    uc->lpszHostName      = heap_alloc( uc->dwHostNameLength * sizeof(WCHAR) );
    uc->dwUrlPathLength   = 128;
    uc->lpszUrlPath       = heap_alloc( uc->dwUrlPathLength * sizeof(WCHAR) );
    uc->dwExtraInfoLength = 128;
    uc->lpszExtraInfo     = heap_alloc( uc->dwExtraInfoLength * sizeof(WCHAR) );
    if (!uc->lpszHostName || !uc->lpszUrlPath || !uc->lpszExtraInfo) goto error;

    if (!WinHttpCrackUrl( url, len, ICU_DECODE, uc ))
    {
        if ((err = GetLastError()) != ERROR_INSUFFICIENT_BUFFER)
        {
            hr = HRESULT_FROM_WIN32( err );
            goto error;
        }
        if (!(tmp = heap_realloc( uc->lpszHostName, uc->dwHostNameLength * sizeof(WCHAR) ))) goto error;
        uc->lpszHostName = tmp;
        if (!(tmp = heap_realloc( uc->lpszUrlPath, uc->dwUrlPathLength * sizeof(WCHAR) ))) goto error;
        uc->lpszUrlPath = tmp;
        if (!(tmp = heap_realloc( uc->lpszExtraInfo, uc->dwExtraInfoLength * sizeof(WCHAR) ))) goto error;
        uc->lpszExtraInfo = tmp;
        WinHttpCrackUrl( url, len, ICU_DECODE, uc );
    }

    return S_OK;

error:
    heap_free( uc->lpszHostName );
    heap_free( uc->lpszUrlPath );
    heap_free( uc->lpszExtraInfo );
    return hr;
}

static HRESULT connect_channel_http( struct channel *channel )
{
    static const WCHAR agentW[] =
        {'M','S','-','W','e','b','S','e','r','v','i','c','e','s','/','1','.','0',0};
    HINTERNET ses = NULL, con = NULL;
    URL_COMPONENTS uc;
    HRESULT hr;

    if (channel->u.http.connect) return S_OK;

    if ((hr = parse_http_url( channel->addr.url.chars, channel->addr.url.length, &uc )) != S_OK) return hr;
    if (!(channel->u.http.path = heap_alloc( (uc.dwUrlPathLength + uc.dwExtraInfoLength + 1) * sizeof(WCHAR) )))
    {
        hr = E_OUTOFMEMORY;
        goto done;
    }
    else
    {
        strcpyW( channel->u.http.path, uc.lpszUrlPath );
        if (uc.dwExtraInfoLength) strcatW( channel->u.http.path, uc.lpszExtraInfo );
    }

    channel->u.http.flags = WINHTTP_FLAG_REFRESH;
    switch (uc.nScheme)
    {
    case INTERNET_SCHEME_HTTP: break;
    case INTERNET_SCHEME_HTTPS:
        channel->u.http.flags |= WINHTTP_FLAG_SECURE;
        break;

    default:
        hr = WS_E_INVALID_ENDPOINT_URL;
        goto done;
    }

    if (!(ses = WinHttpOpen( agentW, 0, NULL, NULL, 0 )))
    {
        hr = HRESULT_FROM_WIN32( GetLastError() );
        goto done;
    }
    if (!(con = WinHttpConnect( ses, uc.lpszHostName, uc.nPort, 0 )))
    {
        hr = HRESULT_FROM_WIN32( GetLastError() );
        goto done;
    }

    channel->u.http.session = ses;
    channel->u.http.connect = con;

done:
    if (hr != S_OK)
    {
        WinHttpCloseHandle( con );
        WinHttpCloseHandle( ses );
    }
    heap_free( uc.lpszHostName );
    heap_free( uc.lpszUrlPath );
    heap_free( uc.lpszExtraInfo );
    return hr;
}

static HRESULT connect_channel_tcp( struct channel *channel )
{
    struct sockaddr_storage storage;
    struct sockaddr *addr = (struct sockaddr *)&storage;
    int addr_len;
    WS_URL_SCHEME_TYPE scheme;
    WCHAR *host;
    USHORT port;
    HRESULT hr;

    if (channel->u.tcp.socket != -1) return S_OK;

    if ((hr = parse_url( &channel->addr.url, &scheme, &host, &port )) != S_OK) return hr;
    if (scheme != WS_URL_NETTCP_SCHEME_TYPE)
    {
        heap_free( host );
        return WS_E_INVALID_ENDPOINT_URL;
    }

    winsock_init();

    hr = resolve_hostname( host, port, addr, &addr_len, 0 );
    heap_free( host );
    if (hr != S_OK) return hr;

    if ((channel->u.tcp.socket = socket( addr->sa_family, SOCK_STREAM, 0 )) == -1)
        return HRESULT_FROM_WIN32( WSAGetLastError() );

    if (connect( channel->u.tcp.socket, addr, addr_len ) < 0)
    {
        closesocket( channel->u.tcp.socket );
        channel->u.tcp.socket = -1;
        return HRESULT_FROM_WIN32( WSAGetLastError() );
    }

    return S_OK;
}

static HRESULT connect_channel_udp( struct channel *channel )
{
    struct sockaddr_storage storage;
    struct sockaddr *addr = (struct sockaddr *)&storage;
    int addr_len;
    WS_URL_SCHEME_TYPE scheme;
    WCHAR *host;
    USHORT port;
    HRESULT hr;

    if (channel->u.udp.socket != -1) return S_OK;

    if ((hr = parse_url( &channel->addr.url, &scheme, &host, &port )) != S_OK) return hr;
    if (scheme != WS_URL_SOAPUDP_SCHEME_TYPE)
    {
        heap_free( host );
        return WS_E_INVALID_ENDPOINT_URL;
    }

    winsock_init();

    hr = resolve_hostname( host, port, addr, &addr_len, 0 );
    heap_free( host );
    if (hr != S_OK) return hr;

    if ((channel->u.udp.socket = socket( addr->sa_family, SOCK_DGRAM, 0 )) == -1)
        return HRESULT_FROM_WIN32( WSAGetLastError() );

    if (connect( channel->u.udp.socket, addr, addr_len ) < 0)
    {
        closesocket( channel->u.udp.socket );
        channel->u.udp.socket = -1;
        return HRESULT_FROM_WIN32( WSAGetLastError() );
    }

    return S_OK;
}

static HRESULT connect_channel( struct channel *channel )
{
    switch (channel->binding)
    {
    case WS_HTTP_CHANNEL_BINDING:
        return connect_channel_http( channel );

    case WS_TCP_CHANNEL_BINDING:
        return connect_channel_tcp( channel );

    case WS_UDP_CHANNEL_BINDING:
        return connect_channel_udp( channel );

    default:
        ERR( "unhandled binding %u\n", channel->binding );
        return E_NOTIMPL;
    }
}

static HRESULT write_message( WS_MESSAGE *handle, WS_XML_WRITER *writer, const WS_ELEMENT_DESCRIPTION *desc,
                              WS_WRITE_OPTION option, const void *body, ULONG size )
{
    HRESULT hr;
    if ((hr = WsWriteEnvelopeStart( handle, writer, NULL, NULL, NULL )) != S_OK) return hr;
    if ((hr = WsWriteBody( handle, desc, option, body, size, NULL )) != S_OK) return hr;
    return WsWriteEnvelopeEnd( handle, NULL );
}

static HRESULT send_message_http( HINTERNET request, BYTE *data, ULONG len )
{
    if (!WinHttpSendRequest( request, NULL, 0, data, len, len, 0 ))
        return HRESULT_FROM_WIN32( GetLastError() );

    if (!WinHttpReceiveResponse( request, NULL ))
        return HRESULT_FROM_WIN32( GetLastError() );
    return S_OK;
}

static HRESULT send_byte( SOCKET socket, BYTE byte )
{
    int count = send( socket, (char *)&byte, 1, 0 );
    if (count < 0) return HRESULT_FROM_WIN32( WSAGetLastError() );
    if (count != 1) return WS_E_OTHER;
    return S_OK;
}

static HRESULT send_bytes( SOCKET socket, BYTE *bytes, int len )
{
    int count = send( socket, (char *)bytes, len, 0 );
    if (count < 0) return HRESULT_FROM_WIN32( WSAGetLastError() );
    if (count != len) return WS_E_OTHER;
    return S_OK;
}

static HRESULT send_size( SOCKET socket, ULONG size )
{
    HRESULT hr;
    if (size < 0x80) return send_byte( socket, size );
    if ((hr = send_byte( socket, (size & 0x7f) | 0x80 )) != S_OK) return hr;
    if ((size >>= 7) < 0x80) return send_byte( socket, size );
    if ((hr = send_byte( socket, (size & 0x7f) | 0x80 )) != S_OK) return hr;
    if ((size >>= 7) < 0x80) return send_byte( socket, size );
    if ((hr = send_byte( socket, (size & 0x7f) | 0x80 )) != S_OK) return hr;
    if ((size >>= 7) < 0x80) return send_byte( socket, size );
    if ((hr = send_byte( socket, (size & 0x7f) | 0x80 )) != S_OK) return hr;
    if ((size >>= 7) < 0x08) return send_byte( socket, size );
    return E_INVALIDARG;
}

enum frame_record_type
{
    FRAME_RECORD_TYPE_VERSION,
    FRAME_RECORD_TYPE_MODE,
    FRAME_RECORD_TYPE_VIA,
    FRAME_RECORD_TYPE_KNOWN_ENCODING,
    FRAME_RECORD_TYPE_EXTENSIBLE_ENCODING,
    FRAME_RECORD_TYPE_UNSIZED_ENVELOPE,
    FRAME_RECORD_TYPE_SIZED_ENVELOPE,
    FRAME_RECORD_TYPE_END,
    FRAME_RECORD_TYPE_FAULT,
    FRAME_RECORD_TYPE_UPGRADE_REQUEST,
    FRAME_RECORD_TYPE_UPGRADE_RESPONSE,
    FRAME_RECORD_TYPE_PREAMBLE_ACK,
    FRAME_RECORD_TYPE_PREAMBLE_END,
};

static inline ULONG size_length( ULONG size )
{
    if (size < 0x80) return 1;
    if (size < 0x4000) return 2;
    if (size < 0x200000) return 3;
    if (size < 0x10000000) return 4;
    return 5;
}

static ULONG string_table_size( const WS_XML_DICTIONARY *dict )
{
    ULONG i, size = 0;
    for (i = 0; i < dict->stringCount; i++)
        size += size_length( dict->strings[i].length ) + dict->strings[i].length;
    return size;
}

static HRESULT send_string_table( SOCKET socket, const WS_XML_DICTIONARY *dict )
{
    ULONG i;
    HRESULT hr;
    for (i = 0; i < dict->stringCount; i++)
    {
        if ((hr = send_size( socket, dict->strings[i].length )) != S_OK) return hr;
        if ((hr = send_bytes( socket, dict->strings[i].bytes, dict->strings[i].length )) != S_OK) return hr;
    }
    return S_OK;
}

static HRESULT string_to_utf8( const WS_STRING *str, unsigned char **ret, int *len )
{
    *len = WideCharToMultiByte( CP_UTF8, 0, str->chars, str->length, NULL, 0, NULL, NULL );
    if (!(*ret = heap_alloc( *len ))) return E_OUTOFMEMORY;
    WideCharToMultiByte( CP_UTF8, 0, str->chars, str->length, (char *)*ret, *len, NULL, NULL );
    return S_OK;
}

enum session_mode
{
    SESSION_MODE_INVALID    = 0,
    SESSION_MODE_SINGLETON  = 1,
    SESSION_MODE_DUPLEX     = 2,
    SESSION_MODE_SIMPLEX    = 3,
};

static enum session_mode map_channel_type( struct channel *channel )
{
    switch (channel->type)
    {
    case WS_CHANNEL_TYPE_DUPLEX_SESSION: return SESSION_MODE_DUPLEX;
    default:
        FIXME( "unhandled channel type %08x\n", channel->type );
        return SESSION_MODE_INVALID;
    }
}

enum known_encoding
{
    KNOWN_ENCODING_SOAP11_UTF8           = 0x00,
    KNOWN_ENCODING_SOAP11_UTF16          = 0x01,
    KNOWN_ENCODING_SOAP11_UTF16LE        = 0x02,
    KNOWN_ENCODING_SOAP12_UTF8           = 0x03,
    KNOWN_ENCODING_SOAP12_UTF16          = 0x04,
    KNOWN_ENCODING_SOAP12_UTF16LE        = 0x05,
    KNOWN_ENCODING_SOAP12_MTOM           = 0x06,
    KNOWN_ENCODING_SOAP12_BINARY         = 0x07,
    KNOWN_ENCODING_SOAP12_BINARY_SESSION = 0x08,
};

static enum known_encoding map_channel_encoding( struct channel *channel )
{
    WS_ENVELOPE_VERSION version;

    prop_get( channel->prop, channel->prop_count, WS_CHANNEL_PROPERTY_ENVELOPE_VERSION, &version, sizeof(version) );

    switch (version)
    {
    case WS_ENVELOPE_VERSION_SOAP_1_1:
        switch (channel->encoding)
        {
        case WS_ENCODING_XML_UTF8:      return KNOWN_ENCODING_SOAP11_UTF8;
        case WS_ENCODING_XML_UTF16LE:   return KNOWN_ENCODING_SOAP11_UTF16LE;
        default:
            FIXME( "unhandled version/encoding %u/%u\n", version, channel->encoding );
            return 0;
        }
    case WS_ENVELOPE_VERSION_SOAP_1_2:
        switch (channel->encoding)
        {
        case WS_ENCODING_XML_UTF8:              return KNOWN_ENCODING_SOAP12_UTF8;
        case WS_ENCODING_XML_UTF16LE:           return KNOWN_ENCODING_SOAP12_UTF16LE;
        case WS_ENCODING_XML_BINARY_1:          return KNOWN_ENCODING_SOAP12_BINARY;
        case WS_ENCODING_XML_BINARY_SESSION_1:  return KNOWN_ENCODING_SOAP12_BINARY_SESSION;
        default:
            FIXME( "unhandled version/encoding %u/%u\n", version, channel->encoding );
            return 0;
        }
    default:
        ERR( "unhandled version %u\n", version );
        return 0;
    }
}

#define FRAME_VERSION_MAJOR 1
#define FRAME_VERSION_MINOR 1

static HRESULT send_preamble( struct channel *channel )
{
    unsigned char *url;
    HRESULT hr;
    int len;

    if ((hr = send_byte( channel->u.tcp.socket, FRAME_RECORD_TYPE_VERSION )) != S_OK) return hr;
    if ((hr = send_byte( channel->u.tcp.socket, FRAME_VERSION_MAJOR )) != S_OK) return hr;
    if ((hr = send_byte( channel->u.tcp.socket, FRAME_VERSION_MINOR )) != S_OK) return hr;

    if ((hr = send_byte( channel->u.tcp.socket, FRAME_RECORD_TYPE_MODE )) != S_OK) return hr;
    if ((hr = send_byte( channel->u.tcp.socket, map_channel_type(channel) )) != S_OK) return hr;

    if ((hr = send_byte( channel->u.tcp.socket, FRAME_RECORD_TYPE_VIA )) != S_OK) return hr;
    if ((hr = string_to_utf8( &channel->addr.url, &url, &len )) != S_OK) return hr;
    if ((hr = send_size( channel->u.tcp.socket, len )) != S_OK) goto done;
    if ((hr = send_bytes( channel->u.tcp.socket, url, len )) != S_OK) goto done;

    if ((hr = send_byte( channel->u.tcp.socket, FRAME_RECORD_TYPE_KNOWN_ENCODING )) != S_OK) goto done;
    if ((hr = send_byte( channel->u.tcp.socket, map_channel_encoding(channel) )) != S_OK) goto done;
    hr = send_byte( channel->u.tcp.socket, FRAME_RECORD_TYPE_PREAMBLE_END );

done:
    heap_free( url );
    return hr;
}

static HRESULT receive_bytes( struct channel *channel, unsigned char *bytes, int len )
{
    int count = recv( channel->u.tcp.socket, (char *)bytes, len, 0 );
    if (count < 0) return HRESULT_FROM_WIN32( WSAGetLastError() );
    if (count != len) return WS_E_INVALID_FORMAT;
    return S_OK;
}

static HRESULT receive_preamble_ack( struct channel *channel )
{
    unsigned char byte;
    HRESULT hr;

    if ((hr = receive_bytes( channel, &byte, 1 )) != S_OK) return hr;
    if (byte != FRAME_RECORD_TYPE_PREAMBLE_ACK) return WS_E_INVALID_FORMAT;
    channel->session_state = SESSION_STATE_SETUP_COMPLETE;
    return S_OK;
}

static HRESULT send_sized_envelope( struct channel *channel, BYTE *data, ULONG len )
{
    ULONG table_size = string_table_size( &channel->dict.dict );
    HRESULT hr;

    if ((hr = send_byte( channel->u.tcp.socket, FRAME_RECORD_TYPE_SIZED_ENVELOPE )) != S_OK) return hr;
    if ((hr = send_size( channel->u.tcp.socket, size_length(table_size) + table_size + len )) != S_OK) return hr;
    if ((hr = send_size( channel->u.tcp.socket, table_size )) != S_OK) return hr;
    if ((hr = send_string_table( channel->u.tcp.socket, &channel->dict.dict )) != S_OK) return hr;
    return send_bytes( channel->u.tcp.socket, data, len );
}

static HRESULT open_http_request( struct channel *channel, HINTERNET *req )
{
    static const WCHAR postW[] = {'P','O','S','T',0};
    if ((*req = WinHttpOpenRequest( channel->u.http.connect, postW, channel->u.http.path,
                                    NULL, NULL, NULL, channel->u.http.flags ))) return S_OK;
    return HRESULT_FROM_WIN32( GetLastError() );
}

static HRESULT send_message( struct channel *channel, WS_MESSAGE *msg )
{
    WS_XML_WRITER *writer;
    WS_BYTES buf;
    HRESULT hr;

    channel->msg = msg;
    if ((hr = connect_channel( channel )) != S_OK) return hr;

    WsGetMessageProperty( channel->msg, WS_MESSAGE_PROPERTY_BODY_WRITER, &writer, sizeof(writer), NULL );
    WsGetWriterProperty( writer, WS_XML_WRITER_PROPERTY_BYTES, &buf, sizeof(buf), NULL );

    switch (channel->binding)
    {
    case WS_HTTP_CHANNEL_BINDING:
        if (channel->u.http.request)
        {
            WinHttpCloseHandle( channel->u.http.request );
            channel->u.http.request = NULL;
        }
        if ((hr = open_http_request( channel, &channel->u.http.request )) != S_OK) return hr;
        if ((hr = message_insert_http_headers( msg, channel->u.http.request )) != S_OK) return hr;
        return send_message_http( channel->u.http.request, buf.bytes, buf.length );

    case WS_TCP_CHANNEL_BINDING:
        if (channel->encoding == WS_ENCODING_XML_BINARY_SESSION_1)
        {
            switch (channel->session_state)
            {
            case SESSION_STATE_UNINITIALIZED:
                if ((hr = send_preamble( channel )) != S_OK) return hr;
                if ((hr = receive_preamble_ack( channel )) != S_OK) return hr;
                /* fall through */

            case SESSION_STATE_SETUP_COMPLETE:
                return send_sized_envelope( channel, buf.bytes, buf.length );

            default:
                ERR( "unhandled session state %u\n", channel->session_state );
                return WS_E_OTHER;
            }
        }
        return send_bytes( channel->u.tcp.socket, buf.bytes, buf.length );

    case WS_UDP_CHANNEL_BINDING:
        return send_bytes( channel->u.udp.socket, buf.bytes, buf.length );

    default:
        ERR( "unhandled binding %u\n", channel->binding );
        return E_NOTIMPL;
    }
}

HRESULT channel_send_message( WS_CHANNEL *handle, WS_MESSAGE *msg )
{
    struct channel *channel = (struct channel *)handle;
    HRESULT hr;

    EnterCriticalSection( &channel->cs );

    if (channel->magic != CHANNEL_MAGIC)
    {
        LeaveCriticalSection( &channel->cs );
        return E_INVALIDARG;
    }

    hr = send_message( channel, msg );

    LeaveCriticalSection( &channel->cs );
    return hr;
}

static HRESULT init_writer( struct channel *channel )
{
    WS_XML_WRITER_BUFFER_OUTPUT buf = {{WS_XML_WRITER_OUTPUT_TYPE_BUFFER}};
    WS_XML_WRITER_TEXT_ENCODING text = {{WS_XML_WRITER_ENCODING_TYPE_TEXT}};
    WS_XML_WRITER_BINARY_ENCODING bin = {{WS_XML_WRITER_ENCODING_TYPE_BINARY}};
    WS_XML_WRITER_ENCODING *encoding;
    HRESULT hr;

    if (!channel->writer && (hr = WsCreateWriter( NULL, 0, &channel->writer, NULL )) != S_OK) return hr;

    switch (channel->encoding)
    {
    case WS_ENCODING_XML_UTF8:
        text.charSet = WS_CHARSET_UTF8;
        encoding = &text.encoding;
        break;

    case WS_ENCODING_XML_BINARY_SESSION_1:
        bin.staticDictionary = (WS_XML_DICTIONARY *)&dict_builtin_static.dict;
        /* fall through */

    case WS_ENCODING_XML_BINARY_1:
        encoding = &bin.encoding;
        break;

    default:
        FIXME( "unhandled encoding %u\n", channel->encoding );
        return WS_E_NOT_SUPPORTED;
    }

    return WsSetOutput( channel->writer, encoding, &buf.output, NULL, 0, NULL );
}

/**************************************************************************
 *          WsSendMessage		[webservices.@]
 */
HRESULT WINAPI WsSendMessage( WS_CHANNEL *handle, WS_MESSAGE *msg, const WS_MESSAGE_DESCRIPTION *desc,
                              WS_WRITE_OPTION option, const void *body, ULONG size, const WS_ASYNC_CONTEXT *ctx,
                              WS_ERROR *error )
{
    struct channel *channel = (struct channel *)handle;
    HRESULT hr;

    TRACE( "%p %p %p %08x %p %u %p %p\n", handle, msg, desc, option, body, size, ctx, error );
    if (error) FIXME( "ignoring error parameter\n" );
    if (ctx) FIXME( "ignoring ctx parameter\n" );

    if (!channel || !msg || !desc) return E_INVALIDARG;

    EnterCriticalSection( &channel->cs );

    if (channel->magic != CHANNEL_MAGIC)
    {
        LeaveCriticalSection( &channel->cs );
        return E_INVALIDARG;
    }

    if ((hr = WsInitializeMessage( msg, WS_REQUEST_MESSAGE, NULL, NULL )) != S_OK) goto done;
    if ((hr = WsAddressMessage( msg, &channel->addr, NULL )) != S_OK) goto done;
    if ((hr = message_set_action( msg, desc->action )) != S_OK) goto done;

    if ((hr = init_writer( channel )) != S_OK) goto done;
    if ((hr = write_message( msg, channel->writer, desc->bodyElementDescription, option, body, size )) != S_OK)
        goto done;
    hr = send_message( channel, msg );

done:
    LeaveCriticalSection( &channel->cs );
    return hr;
}

static HRESULT resize_read_buffer( struct channel *channel, ULONG size )
{
    if (!channel->read_buf)
    {
        if (!(channel->read_buf = heap_alloc( size ))) return E_OUTOFMEMORY;
        channel->read_buflen = size;
        return S_OK;
    }
    if (channel->read_buflen < size)
    {
        char *tmp;
        ULONG new_size = max( size, channel->read_buflen * 2 );
        if (!(tmp = heap_realloc( channel->read_buf, new_size ))) return E_OUTOFMEMORY;
        channel->read_buf = tmp;
        channel->read_buflen = new_size;
    }
    return S_OK;
}

static HRESULT init_reader( struct channel *channel )
{
    WS_XML_READER_BUFFER_INPUT buf = {{WS_XML_READER_INPUT_TYPE_BUFFER}};
    WS_XML_READER_TEXT_ENCODING text = {{WS_XML_READER_ENCODING_TYPE_TEXT}};
    WS_XML_READER_BINARY_ENCODING bin = {{WS_XML_READER_ENCODING_TYPE_BINARY}};
    WS_XML_READER_ENCODING *encoding;
    HRESULT hr;

    if (!channel->reader && (hr = WsCreateReader( NULL, 0, &channel->reader, NULL )) != S_OK) return hr;

    switch (channel->encoding)
    {
    case WS_ENCODING_XML_UTF8:
        text.charSet = WS_CHARSET_UTF8;
        encoding = &text.encoding;
        break;

    case WS_ENCODING_XML_BINARY_SESSION_1:
        bin.staticDictionary  = (WS_XML_DICTIONARY *)&dict_builtin_static.dict;
        bin.dynamicDictionary = &channel->dict.dict;
        /* fall through */

    case WS_ENCODING_XML_BINARY_1:
        encoding = &bin.encoding;
        break;

    default:
        FIXME( "unhandled encoding %u\n", channel->encoding );
        return WS_E_NOT_SUPPORTED;
    }

    buf.encodedData     = channel->read_buf;
    buf.encodedDataSize = channel->read_size;
    return WsSetInput( channel->reader, encoding, &buf.input, NULL, 0, NULL );
}

#define INITIAL_READ_BUFFER_SIZE 4096
static HRESULT receive_message_http( struct channel *channel )
{
    DWORD len, bytes_read, offset = 0, size = INITIAL_READ_BUFFER_SIZE;
    ULONG max_len;
    HRESULT hr;

    prop_get( channel->prop, channel->prop_count, WS_CHANNEL_PROPERTY_MAX_BUFFERED_MESSAGE_SIZE,
              &max_len, sizeof(max_len) );

    if ((hr = resize_read_buffer( channel, size )) != S_OK) return hr;
    channel->read_size = 0;
    for (;;)
    {
        if (!WinHttpQueryDataAvailable( channel->u.http.request, &len ))
        {
            return HRESULT_FROM_WIN32( GetLastError() );
        }
        if (!len) break;
        if (channel->read_size + len > max_len) return WS_E_QUOTA_EXCEEDED;
        if ((hr = resize_read_buffer( channel, channel->read_size + len )) != S_OK) return hr;

        if (!WinHttpReadData( channel->u.http.request, channel->read_buf + offset, len, &bytes_read ))
        {
            return HRESULT_FROM_WIN32( GetLastError() );
        }
        if (!bytes_read) break;
        channel->read_size += bytes_read;
        offset += bytes_read;
    }

    return init_reader( channel );
}

static HRESULT receive_message_unsized( struct channel *channel, SOCKET socket )
{
    int bytes_read;
    ULONG max_len;
    HRESULT hr;

    prop_get( channel->prop, channel->prop_count, WS_CHANNEL_PROPERTY_MAX_BUFFERED_MESSAGE_SIZE,
              &max_len, sizeof(max_len) );

    if ((hr = resize_read_buffer( channel, max_len )) != S_OK) return hr;

    channel->read_size = 0;
    if ((bytes_read = recv( socket, channel->read_buf, max_len, 0 )) < 0)
    {
        return HRESULT_FROM_WIN32( WSAGetLastError() );
    }
    channel->read_size = bytes_read;
    return S_OK;
}

static HRESULT receive_message_sized( struct channel *channel, unsigned int size )
{
    unsigned int offset = 0, to_read = size;
    int bytes_read;
    HRESULT hr;

    if ((hr = resize_read_buffer( channel, size )) != S_OK) return hr;

    channel->read_size = 0;
    while (channel->read_size < size)
    {
        if ((bytes_read = recv( channel->u.tcp.socket, channel->read_buf + offset, to_read, 0 )) < 0)
        {
            return HRESULT_FROM_WIN32( WSAGetLastError() );
        }
        if (!bytes_read) break;
        channel->read_size += bytes_read;
        to_read -= bytes_read;
        offset += bytes_read;
    }
    if (channel->read_size != size) return WS_E_INVALID_FORMAT;
    return S_OK;
}

static HRESULT receive_size( struct channel *channel, unsigned int *size )
{
    unsigned char byte;
    HRESULT hr;

    if ((hr = receive_bytes( channel, &byte, 1 )) != S_OK) return hr;
    *size = byte & 0x7f;
    if (!(byte & 0x80)) return S_OK;

    if ((hr = receive_bytes( channel, &byte, 1 )) != S_OK) return hr;
    *size += (byte & 0x7f) << 7;
    if (!(byte & 0x80)) return S_OK;

    if ((hr = receive_bytes( channel, &byte, 1 )) != S_OK) return hr;
    *size += (byte & 0x7f) << 14;
    if (!(byte & 0x80)) return S_OK;

    if ((hr = receive_bytes( channel, &byte, 1 )) != S_OK) return hr;
    *size += (byte & 0x7f) << 21;
    if (!(byte & 0x80)) return S_OK;

    if ((hr = receive_bytes( channel, &byte, 1 )) != S_OK) return hr;
    if (byte & ~0x0f) return WS_E_INVALID_FORMAT;
    *size += byte << 28;
    return S_OK;
}

static WS_ENCODING map_known_encoding( enum known_encoding encoding )
{
    switch (encoding)
    {
    case KNOWN_ENCODING_SOAP11_UTF8:
    case KNOWN_ENCODING_SOAP12_UTF8:           return WS_ENCODING_XML_UTF8;
    case KNOWN_ENCODING_SOAP11_UTF16:
    case KNOWN_ENCODING_SOAP12_UTF16:          return WS_ENCODING_XML_UTF16BE;
    case KNOWN_ENCODING_SOAP11_UTF16LE:
    case KNOWN_ENCODING_SOAP12_UTF16LE:        return WS_ENCODING_XML_UTF16LE;
    case KNOWN_ENCODING_SOAP12_BINARY:         return WS_ENCODING_XML_BINARY_1;
    case KNOWN_ENCODING_SOAP12_BINARY_SESSION: return WS_ENCODING_XML_BINARY_SESSION_1;
    default:
        WARN( "unhandled encoding %u, assuming UTF8\n", encoding );
        return WS_ENCODING_XML_UTF8;
    }
}

static HRESULT receive_preamble( struct channel *channel )
{
    unsigned char type;
    HRESULT hr;

    for (;;)
    {
        if ((hr = receive_bytes( channel, &type, 1 )) != S_OK) return hr;
        if (type == FRAME_RECORD_TYPE_PREAMBLE_END) break;
        switch (type)
        {
        case FRAME_RECORD_TYPE_VERSION:
        {
            unsigned char major, minor;
            if ((hr = receive_bytes( channel, &major, 1 )) != S_OK) return hr;
            if ((hr = receive_bytes( channel, &minor, 1 )) != S_OK) return hr;
            TRACE( "major %u minor %u\n", major, major );
            break;
        }
        case FRAME_RECORD_TYPE_MODE:
        {
            unsigned char mode;
            if ((hr = receive_bytes( channel, &mode, 1 )) != S_OK) return hr;
            TRACE( "mode %u\n", mode );
            break;
        }
        case FRAME_RECORD_TYPE_VIA:
        {
            unsigned int size;
            unsigned char *url;

            if ((hr = receive_size( channel, &size )) != S_OK) return hr;
            if (!(url = heap_alloc( size ))) return E_OUTOFMEMORY;
            if ((hr = receive_bytes( channel, url, size )) != S_OK)
            {
                heap_free( url );
                return hr;
            }
            TRACE( "transport URL %s\n", debugstr_an((char *)url, size) );
            heap_free( url ); /* FIXME: verify */
            break;
        }
        case FRAME_RECORD_TYPE_KNOWN_ENCODING:
        {
            unsigned char encoding;
            if ((hr = receive_bytes( channel, &encoding, 1 )) != S_OK) return hr;
            TRACE( "encoding %u\n", encoding );
            channel->encoding = map_known_encoding( encoding );
            break;
        }
        default:
            WARN( "unhandled record type %u\n", type );
            return WS_E_INVALID_FORMAT;
        }
    }

    return S_OK;
}

static HRESULT receive_sized_envelope( struct channel *channel )
{
    unsigned char type;
    unsigned int size;
    HRESULT hr;

    if ((hr = receive_bytes( channel, &type, 1 )) != S_OK) return hr;
    if (type != FRAME_RECORD_TYPE_SIZED_ENVELOPE) return WS_E_INVALID_FORMAT;
    if ((hr = receive_size( channel, &size )) != S_OK) return hr;
    if ((hr = receive_message_sized( channel, size )) != S_OK) return hr;
    return S_OK;
}

static HRESULT read_size( const BYTE **ptr, ULONG len, ULONG *size )
{
    const BYTE *buf = *ptr;

    if (len < 1) return WS_E_INVALID_FORMAT;
    *size = buf[0] & 0x7f;
    if (!(buf[0] & 0x80))
    {
        *ptr += 1;
        return S_OK;
    }
    if (len < 2) return WS_E_INVALID_FORMAT;
    *size += (buf[1] & 0x7f) << 7;
    if (!(buf[1] & 0x80))
    {
        *ptr += 2;
        return S_OK;
    }
    if (len < 3) return WS_E_INVALID_FORMAT;
    *size += (buf[2] & 0x7f) << 14;
    if (!(buf[2] & 0x80))
    {
        *ptr += 3;
        return S_OK;
    }
    if (len < 4) return WS_E_INVALID_FORMAT;
    *size += (buf[3] & 0x7f) << 21;
    if (!(buf[3] & 0x80))
    {
        *ptr += 4;
        return S_OK;
    }
    if (len < 5 || (buf[4] & ~0x07)) return WS_E_INVALID_FORMAT;
    *size += buf[4] << 28;
    *ptr += 5;
    return S_OK;
}

static HRESULT build_dict( const BYTE *buf, ULONG buflen, struct dictionary *dict, ULONG *used )
{
    ULONG size, strings_size, strings_offset;
    const BYTE *ptr = buf;
    BYTE *bytes;
    int index;
    HRESULT hr;

    if ((hr = read_size( &ptr, buflen, &strings_size )) != S_OK) return hr;
    strings_offset = ptr - buf;
    if (buflen < strings_offset + strings_size) return WS_E_INVALID_FORMAT;
    *used = strings_offset + strings_size;
    if (!strings_size) return S_OK;

    UuidCreate( &dict->dict.guid );
    dict->dict.isConst = FALSE;

    buflen -= strings_offset;
    ptr = buf + strings_offset;
    while (ptr < buf + strings_size)
    {
        if ((hr = read_size( &ptr, buflen, &size )) != S_OK)
        {
            clear_dict( dict );
            return hr;
        }
        if (size > buflen)
        {
            clear_dict( dict );
            return WS_E_INVALID_FORMAT;
        }
        buflen -= size;
        if (!(bytes = heap_alloc( size )))
        {
            hr = E_OUTOFMEMORY;
            goto error;
        }
        memcpy( bytes, ptr, size );
        if ((index = find_string( dict, bytes, size, NULL )) == -1) /* duplicate */
        {
            heap_free( bytes );
            ptr += size;
            continue;
        }
        if (!insert_string( dict, bytes, size, index, NULL ))
        {
            clear_dict( dict );
            return E_OUTOFMEMORY;
        }
        ptr += size;
    }
    return S_OK;

error:
    clear_dict( dict );
    return hr;
}

static HRESULT send_preamble_ack( struct channel *channel )
{
    HRESULT hr;
    if ((hr = send_byte( channel->u.tcp.socket, FRAME_RECORD_TYPE_PREAMBLE_ACK )) != S_OK) return hr;
    channel->session_state = SESSION_STATE_SETUP_COMPLETE;
    return S_OK;
}

static HRESULT receive_message_session_setup( struct channel *channel )
{
    HRESULT hr;

    if ((hr = receive_preamble( channel )) != S_OK) return hr;
    if ((hr = send_preamble_ack( channel )) != S_OK) return hr;
    if ((hr = receive_sized_envelope( channel )) != S_OK) return hr;
    if (channel->encoding == WS_ENCODING_XML_BINARY_SESSION_1)
    {
        ULONG size;
        if ((hr = build_dict( (const BYTE *)channel->read_buf, channel->read_size, &channel->dict, &size )) != S_OK)
            return hr;
        channel->read_size -= size;
        memmove( channel->read_buf, channel->read_buf + size, channel->read_size );
    }

    return init_reader( channel );
}

static HRESULT receive_message_session( struct channel *channel )
{
    HRESULT hr;
    if ((hr = receive_sized_envelope( channel )) != S_OK) return hr;
    return init_reader( channel );
}

static HRESULT receive_message_sock( struct channel *channel, SOCKET socket )
{
    HRESULT hr;
    if ((hr = receive_message_unsized( channel, socket )) != S_OK) return hr;
    return init_reader( channel );
}

static HRESULT receive_message( struct channel *channel )
{
    HRESULT hr;
    if ((hr = connect_channel( channel )) != S_OK) return hr;

    switch (channel->binding)
    {
    case WS_HTTP_CHANNEL_BINDING:
        return receive_message_http( channel );

    case WS_TCP_CHANNEL_BINDING:
        if (channel->encoding == WS_ENCODING_XML_BINARY_SESSION_1)
        {
            switch (channel->session_state)
            {
            case SESSION_STATE_UNINITIALIZED:
                return receive_message_session_setup( channel );

            case SESSION_STATE_SETUP_COMPLETE:
                return receive_message_session( channel );

            default:
                ERR( "unhandled session state %u\n", channel->session_state );
                return WS_E_OTHER;
            }
        }
        return receive_message_sock( channel, channel->u.tcp.socket );

    case WS_UDP_CHANNEL_BINDING:
        return receive_message_sock( channel, channel->u.udp.socket );

    default:
        ERR( "unhandled binding %u\n", channel->binding );
        return E_NOTIMPL;
    }
}

HRESULT channel_receive_message( WS_CHANNEL *handle )
{
    struct channel *channel = (struct channel *)handle;
    HRESULT hr;

    EnterCriticalSection( &channel->cs );

    if (channel->magic != CHANNEL_MAGIC)
    {
        LeaveCriticalSection( &channel->cs );
        return E_INVALIDARG;
    }

    hr = receive_message( channel );

    LeaveCriticalSection( &channel->cs );
    return hr;
}

HRESULT channel_get_reader( WS_CHANNEL *handle, WS_XML_READER **reader )
{
    struct channel *channel = (struct channel *)handle;

    EnterCriticalSection( &channel->cs );

    if (channel->magic != CHANNEL_MAGIC)
    {
        LeaveCriticalSection( &channel->cs );
        return E_INVALIDARG;
    }

    *reader = channel->reader;

    LeaveCriticalSection( &channel->cs );
    return S_OK;
}

static HRESULT read_message( WS_MESSAGE *handle, WS_XML_READER *reader, const WS_ELEMENT_DESCRIPTION *desc,
                             WS_READ_OPTION option, WS_HEAP *heap, void *body, ULONG size )
{
    HRESULT hr;
    if ((hr = WsReadEnvelopeStart( handle, reader, NULL, NULL, NULL )) != S_OK) return hr;
    if ((hr = WsReadBody( handle, desc, option, heap, body, size, NULL )) != S_OK) return hr;
    return WsReadEnvelopeEnd( handle, NULL );
}

/**************************************************************************
 *          WsReceiveMessage		[webservices.@]
 */
HRESULT WINAPI WsReceiveMessage( WS_CHANNEL *handle, WS_MESSAGE *msg, const WS_MESSAGE_DESCRIPTION **desc,
                                 ULONG count, WS_RECEIVE_OPTION option, WS_READ_OPTION read_option, WS_HEAP *heap,
                                 void *value, ULONG size, ULONG *index, const WS_ASYNC_CONTEXT *ctx, WS_ERROR *error )
{
    struct channel *channel = (struct channel *)handle;
    HRESULT hr;

    TRACE( "%p %p %p %u %08x %08x %p %p %u %p %p %p\n", handle, msg, desc, count, option, read_option, heap,
           value, size, index, ctx, error );
    if (error) FIXME( "ignoring error parameter\n" );
    if (ctx) FIXME( "ignoring ctx parameter\n" );
    if (index)
    {
        FIXME( "index parameter not supported\n" );
        return E_NOTIMPL;
    }
    if (count != 1)
    {
        FIXME( "no support for multiple descriptions\n" );
        return E_NOTIMPL;
    }
    if (option != WS_RECEIVE_REQUIRED_MESSAGE)
    {
        FIXME( "receive option %08x not supported\n", option );
        return E_NOTIMPL;
    }

    if (!channel || !msg || !desc || !count) return E_INVALIDARG;

    EnterCriticalSection( &channel->cs );

    if (channel->magic != CHANNEL_MAGIC)
    {
        LeaveCriticalSection( &channel->cs );
        return E_INVALIDARG;
    }

    if ((hr = receive_message( channel )) != S_OK) goto done;
    hr = read_message( msg, channel->reader, desc[0]->bodyElementDescription, read_option, heap, value, size );

done:
    LeaveCriticalSection( &channel->cs );
    return hr;
}

/**************************************************************************
 *          WsReadMessageStart		[webservices.@]
 */
HRESULT WINAPI WsReadMessageStart( WS_CHANNEL *handle, WS_MESSAGE *msg, const WS_ASYNC_CONTEXT *ctx,
                                   WS_ERROR *error )
{
    struct channel *channel = (struct channel *)handle;
    HRESULT hr;

    TRACE( "%p %p %p %p\n", handle, msg, ctx, error );
    if (error) FIXME( "ignoring error parameter\n" );
    if (ctx) FIXME( "ignoring ctx parameter\n" );

    if (!channel || !msg) return E_INVALIDARG;

    EnterCriticalSection( &channel->cs );

    if (channel->magic != CHANNEL_MAGIC)
    {
        LeaveCriticalSection( &channel->cs );
        return E_INVALIDARG;
    }

    if ((hr = receive_message( channel )) == S_OK)
    {
        hr = WsReadEnvelopeStart( msg, channel->reader, NULL, NULL, NULL );
    }

    LeaveCriticalSection( &channel->cs );
    return hr;
}

/**************************************************************************
 *          WsReadMessageEnd		[webservices.@]
 */
HRESULT WINAPI WsReadMessageEnd( WS_CHANNEL *handle, WS_MESSAGE *msg, const WS_ASYNC_CONTEXT *ctx,
                                 WS_ERROR *error )
{
    struct channel *channel = (struct channel *)handle;
    HRESULT hr;

    TRACE( "%p %p %p %p\n", handle, msg, ctx, error );
    if (error) FIXME( "ignoring error parameter\n" );
    if (ctx) FIXME( "ignoring ctx parameter\n" );

    if (!channel || !msg) return E_INVALIDARG;

    EnterCriticalSection( &channel->cs );

    if (channel->magic != CHANNEL_MAGIC)
    {
        LeaveCriticalSection( &channel->cs );
        return E_INVALIDARG;
    }

    hr = WsReadEnvelopeEnd( msg, NULL );

    LeaveCriticalSection( &channel->cs );
    return hr;
}

/**************************************************************************
 *          WsWriteMessageStart		[webservices.@]
 */
HRESULT WINAPI WsWriteMessageStart( WS_CHANNEL *handle, WS_MESSAGE *msg, const WS_ASYNC_CONTEXT *ctx,
                                    WS_ERROR *error )
{
    struct channel *channel = (struct channel *)handle;
    HRESULT hr;

    TRACE( "%p %p %p %p\n", handle, msg, ctx, error );
    if (error) FIXME( "ignoring error parameter\n" );
    if (ctx) FIXME( "ignoring ctx parameter\n" );

    if (!channel || !msg) return E_INVALIDARG;

    EnterCriticalSection( &channel->cs );

    if (channel->magic != CHANNEL_MAGIC)
    {
        LeaveCriticalSection( &channel->cs );
        return E_INVALIDARG;
    }

    if ((hr = init_writer( channel )) != S_OK) goto done;
    if ((hr = WsAddressMessage( msg, &channel->addr, NULL )) != S_OK) goto done;
    hr = WsWriteEnvelopeStart( msg, channel->writer, NULL, NULL, NULL );

done:
    LeaveCriticalSection( &channel->cs );
    return hr;
}

/**************************************************************************
 *          WsWriteMessageEnd		[webservices.@]
 */
HRESULT WINAPI WsWriteMessageEnd( WS_CHANNEL *handle, WS_MESSAGE *msg, const WS_ASYNC_CONTEXT *ctx,
                                  WS_ERROR *error )
{
    struct channel *channel = (struct channel *)handle;
    HRESULT hr;

    TRACE( "%p %p %p %p\n", handle, msg, ctx, error );
    if (error) FIXME( "ignoring error parameter\n" );
    if (ctx) FIXME( "ignoring ctx parameter\n" );

    if (!channel || !msg) return E_INVALIDARG;

    EnterCriticalSection( &channel->cs );

    if (channel->magic != CHANNEL_MAGIC)
    {
        LeaveCriticalSection( &channel->cs );
        return E_INVALIDARG;
    }

    if ((hr = WsWriteEnvelopeEnd( msg, NULL )) == S_OK) hr = send_message( channel, msg );

    LeaveCriticalSection( &channel->cs );
    return hr;
}

static HRESULT sock_accept( SOCKET socket, HANDLE wait, HANDLE cancel, SOCKET *ret )
{
    HANDLE handles[] = { wait, cancel };
    ULONG nonblocking = 0;
    HRESULT hr = S_OK;

    if (WSAEventSelect( socket, handles[0], FD_ACCEPT )) return HRESULT_FROM_WIN32( WSAGetLastError() );

    switch (WSAWaitForMultipleEvents( 2, handles, FALSE, WSA_INFINITE, FALSE ))
    {
    case 0:
        if ((*ret = accept( socket, NULL, NULL )) != -1)
        {
            WSAEventSelect( *ret, NULL, 0 );
            ioctlsocket( *ret, FIONBIO, &nonblocking );
            break;
        }
        hr = HRESULT_FROM_WIN32( WSAGetLastError() );
        break;

    case 1:
        hr = WS_E_OPERATION_ABORTED;
        break;

    default:
        hr = HRESULT_FROM_WIN32( WSAGetLastError() );
        break;
    }

    return hr;
}

HRESULT channel_accept_tcp( SOCKET socket, HANDLE wait, HANDLE cancel, WS_CHANNEL *handle )
{
    struct channel *channel = (struct channel *)handle;
    HRESULT hr;

    EnterCriticalSection( &channel->cs );

    if (channel->magic != CHANNEL_MAGIC)
    {
        LeaveCriticalSection( &channel->cs );
        return E_INVALIDARG;
    }

    hr = sock_accept( socket, wait, cancel, &channel->u.tcp.socket );

    LeaveCriticalSection( &channel->cs );
    return hr;
}

static HRESULT sock_wait( SOCKET socket, HANDLE wait, HANDLE cancel )
{
    HANDLE handles[] = { wait, cancel };
    ULONG nonblocking = 0;
    HRESULT hr;

    if (WSAEventSelect( socket, handles[0], FD_READ )) return HRESULT_FROM_WIN32( WSAGetLastError() );

    switch (WSAWaitForMultipleEvents( 2, handles, FALSE, WSA_INFINITE, FALSE ))
    {
    case 0:
        hr = S_OK;
        break;

    case 1:
        hr = WS_E_OPERATION_ABORTED;
        break;

    default:
        hr = HRESULT_FROM_WIN32( WSAGetLastError() );
        break;
    }

    WSAEventSelect( socket, NULL, 0 );
    ioctlsocket( socket, FIONBIO, &nonblocking );
    return hr;
}

HRESULT channel_accept_udp( SOCKET socket, HANDLE wait, HANDLE cancel, WS_CHANNEL *handle )
{
    struct channel *channel = (struct channel *)handle;
    HRESULT hr;

    EnterCriticalSection( &channel->cs );

    if (channel->magic != CHANNEL_MAGIC)
    {
        LeaveCriticalSection( &channel->cs );
        return E_INVALIDARG;
    }

    if ((hr = sock_wait( socket, wait, cancel )) == S_OK) channel->u.udp.socket = socket;

    LeaveCriticalSection( &channel->cs );
    return hr;
}
