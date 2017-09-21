/*
 * Copyright 2002 Andriy Palamarchuk
 *
 * Conformance test of the access functions.
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

#include <windef.h>
#include <winbase.h>
#include <winerror.h>
#include <lmaccess.h>
#include <lmerr.h>
#include <lmapibuf.h>

#include "wine/test.h"

static WCHAR user_name[UNLEN + 1];
static WCHAR computer_name[MAX_COMPUTERNAME_LENGTH + 1];

static const WCHAR sNonexistentUser[] = {'N','o','n','e','x','i','s','t','e','n','t',' ',
                                'U','s','e','r',0};
static WCHAR sTooLongName[] = {'T','h','i','s',' ','i','s',' ','a',' ','b','a','d',
    ' ','u','s','e','r','n','a','m','e',0};
static WCHAR sTooLongPassword[] = {'a','b','c','d','e','f','g','h','a','b','c','d','e','f','g','h',
    'a','b','c','d','e','f','g','h','a','b','c','d','e','f','g','h','a','b','c','d','e','f','g','h',
    'a','b','c','d','e','f','g','h','a','b','c','d','e','f','g','h','a','b','c','d','e','f','g','h',
    'a','b','c','d','e','f','g','h','a','b','c','d','e','f','g','h','a','b','c','d','e','f','g','h',
    'a','b','c','d','e','f','g','h','a','b','c','d','e','f','g','h','a','b','c','d','e','f','g','h',
    'a','b','c','d','e','f','g','h','a','b','c','d','e','f','g','h','a','b','c','d','e','f','g','h',
    'a','b','c','d','e','f','g','h','a','b','c','d','e','f','g','h','a','b','c','d','e','f','g','h',
    'a','b','c','d','e','f','g','h','a','b','c','d','e','f','g','h','a','b','c','d','e','f','g','h',
    'a','b','c','d','e','f','g','h','a','b','c','d','e','f','g','h','a','b','c','d','e','f','g','h',
    'a','b','c','d','e','f','g','h','a','b','c','d','e','f','g','h','a','b','c','d','e','f','g','h',
    'a','b','c','d','e','f','g','h','a','b','c','d','e','f','g','h','a','b','c','d','e','f','g','h',
    'a', 0};

static WCHAR sTestUserName[] = {'t', 'e', 's', 't', 'u', 's', 'e', 'r', 0};
static WCHAR sTestUserOldPass[] = {'O', 'l', 'd', 'P', 'a', 's', 's', 'W', '0', 'r', 'd', 'S', 'e', 't', '!', '~', 0};
static const WCHAR sBadNetPath[] = {'\\','\\','B','a',' ',' ','p','a','t','h',0};
static const WCHAR sInvalidName[] = {'\\',0};
static const WCHAR sInvalidName2[] = {'\\','\\',0};
static const WCHAR sEmptyStr[] = { 0 };

static NET_API_STATUS (WINAPI *pNetApiBufferFree)(LPVOID);
static NET_API_STATUS (WINAPI *pNetApiBufferSize)(LPVOID,LPDWORD);
static NET_API_STATUS (WINAPI *pNetQueryDisplayInformation)(LPWSTR,DWORD,DWORD,DWORD,DWORD,LPDWORD,PVOID*);
static NET_API_STATUS (WINAPI *pNetUserGetInfo)(LPCWSTR,LPCWSTR,DWORD,LPBYTE*);
static NET_API_STATUS (WINAPI *pNetUserModalsGet)(LPCWSTR,DWORD,LPBYTE*);
static NET_API_STATUS (WINAPI *pNetUserAdd)(LPCWSTR,DWORD,LPBYTE,LPDWORD);
static NET_API_STATUS (WINAPI *pNetUserDel)(LPCWSTR,LPCWSTR);
static NET_API_STATUS (WINAPI *pNetLocalGroupGetInfo)(LPCWSTR,LPCWSTR,DWORD,LPBYTE*);
static NET_API_STATUS (WINAPI *pNetLocalGroupGetMembers)(LPCWSTR,LPCWSTR,DWORD,LPBYTE*,DWORD,LPDWORD,LPDWORD,PDWORD_PTR);
static DWORD (WINAPI *pDavGetHTTPFromUNCPath)(LPCWSTR,LPWSTR,LPDWORD);
static DWORD (WINAPI *pDavGetUNCFromHTTPPath)(LPCWSTR,LPWSTR,LPDWORD);

static BOOL init_access_tests(void)
{
    DWORD dwSize;
    BOOL rc;

    user_name[0] = 0;
    dwSize = sizeof(user_name)/sizeof(WCHAR);
    rc=GetUserNameW(user_name, &dwSize);
    if (rc==FALSE && GetLastError()==ERROR_CALL_NOT_IMPLEMENTED)
    {
        win_skip("GetUserNameW is not available.\n");
        return FALSE;
    }
    ok(rc, "User Name Retrieved\n");

    computer_name[0] = 0;
    dwSize = sizeof(computer_name)/sizeof(WCHAR);
    ok(GetComputerNameW(computer_name, &dwSize), "Computer Name Retrieved\n");
    return TRUE;
}

static NET_API_STATUS create_test_user(void)
{
    USER_INFO_1 usri;

    usri.usri1_name = sTestUserName;
    usri.usri1_password = sTestUserOldPass;
    usri.usri1_priv = USER_PRIV_USER;
    usri.usri1_home_dir = NULL;
    usri.usri1_comment = NULL;
    usri.usri1_flags = UF_SCRIPT;
    usri.usri1_script_path = NULL;

    return pNetUserAdd(NULL, 1, (LPBYTE)&usri, NULL);
}

static NET_API_STATUS delete_test_user(void)
{
    return pNetUserDel(NULL, sTestUserName);
}

static void run_usergetinfo_tests(void)
{
    NET_API_STATUS rc;
    PUSER_INFO_0 ui0 = NULL;
    PUSER_INFO_10 ui10 = NULL;
    DWORD dwSize;

    if((rc = create_test_user()) != NERR_Success )
    {
        skip("Skipping usergetinfo_tests, create_test_user failed: 0x%08x\n", rc);
        return;
    }

    /* Level 0 */
    rc=pNetUserGetInfo(NULL, sTestUserName, 0, (LPBYTE *)&ui0);
    ok(rc == NERR_Success, "NetUserGetInfo level 0 failed: 0x%08x.\n", rc);
    ok(!lstrcmpW(sTestUserName, ui0->usri0_name),"Username mismatch for level 0.\n");
    pNetApiBufferSize(ui0, &dwSize);
    ok(dwSize >= (sizeof(USER_INFO_0) +
                  (lstrlenW(ui0->usri0_name) + 1) * sizeof(WCHAR)),
       "Is allocated with NetApiBufferAllocate\n");

    /* Level 10 */
    rc=pNetUserGetInfo(NULL, sTestUserName, 10, (LPBYTE *)&ui10);
    ok(rc == NERR_Success, "NetUserGetInfo level 10 failed: 0x%08x.\n", rc);
    ok(!lstrcmpW(sTestUserName, ui10->usri10_name), "Username mismatch for level 10.\n");
    pNetApiBufferSize(ui10, &dwSize);
    ok(dwSize >= (sizeof(USER_INFO_10) +
                  (lstrlenW(ui10->usri10_name) + 1 +
                   lstrlenW(ui10->usri10_comment) + 1 +
                   lstrlenW(ui10->usri10_usr_comment) + 1 +
                   lstrlenW(ui10->usri10_full_name) + 1) * sizeof(WCHAR)),
       "Is allocated with NetApiBufferAllocate\n");

    pNetApiBufferFree(ui0);
    pNetApiBufferFree(ui10);

    /* NetUserGetInfo should always work for the current user. */
    rc=pNetUserGetInfo(NULL, user_name, 0, (LPBYTE*)&ui0);
    ok(rc == NERR_Success, "NetUsetGetInfo for current user failed: 0x%08x.\n", rc);
    pNetApiBufferFree(ui0);

    /* errors handling */
    rc=pNetUserGetInfo(NULL, sTestUserName, 10000, (LPBYTE *)&ui0);
    ok(rc == ERROR_INVALID_LEVEL,"Invalid Level: rc=%d\n",rc);
    rc=pNetUserGetInfo(NULL, sNonexistentUser, 0, (LPBYTE *)&ui0);
    ok(rc == NERR_UserNotFound,"Invalid User Name: rc=%d\n",rc);
    todo_wine {
        /* FIXME - Currently Wine can't verify whether the network path is good or bad */
        rc=pNetUserGetInfo(sBadNetPath, sTestUserName, 0, (LPBYTE *)&ui0);
        ok(rc == ERROR_BAD_NETPATH ||
           rc == ERROR_NETWORK_UNREACHABLE ||
           rc == RPC_S_SERVER_UNAVAILABLE ||
           rc == NERR_WkstaNotStarted || /* workstation service not running */
           rc == RPC_S_INVALID_NET_ADDR, /* Some Win7 */
           "Bad Network Path: rc=%d\n",rc);
    }
    rc=pNetUserGetInfo(sEmptyStr, sTestUserName, 0, (LPBYTE *)&ui0);
    ok(rc == ERROR_BAD_NETPATH || rc == NERR_Success,
       "Bad Network Path: rc=%d\n",rc);
    rc=pNetUserGetInfo(sInvalidName, sTestUserName, 0, (LPBYTE *)&ui0);
    ok(rc == ERROR_INVALID_NAME || rc == ERROR_INVALID_HANDLE,"Invalid Server Name: rc=%d\n",rc);
    rc=pNetUserGetInfo(sInvalidName2, sTestUserName, 0, (LPBYTE *)&ui0);
    ok(rc == ERROR_INVALID_NAME || rc == ERROR_INVALID_HANDLE,"Invalid Server Name: rc=%d\n",rc);

    if(delete_test_user() != NERR_Success)
        trace("Deleting the test user failed. You might have to manually delete it.\n");
}

