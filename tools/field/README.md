# Compare field records

Independent records let people check what their tools actually observed and preserved. This comparison exposes disagreement between Hunter, Vixen, and T-Ultra so findings can be examined against the underlying evidence.

Keep the original records and reports private. This tool reads copies; it does
not upload data, synchronize clocks, assess model correctness, or declare a
field test passed.

Copy Hunter's log and `ev_NNNN` directories, Vixen's exported observation JSONL,
and T-Ultra's `/recon_field_v1.csv`. The legacy watch CSV is not supported.

```powershell
python tools/field/compare_records.py --hunter private/hunter_0001.log --vixen private/observations.jsonl --ultra private/recon_field_v1.csv --hunter-evidence-root private/hunter-card --output private/comparison-01.json
```

Choose a new output filename for each report. Select `--hunter-boot` and
`--ultra-boot` when a file contains multiple boots. New Hunter logs record the
phone link's session; older logs require an independently verified decimal
`--vixen-session`. Conflicting identities stop the comparison.

Hunter/Vixen comparison uses individual event IDs before phone display grouping.
Different counts, missing text, transport gaps, no-SD status and evidence payload
differences remain in the report. A requested evidence window may differ from an
earlier saved window while collection continues. Phone receipt-to-analysis time
does not include capture or transport latency.

Independent receivers require a measured clock offset and common coverage. Add
`--ultra-offset-ms`, `--start-ms`, `--end-ms` and one or more `--common-band`
values using the band names in the files. The offset means Ultra uptime plus
offset equals Hunter uptime. Without these inputs the radio comparison remains
unaligned. Local event IDs, counts and RSSI are not shared between receivers.

Run the hardware-free regression tests with:

```powershell
python -m unittest discover -s tools/field -p "test_*.py"
```

Successful parsing or matching observations does not qualify the AI. Preserve
raw model responses and have a person check each explanation against its input.
