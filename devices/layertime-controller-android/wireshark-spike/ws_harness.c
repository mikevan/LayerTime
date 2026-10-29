/* Minimal libwireshark/epan embedding harness: the JNI core.
 * Dissects raw 802.11 bytes directly via epan (no tshark CLI), prints chosen fields.
 * Proves the libwireshark C API path that an Android JNI layer would call. */
#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <epan/epan.h>
#include <epan/epan_dissect.h>
#include <epan/proto.h>
#include <epan/tvbuff.h>
#include <epan/packet.h>
#include <epan/print.h>
#include <wiretap/wtap.h>
#include <wsutil/privileges.h>
#include <wsutil/wslog.h>

/* packet provider callbacks: single standalone frame, so all return NULL/none */
static const nstime_t *pp_get_ts(struct packet_provider_data *prov _U_, guint32 n _U_) { return NULL; }
static const char *pp_get_if_name(struct packet_provider_data *prov _U_, guint32 i _U_) { return NULL; }
static const char *pp_get_if_descr(struct packet_provider_data *prov _U_, guint32 i _U_) { return NULL; }
static wtap_block_t pp_get_block(struct packet_provider_data *prov _U_, const frame_data *fd _U_) { return NULL; }
static epan_t *make_session(void) {
    static struct packet_provider_funcs funcs;
    memset(&funcs, 0, sizeof(funcs));
    funcs.get_frame_ts = pp_get_ts;
    funcs.get_interface_name = pp_get_if_name;
    funcs.get_interface_description = pp_get_if_descr;
    funcs.get_modified_block = pp_get_block;
    return epan_new(NULL, &funcs);
}

static void dump_field(epan_dissect_t *edt, const char *name) {
    int hf = proto_registrar_get_id_byname(name);
    if (hf <= 0) { printf("  %-24s : <field not registered>\n", name); return; }
    GPtrArray *arr = proto_find_finfo(edt->tree, hf);
    if (!arr || arr->len == 0) { printf("  %-24s : <absent>\n", name); if(arr) g_ptr_array_free(arr,TRUE); return; }
    for (guint i = 0; i < arr->len; i++) {
        field_info *fi = (field_info *)g_ptr_array_index(arr, i);
        char *s = fvalue_to_string_repr(NULL, &fi->value, FTREPR_DISPLAY, fi->hfinfo->display);
        printf("  %-24s : %s\n", name, s ? s : "(null)");
        wmem_free(NULL, s);
    }
    g_ptr_array_free(arr, TRUE);
}

int main(void) {
    /* raw 802.11 deauth frame (same bytes as the pcap fixture) */
    static const guint8 frame[] = {
        0xc0,0x00, 0x3a,0x01, 0xff,0xff,0xff,0xff,0xff,0xff,
        0x00,0x11,0x22,0x33,0xaa,0xbb, 0x00,0x11,0x22,0x33,0xaa,0xbb,
        0x70,0x00, 0x07,0x00 };
    guint len = (guint)sizeof(frame);

    ws_log_init("ws_harness", NULL);
    init_process_policies();
    wtap_init(FALSE);
    fprintf(stderr,"[cp] before epan_init\n");
    if (!epan_init(NULL, NULL, FALSE)) { fprintf(stderr, "epan_init failed\n"); return 2; }
    fprintf(stderr,"[cp] epan_init ok\n");

    epan_t *session = make_session();
    fprintf(stderr,"[cp] session=%p\n",(void*)session);
    epan_dissect_t *edt = epan_dissect_new(session, TRUE, TRUE);
    fprintf(stderr,"[cp] edt=%p\n",(void*)edt);

    wtap_rec rec; memset(&rec, 0, sizeof(rec));
    rec.rec_type = REC_TYPE_PACKET;
    rec.presence_flags = 0;
    rec.rec_header.packet_header.caplen = len;
    rec.rec_header.packet_header.len = len;
    rec.rec_header.packet_header.pkt_encap = WTAP_ENCAP_IEEE_802_11;

    frame_data fdata; memset(&fdata, 0, sizeof(fdata));
    frame_data_init(&fdata, 1, &rec, 0, 0);

    tvbuff_t *tvb = tvb_new_real_data(frame, len, len);
    fprintf(stderr,"[cp] before dissect_run tvb=%p\n",(void*)tvb);
    epan_dissect_run(edt, WTAP_FILE_TYPE_SUBTYPE_UNKNOWN, &rec, tvb, &fdata, NULL);
    fprintf(stderr,"[cp] dissect_run ok\n");

    printf("=== libwireshark embedding harness: decoded fields ===\n");
    dump_field(edt, "wlan.fc.type_subtype");
    dump_field(edt, "wlan.fc.type");
    dump_field(edt, "wlan.fc.subtype");
    dump_field(edt, "wlan.da");
    dump_field(edt, "wlan.sa");
    dump_field(edt, "wlan.bssid");
    dump_field(edt, "wlan.seq");
    dump_field(edt, "wlan.fixed.reason_code");

    epan_dissect_free(edt);
    epan_free(session);
    epan_cleanup();
    return 0;
}
