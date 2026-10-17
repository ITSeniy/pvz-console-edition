"""T09 read-only audit of supplied Xbox assets and Android sound counterparts.

Inputs: assets/*.pak, verified research/cross-platform/xbox-decoded caches,
existing inventories, Android assets/files, save/ and runtime-data/user saves.
Outputs: JSON evidence and a format-table Markdown fragment under research/.
No loader or game-data conversion is implemented. Pillow verifies image decoding;
NumPy and an existing FFmpeg enable a seeded waveform sample (default 20).
Use --sample 0 without NumPy/FFmpeg. --test-xctd-tool runs the existing local
research decoder into the output directory and compares it with the verified cache.
All read inputs carry SHA-256; input trees are fingerprinted again before exit.
"""
import argparse
from collections import Counter
import hashlib
import io
import json
import math
from pathlib import Path
import random
import shutil
import statistics
import struct
import subprocess
import sys
import wave
import xml.etree.ElementTree as ET
import zipfile
import zlib

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(Path(__file__).resolve().parent))
from analyze_cross_platform import pak_entries, xbox_dds, decode_text, lawn_strings
try:
    from PIL import Image
except ImportError:
    sys.path.insert(0, str(ROOT / 'tools/python'))
    from PIL import Image
try:
    import numpy as np
except ImportError:
    np = None


def sha(raw):
    return hashlib.sha256(raw).hexdigest()


def file_sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(chunk)
    return h.hexdigest()


def fingerprint_tree(path):
    return {p.relative_to(ROOT).as_posix(): file_sha(p)
            for p in sorted(path.rglob('*')) if p.is_file()}


def riff(raw):
    if raw[:4] != b'RIFF' or raw[8:12] != b'WAVE' or len(raw) < 12:
        raise ValueError('expected RIFF/WAVE')
    if struct.unpack_from('<I', raw, 4)[0] + 8 != len(raw):
        raise ValueError('RIFF size mismatch')
    chunks = {}
    pos = 12
    while pos < len(raw):
        if pos + 8 > len(raw):
            raise ValueError('truncated RIFF chunk header')
        name, size = raw[pos:pos+4], struct.unpack_from('<I', raw, pos+4)[0]
        end = pos + 8 + size
        if end > len(raw) or name in chunks:
            raise ValueError('truncated/duplicate RIFF chunk')
        chunks[name] = raw[pos+8:end]
        # Supplied Android WAVs omit final padding on odd-length 8-bit data.
        pos = end + (size & 1) if end < len(raw) else end
    fmt = chunks[b'fmt ']
    tag, channels, rate, avg, align, bits = struct.unpack_from('<HHIIHH', fmt)
    result = dict(tag=hex(tag), channels=channels, rate=rate, bits=bits,
                  block_align=align, data_bytes=len(chunks[b'data']))
    if not rate or not channels or not align:
        raise ValueError('invalid audio dimensions')
    if tag == 0x166:
        if len(fmt) != 52 or struct.unpack_from('<H', fmt, 16)[0] != 34:
            raise ValueError('unsupported XMA2 format block')
        result.update(streams=struct.unpack_from('<H', fmt, 18)[0],
                      encoded_samples=struct.unpack_from('<I', fmt, 24)[0],
                      play_begin=struct.unpack_from('<I', fmt, 32)[0],
                      samples=struct.unpack_from('<I', fmt, 36)[0])
    elif tag == 1:
        if len(chunks[b'data']) % align:
            raise ValueError('incomplete PCM frame')
        result['samples'] = len(chunks[b'data']) // align
    else:
        raise ValueError('unsupported WAVE tag: ' + hex(tag))
    result['duration'] = result['samples'] / rate
    return result


