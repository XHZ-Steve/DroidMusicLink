"""Opt-in real-player control check over authenticated LAN WSS. No titles/tokens logged.

Temporarily pauses, skips next/previous, then restores the original play state.
Explicitly compare the printed code with the phone before approving the test peer.
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


def envelope(kind, payload):
    return dict(protocolVersion=1, messageId=str(uuid.uuid4()), timestamp=int(time.time()*1000), type=kind, payload=payload)


async def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--url', required=True)
    parser.add_argument('--report', default='artifacts/physical-live-media.json')
    args=parser.parse_args()
    tls=ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
    tls.check_hostname=False
    tls.verify_mode=ssl.CERT_NONE  # Provisional identity becomes SAS-bound before control.
    tls.minimum_version=tls.maximum_version=ssl.TLSVersion.TLSv1_2
    async with connect(args.url, ssl=tls, proxy=None) as ws:
        snapshot={}
        async def send(kind, payload):
            await ws.send(json.dumps(envelope(kind, payload)))
        async def until(kind, predicate=lambda p: True, timeout=60):
            nonlocal snapshot
            async def receive():
                nonlocal snapshot
                while True:
                    event=json.loads(await ws.recv())
                    payload=event['payload']
                    if event['type']=='session_snapshot': snapshot=payload
                    if event['type']=='error': raise AssertionError(payload.get('code'))
                    if event['type']==kind and predicate(payload): return payload
            return await asyncio.wait_for(receive(), timeout)
        await until('hello')
        cert=ws.transport.get_extra_info('ssl_object').getpeercert(binary_form=True)
        fingerprint=hashlib.sha256(cert).digest()
        client='media-check-'+uuid.uuid4().hex
        nonce=secrets.token_bytes(32)
        await send('pair_begin',dict(clientId=client,clientName='DroidMediaLink media check',clientCommit=hashlib.sha256(nonce).hexdigest()))
        challenge=await until('pair_challenge')
        await send('pair_reveal',dict(clientNonce=nonce.hex()))
        compare=await until('pair_compare')
        server_nonce=bytes.fromhex(compare['serverNonce'])
        assert hashlib.sha256(server_nonce).hexdigest()==challenge['serverCommit']
        assert compare['deviceId'].lower()==fingerprint.hex()
        digest=hashlib.sha256(b'DroidMediaLink-SAS-v2'+fingerprint+nonce+server_nonce+client.encode()).digest()
        print('Compare and approve media check: '+f'{int.from_bytes(digest[:4],"big")%1000000:06d}',flush=True)
        approval=await until('pair_approve')
        assert approval['clientId']==client
        await send('auth',dict(clientId=client,token=approval['token']))
        await until('auth_result',lambda p:p.get('authenticated') is True)
        await until('session_snapshot')
        assert snapshot.get('title'), 'No active media metadata'
        original=snapshot.copy()
        async def command(name, **values):
            request=uuid.uuid4().hex
            await send('command_request',dict(requestId=request,command=name,**values))
            result=await until('command_result',lambda p:p.get('requestId')==request,timeout=10)
            assert result.get('success'), result.get('error')
        async def state(value):
            if snapshot.get('state')!=value:
                await until('session_snapshot',lambda p:p.get('state')==value,timeout=10)
        results={}
        try:
            await command('pause');await state('paused');results['pause']=True
            assert all(c in snapshot.get('commands',[]) for c in ['play','pause','next','previous','toggle'])
            results['paused_controls_available']=True
            old_title=snapshot['title']
            await command('next')
            if snapshot.get('title')==old_title:
                await until('session_snapshot',lambda p:p.get('title')!=old_title,timeout=10)
            results['next_while_paused']=True
            await command('pause');await state('paused')
            old_title=snapshot['title']
            await command('previous')
            if snapshot.get('title')==old_title:
                await until('session_snapshot',lambda p:p.get('title')!=old_title,timeout=10)
            results['previous_while_paused']=True
            results['original_track_restored']=snapshot.get('title')==original['title']
            if results['original_track_restored'] and 'seek' in snapshot.get('commands',[]):
                await command('seek',positionMs=original.get('positionMs',0))
            await command('play');await state('playing');results['play_from_paused']=True
        finally:
            await command('play' if original.get('state')=='playing' else 'pause')
        Path(args.report).write_text(json.dumps(results,indent=2)+'\n',encoding='utf-8')
        print(json.dumps(results),flush=True)


asyncio.run(main())
