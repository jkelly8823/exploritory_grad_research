    StreamReq::ResetObject(req_wrap_obj);
  }

  AsyncHooks::DefaultTriggerAsyncIdScope trigger_scope(GetAsyncWrap());
  ShutdownWrap* req_wrap = CreateShutdownWrap(req_wrap_obj);
  int err = DoShutdown(req_wrap);

  if (err != 0 && req_wrap != nullptr) {
    req_wrap->Dispose();
  if (send_handle == nullptr) {
    err = DoTryWrite(&bufs, &count);
    if (err != 0 || count == 0) {
      return StreamWriteResult { false, err, nullptr, total_bytes };
    }
  }

  v8::HandleScope handle_scope(env->isolate());
    if (!env->write_wrap_template()
             ->NewInstance(env->context())
             .ToLocal(&req_wrap_obj)) {
      return StreamWriteResult { false, UV_EBUSY, nullptr, 0 };
    }
    StreamReq::ResetObject(req_wrap_obj);
  }

  AsyncHooks::DefaultTriggerAsyncIdScope trigger_scope(GetAsyncWrap());
  WriteWrap* req_wrap = CreateWriteWrap(req_wrap_obj);

  err = DoWrite(req_wrap, bufs, count, send_handle);
  bool async = err == 0;

    ClearError();
  }

  return StreamWriteResult { async, err, req_wrap, total_bytes };
}

template <typename OtherBase>
SimpleShutdownWrap<OtherBase>::SimpleShutdownWrap(