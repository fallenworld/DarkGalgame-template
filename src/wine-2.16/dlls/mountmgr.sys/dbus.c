/*
 * DBus devices support
 *
 * Copyright 2006, 2011 Alexandre Julliard
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

#include "config.h"
#include "wine/port.h"

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/time.h>
#ifdef SONAME_LIBDBUS_1
# include <dbus/dbus.h>
#endif
#ifdef SONAME_LIBHAL
# include <hal/libhal.h>
#endif

#include "mountmgr.h"
#include "winnls.h"
#include "excpt.h"

#include "wine/library.h"
#include "wine/exception.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(mountmgr);

#ifdef SONAME_LIBDBUS_1

#define DBUS_FUNCS \
    DO_FUNC(dbus_bus_add_match); \
    DO_FUNC(dbus_bus_get); \
    DO_FUNC(dbus_bus_remove_match); \
    DO_FUNC(dbus_connection_add_filter); \
    DO_FUNC(dbus_connection_read_write_dispatch); \
    DO_FUNC(dbus_connection_remove_filter); \
    DO_FUNC(dbus_connection_send_with_reply_and_block); \
    DO_FUNC(dbus_error_free); \
    DO_FUNC(dbus_error_init); \
    DO_FUNC(dbus_error_is_set); \
    DO_FUNC(dbus_free_string_array); \
    DO_FUNC(dbus_message_get_args); \
    DO_FUNC(dbus_message_get_interface); \
    DO_FUNC(dbus_message_get_member); \
    DO_FUNC(dbus_message_get_path); \
    DO_FUNC(dbus_message_get_type); \
    DO_FUNC(dbus_message_is_signal); \
    DO_FUNC(dbus_message_iter_append_basic); \
    DO_FUNC(dbus_message_iter_get_arg_type); \
    DO_FUNC(dbus_message_iter_get_basic); \
    DO_FUNC(dbus_message_iter_get_fixed_array); \
    DO_FUNC(dbus_message_iter_init); \
    DO_FUNC(dbus_message_iter_init_append); \
    DO_FUNC(dbus_message_iter_next); \
    DO_FUNC(dbus_message_iter_recurse); \
    DO_FUNC(dbus_message_new_method_call); \
    DO_FUNC(dbus_message_unref)

#define DO_FUNC(f) static typeof(f) * p_##f
DBUS_FUNCS;
#undef DO_FUNC

static int udisks_timeout = -1;
static DBusConnection *connection;

#ifdef SONAME_LIBHAL

#define HAL_FUNCS \
    DO_FUNC(libhal_ctx_free); \
    DO_FUNC(libhal_ctx_init); \
    DO_FUNC(libhal_ctx_new); \
    DO_FUNC(libhal_ctx_set_dbus_connection); \
    DO_FUNC(libhal_ctx_set_device_added); \
    DO_FUNC(libhal_ctx_set_device_property_modified); \
    DO_FUNC(libhal_ctx_set_device_removed); \
    DO_FUNC(libhal_ctx_shutdown); \
    DO_FUNC(libhal_device_get_property_bool); \
    DO_FUNC(libhal_device_get_property_string); \
    DO_FUNC(libhal_device_add_property_watch); \
    DO_FUNC(libhal_device_remove_property_watch); \
    DO_FUNC(libhal_free_string); \
    DO_FUNC(libhal_free_string_array); \
    DO_FUNC(libhal_get_all_devices)

#define DO_FUNC(f) static typeof(f) * p_##f
HAL_FUNCS;
#undef DO_FUNC

static BOOL load_hal_functions(void)
{
    void *hal_handle;
    char error[128];

    /* Load libhal with RTLD_GLOBAL so that the dbus symbols are available.
     * We can't load libdbus directly since libhal may have been built against a
     * different version but with the same soname. Binary compatibility is for wimps. */

    if (!(hal_handle = wine_dlopen(SONAME_LIBHAL, RTLD_NOW|RTLD_GLOBAL, error, sizeof(error))))
        goto failed;

