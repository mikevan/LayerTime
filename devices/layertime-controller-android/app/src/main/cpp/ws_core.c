/* ltws core: see ws_core.h. Built for Wireshark 4.4.19. */
#include "ws_core.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <glib.h>
#include <epan/epan.h>
#include <epan/epan_dissect.h>
#include <epan/proto.h>
#include <epan/tvbuff.h>
#include <epan/packet.h>
#include <epan/frame_data.h>
#include <wiretap/wtap.h>
#include <wsutil/privileges.h>
#include <wsutil/wslog.h>

static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static int g_attempted = 0;
static ltws_init_result g_first;   /* timings and status from the call that initialized */
static int g_calls = 0;
/* epan dissection is not thread-safe. Every dissection in this file holds this
 * lock, so the diagnostic screen and a capture import can never overlap. */
static pthread_mutex_t g_dissect = PTHREAD_MUTEX_INITIALIZER;

static int64_t now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

ltws_init_result ltws_init(void) {
    ltws_init_result r;
    pthread_mutex_lock(&g_lock);
    g_calls++;
    if (!g_attempted) {
        g_attempted = 1;
        int64_t t0 = now_ns();
        ws_log_init("ltws", NULL);
        init_process_policies();
        wtap_init(false);
        int64_t t1 = now_ns();
        bool ok = epan_init(NULL, NULL, false);
        int64_t t2 = now_ns();
        g_first.performed_now = 1;
        g_first.ok = ok ? 1 : 0;
        g_first.epan_init_ns = t2 - t1;
        g_first.total_init_ns = t2 - t0;
        r = g_first;
    } else {
        r = g_first;
        r.performed_now = 0;
    }
    r.calls = g_calls;
    pthread_mutex_unlock(&g_lock);
    return r;
}

const char *ltws_version(void) { return epan_get_version(); }

static int ltws_ready(void) {
    pthread_mutex_lock(&g_lock);
    int ready = g_attempted && g_first.ok;
    pthread_mutex_unlock(&g_lock);
    return ready;
}

static const nstime_t *pp_ts(struct packet_provider_data *p _U_, guint32 n _U_) { return NULL; }

static void append_field(GString *out, epan_dissect_t *edt, const char *name) {
    int hf = proto_registrar_get_id_byname(name);
    if (hf <= 0) { g_string_append_printf(out, "%-24s : <not registered>\n", name); return; }
    GPtrArray *arr = proto_find_finfo(edt->tree, hf);
    if (!arr || arr->len == 0) {
        g_string_append_printf(out, "%-24s : <absent>\n", name);
        if (arr) g_ptr_array_free(arr, TRUE);
        return;
    }
    for (guint i = 0; i < arr->len; i++) {
        field_info *fi = (field_info *)g_ptr_array_index(arr, i);
        char *s = fvalue_to_string_repr(NULL, fi->value, FTREPR_DISPLAY, fi->hfinfo->display);
        g_string_append_printf(out, "%-24s : %s\n", name, s ? s : "(null)");
        wmem_free(NULL, s);
    }
    g_ptr_array_free(arr, TRUE);
}

/* Build a dissection for one raw 802.11 frame. Caller frees via finish_decode. */
static epan_dissect_t *run_decode(const uint8_t *frame, int len, epan_t **session_out, frame_data *fdata) {
    static struct packet_provider_funcs funcs = { .get_frame_ts = pp_ts };
    epan_t *session = epan_new(NULL, &funcs);
    epan_dissect_t *edt = epan_dissect_new(session, TRUE, TRUE);
    static wtap_rec rec;
    memset(&rec, 0, sizeof(rec));
    rec.rec_type = REC_TYPE_PACKET;
    rec.rec_header.packet_header.caplen = (guint32)len;
    rec.rec_header.packet_header.len = (guint32)len;
    rec.rec_header.packet_header.pkt_encap = WTAP_ENCAP_IEEE_802_11;
    memset(fdata, 0, sizeof(*fdata));
    frame_data_init(fdata, 1, &rec, 0, 0);
    tvbuff_t *tvb = tvb_new_real_data(frame, (guint)len, (gint)len);  /* owned by edt */
    epan_dissect_run(edt, WTAP_FILE_TYPE_SUBTYPE_UNKNOWN, &rec, tvb, fdata, NULL);
    *session_out = session;
    return edt;
}
static void finish_decode(epan_dissect_t *edt, epan_t *session, frame_data *fdata) {
    epan_dissect_free(edt);
    frame_data_destroy(fdata);
    epan_free(session);
}

