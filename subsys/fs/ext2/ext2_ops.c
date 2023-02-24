/*
 * Copyright (c) 2023 Antmicro <www.antmicro.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <zephyr/init.h>
#include <zephyr/fs/fs.h>
#include <zephyr/fs/fs_sys.h>
#include <zephyr/fs/ext2.h>
#include <zephyr/logging/log.h>

#include "../fs_impl.h"
#include "ext2.h"
#include "ext2_impl.h"
#include "ext2_struct.h"

LOG_MODULE_DECLARE(ext2);

/* File operations */

static int ext2_open(struct fs_file_t *filp, const char *fs_path, fs_mode_t flags)
{
	int rc, ret = 0;
	struct ext2_data *fs = filp->mp->fs_data;

	if (fs->open_files >= CONFIG_MAX_FILES) {
		LOG_DBG("Too many open files");
		return -EMFILE;
	}

	LOG_DBG("Open mode: Rd:%d Wr:%d App:%d Creat:%d",
			(flags & FS_O_READ) != 0,
			(flags & FS_O_WRITE) != 0,
			(flags & FS_O_APPEND) != 0,
			(flags & FS_O_CREATE) != 0);

	const char *path = fs_impl_strip_prefix(fs_path, filp->mp);
	struct ext2_lookup_args args = {
		.path = path,
		.inode = NULL,
		.flags = LOOKUP_ARG_OPEN,
	};

	if (flags & FS_O_CREATE) {
		args.flags |= LOOKUP_ARG_CREATE;
		args.parent = NULL;
	}

	rc = ext2_lookup_inode(fs, &args);
	if (rc < 0) {
		return rc;
	}

	/* Inodes allocated by lookup. Must be freed in manually. */
	struct ext2_inode *found_inode = args.inode;

	/* Not NULL iff FS_O_CREATE and found_inode == NULL */
	struct ext2_inode *parent = args.parent;

	/* File has to be created */
	if (flags & FS_O_CREATE && found_inode == NULL) {
		LOG_DBG("Returned from lookup & create: '%s':%d creating file: %d",
				path + args.name_pos, args.name_len, found_inode == NULL);

		struct ext2_inode *new_inode;

		rc = ext2_inode_get(fs, 0, &new_inode);
		if (rc < 0) {
			ret = rc;
			goto out;
		}

		rc = ext2_create_file(parent, new_inode, &args);
		if (rc < 0) {
			ext2_inode_drop(new_inode);
			ret = rc;
			goto out;
		}

		found_inode = new_inode;
	}

	if ((found_inode->i_mode & EXT2_S_IFMT) != EXT2_S_IFREG) {
		ret = -EINVAL;
		goto out;
	}

	struct ext2_file *f = ext2_heap_alloc(sizeof(struct ext2_file));

	if (!f) {
		ret = -ENOMEM;
		goto out;
	}

	f->f_inode = found_inode;
	f->f_off = 0;
	f->f_flags = flags & (FS_O_RDWR | FS_O_APPEND);

	filp->filep = f;

	ext2_inode_drop(parent);
	return 0;

out:
	ext2_inode_drop(found_inode);
	ext2_inode_drop(parent);
	return ret;
}

static int ext2_close(struct fs_file_t *filp)
{
	int rc;
	struct ext2_file *f = filp->filep;

	rc = ext2_inode_drop(f->f_inode);
	if (rc < 0) {
		goto out;
	}

	ext2_heap_free(f);
	filp->filep = NULL;
out:
	return rc;
}

static ssize_t ext2_read(struct fs_file_t *filp, void *dest, size_t nbytes)
{
	struct ext2_file *f = filp->filep;

	if ((f->f_flags & FS_O_READ) == 0) {
		return -EACCES;
	}

	ssize_t r = ext2_inode_read(f->f_inode, dest, f->f_off, nbytes);

	if (r < 0) {
		return r;
	}
	f->f_off += r;
	return r;
}

static ssize_t ext2_write(struct fs_file_t *filp, const void *src, size_t nbytes)
{
	struct ext2_file *f = filp->filep;

	if ((f->f_flags & FS_O_WRITE) == 0) {
		return -EACCES;
	}

	if (f->f_flags & FS_O_APPEND) {
		f->f_off = f->f_inode->i_size;
	}

	ssize_t r = ext2_inode_write(f->f_inode, src, f->f_off, nbytes);

	if (r < 0) {
		return r;
	}

	f->f_off += r;
	return r;
}