#define DO_FUNC(f) if (!(p_##f = wine_dlsym( RTLD_DEFAULT, #f, error, sizeof(error) ))) goto failed
    DBUS_FUNCS;
#undef DO_FUNC

#define DO_FUNC(f) if (!(p_##f = wine_dlsym( hal_handle, #f, error, sizeof(error) ))) goto failed
    HAL_FUNCS;
#undef DO_FUNC

    udisks_timeout = 3000;  /* don't try for too long if we can fall back to HAL */
    return TRUE;

failed:
    WARN( "failed to load HAL support: %s\n", error );
    return FALSE;
}

#endif /* SONAME_LIBHAL */

static LONG WINAPI assert_fault(EXCEPTION_POINTERS *eptr)
{
    if (eptr->ExceptionRecord->ExceptionCode == EXCEPTION_WINE_ASSERTION)
        return EXCEPTION_EXECUTE_HANDLER;
    return EXCEPTION_CONTINUE_SEARCH;
}

static inline int starts_with( const char *str, const char *prefix )
{
    return !strncmp( str, prefix, strlen(prefix) );
}

static GUID *parse_uuid( GUID *guid, const char *str )
{
    /* standard uuid format */
    if (strlen(str) == 36)
    {
        UNICODE_STRING strW;
        WCHAR buffer[39];

        if (MultiByteToWideChar( CP_UNIXCP, 0, str, 36, buffer + 1, 36 ))
        {
            buffer[0] = '{';
            buffer[37] = '}';
            buffer[38] = 0;
            RtlInitUnicodeString( &strW, buffer );
            if (!RtlGUIDFromString( &strW, guid )) return guid;
        }
    }

    /* check for xxxx-xxxx format (FAT serial number) */
    if (strlen(str) == 9 && str[4] == '-')
    {
        memset( guid, 0, sizeof(*guid) );
        if (sscanf( str, "%hx-%hx", &guid->Data2, &guid->Data3 ) == 2) return guid;
    }
    return NULL;
}

static BOOL load_dbus_functions(void)
{
    void *handle;
    char error[128];

    if (!(handle = wine_dlopen(SONAME_LIBDBUS_1, RTLD_NOW, error, sizeof(error))))
        goto failed;

#define DO_FUNC(f) if (!(p_##f = wine_dlsym( handle, #f, error, sizeof(error) ))) goto failed
    DBUS_FUNCS;
#undef DO_FUNC
    return TRUE;

failed:
    WARN( "failed to load DBUS support: %s\n", error );
    return FALSE;
}

static const char *udisks_next_dict_entry( DBusMessageIter *iter, DBusMessageIter *variant )
{
    DBusMessageIter sub;
    const char *name;

    if (p_dbus_message_iter_get_arg_type( iter ) != DBUS_TYPE_DICT_ENTRY) return NULL;
    p_dbus_message_iter_recurse( iter, &sub );
    p_dbus_message_iter_next( iter );
    p_dbus_message_iter_get_basic( &sub, &name );
    p_dbus_message_iter_next( &sub );
    p_dbus_message_iter_recurse( &sub, variant );
    return name;
}

static enum device_type udisks_parse_media_compatibility( DBusMessageIter *iter )
{
    DBusMessageIter media;
    enum device_type drive_type = DEVICE_UNKNOWN;

    p_dbus_message_iter_recurse( iter, &media );
    while (p_dbus_message_iter_get_arg_type( &media ) == DBUS_TYPE_STRING)
    {
        const char *media_type;
        p_dbus_message_iter_get_basic( &media, &media_type );
        if (starts_with( media_type, "optical_dvd" ))
            drive_type = DEVICE_DVD;
        if (starts_with( media_type, "floppy" ))
            drive_type = DEVICE_FLOPPY;
        else if (starts_with( media_type, "optical_" ) && drive_type == DEVICE_UNKNOWN)
            drive_type = DEVICE_CDROM;
        p_dbus_message_iter_next( &media );
    }
    return drive_type;
}

