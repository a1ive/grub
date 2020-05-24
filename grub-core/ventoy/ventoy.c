/******************************************************************************
 * ventoy.c 
 *
 * Copyright (c) 2020, longpanda <admin@ventoy.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <grub/types.h>
#include <grub/misc.h>
#include <grub/mm.h>
#include <grub/err.h>
#include <grub/dl.h>
#include <grub/disk.h>
#include <grub/device.h>
#include <grub/term.h>
#include <grub/partition.h>
#include <grub/file.h>
#include <grub/normal.h>
#include <grub/extcmd.h>
#include <grub/datetime.h>
#include <grub/i18n.h>
#include <grub/net.h>
#ifdef GRUB_MACHINE_EFI
#include <grub/efi/efi.h>
#endif
#include <grub/time.h>
#include <grub/relocator.h>
#include "ventoy.h"
#include "ventoy_def.h"
#include "ventoy_wrap.h"

GRUB_MOD_LICENSE ("GPLv3+");

int g_ventoy_debug = 0;
initrd_info *g_initrd_img_list = NULL;
initrd_info *g_initrd_img_tail = NULL;
int g_initrd_img_count = 0;
int g_valid_initrd_count = 0;

char g_img_swap_tmp_buf[1024];

grub_uint8_t *g_ventoy_cpio_buf = NULL;
grub_uint32_t g_ventoy_cpio_size = 0;
cpio_newc_header *g_ventoy_initrd_head = NULL;
grub_uint8_t *g_ventoy_runtime_buf = NULL;

ventoy_grub_param *g_grub_param = NULL;

ventoy_img_chunk_list g_img_chunk_list;

void ventoy_debug(const char *fmt, ...)
{
    va_list args;

    va_start (args, fmt);
    grub_vprintf (fmt, args);
    va_end (args);
}

int ventoy_is_efi_os(void)
{
#ifdef GRUB_MACHINE_EFI
    return 1;
#else
    return 0;
#endif
}

static grub_err_t ventoy_cmd_debug(grub_extcmd_context_t ctxt, int argc, char **args)
{
    if (argc != 1)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Usage: %s {on|off}", cmd_raw_name);
    }

    if (0 == grub_strcmp(args[0], "on"))
    {
        g_ventoy_debug = 1;
        grub_env_set("vtdebug_flag", "debug");
    }
    else
    {
        g_ventoy_debug = 0;
        grub_env_set("vtdebug_flag", "");
    }

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

static grub_err_t ventoy_cmd_break(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;

    if (argc < 1 || (args[0][0] != '0' && args[0][0] != '1'))
    {
        grub_printf("Usage: %s {level} [debug]\r\n", cmd_raw_name);
        grub_printf(" level:\r\n");
        grub_printf("    01/11: busybox / (+cat log)\r\n");
        grub_printf("    02/12: initrd / (+cat log)\r\n");
        grub_printf("    03/13: hook / (+cat log)\r\n");
        grub_printf("\r\n");
        grub_printf(" debug:\r\n");
        grub_printf("    0: debug is on\r\n");
        grub_printf("    1: debug is off\r\n");
        grub_printf("\r\n");
        VENTOY_CMD_RETURN(GRUB_ERR_NONE);
    }

    g_ventoy_break_level = (grub_uint8_t)grub_strtoul(args[0], NULL, 16);

    if (argc > 1 && grub_strtoul(args[1], NULL, 10) > 0)
    {
        g_ventoy_debug_level = 1;
    }

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

static grub_err_t ventoy_cmd_check_compatible(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int i;
    char buf[256];
    grub_disk_t disk;
    char *pos = NULL;
    const char *files[] = { "ventoy.dat", "VENTOY.DAT" };

    (void)ctxt;
    
    if (argc != 1)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Usage: %s  (loop)", cmd_raw_name);
    }

    for (i = 0; i < (int)ARRAY_SIZE(files); i++)
    {
        grub_snprintf(buf, sizeof(buf) - 1, "[ -e %s/%s ]", args[0], files[i]);
        if (0 == grub_script_execute_sourcecode(buf))
        {
            debug("file %s exist, ventoy_compatible YES\n", buf);
            grub_env_set("ventoy_compatible", "YES");
            VENTOY_CMD_RETURN(GRUB_ERR_NONE);
        }
        else
        {
            debug("file %s NOT exist\n", buf);
        }
    }
    
    grub_snprintf(buf, sizeof(buf) - 1, "%s", args[0][0] == '(' ? (args[0] + 1) : args[0]);
    pos = grub_strstr(buf, ")");
    if (pos)
    {
        *pos = 0;
    }

    disk = grub_disk_open(buf);
    if (disk)
    {
        grub_disk_read(disk, 16 << 2, 0, 1024, g_img_swap_tmp_buf);
        grub_disk_close(disk);
        
        g_img_swap_tmp_buf[703] = 0;
        for (i = 319; i < 703; i++)
        {
            if (g_img_swap_tmp_buf[i] == 'V' &&
                0 == grub_strncmp(g_img_swap_tmp_buf + i, VENTOY_COMPATIBLE_STR, VENTOY_COMPATIBLE_STR_LEN))
            {
                debug("Ventoy compatible string exist at  %d, ventoy_compatible YES\n", i);
                grub_env_set("ventoy_compatible", "YES");
                VENTOY_CMD_RETURN(GRUB_ERR_NONE);
            }
        }
    }
    else
    {
        debug("failed to open disk <%s>\n", buf);
    }

    grub_env_set("ventoy_compatible", "NO");
    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_uint32_t ventoy_get_iso_boot_catlog(grub_file_t file)
{
    eltorito_descriptor desc;

    grub_memset(&desc, 0, sizeof(desc));
    grub_file_seek(file, 17 * 2048);
    grub_file_read(file, &desc, sizeof(desc));

    if (desc.type != 0 || desc.version != 1)
    {
        return 0;
    }

    if (grub_strncmp((char *)desc.id, "CD001", 5) != 0 ||
        grub_strncmp((char *)desc.system_id, "EL TORITO SPECIFICATION", 23) != 0)
    {
        return 0;
    }

    return desc.sector;
}

int ventoy_has_efi_eltorito(grub_file_t file, grub_uint32_t sector)
{
    int i;
    grub_uint8_t buf[512];

    grub_file_seek(file, sector * 2048);
    grub_file_read(file, buf, sizeof(buf));

    if (buf[0] == 0x01 && buf[1] == 0xEF)
    {
        debug("%s efi eltorito in Validation Entry\n", file->name);
        return 1;
    }

    for (i = 64; i < (int)sizeof(buf); i += 32)
    {
        if ((buf[i] == 0x90 || buf[i] == 0x91) && buf[i + 1] == 0xEF)
        {
            debug("%s efi eltorito offset %d 0x%02x\n", file->name, i, buf[i]);
            return 1;
        }
    }

    debug("%s does not contain efi eltorito\n", file->name);
    return 0;
}

static grub_err_t ventoy_cmd_img_sector(grub_extcmd_context_t ctxt, int argc, char **args)
{
    grub_file_t file;

    (void)ctxt;
    (void)argc;

    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s", args[0]);
    if (!file)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Can't open file %s\n", args[0]); 
    }

    if (g_img_chunk_list.chunk)
    {
        grub_free(g_img_chunk_list.chunk);
    }

    /* get image chunk data */
    grub_memset(&g_img_chunk_list, 0, sizeof(g_img_chunk_list));
    g_img_chunk_list.chunk = grub_malloc(sizeof(ventoy_img_chunk) * DEFAULT_CHUNK_NUM);
    if (NULL == g_img_chunk_list.chunk)
    {
        return grub_error(GRUB_ERR_OUT_OF_MEMORY, "Can't allocate image chunk memoty\n");
    }

    g_img_chunk_list.max_chunk = DEFAULT_CHUNK_NUM;
    g_img_chunk_list.cur_chunk = 0;

    debug("get fat file chunk part start:%llu\n",
          grub_partition_get_start (file->device->disk->partition));
    vt_get_file_chunk(grub_partition_get_start (file->device->disk->partition), file, &g_img_chunk_list);

    grub_file_close(file);

    grub_memset(&g_grub_param->file_replace, 0, sizeof(g_grub_param->file_replace));

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

