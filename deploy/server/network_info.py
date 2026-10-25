"""Local adapter discovery only; no Internet probes, DNS tricks or firewall edits."""
import ipaddress
import json
import os
from pathlib import Path
import socket
import subprocess

PRIVATE = tuple(ipaddress.ip_network(n) for n in ('10.0.0.0/8','172.16.0.0/12','192.168.0.0/16'))

def normalize(rows):
    result=[];seen=set()
    for row in rows:
        try: address=ipaddress.IPv4Address(row.get('address',''))
        except (ValueError,AttributeError): continue
        if address.is_loopback or address.is_unspecified or address.is_link_local or address.is_multicast or address.is_reserved: continue
        if str(address) in seen: continue
        seen.add(str(address))
        name=''.join(c for c in str(row.get('name','adapter')) if c.isprintable())[:100]
        result.append({'name':name,'address':str(address),'private':any(address in n for n in PRIVATE),
                       'defaultRoute':bool(row.get('defaultRoute',False))})
    return sorted(result,key=lambda x:(not x['private'],not x['defaultRoute'],x['name'],x['address']))

def adapters():
    if os.name=='nt':
        # PowerShell serializes Unicode to UTF-8 explicitly. Interface names and
        # addresses are data, never interpolated into a command line.
        script=r'''[Console]::OutputEncoding=New-Object System.Text.UTF8Encoding($false);$ErrorActionPreference='Stop';$routes=@(Get-NetRoute -AddressFamily IPv4 -DestinationPrefix '0.0.0.0/0' -ErrorAction SilentlyContinue | Select-Object -ExpandProperty InterfaceIndex);$rows=@(Get-NetIPAddress -AddressFamily IPv4 -AddressState Preferred | Where-Object { $_.InterfaceAlias -and (Get-NetAdapter -InterfaceIndex $_.InterfaceIndex -ErrorAction SilentlyContinue).Status -eq 'Up' } | ForEach-Object { @{name=$_.InterfaceAlias;address=$_.IPAddress;defaultRoute=($routes -contains $_.InterfaceIndex)} });ConvertTo-Json -InputObject $rows -Compress'''
        executable=Path(os.environ.get('SystemRoot',r'C:\Windows'))/'System32/WindowsPowerShell/v1.0/powershell.exe'
        try:
            done=subprocess.run([str(executable),'-NoProfile','-NonInteractive','-Command',script],capture_output=True,
                timeout=12,creationflags=subprocess.CREATE_NO_WINDOW)
            if done.returncode: raise RuntimeError('Windows adapter query failed')
            return normalize(json.loads(done.stdout.decode('utf-8-sig'))),None
        except (OSError,ValueError,RuntimeError,subprocess.TimeoutExpired):
            return [],'Automatic adapter discovery failed; run ipconfig to find this PC\'s IPv4 address.'
    try:
        return normalize([{'name':'host','address':v[4][0]} for v in socket.getaddrinfo(socket.gethostname(),None,socket.AF_INET)]),None
    except OSError:return [],'Automatic adapter discovery failed.'

def connection_info(host,port,rows):
    address=ipaddress.IPv4Address(host)
    local='http://127.0.0.1:'+str(port) if address.is_unspecified or address.is_loopback else 'http://'+host+':'+str(port)
    usable=normalize(rows) if address.is_unspecified else [r for r in normalize(rows) if r['address']==host]
    return {'local':local,'health':local+'/health','lan':[dict(r,url='http://'+r['address']+':'+str(port)) for r in usable]}

def show(logger,host,port,public_only,state):
    rows,warning=adapters() if host=='0.0.0.0' or not ipaddress.ip_address(host).is_loopback else ([],None)
    info=connection_info(host,port,rows)
    logger.info('Connect on this PC: %s',info['local'])
    logger.info('Health check: %s',info['health'])
    for row in info['lan']:
        logger.info('%s [%s%s]: %s', 'LAN client address' if row['private'] else 'Other interface (check routing)',row['name'],', default route' if row['defaultRoute'] else '',row['url'])
    if host=='0.0.0.0':
        logger.info('0.0.0.0 is the listening address. Clients must use a PC address listed above; LAN reachability is not yet verified.')
        if not info['lan']:logger.warning('No usable LAN IPv4 detected. Connect Wi-Fi/Ethernet and check ipconfig.')
        logger.info('If another PC cannot connect, allow inbound TCP %d on the Windows Private network profile; no firewall rule was changed.',port)
    if warning:logger.warning('%s',warning)
    logger.info('Mode: %s; data: %s; logs: %s','authenticated participants (legacy routes disabled)' if public_only else 'local legacy / solo',state,state/'logs/server.log')
    logger.info('Ctrl+C stops this foreground server cleanly; Stop-Server.cmd also stops this instance.')
    return info
