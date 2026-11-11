"""Compute causal completion loss without allocating vocabulary logits for masked prompt tokens."""
def completion_loss(model, inputs):
    import torch
    labels=inputs['labels']
    if labels.ndim!=2 or labels.shape[0]!=1:
        raise ValueError('The initial memory-bounded configuration requires batch size one')
    positions=torch.nonzero(labels[0,1:]!=-100,as_tuple=False).flatten()
    if positions.numel()==0:
        raise ValueError('The training example contains no supervised completion')
    outputs=model(input_ids=inputs['input_ids'],attention_mask=inputs.get('attention_mask'),
                  logits_to_keep=positions,use_cache=False)
    targets=labels[:,1:][:,positions].to(outputs.logits.device)
    loss=torch.nn.functional.cross_entropy(outputs.logits.float().reshape(-1,outputs.logits.shape[-1]),targets.reshape(-1))
    return loss,outputs
