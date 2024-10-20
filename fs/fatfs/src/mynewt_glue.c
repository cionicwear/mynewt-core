/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "os/mynewt.h"
#include <hal/hal_flash.h>
#include <disk/disk.h>
#include <flash_map/flash_map.h>

#include <fatfs/ff.h>
#include <fatfs/diskio.h>

#include <fs/fs.h>
#include <fs/fs_if.h>

static int fatfs_open(const char *path, uint8_t access_flags,
  struct fs_file **out_file);
static int fatfs_close(struct fs_file *fs_file);
static int fatfs_read(struct fs_file *fs_file, uint32_t len, void *out_data,
  uint32_t *out_len);
static int fatfs_write(struct fs_file *fs_file, const void *data, int len);
static int fatfs_flush(struct fs_file *fs_file);
static int fatfs_seek(struct fs_file *fs_file, uint32_t offset);
static uint32_t fatfs_getpos(const struct fs_file *fs_file);
static int fatfs_file_len(const struct fs_file *fs_file, uint32_t *out_len);
static int fatfs_unlink(const char *path);
static int fatfs_rename(const char *from, const char *to);
static int fatfs_mkdir(const char *path);
static int fatfs_opendir(const char *path, struct fs_dir **out_fs_dir);
static int fatfs_readdir(struct fs_dir *dir, struct fs_dirent **out_dirent);
static int fatfs_closedir(struct fs_dir *dir);
static int fatfs_dirent_name(const struct fs_dirent *fs_dirent, size_t max_len,
  char *out_name, uint8_t *out_name_len);
static int fatfs_dirent_is_dir(const struct fs_dirent *fs_dirent);
static int fatfs_mkfs(const char *path, uint8_t format);
static int fatfs_mount(const char *disk_name);
static int fatfs_unmount(const char *disk_name);
static bool fatfs_is_mounted(const char *disk_name);

#define DRIVE_LEN 4

struct fatfs_dir {
    struct fs_ops *fops;
    int disk_number;
    FATFS_DIR *dir;
};

struct fatfs_dirent {
    struct fs_ops *fops;
    int disk_number;
    FILINFO filinfo;
};

/* NOTE: to ease memory management of dirent structs, this single static
 * variable holds the latest entry found by readdir. This limits FAT to
 * working on a single thread, single opendir -> closedir cycle.
 */
static struct fatfs_dirent dirent;

static struct fs_ops fatfs_ops = {
    .f_open = fatfs_open,
    .f_close = fatfs_close,
    .f_read = fatfs_read,
    .f_write = fatfs_write,
    .f_flush = fatfs_flush,

    .f_seek = fatfs_seek,
    .f_getpos = fatfs_getpos,
    .f_filelen = fatfs_file_len,

    .f_unlink = fatfs_unlink,
    .f_rename = fatfs_rename,
    .f_mkdir = fatfs_mkdir,
    .f_mkfs = fatfs_mkfs,
    .f_opendir = fatfs_opendir,
    .f_readdir = fatfs_readdir,
    .f_closedir = fatfs_closedir,

    .f_dirent_name = fatfs_dirent_name,
    .f_dirent_is_dir = fatfs_dirent_is_dir,

    .f_unmount = fatfs_unmount,
    .f_mount = fatfs_mount,
    .f_is_mounted = fatfs_is_mounted,

    .f_name = "fatfs"
};

