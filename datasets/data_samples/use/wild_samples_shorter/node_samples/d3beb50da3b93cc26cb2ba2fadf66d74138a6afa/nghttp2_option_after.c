  option->opt_set_mask |= NGHTTP2_OPT_MAX_OUTBOUND_ACK;
  option->max_outbound_ack = val;
}

void nghttp2_option_set_max_settings(nghttp2_option *option, size_t val) {
  option->opt_set_mask |= NGHTTP2_OPT_MAX_SETTINGS;
  option->max_settings = val;
}