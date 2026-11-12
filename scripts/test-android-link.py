"""Integration against an installed Android server; approval stays on the phone.

Requires websockets==15.0.1 and `adb forward tcp:18123 tcp:18123`.
SAS v2 is compared on the phone; approval is never automated by this helper. This script never writes the received authentication token.
"""
import argparse
import asyncio
import hashlib
import json
import secrets
import ssl
import time
import uuid
from pathlib import Path
from websockets.asyncio.client import connect

def envelope(kind, payload=None):
    return dict(protocolVersion=1,messageId=str(uuid.uuid4()),timestamp=int(time.time()*1000),type=kind,payload=payload or {})

async def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--url', default='wss://127.0.0.1:18123')
    parser.add_argument('--report', default='artifacts/android-link-test.json')
    args=parser.parse_args()
    context=ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
    context.check_hostname=False
    context.verify_mode=ssl.CERT_NONE
    context.minimum_version=context.maximum_version=ssl.TLSVersion.TLSv1_2
    results={}
    # Initial TLS identity is provisional; the human SAS check binds the channel.
    # No previously saved token is used or written by this test helper.
    async with connect(args.url,ssl=context,open_timeout=10,max_size=1048576) as ws:
        certificate=ws.transport.get_extra_info('ssl_object').getpeercert(binary_form=True)
        fingerprint=hashlib.sha256(certificate).digest()
        async def until(kind):
            while True:
                message=json.loads(await asyncio.wait_for(ws.recv(),60))
                if message['type']==kind:return message['payload']
        await until('hello')
        await ws.send(json.dumps(envelope('command_request',{'requestId':'unauthorized-test','command':'play'})))
        assert (await until('error'))['code']=='unauthorized'
        results['unauthorized_control_rejected']=True
        await ws.send('{invalid json')
        assert (await until('error'))['code']=='invalid_message'
        results['malformed_json_rejected']=True
        duplicate=envelope('ping')
        await ws.send(json.dumps(duplicate));await until('pong')
        await ws.send(json.dumps(duplicate))
        assert (await until('error'))['code']=='duplicate_message'
        results['duplicate_message_rejected']=True
        client_id='integration-'+uuid.uuid4().hex
        nonce=secrets.token_bytes(32)
        await ws.send(json.dumps(envelope('pair_begin',{'clientId':client_id,'clientName':'Integration test PC','clientCommit':hashlib.sha256(nonce).hexdigest()})))
        challenge=await until('pair_challenge')
        await ws.send(json.dumps(envelope('pair_reveal',{'clientNonce':nonce.hex()})))
        comparison=await until('pair_compare')
        server_nonce=bytes.fromhex(comparison['serverNonce'])
        assert len(server_nonce)==32 and hashlib.sha256(server_nonce).hexdigest()==challenge['serverCommit']
        digest=hashlib.sha256(b'DroidMediaLink-SAS-v2'+fingerprint+nonce+server_nonce+client_id.encode()).digest()
        code=f"{int.from_bytes(digest[:4],'big')%1000000:06d}"
        print('Compare on Android and approve only if identical: '+code,flush=True)
        results['sas_commitments_verified']=True
        approved=await until('pair_approve')
        assert approved['clientId']==client_id and len(approved['token'])>=32
        await ws.send(json.dumps(envelope('auth',{'clientId':client_id,'token':approved['token']})))
        assert (await until('auth_result'))['authenticated'] is True
        results['explicit_pairing_and_auth']=True
        snapshot=await until('session_snapshot')
        results['snapshot_received']=isinstance(snapshot,dict)
        await ws.send(json.dumps(envelope('ping')));await until('pong')
        results['heartbeat']=True
    Path(args.report).parent.mkdir(parents=True,exist_ok=True)
    Path(args.report).write_text(json.dumps({'platform':'Android API 30 emulator, ADB forwarded WSS','results':results,'real_music_session_tested':False},indent=2)+'\n')
    print(json.dumps(results,indent=2))

asyncio.run(main())