int fatfs_to_vfs_error(FRESULT res)
{
    int rc = FS_EOS;

    switch (res) {
    case FR_OK:
        rc = FS_EOK;
        break;
    case FR_DISK_ERR:              /* (1) A hard error occurred in the low level disk I/O layer */
        rc = FS_EHW;
        break;
    case FR_INT_ERR:               /* (2) Assertion failed */
        rc = FS_EOS;
        break;
    case FR_NOT_READY:             /* (3) The physical drive cannot work */
        rc = FS_ECORRUPT;
        break;
    case FR_NO_FILE:               /* (4) Could not find the file */
        /* passthrough */
    case FR_NO_PATH:               /* (5) Could not find the path */
        rc = FS_ENOENT;
        break;
    case FR_INVALID_NAME:          /* (6) The path name format is invalid */
        rc = FS_EINVAL;
        break;
    case FR_DENIED:                /* (7) Access denied due to prohibited access or directory full */
        rc = FS_EACCESS;
        break;
    case FR_EXIST:                 /* (8) Access denied due to prohibited access */
        rc = FS_EEXIST;
        break;
    case FR_INVALID_OBJECT:        /* (9) The file/directory object is invalid */
        rc = FS_EINVAL;
        break;
    case FR_WRITE_PROTECTED:       /* (10) The physical drive is write protected */
        /* TODO: assign correct error */
        break;
    case FR_INVALID_DRIVE:         /* (11) The logical drive number is invalid */
        rc = FS_EHW;
        break;
    case FR_NOT_ENABLED:           /* (12) The volume has no work area */
        rc = FS_EUNEXP;
        break;
    case FR_NO_FILESYSTEM:         /* (13) There is no valid FAT volume */
        rc = FS_EUNINIT;
        break;
    case FR_MKFS_ABORTED:          /* (14) The f_mkfs() aborted due to any problem */
        /* TODO: assign correct error */
        break;
    case FR_TIMEOUT:               /* (15) Could not get a grant to access the volume within defined period */
        /* TODO: assign correct error */
        break;
    case FR_LOCKED:                /* (16) The operation is rejected according to the file sharing policy */
        /* TODO: assign correct error */
        break;
    case FR_NOT_ENOUGH_CORE:       /* (17) LFN working buffer could not be allocated */
        rc = FS_ENOMEM;
        break;
    case FR_TOO_MANY_OPEN_FILES:   /* (18) Number of open files > _FS_LOCK */
        /* TODO: assign correct error */
        break;
    case FR_INVALID_PARAMETER:     /* (19) Given parameter is invalid */
        rc = FS_EINVAL;
        break;
    }

    return rc;
}

struct mounted_disk {
    char *disk_name;
    int disk_number;
    FATFS *fs_instance;
    struct disk_ops *dops;
    bool mounted;
    SLIST_ENTRY(mounted_disk) sc_next;
};

static SLIST_HEAD(, mounted_disk) mounted_disks = SLIST_HEAD_INITIALIZER();

struct mounted_disk*
fatfs_get_disk_by_name(const char *disk_name, int *disk_nb)
{
    struct mounted_disk *sc;

    if(disk_nb){
        *disk_nb = 0;
    }
    
    SLIST_FOREACH(sc, &mounted_disks, sc_next) {
        if (strncmp(sc->disk_name, disk_name, strlen(sc->disk_name)) == 0) {
            return sc;
        }
         if(disk_nb){
           (*disk_nb)++;
        }
    }

    return NULL;
}

struct mounted_disk*
fatfs_get_disk_by_number(int nb)
{
    struct mounted_disk *sc;
    
    SLIST_FOREACH(sc, &mounted_disks, sc_next) {
        if(sc->disk_number == nb) {
            return sc;
        }
    }

    return NULL;
}

static int 
fatfs_unmount(const char *disk_name)
{
    struct mounted_disk *sc;
    char path[6];
    int rc;
    
    sc = fatfs_get_disk_by_name(disk_name, NULL);

    if(!sc->mounted){
        return FS_EOK;
    }

    sprintf(path, "%d:", (uint8_t)sc->disk_number);
    rc = f_mount(NULL, path, 0); // set FAT context to NULL unmount the disk
    sc->mounted = false;

    return rc;
}

static int 
fatfs_mount(const char *disk_name)
{
    struct mounted_disk *sc;
    int disk_number;
    char path[6];
    int rc;

    sc = fatfs_get_disk_by_name(disk_name, &disk_number);

    if(sc && sc->mounted){
        return FS_EOK;
    }

    if(sc == NULL){
        sc = malloc(sizeof(struct mounted_disk));
        sc->disk_name = strdup(disk_name);
        sc->disk_number = disk_number;
        sc->dops = disk_ops_for(disk_name);
        /* XXX: check for errors? */
        sc->fs_instance = malloc(sizeof(FATFS));
        SLIST_INSERT_HEAD(&mounted_disks, sc, sc_next);
    }

    sprintf(path, "%d:", (uint8_t)sc->disk_number);
    rc = f_mount(sc->fs_instance, path, 1);
    sc->mounted = true;

    return rc;
}

