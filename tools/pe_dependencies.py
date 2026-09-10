"""Read PE imports without loading or running the executable."""
# @implements spec/setup/phase1-portable-build.md Build
from pathlib import Path
import struct


def imported_dlls(executable: Path) -> list[str]:
    data = executable.read_bytes()

    def unpack(fmt: str, offset: int) -> tuple[int, ...]:
        size = struct.calcsize(fmt)
        if offset < 0 or offset + size > len(data):
            raise ValueError("truncated PE structure")
        return struct.unpack_from(fmt, data, offset)

    if len(data) < 0x40 or data[:2] != b"MZ":
        raise ValueError("not a PE executable")
    pe = unpack("<I", 0x3C)[0]
    if pe > len(data) - 24 or data[pe:pe + 4] != b"PE\0\0":
        raise ValueError("not a PE executable")
    machine, section_count = unpack("<HH", pe + 4)
    optional_size = unpack("<H", pe + 20)[0]
    optional = pe + 24
    if (machine != 0x8664 or optional_size < 224 or
            optional + optional_size > len(data) or
            unpack("<H", optional)[0] != 0x20B or
            unpack("<I", optional + 108)[0] < 14):
        raise ValueError("expected a Windows x64 executable")
    if section_count == 0 or section_count > 96:
        raise ValueError("invalid PE section count")
    sections = []
    for index in range(section_count):
        offset = optional + optional_size + index * 40
        virtual_size, virtual_address, raw_size, raw_address = unpack("<IIII", offset + 8)
        if raw_address + raw_size > len(data):
            raise ValueError("PE section exceeds file")
        sections.append((virtual_address, max(virtual_size, raw_size),
                         raw_address, raw_size))

    def file_offset(rva: int) -> int:
        for address, size, raw, raw_size in sections:
            if address <= rva < address + size:
                delta = rva - address
                if delta >= raw_size:
                    raise ValueError("PE RVA has no file backing")
                result = raw + delta
                return result
        raise ValueError("unmapped PE RVA")

    def directory(index: int) -> tuple[int, int]:
        rva, size = unpack("<II", optional + 112 + index * 8)
        return rva, size

    def dll_name(rva: int) -> str:
        offset = file_offset(rva)
        end = data.find(b"\0", offset, min(offset + 512, len(data)))
        if end < 0:
            raise ValueError("unterminated PE import name")
        return data[offset:end].decode("ascii")

    names = []
    import_rva, import_size = directory(1)
    if import_rva:
        start = file_offset(import_rva)
        for index in range(min(512, import_size // 20 + 1)):
            entry = unpack("<IIIII", start + index * 20)
            if not any(entry):
                break
            names.append(dll_name(entry[3]))
        else:
            raise ValueError("unterminated PE import directory")

    # IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT uses 32-byte descriptors. When the
    # dlattrRva flag is clear, szName is an image-base-relative virtual address.
    delay_rva, delay_size = directory(13)
    if delay_rva:
        image_base = unpack("<Q", optional + 24)[0]
        start = file_offset(delay_rva)
        for index in range(min(512, delay_size // 32 + 1)):
            entry = unpack("<IIIIIIII", start + index * 32)
            if not any(entry):
                break
            name_rva = entry[1] if entry[0] & 1 else entry[1] - image_base
            if name_rva <= 0:
                raise ValueError("invalid PE delay-import name")
            names.append(dll_name(name_rva))
        else:
            raise ValueError("unterminated PE delay-import directory")

    return sorted(set(names), key=str.lower)
