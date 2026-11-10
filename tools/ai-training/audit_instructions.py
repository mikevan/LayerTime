"""Inspect public instruction data; selection is not factual or expert validation."""
import argparse
import collections
import hashlib
import json
import re
from pathlib import Path

TOPICS={
    'wifi':re.compile(r'\b(?:wi-?fi|wireless|802\.11|eapol|deauth\w*|wpa[23]?|ssid|bssid)\b',re.I),
    'ble':re.compile(r'\b(?:bluetooth|ble)\b',re.I),
    'network':re.compile(r'\b(?:network|traffic|packet|protocol|wireshark)\b',re.I),
    'investigation':re.compile(r'\b(?:incident|threat|detect\w*|forensic\w*|triage)\b',re.I),
}
DEFENSIVE=re.compile(r'\b(?:detect\w*|defen\w*|analy[sz]\w*|monitor\w*|investigat\w*|interpret\w*|identif\w*|explain|protect\w*|mitigat\w*|forensic\w*|triage)\b',re.I)
EXCLUDE=re.compile(r'\b(?:execut\w*|launch\w*|perform\w*|conduct\w*|craft\w*|weaponiz\w*|bypass\w*|evad\w*|exploit\w*|extract\w*|key\s+recovery|reverse\s+shell)\b',re.I)

def normalize_text(text):
    # Source prose contains literal multiply escaped paragraph separators. Do not
    # apply unicode_escape to arbitrary text, which corrupts non-ASCII characters.
    return re.sub(r'\\{2,}n','\n',text).replace('\r\n','\n').strip()

def inspect(row):
    if any(not isinstance(row.get(k),str) or not row[k].strip() for k in ('system','user','assistant')):
        return None,'missing_fields'
    question=normalize_text(row['user']);answer=normalize_text(row['assistant'])
    if '\ufffd' in question+answer:return None,'replacement_characters'
    if any(ord(c)<32 and c not in '\n\r\t' for c in question+answer):return None,'control_characters'
    topics=[name for name,pattern in TOPICS.items() if pattern.search(question)]
    if not topics:return None,'outside_selected_topics'
    if not DEFENSIVE.search(question) or EXCLUDE.search(question):return None,'requires_scope_review'
    if '```' in answer:return None,'code_requires_review'
    if len(answer)<120 or len(answer)>14000:return None,'answer_length'
    key=re.sub(r'[^\w]+',' ',question.casefold()).strip()
    return {'question':question,'answer':answer,'topics':topics,
            'questionHash':hashlib.sha256(key.encode()).hexdigest()},None

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--input',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
    args.output.mkdir(parents=True,exist_ok=False)
    rejected=collections.Counter();topic_counts=collections.Counter();schemas=collections.Counter()
    candidates=[];seen=set();total=0;escaped=0
    with args.input.open(encoding='utf-8') as stream:
        for number,line in enumerate(stream,1):
            if not line.strip():continue
            total+=1
            try:row=json.loads(line)
            except json.JSONDecodeError:rejected['invalid_json']+=1;continue
            schemas[','.join(sorted(row))]+=1
            for name,pattern in TOPICS.items():
                if pattern.search(row.get('user','')):topic_counts[name]+=1
            if re.search(r'\\{2,}n',row.get('assistant','')):escaped+=1
            selected,reason=inspect(row)
            if reason:rejected[reason]+=1;continue
            if selected['questionHash'] in seen:rejected['duplicate_question']+=1;continue
            seen.add(selected['questionHash'])
            selected.update(sourceLine=number,sourceId='trendyol-357544e',
                            labelStatus='automatically-selected-unreviewed',allowedForProductRelease=False)
            candidates.append(selected)
    with (args.output/'candidates.jsonl').open('w',encoding='utf-8') as stream:
        for row in candidates:stream.write(json.dumps(row,ensure_ascii=False)+'\n')
    # Keep a deterministic spread for human/factual review; none are automatically approved.
    review=sorted(candidates,key=lambda x:(0 if 'wifi' in x['topics'] or 'ble' in x['topics'] else 1,x['questionHash']))[:100]
    (args.output/'review-queue.json').write_text(json.dumps(review,indent=2,ensure_ascii=False),encoding='utf-8')
    with args.input.open('rb') as stream:source_sha=hashlib.file_digest(stream,'sha256').hexdigest()
    report={'sourceFile':args.input.name,'sha256':source_sha,
            'totalRows':total,'schemas':dict(schemas),'topicPromptCounts':dict(topic_counts),
            'multiplyEscapedAnswers':escaped,'candidateRows':len(candidates),'rejections':dict(rejected),
            'candidateTopicCounts':{name:sum(name in r['topics'] for r in candidates) for name in TOPICS},
            'limitations':['Automatic scope and formatting audit only; no factual correctness certification.',
                           'Question deduplication does not establish independence of paraphrased families.',
                           'Review and grouped split preparation are required before training.']}
    (args.output/'audit.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print(json.dumps(report,indent=2))

if __name__=='__main__':main()
