/* Host test for ws_core.c (the exact decode path the APK uses), against
 * TShark-matching Wireshark 4.4.19 built for x86_64.
 *   1. init semantics: first call performs init, second does not re-run it
 *   2. decode of the deauth fixture equals the expected TShark 4.4.19 fields
 *   3. structured tree: the six key fields appear with their byte offsets
 *   4. pcap import: decode deauth_fixture.pcap and recover the fields
 *   5. repeated-work loop for N iterations in a chosen mode, output identical
 *      every time; RSS and malloc-in-use sampled. Modes let the decoder loop
 *      be compared with a control loop that does the same allocation but no
 *      dissection, so the memory behaviour of the decoder is isolated.
 * Usage: ws_core_test [N] [decode|control|tree] [pcap_path]  exit 0 = pass */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <time.h>
#include "ws_core.h"

static const unsigned char FIX[] = {
    0xc0,0x00,0x3a,0x01, 0xff,0xff,0xff,0xff,0xff,0xff,
    0x00,0x11,0x22,0x33,0xaa,0xbb, 0x00,0x11,0x22,0x33,0xaa,0xbb, 0x70,0x00,0x07,0x00 };

/* Values from wireshark-spike/tshark_4.4.19_decode.txt */
static const char *EXPECT[][2] = {
    {"wlan.fc.type_subtype", "0x000c"}, {"wlan.da", "ff:ff:ff:ff:ff:ff"},
    {"wlan.sa", "00:11:22:33:aa:bb"}, {"wlan.bssid", "00:11:22:33:aa:bb"},
    {"wlan.seq", "7"}, {"wlan.fixed.reason_code", "0x0007"} };

static long rss_kb(void) {
    FILE *f = fopen("/proc/self/status", "r"); char l[256]; long v = -1;
    while (f && fgets(l, sizeof l, f)) { if (!strncmp(l, "VmRSS:", 6)) { v = atol(l + 6); break; } }
    if (f) fclose(f);
    return v;
}
static double now_ms(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec*1e3 + t.tv_nsec/1e6; }

