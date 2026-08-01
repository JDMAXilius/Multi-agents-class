#!/usr/bin/env python3
# =====================================================================================
# BREACHPOINT - bp_package.py :: a read-only .uasset package reader (NO ENGINE NEEDED)
#
# Serves:   R26 enforcement (docs/DESIGN-RULINGS.md). This is the half of the auditor
#           that is plain CPython 3.8+, stdlib only - same split as Tools/blockout
#           (arena_plan.py vs build_arena.py): every DECISION lives in plain python that
#           anyone can run with no engine installed and no editor free.
#
# What it reads, and only this: the uncooked package header - name table, import table,
# export table, and the raw serial range of each export. It never writes, never loads a
# UObject, and never needs Unreal. That is what lets the audit run while a human has the
# editor open.
#
# What it deliberately does NOT do: full tagged-property deserialisation. Where a fact
# needs more than the header can honestly give, the caller is told the confidence is
# 'low' and the audit reports INCONCLUSIVE rather than PASS. An auditor that guesses is
# worse than no auditor.
#
# Format notes (verified against UE 5.8 editor packages on 1 Aug 2026, five real assets):
#   summary: tag(0x9E2A83C1) legacy(-9) ue3ver ue4ver ue5ver licensee savedHash[20]
#            totalHeaderSize customVersionCount customVersions[20 bytes each]
#            packageName(FString) packageFlags nameCount nameOffset
#            softObjectPathsCount softObjectPathsOffset localizationId(FString)
#            gatherableTextCount gatherableTextOffset exportCount exportOffset
#            importCount importOffset cellExportCount cellExportOffset
#            cellImportCount cellImportOffset metaDataOffset dependsOffset ...
#   FName in a table  : int32 len (negative = UTF-16) + chars + null + 2x uint16 hash
#   FName referenced  : int32 nameIndex + int32 number
#   FPackageIndex     : >0 export (idx-1), <0 import (-idx-1), 0 null
# Record sizes are DERIVED from the table spans, never hardcoded, so a version bump that
# adds a field to FObjectExport does not silently mis-parse: it fails to validate and the
# asset is reported INCONCLUSIVE.
# =====================================================================================
"""Header-level reader for uncooked Unreal .uasset packages."""

import os
import struct

PACKAGE_TAG = 0x9E2A83C1
LFS_POINTER_MAGIC = b"version https://git-lfs.github.com/spec/v1"

# Candidate FObjectImport / FObjectExport record sizes, largest first. Only the leading
# fields are ever read; the size is used purely to stride the table.
IMPORT_RECORD_CANDIDATES = (44, 40, 36, 32, 28)
EXPORT_RECORD_CANDIDATES = (144, 136, 128, 120, 112, 104, 96, 88)


class PackageError(Exception):
    """Raised when the package cannot be parsed. Callers must treat this as
    INCONCLUSIVE - never as a pass."""


class LfsPointerError(PackageError):
    """The file on disk is a git-lfs pointer, not the asset. Nothing can be audited."""


def _i32(buf, off):
    return struct.unpack_from("<i", buf, off)[0]


def _u32(buf, off):
    return struct.unpack_from("<I", buf, off)[0]


def _i64(buf, off):
    return struct.unpack_from("<q", buf, off)[0]


class Export(object):
    __slots__ = ("index", "class_index", "super_index", "template_index",
                 "outer_index", "name", "serial_size", "serial_offset")

    def __init__(self, **kw):
        for k in self.__slots__:
            setattr(self, k, kw.get(k))

    def __repr__(self):
        return "<Export %s>" % self.name


class Import(object):
    __slots__ = ("index", "class_package", "class_name", "outer_index", "name")

    def __init__(self, **kw):
        for k in self.__slots__:
            setattr(self, k, kw.get(k))

    def __repr__(self):
        return "<Import %s.%s>" % (self.class_package, self.name)


