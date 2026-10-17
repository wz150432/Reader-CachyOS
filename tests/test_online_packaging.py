"""Run the real component packager against a controlled ELF and dependency."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tarfile
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
TOOL = ROOT / 'packaging/build-online-component.py'


class PackagingTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / 'lib').mkdir()
        (self.root / 'dep.c').write_text('int answer(void) { return 42; }\n')
        subprocess.run(['cc', '-shared', '-fPIC', '-Wl,-soname,libsample.so.1',
                        'dep.c', '-o', 'lib/libsample.so.1.0'], cwd=self.root, check=True)
        (self.root / 'lib/libsample.so.1').symlink_to('libsample.so.1.0')
        (self.root / 'main.c').write_text('''#include <stdio.h>
extern int answer(void);
int main(void) { if(answer()!=42)return 1; char b[1024]; if(fgets(b,sizeof b,stdin))
puts("{\\"protocol\\":1,\\"id\\":\\"package\\",\\"session\\":\\"package\\",\\"ok\\":true,\\"result\\":{\\"protocol\\":1,\\"version\\":\\"0.1.0\\"}}"); return 0; }
''')
        subprocess.run(['cc', 'main.c', '-Llib', '-l:libsample.so.1',
                        '-Wl,-rpath,$ORIGIN/lib', '-o', 'reader-online'], cwd=self.root, check=True)
        (self.root / 'LICENSE').write_text('Controlled sample library license\n')
        self.args = [sys.executable, str(TOOL), '--engine', str(self.root / 'reader-online'),
                     '--base', '/usr/bin/true', '--output-dir', str(self.root / 'out'),
                     '--version', '0.1.0', '--app-version', '0.1.0',
                     '--license', 'libsample.so.1=' + str(self.root / 'LICENSE')]

    def run_tool(self, *extra):
        return subprocess.run(self.args + list(extra), capture_output=True, text=True)

    def test_archive_hash_sizes_private_library_and_reproducibility(self):
        result = self.run_tool()
        self.assertEqual(result.returncode, 0, result.stderr)
        archive = self.root / 'out/reader-online-0.1.0-linux-x86_64.tar.gz'
        metadata = json.loads(archive.with_suffix('').with_suffix('.json').read_text())
        self.assertEqual(metadata['url'], '')
        self.assertEqual(metadata['size'], archive.stat().st_size)
        self.assertEqual(metadata['sha256'], hashlib.sha256(archive.read_bytes()).hexdigest())
        with tarfile.open(archive) as package:
            members = package.getmembers()
            self.assertTrue(all(not m.name.startswith('/') and '..' not in m.name.split('/') for m in members))
            self.assertTrue(all(m.isfile() for m in members))
            self.assertEqual(metadata['unpackedSize'], sum(m.size for m in members))
            self.assertTrue(package.getmember('reader-online').mode & 0o100)
            self.assertIn('lib/libsample.so.1', package.getnames())
            self.assertIn('licenses/libsample.so.1.txt', package.getnames())
            package.extractall(self.root / 'installed', filter='data')
        hello = subprocess.run([str(self.root / 'installed/reader-online')], input='hello\n', text=True, capture_output=True)
        self.assertEqual(hello.returncode, 0, hello.stderr)
        self.assertEqual(json.loads(hello.stdout)['result']['version'], '0.1.0')
        digest = metadata['sha256']
        self.assertEqual(self.run_tool().returncode, 0)
        self.assertEqual(digest, hashlib.sha256(archive.read_bytes()).hexdigest())

    def test_refuses_nonofficial_or_malformed_remote_urls(self):
        for url in ['http://github.com/wz150432/Reader-CachyOS/releases/download/v/a',
                    'https://example.org/a', 'https://github.com/other/repo/releases/download/v/a',
                    'https://user@github.com/wz150432/Reader-CachyOS/releases/download/v/a',
                    'https://github.com:444/wz150432/Reader-CachyOS/releases/download/v/a',
                    'https://github.com/wz150432/Reader-CachyOS/releases/download/v/a?token=x']:
            with self.subTest(url=url):
                self.assertNotEqual(self.run_tool('--url', url).returncode, 0)
        url = 'https://github.com/wz150432/Reader-CachyOS/releases/download/v0.1.0/reader-online-0.1.0-linux-x86_64.tar.gz'
        self.assertEqual(self.run_tool('--url', url).returncode, 0)

    def test_refuses_version_disagreement_and_missing_license(self):
        self.assertNotEqual(self.run_tool('--version', '0.2.0').returncode, 0)
        self.assertNotEqual(self.run_tool('--license', 'libsample.so.1=/does/not/exist').returncode, 0)

    def test_refuses_wrong_architecture(self):
        self.assertNotEqual(self.run_tool('--arch', 'aarch64').returncode, 0)


if __name__ == '__main__':
    unittest.main()