def correlation(x, y, max_lag=2205):
    """Maximum overlap-normalized Pearson r, lag +/-100ms at 22050Hz.

    At least 90% of the shorter signal must overlap. Positive lag drops leading
    samples from x. No time stretching, nonlinear alignment or gain matching.
    """
    x, y = np.asarray(x, dtype=np.float64), np.asarray(y, dtype=np.float64)
    size = 1 << (len(x) + len(y) - 2).bit_length()
    cross = np.fft.irfft(np.fft.rfft(x, size) * np.fft.rfft(y[::-1], size), size)
    sums = [np.concatenate(([0.], np.cumsum(a))) for a in (x, y, x*x, y*y)]
    best = None
    for lag in range(-max_lag, max_lag + 1):
        ax, ay = max(lag, 0), max(-lag, 0)
        n = min(len(x)-ax, len(y)-ay)
        if n < max(32, math.ceil(.9 * min(len(x), len(y)))):
            continue
        sx, sy, xx, yy = [a[start+n]-a[start] for a, start in zip(sums, (ax, ay, ax, ay))]
        denom = max(0., xx-sx*sx/n) * max(0., yy-sy*sy/n)
        if denom < 1e-20:
            continue
        r = (cross[len(y)-1+lag] - sx*sy/n) / math.sqrt(denom)
        if best is None or r > best['pearson_r']:
            best = dict(pearson_r=float(max(-1., min(1., r))), lag_samples=lag,
                        overlap_samples=n, overlap_fraction=n/min(len(x), len(y)))
    return best


def decode(ffmpeg, raw):
    process = subprocess.run([ffmpeg, '-v', 'error', '-threads', '1', '-i', 'pipe:0',
                              '-vn', '-ac', '1', '-ar', '22050', '-f', 'f32le', 'pipe:1'],
                             input=raw, capture_output=True, timeout=90)
    if process.returncode or not process.stdout or len(process.stdout) % 4:
        raise ValueError('FFmpeg decode failed: ' + process.stderr.decode(errors='replace')[-500:])
    samples = np.frombuffer(process.stdout, dtype='<f4')
    if not np.isfinite(samples).all():
        raise ValueError('nonfinite decoded audio')
    return samples, process.stderr.decode(errors='replace')


