import unittest
import numpy as np
from ble_features import decode_advertisement,summarize,FEATURES

SAMPLE=bytes.fromhex('0821000365fc020a01253c0000c3cd492dd6be898e420e47b6768610f307ff4c0012020001220821')

class BleFeaturesTests(unittest.TestCase):
    def test_decodes_nordic_v3_advertisement(self):
        packet,reason=decode_advertisement(SAMPLE)
        self.assertIsNone(reason);self.assertEqual(2,packet['kind']);self.assertEqual(37,packet['channel'])
        self.assertTrue(packet['random']);self.assertTrue(packet['manufacturer'])
        self.assertEqual(bytes.fromhex('47b6768610f3'),packet['address'])
    def test_rejects_bad_crc_truncated_record_and_unhandled_linktype(self):
        bad=bytearray(SAMPLE);bad[8]=0
        self.assertEqual('crc_failed',decode_advertisement(bytes(bad))[1])
        self.assertIsNone(decode_advertisement(SAMPLE[:-1])[0])
        self.assertEqual('unsupported_linktype',decode_advertisement(SAMPLE,1)[1])
    def test_identity_renaming_preserves_model_features(self):
        a,_=decode_advertisement(SAMPLE)
        renamed=dict(a,address=b'abcdef')
        x,_=summarize([(1.0,a),(1.1,a)],10,{a['address']})
        y,_=summarize([(1.0,renamed),(1.1,renamed)],10,{renamed['address']})
        np.testing.assert_array_equal(x,y);self.assertEqual(len(FEATURES),len(x))
    def test_constant_timestamps_do_not_produce_nan(self):
        a,_=decode_advertisement(SAMPLE)
        x,_=summarize([(1.0,a),(1.0,a)],10,set())
        self.assertTrue(np.isfinite(x).all())

if __name__=='__main__':unittest.main()