static bool 
_fatfs_is_mounted(struct mounted_disk *sc)
{
    if(sc && sc->mounted){
        return true;
    }

    return false;
} 

static bool 
fatfs_is_mounted(const char *disk_name)
{
    struct mounted_disk *sc;
    
    sc = fatfs_get_disk_by_name(disk_name, NULL);

    return _fatfs_is_mounted(sc);
}

/**
 * Converts fs path to fatfs path
 *
 * Function changes mmc0:/dir/file.ext to 0:/dir/file.ext
 *
 * @param fs_path
 * @return pointer to allocated memory with fatfs path
 */
static char *
fatfs_path_from_fs_path(const char *fs_path)
{
    struct mounted_disk *sc;
    int disk_number;
    char *file_path = NULL;
    char *fatfs_path = NULL;

    sc = fatfs_get_disk_by_name(fs_path, &disk_number);
    
    if(!sc){
        goto err;
    }

    disk_number = sc->disk_number;

    file_path = disk_filepath_from_path(fs_path);

    if (file_path == NULL) {
        goto err;
    }

    fatfs_path = malloc(strlen(file_path) + 3);
    if (fatfs_path == NULL) {
        goto err;
    }
    sprintf(fatfs_path, "%d:%s", disk_number, file_path);
err:
    free(file_path);
    return fatfs_path;
}

static int
fatfs_open(const char *path, uint8_t access_flags, struct fs_file **out_fs_file)
{
    FRESULT res;
    FIL *out_file = NULL;
    BYTE mode;
    struct fatfs_file *file = NULL;
    char *fatfs_path = NULL;
    int rc;
    struct mounted_disk *sc;

    fatfs_path = fatfs_path_from_fs_path(path);
    if (fatfs_path == NULL) {
        rc = FS_ENOMEM;
        goto out;
    }

    sc = fatfs_get_disk_by_name(path, NULL);

    if(!sc || !_fatfs_is_mounted(sc)){
        return FS_EUNINIT;
    }

    file = malloc(sizeof(struct fatfs_file));
    if (!file) {
        rc = FS_ENOMEM;
        goto out;
    }

    out_file = malloc(sizeof(FIL));
    if (!out_file) {
        rc = FS_ENOMEM;
        goto out;
    }

    mode = FA_OPEN_EXISTING;
    if (access_flags & FS_ACCESS_READ) {
        mode |= FA_READ;
    }
    if (access_flags & FS_ACCESS_WRITE) {
        mode |= FA_WRITE;
    }
    if (access_flags & FS_ACCESS_APPEND) {
        mode |= FA_OPEN_APPEND;
    }
    if (access_flags & FS_ACCESS_TRUNCATE) {
        mode &= ~FA_OPEN_EXISTING;
        mode |= FA_CREATE_ALWAYS;
    }

    res = f_open(out_file, fatfs_path, mode);
    if (res != FR_OK) {
        rc = fatfs_to_vfs_error(res);
        goto out;
    }

    file->file = out_file;
    file->fops = &fatfs_ops;
    file->disk_number = sc->disk_number;
    *out_fs_file = (struct fs_file *) file;
    rc = FS_EOK;

out:
    if(fatfs_path) free(fatfs_path);
    if (rc != FS_EOK) {
        if (file) free(file);
        if (out_file) free(out_file);
    }
    return rc;
}

static int
fatfs_close(struct fs_file *fs_file)
{
    FRESULT res = FR_OK;
    FIL *file = ((struct fatfs_file *) fs_file)->file;
    struct mounted_disk* sc;

    sc = fatfs_get_disk_by_number(((struct fatfs_file *) fs_file)->disk_number);

    if(!sc || !_fatfs_is_mounted(sc)){
        return FS_EUNINIT;
    }


    if (file != NULL) {
        res = f_close(file);
        free(file);
    }

    free(fs_file);
    return fatfs_to_vfs_error(res);
}