/* UDisks callback for new device */
static void udisks_new_device( const char *udi )
{
    static const char *dev_name = "org.freedesktop.UDisks.Device";
    DBusMessage *request, *reply;
    DBusMessageIter iter, variant;
    DBusError error;
    const char *device = NULL;
    const char *mount_point = NULL;
    const char *type = NULL;
    GUID guid, *guid_ptr = NULL;
    int removable = FALSE;
    enum device_type drive_type = DEVICE_UNKNOWN;

    request = p_dbus_message_new_method_call( "org.freedesktop.UDisks", udi,
                                              "org.freedesktop.DBus.Properties", "GetAll" );
    if (!request) return;

    p_dbus_message_iter_init_append( request, &iter );
    p_dbus_message_iter_append_basic( &iter, DBUS_TYPE_STRING, &dev_name );

    p_dbus_error_init( &error );
    reply = p_dbus_connection_send_with_reply_and_block( connection, request, -1, &error );
    p_dbus_message_unref( request );
    if (!reply)
    {
        WARN( "failed: %s\n", error.message );
        p_dbus_error_free( &error );
        return;
    }
    p_dbus_error_free( &error );

    p_dbus_message_iter_init( reply, &iter );
    if (p_dbus_message_iter_get_arg_type( &iter ) == DBUS_TYPE_ARRAY)
    {
        const char *name;

        p_dbus_message_iter_recurse( &iter, &iter );
        while ((name = udisks_next_dict_entry( &iter, &variant )))
        {
            if (!strcmp( name, "DeviceFile" ))
                p_dbus_message_iter_get_basic( &variant, &device );
            else if (!strcmp( name, "DeviceIsRemovable" ))
                p_dbus_message_iter_get_basic( &variant, &removable );
            else if (!strcmp( name, "IdType" ))
                p_dbus_message_iter_get_basic( &variant, &type );
            else if (!strcmp( name, "DriveMediaCompatibility" ))
                drive_type = udisks_parse_media_compatibility( &variant );
            else if (!strcmp( name, "DeviceMountPaths" ))
            {
                DBusMessageIter paths;
                p_dbus_message_iter_recurse( &variant, &paths );
                if (p_dbus_message_iter_get_arg_type( &paths ) == DBUS_TYPE_STRING)
                    p_dbus_message_iter_get_basic( &paths, &mount_point );
            }
            else if (!strcmp( name, "IdUuid" ))
            {
                char *uuid_str;
                p_dbus_message_iter_get_basic( &variant, &uuid_str );
                guid_ptr = parse_uuid( &guid, uuid_str );
            }
        }
    }

    TRACE( "udi %s device %s mount point %s uuid %s type %s removable %u\n",
           debugstr_a(udi), debugstr_a(device), debugstr_a(mount_point),
           debugstr_guid(guid_ptr), debugstr_a(type), removable );

    if (type)
    {
        if (!strcmp( type, "iso9660" ))
        {
            removable = TRUE;
            drive_type = DEVICE_CDROM;
        }
        else if (!strcmp( type, "udf" ))
        {
            removable = TRUE;
            drive_type = DEVICE_DVD;
        }
    }

    if (device)
    {
        if (removable) add_dos_device( -1, udi, device, mount_point, drive_type, guid_ptr );
        else if (guid_ptr) add_volume( udi, device, mount_point, DEVICE_HARDDISK_VOL, guid_ptr );
    }

    p_dbus_message_unref( reply );
}

/* UDisks callback for removed device */
static void udisks_removed_device( const char *udi )
{
    TRACE( "removed %s\n", wine_dbgstr_a(udi) );

    if (!remove_dos_device( -1, udi )) remove_volume( udi );
}

/* UDisks callback for changed device */
static void udisks_changed_device( const char *udi )
{
    udisks_new_device( udi );
}

