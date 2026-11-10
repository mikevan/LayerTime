import unittest
from training_stability import summarize_steps, clip_finite_gradients


class TrainingStabilityTests(unittest.TestCase):
    def test_large_finite_gradients_have_finite_norm_and_remain_nonzero(self):
        import torch
        parameter=torch.nn.Parameter(torch.zeros(2))
        parameter.grad=torch.tensor([1e30,1e30])
        norm=clip_finite_gradients([parameter],1.0)
        self.assertTrue(torch.isfinite(norm))
        self.assertAlmostEqual(float(torch.linalg.vector_norm(parameter.grad)),1.0,places=5)
        self.assertTrue(bool((parameter.grad>0).all()))

    def test_nonfinite_gradients_fail_before_mutation(self):
        import torch
        parameter=torch.nn.Parameter(torch.zeros(2))
        parameter.grad=torch.tensor([1.0,float('inf')])
        with self.assertRaises(FloatingPointError):clip_finite_gradients([parameter],1.0)
        self.assertEqual(1.0,float(parameter.grad[0]))

    def test_skipped_iterations_are_not_counted_as_applied_updates(self):
        result=summarize_steps([dict(optimizerStepSkipped=False),dict(optimizerStepSkipped=True)])
        self.assertEqual(1,result['appliedOptimizerUpdates'])
        self.assertEqual(1,result['skippedOptimizerUpdates'])
        self.assertEqual(2,result['recordedIterations'])


if __name__=='__main__':unittest.main()
