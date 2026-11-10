import unittest
try:
    import torch
except ModuleNotFoundError:
    torch=None

@unittest.skipIf(torch is None,'Run model tests in the dedicated training environment')
class CompletionLossTests(unittest.TestCase):
    def test_selected_logits_match_full_causal_loss_and_gradients(self):
        from transformers import Qwen3Config,Qwen3ForCausalLM
        from completion_loss import completion_loss
        torch.manual_seed(4)
        config=Qwen3Config(vocab_size=97,hidden_size=32,intermediate_size=64,num_hidden_layers=1,
                          num_attention_heads=4,num_key_value_heads=2,head_dim=8)
        full=Qwen3ForCausalLM(config).eval()
        selected=Qwen3ForCausalLM(config).eval();selected.load_state_dict(full.state_dict())
        ids=torch.tensor([[1,2,3,4,5,6]])
        labels=torch.tensor([[-100,-100,-100,4,5,6]])
        inputs=dict(input_ids=ids,attention_mask=torch.ones_like(ids),labels=labels)
        reference=full(**inputs).loss;reference.backward()
        reduced,outputs=completion_loss(selected,inputs);reduced.backward()
        self.assertTrue(torch.allclose(reference,reduced,atol=1e-6))
        self.assertEqual(outputs.logits.shape[1],3)
        for a,b in zip(full.parameters(),selected.parameters()):
            if a.grad is not None:self.assertTrue(torch.allclose(a.grad,b.grad,atol=1e-5))
    def test_empty_completion_is_rejected(self):
        from completion_loss import completion_loss
        with self.assertRaisesRegex(ValueError,'no supervised'):
            completion_loss(None,dict(input_ids=torch.tensor([[1,2]]),labels=torch.tensor([[-100,-100]])))

    @unittest.skipIf(torch is None or not torch.cuda.is_available(),'CUDA kernel comparison')
    def test_expanded_grouped_heads_match_reference_on_efficient_kernel(self):
        from torch.nn.attention import sdpa_kernel,SDPBackend
        from transformers.integrations.sdpa_attention import repeat_kv
        torch.manual_seed(7)
        q=torch.randn(1,4,16,32,device='cuda',dtype=torch.float16,requires_grad=True)
        k=torch.randn(1,2,16,32,device='cuda',dtype=torch.float16,requires_grad=True)
        v=torch.randn_like(k,requires_grad=True)
        with sdpa_kernel(SDPBackend.MATH):
            expected=torch.nn.functional.scaled_dot_product_attention(q,k,v,is_causal=True,enable_gqa=True)
        expected.sum().backward();gradients=[x.grad.clone() for x in (q,k,v)]
        for x in (q,k,v):x.grad=None
        with sdpa_kernel(SDPBackend.EFFICIENT_ATTENTION):
            actual=torch.nn.functional.scaled_dot_product_attention(q,repeat_kv(k,2),repeat_kv(v,2),is_causal=True)
        actual.sum().backward()
        self.assertTrue(torch.allclose(expected,actual,atol=0.003,rtol=0.003))
        for x,g in zip((q,k,v),gradients):self.assertTrue(torch.allclose(g,x.grad,atol=0.006,rtol=0.006))

if __name__=='__main__':unittest.main()
