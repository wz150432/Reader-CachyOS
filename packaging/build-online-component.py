#!/usr/bin/env python3
"""Package a trusted local Linux build; never fetch or publish executable code."""
import argparse
import gzip
import hashlib
import io
import json
import os
from pathlib import Path
import platform
import re
import subprocess
import tarfile
import tempfile
from urllib.parse import urlsplit

VERSION = r'[0-9]+\.[0-9]+\.[0-9]+(?:-[A-Za-z0-9.-]+)?'
OFFICIAL = '/wz150432/Reader-CachyOS/releases/download/'


def command(*args):
    return subprocess.check_output(args, text=True, stderr=subprocess.STDOUT)


def dependencies(path):
    """Use the loader's resolved transitive closure, retaining needed SONAMEs."""
    result = {}
    for line in command('ldd', str(path)).splitlines():
        if 'not found' in line:
            raise ValueError('Unresolved runtime dependency: ' + line.strip())
        match = re.match(r'\s*(\S+) => (/\S+) \(', line)
        if match:
            result[match[1]] = Path(match[2])
    return result


def check_elf(path):
    header = command('readelf', '-h', str(path))
    if 'ELF64' not in header or 'Advanced Micro Devices X86-64' not in header:
        raise ValueError('Only Linux x86_64 ELF builds are supported')


def validate_url(url):
    if not url:
        return
    u = urlsplit(url)
    pieces = u.path.removeprefix(OFFICIAL).split('/')
    if (u.scheme != 'https' or u.netloc != 'github.com' or u.query or u.fragment
            or not u.path.startswith(OFFICIAL) or len(pieces) != 2
            or any(not p or p in ('.', '..') or '%' in p for p in pieces)):
        raise ValueError('URL must be an official HTTPS GitHub release asset without credentials/query')


def atomic_write(path, data):
    with tempfile.NamedTemporaryFile(dir=path.parent, delete=False) as f:
        temp = Path(f.name)
        try:
            f.write(data)
            f.flush()
            os.fsync(f.fileno())
        except BaseException:
            temp.unlink(missing_ok=True)
            raise
    try:
        os.replace(temp, path)
    finally:
        temp.unlink(missing_ok=True)


def build(args):
    if args.arch != 'x86_64' or platform.system() != 'Linux' or platform.machine() != 'x86_64':
        raise ValueError('Only Linux x86_64 is supported')
    if not re.fullmatch(VERSION, args.version) or not re.fullmatch(VERSION, args.app_version):
        raise ValueError('Component and app versions must be semantic version triplets')
    validate_url(args.url)
    engine, base = args.engine.resolve(), args.base.resolve()
    check_elf(engine)
    check_elf(base)
    dynamic = command('readelf', '-d', str(engine))
    if not re.search(r'\[(?:\$ORIGIN/lib)\]', dynamic):
        raise ValueError('Build the helper with exactly $ORIGIN/lib RPATH/RUNPATH (CMake OnlineEngine target)')
    request = dict(protocol=1, id='package', session='package', op='hello')
    probe = subprocess.run([str(engine)], input=json.dumps(request) + '\n', text=True,
                           capture_output=True, timeout=20, check=True)
    reply = json.loads(probe.stdout.splitlines()[0])
    if (reply.get('ok') is not True or reply.get('protocol') != 1
            or reply.get('result') != {'protocol': 1, 'version': args.version}):
        raise ValueError('Built helper handshake disagrees with component version/protocol')
    engine_deps, base_deps = dependencies(engine), dependencies(base)
    # libxml2 is private even on Arch builds where libarchive transitively uses it.
    # Qt6Concurrent belongs to the already-required qt6-base runtime package.
    private = {name: path for name, path in engine_deps.items()
               if (name.startswith('libxml2.so') or name not in base_deps)
               and not name.startswith('libQt6Concurrent.so')}
    licenses = {name: Path('/usr/share/licenses/libxml2/Copyright')
                for name in private if name.startswith('libxml2.so')}
    for item in args.license:
        name, separator, path = item.partition('=')
        if not separator or not name or Path(name).name != name:
            raise ValueError('--license must be SONAME=/absolute/license/path')
        licenses[name] = Path(path)
    files = {'reader-online': (engine.read_bytes(), 0o755)}
    for name, path in sorted(private.items()):
        if Path(name).name != name or name in ('.', '..'):
            raise ValueError('Unsafe dependency SONAME')
        if name not in licenses or not licenses[name].is_file():
            raise ValueError('Missing license; supply --license ' + name + '=/path/to/license')
        files['lib/' + name] = (path.read_bytes(), 0o644)  # dereference symlinks
        files['licenses/' + name + '.txt'] = (licenses[name].read_bytes(), 0o644)
    details = ('Reader online component ' + args.version + '\n'
               'Reader source: https://github.com/wz150432/Reader-CachyOS\n'
               'Private libraries: ' + ', '.join(sorted(private)) + '\n'
               'System runtime: Qt 6 (including Concurrent), libc/libstdc++ and base application dependencies.\n'
               'Content is supplied separately; preserve each source URL and its attribution/license.\n')
    files['licenses/README.txt'] = (details.encode(), 0o644)
    unpacked = sum(len(data) for data, _ in files.values())
    if unpacked > 512 * 1024 * 1024 or len(files) > 256:
        raise ValueError('Component exceeds manager unpacked/entry limits')
    output = io.BytesIO()
    with gzip.GzipFile(filename='', mode='wb', fileobj=output, mtime=0) as compressed:
        with tarfile.open(fileobj=compressed, mode='w', format=tarfile.USTAR_FORMAT) as archive:
            for name, (data, mode) in sorted(files.items()):
                entry = tarfile.TarInfo(name)
                entry.size, entry.mode, entry.mtime = len(data), mode, 0
                archive.addfile(entry, io.BytesIO(data))
    payload = output.getvalue()
    if len(payload) > 128 * 1024 * 1024:
        raise ValueError('Component exceeds manager packed limit')
    metadata = dict(version=args.version, appVersion=args.app_version, protocol=1,
                    platform='linux', arch=args.arch, entrypoint='reader-online', url=args.url,
                    size=len(payload), unpackedSize=unpacked,
                    sha256=hashlib.sha256(payload).hexdigest())
    args.output_dir.mkdir(parents=True, exist_ok=True)
    stem = 'reader-online-' + args.version + '-linux-' + args.arch
    archive_path = args.output_dir / (stem + '.tar.gz')
    metadata_path = args.output_dir / (stem + '.json')
    atomic_write(archive_path, payload)
    atomic_write(metadata_path, (json.dumps(metadata, indent=2, sort_keys=True) + '\n').encode())
    print(json.dumps(dict(archive=str(archive_path.resolve()), metadata=str(metadata_path.resolve()),
                          mode='download' if args.url else 'local-import-only', **metadata), indent=2))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--engine', type=Path, required=True, help='Trusted locally built helper')
    parser.add_argument('--base', type=Path, required=True, help='Built base reader for dependency comparison')
    parser.add_argument('--version', required=True)
    parser.add_argument('--app-version', required=True)
    parser.add_argument('--arch', default='x86_64')
    parser.add_argument('--output-dir', type=Path, default=Path('release'))
    parser.add_argument('--url', default='', help='Real official published release asset URL; omit for local import')
    parser.add_argument('--license', action='append', default=[], help='Private library SONAME=/license/path')
    args = parser.parse_args()
    try:
        build(args)
    except (ValueError, OSError, subprocess.SubprocessError, IndexError) as error:
        parser.exit(1, str(error) + '\n')


if __name__ == '__main__':
    main()
