"""Numerical diagnostics; optimizer iterations are distinct from applied updates."""
import math


def clip_finite_gradients(parameters, max_norm, norm_type=2):
    """Single-device clipping with a float64 norm; reject corruption before mutation."""
    import torch
    if norm_type != 2 or not math.isfinite(max_norm) or max_norm <= 0:
        raise ValueError('Only a positive finite L2 clipping limit is supported')
    gradients=[p.grad for p in parameters if p.grad is not None]
    if not gradients:
        raise ValueError('No gradients were available for the optimizer update')
    norms=torch.stack([torch.linalg.vector_norm(g.detach().to(torch.float64)) for g in gradients])
    total=torch.linalg.vector_norm(norms)
    if not bool(torch.isfinite(total)):
        raise FloatingPointError('Nonfinite unscaled gradients; stopping before optimizer update')
    coefficient=torch.clamp(max_norm/(total+1e-6),max=1.0)
    for gradient in gradients:
        gradient.mul_(coefficient.to(device=gradient.device,dtype=gradient.dtype))
    return total


def summarize_steps(steps):
    applied=sum(not step['optimizerStepSkipped'] for step in steps)
    return dict(recordedIterations=len(steps),appliedOptimizerUpdates=applied,
                skippedOptimizerUpdates=len(steps)-applied,
                note='Mixed-precision loss scaling may skip updates after overflow. Requested iterations are not proof of applied updates.')


def inspect_adapter(path):
    import torch
    from safetensors.torch import load_file
    weights=load_file(str(path))
    bad=[name for name,value in weights.items() if not bool(torch.isfinite(value).all())]
    return dict(tensors=len(weights),allWeightsFinite=not bad,nonfiniteTensors=bad,
                nonzeroLoraBTensors=sum(bool(torch.count_nonzero(v)) for k,v in weights.items() if 'lora_B' in k))
