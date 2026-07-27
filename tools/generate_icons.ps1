param(
    [Parameter(Mandatory = $true)]
    [string]$SourcePath,

    [string]$OutputDirectory
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $scriptDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
    $OutputDirectory = Join-Path $scriptDirectory "..\app\assets"
}

$resolvedSource = (Resolve-Path -LiteralPath $SourcePath).Path
$resolvedOutput = [System.IO.Path]::GetFullPath($OutputDirectory)

Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies System.Drawing.dll -TypeDefinition @"
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.IO;
using System.Runtime.InteropServices;

public static class XakeIconGenerator
{
    private static Bitmap RemoveGreenBackground(string sourcePath)
    {
        using (Bitmap source = new Bitmap(sourcePath))
        {
            Bitmap result = new Bitmap(
                source.Width,
                source.Height,
                PixelFormat.Format32bppArgb);
            using (Graphics graphics = Graphics.FromImage(result))
            {
                graphics.CompositingMode = CompositingMode.SourceCopy;
                graphics.DrawImageUnscaled(source, 0, 0);
            }

            Rectangle bounds = new Rectangle(0, 0, result.Width, result.Height);
            BitmapData data = result.LockBits(
                bounds,
                ImageLockMode.ReadWrite,
                PixelFormat.Format32bppArgb);
            int byteCount = Math.Abs(data.Stride) * data.Height;
            byte[] pixels = new byte[byteCount];
            Marshal.Copy(data.Scan0, pixels, 0, byteCount);

            for (int y = 0; y < data.Height; ++y)
            {
                int row = y * data.Stride;
                for (int x = 0; x < data.Width; ++x)
                {
                    int index = row + x * 4;
                    int blue = pixels[index];
                    int green = pixels[index + 1];
                    int red = pixels[index + 2];
                    int originalAlpha = pixels[index + 3];
                    int redBlueAverage = (red + blue) / 2;
                    if (green <= redBlueAverage) {
                        continue;
                    }

                    int sourceAlpha = Math.Min(
                        originalAlpha,
                        Math.Max(0, 255 - green + redBlueAverage));
                    if (sourceAlpha <= 48)
                    {
                        pixels[index] = 0;
                        pixels[index + 1] = 0;
                        pixels[index + 2] = 0;
                        pixels[index + 3] = 0;
                        continue;
                    }

                    if (sourceAlpha < 250)
                    {
                        int alpha = Math.Min(
                            originalAlpha,
                            (sourceAlpha - 48) * 255 / (255 - 48));
                        pixels[index] = (byte)Math.Min(
                            255,
                            blue * 255 / sourceAlpha);
                        pixels[index + 1] = (byte)Math.Min(
                            255,
                            Math.Max(
                                0,
                                (green * 255 - (255 - sourceAlpha) * 255)
                                    / sourceAlpha));
                        pixels[index + 2] = (byte)Math.Min(
                            255,
                            red * 255 / sourceAlpha);
                        pixels[index + 3] = (byte)alpha;
                    }
                }
            }

            Marshal.Copy(pixels, 0, data.Scan0, byteCount);
            result.UnlockBits(data);
            return result;
        }
    }

    private static Bitmap Resize(Bitmap source, int size)
    {
        Bitmap result = new Bitmap(size, size, PixelFormat.Format32bppArgb);
        result.SetResolution(96.0f, 96.0f);
        using (Graphics graphics = Graphics.FromImage(result))
        {
            graphics.CompositingMode = CompositingMode.SourceCopy;
            graphics.CompositingQuality = CompositingQuality.HighQuality;
            graphics.InterpolationMode = InterpolationMode.HighQualityBicubic;
            graphics.PixelOffsetMode = PixelOffsetMode.HighQuality;
            graphics.SmoothingMode = SmoothingMode.HighQuality;
            graphics.DrawImage(
                source,
                new Rectangle(0, 0, size, size),
                new Rectangle(0, 0, source.Width, source.Height),
                GraphicsUnit.Pixel);
        }
        return result;
    }

