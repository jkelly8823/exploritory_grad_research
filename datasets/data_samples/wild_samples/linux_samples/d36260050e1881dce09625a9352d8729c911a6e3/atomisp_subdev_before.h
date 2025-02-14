struct atomisp_in_fmt_conv {
	u32     code;
	uint8_t bpp; /* bits per pixel */
	uint8_t depth; /* uncompressed */
	enum atomisp_css_stream_format atomisp_in_fmt;
	enum atomisp_css_bayer_order bayer_order;
	enum ia_css_stream_format css_stream_fmt;
};

struct atomisp_sub_device;

struct atomisp_video_pipe {
	struct video_device vdev;
	enum v4l2_buf_type type;
	struct media_pad pad;
	struct videobuf_queue capq;
	struct videobuf_queue outq;
	struct list_head activeq;
	struct list_head activeq_out;
	/*
	 * the buffers waiting for per-frame parameters, this is only valid
	 * in per-frame setting mode.
	 */
	struct list_head buffers_waiting_for_param;
	/* the link list to store per_frame parameters */
	struct list_head per_frame_params;

	unsigned int buffers_in_css;

	/* irq_lock is used to protect video buffer state change operations and
	 * also to make activeq, activeq_out, capq and outq list
	 * operations atomic. */
	spinlock_t irq_lock;
	unsigned int users;

	struct atomisp_device *isp;
	struct v4l2_pix_format pix;
	uint32_t sh_fmt;

	struct atomisp_sub_device *asd;

	/*
	 * This frame_config_id is got from CSS when dequueues buffers from CSS,
	 * it is used to indicate which parameter it has applied.
	 */
	unsigned int frame_config_id[VIDEO_MAX_FRAME];
	/*
	 * This config id is set when camera HAL enqueues buffer, it has a
	 * non-zero value to indicate which parameter it needs to applu
	 */
	unsigned int frame_request_config_id[VIDEO_MAX_FRAME];
	struct atomisp_css_params_with_list *frame_params[VIDEO_MAX_FRAME];
#ifdef ISP2401

	/*
	* move wdt from asd struct to create wdt for each pipe
	*/
	struct timer_list wdt;
	unsigned int wdt_duration;	/* in jiffies */
	unsigned long wdt_expires;
	atomic_t wdt_count;
#endif
};

struct atomisp_acc_pipe {
	struct video_device vdev;
	unsigned int users;
	bool running;
	struct atomisp_sub_device *asd;
	struct atomisp_device *isp;
};

struct atomisp_pad_format {
	struct v4l2_mbus_framefmt fmt;
	struct v4l2_rect crop;
	struct v4l2_rect compose;
};

/* Internal states for flash process */
enum atomisp_flash_state {
	ATOMISP_FLASH_IDLE,
	ATOMISP_FLASH_REQUESTED,
	ATOMISP_FLASH_ONGOING,
	ATOMISP_FLASH_DONE
};

/*
 * This structure is used to cache the CSS parameters, it aligns to
 * struct ia_css_isp_config but without un-supported and deprecated parts.
 */
struct atomisp_css_params {
	struct ia_css_wb_config   wb_config;
	struct ia_css_cc_config   cc_config;
	struct ia_css_tnr_config  tnr_config;
	struct ia_css_ecd_config  ecd_config;
	struct ia_css_ynr_config  ynr_config;
	struct ia_css_fc_config   fc_config;
	struct ia_css_formats_config formats_config;
	struct ia_css_cnr_config  cnr_config;
	struct ia_css_macc_config macc_config;
	struct ia_css_ctc_config  ctc_config;
	struct ia_css_aa_config   aa_config;
	struct ia_css_aa_config   baa_config;
	struct ia_css_ce_config   ce_config;
	struct ia_css_ob_config   ob_config;
	struct ia_css_dp_config   dp_config;
	struct ia_css_de_config   de_config;
	struct ia_css_gc_config   gc_config;
	struct ia_css_nr_config   nr_config;
	struct ia_css_ee_config   ee_config;
	struct ia_css_anr_config  anr_config;
	struct ia_css_3a_config   s3a_config;
	struct ia_css_xnr_config  xnr_config;
	struct ia_css_dz_config   dz_config;
	struct ia_css_cc_config yuv2rgb_cc_config;
	struct ia_css_cc_config rgb2yuv_cc_config;
	struct ia_css_macc_table  macc_table;
	struct ia_css_gamma_table gamma_table;
	struct ia_css_ctc_table   ctc_table;

