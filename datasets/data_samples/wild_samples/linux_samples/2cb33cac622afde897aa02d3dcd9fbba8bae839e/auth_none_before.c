{
	struct ceph_auth_none_info *xi = ac->private;

	return xi->starting;
}

/*
 * the generic auth code decode the global_id, and we carry no actual
 * authenticate state, so nothing happens here.
 */
static int handle_reply(struct ceph_auth_client *ac, int result,
			void *buf, void *end)
{
	struct ceph_auth_none_info *xi = ac->private;

	xi->starting = false;
	return result;
}

/*
 * build an 'authorizer' with our entity_name and global_id.  we can
 * reuse a single static copy since it is identical for all services
 * we connect to.
 */
static int ceph_auth_none_create_authorizer(
	struct ceph_auth_client *ac, int peer_type,
	struct ceph_auth_handshake *auth)
{
	struct ceph_auth_none_info *ai = ac->private;
	struct ceph_none_authorizer *au = &ai->au;
	void *p, *end;
	int ret;

	if (!ai->built_authorizer) {
		p = au->buf;
		end = p + sizeof(au->buf);
		ceph_encode_8(&p, 1);
		ret = ceph_entity_name_encode(ac->name, &p, end - 8);
		if (ret < 0)
			goto bad;
		ceph_decode_need(&p, end, sizeof(u64), bad2);
		ceph_encode_64(&p, ac->global_id);
		au->buf_len = p - (void *)au->buf;
		ai->built_authorizer = true;
		dout("built authorizer len %d\n", au->buf_len);
	}
static const struct ceph_auth_client_ops ceph_auth_none_ops = {
	.name = "none",
	.reset = reset,
	.destroy = destroy,
	.is_authenticated = is_authenticated,
	.should_authenticate = should_authenticate,
	.handle_reply = handle_reply,
	.create_authorizer = ceph_auth_none_create_authorizer,
	.destroy_authorizer = ceph_auth_none_destroy_authorizer,
};

int ceph_auth_none_init(struct ceph_auth_client *ac)
{
	struct ceph_auth_none_info *xi;

	dout("ceph_auth_none_init %p\n", ac);
	xi = kzalloc(sizeof(*xi), GFP_NOFS);
	if (!xi)
		return -ENOMEM;

	xi->starting = true;
	xi->built_authorizer = false;

	ac->protocol = CEPH_AUTH_NONE;
	ac->private = xi;
	ac->ops = &ceph_auth_none_ops;
	return 0;
}