static int ext2_lseek(struct fs_file_t *filp, off_t off, int whence)
{
	struct ext2_file *f = filp->filep;

	uint32_t new_off = 0;

	switch (whence) {
	case FS_SEEK_SET:
		new_off = off;
		break;

	case FS_SEEK_CUR:
		new_off = f->f_off + off;
		break;

	case FS_SEEK_END:
		new_off = f->f_inode->i_size + off;
		break;

	default:
		return -EINVAL;
	}

	/* New offset not inside the file. */
	if (new_off < 0 || new_off > f->f_inode->i_size) {
		return -EINVAL;
	}
	f->f_off = new_off;
	return 0;
}

static off_t ext2_tell(struct fs_file_t *filp)
{
	struct ext2_file *f = filp->filep;

	return f->f_off;
}

static int ext2_truncate(struct fs_file_t *filp, off_t length)
{
	struct ext2_file *f = filp->filep;

	if ((f->f_flags & FS_O_WRITE) == 0) {
		return -EACCES;
	}

	int rc = ext2_inode_trunc(f->f_inode, length);

	if (rc < 0) {
		return rc;
	}
	return 0;
}

/* Directory operations */

static int ext2_mkdir(struct fs_mount_t *mountp, const char *name)
{
	int rc, ret = 0;
	struct ext2_data *fs = mountp->fs_data;

	const char *path = fs_impl_strip_prefix(name, mountp);
	struct ext2_lookup_args args = {
		args.path = path,
		args.inode = NULL,
		args.parent = NULL,
	};

	args.flags = LOOKUP_ARG_CREATE;

	rc = ext2_lookup_inode(fs, &args);
	if (rc < 0) {
		return rc;
	}

	struct ext2_inode *found_inode = args.inode;
	struct ext2_inode *parent = args.parent;

	LOG_DBG("Returned from lookup & create: '%s':%d res: %d",
			path + args.name_pos, args.name_len, found_inode == NULL);

	if (found_inode != NULL) {
		ret = -EEXIST;
		goto out;
	}

	rc = ext2_inode_get(fs, 0, &found_inode);
	if (rc < 0) {
		ret = rc;
		goto out;
	}

	rc = ext2_create_dir(parent, found_inode, &args);
	if (rc < 0) {
		ret = rc;
	}

out:
	ext2_inode_drop(parent);
	ext2_inode_drop(found_inode);
	return ret;
}

/* File system level operations */

#ifdef CONFIG_FILE_SYSTEM_MKFS
FS_EXT2_DECLARE_DEFAULT_CONFIG(ext2_default_cfg);
#endif

static int ext2_mount(struct fs_mount_t *mountp)
{
	int ret = 0;
	struct ext2_data *fs = NULL;
	struct ext2_disk_superblock *sb;
#ifdef CONFIG_FILE_SYSTEM_MKFS
	bool do_format = false;
	bool possible_format = (mountp->flags & FS_MOUNT_FLAG_NO_FORMAT) == 0 &&
				(mountp->flags & FS_MOUNT_FLAG_READ_ONLY) == 0;
#endif

	/* Allocate superblock structure for temporary use */
	sb = ext2_heap_alloc(sizeof(struct ext2_disk_superblock));
	if (sb == NULL) {
		ret = -ENOMEM;
		goto err;
	}

	ret = ext2_init_storage(&fs, mountp->storage_dev, mountp->flags);
	if (ret < 0) {
		goto err;
	}

	fs->flags = 0;
	if (mountp->flags & FS_MOUNT_FLAG_READ_ONLY) {
		fs->flags |= EXT2_DATA_FLAGS_RO;
	}

	ret = fs->backend_ops->read_superblock(fs, sb);
	if (ret < 0) {
		goto err;
	}

	ret = ext2_verify_superblock(sb);
	if (ret == 0) {
		fs->block_size = 1024 << sb->s_log_block_size;

	} else if (ret == -EROFS) {
		fs->block_size = 1024 << sb->s_log_block_size;
		fs->flags |= EXT2_DATA_FLAGS_RO;

#ifdef CONFIG_FILE_SYSTEM_MKFS
	} else if (ret == -EINVAL && possible_format) {
		do_format = true;
		fs->block_size = ext2_default_cfg.block_size;
#endif

	} else {
		goto err;
	}

	if (fs->block_size % fs->write_size != 0) {
		LOG_ERR("Blocks size isn't multiple of sector size. (bsz: %d, ssz: %d)",
				fs->block_size, fs->write_size);
		ret = -ENOTSUP;
		goto err;
	}

	/* Temporary superblock won't be used anymore */
	ext2_heap_free(sb);

	ext2_init_blocks_slab(fs);

#ifdef CONFIG_FILE_SYSTEM_MKFS
	if (do_format) {
		LOG_INF("Formatting the storage device");

		ret = ext2_format(fs, &ext2_default_cfg);
		if (ret < 0) {
			goto err;
		}
		/* We don't need to verify superblock here again. Format has succeeded hence
		 * superblock must be valid.
		 */
	}
#endif

	ret = ext2_init_fs(fs);
	if (ret < 0) {
		goto err;
	}

	mountp->fs_data = fs;
	return 0;

err:
	ext2_heap_free(sb);
	ext2_close_struct(fs);
	return ret;
}

