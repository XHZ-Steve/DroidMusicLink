"""Check bilingual resource key parity and unfinished Qt translations."""
from pathlib import Path
import xml.etree.ElementTree as ET

root=Path(__file__).resolve().parents[1]
res=root/'android/app/src/main/res'
def names(path):
    nodes=ET.parse(path).getroot().findall('string')
    keys=[node.attrib['name'] for node in nodes]
    assert len(keys)==len(set(keys)),f'Duplicate resource keys: {path}'
    assert all(''.join(node.itertext()).strip() for node in nodes),f'Empty resource: {path}'
    return set(keys)
english=names(res/'values/strings.xml')
chinese=names(res/'values-zh-rCN/strings.xml')
assert english==chinese,f'Android language mismatch: {english ^ chinese}'
count=0
for path in (root/'windows/translations').glob('*.ts'):
    for message in ET.parse(path).iter('message'):
        translation=message.find('translation')
        assert translation is not None and translation.get('type')!='unfinished',f'Unfinished translation: {message.findtext("source")}'
        assert ''.join(translation.itertext()).strip(),f'Empty translation: {message.findtext("source")}'
        count+=1
print(f'Resources: {len(english)} Android keys match both languages; {count} finished Qt translations.')