/* Checks Level 1 of NetQueryDisplayInformation */
static void run_querydisplayinformation1_tests(void)
{
    PNET_DISPLAY_USER Buffer, rec;
    DWORD Result, EntryCount;
    DWORD i = 0;
    BOOL hasAdmin = FALSE;
    BOOL hasGuest = FALSE;

    do
    {
        Result = pNetQueryDisplayInformation(
            NULL, 1, i, 1000, MAX_PREFERRED_LENGTH, &EntryCount,
            (PVOID *)&Buffer);

        ok((Result == ERROR_SUCCESS) || (Result == ERROR_MORE_DATA),
           "Information Retrieved\n");
        rec = Buffer;
        for(; EntryCount > 0; EntryCount--)
        {
            if (rec->usri1_user_id == DOMAIN_USER_RID_ADMIN)
            {
                ok(!hasAdmin, "One admin user\n");
                ok(rec->usri1_flags & UF_SCRIPT, "UF_SCRIPT flag is set\n");
                ok(rec->usri1_flags & UF_NORMAL_ACCOUNT, "UF_NORMAL_ACCOUNT flag is set\n");
                hasAdmin = TRUE;
            }
            else if (rec->usri1_user_id == DOMAIN_USER_RID_GUEST)
            {
                ok(!hasGuest, "One guest record\n");
                ok(rec->usri1_flags & UF_SCRIPT, "UF_SCRIPT flag is set\n");
                ok(rec->usri1_flags & UF_NORMAL_ACCOUNT, "UF_NORMAL_ACCOUNT flag is set\n");
                hasGuest = TRUE;
            }

            i = rec->usri1_next_index;
            rec++;
        }

        pNetApiBufferFree(Buffer);
    } while (Result == ERROR_MORE_DATA);

    ok(hasAdmin, "Doesn't have 'Administrator' account\n");
}

