{
	struct sock *s = unix_get_socket(fp);

	if (s) {
		struct unix_sock *u = unix_sk(s);

		spin_lock(&unix_gc_lock);

		if (atomic_long_inc_return(&u->inflight) == 1) {
			BUG_ON(!list_empty(&u->link));
			list_add_tail(&u->link, &gc_inflight_list);
		} else {
			BUG_ON(list_empty(&u->link));
		}
		unix_tot_inflight++;
		spin_unlock(&unix_gc_lock);
	}
}