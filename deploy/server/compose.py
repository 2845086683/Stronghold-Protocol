"""Linux Compose entry point; never installs software or edits firewall rules."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

from manage import configure, owned_read_directory, private_dir, read_json

HERE = Path(__file__).resolve().parent


def run(command, capture=False):
    result = subprocess.run(command, check=True, text=True, capture_output=capture)
    return result.stdout.strip() if capture else ''


def command(config_dir):
    config_dir = owned_read_directory(config_dir)
    config = read_json(config_dir / 'deployment.json')
    if config.get('mode') not in ('public', 'lan'):
        raise RuntimeError('invalid deployment mode')
    # Revalidate stored values before Docker accepts a modified env file.
    with tempfile.TemporaryDirectory(prefix='stronghold-compose-config-') as temp:
        configure(Path(temp), config['site'] if config['mode'] == 'public' else None,
                  config['lanAddress'] if config['mode'] == 'lan' else None,
                  config['pythonImage'], config['caddyImage'])
        expected = (Path(temp) / 'compose.env').read_bytes()
    if expected != (config_dir / 'compose.env').read_bytes():
        raise RuntimeError('compose.env differs from validated deployment.json; reconfigure first')
    return ['docker', 'compose', '--project-directory', str(HERE),
            '--env-file', str(config_dir / 'compose.env'), '-f', str(HERE / 'compose.yaml'),
            '-f', str(HERE / ('compose.' + config['mode'] + '.yaml'))]


def backup(compose, folder):
    record = json.loads(run(compose + ['exec', '-T', 'backend', 'python', '-I',
        '/opt/stronghold/deploy/server/manage.py', 'backup', '--state-dir', '/data'], True))
    path = record.get('backup', '')
    if not re.fullmatch(r'/data/backups/[A-Za-z0-9-]+', path):
        raise RuntimeError('unexpected container backup path')
    folder = private_dir(folder)
    run(compose + ['cp', 'backend:' + path, str(folder)])
    target = folder / Path(path).name
    private_dir(target)
    image = run(compose + ['images', '--format', 'json'], True)
    (target / 'compose-images.json').write_text(image, encoding='utf-8')
    print('Backup: ' + str(target), flush=True)
    return target


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=('setup', 'init', 'up', 'stop', 'status', 'logs', 'backup', 'restore', 'upgrade', 'verify'))
    parser.add_argument('--config-dir', type=Path, default=HERE / 'instance')
    parser.add_argument('--backup-dir', type=Path, default=HERE / 'backups')
    parser.add_argument('--domain'); parser.add_argument('--lan-address')
    parser.add_argument('--python-image'); parser.add_argument('--caddy-image')
    parser.add_argument('--source', type=Path)
    parser.add_argument('--non-interactive', action='store_true')
    parser.add_argument('--no-start', action='store_true')
    args = parser.parse_args()
    if args.action == 'setup':
        from setup import setup_deployment
        print(json.dumps(setup_deployment(args.config_dir, args.domain, args.lan_address,
            runner=run, command_builder=command, non_interactive=args.non_interactive,
            start=not args.no_start)))
        return
    if args.action == 'init':
        print(json.dumps(configure(args.config_dir, args.domain, args.lan_address,
                                  args.python_image or '', args.caddy_image or '')))
        return
    try:
        run(['docker', 'compose', 'version'], True)
    except (FileNotFoundError, subprocess.CalledProcessError):
        raise RuntimeError('Docker Engine + Compose v2 required. See deploy/server/README.md official installation entry; no software was installed.') from None
    compose = command(args.config_dir)
    if args.action == 'verify':
        run(compose + ['config', '--quiet'])
        print('Compose static validation passed; no container started.')
    elif args.action == 'up':
        run(compose + ['up', '-d', '--build', '--wait', '--wait-timeout', '90'])
    elif args.action == 'stop':
        # Do not down -v or prune: server data and certificates must survive.
        run(compose + ['stop'])
    elif args.action == 'status': run(compose + ['ps'])
    elif args.action == 'logs': run(compose + ['logs', '--tail', '200', '-f'])
    elif args.action == 'backup': backup(compose, args.backup_dir)
    elif args.action == 'upgrade':
        backup(compose, args.backup_dir)
        run(compose + ['build', 'backend'])
        run(compose + ['up', '-d', '--wait', '--wait-timeout', '90'])
    elif args.action == 'restore':
        if not args.source: raise RuntimeError('restore requires --source backup-folder')
        source = owned_read_directory(args.source)
        if ':' in str(source) or not (source / 'manifest.json').is_file():
            raise RuntimeError('invalid Linux backup directory')
        # The volume lock also enforces stopped state in the maintenance
        # container. No host-side access to Docker volume internals is used.
        run(compose + ['stop'])
        run(compose + ['run', '--rm', '--no-deps', '-v', str(source) + ':/restore:ro',
                       'backend', 'restore', '--state-dir', '/data', '--source', '/restore'])
        run(compose + ['up', '-d', '--wait', '--wait-timeout', '90'])


if __name__ == '__main__':
    try: main()
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError) as error:
        print(str(error), file=sys.stderr); sys.exit(1)
