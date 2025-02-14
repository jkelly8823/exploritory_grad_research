  enum Http2OptionsIndex {
    IDX_OPTIONS_MAX_DEFLATE_DYNAMIC_TABLE_SIZE,
    IDX_OPTIONS_MAX_RESERVED_REMOTE_STREAMS,
    IDX_OPTIONS_MAX_SEND_HEADER_BLOCK_LENGTH,
    IDX_OPTIONS_PEER_MAX_CONCURRENT_STREAMS,
    IDX_OPTIONS_PADDING_STRATEGY,
    IDX_OPTIONS_MAX_HEADER_LIST_PAIRS,
    IDX_OPTIONS_MAX_OUTSTANDING_PINGS,
    IDX_OPTIONS_MAX_OUTSTANDING_SETTINGS,
    IDX_OPTIONS_MAX_SESSION_MEMORY,
    IDX_OPTIONS_MAX_SETTINGS,
    IDX_OPTIONS_FLAGS
  };

  enum Http2StreamStatisticsIndex {
    IDX_STREAM_STATS_ID,
    IDX_STREAM_STATS_TIMETOFIRSTBYTE,
    IDX_STREAM_STATS_TIMETOFIRSTHEADER,
    IDX_STREAM_STATS_TIMETOFIRSTBYTESENT,
    IDX_STREAM_STATS_SENTBYTES,
    IDX_STREAM_STATS_RECEIVEDBYTES,
    IDX_STREAM_STATS_COUNT
  };

  enum Http2SessionStatisticsIndex {
    IDX_SESSION_STATS_TYPE,
    IDX_SESSION_STATS_PINGRTT,
    IDX_SESSION_STATS_FRAMESRECEIVED,
    IDX_SESSION_STATS_FRAMESSENT,
    IDX_SESSION_STATS_STREAMCOUNT,
    IDX_SESSION_STATS_STREAMAVERAGEDURATION,
    IDX_SESSION_STATS_DATA_SENT,
    IDX_SESSION_STATS_DATA_RECEIVED,
    IDX_SESSION_STATS_MAX_CONCURRENT_STREAMS,
    IDX_SESSION_STATS_COUNT
  };

class Http2State : public BaseObject {
 public:
  Http2State(Environment* env, v8::Local<v8::Object> obj)
      : BaseObject(env, obj),
        root_buffer(env->isolate(), sizeof(http2_state_internal)),
        session_state_buffer(
            env->isolate(),
            offsetof(http2_state_internal, session_state_buffer),
            IDX_SESSION_STATE_COUNT,
            root_buffer),
        stream_state_buffer(
            env->isolate(),
            offsetof(http2_state_internal, stream_state_buffer),
            IDX_STREAM_STATE_COUNT,
            root_buffer),
        stream_stats_buffer(
            env->isolate(),
            offsetof(http2_state_internal, stream_stats_buffer),
            IDX_STREAM_STATS_COUNT,
            root_buffer),
        session_stats_buffer(
            env->isolate(),
            offsetof(http2_state_internal, session_stats_buffer),
            IDX_SESSION_STATS_COUNT,
            root_buffer),
        options_buffer(env->isolate(),
                        offsetof(http2_state_internal, options_buffer),
                        IDX_OPTIONS_FLAGS + 1,
                        root_buffer),
        settings_buffer(env->isolate(),
                        offsetof(http2_state_internal, settings_buffer),
                        IDX_SETTINGS_COUNT + 1,
                        root_buffer) {}

  AliasedUint8Array root_buffer;
  AliasedFloat64Array session_state_buffer;
  AliasedFloat64Array stream_state_buffer;
  AliasedFloat64Array stream_stats_buffer;
  AliasedFloat64Array session_stats_buffer;
  AliasedUint32Array options_buffer;
  AliasedUint32Array settings_buffer;

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_SELF_SIZE(Http2State)
  SET_MEMORY_INFO_NAME(Http2State)

  static constexpr FastStringKey binding_data_name { "http2" };

 private:
  struct http2_state_internal {
    // doubles first so that they are always sizeof(double)-aligned
    double session_state_buffer[IDX_SESSION_STATE_COUNT];
    double stream_state_buffer[IDX_STREAM_STATE_COUNT];
    double stream_stats_buffer[IDX_STREAM_STATS_COUNT];
    double session_stats_buffer[IDX_SESSION_STATS_COUNT];
    uint32_t options_buffer[IDX_OPTIONS_FLAGS + 1];
    uint32_t settings_buffer[IDX_SETTINGS_COUNT + 1];
  };
};

}  // namespace http2
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_HTTP2_STATE_H_