static void run_usermodalsget_tests(void)
{
    NET_API_STATUS rc;
    USER_MODALS_INFO_2 * umi2 = NULL;

    rc = pNetUserModalsGet(NULL, 2, (LPBYTE *)&umi2);
    ok(rc == ERROR_SUCCESS, "NetUserModalsGet failed, rc = %d\n", rc);

    if (umi2)
        pNetApiBufferFree(umi2);
}

static void run_userhandling_tests(void)
{
    NET_API_STATUS ret;
    USER_INFO_1 usri;

    usri.usri1_priv = USER_PRIV_USER;
    usri.usri1_home_dir = NULL;
    usri.usri1_comment = NULL;
    usri.usri1_flags = UF_SCRIPT;
    usri.usri1_script_path = NULL;

    usri.usri1_name = sTooLongName;
    usri.usri1_password = sTestUserOldPass;

    ret = pNetUserAdd(NULL, 1, (LPBYTE)&usri, NULL);
    if (ret == NERR_Success || ret == NERR_UserExists)
    {
        /* Windows NT4 does create the user. Delete the user and also if it already existed
         * due to a previous test run on NT4.
         */
        trace("We are on NT4, we have to delete the user with the too long username\n");
        ret = pNetUserDel(NULL, sTooLongName);
        ok(ret == NERR_Success, "Deleting the user failed : %d\n", ret);
    }
    else if (ret == ERROR_ACCESS_DENIED)
    {
        skip("not enough permissions to add a user\n");
        return;
    }
    else
        ok(ret == NERR_BadUsername ||
           broken(ret == NERR_PasswordTooShort), /* NT4 */
           "Adding user with too long username returned 0x%08x\n", ret);

    usri.usri1_name = sTestUserName;
    usri.usri1_password = sTooLongPassword;

    ret = pNetUserAdd(NULL, 1, (LPBYTE)&usri, NULL);
    ok(ret == NERR_PasswordTooShort || ret == ERROR_ACCESS_DENIED /* Win2003 */,
       "Adding user with too long password returned 0x%08x\n", ret);

    usri.usri1_name = sTooLongName;
    usri.usri1_password = sTooLongPassword;

    ret = pNetUserAdd(NULL, 1, (LPBYTE)&usri, NULL);
    /* NT4 doesn't have a problem with the username so it will report the too long password
     * as the error. NERR_PasswordTooShort is reported for all kind of password related errors.
     */
    ok(ret == NERR_BadUsername || ret == NERR_PasswordTooShort,
            "Adding user with too long username/password returned 0x%08x\n", ret);

    usri.usri1_name = sTestUserName;
    usri.usri1_password = sTestUserOldPass;

    ret = pNetUserAdd(NULL, 5, (LPBYTE)&usri, NULL);
    ok(ret == ERROR_INVALID_LEVEL, "Adding user with level 5 returned 0x%08x\n", ret);

    ret = pNetUserAdd(NULL, 1, (LPBYTE)&usri, NULL);
    if(ret == ERROR_ACCESS_DENIED)
    {
        skip("Insufficient permissions to add users. Skipping test.\n");
        return;
    }
    if(ret == NERR_UserExists)
    {
        skip("User already exists, skipping test to not mess up the system\n");
        return;
    }

    ok(ret == NERR_Success ||
       broken(ret == NERR_PasswordTooShort), /* NT4 */
       "Adding user failed with error 0x%08x\n", ret);
    if(ret != NERR_Success)
        return;

    /* On Windows XP (and newer), calling NetUserChangePassword with a NULL
     * domainname parameter creates a user home directory, iff the machine is
     * not member of a domain.
     * Using \\127.0.0.1 as domain name does not work on standalone machines
     * either, unless the ForceGuest option in the registry is turned off.
     * So let's not test NetUserChangePassword for now.
     */

    ret = pNetUserDel(NULL, sTestUserName);
    ok(ret == NERR_Success, "Deleting the user failed.\n");

    ret = pNetUserDel(NULL, sTestUserName);
    ok(ret == NERR_UserNotFound, "Deleting a nonexistent user returned 0x%08x\n",ret);
}

