import argparse
from pathlib import Path
from .catalog import Catalog
from .engine import Engine
from .protocol import Protocol
from .storage import Store
from .http_service import make_server

def main():
    root=Path(__file__).resolve().parents[3]
    parser=argparse.ArgumentParser(description='Independent CN native Stronghold activity private backend')
    parser.add_argument('--catalog',type=Path,default=root/'server-data/cn-autochess.json')
    parser.add_argument('--database',type=Path,default=root/'local-server/stronghold.sqlite3')
    parser.add_argument('--port',type=int,default=16680)
    parser.add_argument('--host',default='127.0.0.1',help='numeric IPv4 bind address; LAN requires --access-token-file')
    parser.add_argument('--access-token-file',type=Path,help='UTF-8 file containing an operator token (32+ visible ASCII characters)')
    args=parser.parse_args();args.database.parent.mkdir(parents=True,exist_ok=True)
    token=args.access_token_file.read_text(encoding='utf-8-sig').strip() if args.access_token_file else None
    catalog=Catalog(args.catalog);store=Store(args.database,Engine(catalog))
    try:server=make_server(store,Protocol(catalog),args.port,host=args.host,access_token=token)
    except Exception:store.close();raise
    print(f'CN activity independent backend on http://{args.host}:{server.server_port}; legacy routes are loopback-only; officialFallback=false',flush=True)
    try:server.serve_forever()
    except KeyboardInterrupt:pass
    finally:server.server_close();store.close()

if __name__=='__main__':main()