static BOOL udisks_enumerate_devices(void)
{
    DBusMessage *request, *reply;
    DBusError error;
    char **paths;
    int i, count;

    request = p_dbus_message_new_method_call( "org.freedesktop.UDisks", "/org/freedesktop/UDisks",
                                              "org.freedesktop.UDisks", "EnumerateDevices" );
    if (!request) return FALSE;

    p_dbus_error_init( &error );
    reply = p_dbus_connection_send_with_reply_and_block( connection, request, udisks_timeout, &error );
    p_dbus_message_unref( request );
    if (!reply)
    {
        WARN( "failed: %s\n", error.message );
        p_dbus_error_free( &error );
        return FALSE;
    }
    p_dbus_error_free( &error );

    if (p_dbus_message_get_args( reply, &error, DBUS_TYPE_ARRAY,
                                 DBUS_TYPE_OBJECT_PATH, &paths, &count, DBUS_TYPE_INVALID ))
    {
        for (i = 0; i < count; i++) udisks_new_device( paths[i] );
        p_dbus_free_string_array( paths );
    }
    else WARN( "unexpected args in EnumerateDevices reply\n" );

    p_dbus_message_unref( reply );
    return TRUE;
}

/* to make things easier, UDisks2 stores strings as array of bytes instead of strings... */
static const char *udisks2_string_from_array( DBusMessageIter *iter )
{
    DBusMessageIter string;
    const char *array;
    int size;

    p_dbus_message_iter_recurse( iter, &string );
    p_dbus_message_iter_get_fixed_array( &string, &array, &size );
    return array;
}

/* find the drive entry in the dictionary and get its parameters */
static void udisks2_get_drive_info( const char *drive_name, DBusMessageIter *dict,
                                    enum device_type *drive_type, int *removable )
{
    DBusMessageIter iter, drive, variant;
    const char *name;

    p_dbus_message_iter_recurse( dict, &iter );
    while ((name = udisks_next_dict_entry( &iter, &drive )))
    {
        if (strcmp( name, drive_name )) continue;
        while ((name = udisks_next_dict_entry( &drive, &iter )))
        {
            if (strcmp( name, "org.freedesktop.UDisks2.Drive" )) continue;
            while ((name = udisks_next_dict_entry( &iter, &variant )))
            {
                if (!strcmp( name, "Removable" ))
                    p_dbus_message_iter_get_basic( &variant, removable );
                else if (!strcmp( name, "MediaCompatibility" ))
                    *drive_type = udisks_parse_media_compatibility( &variant );
            }
        }
    }
}

static void udisks2_add_device( const char *udi, DBusMessageIter *dict, DBusMessageIter *block )
{
    DBusMessageIter iter, variant, paths, string;
    const char *device = NULL;
    const char *mount_point = NULL;
    const char *type = NULL;
    const char *drive = NULL;
    GUID guid, *guid_ptr = NULL;
    const char *iface, *name;
    int removable = FALSE;
    enum device_type drive_type = DEVICE_UNKNOWN;

    while ((iface = udisks_next_dict_entry( block, &iter )))
    {
        if (!strcmp( iface, "org.freedesktop.UDisks2.Filesystem" ))
        {
            while ((name = udisks_next_dict_entry( &iter, &variant )))
            {
                if (!strcmp( name, "MountPoints" ))
                {
                    p_dbus_message_iter_recurse( &variant, &paths );
                    if (p_dbus_message_iter_get_arg_type( &paths ) == DBUS_TYPE_ARRAY)
                    {
                        p_dbus_message_iter_recurse( &variant, &string );
                        mount_point = udisks2_string_from_array( &string );
                    }
                }
            }
        }
        if (!strcmp( iface, "org.freedesktop.UDisks2.Block" ))
        {
            while ((name = udisks_next_dict_entry( &iter, &variant )))
            {
                if (!strcmp( name, "Device" ))
                    device = udisks2_string_from_array( &variant );
                else if (!strcmp( name, "IdType" ))
                    p_dbus_message_iter_get_basic( &variant, &type );
                else if (!strcmp( name, "Drive" ))
                {
                    p_dbus_message_iter_get_basic( &variant, &drive );
                    udisks2_get_drive_info( drive, dict, &drive_type, &removable );
                }
                else if (!strcmp( name, "IdUUID" ))
                {
                    const char *uuid_str;
                    if (p_dbus_message_iter_get_arg_type( &variant ) == DBUS_TYPE_ARRAY)
                        uuid_str = udisks2_string_from_array( &variant );
                    else
                        p_dbus_message_iter_get_basic( &variant, &uuid_str );
                    guid_ptr = parse_uuid( &guid, uuid_str );
                }
            }
        }
    }

    TRACE( "udi %s device %s mount point %s uuid %s type %s removable %u\n",
           debugstr_a(udi), debugstr_a(device), debugstr_a(mount_point),
           debugstr_guid(guid_ptr), debugstr_a(type), removable );

    if (type)
    {
        if (!strcmp( type, "iso9660" ))
        {
            removable = TRUE;
            drive_type = DEVICE_CDROM;
        }
        else if (!strcmp( type, "udf" ))
        {
            removable = TRUE;
            drive_type = DEVICE_DVD;
        }
    }
    if (device)
    {
        if (removable) add_dos_device( -1, udi, device, mount_point, drive_type, guid_ptr );
        else if (guid_ptr) add_volume( udi, device, mount_point, DEVICE_HARDDISK_VOL, guid_ptr );
    }
}

