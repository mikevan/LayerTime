"""Session-held-out BLE pilots: supervised CatBoost and benign-only autoencoder ensemble."""
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np

GROUPS=((0,1,2,3,4),(5,6,7,8,9,10),(11,12,13,14,15))

def session_weights(groups):
    names,counts=np.unique(groups,return_counts=True)
    lookup=dict(zip(names,counts))
    return np.array([1/lookup[g] for g in groups])*len(groups)/len(names)

class AutoencoderEnsemble:
    """Kitsune-inspired fixed feature groups; not the original KitNET implementation."""
    def __init__(self,seed=42,epochs=40):self.seed=seed;self.epochs=epochs
    def fit(self,x):
        import torch
        torch.manual_seed(self.seed);torch.set_num_threads(2)
        self.mean=x.mean(axis=0);self.scale=np.maximum(x.std(axis=0),0.01)
        z=np.clip((x-self.mean)/self.scale,-8,8).astype('float32')
        self.models=[]
        def train(data):
            d=data.shape[1]
            model=torch.nn.Sequential(torch.nn.Linear(d,max(1,(d+1)//2)),torch.nn.ReLU(),torch.nn.Linear(max(1,(d+1)//2),d))
            optimizer=torch.optim.Adam(model.parameters(),lr=0.003)
            tensor=torch.from_numpy(data)
            for _ in range(self.epochs):
                for indices in torch.randperm(len(tensor)).split(256):
                    batch=tensor[indices];loss=(model(batch)-batch).square().mean()
                    optimizer.zero_grad();loss.backward();optimizer.step()
            return model.eval()
        for group in GROUPS:self.models.append(train(z[:,group]))
        self.output=train(self._errors(z))
        return self
    def _errors(self,z):
        import torch
        with torch.no_grad():
            return np.stack([
                np.mean((m(torch.from_numpy(z[:,g])).numpy()-z[:,g])**2,axis=1) for m,g in zip(self.models,GROUPS)],axis=1).astype('float32')
    def score(self,x):
        import torch
        z=np.clip((x-self.mean)/self.scale,-8,8).astype('float32');errors=self._errors(z)
        with torch.no_grad():output=self.output(torch.from_numpy(errors)).numpy()
        return np.mean((output-errors)**2,axis=1)
    def save(self,path):
        arrays={'mean':self.mean,'scale':self.scale}
        for i,model in enumerate(self.models+[self.output]):
            for name,tensor in model.state_dict().items():arrays[f'model{i}_{name}']=tensor.numpy()
        np.savez_compressed(path,**arrays)

def score_saved_autoencoders(path,x):
    """Portable NumPy reference for validating a later phone implementation."""
    arrays=np.load(path)
    z=np.clip((x-arrays['mean'])/arrays['scale'],-8,8).astype('float32')
    def forward(i,v):
        hidden=np.maximum(v@arrays[f'model{i}_0.weight'].T+arrays[f'model{i}_0.bias'],0)
        return hidden@arrays[f'model{i}_2.weight'].T+arrays[f'model{i}_2.bias']
    errors=np.stack([np.mean((forward(i,z[:,g])-z[:,g])**2,axis=1) for i,g in enumerate(GROUPS)],axis=1)
    return np.mean((forward(len(GROUPS),errors)-errors)**2,axis=1)

def metrics(y,score,threshold):
    from sklearn.metrics import confusion_matrix,roc_auc_score,average_precision_score
    p=score>=threshold;tn,fp,fn,tp=confusion_matrix(y,p,labels=[0,1]).ravel()
    both=len(np.unique(y))==2
    return {'windows':len(y),'tn':int(tn),'fp':int(fp),'fn':int(fn),'tp':int(tp),
            'recall':float(tp/(tp+fn)) if tp+fn else None,
            'falsePositiveFraction':float(fp/(fp+tn)) if fp+tn else None,
            'rocAuc':float(roc_auc_score(y,score)) if both else None,
            'averagePrecision':float(average_precision_score(y,score)) if both else None}

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--features',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
    from catboost import CatBoostClassifier
    args.output.mkdir(parents=True,exist_ok=False)
    manifest=json.loads((args.features/'manifest.json').read_text())
    if len({s['sha256'] for s in manifest['sessions']})!=len(manifest['sessions']):
        raise ValueError('Duplicate source capture content cannot form independent session groups')
    dataset=np.load(args.features/'features.npz');x=dataset['x'];y=dataset['y'];groups=dataset['groups']
    if not np.isfinite(x).all():raise ValueError('Nonfinite features')
    sessions=np.unique(groups)
    if len(sessions)<4:raise ValueError('Insufficient session groups')
    predictions=[];folds=[]
    def classifier():return CatBoostClassifier(iterations=300,depth=5,learning_rate=0.05,
        loss_function='Logloss',auto_class_weights='Balanced',random_seed=42,thread_count=4,
        verbose=False,allow_writing_files=False)
    for held in sessions:
        test=groups==held;train=~test
        if set(np.unique(y[train]))!={0,1}:raise ValueError('Fold has no independent training example for a class')
        clf=classifier();clf.fit(x[train],y[train],sample_weight=session_weights(groups[train]))
        cls=clf.predict_proba(x[test])[:,1]
        normal=train & (y==0)
        ae=AutoencoderEnsemble().fit(x[normal])
        # A train-normal percentile is a provisional threshold, not calibrated field confidence.
        threshold=float(np.quantile(ae.score(x[normal]),0.995))
        anomaly=ae.score(x[test])
        fold={'heldOutSession':str(held),'catboost':metrics(y[test],cls,0.5),
              'autoencoder':metrics(y[test],anomaly,threshold),'anomalyThreshold':threshold,
              'trainingSessions':sessions[sessions!=held].tolist(),
              'normalTrainingSessions':np.unique(groups[normal]).tolist()}
        folds.append(fold)
        for index,c,a in zip(np.flatnonzero(test),cls,anomaly):
            predictions.append({'row':int(index),'session':str(held),'label':int(y[index]),
                                'catboostScore':float(c),'anomalyScore':float(a),'anomalyThreshold':threshold})
        print(json.dumps(fold),flush=True)
    clf=classifier();clf.fit(x,y,sample_weight=session_weights(groups));clf.save_model(str(args.output/'ble-classifier.cbm'))
    ae=AutoencoderEnsemble().fit(x[y==0]);ae.save(args.output/'ble-autoencoder-ensemble.npz')
    aggregate={}
    for name in ('catboost','autoencoder'):
        totals={key:sum(f[name][key] for f in folds) for key in ('tn','fp','fn','tp')}
        totals['recall']=totals['tp']/(totals['tp']+totals['fn'])
        totals['falsePositiveFraction']=totals['fp']/(totals['fp']+totals['tn'])
        aggregate[name]=totals
    report={'schemaVersion':1,'purpose':'public-single-testbed BLE pilot','releaseEligible':False,
        'featuresManifestSha256':hashlib.sha256((args.features/'manifest.json').read_bytes()).hexdigest(),
        'features':manifest['features'],'folds':folds,'aggregateHeldOutWindows':aggregate,
        'supervisedTraining':'CatBoost, session-balanced weights, fixed hyperparameters',
        'autoencoderTraining':'Three fixed feature-group autoencoders plus an output autoencoder; benign sessions only',
        'limitations':manifest['limitations']+['Leave-one-session-out is not independent-environment evaluation.',
            'No field probability calibration; thresholds are experimental.',
            'Invalid advertisement structures were excluded in feature version v1; this is not complete BLE anomaly coverage.',
            'Only two benign sessions are available; they cannot supply independent training, calibration and test sets.',
            'Final saved models use all sessions; reported metrics are from separate held-out fold models.',
            'These models are not bundled into or enabled in the Scope.']}
    (args.output/'report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    with (args.output/'held-out-predictions.jsonl').open('w') as stream:
        for row in sorted(predictions,key=lambda r:r['row']):stream.write(json.dumps(row)+'\n')
    print(json.dumps({'complete':True,'windows':len(y),'sessions':len(sessions),'releaseEligible':False}),flush=True)

if __name__=='__main__':main()