class Package(object):
    """Parsed header of one .uasset."""

    def __init__(self, path):
        self.path = path
        with open(path, "rb") as fh:
            data = fh.read()
        if data[:len(LFS_POINTER_MAGIC)] == LFS_POINTER_MAGIC:
            raise LfsPointerError(
                "%s is a git-lfs POINTER, not the asset. Run `git lfs pull` before "
                "auditing - an audit of a pointer file proves nothing." % path)
        if len(data) < 64 or _u32(data, 0) != PACKAGE_TAG:
            raise PackageError("%s is not an Unreal package (bad tag)" % path)
        self.data = data
        self.legacy_version = _i32(data, 4)
        if self.legacy_version > -6 or self.legacy_version < -12:
            raise PackageError("%s: unsupported legacy package version %d - this reader "
                               "was written for UE5 editor packages (-8/-9). Refusing to "
                               "guess." % (path, self.legacy_version))
        self._parse_summary()
        self._parse_names()
        self._parse_imports()
        self._parse_exports()

    # -- summary -----------------------------------------------------------------------
    def _parse_summary(self):
        d = self.data
        o = 8                                   # tag + legacy
        o += 4                                  # LegacyUE3Version
        self.ue4_version = _i32(d, o); o += 4
        self.ue5_version = _i32(d, o); o += 4
        o += 4                                  # licensee version
        if self.legacy_version <= -9:
            o += 20                             # FIoHash SavedHash (UE 5.5+)
        self.total_header_size = _i32(d, o); o += 4
        custom_count = _i32(d, o); o += 4
        if custom_count < 0 or custom_count > 512:
            raise PackageError("%s: implausible custom-version count %d" % (self.path, custom_count))
        o += 20 * custom_count
        o = self._skip_fstring(o)               # PackageName
        self.package_flags = _u32(d, o); o += 4
        self.name_count = _i32(d, o); o += 4
        self.name_offset = _i32(d, o); o += 4
        o += 8                                  # SoftObjectPaths count/offset (UE5)
        o = self._skip_fstring(o)               # LocalizationId
        tail = struct.unpack_from("<15i", d, o)
        self.export_count, self.export_offset = tail[2], tail[3]
        self.import_count, self.import_offset = tail[4], tail[5]
        self.depends_offset = tail[11]
        for label, value in (("name", self.name_offset), ("export", self.export_offset),
                             ("import", self.import_offset)):
            if value < 0 or value > len(d):
                raise PackageError("%s: %s table offset %d is outside the file"
                                   % (self.path, label, value))

    def _skip_fstring(self, off):
        length = _i32(self.data, off)
        if length >= 0:
            return off + 4 + length
        return off + 4 + (-length) * 2

    # -- names -------------------------------------------------------------------------
    def _parse_names(self):
        d, p = self.data, self.name_offset
        self.names = []
        for _ in range(self.name_count):
            length = _i32(d, p); p += 4
            if length >= 0:
                if length == 0 or p + length > len(d):
                    raise PackageError("%s: malformed name entry" % self.path)
                text = d[p:p + length - 1].decode("utf-8", "replace"); p += length
            else:
                count = -length
                if p + count * 2 > len(d):
                    raise PackageError("%s: malformed wide name entry" % self.path)
                text = d[p:p + count * 2 - 2].decode("utf-16-le", "replace"); p += count * 2
            p += 4          # two uint16 hashes
            self.names.append(text)
        self.name_set = set(self.names)
        self.name_table_end = p

    def name(self, index):
        if 0 <= index < len(self.names):
            return self.names[index]
        return "<name#%d>" % index

    # -- record-size derivation ---------------------------------------------------------
    def _derive_record_size(self, table_offset, count, candidates, validator, span_end):
        if count <= 0:
            return 0
        if span_end is not None and span_end > table_offset:
            span = span_end - table_offset
            if span % count == 0:
                size = span // count
                if size in candidates and validator(table_offset, count, size):
                    return size
        for size in candidates:
            if table_offset + count * size > len(self.data):
                continue
            if validator(table_offset, count, size):
                return size
        raise PackageError("%s: could not determine a record size for the table at %d "
                           "(count %d). This package layout is newer than this reader."
                           % (self.path, table_offset, count))

    def _import_ok(self, base, count, size):
        for i in range(count):
            b = base + i * size
            if b + 28 > len(self.data):
                return False
            for field in (0, 8, 20):
                idx = _i32(self.data, b + field)
                if not (0 <= idx < self.name_count):
                    return False
            outer = _i32(self.data, b + 16)
            if outer > 0 or -outer - 1 > count:
                return False
        return True

    def _export_ok(self, base, count, size):
        for i in range(count):
            b = base + i * size
            if b + 44 > len(self.data):
                return False
            if not (0 <= _i32(self.data, b + 16) < self.name_count):
                return False
            serial_size = _i64(self.data, b + 28)
            serial_off = _i64(self.data, b + 36)
            if serial_size < 0 or serial_off < 0 or serial_off + serial_size > len(self.data):
                return False
            if serial_off < self.total_header_size:
                return False
        return True

    # -- tables ------------------------------------------------------------------------
    def _parse_imports(self):
        size = self._derive_record_size(self.import_offset, self.import_count,
                                        IMPORT_RECORD_CANDIDATES, self._import_ok,
                                        self.export_offset)
        self.import_record_size = size
        self.imports = []
        for i in range(self.import_count):
            b = self.import_offset + i * size
            self.imports.append(Import(
                index=i,
                class_package=self.name(_i32(self.data, b)),
                class_name=self.name(_i32(self.data, b + 8)),
                outer_index=_i32(self.data, b + 16),
                name=self.name(_i32(self.data, b + 20))))

    def _parse_exports(self):
        size = self._derive_record_size(self.export_offset, self.export_count,
                                        EXPORT_RECORD_CANDIDATES, self._export_ok,
                                        self.depends_offset)
        self.export_record_size = size
        self.exports = []
        for i in range(self.export_count):
            b = self.export_offset + i * size
            self.exports.append(Export(
                index=i,
                class_index=_i32(self.data, b),
                super_index=_i32(self.data, b + 4),
                template_index=_i32(self.data, b + 8),
                outer_index=_i32(self.data, b + 12),
                name=self.name(_i32(self.data, b + 16)),
                serial_size=_i64(self.data, b + 28),
                serial_offset=_i64(self.data, b + 36)))

    # -- resolution --------------------------------------------------------------------
    def resolve(self, package_index):
        """FPackageIndex -> ('import'|'export'|'null', object_or_None)."""
        if package_index < 0:
            idx = -package_index - 1
            if idx < len(self.imports):
                return ("import", self.imports[idx])
            return ("null", None)
        if package_index > 0:
            idx = package_index - 1
            if idx < len(self.exports):
                return ("export", self.exports[idx])
            return ("null", None)
        return ("null", None)

    def describe(self, package_index):
        kind, obj = self.resolve(package_index)
        if obj is None:
            return "None"
        if kind == "import":
            outer = ""
            k2, o2 = self.resolve(obj.outer_index)
            if o2 is not None:
                outer = o2.name + "."
            return "%s%s (%s)" % (outer, obj.name, obj.class_name)
        return "%s (export)" % obj.name

    def class_of(self, export):
        kind, obj = self.resolve(export.class_index)
        if obj is None:
            return "None"
        return obj.name

    def outer_of(self, export):
        return self.resolve(export.outer_index)

    # -- blueprint-specific views ---------------------------------------------------------
    @property
    def is_blueprint(self):
        for export in self.exports:
            if self.class_of(export) in ("Blueprint", "WidgetBlueprint", "AnimBlueprint",
                                         "BlueprintGeneratedClass"):
                return True
        return False

    def blueprint_export(self):
        for export in self.exports:
            if self.class_of(export) in ("Blueprint", "WidgetBlueprint", "AnimBlueprint"):
                return export
        return None

    def generated_class_export(self):
        for export in self.exports:
            if self.class_of(export).endswith("BlueprintGeneratedClass"):
                return export
        return None

    def exports_outered_to(self, export):
        out = []
        for candidate in self.exports:
            kind, obj = self.resolve(candidate.outer_index)
            if kind == "export" and obj is export:
                out.append(candidate)
        return out

    def referenced_names(self, export):
        """Every name the export's serial blob references, found by scanning for
        (int32 nameIndex, int32 number=0) pairs. A lenient scan on purpose: it is used
        for PRESENCE tests ('does this node carry ENodeEnabledState::Disabled'), which
        tolerate a false positive far better than a missed node would."""
        blob = self.data[export.serial_offset:export.serial_offset + export.serial_size]
        found = set()
        for q in range(0, max(0, len(blob) - 8)):
            idx, number = struct.unpack_from("<ii", blob, q)
            if number == 0 and 0 <= idx < self.name_count:
                found.add(self.names[idx])
        return found

    def tagged_properties(self, export):
        """[(property_name, property_type)] for an export's tagged serial data.

        Lenient scan for adjacent (FName name, FName type) pairs whose type name ends in
        'Property'. Uncooked editor packages only serialise properties that DIFFER from
        the parent default, so this list is precisely 'what this asset overrides' - which
        is exactly what R26 condition 4 needs a human to see."""
        blob = self.data[export.serial_offset:export.serial_offset + export.serial_size]
        out = []
        seen = set()
        for q in range(0, max(0, len(blob) - 16)):
            a, an, b, bn = struct.unpack_from("<iiii", blob, q)
            if an != 0 or bn != 0:
                continue
            if not (0 <= a < self.name_count and 0 <= b < self.name_count):
                continue
            type_name = self.names[b]
            if not type_name.endswith("Property"):
                continue
            prop_name = self.names[a]
            if prop_name in ("None", "") or prop_name.endswith("Property"):
                continue
            # A package path is never a property name; the lenient scan picks these up
            # when an object reference happens to sit next to a type name.
            if "/" in prop_name or "." in prop_name:
                continue
            key = (prop_name, type_name)
            if key in seen:
                continue
            seen.add(key)
            out.append(key)
        return out


def package_name_from_path(content_dir, file_path):
    rel = os.path.relpath(file_path, content_dir).replace("\\", "/")
    if rel.lower().endswith(".uasset"):
        rel = rel[:-len(".uasset")]
    return "/Game/" + rel
