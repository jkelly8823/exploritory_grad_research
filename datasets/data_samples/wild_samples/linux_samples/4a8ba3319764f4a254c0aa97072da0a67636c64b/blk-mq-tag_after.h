struct blk_mq_tags {
	unsigned int nr_tags;
	unsigned int nr_reserved_tags;

	atomic_t active_queues;

	struct blk_mq_bitmap_tags bitmap_tags;
	struct blk_mq_bitmap_tags breserved_tags;

	struct request **rqs;
	struct list_head page_list;
};


extern struct blk_mq_tags *blk_mq_init_tags(unsigned int nr_tags, unsigned int reserved_tags, int node);
extern void blk_mq_free_tags(struct blk_mq_tags *tags);

extern unsigned int blk_mq_get_tag(struct blk_mq_alloc_data *data);
extern void blk_mq_put_tag(struct blk_mq_hw_ctx *hctx, unsigned int tag, unsigned int *last_tag);
extern bool blk_mq_has_free_tags(struct blk_mq_tags *tags);
extern ssize_t blk_mq_tag_sysfs_show(struct blk_mq_tags *tags, char *page);
extern void blk_mq_tag_init_last_tag(struct blk_mq_tags *tags, unsigned int *last_tag);
extern int blk_mq_tag_update_depth(struct blk_mq_tags *tags, unsigned int depth);
extern void blk_mq_tag_wakeup_all(struct blk_mq_tags *tags, bool);

enum {
	BLK_MQ_TAG_CACHE_MIN	= 1,
	BLK_MQ_TAG_CACHE_MAX	= 64,
};

enum {
	BLK_MQ_TAG_FAIL		= -1U,
	BLK_MQ_TAG_MIN		= BLK_MQ_TAG_CACHE_MIN,
	BLK_MQ_TAG_MAX		= BLK_MQ_TAG_FAIL - 1,
};

extern bool __blk_mq_tag_busy(struct blk_mq_hw_ctx *);
extern void __blk_mq_tag_idle(struct blk_mq_hw_ctx *);

static inline bool blk_mq_tag_busy(struct blk_mq_hw_ctx *hctx)
{
	if (!(hctx->flags & BLK_MQ_F_TAG_SHARED))
		return false;

	return __blk_mq_tag_busy(hctx);
}

static inline void blk_mq_tag_idle(struct blk_mq_hw_ctx *hctx)
{
	if (!(hctx->flags & BLK_MQ_F_TAG_SHARED))
		return;

	__blk_mq_tag_idle(hctx);
}

#endif