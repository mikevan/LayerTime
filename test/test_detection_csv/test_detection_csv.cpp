// Unit tests for src/core/logic/DetectionCsv, moved out of
// WatchApp::logReconDetection in Phase 0 Step 3i. test_detection_log still
// checks the rows WatchApp writes; this suite checks the format directly.

#include "check.h"

#include <cstring>

#include "core/logic/DetectionCsv.h"

using namespace layertime::detection_log;

namespace {
RowFields sample()
{
    RowFields f;
    f.year = 2026; f.month = 9; f.day = 8; f.hour = 7; f.minute = 5; f.second = 3;
    f.category = "PWNAGOTCHI";
    f.detail = "Pwnagotchi beacon";
    f.address = "DE:AD:BE:EF:DE:AD";
    f.rssi = -55;
    f.channel = 11;
    f.confidence = "HIGH";
    return f;
}
} // namespace

void path_and_header()
{
    CHECK_STR("/recon_log.csv", kPath);
    CHECK_STR("timestamp,category,detail,address,rssi,channel,confidence", kHeader);
    CHECK_INT(160, kRowBufferSize);
}

void row_is_zero_padded_and_comma_separated()
{
    char row[kRowBufferSize];
    const int n = formatRow(sample(), row, sizeof(row));
    CHECK_STR("2026-09-08 07:05:03,PWNAGOTCHI,Pwnagotchi beacon,DE:AD:BE:EF:DE:AD,-55,11,HIGH", row);
    CHECK_INT(static_cast<long>(strlen(row)), n);
}

void fields_are_written_as_is_KNOWN_DEFECT()
{
    RowFields f = sample();
    f.detail = "SSID a,\"b\"";
    char row[kRowBufferSize];
    formatRow(f, row, sizeof(row));
    CHECK_STR("2026-09-08 07:05:03,PWNAGOTCHI,SSID a,\"b\",DE:AD:BE:EF:DE:AD,-55,11,HIGH", row);
}

void longest_real_row_fits_the_buffer()
{
    // Longer than anything the recorder stores (it keeps 13-character
    // categories, 39-character details, and 18-character addresses).
    RowFields f = sample();
    f.category = "COUNTER-INTRUSION";
    f.detail = "012345678901234567890123456789012345678";
    f.address = "aa:bb:cc:dd:ee:ff";
    f.rssi = -128;
    f.channel = 255;
    f.confidence = "HIGH";
    char row[kRowBufferSize];
    const int n = formatRow(f, row, sizeof(row));
    CHECK_TRUE(n < static_cast<int>(kRowBufferSize));
}

void truncates_like_snprintf()
{
    char row[10];
    const int n = formatRow(sample(), row, sizeof(row));
    CHECK_STR("2026-09-0", row);
    CHECK_TRUE(n > 9);
}

int main(int argc, char **argv)
{
    CHECK_MAIN(argc, argv);
    CASE(path_and_header);
    CASE(row_is_zero_padded_and_comma_separated);
    CASE(fields_are_written_as_is_KNOWN_DEFECT);
    CASE(longest_real_row_fits_the_buffer);
    CASE(truncates_like_snprintf);
    CHECK_SUMMARY();
}