static void run_localgroupgetinfo_tests(void)
{
    NET_API_STATUS status;
    static const WCHAR admins[] = {'A','d','m','i','n','i','s','t','r','a','t','o','r','s',0};
    PLOCALGROUP_INFO_1 lgi = NULL;
    PLOCALGROUP_MEMBERS_INFO_3 buffer = NULL;
    DWORD entries_read = 0, total_entries =0;
    int i;

    status = pNetLocalGroupGetInfo(NULL, admins, 1, (LPBYTE *)&lgi);
    ok(status == NERR_Success || broken(status == NERR_GroupNotFound),
       "NetLocalGroupGetInfo unexpectedly returned %d\n", status);
    if (status != NERR_Success) return;

    trace("Local groupname:%s\n", wine_dbgstr_w( lgi->lgrpi1_name));
    trace("Comment: %s\n", wine_dbgstr_w( lgi->lgrpi1_comment));

    pNetApiBufferFree(lgi);

    status = pNetLocalGroupGetMembers(NULL, admins, 3, (LPBYTE *)&buffer, MAX_PREFERRED_LENGTH, &entries_read, &total_entries, NULL);
    ok(status == NERR_Success, "NetLocalGroupGetMembers unexpectedly returned %d\n", status);
    ok(entries_read > 0 && total_entries > 0, "Amount of entries is unexpectedly 0\n");

    for(i=0;i<entries_read;i++)
        trace("domain and name: %s\n", wine_dbgstr_w(buffer[i].lgrmi3_domainandname));

    pNetApiBufferFree(buffer);
}