char *ltws_decode(const uint8_t *frame, int len) {
    if (!ltws_ready() || frame == NULL || len <= 0) return NULL;
    epan_t *session; frame_data fdata;
    pthread_mutex_lock(&g_dissect);
    epan_dissect_t *edt = run_decode(frame, len, &session, &fdata);
    GString *out = g_string_new(NULL);
    g_string_append_printf(out, "epan %s, %d bytes\n", epan_get_version(), len);
    append_field(out, edt, "wlan.fc.type_subtype");
    append_field(out, edt, "wlan.da");
    append_field(out, edt, "wlan.sa");
    append_field(out, edt, "wlan.bssid");
    append_field(out, edt, "wlan.seq");
    append_field(out, edt, "wlan.fixed.reason_code");
    finish_decode(edt, session, &fdata);
    pthread_mutex_unlock(&g_dissect);
    return g_string_free(out, FALSE);
}

char *ltws_control(const uint8_t *frame, int len) {
    /* Same JNI path and a comparable result string, but no epan work at all. */
    if (frame == NULL || len <= 0) return NULL;
    unsigned sum = 0;
    for (int i = 0; i < len; i++) sum += frame[i];
    GString *out = g_string_new(NULL);
    g_string_append_printf(out,
        "control %s, %d bytes, checksum 0x%04x\n"
        "wlan.fc.type_subtype     : (control)\n"
        "wlan.da                  : (control)\n"
        "wlan.sa                  : (control)\n"
        "wlan.bssid               : (control)\n"
        "wlan.seq                 : (control)\n"
        "wlan.fixed.reason_code   : (control)\n",
        epan_get_version(), len, sum & 0xffff);
    return g_string_free(out, FALSE);
}

typedef struct { GString *out; int depth; } tree_ctx;

static void tree_node(proto_node *node, void *data) {
    tree_ctx *ctx = (tree_ctx *)data;
    field_info *fi = PNODE_FINFO(node);
    if (fi && fi->hfinfo) {
        char *val = NULL;
        if (fi->value) val = fvalue_to_string_repr(NULL, fi->value, FTREPR_DISPLAY, fi->hfinfo->display);
        char label[ITEM_LABEL_LENGTH];
        label[0] = '\0';
        if (fi->rep && fi->rep->representation[0]) {
            g_strlcpy(label, fi->rep->representation, sizeof label);
        } else {
            proto_item_fill_label(fi, label);
        }
        for (char *p = label; *p; p++) if (*p == '\t' || *p == '\n') *p = ' ';
        if (val) for (char *p = val; *p; p++) if (*p == '\t' || *p == '\n') *p = ' ';
        g_string_append_printf(ctx->out, "%d\t%s\t%s\t%d\t%d\t%s\n",
            ctx->depth,
            fi->hfinfo->abbrev ? fi->hfinfo->abbrev : "",
            val ? val : label,
            fi->start, fi->length, label);
        if (val) wmem_free(NULL, val);
    }
    tree_ctx child = { ctx->out, ctx->depth + 1 };
    proto_tree_children_foreach(node, tree_node, &child);
}

char *ltws_decode_tree(const uint8_t *frame, int len) {
    if (!ltws_ready() || frame == NULL || len <= 0) return NULL;
    epan_t *session; frame_data fdata;
    pthread_mutex_lock(&g_dissect);
    epan_dissect_t *edt = run_decode(frame, len, &session, &fdata);
    GString *out = g_string_new(NULL);
    tree_ctx ctx = { out, 0 };
    proto_tree_children_foreach(edt->tree, tree_node, &ctx);
    finish_decode(edt, session, &fdata);
    pthread_mutex_unlock(&g_dissect);
    return g_string_free(out, FALSE);
}