static grub_err_t ventoy_cmd_dump_img_sector(grub_extcmd_context_t ctxt, int argc, char **args)
{
    grub_uint32_t i;
    ventoy_img_chunk *cur;
    
    (void)ctxt;
    (void)argc;
    (void)args;

    for (i = 0; i < g_img_chunk_list.cur_chunk; i++)
    {
        cur = g_img_chunk_list.chunk + i;
        grub_printf("image:[%u - %u]   <==>  disk:[%llu - %llu]\n", 
            cur->img_start_sector, cur->img_end_sector,
            (unsigned long long)cur->disk_start_sector, (unsigned long long)cur->disk_end_sector
            );
    }

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

#ifdef GRUB_MACHINE_PCBIOS
static grub_err_t ventoy_cmd_relocator_chaindata(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int rc = 0;
    ulong chain_len = 0;
    char *chain_data = NULL;
    char *relocator_addr = NULL;
    grub_relocator_chunk_t ch;
    struct grub_relocator *relocator = NULL;
    char envbuf[64] = { 0 };

    (void)ctxt;
    (void)argc;
    (void)args;

    if (argc != 2)
    {
        return 1;
    }

    chain_data = (char *)grub_strtoul(args[0], NULL, 16);
    chain_len = grub_strtoul(args[1], NULL, 10);

    relocator = grub_relocator_new ();
    if (!relocator)
    {
        debug("grub_relocator_new failed %p %lu\n", chain_data, chain_len);
        return 1;
    }

    rc = grub_relocator_alloc_chunk_addr (relocator, &ch,
					   0x100000, // GRUB_LINUX_BZIMAGE_ADDR,
					   chain_len);
    if (rc)
    {
        debug("grub_relocator_alloc_chunk_addr failed %d %p %lu\n", rc, chain_data, chain_len);
        grub_relocator_unload (relocator);
        return 1;
    }

    relocator_addr = get_virtual_current_address(ch);

    grub_memcpy(relocator_addr, chain_data, chain_len);

    grub_relocator_unload (relocator);

    grub_snprintf(envbuf, sizeof(envbuf), "0x%lx", (unsigned long)relocator_addr);
    grub_env_set("vtoy_chain_relocator_addr", envbuf);

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}
#else
static grub_err_t ventoy_cmd_relocator_chaindata(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    (void)argc;
    (void)args;
    return 0;
}
#endif

static grub_err_t ventoy_cmd_add_replace_file(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int i;
    ventoy_grub_param_file_replace *replace = NULL;
    
    (void)ctxt;
    (void)argc;
    (void)args;

    if (argc >= 2)
    {
        replace = &(g_grub_param->file_replace);
        replace->magic = GRUB_FILE_REPLACE_MAGIC;
            
        replace->old_name_cnt = 0;
        for (i = 0; i < 4 && i + 1 < argc; i++)
        {
            replace->old_name_cnt++;
            grub_snprintf(replace->old_file_name[i], sizeof(replace->old_file_name[i]), "%s", args[i + 1]);
        }
        
        replace->new_file_virtual_id = (grub_uint32_t)grub_strtoul(args[0], NULL, 10);
    }

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_file_t ventoy_grub_file_open(enum grub_file_type type, const char *fmt, ...)
{
    va_list ap;
    grub_file_t file;
    char fullpath[256] = {0};

    va_start (ap, fmt);
    grub_vsnprintf(fullpath, 255, fmt, ap);
    va_end (ap);

    file = grub_file_open(fullpath, type);
    if (!file)
    {
        debug("grub_file_open failed <%s>\n", fullpath);
        grub_errno = 0;
    }

    return file;
}

static int
ventoy_env_init (void)
{
    char buf[64];

    grub_env_set("vtdebug_flag", "");

    g_grub_param = (ventoy_grub_param *)grub_zalloc(sizeof(ventoy_grub_param));
    if (g_grub_param)
    {
        g_grub_param->grub_env_get = grub_env_get;
        grub_snprintf(buf, sizeof(buf), "%p", g_grub_param);
        grub_env_set("env_param", buf);
    }

    return 0;
}

static cmd_para ventoy_cmds[] = 
{
    { "vt_debug", ventoy_cmd_debug, 0, NULL, "{on|off}",   "turn debug on/off",    NULL },
    { "vtdebug", ventoy_cmd_debug, 0, NULL, "{on|off}",   "turn debug on/off",    NULL },
    { "vtbreak", ventoy_cmd_break, 0, NULL, "{level}",   "set debug break",    NULL },
    { "vt_check_compatible",   ventoy_cmd_check_compatible, 0, NULL, "", "", NULL },
    { "vt_img_sector", ventoy_cmd_img_sector, 0, NULL, "{imageName}", "", NULL },
    { "vt_dump_img_sector", ventoy_cmd_dump_img_sector, 0, NULL, "", "", NULL },
    { "vt_load_cpio", ventoy_cmd_load_cpio, 0, NULL, "", "", NULL },

    { "vt_linux_parse_initrd_isolinux", ventoy_cmd_isolinux_initrd_collect, 0, NULL, "{cfgfile}", "", NULL },
    { "vt_linux_parse_initrd_grub", ventoy_cmd_grub_initrd_collect, 0, NULL, "{cfgfile}", "", NULL },
    { "vt_linux_specify_initrd_file", ventoy_cmd_specify_initrd_file, 0, NULL, "", "", NULL },
    { "vt_linux_clear_initrd", ventoy_cmd_clear_initrd_list, 0, NULL, "", "", NULL },
    { "vt_linux_dump_initrd", ventoy_cmd_dump_initrd_list, 0, NULL, "", "", NULL },
    { "vt_linux_initrd_count", ventoy_cmd_initrd_count, 0, NULL, "", "", NULL },
    { "vt_linux_valid_initrd_count", ventoy_cmd_valid_initrd_count, 0, NULL, "", "", NULL },
    { "vt_linux_locate_initrd", ventoy_cmd_linux_locate_initrd, 0, NULL, "", "", NULL },
    { "vt_linux_chain_data", ventoy_cmd_linux_chain_data, 0, NULL, "", "", NULL },

    { "vt_windows_reset",      ventoy_cmd_wimdows_reset, 0, NULL, "", "", NULL },
    { "vt_windows_locate_wim", ventoy_cmd_wimdows_locate_wim, 0, NULL, "", "", NULL },
    { "vt_windows_chain_data", ventoy_cmd_windows_chain_data, 0, NULL, "", "", NULL },

    { "vt_add_replace_file", ventoy_cmd_add_replace_file, 0, NULL, "", "", NULL },
    { "vt_relocator_chaindata", ventoy_cmd_relocator_chaindata, 0, NULL, "", "", NULL },

};

GRUB_MOD_INIT(ventoy)
{
    grub_uint32_t i;
    cmd_para *cur = NULL;

    ventoy_env_init ();

    for (i = 0; i < ARRAY_SIZE(ventoy_cmds); i++)
    {
        cur = ventoy_cmds + i;
        cur->cmd = grub_register_extcmd(cur->name, cur->func, cur->flags, 
                                        cur->summary, cur->description, cur->parser);
    }
    vt_compatible_init ();
}

GRUB_MOD_FINI(ventoy)
{
    grub_uint32_t i;

    for (i = 0; i < ARRAY_SIZE(ventoy_cmds); i++)
    {
        grub_unregister_extcmd(ventoy_cmds[i].cmd);
    }
    vt_compatible_fini ();
}