static void test_DavGetHTTPFromUNCPath(void)
{
    static const WCHAR path[] =
        {0};
    static const WCHAR path2[] =
        {'c',':','\\',0};
    static const WCHAR path3[] =
        {'\\','\\','.','\\','c',':',0};
    static const WCHAR path4[] =
        {'\\','\\','.','\\','c',':','\\',0};
    static const WCHAR path5[] =
        {'\\','\\','.','\\','c',':','\\','n','o','s','u','c','h','p','a','t','h',0};
    static const WCHAR path6[] =
        {'\\','\\','n','o','s','u','c','h','s','e','r','v','e','r','\\','c',':','\\',0};
    static const WCHAR path7[] =
        {'\\','.','\\','c',':',0};
    static const WCHAR path8[] =
        {'\\','\\','.','\\','c',':','\\','\\',0};
    static const WCHAR path9[] =
        {'\\','\\','.','@','S','S','L','\\','c',':',0};
    static const WCHAR path10[] =
        {'\\','\\','.','@','s','s','l','\\','c',':',0};
    static const WCHAR path11[] =
        {'\\','\\','.','@','t','l','s','\\','c',':',0};
    static const WCHAR path12[] =
        {'\\','\\','.','@','S','S','L','@','4','4','3','\\','c',':',0};
    static const WCHAR path13[] =
        {'\\','\\','.','@','S','S','L','@','8','0','\\','c',':',0};
    static const WCHAR path14[] =
        {'\\','\\','.','@','8','0','\\','c',':',0};
    static const WCHAR path15[] =
        {'\\','\\','.','@','8','0','8','0','\\','c',':',0};
    static const WCHAR path16[] =
        {'\\','\\','\\','c',':',0};
    static const WCHAR path17[] =
        {'\\','\\',0};
    static const WCHAR path18[] =
        {'/','/','.','/','c',':',0};
    static const WCHAR path19[] =
        {'\\','\\','.','\\','c',':','/',0};
    static const WCHAR path20[] =
        {'\\','\\','.','\\','c',':','\\','\\','\\',0};
    static const WCHAR path21[] =
        {'\\','\\','.','\\','\\','c',':',0};
    static const WCHAR path22[] =
        {'\\','\\','.','\\','c',':','d','i','r',0};
    static const WCHAR path23[] =
        {'\\','\\','.',0};
    static const WCHAR path24[] =
        {'\\','\\','.','\\','d','i','r',0};
    static const WCHAR path25[] =
        {'\\','\\','.','\\','\\',0};
    static const WCHAR path26[] =
        {'\\','\\','.','\\','c',':','d','i','r','/',0};
    static const WCHAR path27[] =
        {'\\','\\','.','/','c',':',0};
    static const WCHAR path28[] =
        {'\\','\\','.','@','8','0','@','S','S','L','\\','c',':',0};
    static const WCHAR result[] =
        {'h','t','t','p',':','/','/','.','/','c',':',0};
    static const WCHAR result2[] =
        {'h','t','t','p',':','/','/','.','/','c',':','/','n','o','s','u','c','h','p','a','t','h',0};
    static const WCHAR result3[] =
        {'h','t','t','p',':','/','/','n','o','s','u','c','h','s','e','r','v','e','r','/','c',':',0};
    static const WCHAR result4[] =
        {'h','t','t','p',':','/','/','.','/','c',':','/',0};
    static const WCHAR result5[] =
        {'h','t','t','p','s',':','/','/','.','/','c',':',0};
    static const WCHAR result6[] =
        {'h','t','t','p','s',':','/','/','.',':','8','0','/','c',':',0};
    static const WCHAR result7[] =
        {'h','t','t','p',':','/','/','.',':','8','0','8','0','/','c',':',0};
    static const WCHAR result8[] =
        {'h','t','t','p',':','/','/','/','c',':',0};
    static const WCHAR result9[] =
        {'h','t','t','p',':','/','/','.','/','c',':','/','/',0};
    static const WCHAR result10[] =
        {'h','t','t','p',':','/','/','.','/','/','c',':',0};
    static const WCHAR result11[] =
        {'h','t','t','p',':','/','/','.','/','c',':','d','i','r',0};
    static const WCHAR result12[] =
        {'h','t','t','p',':','/','/','.',0};
    static const WCHAR result13[] =
        {'h','t','t','p',':','/','/','.','/','d','i','r',0};
    static const WCHAR result14[] =
        {'h','t','t','p',':','/','/','.','/',0};
    static const struct
    {
        const WCHAR *path;
        DWORD        size;
        DWORD        ret;
        const WCHAR *ret_path;
        DWORD        ret_size;
        int          todo;
    }
    tests[] =
    {
        { path, MAX_PATH, ERROR_INVALID_PARAMETER, NULL, MAX_PATH },
        { path2, MAX_PATH, ERROR_INVALID_PARAMETER, NULL, MAX_PATH },
        { path3, MAX_PATH, ERROR_SUCCESS, result, 12 },
        { path4, MAX_PATH, ERROR_SUCCESS, result, 12 },
        { path5, MAX_PATH, ERROR_SUCCESS, result2, 23 },
        { path6, MAX_PATH, ERROR_SUCCESS, result3, 23 },
        { path7, MAX_PATH, ERROR_INVALID_PARAMETER, NULL, MAX_PATH },
        { path8, MAX_PATH, ERROR_SUCCESS, result4, 13 },
        { path9, MAX_PATH, ERROR_SUCCESS, result5, 13 },
        { path10, MAX_PATH, ERROR_SUCCESS, result5, 13 },
        { path11, MAX_PATH, ERROR_INVALID_PARAMETER, NULL, MAX_PATH },
        { path12, MAX_PATH, ERROR_SUCCESS, result5, 13 },
        { path13, MAX_PATH, ERROR_SUCCESS, result6, 16 },
        { path14, MAX_PATH, ERROR_SUCCESS, result, 12 },
        { path15, MAX_PATH, ERROR_SUCCESS, result7, 17 },
        { path16, MAX_PATH, ERROR_SUCCESS, result8, 11 },
        { path17, MAX_PATH, ERROR_INVALID_PARAMETER, NULL, MAX_PATH },
        { path18, MAX_PATH, ERROR_INVALID_PARAMETER, NULL, MAX_PATH },
        { path19, MAX_PATH, ERROR_SUCCESS, result, 12 },
        { path20, MAX_PATH, ERROR_SUCCESS, result9, 14 },
        { path21, MAX_PATH, ERROR_SUCCESS, result10, 13 },
        { path22, MAX_PATH, ERROR_SUCCESS, result11, 15 },
        { path23, MAX_PATH, ERROR_SUCCESS, result12, 9 },
        { path24, MAX_PATH, ERROR_SUCCESS, result13, 13 },
        { path25, MAX_PATH, ERROR_SUCCESS, result14, 10, 1 },
        { path26, MAX_PATH, ERROR_SUCCESS, result11, 15 },
        { path27, MAX_PATH, ERROR_SUCCESS, result, 12 },
        { path28, MAX_PATH, ERROR_INVALID_PARAMETER, NULL, MAX_PATH },
    };
    WCHAR buf[MAX_PATH];
    DWORD i, ret, size;

    if (!pDavGetHTTPFromUNCPath)
    {
        win_skip( "DavGetHTTPFromUNCPath is missing\n" );
        return;
    }

    if (0) { /* crash */
    ret = pDavGetHTTPFromUNCPath( NULL, NULL, NULL );
    ok( ret == ERROR_INVALID_PARAMETER, "got %u\n", ret );
    }

    ret = pDavGetHTTPFromUNCPath( path, buf, NULL );
    ok( ret == ERROR_INVALID_PARAMETER, "got %u\n", ret );

    size = 0;
    ret = pDavGetHTTPFromUNCPath( path, NULL, &size );
    ok( ret == ERROR_INVALID_PARAMETER, "got %u\n", ret );

    if (0) { /* crash */
    buf[0] = 0;
    size = 0;
    ret = pDavGetHTTPFromUNCPath( path, buf, &size );
    ok( ret == ERROR_INVALID_PARAMETER, "got %u\n", ret );

    ret = pDavGetHTTPFromUNCPath( path3, buf, NULL );
    ok( ret == ERROR_INVALID_PARAMETER, "got %u\n", ret );
    }

    size = 0;
    ret = pDavGetHTTPFromUNCPath( path3, NULL, &size );
    ok( ret == ERROR_INSUFFICIENT_BUFFER, "got %u\n", ret );

    buf[0] = 0;
    size = 0;
    ret = pDavGetHTTPFromUNCPath( path3, buf, &size );
    ok( ret == ERROR_INSUFFICIENT_BUFFER, "got %u\n", ret );
    ok( size == 12, "got %u\n", size );

    for (i = 0; i < sizeof(tests)/sizeof(tests[0]); i++)
    {
        buf[0] = 0;
        size = tests[i].size;
        ret = pDavGetHTTPFromUNCPath( tests[i].path, buf, &size );
        if (tests[i].todo)
        {
            ok( ret == tests[i].ret, "%u: expected %u got %u\n", i, tests[i].ret, ret );
            todo_wine {
            if (tests[i].ret_path)
            {
                ok( !lstrcmpW( buf, tests[i].ret_path ), "%u: expected %s got %s\n",
                    i, wine_dbgstr_w(tests[i].ret_path), wine_dbgstr_w(buf) );
            }
            ok( size == tests[i].ret_size, "%u: expected %u got %u\n", i, tests[i].ret_size, size );
            }
        }
        else
        {
            ok( ret == tests[i].ret, "%u: expected %u got %u\n", i, tests[i].ret, ret );
            if (tests[i].ret_path)
            {
                ok( !lstrcmpW( buf, tests[i].ret_path ), "%u: expected %s got %s\n",
                    i, wine_dbgstr_w(tests[i].ret_path), wine_dbgstr_w(buf) );
            }
            ok( size == tests[i].ret_size, "%u: expected %u got %u\n", i, tests[i].ret_size, size );
        }
    }
}

