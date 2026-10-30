/* ltws core: the libwireshark/epan decode path with no JNI, so the identical
 * code is exercised by the Android JNI wrapper (ws_jni.c) and by the host
 * test harness (wireshark-spike/ws_core_test.c). Wireshark is the decoder. */
#ifndef LTWS_CORE_H
#define LTWS_CORE_H

#include <stdint.h>

typedef struct {
    int     performed_now;  /* 1 only on the call that actually ran initialization */
    int     ok;             /* 1 if epan_init returned true */
    int64_t epan_init_ns;   /* epan_init alone (dissector registration), monotonic clock */
    int64_t total_init_ns;  /* log + privileges + wtap_init + epan_init */
    int     calls;          /* how many times ltws_init has been called in this process */
} ltws_init_result;

/* Idempotent and thread-safe. Later calls return the first call's timings
 * with performed_now = 0; initialization is never re-run. */
ltws_init_result ltws_init(void);

/* epan version string (static storage). */
const char *ltws_version(void);

/* Decode raw 802.11 bytes (WTAP_ENCAP_IEEE_802_11, no radiotap). Returns a
 * newly allocated field dump (free with ltws_free), or NULL if initialization
 * failed or was never performed. */
char *ltws_decode(const uint8_t *frame, int len);

/* Memory control: the same JNI round trip and an allocated result string of a
 * comparable size, but NO epan dissection. Used to separate the decoder's
 * memory behaviour from JNI marshalling and result-string allocation.
 * Returns a newly allocated string (free with ltws_free). */
char *ltws_control(const uint8_t *frame, int len);

/* Decode and return the FULL Wireshark protocol tree as structured records,
 * one per line: "depth<TAB>abbrev<TAB>value<TAB>start<TAB>length<TAB>label".
 * depth is the tree depth, start/length are byte offsets into the frame,
 * value is the field value as Wireshark displays it (or the label for nodes
 * without a value), label is Wireshark's full tree text for the node.
 * Newly allocated (free with ltws_free), or NULL if not initialized. */
char *ltws_decode_tree(const uint8_t *frame, int len);

/* Decode up to max_frames records from a pcap/pcapng file. Returns a newly
 * allocated summary: a header line, then one line per frame
 * "frame<TAB>encap<TAB>caplen<TAB>type_subtype<TAB>sa<TAB>da<TAB>reason".
 * Fields absent for a given frame are empty. NULL if the file cannot be
 * opened or epan is not initialized; *err_out (if non-NULL) gets a short
 * reason on failure (static storage, do not free). */
char *ltws_decode_pcap(const char *path, int max_frames, const char **err_out);

/* Frame-by-frame capture reader, so a caller can report progress and cancel
 * between frames. Every frame is dissected with Wireshark on its own: no
 * reassembly or conversation state is carried between frames.
 *
 * ltws_pcap_open never returns NULL (except on allocation failure); check
 * ltws_pcap_error right after opening. Each ltws_pcap_next returns a newly
 * allocated record set (free with ltws_free), or NULL at end of file or on a
 * read error (ltws_pcap_error says which). Record set layout, one per line:
 *   #frame<TAB>number<TAB>encap<TAB>encapName<TAB>caplen<TAB>len<TAB>hasTs<TAB>secs<TAB>nsecs<TAB>fileOffset
 *   #hex<TAB>lowercase hex of the first min(caplen, LTWS_HEX_MAX) bytes
 *   depth<TAB>abbrev<TAB>value<TAB>start<TAB>length<TAB>label   (one per tree node) */
#define LTWS_HEX_MAX 4096
typedef struct ltws_pcap ltws_pcap;
ltws_pcap  *ltws_pcap_open(const char *path);
char       *ltws_pcap_next(ltws_pcap *p);
const char *ltws_pcap_error(const ltws_pcap *p);      /* NULL when no error */
const char *ltws_pcap_file_type(const ltws_pcap *p);  /* Wireshark's file-type name */
int64_t     ltws_pcap_file_size(const ltws_pcap *p);  /* bytes, or -1 if unknown */
void        ltws_pcap_close(ltws_pcap *p);

/* Vendor lookup in Wireshark's built-in IEEE registry (MA-L, MA-M, MA-S, and
 * CID blocks), by the longest matching prefix of a 6-byte address, most
 * significant byte first. Needs no initialization. Returns 1 and sets the
 * out pointers (static storage, do not free) when a block matches, else 0. */
int ltws_manuf_lookup(const uint8_t addr[6], const char **short_name,
                      const char **long_name, unsigned *mask_bits);

void ltws_free(char *s);

#endif
