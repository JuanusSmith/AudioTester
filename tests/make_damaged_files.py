"""Makes damaged copies of the good test files and writes expected.txt (file -> PASS/FAIL)."""
import os
import sys

folder = sys.argv[1]


def read(name):
    with open(os.path.join(folder, name), 'rb') as f:
        return bytearray(f.read())


def write(name, data):
    with open(os.path.join(folder, name), 'wb') as f:
        f.write(data)


def truncate(data, fraction=0.7):
    return data[:int(len(data) * fraction)]


def flip(data, position, count=16):
    data = bytearray(data)
    for i in range(position, position + count):
        data[i] ^= 0xFF
    return data


expected = {
    'good.flac': 'PASS',
    'good_unknown_length.flac': 'PASS',
    'good.ogg': 'PASS',
    'good.wv': 'PASS',
    'good.mp3': 'PASS',
    'good_crc.mp3': 'PASS',
    'good_id3v2.mp3': 'PASS',
}

for ext in ('flac', 'ogg', 'wv'):
    good = read('good.' + ext)
    write('truncated.' + ext, truncate(good))
    write('flipped.' + ext, flip(good, len(good) // 2))
    expected['truncated.' + ext] = 'FAIL'
    expected['flipped.' + ext] = 'FAIL'

# MP3: cut mid-frame, break a frame header half way through, and damage CRC-protected side info
FRAME = 417
mp3 = read('good.mp3')
write('truncated.mp3', mp3[:200 * FRAME + 100])
write('broken_header.mp3', flip(mp3, 200 * FRAME, 2))
crc = read('good_crc.mp3')
write('crc_damaged.mp3', flip(crc, 200 * FRAME + 10, 1))
expected['truncated.mp3'] = 'FAIL'
expected['broken_header.mp3'] = 'FAIL'
expected['crc_damaged.mp3'] = 'FAIL'

# An ID3v2 tag whose size bytes aren't sync-safe (a byte >= 0x80). The old signed-char code
# misread the size; the fixed code reports a bad tag
bad_tag = bytearray(b'ID3\x03\x00\x00\x00\x00\x01\x90') + bytes(0x290) + mp3
write('bad_id3v2_size.mp3', bad_tag)
expected['bad_id3v2_size.mp3'] = 'FAIL'

# Not audio at all, and empty files
junk = bytes((i * 37 + 11) & 0xFF for i in range(50000))
for ext in ('flac', 'ogg', 'wv', 'mp3'):
    write('junk.' + ext, junk)
    write('empty.' + ext, b'')
    expected['junk.' + ext] = 'FAIL'
    expected['empty.' + ext] = 'FAIL'

with open(os.path.join(folder, 'expected.txt'), 'w') as f:
    for name in sorted(expected):
        f.write('%s\t%s\n' % (name, expected[name]))
print('%d test files ready' % len(expected))