static void test_DavGetUNCFromHTTPPath(void)
{
    static const WCHAR path[] =
        {0};
    static const WCHAR path2[] =
        {'h','t','t','p',':','/','/','s','e','r','v','e','r','/','p','a','t','h',0};
    static const WCHAR path3[] =
        {'h','t','t','p','s',':','/','/','h','o','s','t','/','p','a','t','h',0};
    static const WCHAR path4[] =
        {'\\','\\','s','e','r','v','e','r',0};
    static const WCHAR path5[] =
        {'\\','\\','s','e','r','v','e','r','\\','p','a','t','h',0};
    static const WCHAR path6[] =
        {'\\','\\','h','t','t','p',':','/','/','s','e','r','v','e','r','/','p','a','t','h',0};
    static const WCHAR path7[] =
        {'h','t','t','p',':','/','/',0};
    static const WCHAR path8[] =
        {'h','t','t','p',':',0};
    static const WCHAR path9[] =
        {'h','t','t','p',0};
    static const WCHAR path10[] =
        {'h','t','t','p',':','s','e','r','v','e','r',0};
    static const WCHAR path11[] =
        {'h','t','t','p',':','/','/','s','e','r','v','e','r',':','8','0',0};
    static const WCHAR path12[] =
        {'h','t','t','p',':','/','/','s','e','r','v','e','r',':','8','1',0};
    static const WCHAR path13[] =
        {'h','t','t','p','s',':','/','/','s','e','r','v','e','r',':','8','0',0};
    static const WCHAR path14[] =
        {'H','T','T','P',':','/','/','s','e','r','v','e','r','/','p','a','t','h',0};
    static const WCHAR path15[] =
        {'h','t','t','p',':','/','/','s','e','r','v','e','r',':','6','5','5','3','7',0};
    static const WCHAR path16[] =
        {'h','t','t','p',':','/','/','s','e','r','v','e','r','/','p','a','t','h','/',0};
    static const WCHAR path17[] =
        {'h','t','t','p',':','/','/','s','e','r','v','e','r','/','p','a','t','h','/','/',0};
    static const WCHAR path18[] =
        {'h','t','t','p',':','/','/','s','e','r','v','e','r',':','/','p','a','t','h',0};
    static const WCHAR path19[] =
        {'h','t','t','p',':','/','/','s','e','r','v','e','r',0};
    static const WCHAR path20[] =
        {'h','t','t','p','s',':','/','/','s','e','r','v','e','r',':','4','4','3',0};
    static const WCHAR path21[] =
        {'h','t','t','p','s',':','/','/','s','e','r','v','e','r',':','8','0',0};
    static const WCHAR result[] =
        {'\\','\\','s','e','r','v','e','r','\\','D','a','v','W','W','W','R','o','o','t','\\','p','a','t','h',0};
    static const WCHAR result2[] =
        {'\\','\\','h','o','s','t','@','S','S','L','\\','D','a','v','W','W','W','R','o','o','t','\\',
         'p','a','t','h',0};
    static const WCHAR result3[] =
        {'\\','\\','s','e','r','v','e','r','\\','D','a','v','W','W','W','R','o','o','t',0};
    static const WCHAR result4[] =
        {'\\','\\','s','e','r','v','e','r','@','8','1','\\','D','a','v','W','W','W','R','o','o','t',0};
    static const WCHAR result5[] =
        {'\\','\\','s','e','r','v','e','r','@','S','S','L','@','8','0','\\','D','a','v','W','W','W','R','o','o','t',0};
    static const WCHAR result6[] =
        {'\\','\\','s','e','r','v','e','r','@','6','5','5','3','7','\\','D','a','v','W','W','W','R','o','o','t',0};
    static const WCHAR result7[] =
        {'\\','\\','s','e','r','v','e','r','@','\\','D','a','v','W','W','W','R','o','o','t','\\','p','a','t','h',0};
    static const WCHAR result8[] =
        {'\\','\\','s','e','r','v','e','r','@','S','S','L','\\','D','a','v','W','W','W','R','o','o','t',0};
    static const WCHAR result9[] =
        {'\\','\\','s','e','r','v','e','r','@','S','S','L','@','8','0','\\','D','a','v','W','W','W','R','o','o','t',0};
    static const struct
    {
        const WCHAR *path;
        DWORD        size;
        DWORD        ret;
        const WCHAR *ret_path;
        DWORD        ret_size;
    }
    tests[] =
    {
        { path, MAX_PATH, ERROR_INVALID_PARAMETER, NULL, MAX_PATH },
        { path2, MAX_PATH, ERROR_SUCCESS, result, 25 },
        { path3, MAX_PATH, ERROR_SUCCESS, result2, 27 },
        { path4, MAX_PATH, ERROR_INVALID_PARAMETER, NULL, MAX_PATH },
        { path5, MAX_PATH, ERROR_INVALID_PARAMETER, NULL, MAX_PATH },
        { path6, MAX_PATH, ERROR_INVALID_PARAMETER, NULL, MAX_PATH },
        { path7, MAX_PATH, ERROR_BAD_NET_NAME, NULL, MAX_PATH },
        { path8, MAX_PATH, ERROR_INVALID_PARAMETER, NULL, MAX_PATH },
        { path9, MAX_PATH, ERROR_INVALID_PARAMETER, NULL, MAX_PATH },
        { path10, MAX_PATH, ERROR_INVALID_PARAMETER, NULL, MAX_PATH },
        { path11, MAX_PATH, ERROR_SUCCESS, result3, 20 },
        { path12, MAX_PATH, ERROR_SUCCESS, result4, 23 },
        { path13, MAX_PATH, ERROR_SUCCESS, result5, 27 },
        { path14, MAX_PATH, ERROR_SUCCESS, result, 25 },
        { path15, MAX_PATH, ERROR_SUCCESS, result6, 26 },
        { path16, MAX_PATH, ERROR_SUCCESS, result, 25 },
        { path17, MAX_PATH, ERROR_BAD_NET_NAME, NULL, MAX_PATH },
        { path18, MAX_PATH, ERROR_SUCCESS, result7, 26 },
        { path19, MAX_PATH, ERROR_SUCCESS, result3, 20 },
        { path20, MAX_PATH, ERROR_SUCCESS, result8, 24 },
        { path21, MAX_PATH, ERROR_SUCCESS, result9, 27 },
    };
    WCHAR buf[MAX_PATH];
    DWORD i, ret, size;

    if (!pDavGetUNCFromHTTPPath)
    {
        win_skip( "DavGetUNCFromHTTPPath is missing\n" );
        return;
    }

    if (0) { /* crash */
    ret = pDavGetUNCFromHTTPPath( NULL, NULL, NULL );
    ok( ret == ERROR_INVALID_PARAMETER, "got %u\n", ret );
    }
    ret = pDavGetUNCFromHTTPPath( path, buf, NULL );
    ok( ret == ERROR_INVALID_PARAMETER, "got %u\n", ret );

    size = 0;
    ret = pDavGetUNCFromHTTPPath( path, NULL, &size );
    ok( ret == ERROR_INVALID_PARAMETER, "got %u\n", ret );

    buf[0] = 0;
    size = 0;
    ret = pDavGetUNCFromHTTPPath( path, buf, &size );
    ok( ret == ERROR_INVALID_PARAMETER, "got %u\n", ret );

    if (0) { /* crash */
    ret = pDavGetUNCFromHTTPPath( path2, buf, NULL );
    ok( ret == ERROR_INVALID_PARAMETER, "got %u\n", ret );
    }
    size = 0;
    ret = pDavGetUNCFromHTTPPath( path2, NULL, &size );
    ok( ret == ERROR_INSUFFICIENT_BUFFER, "got %u\n", ret );

    buf[0] = 0;
    size = 0;
    ret = pDavGetUNCFromHTTPPath( path2, buf, &size );
    ok( ret == ERROR_INSUFFICIENT_BUFFER, "got %u\n", ret );
    ok( size == 25, "got %u\n", size );

    for (i = 0; i < sizeof(tests)/sizeof(tests[0]); i++)
    {
        buf[0] = 0;
        size = tests[i].size;
        ret = pDavGetUNCFromHTTPPath( tests[i].path, buf, &size );
        ok( ret == tests[i].ret, "%u: expected %u got %u\n", i, tests[i].ret, ret );
        if (tests[i].ret_path)
        {
            ok( !lstrcmpW( buf, tests[i].ret_path ), "%u: expected %s got %s\n",
                i, wine_dbgstr_w(tests[i].ret_path), wine_dbgstr_w(buf) );
        }
        ok( size == tests[i].ret_size, "%u: expected %u got %u\n", i, tests[i].ret_size, size );
    }
}


