"""Keep frozen embedding/output matrices on CPU while retaining differentiable transfers."""
def offload_frozen_io(model):
    import types
    import torch
    modules=[]
    for module in (model.get_input_embeddings(),model.get_output_embeddings()):
        if any(module is existing for existing in modules):continue
        modules.append(module);module.to('cpu')
        original=module.forward
        def forward(self,value,*args,_original=original,**kwargs):
            destination=value.device
            return _original(value.to('cpu'),*args,**kwargs).to(destination)
        module.forward=types.MethodType(forward,module)
    if torch.cuda.is_available():torch.cuda.empty_cache()
    return modules
