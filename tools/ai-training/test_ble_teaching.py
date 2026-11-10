import unittest
import numpy as np
from ble_features import FEATURES
from build_ble_teaching import select_rows


class BleTeachingTests(unittest.TestCase):
    def fixture(self):
        x=np.zeros((7,len(FEATURES)));x[:,FEATURES.index('payload_repeat_fraction')]=[0.9,0.8,0.7,0.6,0.5,0.4,1]
        y=np.array([0,0,0,0,0,0,1]);groups=np.array(['a']*3+['b']*3+['c'])
        rows=[dict(row=i,label=int(y[i]),session=groups[i],catboostScore=0.1,anomalyScore=2,anomalyThreshold=1) for i in range(7)]
        return x,y,groups,rows

    def test_selections_are_distinct_and_only_from_benign_recordings(self):
        selected=select_rows(*self.fixture())
        self.assertEqual(6,len(selected));self.assertEqual(6,len({r[2] for r in selected}))
        self.assertNotIn(6,[r[2] for r in selected]);self.assertEqual({'a','b'},{r[0] for r in selected})

    def test_wrong_session_and_duplicate_rows_are_rejected(self):
        args=self.fixture();args[3][0]['session']='wrong'
        with self.assertRaises(ValueError):select_rows(*args)
        args=self.fixture();args[3].append(dict(args[3][0]))
        with self.assertRaises(ValueError):select_rows(*args)

    def test_disagreement_is_required_not_invented(self):
        args=self.fixture()
        for r in args[3]:r['anomalyScore']=0
        with self.assertRaisesRegex(ValueError,'disagreement'):select_rows(*args)


if __name__=='__main__':unittest.main()