	struct ia_css_xnr_table   xnr_table;
	struct ia_css_rgb_gamma_table r_gamma_table;
	struct ia_css_rgb_gamma_table g_gamma_table;
	struct ia_css_rgb_gamma_table b_gamma_table;

	struct ia_css_vector      motion_vector;
	struct ia_css_anr_thres   anr_thres;

	struct ia_css_dvs_6axis_config *dvs_6axis;
	struct ia_css_dvs2_coefficients *dvs2_coeff;
	struct ia_css_shading_table *shading_table;
	struct ia_css_morph_table   *morph_table;

	/*
	 * Used to store the user pointer address of the frame. driver needs to
	 * translate to ia_css_frame * and then set to CSS.
	 */
	void		*output_frame;
	uint32_t	isp_config_id;

	/* Indicates which parameters need to be updated. */
	struct atomisp_parameters update_flag;
};

struct atomisp_subdev_params {
	/* FIXME: Determines whether raw capture buffer are being passed to
	 * user space. Unimplemented for now. */
	int online_process;
	int yuv_ds_en;
	unsigned int color_effect;
	bool gdc_cac_en;
	bool macc_en;
	bool bad_pixel_en;
	bool video_dis_en;
	bool sc_en;
	bool fpn_en;
	bool xnr_en;
	bool low_light;
	int false_color;
	unsigned int histogram_elenum;

	/* Current grid info */
	struct atomisp_css_grid_info curr_grid_info;
	enum atomisp_css_pipe_id s3a_enabled_pipe;

	int s3a_output_bytes;

	bool dis_proj_data_valid;

	struct ia_css_dz_config   dz_config;  /** Digital Zoom */
	struct ia_css_capture_config   capture_config;

	struct atomisp_css_isp_config config;

	/* current configurations */
	struct atomisp_css_params css_param;

	/*
	 * Intermediate buffers used to communicate data between
	 * CSS and user space.
	 */
	struct ia_css_3a_statistics *s3a_user_stat;

	void *metadata_user[ATOMISP_METADATA_TYPE_NUM];
	uint32_t metadata_width_size;

	struct ia_css_dvs2_statistics *dvs_stat;
	struct atomisp_css_dvs_6axis *dvs_6axis;
	uint32_t exp_id;
	int  dvs_hor_coef_bytes;
	int  dvs_ver_coef_bytes;
	int  dvs_ver_proj_bytes;
	int  dvs_hor_proj_bytes;

	/* Flash */
	int num_flash_frames;
	enum atomisp_flash_state flash_state;
	enum atomisp_frame_status last_frame_status;

	/* continuous capture */
	struct atomisp_cont_capture_conf offline_parm;
	/* Flag to check if driver needs to update params to css */
	bool css_update_params_needed;
};

struct atomisp_css_params_with_list {
	/* parameters for CSS */
	struct atomisp_css_params params;
	struct list_head list;
};

struct atomisp_acc_fw {
	struct atomisp_css_fw_info *fw;
	unsigned int handle;
	unsigned int flags;
	unsigned int type;
	struct {
		size_t length;
		unsigned long css_ptr;
	} args[ATOMISP_ACC_NR_MEMORY];
	struct list_head list;
};

struct atomisp_map {
	ia_css_ptr ptr;
	size_t length;
	struct list_head list;
	/* FIXME: should keep book which maps are currently used
	 * by binaries and not allow releasing those
	 * which are in use. Implement by reference counting.
	 */
};

struct atomisp_sub_device {
	struct v4l2_subdev subdev;
	struct media_pad pads[ATOMISP_SUBDEV_PADS_NUM];
	struct atomisp_pad_format fmt[ATOMISP_SUBDEV_PADS_NUM];
	uint16_t capture_pad; /* main capture pad; defines much of isp config */

