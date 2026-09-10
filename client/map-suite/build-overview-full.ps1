param(
    [Parameter(Mandatory = $true)] [string] $SourceImg,
    [Parameter(Mandatory = $true)] [string] $OutputPrefix
)

$ErrorActionPreference = "Stop"

# Preserve the expanded radar's complete 24576x24576 source detail as four
# 12288x12288 DXT1 textures. A single texture is beyond D3D9's 16384 limit;
# four quadrants fit while avoiding the old every-other-block downsample.
$source = @'
using System;
using System.IO;
using System.Text;

public static class ValkyrieFullOverview {
    const int Sector = 2048;
    const int Tiles = 96;
    const int HalfTiles = 48;
    const int TilePixels = 256;
    const int TileBlocks = TilePixels / 4;
    const int AtlasPixels = HalfTiles * TilePixels;
    const int AtlasBlocks = AtlasPixels / 4;
    const int BlockBytes = 8;

    static ushort U16(byte[] b, int o) { return BitConverter.ToUInt16(b, o); }
    static uint U32(byte[] b, int o) { return BitConverter.ToUInt32(b, o); }
    static void Put16(byte[] b, int o, ushort v) { Buffer.BlockCopy(BitConverter.GetBytes(v), 0, b, o, 2); }
    static void Put32(byte[] b, int o, uint v) { Buffer.BlockCopy(BitConverter.GetBytes(v), 0, b, o, 4); }

    public static void Build(string imgPath, string outputPrefix) {
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

            byte[] first = null;
            for (int quadrant = 0; quadrant < 4; ++quadrant) {
                var atlas = new byte[AtlasBlocks * AtlasBlocks * BlockBytes];
                int startX = (quadrant & 1) * HalfTiles;
                int startY = (quadrant >> 1) * HalfTiles;
                for (int localY = 0; localY < HalfTiles; ++localY) {
                    for (int localX = 0; localX < HalfTiles; ++localX) {
                        int index = (startY + localY) * Tiles + startX + localX;
                        img.Position = (long)sectors[index] * Sector;
                        byte[] txd = reader.ReadBytes(lengths[index] * Sector);
                        int payload = TileBlocks * TileBlocks * BlockBytes;
                        if (txd.Length < 0x90 + payload + 24 || U32(txd, 0) != 0x16 ||
                            U16(txd, 0x18) != 1 || U16(txd, 0x84) != TilePixels ||
                            U16(txd, 0x86) != TilePixels)
                            throw new InvalidDataException("Invalid HD radar TXD at IMG entry " + index);
                        if (first == null) first = txd;
                        for (int by = 0; by < TileBlocks; ++by) {
                            int src = 0x90 + by * TileBlocks * BlockBytes;
                            int dst = ((localY * TileBlocks + by) * AtlasBlocks +
                                       localX * TileBlocks) * BlockBytes;
                            Buffer.BlockCopy(txd, src, atlas, dst, TileBlocks * BlockBytes);
                        }
                    }
                }

                int sourcePayload = TileBlocks * TileBlocks * BlockBytes;
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
                byte[] name = Encoding.ASCII.GetBytes("sprp_overview_q" + quadrant);
                Buffer.BlockCopy(name, 0, header, 0x3C, name.Length);
                Put16(header, 0x84, AtlasPixels);
                Put16(header, 0x86, AtlasPixels);
                Put32(header, 0x8C, (uint)atlas.Length);

                string output = outputPrefix + "-q" + quadrant + ".txd";
                Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(output)));
                using (var file = new FileStream(output, FileMode.Create, FileAccess.Write, FileShare.None)) {
                    file.Write(header, 0, header.Length);
                    file.Write(atlas, 0, atlas.Length);
                    file.Write(nativeExtension, 0, nativeExtension.Length);
                    file.Write(dictionaryExtension, 0, dictionaryExtension.Length);
                }
            }
        }
    }
}
'@

Add-Type -TypeDefinition $source -Language CSharp
[ValkyrieFullOverview]::Build(
    (Resolve-Path -LiteralPath $SourceImg).Path,
    [IO.Path]::GetFullPath($OutputPrefix))
Write-Host "[build] $OutputPrefix-q0..q3.txd  (four full-detail 12288 x 12288 DXT1 quadrants)" -ForegroundColor Green
