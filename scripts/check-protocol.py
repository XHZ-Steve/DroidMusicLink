"""Validate canonical protocol vectors and trust-boundary rejection cases."""
import copy
import json
from pathlib import Path
from jsonschema import Draft202012Validator

root = Path(__file__).resolve().parents[1]
validator = Draft202012Validator(json.loads((root / 'protocol/v1.schema.json').read_text(encoding='utf-8')))
Draft202012Validator.check_schema(validator.schema)
vectors = json.loads((root / 'protocol/vectors.json').read_text(encoding='utf-8'))
for vector in vectors:
    validator.validate(vector)
invalid = []
for key, value in [('protocolVersion', 2), ('timestamp', -1), ('type', 'execute_shell'), ('messageId', ''), ('payload', [])]:
    item = copy.deepcopy(vectors[0])
    item[key] = value
    invalid.append(item)
for index, key, value in [(1, 'clientCommit', '123'), (2, 'command', 'shell'), (2, 'positionMs', -1), (3, 'volume', 1.01)]:
    item = copy.deepcopy(vectors[index])
    item['payload'][key] = value
    invalid.append(item)
for index,key in [(5,'serverCommit'),(6,'clientNonce'),(7,'serverNonce')]:
    item=copy.deepcopy(vectors[index]);item['payload'][key]='z'*64;invalid.append(item)
item=copy.deepcopy(vectors[1]);item['type']='pair_request';invalid.append(item)
for item in invalid:
    assert not validator.is_valid(item), f'Accepted malformed frame: {item}'
print(f'Protocol v1: {len(vectors)} canonical vectors and {len(invalid)} rejection cases passed.')