static void append_named(GString *out, epan_dissect_t *edt, const char *name) {
    int hf = proto_registrar_get_id_byname(name);
    g_string_append_c(out, '\t');
    if (hf <= 0) return;
    GPtrArray *arr = proto_find_finfo(edt->tree, hf);
    if (arr && arr->len > 0) {
        field_info *fi = (field_info *)g_ptr_array_index(arr, 0);
        char *s = fvalue_to_string_repr(NULL, fi->value, FTREPR_DISPLAY, fi->hfinfo->display);
        if (s) { g_string_append(out, s); wmem_free(NULL, s); }
    }
    if (arr) g_ptr_array_free(arr, TRUE);
}

char *ltws_decode_pcap(const char *path, int max_frames, const char **err_out) {
    if (err_out) *err_out = NULL;
    if (!ltws_ready()) { if (err_out) *err_out = "epan not initialized"; return NULL; }
    if (!path) { if (err_out) *err_out = "null path"; return NULL; }
    int err = 0; gchar *err_info = NULL;
    wtap *wth = wtap_open_offline(path, WTAP_TYPE_AUTO, &err, &err_info, FALSE);
    if (!wth) {
        if (err_out) *err_out = "cannot open capture file";
        g_free(err_info);
        return NULL;
    }
    GString *out = g_string_new(NULL);
    g_string_append(out, "frame\tencap\tcaplen\ttype_subtype\tsa\tda\treason\n");
    wtap_rec rec; Buffer buf;
    wtap_rec_init(&rec);
    ws_buffer_init(&buf, 1514);
    gint64 offset = 0;
    int count = 0;
    static struct packet_provider_funcs funcs = { .get_frame_ts = pp_ts };
    while ((max_frames <= 0 || count < max_frames) &&
           wtap_read(wth, &rec, &buf, &err, &err_info, &offset)) {
        if (rec.rec_type != REC_TYPE_PACKET) continue;
        count++;
        pthread_mutex_lock(&g_dissect);
        epan_t *session = epan_new(NULL, &funcs);
        epan_dissect_t *edt = epan_dissect_new(session, TRUE, TRUE);
        frame_data fdata; memset(&fdata, 0, sizeof fdata);
        frame_data_init(&fdata, count, &rec, offset, 0);
        guint caplen = rec.rec_header.packet_header.caplen;
        tvbuff_t *tvb = tvb_new_real_data(ws_buffer_start_ptr(&buf), caplen, caplen);
        epan_dissect_run(edt, wtap_file_type_subtype(wth), &rec, tvb, &fdata, NULL);
        g_string_append_printf(out, "%d\t%d\t%u", count,
            rec.rec_header.packet_header.pkt_encap, caplen);
        append_named(out, edt, "wlan.fc.type_subtype");
        append_named(out, edt, "wlan.sa");
        append_named(out, edt, "wlan.da");
        append_named(out, edt, "wlan.fixed.reason_code");
        g_string_append_c(out, '\n');
        epan_dissect_free(edt);
        frame_data_destroy(&fdata);
        epan_free(session);
        pthread_mutex_unlock(&g_dissect);
        wtap_rec_reset(&rec);
    }
    g_string_append_printf(out, "frames decoded: %d\n", count);
    ws_buffer_free(&buf);
    wtap_rec_cleanup(&rec);
    wtap_close(wth);
    g_free(err_info);
    return g_string_free(out, FALSE);
}

/* ---- frame-by-frame capture reader ---- */

struct ltws_pcap {
    wtap     *wth;
    wtap_rec  rec;
    Buffer    buf;
    int       count;
    int       done;
    int64_t   size;
    char      error[320];
    char      file_type[64];
};

static void set_error(ltws_pcap *p, const char *what, int err, char *err_info) {
    snprintf(p->error, sizeof p->error, "%s: %s%s%s%s", what,
        err ? wtap_strerror(err) : "unknown error",
        err_info ? " (" : "", err_info ? err_info : "", err_info ? ")" : "");
}