	enum atomisp_subdev_input_entity input;
	unsigned int output;
	struct atomisp_video_pipe video_in;
	struct atomisp_video_pipe video_out_capture; /* capture output */
	struct atomisp_video_pipe video_out_vf;      /* viewfinder output */
	struct atomisp_video_pipe video_out_preview; /* preview output */
	struct atomisp_acc_pipe video_acc;
	/* video pipe main output */
	struct atomisp_video_pipe video_out_video_capture;
	/* struct isp_subdev_params params; */
	spinlock_t lock;
	struct atomisp_device *isp;
	struct v4l2_ctrl_handler ctrl_handler;
	struct v4l2_ctrl *fmt_auto;
	struct v4l2_ctrl *run_mode;
	struct v4l2_ctrl *depth_mode;
	struct v4l2_ctrl *vfpp;
	struct v4l2_ctrl *continuous_mode;
	struct v4l2_ctrl *continuous_raw_buffer_size;
	struct v4l2_ctrl *continuous_viewfinder;
	struct v4l2_ctrl *enable_raw_buffer_lock;
#ifdef ISP2401
	struct v4l2_ctrl *ion_dev_fd;
#endif
	struct v4l2_ctrl *disable_dz;
#ifdef ISP2401
	struct v4l2_ctrl *select_isp_version;
#endif

	struct {
		struct list_head fw;
		struct list_head memory_maps;
		struct atomisp_css_pipeline *pipeline;
		bool extension_mode;
		struct ida ida;
		struct completion acc_done;
		void *acc_stages;
	} acc;

	struct atomisp_subdev_params params;

	struct atomisp_stream_env stream_env[ATOMISP_INPUT_STREAM_NUM];

	struct v4l2_pix_format dvs_envelop;
	unsigned int s3a_bufs_in_css[CSS_PIPE_ID_NUM];
	unsigned int dis_bufs_in_css;

	unsigned int metadata_bufs_in_css
		[ATOMISP_INPUT_STREAM_NUM][CSS_PIPE_ID_NUM];
	/* The list of free and available metadata buffers for CSS */
	struct list_head metadata[ATOMISP_METADATA_TYPE_NUM];
	/* The list of metadata buffers which have been en-queued to CSS */
	struct list_head metadata_in_css[ATOMISP_METADATA_TYPE_NUM];
	/* The list of metadata buffers which are ready for userspace to get */
	struct list_head metadata_ready[ATOMISP_METADATA_TYPE_NUM];

	/* The list of free and available s3a stat buffers for CSS */
	struct list_head s3a_stats;
	/* The list of s3a stat buffers which have been en-queued to CSS */
	struct list_head s3a_stats_in_css;
	/* The list of s3a stat buffers which are ready for userspace to get */
	struct list_head s3a_stats_ready;

	struct list_head dis_stats;
	struct list_head dis_stats_in_css;
	spinlock_t dis_stats_lock;

	struct atomisp_css_frame *vf_frame; /* TODO: needed? */
	struct atomisp_css_frame *raw_output_frame;
	enum atomisp_frame_status frame_status[VIDEO_MAX_FRAME];

	/* This field specifies which camera (v4l2 input) is selected. */
	int input_curr;
	/* This field specifies which sensor is being selected when there
	   are multiple sensors connected to the same MIPI port. */
	int sensor_curr;

	atomic_t sof_count;
	atomic_t sequence;      /* Sequence value that is assigned to buffer. */
	atomic_t sequence_temp;

	unsigned int streaming; /* Hold both mutex and lock to change this */
	bool stream_prepared; /* whether css stream is created */

	/* subdev index: will be used to show which subdev is holding the
	 * resource, like which camera is used by which subdev
	 */
	unsigned int index;

	/* delayed memory allocation for css */
	struct completion init_done;
	struct workqueue_struct *delayed_init_workq;
	unsigned int delayed_init;
	struct work_struct delayed_init_work;

	unsigned int latest_preview_exp_id; /* CSS ZSL/SDV raw buffer id */

	unsigned int mipi_frame_size;

	bool copy_mode; /* CSI2+ use copy mode */
	bool yuvpp_mode;	/* CSI2+ yuvpp pipe */

	int raw_buffer_bitmap[ATOMISP_MAX_EXP_ID/32 + 1]; /* Record each Raw Buffer lock status */
	int raw_buffer_locked_count;
	spinlock_t raw_buffer_bitmap_lock;

#ifndef ISP2401
	struct timer_list wdt;
	unsigned int wdt_duration;	/* in jiffies */
	unsigned long wdt_expires;

#endif
	struct atomisp_resolution sensor_array_res;
	bool high_speed_mode; /* Indicate whether now is a high speed mode */
	int pending_capture_request; /* Indicates the number of pending capture requests. */
#ifndef ISP2401

#else
	bool re_trigger_capture;
#endif
	unsigned int preview_exp_id;
	unsigned int postview_exp_id;
};

extern const struct atomisp_in_fmt_conv atomisp_in_fmt_conv[];

