namespace node {

#define SECURITY_REVERSIONS(XX)                                            \
  XX(CVE_2019_9514, "CVE-2019-9514", "HTTP/2 Reset Flood")                 \
  XX(CVE_2019_9516, "CVE-2019-9516", "HTTP/2 0-Length Headers Leak")       \
  XX(CVE_2019_9518, "CVE-2019-9518", "HTTP/2 Empty DATA Frame Flooding")   \
//  XX(CVE_2016_PEND, "CVE-2016-PEND", "Vulnerability Title")
  // TODO(addaleax): Remove all of the above before Node.js 13 as the comment
  // at the start of the file indicates.

enum reversion {
#define V(code, ...) SECURITY_REVERT_##code,
  SECURITY_REVERSIONS(V)
#undef V
};

namespace per_process {
extern unsigned int reverted_cve;
}

inline const char* RevertMessage(const reversion cve) {
#define V(code, label, msg) case SECURITY_REVERT_##code: return label ": " msg;
  switch (cve) {
    SECURITY_REVERSIONS(V)
    default:
      return "Unknown";
  }
#undef V
}

inline void Revert(const reversion cve) {
  per_process::reverted_cve |= 1 << cve;
  printf("SECURITY WARNING: Reverting %s\n", RevertMessage(cve));
}

inline void Revert(const char* cve, std::string* error) {
#define V(code, label, _)                                                     \
  if (strcmp(cve, label) == 0) return Revert(SECURITY_REVERT_##code);
  SECURITY_REVERSIONS(V)
#undef V
  *error = "Error: Attempt to revert an unknown CVE [";
  *error += cve;
  *error += ']';
}

inline bool IsReverted(const reversion cve) {
  return per_process::reverted_cve & (1 << cve);
}

inline bool IsReverted(const char* cve) {
#define V(code, label, _)                                                     \
  if (strcmp(cve, label) == 0) return IsReverted(SECURITY_REVERT_##code);
  SECURITY_REVERSIONS(V)
  return false;
#undef V
}

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_REVERT_H_