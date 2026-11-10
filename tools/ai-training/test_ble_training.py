import tempfile
import unittest
from pathlib import Path
import numpy as np
from train_ble_specialists import AutoencoderEnsemble,metrics,session_weights,score_saved_autoencoders

class BleTrainingTests(unittest.TestCase):
    def test_long_sessions_do_not_outweigh_short_sessions(self):
        groups=np.array(['a','a','a','b'])
        weights=session_weights(groups)
        self.assertAlmostEqual(weights[groups=='a'].sum(),weights[groups=='b'].sum())
    def test_single_class_metrics_do_not_claim_auc(self):
        result=metrics(np.array([0,0]),np.array([0.2,0.7]),0.5)
        self.assertIsNone(result['rocAuc']);self.assertEqual(1,result['fp']);self.assertEqual(1,result['tn'])
    def test_exported_weights_reproduce_ensemble_scores(self):
        x=np.random.default_rng(1).normal(size=(24,16)).astype('float32')
        model=AutoencoderEnsemble(epochs=2).fit(x[:16])
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'ensemble.npz';model.save(path)
            np.testing.assert_allclose(model.score(x[16:]),score_saved_autoencoders(path,x[16:]),rtol=1e-5,atol=1e-6)

if __name__=='__main__':unittest.main()
