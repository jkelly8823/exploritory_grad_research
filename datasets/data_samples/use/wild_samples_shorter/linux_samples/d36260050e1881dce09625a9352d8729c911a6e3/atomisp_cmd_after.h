 */
void atomisp_eof_event(struct atomisp_sub_device *asd, uint8_t exp_id);

enum mipi_port_id __get_mipi_port(struct atomisp_device *isp,
				enum atomisp_camera_port port);

bool atomisp_is_vf_pipe(struct atomisp_video_pipe *pipe);
