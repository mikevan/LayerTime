import unittest

class FrozenCpuLayersTest(unittest.TestCase):
    def test_transfers_preserve_body_gradients_and_frozen_matrices(self):
        import torch
        from frozen_cpu_layers import offload_frozen_io
        class Tiny(torch.nn.Module):
            def __init__(self):
                super().__init__();self.embedding=torch.nn.Embedding(7,4);self.body=torch.nn.Linear(4,4);self.head=torch.nn.Linear(4,7)
            def get_input_embeddings(self):return self.embedding
            def get_output_embeddings(self):return self.head
            def forward(self,x):return self.head(self.body(self.embedding(x)))
        torch.manual_seed(7);model=Tiny()
        for p in list(model.embedding.parameters())+list(model.head.parameters()):p.requires_grad_(False)
        ids=torch.tensor([[1,2,3]]);expected=model(ids).detach().clone()
        offload_frozen_io(model);actual=model(ids)
        torch.testing.assert_close(actual,expected)
        actual.sum().backward()
        self.assertTrue(torch.isfinite(model.body.weight.grad).all())
        self.assertGreater(float(model.body.weight.grad.abs().sum()),0)
        self.assertIsNone(model.embedding.weight.grad);self.assertIsNone(model.head.weight.grad)

if __name__=='__main__':unittest.main()