static int
fatfs_seek(struct fs_file *fs_file, uint32_t offset)
{
    FRESULT res;
    FIL *file = ((struct fatfs_file *) fs_file)->file;
    struct mounted_disk* sc;

    sc = fatfs_get_disk_by_number(((struct fatfs_file *) fs_file)->disk_number);

    if(!sc || !_fatfs_is_mounted(sc)){
        return FS_EUNINIT;
    }

    res = f_lseek(file, offset);
    return fatfs_to_vfs_error(res);
}

static uint32_t
fatfs_getpos(const struct fs_file *fs_file)
{
    uint32_t offset;
    FIL *file = ((struct fatfs_file *) fs_file)->file;
    struct mounted_disk* sc;

    sc = fatfs_get_disk_by_number(((struct fatfs_file *) fs_file)->disk_number);

    if(!sc || !_fatfs_is_mounted(sc)){
        return FS_EUNINIT;
    }

    offset = (uint32_t) f_tell(file);
    return offset;
}

static int
fatfs_file_len(const struct fs_file *fs_file, uint32_t *out_len)
{
    FIL *file = ((struct fatfs_file *) fs_file)->file;
    struct mounted_disk* sc;

    sc = fatfs_get_disk_by_number(((struct fatfs_file *) fs_file)->disk_number);

    if(!sc || !_fatfs_is_mounted(sc)){
        return FS_EUNINIT;
    }

    *out_len = (uint32_t) f_size(file);

    return FS_EOK;
}

static int
fatfs_read(struct fs_file *fs_file, uint32_t len, void *out_data,
           uint32_t *out_len)
{
    FRESULT res;
    FIL *file = ((struct fatfs_file *) fs_file)->file;
    UINT uint_len;
    struct mounted_disk* sc;

    sc = fatfs_get_disk_by_number(((struct fatfs_file *) fs_file)->disk_number);

    if(!sc || !_fatfs_is_mounted(sc)){
        return FS_EUNINIT;
    }

    res = f_read(file, out_data, len, &uint_len);
    *out_len = uint_len;
    return fatfs_to_vfs_error(res);
}

static int
fatfs_write(struct fs_file *fs_file, const void *data, int len)
{
    FRESULT res;
    UINT out_len;
    FIL *file = ((struct fatfs_file *) fs_file)->file;
    struct mounted_disk* sc;

    sc = fatfs_get_disk_by_number(((struct fatfs_file *) fs_file)->disk_number);

    if(!sc || !_fatfs_is_mounted(sc)){
        return FS_EUNINIT;
    }

    res = f_write(file, data, len, &out_len);
    if (len != out_len) {
        return FS_EFULL;
    }
    return fatfs_to_vfs_error(res);
}

static int
fatfs_flush(struct fs_file *fs_file)
{
    FRESULT res;
    FIL *file = ((struct fatfs_file *)fs_file)->file;
    struct mounted_disk* sc;

    sc = fatfs_get_disk_by_number(((struct fatfs_file *) fs_file)->disk_number);

    if(!sc || !_fatfs_is_mounted(sc)){
        return FS_EUNINIT;
    }

    res = f_sync(file);

    return fatfs_to_vfs_error(res);
}

static int
fatfs_unlink(const char *path)
{
    FRESULT res;
    char *fatfs_path;
    struct mounted_disk* sc;
    
    fatfs_path = fatfs_path_from_fs_path(path);

    if (fatfs_path == NULL) {
        return FS_ENOMEM;
    }

    sc = fatfs_get_disk_by_name(path, NULL);

    if(!sc || !_fatfs_is_mounted(sc)){
        free(fatfs_path);
        return FS_EUNINIT;
    }
    
    res = f_unlink(fatfs_path);
    free(fatfs_path);

    return fatfs_to_vfs_error(res);
}