START_TEST(access)
{
    HMODULE hnetapi32=LoadLibraryA("netapi32.dll");

    pNetApiBufferFree=(void*)GetProcAddress(hnetapi32,"NetApiBufferFree");
    pNetApiBufferSize=(void*)GetProcAddress(hnetapi32,"NetApiBufferSize");
    pNetQueryDisplayInformation=(void*)GetProcAddress(hnetapi32,"NetQueryDisplayInformation");
    pNetUserGetInfo=(void*)GetProcAddress(hnetapi32,"NetUserGetInfo");
    pNetUserModalsGet=(void*)GetProcAddress(hnetapi32,"NetUserModalsGet");
    pNetUserAdd=(void*)GetProcAddress(hnetapi32, "NetUserAdd");
    pNetUserDel=(void*)GetProcAddress(hnetapi32, "NetUserDel");
    pNetLocalGroupGetInfo=(void*)GetProcAddress(hnetapi32, "NetLocalGroupGetInfo");
    pNetLocalGroupGetMembers=(void*)GetProcAddress(hnetapi32, "NetLocalGroupGetMembers");
    pDavGetHTTPFromUNCPath = (void*)GetProcAddress(hnetapi32, "DavGetHTTPFromUNCPath");
    pDavGetUNCFromHTTPPath = (void*)GetProcAddress(hnetapi32, "DavGetUNCFromHTTPPath");

    /* These functions were introduced with NT. It's safe to assume that
     * if one is not available, none are.
     */
    if (!pNetApiBufferFree) {
        win_skip("Needed functions are not available\n");
        FreeLibrary(hnetapi32);
        return;
    }

    if (init_access_tests()) {
        run_userhandling_tests();
        run_usergetinfo_tests();
        run_querydisplayinformation1_tests();
        run_usermodalsget_tests();
        run_localgroupgetinfo_tests();
    }

    test_DavGetHTTPFromUNCPath();
    test_DavGetUNCFromHTTPPath();
    FreeLibrary(hnetapi32);
}
