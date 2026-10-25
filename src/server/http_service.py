import json
import datetime
import hashlib
import os
import ipaddress
import secrets
from pathlib import Path
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
from urllib.parse import urlsplit,parse_qs
from .engine import DomainError
from .activity_profile import ActivityProfile,ACTIVITY,PREFIX
from .native_session import NativeSession
from .multiplayer_preparation_view import PreparationViewRooms
from .participant_solo import ParticipantSolo

def backend_build_id():
    """Identity of code loaded at server creation, matching native file pins."""
    sources={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(Path(__file__).resolve().parent.glob('*.py'))}
    repair=Path(__file__).resolve().parents[3]/'server-data/cn-autochess-blackboard-repair.json'
    sources['data:cn-autochess-blackboard-repair.json']=hashlib.sha256(repair.read_bytes()).hexdigest()
    return hashlib.sha256(json.dumps(sources,sort_keys=True,separators=(',',':')).encode()).hexdigest()

def make_server(store,protocol,port=0,*,host='127.0.0.1',access_token=None):
    """Serve independently; legacy single-player state remains loopback-only.

    Multiplayer routes authenticate individual participants themselves. A LAN
    listener must still carry an operator token before any socket is opened;
    that token never grants a LAN client access to the legacy shared profile.
    """
    try:loopback_bind=ipaddress.ip_address(host).is_loopback
    except ValueError:raise ValueError('bind host must be a numeric IPv4 address') from None
    if ':' in host:raise ValueError('bind host must be a numeric IPv4 address')
    if access_token is not None and (not isinstance(access_token,str) or len(access_token)<32 or not access_token.isascii() or any(ord(c)<33 or ord(c)>126 for c in access_token)):
        raise ValueError('access token must contain at least 32 visible ASCII characters')
    if not loopback_bind and not access_token:raise ValueError('non-loopback binding requires an access token')
    activity=ActivityProfile(store)
    native=NativeSession(store,protocol,activity)
    rooms=PreparationViewRooms(store,protocol,activity)
    solo=ParticipantSolo(store,protocol,rooms)
    multiplayer_path='/bridge/'+ACTIVITY+'/multiplayer'
    native_path='/bridge/'+ACTIVITY+'/native-session'
    configuration_diagnostics=[]
    build_id=backend_build_id()
    class Handler(BaseHTTPRequestHandler):
        def log_message(self,*args):pass
        def authorize_route(self):
            try:route=urlsplit(self.path).path
            except ValueError:
                self.reply(400,{'ok':False,'error':'invalid_request','officialFallback':False});return False
            if route=='/health':return True
            # These routes must validate their own room participant credential.
            if route.startswith('/bridge/'+ACTIVITY+'/multiplayer/'):
                return True
            # Native MULTI BattleStartController uses this exact original code.
            # It authenticates a room participant, never the legacy/operator key.
            if route in (PREFIX+'multiBattleStart', PREFIX+'multiBattleFinish'):return True
            if not ipaddress.ip_address(self.client_address[0]).is_loopback:
                self.reply(403,{'ok':False,'error':'legacy_route_loopback_only','officialFallback':False});return False
            if access_token and not secrets.compare_digest(self.headers.get('Authorization',''),'Bearer '+access_token):
                self.reply(401,{'ok':False,'error':'backend_access_token_required','officialFallback':False});return False
            return True
        def record_configuration_failure(self,value,error,detail):
            # Only the local activity's known configuration DTO fields belong
            # here; headers and unrelated player/account payloads are excluded.
            if self.path not in tuple(PREFIX+name for name in ('setChessPoolDiyChar','setChessPoolDeploy','removeChessPoolChar')):return
            if not isinstance(value,dict):value={}
            request={key:value[key] for key in ('actId','activityId','chessId') if key in value}
            for name in ('diyChessPool','chessPool'):
                pool=value.get(name)
                if not isinstance(pool,dict):continue
                request[name]={key:{field:item[field] for field in ('diyChar','origChessId','skillIndex','currentEquip') if field in item}
                               for key,item in list(pool.items())[:100] if isinstance(item,dict)}
            record={'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'path':self.path,'error':error,'detail':detail,'request':request}
            with store.lock:
                configuration_diagnostics.append(record)
                del configuration_diagnostics[:-32]
        def reply(self,status,value):
            # Native consumers never read the private domain audit history.
            # It stays intact in saves and ordinary diagnostic views. This
            # compatibility trim also covers older immutable full-view receipts.
            if self.headers.get('X-Stronghold-Native-View')=='1' and self.path.startswith(multiplayer_path+'/'):
                state=value.get('selfState')
                if isinstance(state,dict):state.pop('history',None)
                timeline=value.get('bossTimeline')
                if isinstance(timeline,dict):timeline.pop('reports',None)
            raw=json.dumps(value,ensure_ascii=False,separators=(',',':')).encode()
            from .multiplayer_transport import encoded_reply
            raw,compressed=encoded_reply(raw,self.headers.get('Accept-Encoding',''))
            self.send_response(status);self.send_header('Content-Type','application/json; charset=utf-8')
            self.send_header('Vary','Accept-Encoding')
            if compressed:self.send_header('Content-Encoding','gzip')
            self.send_header('Content-Length',str(len(raw)));self.end_headers();self.wfile.write(raw)
        def do_GET(self):
            if not self.authorize_route():return
            parts=self.path.strip('/').split('/')
            try:
                parsed=urlsplit(self.path)
                if parsed.path.startswith(multiplayer_path+'/'):
                    actor=rooms.authenticate(self.headers.get('Authorization'))
                    route=parsed.path[len(multiplayer_path)+1:].split('/')
                    query=parse_qs(parsed.query,keep_blank_values=True,strict_parsing=True)
                    if route==['bootstrap'] and not query:return self.reply(200,solo.bootstrap(actor,rooms.bootstrap(actor)))
                    if route and route[0]=='solo':return self.solo_reply(actor,'GET',parsed.path[len(multiplayer_path+'/solo/'):]+('?' + parsed.query if parsed.query else ''))
                    if route==['profile'] and not query:return self.reply(200,rooms.profile(actor))
                    if len(route)==2 and route[0]=='rooms' and set(query)<= {'afterEvent','afterStep','stepEpoch'}:
                        values=query.get('afterEvent',['0'])
                        if len(values)!=1 or not values[0].isascii() or not values[0].isdecimal():raise DomainError('invalid_afterEvent')
                        steps=query.get('afterStep',['-1']);epoch=query.get('stepEpoch',['0'])
                        if len(steps)!=1 or not (steps[0]=='-1' or steps[0].isascii() and steps[0].isdecimal()):raise DomainError('invalid_afterStep')
                        if len(epoch)!=1 or not epoch[0].isascii() or not epoch[0].isdecimal():raise DomainError('invalid_stepEpoch')
                        return self.reply(200,rooms.get(actor,route[1],int(values[0]),after_step=int(steps[0]),step_epoch=int(epoch[0]),native_view=self.headers.get('X-Stronghold-Native-View')=='1'))
                    return self.reply(404,{'ok':False,'error':'unknown_multiplayer_route','officialFallback':False})
                if self.path=='/health':return self.reply(200,{'ok':True,'purpose':'CN native activity private backend','officialWireCompatible':False,'catalogHash':store.engine.c.hash,'nativeSessionBridgeVersion':1,'backendBuildId':build_id,'pid':os.getpid(),'nativeReady':False})
                if self.path=='/bridge/'+ACTIVITY+'/bootstrap':return self.reply(200,activity.bootstrap())
                if self.path=='/bridge/'+ACTIVITY+'/configuration-diagnostics':
                    with store.lock:recent=list(configuration_diagnostics)
                    return self.reply(200,{'recentFailures':recent,'officialFallback':False})
                if self.path=='/bridge/'+ACTIVITY+'/player-activity':return self.reply(200,activity.get()['playerActivity'])
                if self.path=='/bridge/'+ACTIVITY+'/player-model':return self.reply(200,activity.player_model())
                if self.path=='/bridge/'+ACTIVITY+'/perm':return self.reply(200,activity.get()['autoChessPerm'])
                if self.path=='/bridge/'+ACTIVITY+'/char-map':return self.reply(200,{v['charId']:v for v in activity._roster['chars'].values()})
                if self.path=='/bridge/'+ACTIVITY+'/sync-info':
                    result=activity.invoke(PREFIX+'syncInfo',{'actId':ACTIVITY})
                    return self.reply(200,{key:result[key] for key in ('changed','battleInfo')})
                if self.path==native_path:return self.reply(200,native.current())
                if self.path==native_path+'/latest-result':return self.reply(200,native.latest_result())
                if len(parts)==4 and '/'.join(parts[:3])==native_path.strip('/'):
                    return self.reply(200,native.get(parts[3]))
                if len(parts)==2 and parts[0]=='runs':
                    state=store.get(parts[1]);return self.reply(200,{'ok':True,'state':state,'dtoProjection':protocol.project(state)})
                return self.reply(404,{'ok':False,'error':'unknown_local_route'})
            except (ValueError,TypeError,KeyError) as ex:return self.reply(400,{'ok':False,'error':'invalid_request','detail':str(ex),'officialFallback':False})
            except DomainError as ex:return self.reply(400,{'ok':False,'error':ex.code,'detail':ex.detail,'officialFallback':False})
        def do_POST(self):
            if not self.authorize_route():return
            value={}
            try:
                if self.headers.get('Origin'):raise DomainError('browser_origin_not_supported')
                if self.headers.get_content_type()!='application/json':raise DomainError('json_required')
                length=int(self.headers.get('Content-Length','0'))
                if not 0<length<=1048576:raise DomainError('invalid_body_size')
                value=json.loads(self.rfile.read(length));parts=self.path.strip('/').split('/')
                if not isinstance(value,dict):raise DomainError('invalid_payload')
                if self.path == PREFIX+'multiBattleStart':
                    actor=rooms.authenticate(self.headers.get('Authorization'))
                    return self.reply(200,rooms.native_start(actor,value))
                if self.path == PREFIX+'multiBattleFinish':
                    actor=rooms.authenticate(self.headers.get('Authorization'))
                    return self.reply(200,rooms.native_finish(actor,value))
                if self.path.startswith(multiplayer_path+'/'):
                    route=self.path[len(multiplayer_path)+1:].split('/')
                    if route==['participants']:return self.reply(201,rooms.register(value))
                    actor=rooms.authenticate(self.headers.get('Authorization'))
                    if route==['profile']:return self.reply(200,rooms.configure(actor,value))
                    if route==['native-sync-info']:
                        from .multiplayer_native_sync import response
                        return self.reply(200,response(rooms,solo,actor,value))
                    if len(route)==5 and route[0]=='rooms' and route[2]=='native-settlement':
                        from .multiplayer_native_settlement import response
                        return self.reply(200,response(rooms,actor,route[1],route[3],route[4],value))
                    # Original Unity FinishGuideBattle consumes PlayerDeltaResponse,
                    # not the private bridge's {ok,payload} envelope. Authenticate
                    # the actor before selecting its private solo database.
                    native_exit=urlsplit(self.path)
                    native_exit_parts=native_exit.path[len(multiplayer_path)+1:].split('/')
                    if (len(native_exit_parts)==4 and native_exit_parts[0]=='solo-native-exit' and
                            native_exit_parts[2] in ('saved-exit','terminal-exit')):
                        target='native-session/'+self.path[len(multiplayer_path+'/solo-native-exit/'):]
                        return self.reply(200,solo.request(actor,'POST',target,value))
                    if route and route[0]=='solo':return self.solo_reply(actor,'POST',self.path[len(multiplayer_path+'/solo/'):],value)
                    if route==['rooms']:return self.reply(201,rooms.create(actor,value))
                    if len(route)==3 and route[0]=='rooms' and route[2]=='join':return self.reply(200,rooms.join(actor,route[1],value))
                    if len(route)==3 and route[0]=='rooms' and route[2]=='commands':
                        from .multiplayer_transport import view_options
                        return self.reply(200,rooms.execute(actor,route[1],value,view_options=view_options(self.headers)))
                    if len(route)==4 and route[0]=='rooms' and route[2:]==['commands','batch']:
                        from .multiplayer_transport import execute_batch,view_options
                        return self.reply(200,execute_batch(rooms,actor,route[1],value,view_options=view_options(self.headers)))
                    return self.reply(404,{'ok':False,'error':'unknown_multiplayer_route','officialFallback':False})
                exit_url=urlsplit(self.path);exit_parts=exit_url.path.strip('/').split('/')
                if (len(exit_parts)==6 and '/'.join(exit_parts[:3])==native_path.strip('/') and exit_parts[4] in ('saved-exit','terminal-exit')):
                    kind=exit_parts[4].replace('-','_')
                    if exit_url.path!='/'+('/'.join(exit_parts)) or exit_url.fragment:raise DomainError('invalid_'+kind+'_proof')
                    query=parse_qs(exit_url.query,keep_blank_values=True,strict_parsing=True)
                    if set(query)!={'revision'} or len(query['revision'])!=1:raise DomainError('invalid_'+kind+'_proof')
                    text=query['revision'][0]
                    if not text.isascii() or not text.isdecimal() or text.startswith('0') or len(text)>10:
                        raise DomainError('invalid_'+kind+'_proof')
                    acknowledge=native.saved_exit_ack if kind=='saved_exit' else native.terminal_exit_ack
                    return self.reply(200,acknowledge(exit_parts[3],exit_parts[5],int(text),value))
                # This inherited native service is never a tutorial-completion
                # command here. Only a dedicated receipt-bound route above
                # can acknowledge scene cleanup; no profile getter may run.
                if exit_url.path==PREFIX+'finishGuideBattle':raise DomainError('saved_exit_proof_required')
                if self.path==native_path:return self.reply(201,native.create(value))
                if (len(parts)==5 and '/'.join(parts[:3])==native_path.strip('/') and parts[4]=='presented' and
                        self.path==native_path+'/'+parts[3]+'/presented'):
                    return self.reply(200,native.result_presented(parts[3],value))
                if len(parts)==5 and '/'.join(parts[:3])==native_path.strip('/') and parts[4]=='protocol':
                    result=native.execute(parts[3],value)
                    return self.reply(200 if result['ok'] else 400,result)
                if self.path.startswith(PREFIX):
                    result=activity.invoke(self.path,value)
                    return self.reply(200,{'result':0,**result,'nativeAcceptancePending':True,'officialFallback':False})
                if self.path=='/runs':
                    s=store.create(value);return self.reply(201,{'ok':True,'state':s,'dtoProjection':protocol.project(s)})
                if len(parts)==3 and parts[0]=='runs' and parts[2] in ('commands','protocol'):
                    if parts[2]=='protocol':op,args=protocol.decode(value.get('type'),value.get('payload',{}))
                    else:op,args=value.get('op'),value.get('payload',{})
                    result=store.execute(parts[1],value.get('requestId'),op,args)
                    if result['ok']:result['dtoProjection']=protocol.project(result['state'])
                    return self.reply(200 if result['ok'] else 400,result)
                return self.reply(404,{'ok':False,'error':'unknown_local_route'})
            except (ValueError,TypeError,KeyError) as ex:return self.reply(400,{'ok':False,'error':'invalid_request','detail':str(ex)})
            except DomainError as ex:
                self.record_configuration_failure(value,ex.code,ex.detail)
                return self.reply(400,{'ok':False,'error':ex.code,'detail':ex.detail})
        def solo_reply(self,actor,verb,path,body=None):
            try: result=solo.request(actor,verb,path,body)
            except DomainError as ex:result={'ok':False,'error':ex.code,'detail':ex.detail,'officialFallback':False}
            return self.reply(200,{'ok':True,'officialFallback':False,'payload':result})
    class RoomServer(ThreadingHTTPServer):
        def service_actions(self):
            rooms.tick()
        def server_close(self):
            try:super().server_close()
            finally:solo.close()
    server=RoomServer((host,port),Handler)
    server.multiplayer=rooms
    return server