/* UDisks2 is almost, but not quite, entirely unlike UDisks.
 * It would have been easy to make it backwards compatible, but where would be the fun in that?
 */
static BOOL udisks2_add_devices( const char *changed )
{
    DBusMessage *request, *reply;
    DBusMessageIter dict, iter, block;
    DBusError error;
    const char *udi;

    request = p_dbus_message_new_method_call( "org.freedesktop.UDisks2", "/org/freedesktop/UDisks2",
                                              "org.freedesktop.DBus.ObjectManager", "GetManagedObjects" );
    if (!request) return FALSE;

    p_dbus_error_init( &error );
    reply = p_dbus_connection_send_with_reply_and_block( connection, request, udisks_timeout, &error );
    p_dbus_message_unref( request );
    if (!reply)
    {
        WARN( "failed: %s\n", error.message );
        p_dbus_error_free( &error );
        return FALSE;
    }
    p_dbus_error_free( &error );

    p_dbus_message_iter_init( reply, &dict );
    if (p_dbus_message_iter_get_arg_type( &dict ) == DBUS_TYPE_ARRAY)
    {
        p_dbus_message_iter_recurse( &dict, &iter );
        while ((udi = udisks_next_dict_entry( &iter, &block )))
        {
            if (!starts_with( udi, "/org/freedesktop/UDisks2/block_devices/" )) continue;
            if (changed && strcmp( changed, udi )) continue;
            udisks2_add_device( udi, &dict, &block );
        }
    }
    else WARN( "unexpected args in GetManagedObjects reply\n" );

    p_dbus_message_unref( reply );
    return TRUE;
}