static int
fatfs_rename(const char *from, const char *to)
{
    FRESULT res;
    char *fatfs_src_path;
    char *fatfs_dst_path;
    struct mounted_disk* sc;
    
    sc = fatfs_get_disk_by_name(from, NULL);

    if(!sc || !_fatfs_is_mounted(sc)){
        return FS_EUNINIT;
    }

    fatfs_src_path = fatfs_path_from_fs_path(from);
    fatfs_dst_path = fatfs_path_from_fs_path(to);

    if (fatfs_src_path == NULL || fatfs_dst_path == NULL) {
        free(fatfs_src_path);
        free(fatfs_dst_path);
        return FS_ENOMEM;
    }

    res = f_rename(fatfs_src_path, fatfs_dst_path);
    free(fatfs_src_path);
    free(fatfs_dst_path);

    return fatfs_to_vfs_error(res);
}

static int
fatfs_mkdir(const char *path)
{
    FRESULT res;
    char *fatfs_path;
    struct mounted_disk* sc;
    
    fatfs_path = fatfs_path_from_fs_path(path);

    if (fatfs_path == NULL) {
        return FS_ENOMEM;
    }

    sc = fatfs_get_disk_by_name(path, NULL);

    if(!sc || !_fatfs_is_mounted(sc)){
        free(fatfs_path);
        return FS_EUNINIT;
    }

    res = f_mkdir(fatfs_path);
    free(fatfs_path);

    return fatfs_to_vfs_error(res);
}

static int
fatfs_mkfs(const char *disk, uint8_t format)
{
    FRESULT res;
    char disk_cpy[5] = {0};
    strncpy(disk_cpy, disk, strlen(disk)-1);
    uint8_t work_buffer[512];
    struct mounted_disk* sc;
    
    sc = fatfs_get_disk_by_name(disk, NULL);

    if(!sc || !_fatfs_is_mounted(sc)){
        return FS_EUNINIT;
    }

    res = f_mkfs(disk_cpy, FM_FAT32, 0,work_buffer, 512);
    return res;
}

static int
fatfs_opendir(const char *path, struct fs_dir **out_fs_dir)
{
    FRESULT res;
    FATFS_DIR *out_dir = NULL;
    struct fatfs_dir *dir = NULL;
    char *fatfs_path = NULL;
    int rc;
    struct mounted_disk* sc;
    
    fatfs_path = fatfs_path_from_fs_path(path);

    sc = fatfs_get_disk_by_name(path, NULL);

    if(!sc || !_fatfs_is_mounted(sc)){
        free(fatfs_path);
        return FS_EUNINIT;
    }

    dir = malloc(sizeof(struct fatfs_dir));
    if (!dir) {
        rc = FS_ENOMEM;
        goto out;
    }

    out_dir = malloc(sizeof(FATFS_DIR));
    if (!out_dir) {
        rc = FS_ENOMEM;
        goto out;
    }

    res = f_opendir(out_dir, fatfs_path);
    if (res != FR_OK) {
        rc = fatfs_to_vfs_error(res);
        goto out;
    }

    dir->dir = out_dir;
    dir->fops = &fatfs_ops;
    dir->disk_number = sc->disk_number;
    *out_fs_dir = (struct fs_dir *)dir;
    rc = FS_EOK;

out:
    free(fatfs_path);
    if (rc != FS_EOK) {
        if (dir) free(dir);
        if (out_dir) free(out_dir);
    }
    return rc;
}

static int
fatfs_readdir(struct fs_dir *fs_dir, struct fs_dirent **out_fs_dirent)
{
    FRESULT res;
    FATFS_DIR *dir = ((struct fatfs_dir *) fs_dir)->dir;
    struct mounted_disk* sc;
    
    sc = fatfs_get_disk_by_number(((struct fatfs_dir *) fs_dir)->disk_number);

    if(!sc || !_fatfs_is_mounted(sc)){
        return FS_EUNINIT;
    }

    dirent.fops = &fatfs_ops;
    res = f_readdir(dir, &dirent.filinfo);
    if (res != FR_OK) {
        return fatfs_to_vfs_error(res);
    }

    *out_fs_dirent = (struct fs_dirent *) &dirent;
    if (!dirent.filinfo.fname[0]) {
        return FS_ENOENT;
    }
    return FS_EOK;
}

