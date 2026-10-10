"""Evidence must become stale after changes to any tested input or artifact."""
from pathlib import Path
import importlib.util
import tempfile
import unittest

module_path = Path(__file__).resolve().parents[1] / 'scripts/input_audit_manifest.py'
spec = importlib.util.spec_from_file_location('manifest', module_path)
manifest = importlib.util.module_from_spec(spec)
spec.loader.exec_module(manifest)

class ManifestTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        for name in ('CMakeLists.txt', 'src/mouse.cpp', 'build/pvz-console-edition.exe',
                     'scripts/run_input_audit.ps1', 'scripts/check_input_coverage.py',
                     'scripts/input_audit_manifest.py', 'baseline/profile.dat',
                     'run.log', 'earlier.log', 'replay.csv'):
            path = self.root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(name)
        self.data = {'built_executable_sha256': manifest.sha(self.root / 'build/pvz-console-edition.exe'),
                     'source_sha256': manifest.source_hashes(self.root),
                     'profile_baseline': str(self.root / 'baseline'),
                     'profile_sha256': {'profile.dat': manifest.sha(self.root / 'baseline/profile.dat')},
                     'runs': {'test': self.evidence('run.log')},
                     'attempts': [{'repeat': 1, 'name': 'test', **self.evidence('earlier.log')}]}

    def evidence(self, log):
        return {'log': str(self.root / log), 'log_sha256': manifest.sha(self.root / log),
                'replay': 'replay.csv', 'replay_sha256': manifest.sha(self.root / 'replay.csv')}

    def test_unchanged(self):
        self.assertEqual(manifest.verify(self.data, self.root), [])

    def test_each_artifact_change(self):
        for name in ('build/pvz-console-edition.exe', 'src/mouse.cpp', 'run.log',
                     'earlier.log', 'replay.csv', 'baseline/profile.dat'):
            with self.subTest(name=name):
                path = self.root / name
                original = path.read_bytes()
                path.write_bytes(original + b'changed')
                self.assertTrue(manifest.verify(self.data, self.root))
                path.write_bytes(original)

    def test_new_source_invalidates_snapshot(self):
        (self.root / 'src/new.h').write_text('new layout')
        self.assertTrue(manifest.verify(self.data, self.root))

    def test_legacy_report_is_stale(self):
        self.assertTrue(manifest.verify({'built_executable_sha256': self.data['built_executable_sha256']}, self.root))

if __name__ == '__main__': unittest.main()
