	ns->ops->put(ns);
}

static int __ns_get_path(struct path *path, struct ns_common *ns)
{
	struct vfsmount *mnt = nsfs_mnt;
	struct dentry *dentry;
	struct inode *inode;
got_it:
	path->mnt = mntget(mnt);
	path->dentry = dentry;
	return 0;
slow:
	rcu_read_unlock();
	inode = new_inode_pseudo(mnt->mnt_sb);
	if (!inode) {
		ns->ops->put(ns);
		return -ENOMEM;
	}
	inode->i_ino = ns->inum;
	inode->i_mtime = inode->i_atime = inode->i_ctime = current_time(inode);
	inode->i_flags |= S_IMMUTABLE;
	dentry = d_alloc_anon(mnt->mnt_sb);
	if (!dentry) {
		iput(inode);
		return -ENOMEM;
	}
	d_instantiate(dentry, inode);
	dentry->d_fsdata = (void *)ns->ops;
	d = atomic_long_cmpxchg(&ns->stashed, 0, (unsigned long)dentry);
		d_delete(dentry);	/* make sure ->d_prune() does nothing */
		dput(dentry);
		cpu_relax();
		return -EAGAIN;
	}
	goto got_it;
}

int ns_get_path_cb(struct path *path, ns_get_path_helper_t *ns_get_cb,
		     void *private_data)
{
	int ret;

	do {
		struct ns_common *ns = ns_get_cb(private_data);
		if (!ns)
			return -ENOENT;
		ret = __ns_get_path(path, ns);
	} while (ret == -EAGAIN);

	return ret;
}

	return args->ns_ops->get(args->task);
}

int ns_get_path(struct path *path, struct task_struct *task,
		  const struct proc_ns_operations *ns_ops)
{
	struct ns_get_path_task_args args = {
		.ns_ops	= ns_ops,
{
	struct path path = {};
	struct file *f;
	int err;
	int fd;

	fd = get_unused_fd_flags(O_CLOEXEC);
	if (fd < 0)
		}

		err = __ns_get_path(&path, relative);
	} while (err == -EAGAIN);

	if (err) {
		put_unused_fd(fd);
		return err;
	}

	f = dentry_open(&path, O_RDONLY, current_cred());
	path_put(&path);