u32 atomisp_subdev_uncompressed_code(u32 code);
bool atomisp_subdev_is_compressed(u32 code);
const struct atomisp_in_fmt_conv *atomisp_find_in_fmt_conv(u32 code);
#ifndef ISP2401
const struct atomisp_in_fmt_conv *atomisp_find_in_fmt_conv_by_atomisp_in_fmt(
	enum atomisp_css_stream_format atomisp_in_fmt);
#else
const struct atomisp_in_fmt_conv
    *atomisp_find_in_fmt_conv_by_atomisp_in_fmt(enum atomisp_css_stream_format
						atomisp_in_fmt);
#endif
const struct atomisp_in_fmt_conv *atomisp_find_in_fmt_conv_compressed(u32 code);
bool atomisp_subdev_format_conversion(struct atomisp_sub_device *asd,
				      unsigned int source_pad);
uint16_t atomisp_subdev_source_pad(struct video_device *vdev);

/* Get pointer to appropriate format */
struct v4l2_mbus_framefmt
*atomisp_subdev_get_ffmt(struct v4l2_subdev *sd,
			 struct v4l2_subdev_pad_config *cfg, uint32_t which,
			 uint32_t pad);
struct v4l2_rect *atomisp_subdev_get_rect(struct v4l2_subdev *sd,
					  struct v4l2_subdev_pad_config *cfg,
					  uint32_t which, uint32_t pad,
					  uint32_t target);
int atomisp_subdev_set_selection(struct v4l2_subdev *sd,
				 struct v4l2_subdev_pad_config *cfg,
				 uint32_t which, uint32_t pad, uint32_t target,
				 uint32_t flags, struct v4l2_rect *r);
/* Actually set the format */
void atomisp_subdev_set_ffmt(struct v4l2_subdev *sd,
			     struct v4l2_subdev_pad_config *cfg, uint32_t which,
			     uint32_t pad, struct v4l2_mbus_framefmt *ffmt);

int atomisp_update_run_mode(struct atomisp_sub_device *asd);

void atomisp_subdev_cleanup_pending_events(struct atomisp_sub_device *asd);

void atomisp_subdev_unregister_entities(struct atomisp_sub_device *asd);
int atomisp_subdev_register_entities(struct atomisp_sub_device *asd,
	struct v4l2_device *vdev);
int atomisp_subdev_init(struct atomisp_device *isp);
void atomisp_subdev_cleanup(struct atomisp_device *isp);
int atomisp_create_pads_links(struct atomisp_device *isp);

#endif /* __ATOMISP_SUBDEV_H__ */
	struct {
		struct list_head fw;
		struct list_head memory_maps;
		struct atomisp_css_pipeline *pipeline;
		bool extension_mode;
		struct ida ida;
		struct completion acc_done;
		void *acc_stages;
	} acc;

	struct atomisp_subdev_params params;

	struct atomisp_stream_env stream_env[ATOMISP_INPUT_STREAM_NUM];

	struct v4l2_pix_format dvs_envelop;
	unsigned int s3a_bufs_in_css[CSS_PIPE_ID_NUM];
	unsigned int dis_bufs_in_css;

	unsigned int metadata_bufs_in_css
		[ATOMISP_INPUT_STREAM_NUM][CSS_PIPE_ID_NUM];
	/* The list of free and available metadata buffers for CSS */
	struct list_head metadata[ATOMISP_METADATA_TYPE_NUM];
	/* The list of metadata buffers which have been en-queued to CSS */
	struct list_head metadata_in_css[ATOMISP_METADATA_TYPE_NUM];
	/* The list of metadata buffers which are ready for userspace to get */
	struct list_head metadata_ready[ATOMISP_METADATA_TYPE_NUM];

	/* The list of free and available s3a stat buffers for CSS */
	struct list_head s3a_stats;
	/* The list of s3a stat buffers which have been en-queued to CSS */
	struct list_head s3a_stats_in_css;
	/* The list of s3a stat buffers which are ready for userspace to get */
	struct list_head s3a_stats_ready;

	struct list_head dis_stats;
	struct list_head dis_stats_in_css;
	spinlock_t dis_stats_lock;

	struct atomisp_css_frame *vf_frame; /* TODO: needed? */
	struct atomisp_css_frame *raw_output_frame;
	enum atomisp_frame_status frame_status[VIDEO_MAX_FRAME];

	/* This field specifies which camera (v4l2 input) is selected. */
	int input_curr;
	/* This field specifies which sensor is being selected when there
	   are multiple sensors connected to the same MIPI port. */
	int sensor_curr;

	atomic_t sof_count;
	atomic_t sequence;      /* Sequence value that is assigned to buffer. */
	atomic_t sequence_temp;

	unsigned int streaming; /* Hold both mutex and lock to change this */
	bool stream_prepared; /* whether css stream is created */

	/* subdev index: will be used to show which subdev is holding the
	 * resource, like which camera is used by which subdev
	 */
	unsigned int index;

	/* delayed memory allocation for css */
	struct completion init_done;
	struct workqueue_struct *delayed_init_workq;
	unsigned int delayed_init;
	struct work_struct delayed_init_work;

	unsigned int latest_preview_exp_id; /* CSS ZSL/SDV raw buffer id */

	unsigned int mipi_frame_size;

	bool copy_mode; /* CSI2+ use copy mode */
	bool yuvpp_mode;	/* CSI2+ yuvpp pipe */

	int raw_buffer_bitmap[ATOMISP_MAX_EXP_ID/32 + 1]; /* Record each Raw Buffer lock status */
	int raw_buffer_locked_count;
	spinlock_t raw_buffer_bitmap_lock;