def xctd_header(raw, decoded):
    magic, version, reserved, crc, flags = struct.unpack_from('>IHHII', raw)
    if magic != 0x0FF512ED or version != 0x100:
        raise ValueError('unsupported XCTD header')
    count = (flags >> 6) & 0xffff
    bits = 32 if (flags >> 22) & 3 else 20
    table_end = 16 + 4 * ((bits * count + 31) // 32)
    if table_end > len(raw):
        raise ValueError('truncated XCTD table')
    table = int.from_bytes(raw[16:table_end], 'big')
    sizes = [(table >> ((table_end-16)*8-(i+1)*bits)) & ((1 << bits)-1) for i in range(count)]
    expected = sum(sizes) if count else len(raw)-16
    if expected != len(decoded):
        raise ValueError('XCTD decoded size mismatch')
    return dict(version=version, reserved=reserved, flags=hex(flags), segments=count,
                table_bits=bits, window=1 << ((flags & 15)+15),
                zone_bytes=0x8000 << ((flags >> 4) & 3), table_end=table_end,
                decoded_size=expected, crc_field=hex(crc),
                decoded_crc32=hex(zlib.crc32(decoded)),
                ordinary_crc32_matches=(crc == zlib.crc32(decoded)), crc_verified=False)


def file_check(name, raw):
    ext = Path(name).suffix
    if ext == '.ptx':
        image = Image.open(io.BytesIO(xbox_dds(raw))).convert('RGBA')
        return dict(check='full-image-decode', width=image.width, height=image.height,
                    rgba_sha256=sha(image.tobytes()))
    if ext in ('.png', '.jpg', '.gif', '.dat'):
        image = Image.open(io.BytesIO(raw)).convert('RGBA')
        return dict(check='full-image-decode', width=image.width, height=image.height,
                    rgba_sha256=sha(image.tobytes()), format=('PNG-in-DAT' if ext=='.dat' else ext))
    if ext == '.xma':
        return dict(check='RIFF-XMA2-metadata', audio=riff(raw))
    if ext in ('.txt', '.xml', '.reanim'):
        text = decode_text(raw)
        if text is None:
            raise ValueError('undecodable text')
        if ext == '.xml':
            ET.fromstring(text)
            return dict(check='XML-syntax')
        if ext == '.reanim':
            ET.fromstring('<root>' + text + '</root>')
            return dict(check='XML-fragment-syntax')
        return dict(check='text-decode-only', lawn_keys=len(lawn_strings(raw)) if 'lawnstrings' in name else None)
    if ext == '.compiled':
        return dict(check='hash-and-envelope-only', semantic_parser=False, head=raw[:20].hex(),
                    android_zlib_envelope=raw[:4]==bytes.fromhex('d4feadde'))
    if ext == '.db':
        if raw[:8] != bytes.fromhex('d0cf11e0a1b11ae1'):
            raise ValueError('unexpected DB signature')
        return dict(check='OLE-signature-only', semantic_parser=False)
    if ext == '.xm':
        if raw[:17] != b'Extended Module: ' or raw[37] != 0x1a:
            raise ValueError('invalid XM header')
        version, size, length, restart, channels, patterns, instruments = struct.unpack_from('<HI5H', raw, 58)
        if version != 0x104 or size < 20 or size+60 > len(raw):
            raise ValueError('unsupported XM header')
        return dict(check='XM-header-only', semantic_parser=False, channels=channels,
                    song_orders=length, patterns=patterns, instruments=instruments,
                    title=raw[17:37].rstrip(b'\0 ').decode('cp1252'))
    if ext == '.ogg':
        if raw[:4] != b'OggS' or b'\x01vorbis' not in raw[:256]:
            raise ValueError('expected Ogg/Vorbis')
        return dict(check='Ogg-Vorbis-signature-only', semantic_parser=False)
    return dict(check='hash-only', semantic_parser=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT/'research/cross-platform/native-assets')
    parser.add_argument('--ffmpeg', default=shutil.which('ffmpeg'))
    parser.add_argument('--sample', type=int, default=20)
    parser.add_argument('--seed', type=int, default=20261017)
    parser.add_argument('--test-xctd-tool', action='store_true')
    parser.add_argument('--self-test', action='store_true')
    args = parser.parse_args()
    if args.self_test:
        test(); return
    out = args.output.resolve()
    if ROOT/'research' not in out.parents:
        parser.error('Output must be a subdirectory of research; input trees are read only')
    if not 0 <= args.sample <= 195:
        parser.error('--sample must be 0..195')
    if args.sample and (np is None or not args.ffmpeg):
        parser.error('Waveform sample needs NumPy and existing FFmpeg; use the bundled Python or --sample 0')
    out.mkdir(parents=True, exist_ok=True)
    inputs = {}
    def track(path):
        path = Path(path).resolve()
        key = str(path.relative_to(ROOT)) if path.is_relative_to(ROOT) else str(path)
        inputs[key] = file_sha(path)
    for path in (Path(__file__), ROOT/'scripts/analyze_cross_platform.py',
                 ROOT/'research/cross-platform/xbox-inventory.json', ROOT/'research/cross-platform/android-inventory.json'):
        track(path)
    immutable_roots = [ROOT/'assets', ROOT/'Chinese Android TV Image', ROOT/'save', ROOT/'runtime-data/user']
    before = {str(p.relative_to(ROOT)):fingerprint_tree(p) for p in immutable_roots}
    expected = json.loads((ROOT/'research/cross-platform/xbox-inventory.json').read_text())
    expected = {(r['package'],r['name']):r for r in expected}
    files, packages, sounds, music, errors, findings = [], [], {}, [], [], []
    for original in sorted((ROOT/'assets').glob('*.pak')):
        track(original); source = original.read_bytes(); package = dict(name=original.name, source_sha256=sha(source))
        if source[:4] == bytes.fromhex('0ff512ed'):
            decoded = ROOT/'research/cross-platform/xbox-decoded'/original.name
            metadata_path = decoded.with_suffix('.pak.meta.json'); track(decoded); track(metadata_path)
            decoded_raw = decoded.read_bytes(); metadata=json.loads(metadata_path.read_text())
            if metadata['source_sha256'] != sha(source) or metadata['decoded_sha256'] != sha(decoded_raw):
                raise ValueError('unverified decoded cache: '+original.name)
            package.update(xctd_header(source,decoded_raw)); package['decoded_sha256']=sha(decoded_raw)
            if args.test_xctd_tool:
                tool=ROOT/'research/cross-platform-tools/xctd_rip.exe'; track(tool)
                track(tool.with_suffix('.cpp'))
                target=out/'xctd-test'/original.name; target.parent.mkdir(exist_ok=True)
                trial=subprocess.run([str(tool),str(original),str(target)],capture_output=True,timeout=120)
                package['research_decoder_test']=dict(exit_code=trial.returncode,
                    stderr=trial.stderr.decode(errors='replace')[-500:],
                    output_sha256=file_sha(target) if target.exists() else None,
                    exact_match=trial.returncode==0 and target.exists() and file_sha(target)==sha(decoded_raw))
        else:
            decoded=original
        batch=pak_entries(decoded)
        package['entries']=len(batch); packages.append(package)
        for name,raw,location in batch:
            old=expected.pop((original.name,name),None)
            if old is None or old['sha256']!=sha(raw) or old['offset']!=location['offset'] or old['size']!=len(raw):
                raise ValueError('inventory no longer matches: '+original.name+':'+name)
            record=dict(package=original.name,name=name,size=len(raw),sha256=sha(raw))
            try:
                record.update(file_check(name,raw))
            except ET.ParseError as error:
                # PopCap's XML dialect includes custom entities, bare attributes
                # and unescaped ampersands. Record strict-parser coverage as a
                # finding; never repair the input or claim semantic parsing.
                record.update(check='strict-XML-rejected',validation_error=str(error))
                findings.append(original.name+':'+name+': '+str(error))
            except Exception as error:
                record['error']=str(error); errors.append(original.name+':'+name+': '+str(error))
            files.append(record)
            if name.endswith('.xma'): sounds[name]=raw
            if name.endswith(('.xm','.ogg')): music.append(record)
    if expected:
        raise ValueError('inventory records were not consumed')
    android=json.loads((ROOT/'research/cross-platform/android-inventory.json').read_text())
    android_music=[]
    for entry in android:
        if not entry['name'].endswith(('.xm','.ogg')): continue
        path=ROOT/entry['container']; track(path); raw=path.read_bytes()
        if sha(raw)!=entry['sha256'] or len(raw)!=entry['size']:
            raise ValueError('Android music inventory mismatch: '+entry['name'])
        android_music.append(dict(entry, **file_check(entry['name'],raw)))
    android_music_by_name={r['name']:r for r in android_music}
    music_comparison=[]
    for entry in music:
        counterpart=android_music_by_name.get(entry['name'])
        music_comparison.append(dict(xbox=entry['name'],
            android=counterpart['name'] if counterpart else None,
            identical_encoded_bytes=entry['sha256']==counterpart['sha256'] if counterpart else None))
    wavs={r['name']:r for r in android if r['name'].endswith('.wav')}
    pairs=[]
    selected=set(random.Random(args.seed).sample(sorted(sounds),args.sample))
    for name,xma in sorted(sounds.items()):
        wav_name=name[:-4]+'.wav'; entry=wavs.pop(wav_name,None)
        if entry is None:
            raise ValueError('missing Android WAV: '+name)
        path=ROOT/entry['container']; track(path); wav=path.read_bytes()
        if sha(wav)!=entry['sha256']: raise ValueError('Android inventory mismatch: '+name)
        a,b=riff(xma),riff(wav)
        pair=dict(xbox=name,android=wav_name,xbox_sha256=sha(xma),android_sha256=sha(wav),
                  xma=a,wav=b,rate_equal=a['rate']==b['rate'],channels_equal=a['channels']==b['channels'],
                  duration_delta_seconds=a['duration']-b['duration'])
        if name in selected:
            try:
                x,xerr=decode(args.ffmpeg,xma); y,yerr=decode(args.ffmpeg,wav)
                pair['waveform']=dict(correlation(x,y) or {},
                    decoded_xma_samples=len(x),decoded_wav_samples=len(y),
                    common_rate=22050,common_channels=1,decoder_stderr=(xerr+yerr)[-400:])
            except Exception as error:
                pair['decode_error']=str(error)
        pairs.append(pair)
    ffmpeg=None
    if args.sample:
        track(args.ffmpeg)
        ffmpeg=dict(path=args.ffmpeg,version=subprocess.run([args.ffmpeg,'-version'],capture_output=True,text=True,check=True).stdout.splitlines()[0])
    loose=[]
    for path in sorted((ROOT/'assets').iterdir()):
        if not path.is_file() or path.suffix=='.pak': continue
        raw=path.read_bytes(); record=dict(name=path.name,size=len(raw),sha256=sha(raw))
        if path.suffix in ('.png','.xml'):
            record.update(file_check(path.name,raw))
        elif path.suffix=='.xex':
            record.update(check='XEX2-signature-only',signature=raw[:4].decode('ascii'))
        elif path.suffix=='.wmv':
            probe=Path(args.ffmpeg).with_name('ffprobe.exe') if args.ffmpeg else None
            record.update(check='ASF-signature-only',head=raw[:16].hex())
            if probe and probe.exists():
                track(probe)
                p=subprocess.run([str(probe),'-v','error','-show_entries',
                    'stream=codec_name,codec_type,width,height,sample_rate,channels:format=duration',
                    '-of','json',str(path)],capture_output=True,text=True,check=True,timeout=30)
                record.update(check='FFprobe-stream-metadata',media=json.loads(p.stdout))
        loose.append(record)
    saves=[]
    save_paths=list((ROOT/'save').glob('*'))+list((ROOT/'runtime-data/user/584109FF/profile/User').glob('*'))
    save_paths+=list((ROOT/'runtime-data/user').rglob('game_*.dat'))
    for path in sorted(p for p in save_paths if p.is_file()):
        track(path); raw=path.read_bytes(); record=dict(path=path.relative_to(ROOT).as_posix(),size=len(raw),sha256=sha(raw),head=raw[:40].hex())
        if path.name in ('63E83FFD','63E83FFE','63E83FFF'):
            size={'63E83FFD':160,'63E83FFE':1000,'63E83FFF':850}[path.name]
            offset=24 if len(raw)==size+24 else 0
            record.update(payload_size=len(raw)-offset,wrapper_bytes=offset,expected_payload_size=size)
            if offset: record.update(setting_id=hex(struct.unpack_from('>I',raw)[0]),setting_type=raw[8],declared_payload=struct.unpack_from('>I',raw,16)[0])
            if path.name=='63E83FFF': record.update(profile_version=struct.unpack_from('>I',raw,offset)[0],adventure_level=struct.unpack_from('>I',raw,offset+4)[0])
        if path.name.startswith('game_'):
            offset=raw.find(bytes.fromhex('feeddead')); record['feeddead_offset']=offset
            if offset>=0: record['observed_header_version_be']=struct.unpack_from('>I',raw,offset+4)[0]
            if len(raw)>=24:
                record.update(prefix_sha1_matches_body=hashlib.sha1(raw[20:]).digest()==raw[:20],
                              header_length_le=struct.unpack_from('<I',raw,20)[0])
        if zipfile.is_zipfile(path):
            with zipfile.ZipFile(path) as archive:
                record['zip_entries']=[dict(name=i.filename,size=i.file_size,sha256=sha(archive.read(i))) for i in archive.infolist()]
        saves.append(record)
    for path in (ROOT/'scripts/make_level_save.py',ROOT/'scripts/import_xenia_profile.py',ROOT/'scripts/export_test_profile_to_xenia.py',
                 ROOT/'research/pvz-pc/Lawn/System/SaveGame.cpp',ROOT/'research/cross-platform-tools/xctd_rip.cpp',
                 ROOT/'research/cross-platform-tools/quickbms/quickbms.txt',
                 ROOT/'research/rexglue-source/src/system/lzx.cpp',
                 ROOT/'Chinese Android TV Image/lib/armeabi/libGameMain.so',
                 ROOT/'research/cross-platform/android-symbols.json',ROOT/'.cache/pvz-image.bin',
                 ROOT/'scripts/compare_cross_platform_images.py',ROOT/'scripts/inspect_cross_platform_code.py',
                 ROOT/'scripts/package_pc_redraw_sprites.py'):
        track(path)
    for path in sorted((ROOT/'research/cross-platform-tools/thirdparty/libmspack').glob('*')):
        if path.suffix in ('.h','.c'): track(path)
    after={str(p.relative_to(ROOT)):fingerprint_tree(p) for p in immutable_roots}
    unchanged=before==after
    if not unchanged: errors.append('input tree changed during audit')
    types=[]
    for ext in sorted(set(Path(r['name']).suffix for r in files)):
        subset=[r for r in files if Path(r['name']).suffix==ext]
        types.append(dict(extension=ext,count=len(subset),checks=dict(Counter(r.get('check','error') for r in subset)),
                          strict_parser_rejections=sum('validation_error' in r for r in subset),
                          errors=sum('error' in r for r in subset)))
    metadata_summary=dict(pairs=len(pairs),unpaired_android=sorted(wavs),
        rates_equal=sum(p['rate_equal'] for p in pairs),channels_equal=sum(p['channels_equal'] for p in pairs),
        xbox_rates=dict(Counter(p['xma']['rate'] for p in pairs)),android_rates=dict(Counter(p['wav']['rate'] for p in pairs)),
        xbox_channels=dict(Counter(p['xma']['channels'] for p in pairs)),android_channels=dict(Counter(p['wav']['channels'] for p in pairs)),
        max_abs_duration_delta=max(abs(p['duration_delta_seconds']) for p in pairs),
        median_abs_duration_delta=statistics.median(abs(p['duration_delta_seconds']) for p in pairs),
        sampled=len(selected),decode_errors=sum('decode_error' in p for p in pairs))
    correlations=[p['waveform']['pearson_r'] for p in pairs if 'pearson_r' in p.get('waveform',{})]
    metadata_summary.update(correlated=len(correlations),pearson_min=min(correlations) if correlations else None,
                            pearson_median=statistics.median(correlations) if correlations else None,
                            pearson_max=max(correlations) if correlations else None)
    result=dict(schema=1,input_sha256=inputs,immutable_trees_before=before,inputs_unchanged=unchanged,
        packages=packages,types=types,files=files,audio_summary=metadata_summary,audio_pairs=pairs,
        audio_method=dict(seed=args.seed,sample_count=args.sample,ffmpeg=ffmpeg,numpy=np.__version__ if np is not None else None,
                          lag_bound_samples=2205,minimum_overlap=.9,rate=22050,channels=1),
        xbox_music=music,android_music=android_music,music_comparison=music_comparison,
        loose_xbox_files=loose,saves=saves,strict_xml_findings=findings,errors=errors)
    (out/'inventory.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    table=['| Тип | Записей | Проверка | Отказов строгого XML | Ошибок аудита |','|---|---:|---|---:|---:|']
    table += [f"| `{r['extension']}` | {r['count']} | {', '.join(r['checks'])} | {r['strict_parser_rejections']} | {r['errors']} |" for r in types]
    (out/'formats.md').write_text('\n'.join(table)+'\n',encoding='utf-8')
    print(json.dumps(dict(output=str(out/'inventory.json'),records=len(files),types=types,audio=metadata_summary,
                          inputs_unchanged=unchanged,strict_xml_findings=findings,errors=errors),ensure_ascii=False,indent=2))
    if errors: raise SystemExit(2)


def test():
    data=io.BytesIO()
    with wave.open(data,'wb') as stream:
        stream.setnchannels(1);stream.setsampwidth(2);stream.setframerate(22050);stream.writeframes(b'\0\0'*100)
    assert riff(data.getvalue())['samples']==100
    for bad in (b'',data.getvalue()[:-1]):
        try: riff(bad)
        except (ValueError,KeyError,struct.error): pass
        else: raise AssertionError('accepted broken RIFF')
    if np is not None:
        signal=np.random.default_rng(42).normal(size=4000)
        match=correlation(np.concatenate((np.zeros(137),signal)),signal,200)
        assert match['lag_samples']==137 and match['pearson_r']>.999999
    print('RIFF bounds, PCM sample counts and optional FFT/lag self-check passed')


if __name__=='__main__':
    main()
