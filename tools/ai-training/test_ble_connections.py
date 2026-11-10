import tempfile
import unittest
from pathlib import Path
from audit_ble_connections import audit_stream


class BleConnectionTests(unittest.TestCase):
    def test_publisher_count_mismatch_is_not_silently_accepted(self):
        with tempfile.TemporaryDirectory() as temporary:
            path=Path(temporary)/'stream.csv'
            path.write_text('t_a,LLID1,LLID0,NESN,SN,MD,CP,RFU1,RFU0,Length\n0.1,1,0,0,0,0,0,0,0,0\n')
            with self.assertRaisesRegex(ValueError,'publisher'):audit_stream(path,2)
            self.assertEqual(1,audit_stream(path,1)['records'])

    def test_clock_reversal_is_recorded_without_reordering_source(self):
        with tempfile.TemporaryDirectory() as temporary:
            path=Path(temporary)/'stream.csv'
            path.write_text('t_a,LLID1,LLID0,NESN,SN,MD,CP,RFU1,RFU0,Length\n0.2,1,0,0,0,0,0,0,0,0\n0.1,1,0,0,0,0,0,0,0,0\n')
            self.assertEqual(1,audit_stream(path,2)['descendingTimestampTransitions'])


if __name__=='__main__':unittest.main()
