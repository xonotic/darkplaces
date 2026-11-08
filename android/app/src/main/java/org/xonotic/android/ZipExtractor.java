package org.xonotic.android;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.FilterInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.RandomAccessFile;
import java.util.zip.CRC32;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

/**
 * Streaming ZIP extraction for Xonotic game data.
 *
 * Bytes are decompressed (deflate) or copied (stored) as they arrive, CRC32
 * checked, and written to "name.part" before an atomic rename, so the full
 * archive never has to sit on the device.
 */
final class ZipExtractor {
    private ZipExtractor() { }

    interface Progress { void update(long bytesDone); }
    interface NamedProgress { void update(String name, long bytesDone); }

    /**
     * Writes one entry's (already decompressed) stream to {@code part}.
     *
     * @param have  bytes already present in {@code part} (resume of stored entries), else 0
     * @param size  expected uncompressed size
     * @param crc   expected CRC32 of the uncompressed data
     */
    static void extract(InputStream in, File part, long have, long size, long crc, boolean fresh,
                        Progress progress) throws IOException {
        CRC32 sum = new CRC32();
        byte[] buf = new byte[1 << 16];
        if (fresh) { part.delete(); have = 0; }
        try (RandomAccessFile out = new RandomAccessFile(part, "rw")) {
            if (have > 0) { // re-hash the bytes we're resuming after
                try (InputStream old = new FileInputStream(part)) {
                    long left = have; int r;
                    while (left > 0 && (r = old.read(buf, 0, (int) Math.min(buf.length, left))) > 0) {
                        sum.update(buf, 0, r); left -= r;
                    }
                }
            }
            out.setLength(have);
            out.seek(have);
            long done = have, lastUi = 0; int r;
            while (done < size && (r = in.read(buf, 0, (int) Math.min(buf.length, size - done))) > 0) {
                out.write(buf, 0, r);
                sum.update(buf, 0, r);
                done += r;
                long now = System.currentTimeMillis();
                if (now - lastUi > 250) { lastUi = now; progress.update(done); }
            }
            if (done != size) throw new IOException("Connection ended early for " + part.getName() + " (will resume)");
        }
        if (sum.getValue() != crc) {
            part.delete();
            throw new IOException("Checksum mismatch for " + part.getName() + "; it will be downloaded again");
        }
        progress.update(size);
    }

    /** Counts compressed bytes consumed from the source (for progress bars). */
    static final class CountingInputStream extends FilterInputStream {
        volatile long count;
        CountingInputStream(InputStream in) { super(in); }
        @Override public int read() throws IOException { int b = super.read(); if (b >= 0) count++; return b; }
        @Override public int read(byte[] b, int o, int l) throws IOException {
            int r = super.read(b, o, l); if (r > 0) count += r; return r;
        }
        @Override public long skip(long n) throws IOException { long s = super.skip(n); count += s; return s; }
    }

    /** True for game-data PK3s: top-level, or directly inside a "data/" folder (e.g. Xonotic/data/x.pk3). */
    static String dataPk3Name(String path) {
        path = path.replace('\\', '/');
        if (!path.toLowerCase().endsWith(".pk3") || path.endsWith("/")) return null;
        int slash = path.lastIndexOf('/');
        String base = path.substring(slash + 1);
        if (base.contains("..") || base.isEmpty()) return null;
        if (slash < 0) return base;
        String parent = path.substring(0, slash);
        return (parent.equals("data") || parent.endsWith("/data")) ? base : null;
    }

    /**
     * Extracts every game-data PK3 from a ZIP stream (e.g. a user-provided
     * xonotic-0.8.6.zip) into {@code dataDir}. Returns the number of files written.
     */
    static int extractDataPk3s(InputStream src, File dataDir, long totalBytes, NamedProgress progress) throws IOException {
        CountingInputStream counted = new CountingInputStream(src);
        int written = 0;
        byte[] buf = new byte[1 << 16];
        try (ZipInputStream zin = new ZipInputStream(counted)) {
            ZipEntry ze;
            while ((ze = zin.getNextEntry()) != null) {
                String name = ze.isDirectory() ? null : dataPk3Name(ze.getName());
                if (name == null) { zin.closeEntry(); continue; }
                File dst = new File(dataDir, name);
                if (ze.getSize() > 0 && dst.length() == ze.getSize()) { zin.closeEntry(); written++; continue; }
                File part = new File(dataDir, name + ".part");
                CRC32 sum = new CRC32();
                long lastUi = 0;
                try (OutputStream out = new FileOutputStream(part)) {
                    int r;
                    while ((r = zin.read(buf)) > 0) {
                        out.write(buf, 0, r);
                        sum.update(buf, 0, r);
                        long now = System.currentTimeMillis();
                        if (now - lastUi > 250) { lastUi = now; progress.update(name, counted.count); }
                    }
                }
                if (ze.getCrc() != -1 && sum.getValue() != ze.getCrc()) {
                    part.delete();
                    throw new IOException("Checksum mismatch in " + name + " (corrupt ZIP?)");
                }
                if (dst.exists()) dst.delete();
                if (!part.renameTo(dst)) throw new IOException("Cannot finalize " + name);
                written++;
                zin.closeEntry();
            }
        }
        progress.update("done", totalBytes > 0 ? totalBytes : counted.count);
        return written;
    }
}
