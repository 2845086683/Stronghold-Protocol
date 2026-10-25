import copy
import hashlib
import json
import sqlite3
import threading
import uuid
from .engine import DomainError

def dump(value):return json.dumps(value,ensure_ascii=False,separators=(',',':'),sort_keys=True)

class Store:
    def __init__(self,path,engine):
        self.engine=engine;self.lock=threading.RLock();self.db=sqlite3.connect(str(path),check_same_thread=False)
        self.db.execute('PRAGMA journal_mode=WAL');self.db.execute('PRAGMA synchronous=FULL')
        self.db.executescript('CREATE TABLE IF NOT EXISTS runs(id TEXT PRIMARY KEY,state TEXT NOT NULL);'
          'CREATE TABLE IF NOT EXISTS commands(run TEXT NOT NULL,id TEXT NOT NULL,fingerprint TEXT NOT NULL,reply TEXT NOT NULL,PRIMARY KEY(run,id));')
        self.db.commit()
    def close(self):self.db.close()
    def create(self,args):
        run=uuid.uuid4().hex;state=self.engine.create(run,args)
        with self.lock,self.db:self.db.execute('INSERT INTO runs VALUES(?,?)',(run,dump(state)))
        return state
    def get(self,run):
        with self.lock:
            row=self.db.execute('SELECT state FROM runs WHERE id=?',(run,)).fetchone()
            if not row:raise DomainError('unknown_run')
            state=json.loads(row[0])
            if state['catalogHash']!=self.engine.c.hash or state['policyVersion']!=1:raise DomainError('save_version_mismatch')
            return state
    def execute(self,run,request_id,op,payload):
        if not isinstance(request_id,str) or not 1<=len(request_id)<=128:raise DomainError('invalid_request_id')
        fingerprint=hashlib.sha256(dump({'op':op,'payload':payload}).encode()).hexdigest()
        with self.lock:
            self.db.execute('BEGIN IMMEDIATE')
            try:
                previous=self.db.execute('SELECT fingerprint,reply FROM commands WHERE run=? AND id=?',(run,request_id)).fetchone()
                if previous:
                    if previous[0]!=fingerprint:raise DomainError('request_id_conflict')
                    reply=json.loads(previous[1]);self.db.commit();return reply
                state=copy.deepcopy(self.get(run))
                try:
                    extra=self.engine.apply(state,op,payload)
                    reply={'ok':True,'requestId':request_id,'details':extra,'state':state}
                    self.db.execute('UPDATE runs SET state=? WHERE id=?',(dump(state),run))
                except DomainError as ex:
                    reply={'ok':False,'requestId':request_id,'error':ex.code,'detail':ex.detail}
                self.db.execute('INSERT INTO commands VALUES(?,?,?,?)',(run,request_id,fingerprint,dump(reply)))
                self.db.commit();return reply
            except Exception:
                self.db.rollback();raise