    private static Bitmap NormalizeCanvas(Bitmap source)
    {
        int minX = source.Width;
        int minY = source.Height;
        int maxX = -1;
        int maxY = -1;
        Rectangle bounds = new Rectangle(0, 0, source.Width, source.Height);
        BitmapData data = source.LockBits(
            bounds,
            ImageLockMode.ReadOnly,
            PixelFormat.Format32bppArgb);
        int byteCount = Math.Abs(data.Stride) * data.Height;
        byte[] pixels = new byte[byteCount];
        Marshal.Copy(data.Scan0, pixels, 0, byteCount);
        source.UnlockBits(data);

        for (int y = 0; y < source.Height; ++y)
        {
            int row = y * data.Stride;
            for (int x = 0; x < source.Width; ++x)
            {
                if (pixels[row + x * 4 + 3] <= 16) {
                    continue;
                }
                minX = Math.Min(minX, x);
                minY = Math.Min(minY, y);
                maxX = Math.Max(maxX, x);
                maxY = Math.Max(maxY, y);
            }
        }

        if (maxX < minX || maxY < minY) {
            throw new InvalidOperationException("The source image is fully transparent.");
        }

        int contentWidth = maxX - minX + 1;
        int contentHeight = maxY - minY + 1;
        int contentSize = Math.Max(contentWidth, contentHeight);
        int canvasSize = (int)Math.Ceiling(contentSize / 0.84);
        Bitmap result = new Bitmap(
            canvasSize,
            canvasSize,
            PixelFormat.Format32bppArgb);
        using (Graphics graphics = Graphics.FromImage(result))
        {
            graphics.CompositingMode = CompositingMode.SourceCopy;
            graphics.InterpolationMode = InterpolationMode.HighQualityBicubic;
            graphics.PixelOffsetMode = PixelOffsetMode.HighQuality;
            int targetX = (canvasSize - contentWidth) / 2;
            int targetY = (canvasSize - contentHeight) / 2;
            graphics.DrawImage(
                source,
                new Rectangle(targetX, targetY, contentWidth, contentHeight),
                new Rectangle(minX, minY, contentWidth, contentHeight),
                GraphicsUnit.Pixel);
        }
        return result;
    }

    private static byte[] EncodePng(Bitmap bitmap)
    {
        using (MemoryStream stream = new MemoryStream())
        {
            bitmap.Save(stream, ImageFormat.Png);
            return stream.ToArray();
        }
    }

    private static void WriteIco(string path, int[] sizes, List<byte[]> images)
    {
        using (BinaryWriter writer = new BinaryWriter(File.Create(path)))
        {
            writer.Write((ushort)0);
            writer.Write((ushort)1);
            writer.Write((ushort)images.Count);

            int offset = 6 + images.Count * 16;
            for (int index = 0; index < images.Count; ++index)
            {
                int size = sizes[index];
                writer.Write((byte)(size == 256 ? 0 : size));
                writer.Write((byte)(size == 256 ? 0 : size));
                writer.Write((byte)0);
                writer.Write((byte)0);
                writer.Write((ushort)1);
                writer.Write((ushort)32);
                writer.Write((uint)images[index].Length);
                writer.Write((uint)offset);
                offset += images[index].Length;
            }

            foreach (byte[] image in images)
            {
                writer.Write(image);
            }
        }
    }

    public static void Generate(string sourcePath, string outputDirectory)
    {
        Directory.CreateDirectory(outputDirectory);
        string iconDirectory = Path.Combine(outputDirectory, "icons");
        Directory.CreateDirectory(iconDirectory);

        int[] sizes = new int[] { 16, 24, 32, 48, 64, 128, 256 };
        List<byte[]> iconImages = new List<byte[]>();
        using (Bitmap transparent = RemoveGreenBackground(sourcePath))
        using (Bitmap normalized = NormalizeCanvas(transparent))
        {
            using (Bitmap master = Resize(normalized, 1024))
            {
                master.Save(
                    Path.Combine(outputDirectory, "xake-logo.png"),
                    ImageFormat.Png);
            }

            foreach (int size in sizes)
            {
                using (Bitmap icon = Resize(normalized, size))
                {
                    byte[] png = EncodePng(icon);
                    File.WriteAllBytes(
                        Path.Combine(iconDirectory, "xake-" + size + ".png"),
                        png);
                    iconImages.Add(png);
                }
            }
        }

        WriteIco(
            Path.Combine(outputDirectory, "xake.ico"),
            sizes,
            iconImages);
    }
}
"@

[XakeIconGenerator]::Generate($resolvedSource, $resolvedOutput)
Write-Output "Generated Xake branding assets in $resolvedOutput"