#if defined(CONFIG_FILE_SYSTEM_MKFS)

static int ext2_mkfs(uintptr_t dev_id, void *vcfg, int flags)
{
	int ret = 0;
	struct ext2_data *fs;
	struct ext2_cfg *cfg = vcfg;

	if (cfg == NULL) {
		cfg = &ext2_default_cfg;
	}

	ret = ext2_init_storage(&fs, (const void *)dev_id, flags);
	if (ret < 0) {
		LOG_ERR("Initialization of %ld device failed (%d)", dev_id, ret);
		goto out;
	}

	fs->block_size = cfg->block_size;

	ext2_init_blocks_slab(fs);

	LOG_INF("Formatting the storage device");
	ret = ext2_format(fs, cfg);
	if (ret < 0) {
		LOG_ERR("Format of %ld device failed (%d)", dev_id, ret);
	}

out:
	ext2_close_struct(fs);
	return ret;
}

#endif /* CONFIG_FILE_SYSTEM_MKFS */

static int ext2_unmount(struct fs_mount_t *mountp)
{
	int ret;
	struct ext2_data *fs = mountp->fs_data;

	ret = ext2_close_fs(fs);
	if (ret < 0) {
		return ret;
	}

	ret = ext2_close_struct(fs);
	if (ret < 0) {
		return ret;
	}
	mountp->fs_data = NULL;
	return 0;
}

static int ext2_stat(struct fs_mount_t *mountp, const char *path, struct fs_dirent *entry)
{
	int rc;
	struct ext2_data *fs = mountp->fs_data;

	if (!fs) {
		return -EINVAL;
	}

	path = fs_impl_strip_prefix(path, mountp);

	struct ext2_lookup_args args = {
		.path = path,
		.parent = NULL,
		.flags = LOOKUP_ARG_STAT,
	};

	rc = ext2_lookup_inode(fs, &args);
	if (rc < 0) {
		return rc;
	}

	uint32_t offset = args.offset;
	struct ext2_inode *parent = args.parent;
	struct ext2_dir dir = {.d_inode = parent, .d_off = offset};

	rc = ext2_get_direntry(&dir, entry);

	ext2_inode_drop(parent);
	ext2_inode_drop(args.inode);
	return rc;
}

static int ext2_statvfs(struct fs_mount_t *mountp, const char *path, struct fs_statvfs *stat)
{
	ARG_UNUSED(path);
	struct ext2_data *fs = mountp->fs_data;

	stat->f_bsize = fs->block_size;
	stat->f_frsize = fs->block_size;
	stat->f_blocks = EXT2_DATA_SBLOCK(fs)->s_blocks_count;
	stat->f_bfree = EXT2_DATA_SBLOCK(fs)->s_free_blocks_count;

	return 0;
}

/* File system interface */

static const struct fs_file_system_t ext2_fs = {
	.open = ext2_open,
	.close = ext2_close,
	.read = ext2_read,
	.write = ext2_write,
	.lseek = ext2_lseek,
	.tell = ext2_tell,
	.truncate = ext2_truncate,
	.mkdir = ext2_mkdir,
	.mount = ext2_mount,
	.unmount = ext2_unmount,
	.stat = ext2_stat,
	.statvfs = ext2_statvfs,
#if defined(CONFIG_FILE_SYSTEM_MKFS)
	.mkfs = ext2_mkfs,
#endif
};

static int ext2_init(void)
{
	int rc = fs_register(FS_EXT2, &ext2_fs);

	if (rc < 0) {
		LOG_WRN("Ext2 register error (%d)\n", rc);
	} else {
		LOG_DBG("Ext2 fs registered\n");
	}

	return rc;
}

SYS_INIT(ext2_init, POST_KERNEL, 99);
