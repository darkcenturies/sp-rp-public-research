param(
    [Parameter(Mandatory = $true)] [string] $SourceImg,
    [Parameter(Mandatory = $true)] [string] $Output
)

$ErrorActionPreference = "Stop"

# The installed radar.img contains 96x96 independent 256px DXT1 tiles. A
# single full-resolution atlas would be 24576px, beyond D3D9's common 16384px
# limit. Keep every second DXT block to produce a 12288px atlas: the highest
# exact power-of-two reduction that fits modern D3D9 hardware, without a lossy
# decode/re-encode cycle.
$source = @'
using System;
using System.IO;
using System.Text;

public static class ValkyrieHdOverview {
    const int Sector = 2048;
    const int Tiles = 96;
    const int SourcePixels = 256;
    const int OutputTilePixels = 128;
    const int SourceBlocks = SourcePixels / 4;
    const int OutputBlocks = OutputTilePixels / 4;
    const int AtlasPixels = Tiles * OutputTilePixels;
    const int AtlasBlocks = AtlasPixels / 4;
    const int BlockBytes = 8;

    static ushort U16(byte[] b, int o) { return BitConverter.ToUInt16(b, o); }
    static uint U32(byte[] b, int o) { return BitConverter.ToUInt32(b, o); }
    static void Put16(byte[] b, int o, ushort v) { Buffer.BlockCopy(BitConverter.GetBytes(v), 0, b, o, 2); }
    static void Put32(byte[] b, int o, uint v) { Buffer.BlockCopy(BitConverter.GetBytes(v), 0, b, o, 4); }

    public static void Build(string imgPath, string outputPath) {
        using (var img = new FileStream(imgPath, FileMode.Open, FileAccess.Read, FileShare.Read))
        using (var reader = new BinaryReader(img, Encoding.ASCII, true)) {
            if (Encoding.ASCII.GetString(reader.ReadBytes(4)) != "VER2")
                throw new InvalidDataException("radar.img is not a GTA SA VER2 archive");
            int count = checked((int)reader.ReadUInt32());
            if (count != Tiles * Tiles)
                throw new InvalidDataException("radar.img has " + count + " entries; expected 9216");

            var sectors = new uint[count];
            var lengths = new ushort[count];
            for (int i = 0; i < count; ++i) {
                sectors[i] = reader.ReadUInt32();
                lengths[i] = reader.ReadUInt16();
                reader.ReadUInt16();
                reader.ReadBytes(24);
            }

            var atlas = new byte[AtlasBlocks * AtlasBlocks * BlockBytes];
            byte[] first = null;
            var seen = new bool[count];
            for (int entry = 0; entry < count; ++entry) {
                img.Position = (long)sectors[entry] * Sector;
                byte[] txd = reader.ReadBytes(lengths[entry] * Sector);
                if (txd.Length < 0x90 + SourceBlocks * SourceBlocks * BlockBytes ||
                    U32(txd, 0) != 0x16 || U16(txd, 0x18) != 1 ||
                    U16(txd, 0x84) != SourcePixels || U16(txd, 0x86) != SourcePixels)
                    throw new InvalidDataException("Invalid HD radar TXD at IMG entry " + entry);

                string name = Encoding.ASCII.GetString(txd, 0x3C, 32).TrimEnd('\0');
                int index;
                if (!name.StartsWith("radar", StringComparison.OrdinalIgnoreCase) ||
                    !Int32.TryParse(name.Substring(5), out index) ||
                    index < 0 || index >= count || seen[index])
                    throw new InvalidDataException("Invalid or duplicate texture " + name);
                seen[index] = true;
                if (first == null) first = txd;

                int tileX = index % Tiles;
                int tileY = index / Tiles;
                for (int by = 0; by < OutputBlocks; ++by) {
                    for (int bx = 0; bx < OutputBlocks; ++bx) {
                        int src = 0x90 + ((by * 2) * SourceBlocks + bx * 2) * BlockBytes;
                        int dst = ((tileY * OutputBlocks + by) * AtlasBlocks +
                                   tileX * OutputBlocks + bx) * BlockBytes;
                        Buffer.BlockCopy(txd, src, atlas, dst, BlockBytes);
                    }
                }
            }

            int sourcePayload = SourceBlocks * SourceBlocks * BlockBytes;
            var header = new byte[0x90];
            Buffer.BlockCopy(first, 0, header, 0, header.Length);
            var nativeExtension = new byte[12];
            Buffer.BlockCopy(first, 0x90 + sourcePayload, nativeExtension, 0, 12);
            var dictionaryExtension = new byte[12];
            Buffer.BlockCopy(first, 0x90 + sourcePayload + 12, dictionaryExtension, 0, 12);

            int total = header.Length + atlas.Length + 24;
            Put32(header, 4, (uint)(total - 12));
            Put16(header, 0x18, 1);
            Put32(header, 0x20, (uint)(atlas.Length + 0x74));
            Put32(header, 0x2C, (uint)(atlas.Length + 0x5C));
            Array.Clear(header, 0x3C, 64);
            byte[] nameBytes = Encoding.ASCII.GetBytes("sprp_overview");
            Buffer.BlockCopy(nameBytes, 0, header, 0x3C, nameBytes.Length);
            Put16(header, 0x84, AtlasPixels);
            Put16(header, 0x86, AtlasPixels);
            Put32(header, 0x8C, (uint)atlas.Length);

            Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(outputPath)));
            using (var output = new FileStream(outputPath, FileMode.Create, FileAccess.Write, FileShare.None)) {
                output.Write(header, 0, header.Length);
                output.Write(atlas, 0, atlas.Length);
                output.Write(nativeExtension, 0, nativeExtension.Length);
                output.Write(dictionaryExtension, 0, dictionaryExtension.Length);
            }
        }
    }
}
'@

Add-Type -TypeDefinition $source -Language CSharp
[ValkyrieHdOverview]::Build(
    (Resolve-Path -LiteralPath $SourceImg).Path,
    [IO.Path]::GetFullPath($Output))
Write-Host "[build] $Output  (12288 x 12288 HD DXT1 overview)" -ForegroundColor Green