static DBusHandlerResult udisks_filter( DBusConnection *ctx, DBusMessage *msg, void *user_data )
{
    char *path;
    DBusError error;

    p_dbus_error_init( &error );

    /* udisks signals */
    if (p_dbus_message_is_signal( msg, "org.freedesktop.UDisks", "DeviceAdded" ) &&
        p_dbus_message_get_args( msg, &error, DBUS_TYPE_OBJECT_PATH, &path, DBUS_TYPE_INVALID ))
    {
        udisks_new_device( path );
    }
    else if (p_dbus_message_is_signal( msg, "org.freedesktop.UDisks", "DeviceRemoved" ) &&
             p_dbus_message_get_args( msg, &error, DBUS_TYPE_OBJECT_PATH, &path, DBUS_TYPE_INVALID ))
    {
        udisks_removed_device( path );
    }
    else if (p_dbus_message_is_signal( msg, "org.freedesktop.UDisks", "DeviceChanged" ) &&
             p_dbus_message_get_args( msg, &error, DBUS_TYPE_OBJECT_PATH, &path, DBUS_TYPE_INVALID ))
    {
        udisks_changed_device( path );
    }
    /* udisks2 signals */
    else if (p_dbus_message_is_signal( msg, "org.freedesktop.DBus.ObjectManager", "InterfacesAdded" ) &&
             p_dbus_message_get_args( msg, &error, DBUS_TYPE_OBJECT_PATH, &path, DBUS_TYPE_INVALID ))
    {
        TRACE( "added %s\n", wine_dbgstr_a(path) );
        udisks2_add_devices( path );
    }
    else if (p_dbus_message_is_signal( msg, "org.freedesktop.DBus.ObjectManager", "InterfacesRemoved" ) &&
             p_dbus_message_get_args( msg, &error, DBUS_TYPE_OBJECT_PATH, &path, DBUS_TYPE_INVALID ))
    {
        udisks_removed_device( path );
    }
    else if (p_dbus_message_is_signal( msg, "org.freedesktop.DBus.Properties", "PropertiesChanged" ))
    {
        const char *udi = p_dbus_message_get_path( msg );
        TRACE( "changed %s\n", wine_dbgstr_a(udi) );
        udisks2_add_devices( udi );
    }
    else TRACE( "ignoring message type=%d path=%s interface=%s method=%s\n",
                p_dbus_message_get_type( msg ), p_dbus_message_get_path( msg ),
                p_dbus_message_get_interface( msg ), p_dbus_message_get_member( msg ) );

    p_dbus_error_free( &error );
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

#ifdef SONAME_LIBHAL

/* HAL callback for new device */
static void hal_new_device( LibHalContext *ctx, const char *udi )
{
    DBusError error;
    char *parent = NULL;
    char *mount_point = NULL;
    char *device = NULL;
    char *type = NULL;
    char *uuid_str = NULL;
    GUID guid, *guid_ptr = NULL;
    enum device_type drive_type;

    p_dbus_error_init( &error );

    if (!(device = p_libhal_device_get_property_string( ctx, udi, "block.device", &error )))
        goto done;

    if (!(mount_point = p_libhal_device_get_property_string( ctx, udi, "volume.mount_point", &error )))
        goto done;

    if (!(parent = p_libhal_device_get_property_string( ctx, udi, "info.parent", &error )))
        goto done;

    if (!(uuid_str = p_libhal_device_get_property_string( ctx, udi, "volume.uuid", &error )))
        p_dbus_error_free( &error );  /* ignore error */
    else
        guid_ptr = parse_uuid( &guid, uuid_str );

    if (!(type = p_libhal_device_get_property_string( ctx, parent, "storage.drive_type", &error )))
        p_dbus_error_free( &error );  /* ignore error */

    if (type && !strcmp( type, "cdrom" )) drive_type = DEVICE_CDROM;
    else if (type && !strcmp( type, "floppy" )) drive_type = DEVICE_FLOPPY;
    else drive_type = DEVICE_UNKNOWN;

    if (p_libhal_device_get_property_bool( ctx, parent, "storage.removable", &error ))
    {
        add_dos_device( -1, udi, device, mount_point, drive_type, guid_ptr );
        /* add property watch for mount point */
        p_libhal_device_add_property_watch( ctx, udi, &error );
    }
    else if (guid_ptr) add_volume( udi, device, mount_point, DEVICE_HARDDISK_VOL, guid_ptr );

done:
    if (type) p_libhal_free_string( type );
    if (parent) p_libhal_free_string( parent );
    if (device) p_libhal_free_string( device );
    if (uuid_str) p_libhal_free_string( uuid_str );
    if (mount_point) p_libhal_free_string( mount_point );
    p_dbus_error_free( &error );
}

/* HAL callback for removed device */
static void hal_removed_device( LibHalContext *ctx, const char *udi )
{
    DBusError error;

    TRACE( "removed %s\n", wine_dbgstr_a(udi) );

    if (!remove_dos_device( -1, udi ))
    {
        p_dbus_error_init( &error );
        p_libhal_device_remove_property_watch( ctx, udi, &error );
        p_dbus_error_free( &error );
    }
    else remove_volume( udi );
}

/* HAL callback for property changes */
static void hal_property_modified (LibHalContext *ctx, const char *udi,
                                   const char *key, dbus_bool_t is_removed, dbus_bool_t is_added)
{
    TRACE( "udi %s key %s %s\n", wine_dbgstr_a(udi), wine_dbgstr_a(key),
           is_added ? "added" : is_removed ? "removed" : "modified" );

    if (!strcmp( key, "volume.mount_point" )) hal_new_device( ctx, udi );
}

static BOOL hal_enumerate_devices(void)
{
    LibHalContext *ctx;
    DBusError error;
    int i, num;
    char **list;

    if (!p_libhal_ctx_new) return FALSE;
    if (!(ctx = p_libhal_ctx_new())) return FALSE;

    p_libhal_ctx_set_dbus_connection( ctx, connection );
    p_libhal_ctx_set_device_added( ctx, hal_new_device );
    p_libhal_ctx_set_device_removed( ctx, hal_removed_device );
    p_libhal_ctx_set_device_property_modified( ctx, hal_property_modified );

    p_dbus_error_init( &error );
    if (!p_libhal_ctx_init( ctx, &error ))
    {
        WARN( "HAL context init failed: %s\n", error.message );
        p_dbus_error_free( &error );
        return FALSE;
    }

    /* retrieve all existing devices */
    if (!(list = p_libhal_get_all_devices( ctx, &num, &error ))) p_dbus_error_free( &error );
    else
    {
        for (i = 0; i < num; i++) hal_new_device( ctx, list[i] );
        p_libhal_free_string_array( list );
    }
    return TRUE;
}

#endif /* SONAME_LIBHAL */

static DWORD WINAPI dbus_thread( void *arg )
{
    static const char udisks_match[] = "type='signal',"
                                       "interface='org.freedesktop.UDisks',"
                                       "sender='org.freedesktop.UDisks'";
    static const char udisks2_match_interfaces[] = "type='signal',"
                                                   "interface='org.freedesktop.DBus.ObjectManager',"
                                                   "path='/org/freedesktop/UDisks2'";
    static const char udisks2_match_properties[] = "type='signal',"
                                                   "interface='org.freedesktop.DBus.Properties'";


    DBusError error;

    p_dbus_error_init( &error );
    if (!(connection = p_dbus_bus_get( DBUS_BUS_SYSTEM, &error )))
    {
        WARN( "failed to get system dbus connection: %s\n", error.message );
        p_dbus_error_free( &error );
        return 1;
    }

    /* first try UDisks2 */

    p_dbus_connection_add_filter( connection, udisks_filter, NULL, NULL );
    p_dbus_bus_add_match( connection, udisks2_match_interfaces, &error );
    p_dbus_bus_add_match( connection, udisks2_match_properties, &error );
    if (udisks2_add_devices( NULL )) goto found;
    p_dbus_bus_remove_match( connection, udisks2_match_interfaces, &error );
    p_dbus_bus_remove_match( connection, udisks2_match_properties, &error );

    /* then try UDisks */

    p_dbus_bus_add_match( connection, udisks_match, &error );
    if (udisks_enumerate_devices()) goto found;
    p_dbus_bus_remove_match( connection, udisks_match, &error );
    p_dbus_connection_remove_filter( connection, udisks_filter, NULL );

    /* then finally HAL */

#ifdef SONAME_LIBHAL
    if (!hal_enumerate_devices())
    {
        p_dbus_error_free( &error );
        return 1;
    }
#endif

found:
    __TRY
    {
        while (p_dbus_connection_read_write_dispatch( connection, -1 )) /* nothing */ ;
    }
    __EXCEPT( assert_fault )
    {
        WARN( "dbus assertion failure, disabling support\n" );
        return 1;
    }
    __ENDTRY;

    return 0;
}

void initialize_dbus(void)
{
    HANDLE handle;

#ifdef SONAME_LIBHAL
    if (!load_hal_functions())
#endif
        if (!load_dbus_functions()) return;
    if (!(handle = CreateThread( NULL, 0, dbus_thread, NULL, 0, NULL ))) return;
    CloseHandle( handle );
}

#else  /* SONAME_LIBDBUS_1 */

void initialize_dbus(void)
{
    TRACE( "Skipping, DBUS support not compiled in\n" );
}

#endif  /* SONAME_LIBDBUS_1 */
