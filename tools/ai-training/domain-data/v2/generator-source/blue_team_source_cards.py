"""Source-anchored candidate paraphrases; no source admission or expert review is implied."""
import hashlib
import json
from knowledge_cards import build as existing_cards,normalize

BLE_CARDS=[
('ble-shared-spectrum',15,'Numerous technologies operate in this band','Can ordinary radio coexistence matter when Bluetooth behavior changes?',
 'The described Bluetooth band is shared with other technologies. A change in radio conditions does not by itself establish deliberate interference.'),
('ble-hopping-limit',15,'minimal transmission security','Does frequency hopping alone establish strong communication security?',
 'The reference describes frequency hopping as reducing interference and transmission errors while providing minimal transmission security. It is not a substitute for checking the security mechanisms in use.'),
('ble-connectionless',21,'connectionless broadcast architecture','Does receiving BLE broadcast data prove that a connection was established?',
 'The described low-energy broadcast architecture can send data without a connection. Observing that broadcast does not establish a connected session.'),
('ble-broadcaster-observer',21,'Observers','What roles are described for connectionless BLE broadcasting?',
 'A broadcaster periodically sends data, while observers listen and consume it. The roles describe communication behavior, not hostile intent.'),
('ble-one-to-many',21,'more than one peer at a time','Must each receiver of a broadcast have its own separate connected exchange?',
 'The reference describes broadcast data reaching more than one peer at a time. Receiver count cannot be inferred as a count of separately established connections from that fact alone.'),
('ble-private-addresses',39,'private device addresses','Can a private address be an ordinary Bluetooth feature?',
 'The reference describes low-energy private addressing as a protocol privacy feature. Its presence alone is not an attack label.'),
('ble-irk-role',39,'Identity Resolving Key','What is the named role of the IRK in the described privacy mechanism?',
 'The Identity Resolving Key supports the described address-privacy mechanism. A passive observer cannot assume it has the identity-resolution capability of a trusted device.'),
('ble-rpa-mapping',39,'map a Resolvable Private Address','Is an observed resolvable private address automatically the underlying identity address?',
 'The reference distinguishes a Resolvable Private Address from an Identity Address and describes their mapping through the privacy mechanism. They should not be treated as interchangeable observed values.'),
('ble-identity-address-types',39,'static random address or a public','Must a Bluetooth Identity Address always be a public address?',
 'In the described mechanism, an Identity Address can be a static random address or a public address. Address terminology should retain that distinction.'),
('ble-address-rotation-privacy',39,'periodically-changing RPA','Does a periodically changing BLE address prove that another physical device arrived?',
 'The privacy mechanism can use a periodically changing RPA. Address changes alone do not establish the arrival of a new physical device.'),
('ble-dual-mode',18,'dual mode','Can one described Bluetooth device support both classic and low-energy communication?',
 'The reference describes dual-mode devices supporting BR/EDR and low-energy operation. Observations of different modes are not by themselves proof of different physical devices.'),
('ble-general-threats',51,'general wireless networking threats','Are Bluetooth risks limited to one named advertising pattern?',
 'The reference describes broader wireless risks, including service disruption and eavesdropping. A particular captured pattern still needs evidence-specific interpretation; the general threat list is not an incident verdict.')]


def build(library):
    rows=existing_cards(library)
    manifest=json.loads((library/'manifest.json').read_text());doc=next(d for d in manifest['documents'] if d['id']=='nist-sp800-121r2-upd1')
    pdf=library/(doc['id']+'.pdf')
    if hashlib.sha256(pdf.read_bytes()).hexdigest()!=doc['sha256']:raise ValueError('Changed Bluetooth reference')
    pages=json.loads((library/(doc['id']+'.pages.json')).read_text())
    for identifier,page,anchor,question,answer in BLE_CARDS:
        text=pages[page-1]['text']
        if normalize(anchor) not in normalize(text):raise ValueError('Missing anchor: '+identifier)
        fact,separator,application=answer.partition('. ')
        rows.append(dict(id=identifier,question=question,sourceFact=fact+'.' if separator else answer,
            authoredProductInterpretation=application if separator else None,sourceId=doc['id'],sourceSha256=doc['sha256'],
            pdfPage=page,supportAnchor=anchor,pageTextSha256=hashlib.sha256(text.encode()).hexdigest(),sourceDate=doc['date'],scope=doc['scope'],
            reviewStatus='agent-authored-anchor-checked-not-independently-reviewed',trainingApproved=False,
            rightsReviewRequired='Publication has external coauthors; retain notices and complete selected-material rights review before admission.',
            purpose='candidate factual paraphrase and explicitly limited defensive interpretation',releaseEligible=False))
    return rows