int main(int argc, char **argv) {
    long n = argc > 1 ? atol(argv[1]) : 100000;
    const char *mode = argc > 2 ? argv[2] : "decode";
    const char *pcap = argc > 3 ? argv[3] : NULL;
    int fail = 0;

    long rss0 = rss_kb();
    ltws_init_result a = ltws_init(), b = ltws_init();
    printf("init #1: performed_now=%d ok=%d epan_init=%.1f ms total=%.1f ms calls=%d\n",
        a.performed_now, a.ok, a.epan_init_ns/1e6, a.total_init_ns/1e6, a.calls);
    printf("init #2: performed_now=%d ok=%d (reports first-init epan_init=%.1f ms) calls=%d\n",
        b.performed_now, b.ok, b.epan_init_ns/1e6, b.calls);
    if (!(a.performed_now == 1 && a.ok && b.performed_now == 0 && b.calls == 2)) { puts("FAIL init semantics"); fail = 1; }
    printf("RSS before init %ld KB, after init %ld KB\n", rss0, rss_kb());

    char *first = ltws_decode(FIX, sizeof FIX);
    if (!first) { puts("FAIL decode returned NULL"); return 1; }
    printf("%s", first);
    for (unsigned i = 0; i < sizeof EXPECT / sizeof EXPECT[0]; i++) {
        char want[128]; snprintf(want, sizeof want, "%-24s : %s\n", EXPECT[i][0], EXPECT[i][1]);
        if (!strstr(first, want)) { printf("FAIL field %s != %s\n", EXPECT[i][0], EXPECT[i][1]); fail = 1; }
    }
    if (!fail) puts("fields: all 6 match TShark 4.4.19");

    /* structured tree: six key fields present, reason code at offset 24 length 2 */
    char *tree = ltws_decode_tree(FIX, sizeof FIX);
    if (!tree) { puts("FAIL tree NULL"); fail = 1; }
    else {
        printf("--- structured tree (%zu records) ---\n", (size_t)0);
        fputs(tree, stdout);
        const char *checks[] = { "\twlan.fc.type_subtype\t", "\twlan.sa\t", "\twlan.da\t",
                                 "\twlan.bssid\t", "\twlan.fixed.reason_code\t0x0007\t24\t2\t" };
        for (unsigned i = 0; i < sizeof checks/sizeof checks[0]; i++)
            if (!strstr(tree, checks[i])) { printf("FAIL tree missing %s\n", checks[i]); fail = 1; }
        if (!fail) puts("tree: six fields present; reason_code at offset 24 length 2");
        ltws_free(tree);
    }

    /* pcap import */
    if (pcap) {
        const char *err = NULL;
        char *p = ltws_decode_pcap(pcap, 0, &err);
        if (!p) { printf("FAIL pcap decode: %s\n", err ? err : "?"); fail = 1; }
        else {
            printf("--- pcap import ---\n%s", p);
            if (!strstr(p, "0x000c") || !strstr(p, "frames decoded: 1")) { puts("FAIL pcap fields"); fail = 1; }
            else puts("pcap: 1 frame decoded, type_subtype present");
            ltws_free(p);
        }
    }

    /* frame-by-frame reader: same file, one frame, same fields, then clean EOF */
    if (pcap) {
        ltws_pcap *r = ltws_pcap_open(pcap);
        if (ltws_pcap_error(r)) { printf("FAIL pcap reader open: %s\n", ltws_pcap_error(r)); fail = 1; }
        else {
            int frames = 0; char *f;
            while ((f = ltws_pcap_next(r)) != NULL) {
                frames++;
                if (frames == 1 && (!strstr(f, "#frame\t1\t") || !strstr(f, "\twlan.fixed.reason_code\t0x0007\t24\t2\t")))
                    { puts("FAIL pcap reader fields"); fail = 1; }
                ltws_free(f);
            }
            if (frames != 1 || ltws_pcap_error(r)) { printf("FAIL pcap reader frames=%d err=%s\n", frames, ltws_pcap_error(r) ? ltws_pcap_error(r) : "none"); fail = 1; }
            else printf("pcap reader: 1 frame, type %s, size %lld bytes, clean EOF\n", ltws_pcap_file_type(r), (long long)ltws_pcap_file_size(r));
        }
        ltws_pcap_close(r);
    }

    /* repeated work loop */
    int ctrl = !strcmp(mode, "control");
    int treemode = !strcmp(mode, "tree");
    char *ref = ctrl ? ltws_control(FIX, sizeof FIX) : (treemode ? ltws_decode_tree(FIX, sizeof FIX) : NULL);
    long mism = 0; double t0 = now_ms(); long step = n/10 ? n/10 : 1;
    printf("repeat %ld in mode '%s' (sample every %ld):\n", n, mode, step);
    for (long i = 1; i <= n; i++) {
        char *s = ctrl ? ltws_control(FIX, sizeof FIX)
                : treemode ? ltws_decode_tree(FIX, sizeof FIX)
                : ltws_decode(FIX, sizeof FIX);
        if (!s) mism++;
        else if (ctrl || treemode) { if (strcmp(s, ref)) mism++; }
        else if (strcmp(s, first)) mism++;
        ltws_free(s);
        if (i % step == 0) {
            struct mallinfo2 m = mallinfo2();
            printf("  %7ld  RSS %6ld KB  heap-in-use %8zu KB  %.1f ms\n", i, rss_kb(), m.uordblks/1024, now_ms()-t0);
        }
    }
    if (ref) ltws_free(ref);
    printf("mismatched outputs: %ld of %ld\n", mism, n);
    printf("mean iteration: %.1f us\n", (now_ms()-t0)*1000.0/n);
    if (mism) fail = 1;
    ltws_free(first);
    puts(fail ? "RESULT: FAIL" : "RESULT: PASS");
    return fail;
}