ltws_pcap *ltws_pcap_open(const char *path) {
    ltws_pcap *p = g_new0(ltws_pcap, 1);
    p->size = -1;
    g_strlcpy(p->file_type, "unknown", sizeof p->file_type);
    if (!ltws_ready()) {
        g_strlcpy(p->error, "Wireshark is not initialized.", sizeof p->error);
        p->done = 1;
        return p;
    }
    if (!path) { g_strlcpy(p->error, "No file path was given.", sizeof p->error); p->done = 1; return p; }
    int err = 0; gchar *err_info = NULL;
    p->wth = wtap_open_offline(path, WTAP_TYPE_AUTO, &err, &err_info, FALSE);
    if (!p->wth) {
        set_error(p, "Wireshark cannot open this file as a capture", err, err_info);
        g_free(err_info);
        p->done = 1;
        return p;
    }
    g_free(err_info);
    wtap_rec_init(&p->rec);
    ws_buffer_init(&p->buf, 1514);
    int serr = 0;
    p->size = wtap_file_size(p->wth, &serr);
    if (serr) p->size = -1;
    const char *name = wtap_file_type_subtype_name(wtap_file_type_subtype(p->wth));
    if (name) g_strlcpy(p->file_type, name, sizeof p->file_type);
    return p;
}

char *ltws_pcap_next(ltws_pcap *p) {
    if (!p || p->done || !p->wth) return NULL;
    int err = 0; gchar *err_info = NULL; gint64 offset = 0;
    for (;;) {
        if (!wtap_read(p->wth, &p->rec, &p->buf, &err, &err_info, &offset)) {
            p->done = 1;
            if (err) {
                char what[64];
                snprintf(what, sizeof what, "Read error after frame %d", p->count);
                set_error(p, what, err, err_info);
            }
            g_free(err_info);
            return NULL;
        }
        if (p->rec.rec_type == REC_TYPE_PACKET) break;
        wtap_rec_reset(&p->rec);   /* skip non-packet records (pcapng metadata blocks) */
    }
    g_free(err_info);
    p->count++;

    guint caplen = p->rec.rec_header.packet_header.caplen;
    int encap = p->rec.rec_header.packet_header.pkt_encap;
    const guint8 *data = ws_buffer_start_ptr(&p->buf);
    int has_ts = (p->rec.presence_flags & WTAP_HAS_TS) ? 1 : 0;
    const char *encap_name = wtap_encap_description(encap);

    GString *out = g_string_new(NULL);
    g_string_append_printf(out, "#frame\t%d\t%d\t%s\t%u\t%u\t%d\t%lld\t%d\t%lld\n",
        p->count, encap, encap_name ? encap_name : "unknown",
        caplen, p->rec.rec_header.packet_header.len, has_ts,
        has_ts ? (long long)p->rec.ts.secs : 0LL, has_ts ? p->rec.ts.nsecs : 0,
        (long long)offset);
    g_string_append(out, "#hex\t");
    guint nhex = caplen < LTWS_HEX_MAX ? caplen : LTWS_HEX_MAX;
    for (guint i = 0; i < nhex; i++) g_string_append_printf(out, "%02x", data[i]);
    g_string_append_c(out, '\n');

    static struct packet_provider_funcs funcs = { .get_frame_ts = pp_ts };
    pthread_mutex_lock(&g_dissect);
    epan_t *session = epan_new(NULL, &funcs);
    epan_dissect_t *edt = epan_dissect_new(session, TRUE, TRUE);
    frame_data fdata; memset(&fdata, 0, sizeof fdata);
    frame_data_init(&fdata, p->count, &p->rec, offset, 0);
    tvbuff_t *tvb = tvb_new_real_data(data, caplen, caplen);
    epan_dissect_run(edt, wtap_file_type_subtype(p->wth), &p->rec, tvb, &fdata, NULL);
    tree_ctx ctx = { out, 0 };
    proto_tree_children_foreach(edt->tree, tree_node, &ctx);
    epan_dissect_free(edt);
    frame_data_destroy(&fdata);
    epan_free(session);
    pthread_mutex_unlock(&g_dissect);

    wtap_rec_reset(&p->rec);
    return g_string_free(out, FALSE);
}

const char *ltws_pcap_error(const ltws_pcap *p) { return (p && p->error[0]) ? p->error : NULL; }
const char *ltws_pcap_file_type(const ltws_pcap *p) { return p ? p->file_type : "unknown"; }
int64_t ltws_pcap_file_size(const ltws_pcap *p) { return p ? p->size : -1; }

void ltws_pcap_close(ltws_pcap *p) {
    if (!p) return;
    if (p->wth) {
        ws_buffer_free(&p->buf);
        wtap_rec_cleanup(&p->rec);
        wtap_close(p->wth);
    }
    g_free(p);
}

void ltws_free(char *s) { g_free(s); }