#ifndef ISP2401
	struct timer_list wdt;
	unsigned int wdt_duration;	/* in jiffies */
	unsigned long wdt_expires;

#endif
	struct atomisp_resolution sensor_array_res;
	bool high_speed_mode; /* Indicate whether now is a high speed mode */
	int pending_capture_request; /* Indicates the number of pending capture requests. */
#ifndef ISP2401

#else
	bool re_trigger_capture;
#endif
	unsigned int preview_exp_id;
	unsigned int postview_exp_id;
};

extern const struct atomisp_in_fmt_conv atomisp_in_fmt_conv[];

u32 atomisp_subdev_uncompressed_code(u32 code);
bool atomisp_subdev_is_compressed(u32 code);
const struct atomisp_in_fmt_conv *atomisp_find_in_fmt_conv(u32 code);
#ifndef ISP2401
const struct atomisp_in_fmt_conv *atomisp_find_in_fmt_conv_by_atomisp_in_fmt(
	enum atomisp_css_stream_format atomisp_in_fmt);
#else
const struct atomisp_in_fmt_conv
    *atomisp_find_in_fmt_conv_by_atomisp_in_fmt(enum atomisp_css_stream_format
						atomisp_in_fmt);
#endif
const struct atomisp_in_fmt_conv *atomisp_find_in_fmt_conv_compressed(u32 code);
bool atomisp_subdev_format_conversion(struct atomisp_sub_device *asd,
				      unsigned int source_pad);
uint16_t atomisp_subdev_source_pad(struct video_device *vdev);

/* Get pointer to appropriate format */
struct v4l2_mbus_framefmt
*atomisp_subdev_get_ffmt(struct v4l2_subdev *sd,
			 struct v4l2_subdev_pad_config *cfg, uint32_t which,
			 uint32_t pad);
struct v4l2_rect *atomisp_subdev_get_rect(struct v4l2_subdev *sd,
					  struct v4l2_subdev_pad_config *cfg,
					  uint32_t which, uint32_t pad,
					  uint32_t target);
int atomisp_subdev_set_selection(struct v4l2_subdev *sd,
				 struct v4l2_subdev_pad_config *cfg,
				 uint32_t which, uint32_t pad, uint32_t target,
				 uint32_t flags, struct v4l2_rect *r);
/* Actually set the format */
void atomisp_subdev_set_ffmt(struct v4l2_subdev *sd,
			     struct v4l2_subdev_pad_config *cfg, uint32_t which,
			     uint32_t pad, struct v4l2_mbus_framefmt *ffmt);

int atomisp_update_run_mode(struct atomisp_sub_device *asd);

void atomisp_subdev_cleanup_pending_events(struct atomisp_sub_device *asd);

void atomisp_subdev_unregister_entities(struct atomisp_sub_device *asd);
int atomisp_subdev_register_entities(struct atomisp_sub_device *asd,
	struct v4l2_device *vdev);
int atomisp_subdev_init(struct atomisp_device *isp);
void atomisp_subdev_cleanup(struct atomisp_device *isp);
int atomisp_create_pads_links(struct atomisp_device *isp);

#endif /* __ATOMISP_SUBDEV_H__ */