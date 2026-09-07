// Lossless P6 RGB -> PNG packaging for Pictor captures. No image manipulation.
// @implements spec/feature/three-store-brands.md Comparison delivery
import { readFileSync, writeFileSync } from 'node:fs';
import { deflateSync } from 'node:zlib';

function crc32(bytes) {
  let crc = 0xffffffff;
  for (const value of bytes) {
    crc ^= value;
    for (let bit = 0; bit < 8; ++bit) {
      crc = (crc >>> 1) ^ ((crc & 1) ? 0xedb88320 : 0);
    }
  }
  return (crc ^ 0xffffffff) >>> 0;
}

function chunk(type, data) {
  const tag = Buffer.from(type);
  const size = Buffer.alloc(4);
  size.writeUInt32BE(data.length);
  const crc = Buffer.alloc(4);
  crc.writeUInt32BE(crc32(Buffer.concat([tag, data])));
  return Buffer.concat([size, tag, data, crc]);
}

if (process.argv.length !== 4) throw new Error('Usage: node encode_ppm.mjs input.ppm output.png');
const input = readFileSync(process.argv[2]);
const header = /^P6\n(\d+) (\d+)\n255\n/.exec(input.subarray(0, 64).toString('ascii'));
if (!header) throw new Error('Expected Pictor P6 RGB output');
const width = Number(header[1]);
const height = Number(header[2]);
const pixels = input.subarray(header[0].length);
if (width < 1 || height < 1 || pixels.length !== width * height * 3) {
  throw new Error('Invalid image dimensions or pixel count');
}
const stride = width * 3;
const rows = Buffer.alloc((stride + 1) * height);
for (let y = 0; y < height; ++y) pixels.copy(rows, y * (stride + 1) + 1, y * stride, (y + 1) * stride);
const ihdr = Buffer.alloc(13);
ihdr.writeUInt32BE(width);
ihdr.writeUInt32BE(height, 4);
ihdr[8] = 8;
ihdr[9] = 2;
writeFileSync(process.argv[3], Buffer.concat([
  Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk('IHDR', ihdr),
  chunk('sRGB', Buffer.from([0])), chunk('IDAT', deflateSync(rows)), chunk('IEND', Buffer.alloc(0)),
]));
