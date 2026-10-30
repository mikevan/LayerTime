"""Synthesizes the clunk and groove thud for the face's roll sounds; see the note at the end for the gravel.
Originally synthesized the whole roll sounds (app/src/main/res/raw/face_slide_out.wav and
face_slide_in.wav), 22.05 kHz mono 16-bit, with numpy. Seeded, so it is reproducible.
Out: a disc rolling over gravel on a rough metal deck for the 380 ms roll (random short
filtered noise grains plus low rumble), then a heavy metal clunk against a stop (a low
body that settles in pitch, an impact click, and inharmonic metal partials ringing out).
In: the same gravel, then a light damped thud as it drops into a fitted groove.
Run from any folder; it writes the two WAV files into the current folder."""
import numpy as np, wave
SR=22050
rng=np.random.default_rng(29)
def bp(x, lo, hi):
    X=np.fft.rfft(x); f=np.fft.rfftfreq(len(x),1/SR)
    X[(f<lo)|(f>hi)]=0; return np.fft.irfft(X,len(x))
def gravel(dur, rate, amp, swell=True):
    n=int(dur*SR); y=np.zeros(n)
    t=0.0
    while True:
        t+=rng.exponential(1/rate)
        i=int(t*SR)
        if i>=n: break
        L=int(rng.uniform(0.002,0.010)*SR)
        g=rng.standard_normal(L)*np.exp(-np.linspace(0,rng.uniform(4,9),L))
        a=rng.uniform(0.2,1.0)
        y[i:i+L]+=a*g[:max(0,min(L,n-i))]
    pad=int(0.05*SR); y=bp(np.concatenate([y,np.zeros(pad)]),900,7000)[:n]
    # low rumble of the rolling disc on a rough deck
    rum=bp(rng.standard_normal(n),40,220)*0.6
    env=np.ones(n)
    if swell: env=np.minimum(1,np.linspace(0,1,n)*6)*np.minimum(1,np.linspace(1,0,n)*10)
    y=(y/np.max(np.abs(y))*0.8+rum/np.max(np.abs(rum))*0.35)*env
    return y*amp
def thud(freq, dur, amp, ring, ringamp, noise=0.5):
    n=int(dur*SR); t=np.arange(n)/SR
    # body: pitch drops a little as the impact settles
    f=freq*(1+0.6*np.exp(-t*40))
    body=np.sin(2*np.pi*np.cumsum(f)/SR)*np.exp(-t*12)
    click=bp(rng.standard_normal(n),200,4000)*np.exp(-t*90)*noise
    metal=np.zeros(n)
    for k,(p,dec) in enumerate(ring):
        metal+=np.sin(2*np.pi*p*t+rng.uniform(0,6))*np.exp(-t*dec)/(k+1)**0.6
    att=np.minimum(1,t/0.002)
    y=(body*amp+click*amp+metal*ringamp)*att
    return y
def place(total, parts):
    y=np.zeros(int(total*SR))
    for start,s in parts:
        i=int(start*SR); y[i:i+len(s)]+=s[:len(y)-i]
    return y
def save(name,y):
    y=y/np.max(np.abs(y))*0.92
    fade=int(0.02*SR); y[-fade:]*=np.linspace(1,0,fade)
    w=wave.open(name,'wb'); w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR)
    w.writeframes((y*32767).astype(np.int16).tobytes()); w.close()
# OUT: gravel for the 380 ms roll, then a heavy metal clunk against a stop
out=place(0.95,[(0.0,gravel(0.40,320,0.55)),
               (0.37,thud(62,0.55,1.0,[(287,9),(611,11),(1093,14),(1747,18),(2633,24)],0.45,0.8))])
save('face_slide_out.wav',out)
# IN: gravel, then a light thud as it drops into a fitted groove (short, damped, little ring)
back=place(0.70,[(0.0,gravel(0.40,280,0.45)),
                 (0.385,thud(105,0.28,0.55,[(733,45),(1460,60)],0.08,0.35))])
save('face_slide_in.wav',back)


# ---- Build 51: real gravel ----
# The synthesized gravel sounded like static, so the gravel is now a real recording:
# "car tires start and stop on gravel shoulder" (Pixabay, file
# freesound_community-car-tires-start-and-stop-on-gravel-shoulder-6852.mp3, supplied by
# Michael). Out uses 2.0 to 3.0 s of the stop (clip 18.3 to 19.3 s), back uses 2.0 to 3.0 s of
# the forward roll (clip 27.0 to 28.0 s), each faded over 20 ms and peak-levelled, then mixed
# with the clunk or groove thud at 0.98 s. The mixing step, as run:
# import numpy as np, wave, subprocess
# exec(open('gen.py').read().split('# OUT:')[0])   # reuse thud(), save(), SR
# def load(path, a, b):
#     raw=subprocess.run(['ffmpeg','-v','error','-ss',str(a),'-to',str(b),'-i',path,'-ac','1','-ar',str(SR),'-f','s16le','-'],capture_output=True).stdout
#     y=np.frombuffer(raw,np.int16)/32768.0
#     f=int(0.02*SR); y=y.copy(); y[:f]*=np.linspace(0,1,f); y[-f:]*=np.linspace(1,0,f)
#     return y/np.max(np.abs(y))
# stop=load('/mnt/user-data/outputs/gravel_2_stop.wav',2.0,3.0)
# fwd=load('/mnt/user-data/outputs/gravel_3_forward.wav',2.0,3.0)
# clunk=thud(62,0.55,0.9,[(287,9),(611,11),(1093,14),(1747,18),(2633,24)],0.45,0.8)
# groove=thud(105,0.28,0.55,[(733,45),(1460,60)],0.08,0.35)
# def mix(g, t, hit_at, total, gamp):
#     y=np.zeros(int(total*SR)); y[:len(g)]+=g*gamp
#     i=int(hit_at*SR); y[i:i+len(t)]+=t[:len(y)-i]
#     return y
# save('face_slide_out.wav', mix(stop, clunk, 0.98, 1.55, 0.75))
# save('face_slide_in.wav', mix(fwd, groove, 0.98, 1.30, 0.65))