static int
fatfs_closedir(struct fs_dir *fs_dir)
{
    FRESULT res;
    FATFS_DIR *dir = ((struct fatfs_dir *) fs_dir)->dir;
    struct mounted_disk* sc;
    
    sc = fatfs_get_disk_by_number(((struct fatfs_dir *) fs_dir)->disk_number);

    if(!sc || !_fatfs_is_mounted(sc)){
        return FS_EUNINIT;
    }

    res = f_closedir(dir);
    free(dir);
    free(fs_dir);

    return fatfs_to_vfs_error(res);
}

static int
fatfs_dirent_name(const struct fs_dirent *fs_dirent, size_t max_len,
                  char *out_name, uint8_t *out_name_len)
{

    FILINFO *filinfo;
    size_t out_len;

    assert(fs_dirent != NULL);
    filinfo = &(((struct fatfs_dirent *)fs_dirent)->filinfo);
    out_len = max_len < sizeof(filinfo->fname) ? max_len : sizeof(filinfo->fname);
    memcpy(out_name, filinfo->fname, out_len);
    *out_name_len = out_len;
    return FS_EOK;
}

static int
fatfs_dirent_is_dir(const struct fs_dirent *fs_dirent)
{
    FILINFO *filinfo;

    assert(fs_dirent != NULL);
    filinfo = &(((struct fatfs_dirent *)fs_dirent)->filinfo);
    return filinfo->fattrib & AM_DIR;
}

DSTATUS
disk_initialize(BYTE pdrv)
{
    /* Don't need to do anything while using hal_flash */
    return RES_OK;
}

DSTATUS
disk_status(BYTE pdrv)
{
    /* Always OK on native emulated flash */
    return RES_OK;
}

static struct disk_ops *dops_from_handle(BYTE pdrv)
{
    struct mounted_disk *sc;

    SLIST_FOREACH(sc, &mounted_disks, sc_next) {
        if (sc->disk_number == pdrv) {
            return sc->dops;
        }
    }

    return NULL;
}

DRESULT
disk_read(BYTE pdrv, BYTE* buff, DWORD sector, UINT count)
{
    int rc;
    uint32_t num_bytes;
    struct disk_ops *dops;

    num_bytes = (uint32_t) count * 512;
#if !MYNEWT_VAL(FATFS_HANDLE_BLOCK)
    /* NOTE: safe to assume sector size as 512 for now, see ffconf.h */
    sector = (uint32_t) sector * 512;
#endif

    dops = dops_from_handle(pdrv);
    if (dops == NULL) {
        return STA_NOINIT;
    }

    rc = dops->read(pdrv, sector, (void *) buff, num_bytes);
    if (rc < 0) {
        return STA_NOINIT;
    }

    return RES_OK;
}

DRESULT
disk_write(BYTE pdrv, const BYTE* buff, DWORD sector, UINT count)
{
    int rc;
    uint32_t num_bytes;
    struct disk_ops *dops;

#if !MYNEWT_VAL(FATFS_HANDLE_BLOCK)
    /* NOTE: safe to assume sector size as 512 for now, see ffconf.h */
    sector = (uint32_t) sector * 512;
#endif

    num_bytes = (uint32_t) count * 512;

    dops = dops_from_handle(pdrv);
    if (dops == NULL) {
        return STA_NOINIT;
    }

    rc = dops->write(pdrv, sector, (const void *) buff, num_bytes);
    if (rc < 0) {
        return STA_NOINIT;
    }

    return RES_OK;
}

DRESULT
disk_ioctl(BYTE pdrv, BYTE cmd, void* buff)
{
    int rc;
    struct disk_ops *dops;

    dops = dops_from_handle(pdrv);
    if (dops == NULL) {
        return STA_NOINIT;
    }

    rc = dops->ioctl(pdrv, cmd, buff);
    
    if (rc < 0) {
        return STA_NOINIT;
    }

    return RES_OK;
}

void
fatfs_pkg_init(void)
{
    /* Ensure this function only gets called by sysinit. */
    SYSINIT_ASSERT_ACTIVE();

    fs_register(&fatfs_ops);
}
